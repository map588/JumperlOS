// SPDX-License-Identifier: MIT
/*
 * Fake GPIO Implementation
 *
 * Provides software-emulated GPIO pins using the crossbar switching matrix.
 * Supports both INPUT (reading via ADC) and OUTPUT (voltage source switching) modes.
 * 
 * See FakeGpio.h for API documentation.
 */

#include "FakeGpio.h"
#include "JumperlessDefines.h"
#include "States.h"
#include "CH446Q.h"
#include "FileParsing.h"
#include "NetManager.h"
#include "Peripherals.h"
#include "Commands.h"  // For waitCore2()
#include "NetsToChipConnections.h"  // For gndChipAlternator
#include <Arduino.h>
#include <cmath>
#include <vector>

// External references - most are declared in included headers
// lastChipXY is in CH446Q.h
// gpioState, gpioReading, gpioReadingColors, gpioNet are in Peripherals.h
// numberOfNets is in NetManager.h
// gndChipAlternator is in NetsToChipConnections.h

// Forward declarations for MicroPython API functions (defined in JumperlessMicroPythonAPI.cpp)
extern "C" {
    void jl_dac_set(int channel, float voltage, int save);
    float jl_adc_get(int channel);
    void jl_fake_gpio_set_mode(int node, int mode);  // For MicroPython Pin class
}

// Debug flag
bool debugFakeGpio = false;

// Global storage
FakeGpioPinConfig fakeGpioPins[MAX_FAKE_GPIO];
int adcCurrentlyConnectedPin = -1;

// Internal state
static bool fakeGpioPinsInitialized = false;
static int tryWithGndChipAlternator = 0;
static bool configurationInProgress = false;  // Prevent update hooks during config

// ============================================================================
// Internal Helper Functions
// ============================================================================

// Initialize fake GPIO pin storage (call once)
static void initFakeGpioPins() {
    if (fakeGpioPinsInitialized)
        return;

    for (int i = 0; i < MAX_FAKE_GPIO; i++) {
        fakeGpioPins[i].active = false;
        fakeGpioPins[i].node = -1;
        fakeGpioPins[i].mode = -1;
        fakeGpioPins[i].chip_k_x = -1;
        fakeGpioPins[i].chip_k_y = -1;
        fakeGpioPins[i].current_state = 0;
        fakeGpioPins[i].hasStoredState = false;
        fakeGpioPins[i].path_length = 0;
        for (int j = 0; j < 4; j++) {
            fakeGpioPins[i].path_chips[j] = -1;
            fakeGpioPins[i].path_x[j] = -1;
            fakeGpioPins[i].path_y[j] = -1;
        }
        // Initialize chipXYState to all zeros
        for (int chip = 0; chip < 12; chip++) {
            for (int y = 0; y < 8; y++) {
                fakeGpioPins[i].chipXYState[chip].connected[y] = 0;
            }
        }
    }
    fakeGpioPinsInitialized = true;
}

// Find config slot for a given node (returns -1 if not found)
static int findFakeGpioPinSlot(int node) {
    for (int i = 0; i < MAX_FAKE_GPIO; i++) {
        if (fakeGpioPins[i].active && fakeGpioPins[i].node == node) {
            return i;
        }
    }
    return -1;
}

// Find free config slot (returns -1 if all full)
static int findFreeFakeGpioPinSlot() {
    for (int i = 0; i < MAX_FAKE_GPIO; i++) {
        if (!fakeGpioPins[i].active) {
            return i;
        }
    }
    return -1;
}

// Check if a DAC is connected to anything (except the node we're configuring)
static bool isDacConnected(int dac_node, int exclude_node = -1) {
    for (int i = 0; i < globalState.connections.numPaths; i++) {
        const pathStruct& path = globalState.connections.paths[i];
        bool has_dac = (path.node1 == dac_node || path.node2 == dac_node);
        bool has_excluded = (path.node1 == exclude_node || path.node2 == exclude_node);
        
        if (has_dac && !has_excluded) {
            return true;
        }
    }
    return false;
}

// Find available DAC for a target voltage
// Returns: 0 for DAC0, 1 for DAC1, -1 if none available
static int findAvailableDAC(float target_voltage, int exclude_node = -1, float tolerance = 0.1) {
    // First priority: Check if a DAC is already set to this voltage
    if (fabs(globalState.power.dac0 - target_voltage) < tolerance) {
        return 0;
    }
    if (fabs(globalState.power.dac1 - target_voltage) < tolerance) {
        return 1;
    }
    
    // Second priority: Find an unconnected DAC
    bool dac0_connected = isDacConnected(DAC0, exclude_node);
    bool dac1_connected = isDacConnected(DAC1, exclude_node);
    
    if (!dac0_connected) {
        return 0;
    }
    if (!dac1_connected) {
        return 1;
    }
    
    return -1;  // No DAC available
}

// Helper: Reroute chip K to connect a specific node (for OUTPUT mode)
static void rerouteChipK(int target_chip_k_x, int chip_k_y, int target_node) {
    // First, disconnect all X positions on this Y to clear previous voltage source
    int voltage_x_positions[] = {4, 5, 6, 7, 15};  // TOP_RAIL, BOTTOM_RAIL, DAC1, DAC0, GND
    for (int i = 0; i < 5; i++) {
        int x = voltage_x_positions[i];
        if (lastChipXY[CHIP_K].connected[chip_k_y] & (1 << x)) {  // Test bit
            sendXYraw(CHIP_K, x, chip_k_y, 0);  // 0 = disconnect
            lastChipXY[CHIP_K].connected[chip_k_y] &= ~(1 << x);  // Clear bit
        }
    }

    // Connect the new voltage source X to this Y
    if (target_chip_k_x >= 0 && target_chip_k_x < 16 && chip_k_y >= 0 && chip_k_y < 8) {
        sendXYraw(CHIP_K, target_chip_k_x, chip_k_y, 1);  // 1 = connect
        lastChipXY[CHIP_K].connected[chip_k_y] |= (1 << target_chip_k_x);  // Set bit
    }
}

// ============================================================================
// Visual Integration Functions
// ============================================================================

// Update fake GPIO visual state (called after each read/write)
static void updateFakeGpioReading(int slot, int reading) {
    if (slot < 0 || slot >= MAX_FAKE_GPIO) return;
    
    int idx = fakeGpioSlotToAnimationIndex(slot);
    gpioReading[idx] = reading;
    
    // Set colors based on state
    if (reading == 1) {
        gpioReadingColors[idx] = 0x230205;  // Reddish for HIGH
    } else if (reading == 0) {
        gpioReadingColors[idx] = 0x052302;  // Greenish for LOW
    } else {
        gpioReadingColors[idx] = 0x040408;  // Gray for floating/unknown
    }
    
    // if (debugFakeGpio) {
    //     Serial.print("updateFakeGpioReading: slot=");
    //     Serial.print(slot);
    //     Serial.print(" idx=");
    //     Serial.print(idx);
    //     Serial.print(" reading=");
    //     Serial.print(reading);
    //     Serial.print(" gpioNet[");
    //     Serial.print(idx);
    //     Serial.print("]=");
    //     Serial.println(gpioNet[idx]);
    // }
}

