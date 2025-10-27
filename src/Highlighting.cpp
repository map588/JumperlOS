#include "Highlighting.h"
#include "JumperlOS.h"
#include "Graphics.h"
#include "JumperlessDefines.h"
#include "LEDs.h"
#include "MatrixState.h"
#include "States.h"
#include "NetManager.h"
#include "NetsToChipConnections.h"
#include "Peripherals.h"
#include "ArduinoStuff.h"
#include "Probing.h"
#include "RotaryEncoder.h"
#include "config.h"
#include "hardware/i2c.h"
#include "oled.h"
#include "PersistentStuff.h"
#include "Commands.h"
#include <Arduino.h>
#include <cmath>

// ============================================================================
// Highlighting Class Implementation
// ============================================================================

// Static member initialization
Highlighting* Highlighting::instance = nullptr;

Highlighting& Highlighting::getInstance() {
    if (instance == nullptr) {
        instance = new Highlighting();
    }
    return *instance;
}

Highlighting::Highlighting() {
    // Initialize colors
    highlightedOriginalColor = {0, 0, 0};
    brightenedOriginalColor = {0, 0, 0};
    warningOriginalColor = {0, 0, 0};
}

/**
 * @brief Main service method for highlighting system
 * 
 * This is called each loop iteration and handles:
 * - Encoder-based net highlighting (CRITICAL - must run every loop for smooth UX)
 * - Probe reading integration
 * - Warning timeouts (rate-limited)
 * - Reading change detection (rate-limited)
 * 
 * OPTIMIZATION: encoder highlighting runs every loop for instant response,
 * but timeout checks and change detection are rate-limited to reduce overhead.
 */
ServiceStatus Highlighting::service() {
    lastStatus = ServiceStatus::IDLE;
    
    // ============================================================================
    // CRITICAL PATH: Button press check - MUST happen before encoder highlighting
    // ============================================================================
    // Check for encoder button press on persistent nodes (rails, DACs) FIRST
    // This prevents encoder movements from changing highlighting before voltage adjustment
    if (encoderButtonState == RELEASED && lastButtonEncoderState == PRESSED) {
        if (handleEncoderButtonPress()) {
            // Button press was handled (voltage adjustment, etc.)
            // CRITICAL: Consume ALL encoder state so nothing else processes it
            encoderButtonState = IDLE;
            lastButtonEncoderState = IDLE;
            encoderDirectionState = NONE;  // Also consume any pending direction changes
            lastStatus = ServiceStatus::BLOCKING;
            return lastStatus;
        }
        // If not handled, button press will be processed by other systems (menus, etc.)
    }
    
    // ============================================================================
    // CRITICAL PATH: Encoder highlighting - MUST run every loop for smooth UX
    // ============================================================================
    int encoderNetHighlighted = encoderNetHighlight();
    
    // Get probe reading from Probing service (cheap - cached value)
    int probeReading = probing.getLastProbeReading();
    
    // Handle probe-based highlighting (only if probe is touching something)
    if (probeReading > 0) {
        if (highlightNets(probeReading) > 0) {
            firstConnection = probeReading;
            lastStatus = ServiceStatus::BUSY;
        }
    }
    
    // ============================================================================
    // NON-CRITICAL PATH: Timeout checks and change detection
    // Rate-limit these to ~50Hz (every 20ms) - no need to check every loop
    // ============================================================================
    static unsigned long lastPeriodicCheckTime = 0;
    unsigned long now = millis();
    if (now - lastPeriodicCheckTime >= 20) {  // 20ms = 50Hz
        lastPeriodicCheckTime = now;
        
        // Handle warning timeouts
        warnNetTimeout(1);
        
        // Check for reading changes
        checkForReadingChanges();
    }
    
    // Track changes for LED updates
    if (lastHighlightedNet != highlightedNet ||
        lastBrightenedNet != brightenedNet ||
        lastWarningNet != warningNet) {
        lastHighlightedNet = highlightedNet;
        lastBrightenedNet = brightenedNet;
        lastWarningNet = warningNet;
        lastStatus = ServiceStatus::BUSY;
    }
    
    return lastStatus;
}

// Backward compatibility - create references to singleton members
rgbColor& highlightedOriginalColor = Highlighting::getInstance().highlightedOriginalColor;
rgbColor& brightenedOriginalColor = Highlighting::getInstance().brightenedOriginalColor;
rgbColor& warningOriginalColor = Highlighting::getInstance().warningOriginalColor;
int& firstConnection = Highlighting::getInstance().firstConnection;
int& showReadingRow = Highlighting::getInstance().showReadingRow;
int& showReadingNet = Highlighting::getInstance().showReadingNet;
int& highlightedRow = Highlighting::getInstance().highlightedRow;
int& lastNodeHighlighted = Highlighting::getInstance().lastNodeHighlighted;
int& highlightedNet = Highlighting::getInstance().highlightedNet;
int& probeConnectHighlight = Highlighting::getInstance().probeConnectHighlight;
int& brightenedNode = Highlighting::getInstance().brightenedNode;
int& brightenedNet = Highlighting::getInstance().brightenedNet;
int& brightenedRail = Highlighting::getInstance().brightenedRail;
int& brightenedAmount = Highlighting::getInstance().brightenedAmount;
int& brightenedNodeAmount = Highlighting::getInstance().brightenedNodeAmount;
int& brightenedNetAmount = Highlighting::getInstance().brightenedNetAmount;
int& warningRow = Highlighting::getInstance().warningRow;
int& warningNet = Highlighting::getInstance().warningNet;
unsigned long& warningTimeout = Highlighting::getInstance().warningTimeout;
unsigned long& warningTimer = Highlighting::getInstance().warningTimer;
unsigned long& highlightTimer = Highlighting::getInstance().highlightTimer;

// ============================================================================
// Existing Functions (now class methods)
// ============================================================================

void Highlighting::clearHighlighting( void ) {

    // netColors[highlightedNet] = highlightedOriginalColor;
    // netColors[brightenedNet] = brightenedOriginalColor;
    // netColors[warningNet] = warningOriginalColor;
    // Serial.println("clearHighlighting");
    // Serial.flush();
    for ( int i = 4; i < numberOfRowAnimations; i++ ) {
        rowAnimations[ i ].row = -1;
        rowAnimations[ i ].net = -1;
    }
    probeConnectHighlight = -1;
    highlightedNet = -1;
    brightenedNet = -1;
    warningNet = -1;
    warningRow = -1;
    brightenedRail = -1;
    brightenedNode = -1;
    highlightedRow = -1;

    firstConnection = -1;

    // Reset highlight timer
    highlightTimer = 0;

    // Note: No need to call assignNetColors() here - core 2's showNets() recomputes colors every frame
    showLEDsCore2 = 1;  // Trigger LED update on core 2
}

int lastReturnNode = -1;
int scrolledRow = -1;