// Assign fake GPIO to visual net for LED animations
static void assignFakeGpioToVisualSlot(int slot, int node) {
    if (slot < 0 || slot >= MAX_FAKE_GPIO) return;
    
    if (debugFakeGpio) {
        Serial.print("assignFakeGpioToVisualSlot: slot=");
        Serial.print(slot);
        Serial.print(" node=");
        Serial.print(node);
        Serial.print(" numberOfNets=");
        Serial.println(numberOfNets);
    }
    
    // Find which net this node belongs to
    int net = -1;
    for (int n = 0; n < numberOfNets; n++) {
        for (int i = 0; i < MAX_NODES; i++) {
            if (globalState.connections.nets[n].nodes[i] == node) {
                net = n;
                if (debugFakeGpio) {
                    Serial.print("  Found node ");
                    Serial.print(node);
                    Serial.print(" in net ");
                    Serial.println(net);
                }
                break;
            }
            if (globalState.connections.nets[n].nodes[i] == 0) break;
        }
        if (net != -1) break;
    }
    
    if (debugFakeGpio && net == -1) {
        Serial.print("  WARNING: Node ");
        Serial.print(node);
        Serial.println(" not found in any net!");
    }
    
    // Assign to animation system
    int animIdx = fakeGpioSlotToAnimationIndex(slot);
    gpioNet[animIdx] = net;
    
    // Set state based on mode
    FakeGpioPinConfig& pin = fakeGpioPins[slot];
    if (pin.mode == 0) {
        // INPUT mode - use keeper animation for visual feedback
        gpioState[animIdx] = 7;  // Bus keeper mode shows current state
    } else if (pin.mode == 1) {
        // OUTPUT mode - use output state
        gpioState[animIdx] = pin.current_state;  // 0=LOW, 1=HIGH
    }
    
    gpioReading[animIdx] = pin.current_state;
    updateFakeGpioReading(slot, pin.current_state);
    
    if (debugFakeGpio) {
        Serial.print("  Set gpioNet[");
        Serial.print(animIdx);
        Serial.print("]=");
        Serial.print(net);
        Serial.print(" gpioState[");
        Serial.print(animIdx);
        Serial.print("]=");
        Serial.print(gpioState[animIdx]);
        Serial.print(" gpioReading[");
        Serial.print(animIdx);
        Serial.print("]=");
        Serial.println(gpioReading[animIdx]);
    }
}

// ============================================================================
// Public Helper Functions
// ============================================================================

int fakeGpioSlotToAnimationIndex(int slot) {
    return 10 + slot;
}

int fakeGpioSlotToNode(int slot) {
    return FAKE_GPIO_1 + slot;
}

bool isFakeGpioVirtualNode(int node) {
    return (node >= FAKE_GPIO_1 && node <= FAKE_GPIO_32);
}

int fakeGpioNodeToSlot(int node) {
    if (!isFakeGpioVirtualNode(node)) return -1;
    return node - FAKE_GPIO_1;
}

// ============================================================================
// Initialization and State Management
// ============================================================================

void initializeFakeGpioFromLoadedState() {
    initFakeGpioPins();  // Ensure base initialization is done
    
    // Access the global restoration list from States.cpp
    extern std::vector<FakeGpioRestorationInfo> pendingFakeGpioRestorations;
    
    // If we have saved configs, use those; otherwise fall back to inference
    if (!pendingFakeGpioRestorations.empty()) {
        // PREFERRED PATH: Restore from saved configs
        for (const auto& info : pendingFakeGpioRestorations) {
            if (info.slot < 0 || info.slot >= MAX_FAKE_GPIO) continue;
            
            // Verify the bridge still exists
            bool bridgeExists = false;
            int fgpVirtualNode = fakeGpioSlotToNode(info.slot);
            
            for (int i = 0; i < globalState.connections.numBridges; i++) {
                int n1 = globalState.connections.bridges[i][0];
                int n2 = globalState.connections.bridges[i][1];
                
                if ((n1 == fgpVirtualNode && n2 == info.node) ||
                    (n2 == fgpVirtualNode && n1 == info.node)) {
                    bridgeExists = true;
                    break;
                }
            }
            
            if (!bridgeExists) {
                if (debugFakeGpio) {
                    Serial.print("Skipping FakeGPIO slot ");
                    Serial.print(info.slot);
                    Serial.println(" - bridge no longer exists");
                }
                continue;
            }
            
            // Restore the configuration based on mode
            if (info.mode == 0) {
                // INPUT mode
                fakeGpioConfigInput(info.node, info.threshold_high, info.threshold_low);
            } else if (info.mode == 1) {
                // OUTPUT mode
                fakeGpioConfigOutput(info.node, info.v_high, info.v_low, info.threshold_high, info.threshold_low);
                
                // CRITICAL: Apply initial LOW state to chip K
                // This ensures the output is driving a voltage immediately after boot
                FakeGpioPinConfig& pin = fakeGpioPins[info.slot];
                if (pin.active && pin.chip_k_x != -1 && pin.chip_k_y != -1) {
                    // Apply LOW state (state 0) to initialize output
                    int target_node = pin.low_voltage_node;
                    int target_x = -1;
                    switch (target_node) {
                        case TOP_RAIL:    target_x = 4; break;
                        case BOTTOM_RAIL: target_x = 5; break;
                        case DAC1:        target_x = 6; break;
                        case DAC0:        target_x = 7; break;
                        case GND:         target_x = 15; break;
                    }
                    
                    if (target_x != -1) {
                        rerouteChipK(target_x, pin.chip_k_y, target_node);
                        pin.current_state = 0;
                        updateFakeGpioReading(info.slot, 0);
                        
                        if (debugFakeGpio) {
                            Serial.print("Initialized OUTPUT slot ");
                            Serial.print(info.slot);
                            Serial.println(" to LOW state");
                        }
                    }
                }
            }
            
            if (debugFakeGpio) {
                Serial.print("Restored FakeGPIO slot ");
                Serial.print(info.slot);
                Serial.print(" (node ");
                Serial.print(info.node);
                Serial.print(") as ");
                Serial.println(info.mode == 0 ? "INPUT" : "OUTPUT");
            }
        }
        
        // Clear the restoration list after applying
        pendingFakeGpioRestorations.clear();
        return;
    }
    
    // FALLBACK PATH: Infer from bridges (legacy behavior for old YAML files)
    // SCAN 1: Find all FAKE_GPIO_x nodes in current bridges
    struct FakeGpioBridge {
        int slot;
        int userNode;
        bool found;
    };
    
    FakeGpioBridge foundPins[MAX_FAKE_GPIO];
    for (int i = 0; i < MAX_FAKE_GPIO; i++) {
        foundPins[i].slot = i;
        foundPins[i].userNode = -1;
        foundPins[i].found = false;
    }
    
    // Scan bridges for FAKE_GPIO connections
    for (int i = 0; i < globalState.connections.numBridges; i++) {
        int n1 = globalState.connections.bridges[i][0];
        int n2 = globalState.connections.bridges[i][1];
        
        int fgpNode = -1;
        int userNode = -1;
        
        if (isFakeGpioVirtualNode(n1)) {
            fgpNode = n1;
            userNode = n2;
        } else if (isFakeGpioVirtualNode(n2)) {
            fgpNode = n2;
            userNode = n1;
        }
        
        if (fgpNode != -1 && !isFakeGpioVirtualNode(userNode)) {
            int slot = fakeGpioNodeToSlot(fgpNode);
            if (slot >= 0 && slot < MAX_FAKE_GPIO) {
                foundPins[slot].userNode = userNode;
                foundPins[slot].found = true;
            }
        }
    }
    
    // SCAN 2: For each found FAKE_GPIO, determine mode and voltage sources
    for (int slot = 0; slot < MAX_FAKE_GPIO; slot++) {
        if (!foundPins[slot].found) continue;
        
        int userNode = foundPins[slot].userNode;
        
        // Check what voltage sources are connected
        bool hasTopRail = false, hasBottomRail = false, hasGnd = false;
        bool hasDac0 = false, hasDac1 = false, hasAdc0 = false;
        
        for (int i = 0; i < globalState.connections.numBridges; i++) {
            int n1 = globalState.connections.bridges[i][0];
            int n2 = globalState.connections.bridges[i][1];
            
            if (n1 == userNode || n2 == userNode) {
                int otherNode = (n1 == userNode) ? n2 : n1;
                
                if (otherNode == TOP_RAIL) hasTopRail = true;
                else if (otherNode == BOTTOM_RAIL) hasBottomRail = true;
                else if (otherNode == GND) hasGnd = true;
                else if (otherNode == DAC0) hasDac0 = true;
                else if (otherNode == DAC1) hasDac1 = true;
                else if (otherNode == ADC0) hasAdc0 = true;
            }
        }
        
        bool isOutput = (hasTopRail || hasBottomRail || hasGnd || hasDac0 || hasDac1);
        
        FakeGpioPinConfig& pin = fakeGpioPins[slot];
        pin.active = true;
        pin.node = userNode;
        
        if (isOutput) {
            // OUTPUT MODE
            pin.mode = 1;
            
            // Find chip K in paths
            pin.chip_k_x = -1;
            pin.chip_k_y = -1;
            
            for (int i = 0; i < globalState.connections.numPaths; i++) {
                const pathStruct& path = globalState.connections.paths[i];
                if (path.node1 == userNode || path.node2 == userNode) {
                    for (int j = 0; j < 4; j++) {
                        if (path.chip[j] == CHIP_K) {
                            pin.chip_k_x = path.x[j];
                            pin.chip_k_y = path.y[j];
                            break;
                        }
                    }
                    if (pin.chip_k_x != -1) break;
                }
            }
            
            // Reconstruct voltage source allocations
            if (hasTopRail) {
                pin.high_voltage_node = TOP_RAIL;
                pin.v_high = globalState.power.topRail;
            } else if (hasDac0) {
                pin.high_voltage_node = DAC0;
                pin.v_high = globalState.power.dac0;
            } else if (hasDac1) {
                pin.high_voltage_node = DAC1;
                pin.v_high = globalState.power.dac1;
            } else if (hasBottomRail) {
                pin.high_voltage_node = BOTTOM_RAIL;
                pin.v_high = globalState.power.bottomRail;
            } else if (hasGnd) {
                pin.high_voltage_node = GND;
                pin.v_high = 0.0;
            }
            
            if (hasGnd) {
                pin.low_voltage_node = GND;
                pin.v_low = 0.0;
            } else if (hasBottomRail) {
                pin.low_voltage_node = BOTTOM_RAIL;
                pin.v_low = globalState.power.bottomRail;
            } else if (hasDac1) {
                pin.low_voltage_node = DAC1;
                pin.v_low = globalState.power.dac1;
            } else if (hasDac0) {
                pin.low_voltage_node = DAC0;
                pin.v_low = globalState.power.dac0;
            } else if (hasTopRail) {
                pin.low_voltage_node = TOP_RAIL;
                pin.v_low = globalState.power.topRail;
            }
            
            pin.threshold_high = 2.0;
            pin.threshold_low = 0.8;
            pin.current_state = 0;
            
            captureCurrentChipXYState(pin.chipXYState);
            pin.hasStoredState = true;
            
        } else {
            // INPUT MODE
            pin.mode = 0;
            pin.v_high = 0.0;
            pin.v_low = 0.0;
            pin.threshold_high = 2.0;
            pin.threshold_low = 0.8;
            pin.high_voltage_node = -1;
            pin.low_voltage_node = -1;
            pin.current_state = 0;
            pin.chip_k_x = -1;
            pin.chip_k_y = -1;
            
            captureCurrentChipXYState(pin.chipXYState);
            pin.hasStoredState = true;
        }
        
        assignFakeGpioToVisualSlot(slot, userNode);
        
        if (debugFakeGpio) {
            Serial.print("Restored fake GPIO slot ");
            Serial.print(slot);
            Serial.print(" (node ");
            Serial.print(userNode);
            Serial.print(") as ");
            Serial.println(pin.mode == 0 ? "INPUT" : "OUTPUT");
        }
    }
}