int Highlighting::encoderNetHighlight( int print, int mode, int divider ) {

    int lastDivider = rotaryDivider;
    rotaryDivider = divider;
    int returnNode = -1;

    // Serial.print (" highlightedNet: ");
    // Serial.println(highlightedNet);

    // Serial.print (" brightenedRail: ");
    // Serial.println(brightenedRail);

    // Serial.print (" brightenedNode: ");
    // Serial.println(brightenedNode);

    // Serial.flush();
    // if (inClickMenu == 1)
    //   return -1;
    // rotaryEncoderStuff();
    if ( mode == 0 ) {
        if ( encoderDirectionState == UP ) {
            // Serial.println(encoderPosition);
            encoderDirectionState = NONE;
            if ( highlightedNet < 0 ) {
                highlightedNet = -1;
                brightenedNet = -1;
                currentHighlightedNode = 0;
            }
            currentHighlightedNode++;
            if ( highlightedNet >= 0 && highlightedNet < numberOfNets && globalState.connections.nets[ highlightedNet ].nodes[ currentHighlightedNode ] <= 0 ) {
                currentHighlightedNode = 0;
                highlightedNet++;
                if ( highlightedNet > numberOfNets - 1 ) {
                    highlightedNet = -2;
                    brightenedNet = -2;
                    currentHighlightedNode = 0;
                }
                brightenedNet = highlightedNet;
                if ( highlightedNet >= 0 && highlightedNet < numberOfNets ) {
                    brightenedNode = globalState.connections.nets[ highlightedNet ].nodes[ currentHighlightedNode ];
                    if ( highlightedNet != 0 && globalState.connections.nets[ highlightedNet ].nodes[ currentHighlightedNode ] != 0 ) {
                        returnNode = globalState.connections.nets[ highlightedNet ].nodes[ currentHighlightedNode ];
                    }
                } else {
                    brightenedNode = -1;
                }
                highlightNets( 0, highlightedNet, print );
                // Serial.print("highlightedNet: ");
                // Serial.println(highlightedNet);
                // Serial.flush();
            }
            if ( highlightedNet > numberOfNets - 1 ) {
                highlightedNet = -2;
                brightenedNet = -2;
                currentHighlightedNode = 0;
            }

            brightenedNet = highlightedNet;

            if ( highlightedNet >= 0 && highlightedNet < numberOfNets ) {
                brightenedNode = globalState.connections.nets[ highlightedNet ].nodes[ currentHighlightedNode ];
                if ( highlightedNet != 0 && globalState.connections.nets[ highlightedNet ].nodes[ currentHighlightedNode ] != 0 ) {
                    returnNode = globalState.connections.nets[ highlightedNet ].nodes[ currentHighlightedNode ];
                }
            } else {

                brightenedNode = -1;
            }
            // Serial.print("returnNode: ");
            // Serial.println(returnNode);
            // Serial.flush();
            highlightNets( 0, highlightedNet, print );
            // Serial.print("highlightedNet: ");
            // Serial.println(highlightedNet);
            // Serial.flush();
            // assignNetColors();
            // assignNetColors();

        } else if ( encoderDirectionState == DOWN ) {
            // Serial.println(encoderPosition);
            encoderDirectionState = NONE;
            if ( highlightedNet == 0 ) {

                highlightedNet = numberOfNets - 1;
                brightenedNet = numberOfNets - 1;
            }

            currentHighlightedNode--;

            if ( currentHighlightedNode < 0 ) {
                highlightedNet--;
                if ( highlightedNet < 0 ) {
                    highlightedNet = numberOfNets - 1;
                    brightenedNet = numberOfNets - 1;
                    currentHighlightedNode = 0;
                }
                currentHighlightedNode = MAX_NODES - 1;
                while ( highlightedNet >= 0 && highlightedNet < numberOfNets && globalState.connections.nets[ highlightedNet ].nodes[ currentHighlightedNode ] <= 0 ) {
                    currentHighlightedNode--;
                    if ( currentHighlightedNode < 0 ) {
                        highlightedNet--;
                        if ( highlightedNet < 0 ) {
                            highlightedNet = numberOfNets - 1;
                            brightenedNet = numberOfNets - 1;
                            currentHighlightedNode = 0;
                        }
                    }
                }
            }
            brightenedNet = highlightedNet;
            if ( highlightedNet >= 0 && highlightedNet < numberOfNets ) {
                brightenedNode = globalState.connections.nets[ highlightedNet ].nodes[ currentHighlightedNode ];
                if ( highlightedNet != 0 && globalState.connections.nets[ highlightedNet ].nodes[ currentHighlightedNode ] != 0 ) {
                    returnNode = globalState.connections.nets[ highlightedNet ].nodes[ currentHighlightedNode ];
                }
            } else {
                brightenedNode = -1;
            }
            // Serial.print("returnNode: ");
            // Serial.println(returnNode);
            // Serial.flush();
            highlightNets( 0, highlightedNet, print );
            // Serial.print("highlightedNet: ");
            // Serial.println(highlightedNet);
            // Serial.flush();
            // assignNetColors();
        }
        if ( returnNode != lastNodeHighlighted ) {
            // b.clear();
            // b.printRawRow(0b00000100, lastNodeHighlighted-2, 0x000000, 0x000000);
            // b.printRawRow(0b00000100, lastNodeHighlighted, 0x0000000, 0x000000);

            // b.printRawRow(0b00000100, returnNode-2, 0x0f0f00, 0x000000);
            // b.printRawRow(0b00000100, returnNode, 0x0f0f00, 0x000000);

            lastNodeHighlighted = returnNode;
            // showLEDsCore2 = 2;
        }
        // rotaryDivider = lastDivider;
        return returnNode;
    } else if ( mode == 1 ) {
        // Initialize scrolledRow if needed (start with breadboard row 1)
        // Valid scroll targets: 1..60 (breadboard), TOP_RAIL, BOTTOM_RAIL, NANO_D0..NANO_RESET_1
        if ( scrolledRow == -1 || !(
                 ( scrolledRow >= 1 && scrolledRow <= 60 ) ||
                 ( scrolledRow == GND ) || ( scrolledRow == TOP_RAIL ) || ( scrolledRow == BOTTOM_RAIL ) ||
                 ( scrolledRow >= NANO_D0 && scrolledRow <= NANO_RESET_1 ) ) ) {
            scrolledRow = 1;
        }
        // Serial.print("                      \rscrolledRow = ");
        // Serial.print(scrolledRow);

        // Helper function to increment scrolledRow across breadboard + rails + nano ranges
        auto incrementRow = []( int& row ) {
            if ( row >= 1 && row < 60 ) {
                row++;
            } else if ( row == 60 ) {
                row = GND; // jump from breadboard to GND
            } else if ( row == GND ) {
                row = TOP_RAIL; // then to rails
            } else if ( row == TOP_RAIL ) {
                row = BOTTOM_RAIL;
            } else if ( row == BOTTOM_RAIL ) {
                row = NANO_D0; // jump from rails to nano header
            } else if ( row >= NANO_D0 && row < NANO_RESET_1 ) {
                row++;
            } else if ( row == NANO_RESET_1 ) {
                row = 1; // wrap around to start
            }
        };

        // Helper function to decrement scrolledRow across breadboard + rails + nano ranges
        auto decrementRow = []( int& row ) {
            if ( row > 1 && row <= 60 ) {
                row--;
            } else if ( row == 1 ) {
                row = NANO_RESET_1; // wrap around to end
            } else if ( row > NANO_D0 && row <= NANO_RESET_1 ) {
                row--;
            } else if ( row == NANO_D0 ) {
                row = BOTTOM_RAIL; // jump from nano to rails
            } else if ( row == BOTTOM_RAIL ) {
                row = TOP_RAIL;
            } else if ( row == TOP_RAIL ) {
                row = GND;
            } else if ( row == GND ) {
                row = 60; // jump from GND to breadboard
            }
        };

        if ( encoderDirectionState == UP ) {
            encoderDirectionState = NONE;
            incrementRow( scrolledRow );

            // Find a row with connections starting from current scrolledRow
            int originalRow = scrolledRow;
            do {
                bool foundConnection = false;
                int connectedNet = -1;

                // Check if current scrolledRow has any connections
                for ( int i = 0; i < numberOfPaths; i++ ) {
                    if ( globalState.connections.paths[ i ].node1 == scrolledRow || globalState.connections.paths[ i ].node2 == scrolledRow ) {
                        foundConnection = true;
                        connectedNet = globalState.connections.paths[ i ].net;
                        break;
                    }
                }
                // Allow highlighting rails and GND even without explicit paths
                if ( !foundConnection ) {
                    if ( scrolledRow == GND ) {
                        foundConnection = true;
                        connectedNet = 1;
                    } else if ( scrolledRow == TOP_RAIL ) {
                        foundConnection = true;
                        connectedNet = 2;
                    } else if ( scrolledRow == BOTTOM_RAIL ) {
                        foundConnection = true;
                        connectedNet = 3;
                    }
                }

                if ( foundConnection ) {
                    highlightedNet = connectedNet;
                    brightenedNet = connectedNet;
                    brightenedNode = scrolledRow;
                    brightenedAmount = 80; // Set brightness for highlighting
                    highlightNets( 0, highlightedNet, print );
                    returnNode = scrolledRow;
                    break;
                } else {
                    incrementRow( scrolledRow );
                }
            } while ( scrolledRow != originalRow ); // prevent infinite loop

        } else if ( encoderDirectionState == DOWN ) {
            encoderDirectionState = NONE;
            decrementRow( scrolledRow );

            // Find a row with connections starting from current scrolledRow
            int originalRow = scrolledRow;
            do {
                bool foundConnection = false;
                int connectedNet = -1;

                // Check if current scrolledRow has any connections
                for ( int i = 0; i < numberOfPaths; i++ ) {
                    if ( globalState.connections.paths[ i ].node1 == scrolledRow || globalState.connections.paths[ i ].node2 == scrolledRow ) {
                        foundConnection = true;
                        connectedNet = globalState.connections.paths[ i ].net;
                        break;
                    }
                }
                // Allow highlighting rails and GND even without explicit paths
                if ( !foundConnection ) {
                    if ( scrolledRow == GND ) {
                        foundConnection = true;
                        connectedNet = 1;
                    } else if ( scrolledRow == TOP_RAIL ) {
                        foundConnection = true;
                        connectedNet = 2;
                    } else if ( scrolledRow == BOTTOM_RAIL ) {
                        foundConnection = true;
                        connectedNet = 3;
                    }
                }

                if ( foundConnection ) {
                    highlightedNet = connectedNet;
                    brightenedNet = connectedNet;
                    brightenedNode = scrolledRow;
                    brightenedAmount = 80; // Set brightness for highlighting
                    highlightNets( 0, highlightedNet, print );
                    returnNode = scrolledRow;
                    break;
                } else {
                    decrementRow( scrolledRow );
                }
            } while ( scrolledRow != originalRow ); // prevent infinite loop
        }

        // Ensure we always return a value for mode == 1
        // if (returnNode == -1) {
        //   returnNode = scrolledRow;
        //   Serial.print("returnNode: ");
        //   Serial.println(returnNode);
        //   Serial.flush();
        // }
        if ( returnNode != lastReturnNode ) {
            lastReturnNode = returnNode;
            // Serial.print("returnNode: ");
            // Serial.println(returnNode);
            // Serial.flush();
        }
        return returnNode;
    }

    // Final return for any other modes
    return returnNode;
}