void updateFakeGpioAfterConnectionChange(int node1, int node2) {
    if (!fakeGpioPinsInitialized) return;
    if (configurationInProgress) return;  // Don't interfere during config
    
    // CLEANUP CHECK: If a FAKE_GPIO virtual node bridge was removed, deactivate the config
    // This happens when the user removes the FGP_x-to-node bridge
    for (int slot = 0; slot < MAX_FAKE_GPIO; slot++) {
        if (!fakeGpioPins[slot].active) continue;
        
        int fgpVirtualNode = fakeGpioSlotToNode(slot);
        int userNode = fakeGpioPins[slot].node;
        
        // Check if this bridge still exists
        bool bridgeExists = false;
        for (int i = 0; i < globalState.connections.numBridges; i++) {
            int n1 = globalState.connections.bridges[i][0];
            int n2 = globalState.connections.bridges[i][1];
            
            if ((n1 == fgpVirtualNode && n2 == userNode) ||
                (n2 == fgpVirtualNode && n1 == userNode)) {
                bridgeExists = true;
                break;
            }
        }
        
        if (!bridgeExists) {
            // Bridge was removed - deactivate this FakeGPIO config
            if (debugFakeGpio) {
                Serial.print("FakeGPIO slot ");
                Serial.print(slot);
                Serial.print(" (node ");
                Serial.print(userNode);
                Serial.println(") deactivated - bridge removed");
            }
            
            // Clear the config
            fakeGpioPins[slot].active = false;
            fakeGpioPins[slot].node = -1;
            fakeGpioPins[slot].mode = -1;
            fakeGpioPins[slot].hasStoredState = false;
            
            // Clear visual assignment
            int animIdx = fakeGpioSlotToAnimationIndex(slot);
            gpioNet[animIdx] = -1;
            gpioState[animIdx] = 0;
            gpioReading[animIdx] = 0;
            gpioReadingColors[animIdx] = 0x000000;
        }
    }
    
    // Quick check: affected slots bitmask
    uint32_t affected = 0;
    
    if (isFakeGpioVirtualNode(node1)) {
        int slot = fakeGpioNodeToSlot(node1);
        if (slot >= 0) affected |= (1 << slot);
    }
    if (isFakeGpioVirtualNode(node2)) {
        int slot = fakeGpioNodeToSlot(node2);
        if (slot >= 0) affected |= (1 << slot);
    }
    
    // Check if any active fake GPIO uses these nodes
    for (int slot = 0; slot < MAX_FAKE_GPIO; slot++) {
        if (!fakeGpioPins[slot].active) continue;
        
        FakeGpioPinConfig& pin = fakeGpioPins[slot];
        
        if (node1 == pin.node || node2 == pin.node) {
            affected |= (1 << slot);
            continue;
        }
        
        if (pin.mode == 0) {  // INPUT
            if (node1 == ADC0 || node2 == ADC0) {
                affected |= (1 << slot);
            }
        } else if (pin.mode == 1) {  // OUTPUT
            if (node1 == pin.high_voltage_node || node2 == pin.high_voltage_node ||
                node1 == pin.low_voltage_node || node2 == pin.low_voltage_node) {
                affected |= (1 << slot);
            }
        }
    }
    
    if (affected == 0) return;  // No pins affected
    
    int updateCount = 0;
    
    for (int slot = 0; slot < MAX_FAKE_GPIO; slot++) {
        if (!(affected & (1 << slot))) continue;
        if (!fakeGpioPins[slot].active) continue;
        
        FakeGpioPinConfig& pin = fakeGpioPins[slot];
        
        if (pin.mode == 0) {  // INPUT
            captureCurrentChipXYState(pin.chipXYState);
            pin.hasStoredState = true;
            updateCount++;
        } else if (pin.mode == 1) {  // OUTPUT
            int old_x = pin.chip_k_x;
            int old_y = pin.chip_k_y;
            
            pin.chip_k_x = -1;
            pin.chip_k_y = -1;
            
            for (int i = 0; i < globalState.connections.numPaths; i++) {
                const pathStruct& path = globalState.connections.paths[i];
                if (path.node1 == pin.node || path.node2 == pin.node) {
                    for (int j = 0; j < 4; j++) {
                        if (path.chip[j] == CHIP_K) {
                            pin.chip_k_x = path.x[j];
                            pin.chip_k_y = path.y[j];
                            break;
                        }
                    }
                    if (pin.chip_k_x != -1) break;
                }
            }
            
            captureCurrentChipXYState(pin.chipXYState);
            pin.hasStoredState = true;
            
            if (old_x != pin.chip_k_x || old_y != pin.chip_k_y) {
                updateCount++;
            }
        }
    }
    
    if (debugFakeGpio && updateCount > 0) {
        Serial.print("Updated ");
        Serial.print(updateCount);
        Serial.println(" fake GPIO pins after connection change");
    }
}

// ============================================================================
// Configuration Functions
// ============================================================================

int fakeGpioConfigInput(int node, float threshold_high, float threshold_low) {
    initFakeGpioPins();
    
    // Prevent update hooks from interfering during configuration
    configurationInProgress = true;

    // Safety check
    if (node >= RP_GPIO_20 && node <= RP_GPIO_27) {
        Serial.println("ERROR: Cannot use RP_GPIO pins for fake GPIO");
        configurationInProgress = false;
        return 0;
    }
    if (node == RP_UART_TX || node == RP_UART_RX) {
        Serial.println("ERROR: Cannot use UART pins for fake GPIO");
        configurationInProgress = false;
        return 0;
    }

    int slot = findFakeGpioPinSlot(node);
    if (slot == -1) {
        slot = findFreeFakeGpioPinSlot();
        if (slot == -1) {
            Serial.println("ERROR: No free fake GPIO slots (max 32)");
            return 0;
        }
    }

    FakeGpioPinConfig& pin = fakeGpioPins[slot];

    // Clear existing voltage sources at this node
    int voltage_sources[] = {TOP_RAIL, BOTTOM_RAIL, GND, DAC0, DAC1};
    for (int i = 0; i < 5; i++) {
        removeBridgeFromState(node, voltage_sources[i], false);
    }

    // Route through unique FAKE_GPIO_x node
    int fake_gpio_node = fakeGpioSlotToNode(slot);
    int adc_node = ADC0;
    
    // Add both bridges without auto-refresh to avoid interference
    addBridgeToState(node, adc_node, 0, false);
    addBridgeToState(fake_gpio_node, node, 0, false);
    
    // Now manually refresh to build paths
    extern void refreshLocalConnections(int ledShowOption, int fillUnused, int clean);
    refreshLocalConnections(1, 1, 0);
    waitCore2();

    // Capture complete chipXY state (with ADC routing in place)
    captureCurrentChipXYState(pin.chipXYState);
    pin.hasStoredState = true;
    
    if (debugFakeGpio) {
        Serial.print("Captured chipXY state for INPUT pin on node ");
        Serial.print(node);
        Serial.print(", slot ");
        Serial.println(slot);
    }

    // Store configuration
    pin.active = true;
    pin.node = node;
    pin.v_high = 0;
    pin.v_low = 0;
    pin.threshold_high = threshold_high;
    pin.threshold_low = threshold_low;
    pin.mode = 0;  // INPUT
    pin.high_voltage_node = -1;
    pin.low_voltage_node = -1;
    pin.current_state = 0;
    pin.path_length = 0;

    assignFakeGpioToVisualSlot(slot, node);

    // Remove the ADC bridge - the chipXY snapshot is already captured above
    // We can safely remove it now since configurationInProgress flag prevents
    // updateFakeGpioAfterConnectionChange() from overwriting our snapshot
    removeBridgeFromState(node, adc_node, true);
    
    // Re-enable update hooks
    configurationInProgress = false;

    return 1;
}

int fakeGpioConfigOutput(int node, float v_high, float v_low, float threshold_high, float threshold_low) {
    initFakeGpioPins();
    
    // Prevent update hooks from interfering during configuration
    configurationInProgress = true;

    // Safety check
    if (node >= RP_GPIO_20 && node <= RP_GPIO_27) {
        Serial.println("ERROR: Cannot use RP_GPIO pins for fake GPIO");
        configurationInProgress = false;
        return 0;
    }
    if (node == RP_UART_TX || node == RP_UART_RX) {
        Serial.println("ERROR: Cannot use UART pins for fake GPIO");
        configurationInProgress = false;
        return 0;
    }

    // Determine voltage sources
    // Strategy: Prioritize clean (unconnected) sources, then rails, then existing DACs
    int needed_high_node = -1;
    int needed_low_node = -1;
    
    const float VOLTAGE_TOLERANCE = 0.15;  // Slightly larger tolerance for matching
    
    // Helper lambda to find best voltage source (without modifying DACs yet)
    // SHARING-FIRST STRATEGY: FakeGPIO outputs can safely share voltage sources!
    auto findBestSource = [&](float target_voltage, int exclude_node) -> int {
        // Priority 1: Check if it's GND (0V) - always safe to share
        if (fabs(target_voltage) < 0.1) {
            return GND;
        }
        
        // Priority 2: Check if rails match voltage (ALWAYS prefer sharing rails!)
        // Rails can be safely shared by multiple FakeGPIO outputs
        if (fabs(target_voltage - globalState.power.topRail) < VOLTAGE_TOLERANCE) {
            if (debugFakeGpio) {
                bool isClean = !isDacConnected(TOP_RAIL, node);
                Serial.print("  -> Using TOP_RAIL");
                Serial.println(isClean ? " (clean)" : " (sharing with other pins)");
            }
            return TOP_RAIL;
        }
        if (fabs(target_voltage - globalState.power.bottomRail) < VOLTAGE_TOLERANCE) {
            if (debugFakeGpio) {
                bool isClean = !isDacConnected(BOTTOM_RAIL, node);
                Serial.print("  -> Using BOTTOM_RAIL");
                Serial.println(isClean ? " (clean)" : " (sharing with other pins)");
            }
            return BOTTOM_RAIL;
        }
        
        // Priority 3: Check if DACs already have this voltage (ALWAYS prefer sharing DACs!)
        // DACs can be safely shared by multiple FakeGPIO outputs
        if (fabs(target_voltage - globalState.power.dac0) < VOLTAGE_TOLERANCE && exclude_node != DAC0) {
            if (debugFakeGpio) {
                bool isClean = !isDacConnected(DAC0, node);
                Serial.print("  -> Using DAC0");
                Serial.println(isClean ? " (clean)" : " (sharing with other pins)");
            }
            return DAC0;
        }
        if (fabs(target_voltage - globalState.power.dac1) < VOLTAGE_TOLERANCE && exclude_node != DAC1) {
            if (debugFakeGpio) {
                bool isClean = !isDacConnected(DAC1, node);
                Serial.print("  -> Using DAC1");
                Serial.println(isClean ? " (clean)" : " (sharing with other pins)");
            }
            return DAC1;
        }
        
        // Priority 4: Need to allocate a NEW DAC (will set voltage later)
        // Only do this if no existing source matches
        return -1;  // Needs DAC allocation
    };
    
    // Find best sources for both voltages
    needed_high_node = findBestSource(v_high, -1);
    needed_low_node = findBestSource(v_low, needed_high_node);
    
    // Now allocate DACs for any unmatched voltages
    // Important: Allocate both DACs together to avoid conflicts
    if (needed_high_node == -1 || needed_low_node == -1) {
        // Need to allocate DAC(s)
        bool need_dac_for_high = (needed_high_node == -1);
        bool need_dac_for_low = (needed_low_node == -1);
        
        if (debugFakeGpio) {
            Serial.print("Need to allocate DAC(s): high=");
            Serial.print(need_dac_for_high);
            Serial.print(" low=");
            Serial.println(need_dac_for_low);
        }
        
        // Strategy: If we need 2 DACs and both are available, use both
        // Otherwise, try to find one available DAC
        bool dac0_available = !isDacConnected(DAC0, node);
        bool dac1_available = !isDacConnected(DAC1, node);
        
        if (need_dac_for_high && need_dac_for_low) {
            // Need 2 different DACs
            if (dac0_available && dac1_available) {
                // Perfect - use both
                needed_high_node = DAC0;
                needed_low_node = DAC1;
                jl_dac_set(0, v_high, 1);  // save=1 to update globalState
                jl_dac_set(1, v_low, 1);   // save=1 to update globalState
                if (debugFakeGpio) {
                    Serial.print("Allocated both DACs: DAC0=");
                    Serial.print(v_high);
                    Serial.print("V (HIGH), DAC1=");
                    Serial.print(v_low);
                    Serial.println("V (LOW)");
                }
            } else if (dac0_available) {
                // Only DAC0 available - use it for one, fail for other
                Serial.print("ERROR: Need 2 DACs (");
                Serial.print(v_high);
                Serial.print("V, ");
                Serial.print(v_low);
                Serial.println("V) but only DAC0 available");
                configurationInProgress = false;
                return 0;
            } else if (dac1_available) {
                // Only DAC1 available
                Serial.print("ERROR: Need 2 DACs (");
                Serial.print(v_high);
                Serial.print("V, ");
                Serial.print(v_low);
                Serial.println("V) but only DAC1 available");
                configurationInProgress = false;
                return 0;
            } else {
                Serial.print("ERROR: Need 2 DACs (");
                Serial.print(v_high);
                Serial.print("V, ");
                Serial.print(v_low);
                Serial.println("V) but both are in use");
                configurationInProgress = false;
                return 0;
            }
        } else if (need_dac_for_high) {
            // Only need DAC for HIGH - prefer unconnected one
            if (dac0_available) {
                needed_high_node = DAC0;
                jl_dac_set(0, v_high, 1);  // save=1 to update globalState
                if (debugFakeGpio) {
                    Serial.print("Allocated DAC0=");
                    Serial.print(v_high);
                    Serial.println("V for HIGH");
                }
            } else if (dac1_available) {
                needed_high_node = DAC1;
                jl_dac_set(1, v_high, 1);  // save=1 to update globalState
                if (debugFakeGpio) {
                    Serial.print("Allocated DAC1=");
                    Serial.print(v_high);
                    Serial.println("V for HIGH");
                }
            } else {
                Serial.print("ERROR: No available DAC for HIGH voltage (");
                Serial.print(v_high);
                Serial.println("V) - both DACs in use");
                configurationInProgress = false;
                return 0;
            }
        } else if (need_dac_for_low) {
            // Only need DAC for LOW - prefer unconnected one
            if (dac0_available) {
                needed_low_node = DAC0;
                jl_dac_set(0, v_low, 1);  // save=1 to update globalState
                if (debugFakeGpio) {
                    Serial.print("Allocated DAC0=");
                    Serial.print(v_low);
                    Serial.println("V for LOW");
                }
            } else if (dac1_available) {
                needed_low_node = DAC1;
                jl_dac_set(1, v_low, 1);  // save=1 to update globalState
                if (debugFakeGpio) {
                    Serial.print("Allocated DAC1=");
                    Serial.print(v_low);
                    Serial.println("V for LOW");
                }
            } else {
                Serial.print("ERROR: No available DAC for LOW voltage (");
                Serial.print(v_low);
                Serial.println("V) - both DACs in use");
                configurationInProgress = false;
                return 0;
            }
        }
    }
    
    if (debugFakeGpio) {
        Serial.print("Selected voltage sources: HIGH=");
        Serial.print(v_high);
        Serial.print("V -> node ");
        Serial.print(needed_high_node);
        Serial.print(", LOW=");
        Serial.print(v_low);
        Serial.print("V -> node ");
        Serial.println(needed_low_node);
    }
    
    // Carefully manage connections
    bool has_high_connection = false;
    bool has_low_connection = false;
    
    for (int i = 0; i < globalState.connections.numPaths; i++) {
        const pathStruct& path = globalState.connections.paths[i];
        if ((path.node1 == node && path.node2 == needed_high_node) ||
            (path.node2 == node && path.node1 == needed_high_node)) {
            has_high_connection = true;
        }
        if ((path.node1 == node && path.node2 == needed_low_node) ||
            (path.node2 == node && path.node1 == needed_low_node)) {
            has_low_connection = true;
        }
    }
    
    // Clear unused voltage sources
    int voltage_sources[] = {TOP_RAIL, BOTTOM_RAIL, GND, DAC0, DAC1};
    bool to_clear[5] = {false};
    int clear_count = 0;
    
    for (int i = 0; i < 5; i++) {
        int vs = voltage_sources[i];
        if (vs != needed_high_node && vs != needed_low_node) {
            for (int j = 0; j < globalState.connections.numPaths; j++) {
                const pathStruct& path = globalState.connections.paths[j];
                if ((path.node1 == node && path.node2 == vs) ||
                    (path.node2 == node && path.node1 == vs)) {
                    to_clear[i] = true;
                    clear_count++;
                    break;
                }
            }
        }
    }
    
    // Interleaved clearing and adding
    int add_order[2] = {-1, -1};
    int add_count = 0;
    
    if (!has_high_connection && needed_high_node != GND) {
        add_order[add_count++] = 0;
    } else if (!has_low_connection && needed_low_node != GND) {
        add_order[add_count++] = 1;
    }
    
    if (!has_high_connection && needed_high_node != needed_low_node && add_order[0] != 0) {
        add_order[add_count++] = 0;
    } else if (!has_low_connection && needed_high_node != needed_low_node && add_order[0] != 1) {
        add_order[add_count++] = 1;
    }
    
    int clear_idx = 0;
    int add_idx = 0;
    
    while (clear_idx < 5 || add_idx < add_count) {
        if (clear_idx < 5) {
            while (clear_idx < 5 && !to_clear[clear_idx]) clear_idx++;
            if (clear_idx < 5) {
                removeBridgeFromState(node, voltage_sources[clear_idx], false);
                clear_idx++;
            }
        }
        
        if (add_idx < add_count) {
            int which = add_order[add_idx];
            if (which == 0) {
                addBridgeToState(node, needed_high_node, -1, true);
            } else {
                addBridgeToState(node, needed_low_node, -1, true);
            }
            add_idx++;
        }
    }
    
    waitCore2();

    // Find or allocate slot
    int slot = findFakeGpioPinSlot(node);
    if (slot == -1) {
        slot = findFreeFakeGpioPinSlot();
        if (slot == -1) {
            Serial.println("ERROR: No free fake GPIO slots (max 32)");
            configurationInProgress = false;
            return 0;
        }
    }

    FakeGpioPinConfig& pin = fakeGpioPins[slot];

    pin.active = true;
    pin.node = node;
    pin.v_high = v_high;
    pin.v_low = v_low;
    pin.threshold_high = threshold_high;
    pin.threshold_low = threshold_low;
    pin.mode = 1;  // OUTPUT
    pin.high_voltage_node = needed_high_node;
    pin.low_voltage_node = needed_low_node;

    // Find chip K coordinates
    pin.chip_k_x = -1;
    pin.chip_k_y = -1;
    
    for (int i = 0; i < globalState.connections.numPaths; i++) {
        const pathStruct& path = globalState.connections.paths[i];
        if (path.node1 == node || path.node2 == node) {
            for (int j = 0; j < 4; j++) {
                if (path.chip[j] == CHIP_K) {
                    pin.chip_k_x = path.x[j];
                    pin.chip_k_y = path.y[j];
                    break;
                }
            }
            if (pin.chip_k_x != -1) break;
        }
    }

    pin.current_state = 0;

    // Validate chip K was found
    if ((pin.chip_k_x == -1 || pin.chip_k_y == -1) && tryWithGndChipAlternator == 0) {
        tryWithGndChipAlternator = 1;
        gndChipAlternator = !gndChipAlternator;
        removeBridgeFromState(node, needed_high_node, false);
        removeBridgeFromState(node, needed_low_node, true);
        return fakeGpioConfigOutput(node, v_high, v_low, threshold_high, threshold_low);
    }
    if (pin.chip_k_x == -1 || pin.chip_k_y == -1) {
        Serial.print("ERROR: Chip K not found in routing path for node ");
        Serial.println(node);
        tryWithGndChipAlternator = 0;
        configurationInProgress = false;
        return 0;
    }

    tryWithGndChipAlternator = 0;

    captureCurrentChipXYState(pin.chipXYState);
    pin.hasStoredState = true;
    
    assignFakeGpioToVisualSlot(slot, node);
    
    // CRITICAL FIX: Apply initial LOW state to chip K hardware
    // This ensures the output is actively driven immediately after configuration
    int target_node = pin.low_voltage_node;
    int target_x = -1;
    switch (target_node) {
        case TOP_RAIL:    target_x = 4; break;
        case BOTTOM_RAIL: target_x = 5; break;
        case DAC1:        target_x = 6; break;
        case DAC0:        target_x = 7; break;
        case GND:         target_x = 15; break;
    }
    
    if (target_x != -1) {
        rerouteChipK(target_x, pin.chip_k_y, target_node);
        updateFakeGpioReading(slot, 0);
        
        if (debugFakeGpio) {
            Serial.print("Applied initial LOW state to chip K for slot ");
            Serial.print(slot);
            Serial.print(" (node ");
            Serial.print(node);
            Serial.println(")");
        }
    }
    
    configurationInProgress = false;

    return 1;
}