int Highlighting::brightenNet( int node, int addBrightness ) {

    if ( node == -1 ) {
        // Don't write to netColors[] directly - let assignNetColors() handle it
        // Just clear the brightened state and trigger LED update
        brightenedNode = -1;
        brightenedNet = 0;
        brightenedRail = -1;
        showLEDsCore2 = 1;  // Trigger LED update
        return -1;
    }
    addBrightness = 0;

    for ( int i = 0; i < numberOfPaths; i++ ) {

        if ( node == globalState.connections.paths[ i ].node1 || node == globalState.connections.paths[ i ].node2 && globalState.connections.paths[ i ].duplicate == 0 ) {
            /// if (brightenedNet != i) {
            brightenedNet = globalState.connections.paths[ i ].net;
            brightenedNode = node;
            // Serial.print("\n\n\rbrightenedNet: ");
            // Serial.println(brightenedNet);
            // Serial.print("net ");
            // Serial.print(globalState.connections.paths[i].net);
            if ( brightenedNet == 1 ) {
                brightenedRail = 1;
                // lightUpRail(-1, 1, 1, addBrightness);
            } else if ( brightenedNet == 2 ) {
                brightenedRail = 0;
                // lightUpRail(-1, 0, 1, addBrightness);
            } else if ( brightenedNet == 3 ) {
                brightenedRail = 2;
                // lightUpRail(-1, 2, 1, addBrightness);
            } else {
                brightenedRail = -1;
                // lightUpNet(brightenedNet, addBrightness);
            }
            // Serial.print("\n\rbrightenedNet = ");
            // Serial.println(brightenedNet);
            brightenedOriginalColor = netColors[ brightenedNet ];
            // Note: No need to call assignNetColors() here - core 2's showNets() recomputes colors every frame
            showLEDsCore2 = 1;  // Trigger LED update on core 2
            return brightenedNet;
        }
    }
    switch ( node ) {
    case ( GND ): {
        //  Serial.print("\n\rGND");
        brightenedNode = node;  // Set brightenedNode for button handler
        brightenedNet = 1;
        brightenedRail = 1;
        // lightUpRail(-1, 1, 1, addBrightness);
        return 1;
    }
    case ( TOP_RAIL ): {
        // Serial.print("\n\rTOP_RAIL");
        brightenedNode = node;  // Set brightenedNode for button handler
        brightenedNet = 2;
        brightenedRail = 0;
        // lightUpRail(-1, 0, 1, addBrightness);
        return 2;
    }
    case ( BOTTOM_RAIL ): {
        // Serial.print("\n\rBOTTOM_RAIL");
        brightenedNode = node;  // Set brightenedNode for button handler
        brightenedNet = 3;
        brightenedRail = 2;
        // lightUpRail(-1, 2, 1, addBrightness);
        return 3;
    }
    }

    return -1;
}