int fakeGpioConfig(int node, float v_high, float v_low, float threshold_high, float threshold_low, int mode) {
    // Auto-detect mode if not specified
    if (mode == -1) {
        if (v_high == 0.0 && v_low == 0.0) {
            mode = FAKE_GPIO_INPUT;
        } else {
            mode = FAKE_GPIO_OUTPUT;
        }
    }
    
    if (mode == FAKE_GPIO_INPUT) {
        return fakeGpioConfigInput(node, threshold_high, threshold_low);
    } else {
        return fakeGpioConfigOutput(node, v_high, v_low, threshold_high, threshold_low);
    }
}

// Node-based OUTPUT configuration (new, preferred method)
// Directly connects specified voltage source nodes without DAC allocation logic
// Accepts node constants: GND=100, TOP_RAIL=101, BOTTOM_RAIL=102, DAC0=106, DAC1=107
int fakeGpioConfigOutputNodes(int node, int high_node, int low_node, float threshold_high, float threshold_low) {
    initFakeGpioPins();
    
    // Prevent update hooks from interfering during configuration
    configurationInProgress = true;

    // Safety check
    if (node >= RP_GPIO_20 && node <= RP_GPIO_27) {
        Serial.println("ERROR: Cannot use RP_GPIO pins for fake GPIO");
        configurationInProgress = false;
        return 0;
    }
    if (node == RP_UART_TX || node == RP_UART_RX) {
        Serial.println("ERROR: Cannot use UART pins for fake GPIO");
        configurationInProgress = false;
        return 0;
    }

    // Validate voltage source nodes
    // GND=100, TOP_RAIL=101, BOTTOM_RAIL=102, DAC0=106, DAC1=107
    int valid_sources[] = {GND, TOP_RAIL, BOTTOM_RAIL, DAC0, DAC1};
    bool high_valid = false, low_valid = false;
    for (int i = 0; i < 5; i++) {
        if (high_node == valid_sources[i]) high_valid = true;
        if (low_node == valid_sources[i]) low_valid = true;
    }
    
    if (!high_valid) {
        Serial.print("ERROR: Invalid high_node ");
        Serial.print(high_node);
        Serial.println(" (must be GND/TOP_RAIL/BOTTOM_RAIL/DAC0/DAC1)");
        configurationInProgress = false;
        return 0;
    }
    if (!low_valid) {
        Serial.print("ERROR: Invalid low_node ");
        Serial.print(low_node);
        Serial.println(" (must be GND/TOP_RAIL/BOTTOM_RAIL/DAC0/DAC1)");
        configurationInProgress = false;
        return 0;
    }

    if (debugFakeGpio) {
        Serial.print("Configuring OUTPUT pin (node-based) on node ");
        Serial.print(node);
        Serial.print(", HIGH=node ");
        Serial.print(high_node);
        Serial.print(", LOW=node ");
        Serial.println(low_node);
    }

    // Carefully manage connections
    bool has_high_connection = false;
    bool has_low_connection = false;
    
    for (int i = 0; i < globalState.connections.numPaths; i++) {
        const pathStruct& path = globalState.connections.paths[i];
        if ((path.node1 == node && path.node2 == high_node) ||
            (path.node2 == node && path.node1 == high_node)) {
            has_high_connection = true;
        }
        if ((path.node1 == node && path.node2 == low_node) ||
            (path.node2 == node && path.node1 == low_node)) {
            has_low_connection = true;
        }
    }
    
    // Clear unused voltage sources
    int voltage_sources[] = {TOP_RAIL, BOTTOM_RAIL, GND, DAC0, DAC1};
    bool to_clear[5] = {false};
    int clear_count = 0;
    
    for (int i = 0; i < 5; i++) {
        int vs = voltage_sources[i];
        if (vs != high_node && vs != low_node) {
            for (int j = 0; j < globalState.connections.numPaths; j++) {
                const pathStruct& path = globalState.connections.paths[j];
                if ((path.node1 == node && path.node2 == vs) ||
                    (path.node2 == node && path.node1 == vs)) {
                    to_clear[i] = true;
                    clear_count++;
                    break;
                }
            }
        }
    }
    
    // Interleaved clearing and adding
    int add_order[2] = {-1, -1};
    int add_count = 0;
    
    if (!has_high_connection && high_node != GND) {
        add_order[add_count++] = 0;
    } else if (!has_low_connection && low_node != GND) {
        add_order[add_count++] = 1;
    }
    
    if (!has_high_connection && high_node != low_node && add_order[0] != 0) {
        add_order[add_count++] = 0;
    } else if (!has_low_connection && high_node != low_node && add_order[0] != 1) {
        add_order[add_count++] = 1;
    }
    
    int clear_idx = 0;
    int add_idx = 0;
    
    while (clear_idx < 5 || add_idx < add_count) {
        if (clear_idx < 5) {
            while (clear_idx < 5 && !to_clear[clear_idx]) clear_idx++;
            if (clear_idx < 5) {
                removeBridgeFromState(node, voltage_sources[clear_idx], false);
                clear_idx++;
            }
        }
        
        if (add_idx < add_count) {
            int which = add_order[add_idx];
            if (which == 0) {
                addBridgeToState(node, high_node, -1, true);
            } else {
                addBridgeToState(node, low_node, -1, true);
            }
            add_idx++;
        }
    }
    
    waitCore2();

    // Find or allocate slot
    int slot = findFakeGpioPinSlot(node);
    if (slot == -1) {
        slot = findFreeFakeGpioPinSlot();
        if (slot == -1) {
            Serial.println("ERROR: No free fake GPIO slots (max 32)");
            configurationInProgress = false;
            return 0;
        }
    }

    FakeGpioPinConfig& pin = fakeGpioPins[slot];

    // Get actual voltages from the nodes
    float v_high = 0.0, v_low = 0.0;
    switch (high_node) {
        case TOP_RAIL:    v_high = globalState.power.topRail; break;
        case BOTTOM_RAIL: v_high = globalState.power.bottomRail; break;
        case DAC0:        v_high = globalState.power.dac0; break;
        case DAC1:        v_high = globalState.power.dac1; break;
        case GND:         v_high = 0.0; break;
    }
    switch (low_node) {
        case TOP_RAIL:    v_low = globalState.power.topRail; break;
        case BOTTOM_RAIL: v_low = globalState.power.bottomRail; break;
        case DAC0:        v_low = globalState.power.dac0; break;
        case DAC1:        v_low = globalState.power.dac1; break;
        case GND:         v_low = 0.0; break;
    }

    pin.active = true;
    pin.node = node;
    pin.v_high = v_high;
    pin.v_low = v_low;
    pin.threshold_high = threshold_high;
    pin.threshold_low = threshold_low;
    pin.mode = 1;  // OUTPUT
    pin.high_voltage_node = high_node;
    pin.low_voltage_node = low_node;

    // Find chip K coordinates
    pin.chip_k_x = -1;
    pin.chip_k_y = -1;
    
    for (int i = 0; i < globalState.connections.numPaths; i++) {
        const pathStruct& path = globalState.connections.paths[i];
        if (path.node1 == node || path.node2 == node) {
            for (int j = 0; j < 4; j++) {
                if (path.chip[j] == CHIP_K) {
                    pin.chip_k_x = path.x[j];
                    pin.chip_k_y = path.y[j];
                    break;
                }
            }
            if (pin.chip_k_x != -1) break;
        }
    }

    pin.current_state = 0;

    // Validate chip K was found
    if ((pin.chip_k_x == -1 || pin.chip_k_y == -1) && tryWithGndChipAlternator == 0) {
        tryWithGndChipAlternator = 1;
        gndChipAlternator = !gndChipAlternator;
        removeBridgeFromState(node, high_node, false);
        removeBridgeFromState(node, low_node, true);
        return fakeGpioConfigOutputNodes(node, high_node, low_node, threshold_high, threshold_low);
    }
    if (pin.chip_k_x == -1 || pin.chip_k_y == -1) {
        Serial.print("ERROR: Chip K not found in routing path for node ");
        Serial.println(node);
        tryWithGndChipAlternator = 0;
        configurationInProgress = false;
        return 0;
    }

    tryWithGndChipAlternator = 0;

    captureCurrentChipXYState(pin.chipXYState);
    pin.hasStoredState = true;
    
    assignFakeGpioToVisualSlot(slot, node);
    
    // Apply initial LOW state to chip K hardware
    int target_node = pin.low_voltage_node;
    int target_x = -1;
    switch (target_node) {
        case TOP_RAIL:    target_x = 4; break;
        case BOTTOM_RAIL: target_x = 5; break;
        case DAC1:        target_x = 6; break;
        case DAC0:        target_x = 7; break;
        case GND:         target_x = 15; break;
    }
    
    if (target_x != -1) {
        rerouteChipK(target_x, pin.chip_k_y, target_node);
        updateFakeGpioReading(slot, 0);
        
        if (debugFakeGpio) {
            Serial.print("Applied initial LOW state to chip K for slot ");
            Serial.print(slot);
            Serial.print(" (node ");
            Serial.print(node);
            Serial.println(")");
        }
    }
    
    configurationInProgress = false;

    return 1;
}

// ============================================================================
// Read/Write Functions
// ============================================================================

int fakeGpioRead(int node) {
    int slot = findFakeGpioPinSlot(node);
    if (slot == -1) {
        if (debugFakeGpio) {
            Serial.print("fakeGpioRead: No fake GPIO configured for node ");
            Serial.println(node);
        }
        return -1;
    }
    
    FakeGpioPinConfig& pin = fakeGpioPins[slot];
    
    if (pin.mode == FAKE_GPIO_OUTPUT) {
        // OUTPUT mode: Return current state
        return pin.current_state;
    } else {
        // INPUT mode: Read from ADC
        // Apply stored chipXY state to route this pin to ADC
        if (!pin.hasStoredState) {
            Serial.println("ERROR: No stored chipXY state for INPUT pin");
            return -1;
        }
        
        applyChipXYState(pin.chipXYState);
        adcCurrentlyConnectedPin = slot;
        
        // Brief settling time for analog signal
        delayMicroseconds(50);
        
        // Read ADC using MicroPython API function
        float voltage = jl_adc_get(0);
        
        if (debugFakeGpio) {
            Serial.print("fakeGpioRead: node=");
            Serial.print(node);
            Serial.print(" slot=");
            Serial.print(slot);
            Serial.print(" voltage=");
            Serial.print(voltage);
            Serial.print("V");
        }
        
        // Apply thresholds with hysteresis
        int reading = -1;  // Default: floating/unknown
        if (voltage >= pin.threshold_high) {
            reading = 1;  // HIGH
        } else if (voltage <= pin.threshold_low) {
            reading = 0;  // LOW
        } else {
            // In hysteresis zone, keep previous state
            reading = pin.current_state;
        }
        
        if (debugFakeGpio) {
            Serial.print(" reading=");
            Serial.println(reading);
        }
        
        pin.current_state = reading;
        updateFakeGpioReading(slot, reading);
        
        return reading;
    }
}