/// @brief  mark a net as warning
/// @param -1 to clear warning
/// @return warningNet
int Highlighting::warnNet( int node ) {
    // Serial.print("warnNet node = ");
    // Serial.println(node);
    // Serial.flush();
    if ( node == -1 ) {
        // Don't write to netColors[] directly - let assignNetColors() handle it
        // Just clear the warning state and trigger LED update
        warningNet = -1;
        warningRow = -1;
        // Serial.print("warningNet = ");
        // Serial.println(warningNet);
        // Serial.flush();
        // brightenedRail = -1;
        return -1;
    }
    // addBrightness = 0;
    warningRow = bbPixelToNodesMap[ node ];

    for ( int i = 0; i < numberOfPaths; i++ ) {

        if ( node == globalState.connections.paths[ i ].node1 || node == globalState.connections.paths[ i ].node2 ) {
            /// if (brightenedNet != i) {
            warningNet = globalState.connections.paths[ i ].net;

            // Serial.print("warningNet = ");
            // Serial.println(warningNet);
            // Serial.flush();

            if ( warningNet == 1 ) {
                // brightenedRail = 1;
                // lightUpRail(-1, 1, 1, addBrightness);
            } else if ( warningNet == 2 ) {
                // brightenedRail = 0;
                // lightUpRail(-1, 0, 1, addBrightness);
            } else if ( warningNet == 3 ) {
                // brightenedRail = 2;
                // lightUpRail(-1, 2, 1, addBrightness);
            } else {
                // brightenedRail = -1;
                // lightUpNet(brightenedNet, addBrightness);
            }

            warningOriginalColor = netColors[ warningNet ];
            // Note: No need to call assignNetColors() here - core 2's showNets() recomputes colors every frame
            showLEDsCore2 = 1;  // Trigger LED update on core 2
            warningTimer = millis( );
            return warningNet;
        }
    }

    return -1;
}

unsigned long lastWarningTimer = 0;
unsigned long lastHighlightTimer = 0;
unsigned long highlightTimeout = 1800;           // 3 seconds timeout for regular nets
unsigned long persistentHighlightTimeout = 15000; // 15 seconds timeout for rails, DACs, GPIO outputs

unsigned long lastFirstConnectionTimer = 0;

void Highlighting::warnNetTimeout( int clearAll ) {
    // Serial.print("warningTimer = ");
    // Serial.println(warningTimer);
    // Serial.print("warningTimeout = ");
    // Serial.println(warningTimeout);
    // Serial.flush();
    if ( lastWarningTimer == 0 ) {
        lastWarningTimer = millis( );
    }

    if ( lastHighlightTimer == 0 ) {
        lastHighlightTimer = millis( );
    }

    // Check for warning timeout
    if ( warningTimer > 0 && millis( ) - warningTimer > warningTimeout ) {
        // warningTimeout = 0;
        // Serial.println("warningTimer timeout");
        if ( clearAll == 1 ) {
            clearHighlighting( );
        } else {
            // netColors[warningNet] = warningOriginalColor;

            warningNet = -1;
            warningRow = -1;
        }
        lastWarningTimer = millis( );
        warningTimer = 0;

        // Note: No need to call assignNetColors() here - core 2's showNets() recomputes colors every frame
        showLEDsCore2 = 1;  // Trigger LED update on core 2
    } else {
        lastWarningTimer = millis( ) - lastWarningTimer;
        // Serial.print("lastWarningTimer = ");
        // Serial.println(lastWarningTimer);
        // Serial.flush();
        // warningTimer = millis();
    }

    // Check for highlighted net timeout
    // Use persistent node system to determine timeout duration
    unsigned long currentTimeout = highlightTimeout;
    
    if (shouldPersistHighlight(brightenedNode)) {
        // Persistent nodes (rails, DACs, GPIO outputs) get much longer timeout
        currentTimeout = persistentHighlightTimeout;
    }

    if (highlightTimer > 0 && millis() - highlightTimer > currentTimeout) {
        clearHighlighting(); // Clear all highlighting when timeout expires
        highlightTimer = 0;
        lastHighlightTimer = millis();
    }
}

int Highlighting::highlightNets( int probeReading, int encoderNetHighlighted, int print ) {
    // Serial.print("justReadProbe = ");
    // Serial.println(probeReading);
    // delay(100);

    //  Serial.println("probeReading: ");
    //  Serial.println(probeReading);
    //  Serial.flush();

    int netHighlighted;

    if ( encoderNetHighlighted > 0 ) {

        netHighlighted = encoderNetHighlighted;

    } else {

        netHighlighted = brightenNet( probeReading );
    }

    if ( netHighlighted > 0 ) {

        highlightedOriginalColor = netColors[ netHighlighted ];
        highlightedNet = netHighlighted;

        // Start the highlight timer
        highlightTimer = millis( );

        // Serial.print("netHighlighted = ");
        // Serial.println(netHighlighted);
        if ( print == 1 ) {
            Serial.print( "\r                                               \r" );
            Serial.flush( );
            oled.setTextSize( 1 );
        }
        clearColorOverrides( 1, 1, 0 );
        brightenedRail = -1;
        lastPrintedNet = -1;
        switch ( netHighlighted ) {
        case 0:
            break;
        case 1:
            if ( lastPrintedNet != netHighlighted ) {
                if ( print == 1 ) {
                    Serial.print( "GND" );
                    Serial.flush( );
                    char oledString[ 30 ];
                    sprintf( oledString, "GND" );

                    //oled.clear( );
                    oled.clearPrintShow( oledString, 2, true, true, true );
                    //oled.show( );
                }
                lastPrintedNet = netHighlighted;
            }
            brightenedRail = 1;
            break;
        case 2:
            if ( lastPrintedNet != netHighlighted ) {
                lastPrintedNet = netHighlighted;
                if ( print == 1 ) {
                    Serial.print( "Top Rail  " );

                    Serial.print( globalState.power.topRail );
                    Serial.print( " V" );
                    Serial.flush( );

                    char oledString[ 30 ];
                    sprintf( oledString, "Top Rail\n%0.2f V", (float)globalState.power.topRail );

                    //oled.clear( );
                    oled.clearPrintShow( oledString, 2, true, true, true );
                    //oled.show( );
                }
            }
            brightenedRail = 0;
            break;
        case 3:
            if ( lastPrintedNet != netHighlighted ) {
                lastPrintedNet = netHighlighted;
                if ( print == 1 ) {
                    Serial.print( "Bottom Rail  " );

                    Serial.print( globalState.power.bottomRail );
                    Serial.print( " V" );
                    Serial.flush( );

                    char oledString[ 30 ];
                    sprintf( oledString, "Bottom Rail\n%0.2f V", (float)globalState.power.bottomRail );

                    //oled.clear( );
                    oled.clearPrintShow( oledString, 2, true, true, true );
                    //oled.show( );
                }
            }
            brightenedRail = 2;
            break;
        case 4:
            if ( lastPrintedNet != netHighlighted ) {

                DACcolorOverride0 = -2;
                DACcolorOverride1 = 0x000000;
                if ( print == 1 ) {
                    Serial.print( "DAC 0  " );
                    Serial.print( globalState.power.dac0 );
                    Serial.print( " V" );
                    Serial.flush( );

                    char oledString[ 30 ];
                    sprintf( oledString, "DAC 0\n%0.2f V", (float)globalState.power.dac0 );

                    //  oled.clear( );
                    oled.clearPrintShow( oledString, 2, true, true, true );
                    //oled.show( );
                }
                lastPrintedNet = netHighlighted;
            }
            break;
        case 5:
            if ( lastPrintedNet != netHighlighted ) {

                DACcolorOverride0 = 0x000000;
                DACcolorOverride1 = -2;
                if ( print == 1 ) {
                    Serial.print( "DAC 1  " );
                    Serial.print( globalState.power.dac1 );
                    Serial.print( " V" );
                    Serial.flush( );

                    char oledString[ 30 ];
                    sprintf( oledString, "DAC 1\n%0.2f V", (float)globalState.power.dac1 );

                    //oled.clear( );
                    oled.clearPrintShow( oledString, 2, true, true, true );
                    //oled.show( );
                }
                lastPrintedNet = netHighlighted;
            }
            break;
        default: {

            if ( print == 1 ) {
                Serial.print( "\r                                          \r" );
                Serial.flush( );
            }

            // Serial.print("  \t ");
            int specialPrint = 0;

            // Detect alternate functions on this net
            bool uartTxOnNet = false;
            bool uartRxOnNet = false;
            bool i2cOnNet = false;
            bool pwmOnNet = false;
            int functionOnNetIndex = -1; // any other function index 0..9
            if ( netHighlighted > 0 ) {
                // gpio indices 8 and 9 correspond to UART TX (pin 0) and RX (pin 1)
                if ( gpioNet[ 8 ] == netHighlighted && gpio_function_map[ 8 ] == GPIO_FUNC_UART ) {
                    uartTxOnNet = true;
                }
                if ( gpioNet[ 9 ] == netHighlighted && gpio_function_map[ 9 ] == GPIO_FUNC_UART ) {
                    uartRxOnNet = true;
                }
                // I2C can be on any pins configured as I2C and tied to this net
                for ( int i = 0; i < 10; i++ ) {
                    if ( gpioNet[ i ] == netHighlighted ) {
                        if ( gpio_get_function( gpioDef[ i ][ 0 ] ) == GPIO_FUNC_I2C || gpio_function_map[ i ] == GPIO_FUNC_I2C ) {
                            i2cOnNet = true;
                            functionOnNetIndex = i;
                            break;
                        } else if ( gpio_get_function( gpioDef[ i ][ 0 ] ) == GPIO_FUNC_PWM || gpio_function_map[ i ] == GPIO_FUNC_PWM ) {
                            pwmOnNet = true;
                            functionOnNetIndex = i;
                            break;
                        } else if ( gpio_get_function( gpioDef[ i ][ 0 ] ) != GPIO_FUNC_SIO ) {
                            functionOnNetIndex = i; // some other function
                        }
                    }
                }
            }

            int adc = anyAdcConnected( netHighlighted );
            int gpioInputNumber = anyGpioInputConnected( netHighlighted );
            int gpioOutputNumber = anyGpioOutputConnected( netHighlighted );

            // for ( int i = 0; i < 10; i++ ) {
            //     if ( gpioNet[ i ] == netHighlighted ) {
            //         Serial.print( "gpioNet[i] = " );
            //         Serial.println( gpioNet[ i ] );
            //         Serial.print( "gpio_function_map[i] = " );
            //         Serial.print( gpio_function_map[ i ] );
            //     }
            // }

            // Serial.print("adc = ");
            // Serial.println(adc);
            // Serial.print("gpioInputNumber = ");
            // Serial.println(gpioInputNumber);
            // Serial.print("gpioOutputNumber = ");
            // Serial.println(gpioOutputNumber);

            if ( uartTxOnNet || uartRxOnNet ) {

                if ( lastPrintedNet != netHighlighted ) {
                    if ( print == 1 ) {
                        // Build UART config string like "115200 8N1"
                        int baud = USBSer1.baud( );
                        int bits = USBSer1.numbits( );
                        int stopbits = USBSer1.stopbits( ) + 1; // API returns 0->1 stop, 1->2 stops
                        int parity = USBSer1.paritytype( );

                        char parityChar = 'N';
                        switch ( parity ) {
                        case 1:
                            parityChar = 'O';
                            break;
                        case 2:
                            parityChar = 'E';
                            break;
                        case 3:
                            parityChar = 'M';
                            break;
                        case 4:
                            parityChar = 'S';
                            break;
                        default:
                            parityChar = 'N';
                            break;
                        }

                        Serial.print( uartTxOnNet && uartRxOnNet ? "UART Tx/Rx  " : ( uartTxOnNet ? "UART Tx    " : "UART Rx    " ) );
                        Serial.print( baud );
                        Serial.print( " " );
                        Serial.print( bits );
                        Serial.print( parityChar );
                        Serial.print( stopbits );
                        Serial.flush( );

                        char oledString[ 32 ];
                        sprintf( oledString, "UART %s\n%d %d%c%d",
                                 ( uartTxOnNet && uartRxOnNet ) ? "Tx/Rx" : ( uartTxOnNet ? "Tx" : "Rx" ),
                                 baud, bits, parityChar, stopbits );
                        oled.clear( );
                        oled.clearPrintShow( oledString, 1, true, true, true );
                        oled.show( );
                    }
                    lastPrintedNet = netHighlighted;
                }
                specialPrint = 1;

            } else if ( i2cOnNet ) {

                if ( lastPrintedNet != netHighlighted ) {
                    if ( print == 1 ) {
                        // Simple I2C label without scanning
                        Serial.print( "I2C  " );
                        //Serial.flush( );
                        const char* line = "I2C";
                        if ( functionOnNetIndex >= 0 ) {
                            int pin = gpioDef[ functionOnNetIndex ][ 0 ];
                            if ( pin == jumperlessConfig.top_oled.sda_pin ) 
                            {
                              line = "I2C  SDA";
                              Serial.print("SDA   ");
                              Serial.flush();
                            }
                            else if ( pin == jumperlessConfig.top_oled.scl_pin ) 
                            {
                              line = "I2C  SCL";
                              Serial.print("SCL   ");
                              Serial.flush();
                            }
                        }
                        char oledString[ 32 ];

                       // Serial.print("i2cSpeed = ");
                        Serial.print(i2cSpeed/1000);
                        Serial.print(" KHz");
                        Serial.flush();
               
                        snprintf( oledString, sizeof( oledString ), "%s\n%d KHz", line, i2cSpeed/1000 );
                        oled.clearPrintShow( oledString, 1, true, true, true );
                       // oled.show( );
                    }
                    lastPrintedNet = netHighlighted;
                }
                specialPrint = 1;

            } else if ( pwmOnNet ) {

                if ( lastPrintedNet != netHighlighted ) {
                    if ( print == 1 ) {
                        float freq = gpioPWMFrequency[ functionOnNetIndex ];
                        float duty = gpioPWMDutyCycle[ functionOnNetIndex ] * 100.0f;
                        Serial.print( "PWM  " );
                        Serial.print( freq, 2 );
                        Serial.print( " Hz  " );
                        Serial.print( duty, 1 );
                        Serial.print( "%" );
                        Serial.flush( );

                        char oledString[ 32 ];
                        sprintf( oledString, "PWM\n%0.0f Hz  %0.0f%%", freq, (duty) );
                        //oled.clear( );
                        oled.clearPrintShow( oledString, 1, true, true, true );
                        //oled.show( );
                    }
                    lastPrintedNet = netHighlighted;
                }
                specialPrint = 1;

            } else if ( functionOnNetIndex != -1 ) {

                if ( lastPrintedNet != netHighlighted ) {
                    if ( print == 1 ) {
                        // Generic function print using gpio_function_names
                        gpio_function_t fun = gpio_function_map[ functionOnNetIndex ];
                        const char* fname = gpio_function_names[ fun ].name;
                        Serial.print( fname );
                        Serial.flush( );

                        char oledString[ 32 ];
                        sprintf( oledString, "%s", fname );
                        //oled.clear( );
                        oled.clearPrintShow( oledString, 1, true, true, true );
                        //oled.show( );
                    }
                    lastPrintedNet = netHighlighted;
                }
                specialPrint = 1;

            } else if ( ( adc != -1 || gpioInputNumber != -1 || gpioOutputNumber != -1 ) ) {

                if ( lastPrintedNet != netHighlighted ) {

                    if ( adc != -1 ) {
                        ADCcolorOverride0 = -2;
                        ADCcolorOverride1 = -2;
                        logoOverrideMap[ 0 ].colorOverride = logoOverrideMap[ 0 ].defaultOverride;
                        logoOverrideMap[ 1 ].colorOverride = logoOverrideMap[ 1 ].defaultOverride;
                        logoOverriden = true;
                        if ( print == 1 ) {
                            Serial.print( "ADC " );
                            Serial.print( adc );
                            Serial.print( "   " );

                            Serial.print( (float)readAdcVoltage( adc, 32 ) );
                            Serial.print( " V" );
                            Serial.flush( );

                            char oledString[ 30 ];
                            sprintf( oledString, "ADC %d\n  %0.2f V", adc, (float)readAdcVoltage( adc, 32 ) );

                            //oled.clear();
                            oled.clearPrintShow( oledString, 2, true, true, true );
                            //oled.show();
                        }
                        specialPrint = 1;
                    }

                    if ( gpioInputNumber != -1 ) {
                        GPIOcolorOverride0 = -2;
                        GPIOcolorOverride1 = -2;
                        logoOverrideMap[ 4 ].colorOverride = logoOverrideMap[ 4 ].defaultOverride;
                        logoOverrideMap[ 5 ].colorOverride = logoOverrideMap[ 5 ].defaultOverride;
                        logoOverriden = true;
                        if ( print == 1 ) {
                            Serial.print( "GPIO " );
                            Serial.print( gpioInputNumber + 1 );
                            Serial.print( " input " );
                            Serial.flush( );

                            int gpioInputState = gpioReadWithFloating( gpioDef[ gpioInputNumber ][ 0 ] );
                            char stateString[ 10 ];
                            switch ( gpioInputState ) {
                            case 0:
                                Serial.print( "low" );
                                strcpy( stateString, "low" );
                                break;
                            case 1:
                                Serial.print( "high" );
                                strcpy( stateString, "high" );
                                break;
                            case 2:
                                Serial.print( "floating" );
                                strcpy( stateString, "floating" );
                                break;
                            default:
                                Serial.print( "?" );
                                strcpy( stateString, "?" );
                                break;
                            }

                            char oledString[ 30 ];
                            sprintf( oledString, "GPIO %d input\n %s", gpioInputNumber + 1, stateString );
                            //oled.clear( );
                            oled.clearPrintShow( oledString, 1, true, true, true );
                            //oled.show( );
                            // Serial.println();
                        }
                        specialPrint = 1;
                    }

                    if ( gpioOutputNumber != -1 ) {
                        GPIOcolorOverride0 = -2;
                        GPIOcolorOverride1 = -2;
                        logoOverrideMap[ 4 ].colorOverride = logoOverrideMap[ 4 ].defaultOverride;
                        logoOverrideMap[ 5 ].colorOverride = logoOverrideMap[ 5 ].defaultOverride;
                        logoOverriden = true;
                        if ( print == 1 ) {
                            Serial.print( "GPIO " );
                            Serial.print( gpioOutputNumber + 1 );
                            Serial.print( " output " );
                            Serial.flush( );

                            char stateString[ 7 ];
                            int gpioOutputState = gpio_get_out_level( gpioDef[ gpioOutputNumber ][ 0 ] );

                            if ( gpioOutputState == 0 ) {
                                Serial.print( "low" );
                                strcpy( stateString, "low" );
                            } else {
                                Serial.print( "high" );
                                strcpy( stateString, "high" );
                            }

                            char oledString[ 30 ];
                            sprintf( oledString, "GPIO %d output\n %s", gpioOutputNumber + 1, stateString );

                            oled.clearPrintShow( oledString, 1, true, true, true );

                            // Serial.println();
                        }
                        specialPrint = 1;
                    }
                }
            } else {
                if ( netHighlighted > 0 ) {
                    int length = 0;
                    if ( print == 1 ) {
                        Serial.print( "Net " );
                        Serial.print( netHighlighted );
                        Serial.print( "\t " );
                        Serial.print( "row " );
                        length = printNodeOrName( brightenedNode );
                        Serial.flush( );

                        char oledString[ 30 ];
                        sprintf( oledString, "Net %d       \n  row %s", netHighlighted, definesToChar( brightenedNode, 0 ) );
                        //oled.clear( );
                        oled.clearPrintShow( oledString, 1, true, true, true );
                        //oled.show( );

                        // for (int i = 0; i < 8 - length; i++) {
                        //   Serial.print(" ");
                    }
                    Serial.flush( );
                }
                if ( specialPrint == 0 ) {
                    // Serial.println();
                }
            }
            lastPrintedNet = netHighlighted;
        }
            Serial.flush( );
        }
    }
    // showLEDsCore2 = 1;

    // Serial.println("netHighlighted: ");
    // Serial.println(netHighlighted);
    // Serial.flush();

    return netHighlighted;
}