int fakeGpioWrite(int node, int state) {
    int slot = findFakeGpioPinSlot(node);
    if (slot == -1) {
        if (debugFakeGpio) {
            Serial.print("fakeGpioWrite: No fake GPIO configured for node ");
            Serial.println(node);
        }
        return 0;
    }
    
    FakeGpioPinConfig& pin = fakeGpioPins[slot];
    
    if (pin.mode != FAKE_GPIO_OUTPUT) {
        Serial.println("ERROR: Cannot write to INPUT pin");
        return 0;
    }
    
    // Determine which voltage source to connect
    int target_node = (state == 1) ? pin.high_voltage_node : pin.low_voltage_node;
    
    // Map voltage node to chip K X coordinate
    int target_x = -1;
    switch (target_node) {
        case TOP_RAIL:    target_x = 4; break;
        case BOTTOM_RAIL: target_x = 5; break;
        case DAC1:        target_x = 6; break;
        case DAC0:        target_x = 7; break;
        case GND:         target_x = 15; break;
    }
    
    if (target_x == -1) {
        Serial.println("ERROR: Invalid voltage node");
        return 0;
    }
    
    // Fast switch using chip K
    rerouteChipK(target_x, pin.chip_k_y, target_node);
    
    pin.current_state = state;
    updateFakeGpioReading(slot, state);
    
    return 1;
}

// ============================================================================
// Fast Toggle Functions
// ============================================================================

struct FakeGpioToggle {
    bool active;
    int node1, node2;
    int chips[4];
    int x[6], y[6];
    int num_connections;
};

static FakeGpioToggle fakeGpioToggles[16];
static bool fakeGpioTogglesInitialized = false;

static void initFakeGpioToggles() {
    if (fakeGpioTogglesInitialized) return;
    
    for (int i = 0; i < 16; i++) {
        fakeGpioToggles[i].active = false;
        fakeGpioToggles[i].node1 = -1;
        fakeGpioToggles[i].node2 = -1;
        fakeGpioToggles[i].num_connections = 0;
    }
    fakeGpioTogglesInitialized = true;
}

static int findToggleSlot(int node1, int node2) {
    for (int i = 0; i < 16; i++) {
        if (fakeGpioToggles[i].active &&
            ((fakeGpioToggles[i].node1 == node1 && fakeGpioToggles[i].node2 == node2) ||
             (fakeGpioToggles[i].node1 == node2 && fakeGpioToggles[i].node2 == node1))) {
            return i;
        }
    }
    return -1;
}

static int findFreeToggleSlot() {
    for (int i = 0; i < 16; i++) {
        if (!fakeGpioToggles[i].active) {
            return i;
        }
    }
    return -1;
}

int fakeGpioDisconnect(int node1, int node2) {
    initFakeGpioToggles();
    
    if (findToggleSlot(node1, node2) != -1) {
        return 0;  // Already disconnected
    }
    
    extern void bridgesToPaths(int fillUnused, int allowStacking, int startIndex);
    bridgesToPaths(1, 0, 0);  // fillUnused=1, allowStacking=0, startIndex=0 (full refresh)
    
    // Find the path
    int pathIdx = -1;
    for (int i = 0; i < globalState.connections.numPaths; i++) {
        const pathStruct& path = globalState.connections.paths[i];
        if ((path.node1 == node1 && path.node2 == node2) ||
            (path.node1 == node2 && path.node2 == node1)) {
            pathIdx = i;
            break;
        }
    }
    
    if (pathIdx == -1) return 0;
    
    int slot = findFreeToggleSlot();
    if (slot == -1) return 0;
    
    const pathStruct& path = globalState.connections.paths[pathIdx];
    FakeGpioToggle& toggle = fakeGpioToggles[slot];
    
    toggle.active = true;
    toggle.node1 = node1;
    toggle.node2 = node2;
    toggle.num_connections = 0;
    
    for (int i = 0; i < 4; i++) {
        toggle.chips[i] = path.chip[i];
        if (path.chip[i] >= 0) {
            toggle.num_connections++;
        }
    }
    
    for (int i = 0; i < 6; i++) {
        toggle.x[i] = path.x[i];
        toggle.y[i] = path.y[i];
    }
    
    // Disconnect
    for (int i = 0; i < 4 && i < toggle.num_connections; i++) {
        if (toggle.chips[i] >= 0 && toggle.x[i] >= 0 && toggle.y[i] >= 0) {
            sendXYraw(toggle.chips[i], toggle.x[i], toggle.y[i], 0);
        }
    }
    
    return 1;
}

int fakeGpioReconnect(int node1, int node2) {
    initFakeGpioToggles();
    
    int slot = findToggleSlot(node1, node2);
    if (slot == -1) return 0;
    
    FakeGpioToggle& toggle = fakeGpioToggles[slot];
    
    // Reconnect
    for (int i = 0; i < 4 && i < toggle.num_connections; i++) {
        if (toggle.chips[i] >= 0 && toggle.x[i] >= 0 && toggle.y[i] >= 0) {
            sendXYraw(toggle.chips[i], toggle.x[i], toggle.y[i], 1);
        }
    }
    
    toggle.active = false;
    toggle.node1 = -1;
    toggle.node2 = -1;
    toggle.num_connections = 0;
    
    return 1;
}