int Highlighting::checkForReadingChanges( void ) {
    // Static variables to store previous measurement values
    static float prevAdcReading = 0.0;
    static int prevGpioInputState = -1;
    static int prevGpioOutputState = -1;
    static float prevDacVoltage = 0.0;
    static float prevRailVoltage = 0.0;
    static int lastMeasuredNet = -1;
    static unsigned long lastUpdateTime = 0;

    // Don't update too frequently
    unsigned long currentTime = millis( );
    if ( currentTime - lastUpdateTime < 50 ) { // Minimum 50ms between updates
        return -1;
    }

    // Update showReadingNet to track brightenedNet (but don't clear on timeout)
    if ( brightenedNet > 0 ) {
        showReadingNet = brightenedNet;
    }

    // Check if there's a net to show readings for
    if ( showReadingNet <= 0 ) {
        lastMeasuredNet = -1;
        return -1;
    }

    // Reset stored values if we switched to a different net
    if ( lastMeasuredNet != showReadingNet ) {
        prevAdcReading = 0.0;
        prevGpioInputState = -1;
        prevGpioOutputState = -1;
        prevDacVoltage = 0.0;
        prevRailVoltage = 0.0;
        lastMeasuredNet = showReadingNet;
        lastUpdateTime = currentTime;
        return -1;
    }

    bool displayUpdated = false;
    char oledString[ 30 ];

    // Check for ADC measurements
    int adcChannel = anyAdcConnected( showReadingNet );
    if ( adcChannel != -1 ) {
        float currentAdcReading = readAdcVoltage( adcChannel, 64 );

        // Check if change is significant (>0.05V dead zone)
        if ( fabs( currentAdcReading - prevAdcReading ) > 0.009 ) {
            prevAdcReading = currentAdcReading;

            sprintf( oledString, "ADC %d\n  %0.2f V", adcChannel, currentAdcReading );
            // oled.clear();
            oled.clearPrintShow( oledString, 2, true, true, true );
            // oled.show();
            Serial.print( "\r                                 \r" );

            Serial.printf( "ADC %d   %0.2f V", adcChannel, currentAdcReading );
            Serial.flush( );

            displayUpdated = true;
        }
        showReadingRow = showReadingNet;
    }

    // Check for GPIO input
    int gpioInputNumber = anyGpioInputConnected( showReadingNet );
    if ( gpioInputNumber != -1 ) {
        int currentGpioInputState = gpioReading[ gpioInputNumber ];

        // Update if state changed
        if ( currentGpioInputState != prevGpioInputState ) {
            prevGpioInputState = currentGpioInputState;

            char stateString[ 10 ];
            switch ( currentGpioInputState ) {
            case 0:
                strcpy( stateString, "low" );
                break;
            case 1:
                strcpy( stateString, "high" );
                break;
            case 2:
                strcpy( stateString, "floating" );
                break;
            default:
                strcpy( stateString, "?" );
                break;
            }

            sprintf( oledString, "GPIO %d input\n %s", gpioInputNumber + 1, stateString );
            // oled.clear();
            oled.clearPrintShow( oledString, 1, true, true, true );
            // oled.show();

            Serial.print( "\r                                 \r" );
            Serial.printf( "GPIO %d input %s", gpioInputNumber + 1, stateString );
            Serial.flush( );

            displayUpdated = true;
        }
        showReadingRow = showReadingNet;
    }

    // Check for GPIO output
    int gpioOutputNumber = anyGpioOutputConnected( showReadingNet );
    if ( gpioOutputNumber != -1 ) {
        int currentGpioOutputState = gpio_get_out_level( gpioDef[ gpioOutputNumber ][ 0 ] );

        // Update if state changed
        if ( currentGpioOutputState != prevGpioOutputState ) {
            prevGpioOutputState = currentGpioOutputState;

            char stateString[ 7 ];
            if ( currentGpioOutputState == 0 ) {
                strcpy( stateString, "low" );
            } else {
                strcpy( stateString, "high" );
            }

            sprintf( oledString, "GPIO %d output\n %s", gpioOutputNumber + 1, stateString );
            // oled.clear();
            oled.clearPrintShow( oledString, 1, true, true, true );
            // oled.show();

            displayUpdated = true;
        }
        showReadingRow = showReadingNet;
    }

    // Check for DAC connections (nets 4 and 5)
    if ( showReadingNet == 4 ) { // DAC 0
        float currentDacVoltage = getDacVoltage( 0 );

        // Check if change is significant (>0.05V dead zone)
        if ( fabs( currentDacVoltage - prevDacVoltage ) > 0.05 ) {
            prevDacVoltage = currentDacVoltage;

            sprintf( oledString, "DAC 0\n%0.2f V", currentDacVoltage );
            // oled.clear();
            oled.clearPrintShow( oledString, 2, true, true, true );
            // oled.show();

            displayUpdated = true;
        }
    } else if ( showReadingNet == 5 ) { // DAC 1
        float currentDacVoltage = getDacVoltage( 1 );

        // Check if change is significant (>0.05V dead zone)
        if ( fabs( currentDacVoltage - prevDacVoltage ) > 0.05 ) {
            prevDacVoltage = currentDacVoltage;

            sprintf( oledString, "DAC 1\n%0.2f V", currentDacVoltage );
            // oled.clear();
            oled.clearPrintShow( oledString, 2, true, true, true );
            // oled.show();

            displayUpdated = true;
        }
        showReadingRow = showReadingNet;
    }

    // Check for rail connections (nets 1, 2, 3)
    if ( showReadingNet == 2 ) { // Top Rail
        float currentRailVoltage = globalState.power.topRail;

        // Check if change is significant (>0.05V dead zone)
        if ( fabs( currentRailVoltage - prevRailVoltage ) > 0.05 ) {
            prevRailVoltage = currentRailVoltage;

            sprintf( oledString, "Top Rail\n%0.2f V", currentRailVoltage );
            // oled.clear();
            oled.clearPrintShow( oledString, 2, true, true, true );
            // oled.show();

            displayUpdated = true;
        }
    } else if ( showReadingNet == 3 ) { // Bottom Rail
        float currentRailVoltage = globalState.power.bottomRail;

        // Check if change is significant (>0.05V dead zone)
        if ( fabs( currentRailVoltage - prevRailVoltage ) > 0.05 ) {
            prevRailVoltage = currentRailVoltage;

            sprintf( oledString, "Bottom Rail\n%0.2f V", currentRailVoltage );
            //  oled.clear();
            oled.clearPrintShow( oledString, 2, true, true, true );
            // oled.show();

            displayUpdated = true;
        }
        showReadingRow = showReadingNet;
    }

    if ( displayUpdated ) {
        lastUpdateTime = currentTime;
        return 1; // Indicates display was updated
    }

    return -1; // No updates
}

// ============================================================================
// Persistent Node Highlighting and Button Actions
// ============================================================================

/**
 * @brief Static callback for rail voltage updates
 */
static void railVoltageCallback(float value, bool isLive, void* context) {
    int rail = (int)(intptr_t)context;  // 0=both, 1=top, 2=bottom
    
    if (isLive) {
        // Live update - set the hardware AND update globalState
        switch (rail) {
            case 0: // Both rails
                setTopRail(value, 1, 0);  // save=1, saveEEPROM=0
                setBotRail(value, 1, 0);
                globalState.power.topRail = value;
                globalState.power.bottomRail = value;
                break;
            case 1: // Top rail
                setTopRail(value, 1, 0);
                globalState.power.topRail = value;
                break;
            case 2: // Bottom rail
                setBotRail(value, 1, 0);
                globalState.power.bottomRail = value;
                break;
        }
    } else {
        // Preview only - update globalState for LED display but DON'T set hardware
        // The LEDs read from globalState.power, so updating it will show preview
        switch (rail) {
            case 0: // Both rails
                globalState.power.topRail = value;
                globalState.power.bottomRail = value;
                break;
            case 1: // Top rail
                globalState.power.topRail = value;
                break;
            case 2: // Bottom rail
                globalState.power.bottomRail = value;
                break;
        }
        // Force LED update to show preview
        lightUpRail(-1, -1, 1, LEDbrightnessRail);
    }
}

/**
 * @brief Static callback for DAC voltage updates
 */
static void dacVoltageCallback(float value, bool isLive, void* context) {
    int dac = (int)(intptr_t)context;  // 0=DAC0, 1=DAC1
    
    if (isLive) {
        // Live update - set the hardware AND update globalState
        if (dac == 0) {
            setDac0voltage(value, 1, 0);  // save=1, saveEEPROM=0
            globalState.power.dac0 = value;
        } else {
            setDac1voltage(value, 1, 0);
            globalState.power.dac1 = value;
        }
    } else {
        // Preview only - update globalState for LED display but DON'T set hardware
        if (dac == 0) {
            globalState.power.dac0 = value;
        } else {
            globalState.power.dac1 = value;
        }
        // Force LED update to show preview (DACs are nets 4 and 5)
        lightUpNet(dac == 0 ? 4 : 5, -1, 1, LEDbrightnessSpecial, 0);
    }
}

/**
 * @brief Check if a node should persist in highlighting (not timeout)
 * 
 * Persistent nodes include:
 * - Rails (GND, TOP_RAIL, BOTTOM_RAIL)
 * - DACs (DAC0, DAC1)
 * - GPIO outputs
 * 
 * @param node The node to check
 * @return true if node should persist, false otherwise
 */
bool Highlighting::shouldPersistHighlight(int node) {
    // Special nodes that persist
    if (node == TOP_RAIL || node == BOTTOM_RAIL) {
        return true;
    }
    
    // DAC nets (4 and 5)
    if (highlightedNet == 4 || highlightedNet == 5) {
        return true;
    }
    
    // GPIO outputs persist
    if (highlightedNet > 0 && anyGpioOutputConnected(highlightedNet) != -1) {
        return true;
    }
    
    return false;
}

/**
 * @brief Check if Highlighting wants to handle the current button press
 * 
 * This is called by higher-priority services (like Menus) to check if they
 * should defer button handling to the Highlighting system for voltage adjustment.
 * 
 * @return true if button press should be handled by voltage adjustment, false otherwise
 */
bool Highlighting::wantsToHandleButtonPress(void) {
    // Only want to handle if a net is highlighted
    if (highlightedNet <= 0) {
        return false;
    }
    
    // Only handle persistent nodes that can be adjusted
    if (!shouldPersistHighlight(brightenedNode)) {
        return false;
    }
    
    // GND is persistent but not adjustable
    if (brightenedNode == GND) {
        return false;
    }
    
    // Rails and DACs are adjustable
    if (brightenedNode == TOP_RAIL || brightenedNode == BOTTOM_RAIL ||
        highlightedNet == 4 || highlightedNet == 5) {
        return true;
    }
    
    // GPIO outputs - for now, don't handle (let menu open)
    // Future: could return true here if we want to toggle GPIO on button press
    
    return false;
}

/**
 * @brief Handle encoder button press when a persistent node is highlighted
 * 
 * Launches appropriate adjustment UI for:
 * - Rails -> voltage adjustment
 * - DACs -> voltage adjustment
 * - GPIO -> (future: toggle output)
 * 
 * @return 1 if button press was handled, 0 if not
 */
int Highlighting::handleEncoderButtonPress(void) {
    // Only handle if something is highlighted (check net, not node which can be 0 or negative)
    if (highlightedNet <= 0) {
        return 0;
    }
    
    // Check if this is a persistent node
    if (!shouldPersistHighlight(brightenedNode)) {
        return 0;
    }
    
    // Handle rails
    if (brightenedNode == GND) {
        // GND is not adjustable
        return 0;
    }
    
    if (brightenedNode == TOP_RAIL) {
        adjustRailVoltage(1);
        return 1;
    }
    
    if (brightenedNode == BOTTOM_RAIL) {
        adjustRailVoltage(2);
        return 1;
    }
    
    // Handle DACs
    if (highlightedNet == 4) {
        adjustDACVoltage(0);
        return 1;
    }
    
    if (highlightedNet == 5) {
        adjustDACVoltage(1);
        return 1;
    }
    
    // GPIO outputs - for now, just indicate we handled it
    // Future: could toggle output here
    if (anyGpioOutputConnected(highlightedNet) != -1) {
        // For now, don't do anything but consume the button press
        // This prevents menu from opening when clicking on GPIO outputs
        return 0;  // Return 0 so menu can still open for GPIO
    }
    
    return 0;
}

/**
 * @brief Launch voltage adjustment UI for a rail
 * 
 * @param rail Which rail to adjust (0=both, 1=top, 2=bottom)
 */
void Highlighting::adjustRailVoltage(int rail) {
    // Save original values for cancellation
    float origTopRail = globalState.power.topRail;
    float origBottomRail = globalState.power.bottomRail;
    
    // Prepare configuration
    VoltageAdjustConfig config;
    config.minVoltage = -8.0;
    config.maxVoltage = 8.0;
    config.enableSnap = false;
    config.liveUpdateInRange = true;
    config.liveUpdateMin = 0.0;
    config.liveUpdateMax = 5.0;
    
    // Set initial value and label based on which rail
    switch (rail) {
        case 0: // Both rails
            config.initialValue = (globalState.power.topRail + globalState.power.bottomRail) / 2.0;
            config.label = "Rails";
            break;
        case 1: // Top rail
            config.initialValue = globalState.power.topRail;
            config.label = "Top Rail";
            break;
        case 2: // Bottom rail
            config.initialValue = globalState.power.bottomRail;
            config.label = "Bot Rail";
            break;
    }
    
    // Set callback and context
    config.callback = railVoltageCallback;
    config.context = (void*)(intptr_t)rail;
    
    // Run the adjuster
    AdjustResult result = VoltageAdjuster::adjust(config);
    
    if (result == AdjustResult::CONFIRMED) {
        // User confirmed - save voltages to persistent storage
        saveVoltages(globalState.power.topRail, globalState.power.bottomRail,
                     globalState.power.dac0, globalState.power.dac1);
        
        // Re-highlight the rail to show updated voltage
        clearHighlighting();
        highlightNets(0, highlightedNet, 1);
    } else {
        // User cancelled - restore original values in globalState
        globalState.power.topRail = origTopRail;
        globalState.power.bottomRail = origBottomRail;
        
        // Hardware was already restored by VoltageAdjuster callback
        // Re-highlight to refresh display
        clearHighlighting();
        highlightNets(0, highlightedNet, 1);
    }
    
    showLEDsCore2 = 1;
}

/**
 * @brief Launch voltage adjustment UI for a DAC
 * 
 * @param dac Which DAC to adjust (0=DAC0, 1=DAC1)
 */
void Highlighting::adjustDACVoltage(int dac) {
    // Save original values for cancellation
    float origDac0 = globalState.power.dac0;
    float origDac1 = globalState.power.dac1;
    
    // Prepare configuration
    VoltageAdjustConfig config;
    config.minVoltage = -8.0;
    config.maxVoltage = 8.0;
    config.enableSnap = false;
    config.liveUpdateInRange = true;
    config.liveUpdateMin = 0.0;
    config.liveUpdateMax = 5.0;
    
    // Set initial value and label based on which DAC
    config.initialValue = (dac == 0) ? globalState.power.dac0 : globalState.power.dac1;
    config.label = (dac == 0) ? "DAC 0" : "DAC 1";
    
    // Set callback and context
    config.callback = dacVoltageCallback;
    config.context = (void*)(intptr_t)dac;
    
    // Run the adjuster
    AdjustResult result = VoltageAdjuster::adjust(config);
    
    if (result == AdjustResult::CONFIRMED) {
        // User confirmed - save voltages to persistent storage
        saveVoltages(globalState.power.topRail, globalState.power.bottomRail,
                     globalState.power.dac0, globalState.power.dac1);
        
        // Re-highlight the DAC to show updated voltage
        clearHighlighting();
        highlightNets(0, highlightedNet, 1);
    } else {
        // User cancelled - restore original values in globalState
        globalState.power.dac0 = origDac0;
        globalState.power.dac1 = origDac1;
        
        // Hardware was already restored by VoltageAdjuster callback
        // Re-highlight to refresh display
        clearHighlighting();
        highlightNets(0, highlightedNet, 1);
    }
    
    showLEDsCore2 = 1;
}