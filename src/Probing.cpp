#include "Probing.h"
#include "CH446Q.h"
#include "FileParsing.h"
#include "JumperlOS.h"
#include "JumperlessDefines.h"
#include "LEDs.h"
#include "MatrixState.h"
#include "NetManager.h"
#include "NetsToChipConnections.h"
#include "Peripherals.h"
#include "States.h"
// #include "AdcUsb.h"
#include "Commands.h"
#include "Graphics.h"
#include "PersistentStuff.h"
#include "RotaryEncoder.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"

#include <EEPROM.h>
// #include <FastLED.h>
#include "ArduinoStuff.h"
#include <algorithm>

#include "Highlighting.h"
#include "PersistentStuff.h"
#include "Python_Proper.h"
#include "config.h"
#include "externVars.h"
#include "oled.h"

// Button timing constants
#define BUTTON_SETTLE_US 22
#define BUTTON_SETTLE_SHORT_US 4

// ============================================================================
// ProbeButton Implementation (class declared in Probing.h)
// High-frequency button checking service for instant response
// ============================================================================

// Static member initialization
ProbeButton* ProbeButton::instance = nullptr;

ProbeButton& ProbeButton::getInstance( ) {
    if ( instance == nullptr ) {
        instance = new ProbeButton( );
    }
    return *instance;
}

ProbeButton::ProbeButton( ) {
    // Initialize button state
}

/**
 * @brief High-frequency service method - checks button hardware with new blocking behavior
 *
 * New behavior:
 * 1. Checks hardware frequently (respects rate limiting)
 * 2. When press detected: blocks subsequent presses for blockDurationMs (default 1s)
 * 3. Block clears when: button released OR block timer expires
 * 4. Quick successive clicks register (each release clears block)
 * 5. Holding button registers once (block prevents re-trigger)
 * 6. Tracks continuous hold time and sets CONNECT_HELD/REMOVE_HELD flags
 */
ServiceStatus ProbeButton::service( ) {
    lastStatus = ServiceStatus::IDLE;

    // Rate limiting - only check hardware at specified interval
    unsigned long now = millis( );
    if ( now - lastCheckTime < checkIntervalMs ) {
        return lastStatus; // Not time to check yet
    }
    lastCheckTime = now;

    // ALWAYS read hardware state
    int newState = checkProbeButtonHardware( );

    // Serial.print(newState);
    // Serial.print(" ");
    // Serial.println(currentButtonState);
    // Serial.flush();

    // ========================================================================
    // BUTTON RELEASED - Clear state with debounce protection
    // ========================================================================
    if ( newState == 0 ) {
        // Button released!
        if ( currentButtonState != 0 ) {
            blockProbeButton = 0;
            blockProbeButtonTimer = 0;
            lastButtonState = currentButtonState;
            currentButtonState = 0;
            buttonChanged = true;
            lastStatus = ServiceStatus::BUSY;
        }

        // Clear hold state immediately
        connectHeld = false;
        removeHeld = false;
        connectHoldTime = 0;
        removeHoldTime = 0;
        pressStartTime = 0;

        // CRITICAL: Only clear block if minimum block time has elapsed
        // This prevents button bounce from causing rapid re-triggering
        if ( isBlocked && blockStartTime > 0 ) {
            unsigned long blockElapsed = now - blockStartTime;
            if ( blockElapsed >= minimumBlockMs ) {
                // Minimum block time elapsed, safe to clear for next press
                isBlocked = false;
                blockStartTime = 0;
                blockProbeButton = 0;
                blockProbeButtonTimer = 0;
            }
            // else: Keep block active to prevent bounce re-trigger
        } else {
            // Not blocked, clear everything
            blockProbeButton = 10;
            blockProbeButtonTimer = 0;
        }

        return lastStatus;
    }

    // ========================================================================
    // BUTTON PRESSED - Handle blocking, press detection, and hold tracking
    // ========================================================================

    // Check if block timer has expired
    if ( isBlocked && ( now - blockStartTime >= blockDurationMs ) ) {
        isBlocked = false;
        blockStartTime = 0;
    }

    // Detect state changes
    bool stateChanged = ( newState != currentButtonState );

    if ( stateChanged ) {
        lastButtonState = currentButtonState;
        currentButtonState = newState;
        buttonChanged = true;
        lastStatus = ServiceStatus::BUSY;
    } else {
        buttonChanged = false;
    }

    // ========================================================================
    // PRESS DETECTION - Register press event if not blocked
    // ========================================================================
    if ( !isBlocked ) {
        // Register a press event when:
        // 1. Transitioning from RELEASED (0) to any PRESSED (1 or 2)
        // 2. Switching between different button types (1↔2)
        if ( stateChanged && newState > 0 &&
             ( lastButtonState == 0 || ( lastButtonState > 0 && lastButtonState != newState ) ) ) {

            buttonPress = newState;

            // Start block period
            isBlocked = true;
            blockStartTime = now;
            pressStartTime = now;

            // Set old global blocking variables for backward compatibility
            blockProbeButton = blockDurationMs;
            blockProbeButtonTimer = now;

            lastStatus = ServiceStatus::BUSY;
        }
    }

    // ========================================================================
    // HOLD TRACKING - Update continuous hold time and flags
    // ========================================================================
    if ( currentButtonState > 0 && pressStartTime > 0 ) {
        unsigned long holdDuration = now - pressStartTime;

        if ( currentButtonState == 2 ) {
            // Connect button (2)
            connectHoldTime = holdDuration;

            // Set hold flag if threshold reached
            if ( !connectHeld && holdDuration >= connectHoldThresholdMs ) {
                connectHeld = true;
                lastStatus = ServiceStatus::BUSY;
            }
        } else if ( currentButtonState == 1 ) {
            // Remove button (1)
            removeHoldTime = holdDuration;

            // Set hold flag if threshold reached
            if ( !removeHeld && holdDuration >= removeHoldThresholdMs ) {
                removeHeld = true;
                lastStatus = ServiceStatus::BUSY;
            }
        }
    }

    return lastStatus;
}

/**
 * @brief Get button press event (consumes the event)
 */
int ProbeButton::getButtonPress( ) {
    int press = buttonPress;

    if ( press != 0 ) {
        blockProbeButton = 300;
        blockProbeButtonTimer = millis( );
    }

    buttonPress = 0; // Clear after reading
    return press;
}

/**
 * @brief Direct hardware button check - fast and non-blocking
 *
 * @return 0 = neither pressed, 1 = remove button, 2 = connect button
 *
 * NOTE: Blocking logic is now handled at the service() level
 */
int ProbeButton::checkProbeButtonHardware( void ) {
    extern struct config jumperlessConfig;

    // Wait if LEDs are being updated
    while ( showingProbeLEDs == 1 ) {
        // Spin wait - LEDs are fast
    }

    core1busy = true;
    checkingButton = 1;

    int buttonState = 0;
    int buttonState2 = 0;
    int buttonState3 = 0;
    int returnState = 0;

    gpio_function_t lastProbeButtonFunction = gpio_get_function( PROBE_LED_PIN );

    // Get pin references from Probing singleton
    Probing& p = Probing::getInstance( );

    // Button reading sequence with proper timing
    gpio_set_dir( BUTTON_PIN, false );
    gpio_set_function( PROBE_LED_PIN, GPIO_FUNC_SIO );
    gpio_disable_pulls( PROBE_LED_PIN );
    gpio_set_dir( PROBE_LED_PIN, false );
    delayMicroseconds( BUTTON_SETTLE_US );

    gpio_set_dir( PROBE_PIN, true );
    gpio_put( PROBE_PIN, true );
    gpio_set_pulls( BUTTON_PIN, false, true );
    gpio_set_input_enabled( BUTTON_PIN, true );
    buttonState = gpio_get( BUTTON_PIN );
    gpio_set_input_enabled( BUTTON_PIN, false );

    delayMicroseconds( BUTTON_SETTLE_US );

    gpio_set_pulls( BUTTON_PIN, true, false );
    gpio_set_input_enabled( BUTTON_PIN, true );
    delayMicroseconds( BUTTON_SETTLE_US );
    buttonState2 = gpio_get( BUTTON_PIN );
    gpio_set_input_enabled( BUTTON_PIN, false );

    delayMicroseconds( BUTTON_SETTLE_US );

    gpio_set_pulls( BUTTON_PIN, false, false );
    delayMicroseconds( BUTTON_SETTLE_US );

    gpio_set_dir( BUTTON_PIN, true );
    gpio_put( BUTTON_PIN, false );
    delayMicroseconds( BUTTON_SETTLE_SHORT_US );

    gpio_set_dir( BUTTON_PIN, false );
    gpio_set_pulls( BUTTON_PIN, false, true );
    gpio_set_input_enabled( BUTTON_PIN, true );
    buttonState3 = gpio_get( BUTTON_PIN );
    gpio_set_input_enabled( BUTTON_PIN, false );

    gpio_set_pulls( BUTTON_PIN, false, false );
    gpio_set_function( PROBE_LED_PIN, lastProbeButtonFunction );

    checkingButton = 0;
    core1busy = false;

    // Determine button state (handles probe revision)
    if ( buttonState == 1 && buttonState2 == 1 && buttonState3 == 1 ) {
        returnState = ( jumperlessConfig.hardware.probe_revision >= 4 ) ? 2 : 1;
    } else if ( buttonState == 0 && buttonState2 == 0 && buttonState3 == 0 ) {
        returnState = ( jumperlessConfig.hardware.probe_revision >= 4 ) ? 1 : 2;
    }

    return returnState;
}

// Global reference for clean syntax
ProbeButton& probeButton = ProbeButton::getInstance( );

// ============================================================================
// Probing Class Implementation
// ============================================================================

// Static member initialization
Probing* Probing::instance = nullptr;

Probing& Probing::getInstance( ) {
    if ( instance == nullptr ) {
        instance = new Probing( );
    }
    return *instance;
}

Probing::Probing( ) {
    // Initialize probe row maps
    int probeRowMapInit[ 108 ] = {
        -1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, TOP_RAIL, GND,
        BOTTOM_RAIL, GND, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60,
        NANO_D1, NANO_D0, NANO_RESET_1, GND, NANO_D2, NANO_D3, NANO_D4, NANO_D5, NANO_D6, NANO_D7, NANO_D8, NANO_D9, NANO_D10, NANO_D11, NANO_D12,
        NANO_D13, NANO_3V3, NANO_AREF, NANO_A0, NANO_A1, NANO_A2, NANO_A3, NANO_A4, NANO_A5, NANO_A6, NANO_A7, NANO_5V, NANO_RESET_0, GND, NANO_VIN,
        LOGO_PAD_BOTTOM, LOGO_PAD_TOP, GPIO_PAD, DAC_PAD, ADC_PAD, BUILDING_PAD_TOP, BUILDING_PAD_BOTTOM, -1, -1, -1, -1 };

    int probeRowMapByPadInit[ 108 ] = {
        -1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, TOP_RAIL, TOP_RAIL_GND,
        BOTTOM_RAIL, BOTTOM_RAIL_GND, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60,
        NANO_D1, NANO_D0, NANO_RESET_1, NANO_GND_1, NANO_D2, NANO_D3, NANO_D4, NANO_D5, NANO_D6, NANO_D7, NANO_D8, NANO_D9, NANO_D10, NANO_D11, NANO_D12,
        NANO_D13, NANO_3V3, NANO_AREF, NANO_A0, NANO_A1, NANO_A2, NANO_A3, NANO_A4, NANO_A5, NANO_A6, NANO_A7, NANO_5V, NANO_RESET_0, NANO_GND_0, NANO_VIN,
        LOGO_PAD_BOTTOM, LOGO_PAD_TOP, GPIO_PAD, DAC_PAD, ADC_PAD, BUILDING_PAD_TOP, BUILDING_PAD_BOTTOM, -1, -1, -1, -1 };

    memcpy( probeRowMap, probeRowMapInit, sizeof( probeRowMap ) );
    memcpy( probeRowMapByPad, probeRowMapByPadInit, sizeof( probeRowMapByPad ) );

    // Initialize other defaults
    switchPosition = 1;
    lastSwitchPositions[ 0 ] = 1;
    lastSwitchPositions[ 1 ] = 1;
    lastSwitchPositions[ 2 ] = 1;
}

/**
 * @brief Handle probe button actions and toggle logic
 *
 * This is called from service() and handles all the probe button
 * interactions, toggle logic, and probe mode triggering.
 */
void Probing::handleProbeButtonActions( ) {
    extern unsigned long startupTimers[];
    extern int probeToggle( void );        // Defined in Peripherals.cpp
    extern class ProbeButton& probeButton; // High-frequency button service

    // Handle probe toggle when brightenedNet is active
    if ( brightenedNet > 0 ) {
        int probeToggleResult = probeToggle( );
        if ( probeToggleResult >= 0 && brightenedNet > 0 ) {
            blockProbeButton = gpioToggleFrequency;
            blockProbeButtonTimer = millis( );
        } else if ( probeToggleResult == -5 ) {
            if ( firstConnection > 0 ) {
                if ( warningNet == brightenedNet && warningTimeout > 0 ) {
                    // Trigger probe clear mode
                    warningTimeout = 0;
                    connectOrClearProbe = 0;
                    showProbeLEDs = 2;
                    probingTimer = millis( );
                    startupTimers[ 0 ] = millis( );

                    // Run probe mode directly instead of goto (it handles button state internally)
                    probeMode( 0, firstConnection );
                    highlighting.clearHighlighting( );

                } else {
                    highlighting.warnNet( firstConnection );
                    warningTimeout = 1500;
                    warningTimer = millis( );
                }
            }
            blockProbeButton = 800;
            blockProbeButtonTimer = millis( );
        } else if ( probeToggleResult == -3 || probeToggleResult == -2 ) {
            blockProbeButton = 800;
            blockProbeButtonTimer = millis( );
        } else if ( probeToggleResult == -4 ) {
            firstConnection = -1;
            blockProbeButton = 800;
            blockProbeButtonTimer = millis( );
        }
    } else {
        firstConnection = -1;
    }

    // Check for button press events from high-frequency ProbeButton service
    int buttonPress = probeButton.getButtonPress( );

    if ( buttonPress != 0 ) {
        // Button was pressed - stored state changed
        lastProbeButton = buttonPress;

        if ( buttonPress == 2 ) {
            // Connect button pressed - trigger probe connect mode
            connectOrClearProbe = 1;
            showProbeLEDs = 1;
            probingTimer = millis( );
            brightenedNet = 0;
            blockProbeButtonTimer = millis( );
            blockProbeButton = 1000;
            core1passthrough = 0;

            // Run probe mode directly (it handles button blocking/clearing internally)
            probeMode( 1, firstConnection );
            highlighting.clearHighlighting( );

        } else if ( buttonPress == 1 ) {
            // Remove button pressed - trigger probe clear mode
            startupTimers[ 0 ] = millis( );
            connectOrClearProbe = 0;
            showProbeLEDs = 2;
            probingTimer = millis( );
            brightenedNet = 0;
            blockProbeButtonTimer = millis( );
            blockProbeButton = 1000;
            core1passthrough = 0;

            // Run probe mode directly (it handles button blocking/clearing internally)
            probeMode( 0, firstConnection );
            highlighting.clearHighlighting( );
        }
    }
}

// ============================================================================
// ProbeSwitch Service Implementation - LOW priority
// ============================================================================

ProbeSwitch* ProbeSwitch::instance = nullptr;

ProbeSwitch& ProbeSwitch::getInstance( ) {
    if ( instance == nullptr ) {
        instance = new ProbeSwitch( );
    }
    return *instance;
}

/**
 * @brief Service method for probe switch checking
 * Checks the 3-position switch state.
 * LOW priority - not time-critical, can run infrequently.
 */
ServiceStatus ProbeSwitch::service( ) {
    lastStatus = ServiceStatus::IDLE;

    // Check switch position (cheap: digital read)
    Probing::getInstance( ).checkSwitchPosition( );

    return lastStatus;
}

// ============================================================================
// ProbePads Service Implementation - LOW priority
// ============================================================================

ProbePads* ProbePads::instance = nullptr;

ProbePads& ProbePads::getInstance( ) {
    if ( instance == nullptr ) {
        instance = new ProbePads( );
    }
    return *instance;
}

/**
 * @brief Service method for pad checking
 * Does expensive ADC pad reading.
 * LOW priority - expensive and not time-critical.
 * Rate-limited to 20Hz (every 50ms) to reduce overhead.
 */
ServiceStatus ProbePads::service( ) {
    lastStatus = ServiceStatus::IDLE;

    // Rate limit to 20Hz (every 50ms) - pad checking is expensive
    unsigned long now = millis( );
    if ( now - lastCheckTime < 50 ) {
        return lastStatus; // Skip this iteration
    }
    lastCheckTime = now;

    // Check pads (expensive: multiple ADC readings)
    Probing::getInstance( ).checkPads( );
    lastStatus = ServiceStatus::BUSY;

    return lastStatus;
}

// ============================================================================
// Probing Service Implementation - HIGH priority
// ============================================================================

/**
 * @brief Main service method for probing system
 *
 * This is called each loop iteration and handles:
 * - Reading probe position (HIGH priority - responsive)
 * - Probe button actions and toggle logic
 *
 * OPTIMIZATION: Expensive operations (checkPads, checkSwitchPosition) are now
 * handled by separate LOW priority services, keeping this service fast.
 */
ServiceStatus Probing::service( ) {
    // Update last status for base class tracking
    lastStatus = ServiceStatus::IDLE;

    // Check if probe menu is active (blocking)
    if ( sfProbeMenu != 0 || inPadMenu != 0 ) {
        lastStatus = ServiceStatus::BLOCKING;
        return lastStatus;
    }

    // Rate limiting for probe reading
    // Run probe check every ~10ms (100Hz) for responsive probing
    static unsigned long lastProbeCheckTime = 0;
    unsigned long now = millis( );
    bool shouldCheckProbe = ( now - lastProbeCheckTime >= 10 ); // 10ms = 100Hz

    int probeReading = lastProbeReading; // Use cached value by default

    if ( shouldCheckProbe ) {
        lastProbeCheckTime = now;

        // Read probe position (moderately expensive: ADC + mapping)
        probeReading = justReadProbe( true );
        lastProbeReading = probeReading;

        // If we did any work, mark as BUSY
        if ( probeReading > 0 ) {
            lastStatus = ServiceStatus::BUSY;
        }
    }

    // Handle probe button actions and toggle logic (always run - critical for UX)
    handleProbeButtonActions( );

    return lastStatus;
}

// Global service references
ProbeSwitch& probeSwitch = ProbeSwitch::getInstance( );
ProbePads& probePads = ProbePads::getInstance( );

// Backward compatibility - create references to singleton members
volatile int& sfProbeMenu = Probing::getInstance( ).sfProbeMenu;
unsigned long& probingTimer = Probing::getInstance( ).probingTimer;
int& probePin = Probing::getInstance( ).probePin;
int& buttonPin = Probing::getInstance( ).buttonPin;
volatile unsigned long& blockProbeButton = Probing::getInstance( ).blockProbeButton;
volatile unsigned long& blockProbeButtonTimer = Probing::getInstance( ).blockProbeButtonTimer;
volatile int& connectOrClearProbe = Probing::getInstance( ).connectOrClearProbe;
int& node1or2 = Probing::getInstance( ).node1or2;
int& probeHighlight = Probing::getInstance( ).probeHighlight;
volatile int& removeFade = Probing::getInstance( ).removeFade;
volatile bool& bufferPowerConnected = Probing::getInstance( ).bufferPowerConnected;
int& debugProbing = Probing::getInstance( ).debugProbing;
volatile int& showingProbeLEDs = Probing::getInstance( ).showingProbeLEDs;
int& switchPosition = Probing::getInstance( ).switchPosition;
int& probePowerDAC = Probing::getInstance( ).probePowerDAC;
int& lastProbePowerDAC = Probing::getInstance( ).lastProbePowerDAC;
bool& probePowerDACChanged = Probing::getInstance( ).probePowerDACChanged;
int& showProbeCurrent = Probing::getInstance( ).showProbeCurrent;

// Additional references for global access
volatile int& inPadMenu = Probing::getInstance( ).inPadMenu;
volatile int& checkingButton = Probing::getInstance( ).checkingButton;
int& lastProbeLEDs = Probing::getInstance( ).lastProbeLEDs;

// Export probe maps as references
int ( &probeRowMap )[ 108 ] = Probing::getInstance( ).probeRowMap;
int ( &probeRowMapByPad )[ 108 ] = Probing::getInstance( ).probeRowMapByPad;

// Export pad settings
int ( &logoTopSetting )[ 2 ] = Probing::getInstance( ).logoTopSetting;
int ( &logoBottomSetting )[ 2 ] = Probing::getInstance( ).logoBottomSetting;
int ( &buildingTopSetting )[ 2 ] = Probing::getInstance( ).buildingTopSetting;
int ( &buildingBottomSetting )[ 2 ] = Probing::getInstance( ).buildingBottomSetting;

static int resolveLogoPadAssignment( int configValue, int defaultNode ) {
    switch ( configValue ) {
    case -1:
        return -1;
    case 0:
        return RP_UART_TX;
    case 1:
        return RP_UART_RX;
    case 25:
        return ISENSE_PLUS;
    case 26:
        return ISENSE_MINUS;
    default:
        return defaultNode;
    }
}

static int nodeToLogoPadConfig( int node, int fallbackConfig ) {
    switch ( node ) {
    case RP_UART_TX:
        return 0;
    case RP_UART_RX:
        return 1;
    case ISENSE_PLUS:
        return 25;
    case ISENSE_MINUS:
        return 26;
    case -1:
        return -1;
    default:
        return fallbackConfig;
    }
}

// ============================================================================
// Legacy Global Variables (not moved to class yet)
// ============================================================================

#define SPACE_FROM_LEFT 6

int lastReadRaw = 0;

int probeHalfPeriodus = 20;

unsigned long probeTimeout = 0;

int connectedRowsIndex = 0;
int connectedRows[ 32 ] = { -1 };

int nodesToConnect[ 2 ] = { -1, -1 };

unsigned long probeButtonTimer = 0;

int voltageSelection = TOP_RAIL;

int rainbowList[ 13 ][ 3 ] = {
    { 40, 50, 80 }, { 88, 33, 70 }, { 30, 15, 45 }, { 8, 27, 45 }, { 45, 18, 19 }, { 35, 42, 5 }, { 02, 45, 35 }, { 18, 25, 45 }, { 40, 12, 45 }, { 10, 32, 45 }, { 18, 5, 43 }, { 45, 28, 13 }, { 8, 12, 8 } };

int checkingPads = 0;

// Special function option colors - used in encoder cursor and probe menus
uint32_t sfOptionColors[ 12 ] = {
    0x09000a,
    0x0d0500,
    0x000809,
    0x040f00,
    0x000f03,
    0x00030d,
    0x080a00,
    0x030013,
    0x000a03,
    0x00030a,
    0x040010,
    0x070006,
};

uint32_t deleteFade[ 13 ] = { 0x371f16, 0x28160b, 0x191307, 0x141005, 0x0f0901,
                              0x090300, 0x050200, 0x030100, 0x030000, 0x020000,
                              0x010000, 0x000000, 0x000000 };

int fadeIndex = 0;

// Global encoder cursor position for override in LED display functions
volatile int globalEncoderCursorNode = -1;             // -1 = cursor hidden
volatile int globalEncoderCursorInHeader = 0;          // 1 if in nano header
volatile uint32_t globalEncoderCursorColor = 0x4500e8; // Cursor color

int Probing::probeMode( int setOrClear, int firstConnection ) {

    // clearColorOverrides(1, 1, 0);

    // Block button and clear any pre-existing state to prevent double-detection
    blockProbeButton = 3000;
    blockProbeButtonTimer = millis( );
    probeButton.clearButtonState( ); // Clear the button state that triggered entry

    // Enable text layer for special nodes display (UART, Current, etc.)

    /* clang-format off */

    int deleteMisses[ 20 ] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    };

    /* clang-format on */

    int deleteMissesIndex = 0;

    int connectionsThisSession = 0; // Track total connections made this probe mode session

restartProbing:

    probeActive = 1;
    brightenNet( -1 );

    if ( switchPosition == 0 && globalState.hasConnection( probePowerDAC == 0 ? DAC0 : DAC1, ROUTABLE_BUFFER_IN ) ) {
        changeTerminalColor( 197 );
        Serial.println( "  Switch is in Measure mode!\n\r  Set switch to Select mode for best results\n\r" );
        Serial.flush( );
    }

    if ( setOrClear == 1 ) {

        changeTerminalColor( 45 );
        Serial.println( "\n\r\t connect nodes\n\r" );
        Serial.flush( );
        changeTerminalColor( -1 );
        rawOtherColors[ 1 ] = 0x4500e8;

    } else {

        changeTerminalColor( 202 );
        Serial.println( "\n\r\t clear nodes\n\r" );
        Serial.flush( );
        changeTerminalColor( -1 );
        rawOtherColors[ 1 ] = 0x6644A8;
    }

restartProbingNoPrint:

    if ( setOrClear == 1 && firstConnection == -1 ) {
        oled.clearPrintShow( "connect nodes", 1, true, true, true );
    } else if ( setOrClear == 0 && firstConnection == -1 ) {
        oled.clearPrintShow( "clear nodes", 1, true, true, true );
    }

    clearColorOverrides( 1, 1, 0 );
    routableBufferPower( 1, 1 );

    probeHighlight = -1;

    int numberOfLocalChanges = 0;

    connectOrClearProbe = setOrClear;

    int row[ 16 ] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    row[ 1 ] = -2;
    row[ 0 ] = -2;

    probeTimeout = millis( );

    if ( setOrClear == 0 ) {
        probeButtonTimer = millis( );
    }

    if ( setOrClear == 1 ) {
        showProbeLEDs = 1;
    } else {
        showProbeLEDs = 2;
    }

    connectOrClearProbe = setOrClear;

    showLEDsCore2 = 1;
    unsigned long doubleSelectTimeout = millis( );
    int doubleSelectCountdown = 0;

    int lastProbedRows[ 4 ] = { 0, 0, 0, 0 };
    unsigned long fadeTimer = millis( );
    int fadeClear = -1;

    // Encoder support for node selection with special function zones
    EncoderAccelerator encoderAccel = EncoderAccelerator::Slow( ); // Use slow preset for precise node selection
    long lastEncoderPosition = encoderPosition;
    float encoderAccumulator = 0.0f; // Fractional position accumulator

    // Navigation zones: 0=Breadboard, 1=NanoHeader, 2=Rails, 3=DAC, 4=ADC, 5=GPIO, 6=UART
    enum CursorZone { ZONE_BREADBOARD = 0,
                      ZONE_NANO = 1,
                      ZONE_RAILS = 2,
                      ZONE_DAC = 3,
                      ZONE_ADC = 4,
                      ZONE_GPIO = 5,
                      ZONE_UART = 6,
                      ZONE_CURRENT = 7 };

    // Static variable to persist cursor position between selections within same probe mode session
    static int persistentEncoderCursorNode = -1;
    static int persistentCursorZone = ZONE_BREADBOARD;
    static int persistentSubIndex = 0;  // For multi-item zones (DAC 0/1, ADC 0-4, etc.)
    static bool firstProbeEntry = true; // Track first entry to reset position

    // Initialize cursor position on first entry or use persistent position
    int encoderCursorNode = firstProbeEntry ? 14 : persistentEncoderCursorNode; // Start at row 15 (0-indexed = 14)
    int cursorZone = firstProbeEntry ? ZONE_BREADBOARD : persistentCursorZone;
    int subIndex = firstProbeEntry ? 0 : persistentSubIndex; // Index within special function zone
    firstProbeEntry = false;                                 // After first use, persist position

    int lastEncoderCursorNode = -1; // Track last position to clear it
    int lastCursorZone = -1;
    int lastSubIndex = -1;
    unsigned long lastEncoderMovement = millis( ); // Time of last encoder movement
    unsigned long encoderHideTimeout = 5000;       // Hide cursor after 2 seconds of no movement
    bool encoderCursorVisible = false;             // Whether cursor is currently shown

    // Set rotary divider for good responsiveness during probing
    int savedRotaryDivider = rotaryDivider;
    rotaryDivider = 3;

    blockProbeButton = 1000;
    blockProbeButtonTimer = millis( );

    unsigned long probeModeStartTime = millis( );

    //! this is the main loop for probing
    while ( Serial.available( ) == 0 && ( millis( ) - probeTimeout ) < 6200 ) {
        delayMicroseconds( 200 ); // Reduced from 500 for faster encoder response

        // Keep critical services (like ProbeButton) running during blocking probeMode
        jOS.serviceCritical( );
        // rotaryEncoderStuff();  // Update encoder state

        connectedRowsIndex = 0;

        // ======= ENCODER-BASED NODE SELECTION WITH SPECIAL FUNCTION ZONES =======
        // Check for encoder movement
        long currentEncoderPosition = encoderPosition;
        long encoderDelta = -( currentEncoderPosition - lastEncoderPosition );

        if ( millis( ) - probeModeStartTime < 100 ) {
            lastEncoderPosition = encoderPosition;
        }

        // Don't clear color overrides here - they need to persist while cursor is visible in zones
        // Overrides are cleared when changing zones or on timeout
        if ( encoderDelta != 0 && ( millis( ) - probeModeStartTime > 100 ) ) {
            // Encoder moved - show cursor and reset timeout
            lastEncoderMovement = millis( );
            encoderCursorVisible = true;

            // Get accelerated delta
            float accelDelta = encoderAccel.getAcceleratedDelta( encoderDelta );
            encoderAccumulator += accelDelta;

            // Convert accumulated movement to integer steps
            int steps = (int)encoderAccumulator;
            if ( steps != 0 ) {
                encoderAccumulator -= steps; // Keep fractional part

                // Save last position for clearing (before we move)
                lastEncoderCursorNode = encoderCursorNode;
                lastCursorZone = cursorZone;
                lastSubIndex = subIndex;

                // Navigate through zones
                if ( steps < 0 ) {
                    // Moving down (counter-clockwise)
                    if ( cursorZone == ZONE_BREADBOARD ) {
                        encoderCursorNode += steps;
                        if ( encoderCursorNode < 0 ) {
                            // Wrap to last special functions zone (UART)
                            cursorZone = ZONE_CURRENT;
                            subIndex = 1; // UART RX
                            encoderCursorNode = -1;
                        }
                    } else if ( cursorZone == ZONE_NANO ) {
                        encoderCursorNode += steps;
                        if ( encoderCursorNode < NANO_D0 ) {
                            // Go to breadboard
                            cursorZone = ZONE_BREADBOARD;
                            encoderCursorNode = 59;
                        }
                    } else {
                        // Navigate within special function zones
                        subIndex += steps;
                        if ( subIndex < 0 ) {
                            // Move to previous zone
                            cursorZone--;
                            if ( cursorZone < ZONE_RAILS ) {
                                cursorZone = ZONE_NANO;
                                encoderCursorNode = NANO_A7;
                            } else {
                                // Set subIndex to max for new zone
                                switch ( cursorZone ) {
                                case ZONE_RAILS:
                                    subIndex = 2;
                                    break; // 3 rails (TOP, BOTTOM, GND)
                                case ZONE_DAC:
                                    subIndex = 1;
                                    break; // 2 DACs
                                case ZONE_ADC:
                                    subIndex = 5;
                                    break; // 6 ADCs (0-4, 7)
                                case ZONE_GPIO:
                                    subIndex = 7;
                                    break; // 8 GPIOs
                                case ZONE_UART:
                                    subIndex = 1;
                                    break; // 2 UART (TX, RX)
                                case ZONE_CURRENT:
                                    subIndex = 1;
                                    break; // Current +/-
                                }
                            }
                        }
                    }
                } else {
                    // Moving up (clockwise)
                    if ( cursorZone == ZONE_BREADBOARD ) {
                        encoderCursorNode += steps;
                        if ( encoderCursorNode > 59 ) {
                            // Go to nano header
                            cursorZone = ZONE_NANO;
                            encoderCursorNode = NANO_D0;
                        }
                    } else if ( cursorZone == ZONE_NANO ) {
                        encoderCursorNode += steps;
                        if ( encoderCursorNode > NANO_A7 ) {
                            // Go to special functions (Rails)
                            cursorZone = ZONE_RAILS;
                            subIndex = 0;
                            encoderCursorNode = -1;
                        }
                    } else {
                        // Navigate within special function zones
                        int maxSubIndex = 0;
                        switch ( cursorZone ) {
                        case ZONE_RAILS:
                            maxSubIndex = 2;
                            break;
                        case ZONE_DAC:
                            maxSubIndex = 1;
                            break;
                        case ZONE_ADC:
                            maxSubIndex = 5;
                            break;
                        case ZONE_GPIO:
                            maxSubIndex = 7;
                            break;
                        case ZONE_UART:
                            maxSubIndex = 1;
                            break;
                        case ZONE_CURRENT:
                            maxSubIndex = 1;
                            break;
                        }

                        subIndex += steps;
                        if ( subIndex > maxSubIndex ) {
                            // Move to next zone
                            cursorZone++;
                            if ( cursorZone > ZONE_CURRENT ) {
                                // Wrap back to breadboard
                                cursorZone = ZONE_BREADBOARD;
                                encoderCursorNode = 0;
                            } else {
                                subIndex = 0;
                            }
                        }
                    }
                }

                // ========== ATOMIC LED UPDATE - PAUSE CORE2 WHILE MAKING CHANGES ==========
                pauseCore2 = 1; // Pause Core 2 LED updates

                // 1. Clear previous highlighting if zone or subIndex changed
                bool zoneChanged = ( lastCursorZone != cursorZone );
                bool subIndexChanged = ( cursorZone >= ZONE_RAILS && lastSubIndex != subIndex );

                if ( zoneChanged || subIndexChanged ) {
                    Highlighting::getInstance( ).clearHighlighting( );
                    clearLEDsExceptRails( );
                    b.clear( );
                    showLEDsCore2 = 2;
                    clearColorOverrides( 1, 1, 0 );

                    // Clear inPadMenu when leaving special function zones
                    if ( lastCursorZone >= ZONE_RAILS && cursorZone < ZONE_RAILS ) {
                        inPadMenu = 0;
                    }
                }

                // 2. Clear previous cursor position only if in same zone
                if ( !zoneChanged ) {
                    if ( cursorZone == ZONE_BREADBOARD && lastEncoderCursorNode >= 0 && lastEncoderCursorNode != encoderCursorNode ) {
                        b.printRawRow( 0b00000100, lastEncoderCursorNode, 0x000000, 0x000000 );
                    } else if ( cursorZone == ZONE_NANO && lastEncoderCursorNode >= 0 && lastEncoderCursorNode != encoderCursorNode ) {
                        int pixel = getNanoHeaderPixel( lastEncoderCursorNode );
                        if ( pixel >= 0 )
                            leds.setPixelColor( pixel, 0x000000 );
                    }
                }

                // 3. Set cursor color based on mode
                uint32_t cursorColor = setOrClear == 1 ? 0x250035 : 0x362404;
                uint32_t dimColor = setOrClear == 1 ? 0x080205 : 0x080500;

                ///[active/dim][setOrClear][index]
                uint32_t cursorColors[ 2 ][ 2 ][ 8 ] = {
                    {
                        // active

                        // clear
                        { 0x391912, 0x3a1810, 0x3b1608, 0x3d1411, 0x3f1214, 0x411015, 0x450e16, 0x480c14 },

                        // set
                        { 0x191229, 0x18102a, 0x16082b, 0x14112d, 0x12142f, 0x101531, 0x0e1635, 0x0c1438 },

                    },
                    {
                        // dim
                        // clear
                        { 0x040302, 0x040302, 0x040202, 0x050302, 0x050401, 0x050301, 0x040302, 0x040302 },
                        

                        // set
                        { 0x050304, 0x050204, 0x040204, 0x030205, 0x040105, 0x050104, 0x040203, 0x030305 },

                    } };

                globalEncoderCursorColor = cursorColor;

                // 4. Get actual node and display based on zone
                int actualNode = -1;
                char displayName[ 32 ] = "";

                if ( cursorZone == ZONE_BREADBOARD ) {
                    actualNode = encoderCursorNode + 1;
                    strcpy( displayName, definesToChar( actualNode, 0 ) );
                    b.printRawRow( 0b00000100, encoderCursorNode, 0x121215, cursorColor );
                    globalEncoderCursorNode = encoderCursorNode;
                    globalEncoderCursorInHeader = 0;
                } else if ( cursorZone == ZONE_NANO ) {
                    actualNode = encoderCursorNode;
                    strcpy( displayName, definesToChar( actualNode, 0 ) );
                    int pixel = getNanoHeaderPixel( encoderCursorNode );
                    if ( pixel >= 0 )
                        leds.setPixelColor( pixel, cursorColor );
                    globalEncoderCursorNode = encoderCursorNode;
                    globalEncoderCursorInHeader = 1;
                } else if ( cursorZone == ZONE_RAILS ) {
                    // Display rails: TOP_RAIL, BOTTOM_RAIL, GND - one at a time
                    const char* railNames[ 3 ] = { "  Top    Rail", " Bottom  Rail", "         GND" };
                    const char* railDisplayNames[ 3 ] = { "Top Rail", "Bottom Rail", "GND" };
                    int railBrightened[ 3 ] = { 0, 2, 1 };
                    uint32_t railColors[ 3 ] = { 0x200010, 0x150015, 0x003005 };
                    int railNodes[ 3 ] = { TOP_RAIL, BOTTOM_RAIL, GND };
                    actualNode = railNodes[ subIndex ];
                    strcpy( displayName, railDisplayNames[ subIndex ] );

                    // DON'T set inPadMenu for rails - we want the rail LEDs to update
                    // Rails don't have breadboard positions so no text conflict
                    inPadMenu = 1;

                    // Highlight the rail LEDs
                    Highlighting::getInstance( ).brightenedRail = railBrightened[ subIndex ];

                    // Show only the currently selected rail in the center
                    b.print( railNames[ subIndex ], scaleBrightness( railColors[ subIndex ], 0 ), 0xFFFFFF, 0, -1, -2 );
                } else if ( cursorZone == ZONE_DAC ) {
                    // Display DAC 0 and 1
                    const int dacNodes[ 2 ] = { DAC0, DAC1 };
                    actualNode = dacNodes[ subIndex ];
                    snprintf( displayName, sizeof( displayName ), "DAC %d", subIndex );

                    // Prevent net LEDs from overwriting our text display
                    inPadMenu = 1;

                    clearLEDsExceptRails( );
                    b.print( "DAC", scaleDownBrightness( rawOtherColors[ 9 ], 4, 22 ), 0xFFFFFF, 1, 0, 3 );

                    // Set DAC colorOverrides using helper functions
                    if ( subIndex == 0 ) {
                        setLogoOverride( DAC_0, -2 );

                        b.print( "0", cursorColors[ 1 ][ setOrClear ][ 0], 0xFFFFFF, 0, 1, 3 );
                        b.print( "1", cursorColors[ 0 ][ setOrClear ][ 4 ], 0xFFFFFF, 5, 1, 0 );
                    } else {

                        setLogoOverride( DAC_1, -2 );
                        b.print( "0", cursorColors[ 0 ][ setOrClear ][ 0 ], 0xFFFFFF, 0, 1, 3 );
                        b.print( "1", cursorColors[ 1 ][ setOrClear ][ 4 ], 0xFFFFFF, 5, 1, 0 );
                    }
                } else if ( cursorZone == ZONE_ADC ) {
                    // Display ADC 0-4, 7 (ADC 5,6 don't exist)
                    const int adcMap[ 6 ] = { ADC0, ADC1, ADC2, ADC3, ADC4, ADC7 };
                    const char* adcLabels[ 6 ] = { "0", "1", "2", "3", "4", "P" }; // P for probe
                    actualNode = adcMap[ subIndex ];
                    snprintf( displayName, sizeof( displayName ), "ADC %s", adcLabels[ subIndex ] );

                    // Prevent net LEDs from overwriting our text display
                    inPadMenu = 1;

                    clearLEDsExceptRails( );
                    b.print( " ADC", scaleDownBrightness( rawOtherColors[ 8 ], 4, 22 ), 0xFFFFFF, 0, 0, 3 );

                    for ( int i = 0; i < 6; i++ ) {
                        uint32_t color = ( i == subIndex ) ? cursorColors[ 0 ][ setOrClear ][ i ] : cursorColors[ 1 ][ setOrClear ][ i ];
                        b.print( adcLabels[ i ], color, 0xFFFFFF, i, 1, ( i == 0 ? -1 : i - 1 ) );
                    }

                    if ( subIndex % 2 == 0 ) {
                        setLogoOverride( ADC_0, -2 );

                    } else {

                        setLogoOverride( ADC_1, -2 );
                    }

                } else if ( cursorZone == ZONE_GPIO ) {
                    // Display GPIO 1-8
                    actualNode = RP_GPIO_1 + subIndex;
                    snprintf( displayName, sizeof( displayName ), "GPIO %d", subIndex + 1 );

                    // Prevent net LEDs from overwriting our text display
                    inPadMenu = 1;

                    clearLEDsExceptRails( );
                    uint32_t inColor = ( connectOrClearProbe == 0 ) ? 0x000000 : 0x000606;
                    uint32_t outColor = ( connectOrClearProbe == 0 ) ? 0x000000 : 0x060100;

                    // Display GPIO 1-4 on top row using original positioning
                    const int positions[ 4 ] = { 0, 2, 4, 6 };
                    const int nudges[ 4 ] = { 1, 0, -1, -2 };

                    for ( int i = 0; i < 4; i++ ) {
                        uint32_t numColor = ( i == subIndex ) ? cursorColors[ 0 ][ setOrClear ][ i ] : cursorColors[ 1 ][ setOrClear ][ i ];
                        char numStr[ 2 ] = { (char)( '1' + i ), '\0' };
                        b.print( numStr, numColor, 0xFFFFFF, positions[ i ], 0, nudges[ i ] );

                        // Show input/output indicators for top row
                        int rowBase = 2 + i * 7;
                        b.printRawRow( 0b00011000, rowBase, ( i == subIndex ) ? inColor : 0x000000, 0xFFFFFF );
                        b.printRawRow( 0b00011000, rowBase + 4, ( i == subIndex ) ? outColor : 0x000000, 0xFFFFFF );
                    }

                    // Display GPIO 5-8 on bottom row using original positioning
                    for ( int i = 4; i < 8; i++ ) {
                        uint32_t numColor = ( i == subIndex ) ? cursorColors[ 0 ][ setOrClear ][ i ] : cursorColors[ 1 ][ setOrClear ][ i ];
                        char numStr[ 2 ] = { (char)( '1' + i ), '\0' };
                        b.print( numStr, numColor, 0xFFFFFF, positions[ i - 4 ], 1, nudges[ i - 4 ] );

                        // Show input/output indicators for bottom row
                        int rowBase = 32 + ( i - 4 ) * 7;
                        b.printRawRow( 0b00000011, rowBase, ( i == subIndex ) ? inColor : 0x000000, 0xFFFFFF );
                        b.printRawRow( 0b00000011, rowBase + 4, ( i == subIndex ) ? outColor : 0x000000, 0xFFFFFF );
                    }

                    // Clear all logo overrides first, then set only GPIO
                    clearColorOverrides( true, true, false );

                    // Set GPIO colorOverrides using helper functions
                    if ( subIndex % 2 == 0 ) {
                        setLogoOverride( GPIO_0, -2 );

                    } else {

                        setLogoOverride( GPIO_1, -2 ); //-2 sets  the override to the default highlighed color
                    }
                } else if ( cursorZone == ZONE_UART ) {
                    // Display UART TX and RX - one at a time to prevent overlap
                    const char* uartNames[ 2 ] = { "TX", "RX" };
                    int uartNodes[ 2 ] = { RP_UART_TX, RP_UART_RX };
                    actualNode = uartNodes[ subIndex ];
                    snprintf( displayName, sizeof( displayName ), "UART %s", uartNames[ subIndex ] );

                    // Prevent net LEDs from overwriting our text display
                    inPadMenu = 1;

                    // Show UART label and only the selected TX or RX
                    b.print( " UART", sfOptionColors[ 3 ], 0xFFFFFF, 0, 0, 2 );
                    b.print( uartNames[ 0 ], subIndex == 0 ? cursorColors[ 0 ][ setOrClear ][ 0 ] : cursorColors[ 1 ][ setOrClear ][ 0 ], 0xFFFFFF, 1, 1, -2 );
                    b.print( uartNames[ 1 ], subIndex == 1 ? cursorColors[ 0 ][ setOrClear ][ 1 ] : cursorColors[ 1 ][ setOrClear ][ 1 ], 0xFFFFFF, 4, 1, 2 );

                    // Clear all logo overrides first, then set only UART
                    // clearColorOverrides(true, true, false);

                    // Set logo colorOverrides for UART using helper functions
                    if ( subIndex == 0 ) {
                        setLogoOverride( LOGO_TOP, -2 );

                    } else {

                        setLogoOverride( LOGO_BOTTOM, -2 );
                    }
                } else if ( cursorZone == ZONE_CURRENT ) {
                    const char* currentNames[ 2 ] = { "I+", "I-" };
                    int currentNodes[ 2 ] = { ISENSE_PLUS, ISENSE_MINUS };
                    actualNode = currentNodes[ subIndex ];
                    snprintf( displayName, sizeof( displayName ), "Current %s", subIndex == 0 ? "+" : "-" );

                    // Color definitions for I+ (red) and I- (green)
                    const uint32_t plusBrightColor = 0x2A0002;  // Bright red for selected I+
                    const uint32_t plusDimColor = 0x0a0000;     // Dim red for unselected I+
                    const uint32_t minusBrightColor = 0x002A02; // Bright green for selected I-
                    const uint32_t minusDimColor = 0x000A00;    // Dim green for unselected I-

                    inPadMenu = 1;
                    clearLEDsExceptRails( );

                    b.print( "Current", sfOptionColors[ 6 ], 0xFFFFFF, 0, 0, 1 );
                    b.print( currentNames[ 0 ], subIndex == 0 ? plusBrightColor : plusDimColor, 0xFFFFFF, 1, 1, -2 );
                    b.print( currentNames[ 1 ], subIndex == 1 ? minusBrightColor : minusDimColor, 0xFFFFFF, 4, 1, 2 );

                    clearColorOverrides( true, true, false );
                }

                // 5. Try highlighting nets if we're on a regular node
                int netOnNode = 0;
                if ( actualNode > 0 && ( cursorZone == ZONE_BREADBOARD || cursorZone == ZONE_NANO ) ) {
                    netOnNode = Highlighting::getInstance( ).highlightNets( actualNode, 0, 1 );
                }

                // 6. Display name on Serial and OLED
                if ( netOnNode <= 0 || cursorZone >= ZONE_RAILS ) {
                    if ( brightenedNode > 0 ) {
                        Highlighting::getInstance( ).clearHighlighting( );
                    }

                    Serial.print( "\r                                               \r" );
                    Serial.print( ">>>> " );
                    Serial.print( displayName );
                    Serial.flush( );

                    oled.clearPrintShow( displayName, 2, true, true );
                }

                // 7. Save persistent cursor position
                persistentEncoderCursorNode = encoderCursorNode;
                persistentCursorZone = cursorZone;
                persistentSubIndex = subIndex;

                // 8. NOW update LEDs atomically - unpause and trigger update
                pauseCore2 = 0;    // Unpause Core 2
                showLEDsCore2 = 2; // Trigger single atomic update

                // ========== END ATOMIC UPDATE ==========

                // If we have first node and selecting second, show preview
                if ( node1or2 == 1 && nodesToConnect[ 0 ] > 0 && setOrClear == 1 && cursorZone <= ZONE_NANO ) {
                    int previewNode = ( cursorZone == ZONE_NANO ) ? encoderCursorNode : ( encoderCursorNode + 1 );

                    // Visual preview: highlight both nodes without modifying state
                    if ( previewNode > 0 && previewNode != nodesToConnect[ 0 ] &&
                         previewNode >= 1 && previewNode <= 60 ) {
                        // Show first node
                        b.printRawRow( 0b00000100, nodesToConnect[ 0 ] - 1, 0x121215, 0x4500e8 );
                        // Show second node being previewed
                        b.printRawRow( 0b00000100, previewNode - 1, 0x121215, 0x00e845 );
                    }
                }

                showLEDsCore2 = 2;
            }

            lastEncoderPosition = currentEncoderPosition;
        }

        // Check for encoder cursor timeout (auto-hide after 5 seconds)
        if ( encoderCursorVisible && ( millis( ) - lastEncoderMovement ) > encoderHideTimeout ) {
            // Clear the cursor position before hiding
            if ( cursorZone == ZONE_BREADBOARD && encoderCursorNode >= 0 ) {
                b.printRawRow( 0b00000100, encoderCursorNode, 0x000000, 0x000000 );
            } else if ( cursorZone == ZONE_NANO && encoderCursorNode >= 0 ) {
                int pixel = getNanoHeaderPixel( encoderCursorNode );
                if ( pixel >= 0 )
                    leds.setPixelColor( pixel, 0x000000 );
            } else if ( cursorZone >= ZONE_RAILS ) {
                // Clear special function zone display
                clearLEDsExceptRails( );
                // Clear inPadMenu flag
                inPadMenu = 0;
            }

            // Clear net highlighting and all color overrides
            Highlighting::getInstance( ).clearHighlighting( 0 );
            clearColorOverrides( true, true, false );

            encoderCursorVisible = false;
            lastEncoderCursorNode = -1;
            lastCursorZone = -1;
            globalEncoderCursorNode = -1; // Clear global
            globalEncoderCursorInHeader = 0;
            showLEDsCore2 = 2;
        }

        // Check for encoder button press to select node
        // Only process encoder button if cursor is visible (otherwise it might interfere with normal operation)
        if ( encoderCursorVisible && ( ( encoderButtonState == PRESSED && lastButtonEncoderState == IDLE ) || ( ProbeButton::getInstance( ).getButtonState( ) == ( setOrClear == 1 ? 2 : 1 ) ) && ( millis( ) - probeModeStartTime > 500 ) ) ) {
            // IMMEDIATELY reset button state to prevent it from triggering click menu

            // Serial.println(encoderButtonState);
            // Serial.println(lastButtonEncoderState);
            // Serial.println(ProbeButton::getInstance().getButtonState());
            // Serial.println(encoderCursorVisible);
            // Serial.println(blockProbeButton);
            // Serial.println(blockProbeButtonTimer);
            // Serial.println(millis());
            // Serial.flush();

            encoderButtonState = IDLE;
            lastButtonEncoderState = IDLE;
            blockProbeButton = 1000;
            blockProbeButtonTimer = millis( );
            ProbeButton::getInstance( ).clearButtonState( );

            // Get the actual node number based on current zone
            int selectedNode = -1;

            if ( cursorZone == ZONE_BREADBOARD ) {
                selectedNode = encoderCursorNode + 1;
            } else if ( cursorZone == ZONE_NANO ) {
                selectedNode = encoderCursorNode;
            } else if ( cursorZone == ZONE_RAILS ) {
                const int railNodes[ 3 ] = { TOP_RAIL, BOTTOM_RAIL, GND };
                selectedNode = railNodes[ subIndex ];
            } else if ( cursorZone == ZONE_DAC ) {
                const int dacNodes[ 2 ] = { DAC0, DAC1 };
                selectedNode = dacNodes[ subIndex ];

                // For DAC, launch voltage adjuster
                VoltageAdjustConfig config;
                config.minVoltage = -8.0;
                config.maxVoltage = 8.0;
                config.enableSnap = false;
                config.liveUpdateInRange = true;
                config.liveUpdateMin = 0.0;
                config.liveUpdateMax = 5.0;

                if ( subIndex == 0 ) {
                    // DAC 0
                    config.initialValue = globalState.power.dac0;
                    config.label = "DAC 0";
                    config.callback = []( float newValue, bool isLive, void* context ) {
                        setDac0voltage( newValue, 1, 0, false );
                        globalState.power.dac0 = newValue;
                    };
                } else {
                    // DAC 1
                    config.initialValue = globalState.power.dac1;
                    config.label = "DAC 1";
                    config.callback = []( float newValue, bool isLive, void* context ) {
                        setDac1voltage( newValue, 1, 0, false );
                        globalState.power.dac1 = newValue;
                    };
                }

                AdjustResult result = VoltageAdjuster::adjust( config );
                if ( result == AdjustResult::CONFIRMED ) {
                    saveVoltages( globalState.power.topRail, globalState.power.bottomRail,
                                  globalState.power.dac0, globalState.power.dac1 );
                }

                // Clear and continue without selecting node
                encoderCursorVisible = false;
                lastEncoderCursorNode = -1;
                globalEncoderCursorNode = -1;
                setLogoOverride( DAC_0, -2 );
                setLogoOverride( DAC_1, -2 );
                clearLEDsExceptRails( );
                showLEDsCore2 = -1;
                continue;
            } else if ( cursorZone == ZONE_ADC ) {
                const int adcMap[ 6 ] = { ADC0, ADC1, ADC2, ADC3, ADC4, ADC7 };
                selectedNode = adcMap[ subIndex ];
            } else if ( cursorZone == ZONE_GPIO ) {
                selectedNode = RP_GPIO_1 + subIndex;

                // For GPIO, prompt for input/output selection if connecting
                if ( connectOrClearProbe == 1 ) {
                    int gpioIndex = subIndex + 1; // GPIO 1-8

                    // Clear the special function display first
                    clearLEDsExceptRails( );
                    b.clear( );

                    // Show input/output selection menu
                    int ioSelection = chooseGPIOinputOutput( gpioIndex );

                    // If user cancelled, don't select the node
                    if ( ioSelection == -1 ) {
                        // User cancelled - clear and don't select node
                        selectedNode = -1;
                    }
                }
            } else if ( cursorZone == ZONE_UART ) {
                const int uartNodes[ 2 ] = { RP_UART_TX, RP_UART_RX };
                selectedNode = uartNodes[ subIndex ];
            } else if ( cursorZone == ZONE_CURRENT ) {
                const int currentNodes[ 2 ] = { ISENSE_PLUS, ISENSE_MINUS };
                selectedNode = currentNodes[ subIndex ];
            }

            // Treat encoder selection like a probe touch
            if ( selectedNode > 0 ) {
                row[ 0 ] = selectedNode;
                connectedRows[ 0 ] = selectedNode;
                connectedRowsIndex = 1;
            }

            // Clear net highlighting and color overrides
            Highlighting::getInstance( ).clearHighlighting( 0 );

            // Clear all colorOverrides using helper function
            clearColorOverrides( true, true, false );

            // Clear inPadMenu flag
            inPadMenu = 0;

            // Reset encoder cursor for next selection
            encoderCursorVisible = false;
            lastEncoderCursorNode = -1;
            lastCursorZone = -1;
            globalEncoderCursorNode = -1; // Clear global (hides cursor)
            globalEncoderCursorInHeader = 0;
            lastEncoderMovement = millis( ); // Reset timeout

            // If we selected from special function zone, reset to row 15 breadboard for next selection
            if ( cursorZone >= ZONE_RAILS ) {
                persistentEncoderCursorNode = 14; // Row 15 (0-indexed)
                persistentCursorZone = ZONE_BREADBOARD;
                persistentSubIndex = 0;
                // Also reset the local working variables immediately
                encoderCursorNode = 14;
                cursorZone = ZONE_BREADBOARD;
                subIndex = 0;
            } else {
                // Normal breadboard/nano selection - persist position
                persistentEncoderCursorNode = encoderCursorNode;
                persistentCursorZone = cursorZone;
                persistentSubIndex = subIndex;
            }

            // Clear breadboard display and continue to normal probe processing
            clearLEDsExceptRails( );
            showLEDsCore2 = -1;

            // Continue to normal probe processing below
        } else {
            row[ 0 ] = -1; // No encoder selection this iteration
        }

        // Check for encoder button HELD to exit probe mode
        if ( encoderButtonState == HELD ) {
            // User held encoder button - exit probe mode
            encoderButtonState = IDLE;
            lastButtonEncoderState = IDLE;

            // Clear cursor LED before exiting based on zone
            if ( cursorZone == ZONE_BREADBOARD && encoderCursorNode >= 0 ) {
                b.printRawRow( 0b00000100, encoderCursorNode, 0x000000, 0x000000 );
            } else if ( cursorZone == ZONE_NANO && encoderCursorNode >= 0 ) {
                int pixel = getNanoHeaderPixel( encoderCursorNode );
                if ( pixel >= 0 )
                    leds.setPixelColor( pixel, 0x000000 );
            } else if ( cursorZone >= ZONE_RAILS ) {
                clearLEDsExceptRails( );
            }

            globalEncoderCursorNode = -1; // Clear globals on exit
            globalEncoderCursorInHeader = 0;

            // Clear all highlighting and color overrides
            Highlighting::getInstance( ).clearHighlighting( 0 );
            clearColorOverrides( true, true, false );

            // Clear inPadMenu flag
            inPadMenu = 0;

            showLEDsCore2 = 2; // Update LEDs to show cleared state
            break;
        }

        // CRITICAL: Always clear encoder button state when cursor not visible
        // This prevents the encoder button from triggering the click menu
        if ( !encoderCursorVisible ) {
            if ( encoderButtonState == PRESSED || encoderButtonState == RELEASED ) {
                encoderButtonState = IDLE;
            }
            if ( lastButtonEncoderState == PRESSED || lastButtonEncoderState == RELEASED ) {
                lastButtonEncoderState = IDLE;
            }
        }

        // ======= END ENCODER SELECTION =======

        if ( firstConnection > 0 ) {
            row[ 0 ] = firstConnection;
            connectedRows[ 0 ] = row[ 0 ];
            connectedRowsIndex = 1;
            if ( setOrClear == 0 ) {
                firstConnection = -2;
            } else {
                firstConnection = -3;
            }

        } else if ( row[ 0 ] == -1 ) { // Only read physical probe if encoder didn't select
            row[ 0 ] = readProbe( );
        }

        // Handle encoder returns from readProbe() - these are handled by cursor logic above
        if ( row[ 0 ] == -19 || row[ 0 ] == -17 || row[ 0 ] == -10 ) {
            // Encoder movement/button - already handled by cursor logic
            // Reset row[0] and continue loop
            row[ 0 ] = -1;
            continue;
        }

        if ( row[ 0 ] == -1 ) {
            // tuiGlue.loop();
        }
        // Serial.println(row[0]);

        // probeButtonToggle = checkProbeButton();
        // if (isConnectable(row[0]) == false && row[0] != -1) {
        //   row[0] = -1;
        // //  continue;
        // }

        if ( setOrClear == 1 ) { //! remove fade animation
            deleteMissesIndex = 0;
            if ( millis( ) - fadeTimer > 500 ) {
                fadeTimer = millis( );
                if ( numberOfLocalChanges > 0 ) {

                    // saveStateToSlot( netSlot );
                    // saveLocalNodeFile( netSlot );
                    // Serial.print("\n\r");
                    // Serial.print("saving local node file\n\r");

                    // refreshConnections();
                    // numberOfLocalChanges = 0;
                }
            }
            // showLEDsCore2 = -1;

        } else {
            if ( millis( ) - fadeTimer > 10 ) {
                fadeTimer = millis( );

                if ( fadeIndex < 12 ) {
                    fadeIndex++;
                    if ( removeFade > 0 ) {
                        removeFade--;
                        showProbeLEDs = 2;
                    }
                } else {
                    deleteMissesIndex = 0;
                    for ( int i = 0; i < 20; i++ ) {
                        deleteMisses[ i ] = -1;
                    }
                }

                int fadeFloor = fadeIndex;
                if ( fadeFloor < 0 ) {
                    fadeFloor = 0;
                }

                for ( int i = deleteMissesIndex - 1; i >= 0; i-- ) {
                    int fadeOffset = map( i, 0, deleteMissesIndex, 0, 12 ) + fadeFloor;
                    if ( fadeOffset > 12 ) {
                        fadeOffset = 12;
                        // showLEDsCore2 = -1;
                    }
                    // clearLEDsExceptMiddle(deleteMisses[i], -1);

                    //  Serial.println(fadeOffset);
                    //  Serial.flush();
                    // b.printRawRow(0b00001010, deleteMisses[i] - 1, deleteFadeSides[fadeOffset], 0xfffffe);
                    b.printRawRow( 0b00000100, deleteMisses[ i ] - 1, deleteFade[ fadeOffset ],
                                   0xfffffe );
                    showLEDsCore2 = 2;
                }

                if ( deleteMissesIndex == 0 && fadeClear == 0 ) {
                    fadeClear = 1;
                    showLEDsCore2 = -1;
                    if ( numberOfLocalChanges > 0 ) {
                        // saveLocalNodeFile( netSlot );
                        // Serial.print("\n\r");
                        // Serial.print("saving local node file\n\r");
                        // saveStateToSlot( netSlot );
                        // refreshConnections();
                        // numberOfLocalChanges = 0;
                    }
                }
            }
        }

        if ( ( row[ 0 ] == -18 || row[ 0 ] == -16 ) ) { // ! Button press detected
            // (millis() - probingTimer > 500)) { //&&

            // Serial.println("row[0] = " + String(row[0]));
            // Serial.println("setOrClear = " + String(setOrClear));
            // Serial.println("node1or2 = " + String(node1or2));
            // Serial.println("connectionsThisSession = " + String(connectionsThisSession));
            // Serial.println("probingTimer = " + String(probingTimer));
            // Serial.println("probeButtonTimer = " + String(probeButtonTimer));
            // Serial.println("probeHighlight = " + String(probeHighlight));
            // Serial.println("showLEDsCore2 = " + String(showLEDsCore2));
            // Serial.println("connectedRowsIndex = " + String(connectedRowsIndex));
            // Serial.println("--------------------------------\n\r1\n\r2\n\r3\n\r4\n\r5\n\r");
            // Serial.flush( );

            if ( row[ 0 ] == -18 ) { // clear button
                // Serial.println("-18 clear button\n\r");

                if ( setOrClear == 0 ) { // already in clear mode
                    // Serial.println("-18 setOrClear == 0\n\r");
                    nodesToConnect[ 0 ] = -1;
                    nodesToConnect[ 1 ] = -1;
                    node1or2 = 0;
                    // clearLEDsExceptRails();
                    probeHighlight = -1;
                    showLEDsCore2 = -1;
                    // connectionsThisSession = 0;
                    Serial.print( "\x1b[2K\r" ); // Clear the line and return cursor to start

                    Serial.flush( );
                    // Serial.println("setOrClear == 0");
                } else { // switch to clear mode

                    // Serial.println("-18 setOrClear == 1\n\r");
                    setOrClear = 0;
                    probingTimer = millis( );
                    probeButtonTimer = millis( );

                    sfProbeMenu = 0;
                    connectedRowsIndex = 0;
                    connectedRows[ 0 ] = -1;
                    connectedRows[ 1 ] = -1;
                    nodesToConnect[ 0 ] = -1;
                    nodesToConnect[ 1 ] = -1;
                    //           lastProbedRows[0] = -1;
                    // lastProbedRows[1] = -1;
                    // clearLEDsExceptRails();
                    showLEDsCore2 = 1;
                    node1or2 = 0;

                    // Serial.println("-18 connectionsThisSession = " + String(connectionsThisSession) + "\n\n\n\n\n\r");
                    if ( connectionsThisSession == 0 ) {
                        Serial.print( "\x1b[3A\x1b[0J" ); // Clear the line and return cursor to start
                        Serial.flush( );
                        connectionsThisSession = 0;
                    }

                    // showProbeLEDs = 1;
                    goto restartProbing;
                }
                // break;
            } else if ( row[ 0 ] == -16 ) { // connect button

                if ( setOrClear == 1 ) { // already in connect mode
                    // showProbeLEDs = 2;
                    //  delay(100);
                    if ( node1or2 == 1 ) {
                        connectedRowsIndex = 0;
                        nodesToConnect[ 0 ] = -1;
                        nodesToConnect[ 1 ] = -1;
                        node1or2 = 0;
                        // Serial.println(probeHighlight);

                        probeHighlight = -1;
                        // clearLEDsExceptRails();
                        showLEDsCore2 = -2;
                        // waitCore2();

                        // Serial.println("-16 setOrClear == 1\n\r");
                        // if ( connectionsThisSession == 0 ) {
                        Serial.print( "\x1b[2K\r" ); // Clear the line and return cursor to start

                        Serial.flush( );
                        // connectionsThisSession = 0;
                        // }
                        goto restartProbingNoPrint;

                    } else {

                        // Serial.println("-16 setOrClear == 0 && node1or2 == 0\n\r");
                        //  Serial.println("setOrClear == 1 && node1or2 ==
                        //  0");
                    }
                } else { // switch to connect mode
                    // Serial.println("-16 setOrClear == 0\n\r");
                    setOrClear = 1;
                    // showProbeLEDs = 2;

                    probingTimer = millis( );
                    probeButtonTimer = millis( );
                    // showNets();
                    // showLEDsCore2 = 1;
                    sfProbeMenu = 0;
                    connectedRowsIndex = 0;
                    connectedRows[ 0 ] = -1;
                    connectedRows[ 1 ] = -1;
                    nodesToConnect[ 0 ] = -1;
                    nodesToConnect[ 1 ] = -1;
                    node1or2 = 0;
                    // clearLEDsExceptRails();
                    //  lastProbedRows[0] = -1;
                    //  lastProbedRows[1] = -1;

                    for ( int i = deleteMissesIndex - 1; i >= 0; i-- ) {

                        b.printRawRow( 0b00000100, deleteMisses[ i ] - 1, 0, 0xfffffe );
                        //   Serial.print(i);
                        //   Serial.print("   ");
                        //   Serial.print(deleteMisses[i]);
                        //   Serial.print("    ");
                        //  Serial.println(map(i, 0,deleteMissesIndex, 0, 19));
                    }
                    // showLEDsCore2 = 1;
                    if ( connectionsThisSession == 0 ) {
                        Serial.print( "\x1b[3A\x1b[0J" ); // Clear the line and return cursor to start
                        Serial.flush( );

                    } else {
                        Serial.print( "\x1b[2K\r" ); // Clear the line and return cursor to start
                        Serial.flush( );
                    }
                    connectionsThisSession = 0;

                    // Serial.println("setOrClear == 1");

                    goto restartProbing;
                }
                // break;
            }

            // Serial.print("\n\rCommitting paths!\n\r");
            row[ 1 ] = -2;
            probingTimer = millis( );

            connectedRowsIndex = 0;

            node1or2 = 0;
            nodesToConnect[ 0 ] = -1;
            nodesToConnect[ 1 ] = -1;
            probeHighlight = -1;
            showLEDsCore2 = -1;
            break;
        } else {
            // probingTimer = millis();
        }

        if ( isConnectable( row[ 0 ] ) == false && row[ 0 ] != -1 ) {
            row[ 0 ] = -1;
        }

        if ( row[ 0 ] != -1 && row[ 0 ] != row[ 1 ] ) { // && row[0] != lastProbedRows[0] &&
            // row[0] != lastProbedRows[1]) {

            lastProbedRows[ 1 ] = lastProbedRows[ 0 ];
            lastProbedRows[ 0 ] = row[ 0 ];
            if ( connectedRowsIndex == 1 ) {
                nodesToConnect[ node1or2 ] = connectedRows[ 0 ];

                // oled.clear();
                // oled.print(definesToChar(nodesToConnect[0]));
                // oled.print(" - ");
                // oled.show();
                char node1Name[ 12 ];
                strcpy( node1Name, definesToChar( nodesToConnect[ 0 ] ) );
                char node2Name[ 12 ];
                strcpy( node2Name, "   " );

                char bothNames[ 25 ];
                strcpy( bothNames, node1Name );
                strcat( bothNames, " - " );
                strcat( bothNames, node2Name );

                Serial.print( "\r                   \r" );

                int numChars = strlen( node1Name );
                for ( int i = 0; i < SPACE_FROM_LEFT - numChars; i++ ) {
                    Serial.print( " " );
                }
                Serial.print( node1Name );
                if ( setOrClear == 1 ) {
                    Serial.print( "  -  " );
                }
                // numChars = Serial.print(node2Name);
                Serial.flush( );

                probeHighlight = nodesToConnect[ node1or2 ];
                if ( setOrClear == 1 ) {
                    // probeConnectHighlight = nodesToConnect[node1or2];
                    brightenNet( probeHighlight, 5 );
                    oled.clearPrintShow( bothNames, 2, true, true, true );
                }

                // oled.clearPrintShow(bothNames, 2, 0, 5, true, true, true);

                // Serial.print("probing Highlight: ");
                // Serial.println(probeHighlight);
                // showProbeLEDs = 1;

                if ( nodesToConnect[ node1or2 ] > 0 &&
                     nodesToConnect[ node1or2 ] <= NANO_RESET_1 && setOrClear == 1 ) {

                    // probeConnectHighlight = nodesToConnect[node1or2];
                    //  Serial.print("probeConnectHighlight = ");
                    //  Serial.println(probeConnectHighlight);
                    // showLEDsCore2 = 2;

                    // // b.clear();
                    b.printRawRow( 0b0010001, nodesToConnect[ node1or2 ] - 1, 0x000121e,
                                   0xfffffe );
                    showLEDsCore2 = 2;
                    delay( 30 );
                    b.printRawRow( 0b00001010, nodesToConnect[ node1or2 ] - 1, 0x0f0498,
                                   0xfffffe );
                    showLEDsCore2 = 2;
                    delay( 30 );

                    b.printRawRow( 0b00000100, nodesToConnect[ node1or2 ] - 1, 0x4000e8,
                                   0xfffffe );
                    showLEDsCore2 = 2;
                    delay( 50 );
                    showLEDsCore2 = 2;
                }

                node1or2++;
                probingTimer = millis( );
                showLEDsCore2 = 1;
                doubleSelectTimeout = millis( );
                doubleSelectCountdown = 200;
                // delay(500);

                // delay(3);
            }

            if ( node1or2 >= 2 || ( setOrClear == 0 && node1or2 >= 1 ) ) {

                probeHighlight = -1;
                // Serial.print("fuck");

                if ( setOrClear == 1 && ( nodesToConnect[ 0 ] != nodesToConnect[ 1 ] ) &&
                     nodesToConnect[ 0 ] > 0 && nodesToConnect[ 1 ] > 0 ) {
                    b.printRawRow( 0b00011111, nodesToConnect[ 0 ] - 1, 0x0, 0x00000000 );
                    b.printRawRow( 0b00011111, nodesToConnect[ 1 ] - 1, 0x0, 0x00000000 );
                    Serial.print( "\r              \r" );

                    // Serial.println("fuck");
                    Serial.flush( );
                    char node1Name[ 12 ];

                    strcpy( node1Name, definesToChar( nodesToConnect[ 0 ] ) );

                    char node2Name[ 12 ];

                    strcpy( node2Name, definesToChar( nodesToConnect[ 1 ] ) );

                    char bothNames[ 25 ];
                    strcpy( bothNames, node1Name );
                    strcat( bothNames, " - " );
                    strcat( bothNames, node2Name );
                    if ( connectionAllowed( nodesToConnect[ 0 ], nodesToConnect[ 1 ] ) == false ) {
                        Serial.print( " can't connect " );
                        Serial.print( node1Name );
                        Serial.print( " to " );
                        Serial.println( node2Name );
                        Serial.flush( );
                        node1or2 = 0;
                        nodesToConnect[ 0 ] = -1;
                        nodesToConnect[ 1 ] = -1;
                        continue;
                    }

                    int numChars = strlen( node1Name );
                    for ( int i = 0; i < SPACE_FROM_LEFT - numChars; i++ ) {
                        Serial.print( " " );
                    }
                    Serial.print( node1Name );
                    Serial.print( "  -  " );
                    numChars = Serial.print( node2Name );
                    // for (int i = 0; i < 12 - numChars; i++) {
                    //   Serial.print(" ");
                    //   }

                    // Check if the second node is a GPIO and print its direction
                    if ( nodesToConnect[ 1 ] >= RP_GPIO_1 && nodesToConnect[ 1 ] <= RP_GPIO_8 ) {
                        // Find the GPIO index in gpioDef array
                        int gpioIndex = -1;
                        for ( int i = 0; i < 10; i++ ) {
                            if ( gpioDef[ i ][ 1 ] == nodesToConnect[ 1 ] ) {
                                gpioIndex = i;
                                break;
                            }
                        }

                        if ( gpioIndex != -1 ) {
                            if ( globalState.config.gpioDirection[ gpioIndex ] == 0 ) {
                                Serial.print( " (output)  connected" );
                            } else {
                                Serial.print( " (input)  connected" );
                            }
                        }
                        Serial.flush( );
                    } else {

                        Serial.print( "   \tconnected\n\r" );
                        Serial.flush( );
                    }

                    if ( firstConnection == -3 ) {
                        // Add to RAM state - DON'T save yet, let auto-save handle it
                        addBridgeToState( nodesToConnect[ 0 ], nodesToConnect[ 1 ] );
                        numberOfLocalChanges++;
                        // refreshConnections(1, 1, 0);
                        // showLEDsCore2 = -1;
                        connectionsThisSession++;
                        break;

                    } else {

                        // Add to RAM state (local changes accumulated in RAM)
                        addBridgeToState( nodesToConnect[ 0 ], nodesToConnect[ 1 ] );
                        numberOfLocalChanges++;
                        connectionsThisSession++;
                    }
                    brightenNet( -1 );

                    oled.clearPrintShow( bothNames, 2, true, true, true );
                    // oled.clear();
                    // oled.print(node1Name);
                    // oled.print(" - ");
                    // oled.print(node2Name);
                    // oled.print(" connected");
                    // oled.show();
                    // Serial.println(numberOfLocalChanges);

                    // NOTE: refreshLocalConnections() is already called inside addBridgeToState()
                    // No need to call it again here - that was causing double refresh delay!
                    fadeTimer = millis( );
                    // if (numberOfLocalChanges > 5) {
                    //   saveLocalNodeFile(netSlot);
                    //   // refreshConnections();
                    //   numberOfLocalChanges = 0;

                    // } // else {
                    //  saveLocalNodeFile(netSlot);

                    //}

                    row[ 1 ] = -1;

                    // doubleSelectTimeout = millis();
                    for ( int i = 0; i < 12; i++ ) {
                        deleteMisses[ i ] = -1;
                    }

                    doubleSelectTimeout = millis( );
                    doubleSelectCountdown = 200;

                } else if ( setOrClear == 0 ) {

                    char node1Name[ 12 ];

                    char node2Name[ 12 ];
                    strcpy( node2Name, "   " );
                    char bothNames[ 25 ];
                    strcpy( bothNames, node1Name );
                    strcat( bothNames, " - " );
                    strcat( bothNames, node2Name );

                    // int numChars = strlen(node1Name);
                    // for (int i = 0; i < SPACE_FROM_LEFT - numChars; i++) {
                    //   Serial.print(" ");
                    //   }
                    // Serial.print(node1Name);
                    // Serial.print(" ");
                    // numChars = Serial.print(node2Name);

                    for ( int i = 12; i > 0; i-- ) {
                        deleteMisses[ i ] = deleteMisses[ i - 1 ];
                        // Serial.print(i);
                        // Serial.print("   ");
                        // Serial.println(deleteMisses[i]);
                    }
                    // Serial.print("\n\r");
                    deleteMisses[ 0 ] = nodesToConnect[ 0 ];

                    // deleteMisses[deleteMissesIndex] = nodesToConnect[0];
                    if ( deleteMissesIndex < 12 ) {
                        deleteMissesIndex++;
                    }
                    fadeIndex = -3;

                    //  Serial.println("\n\r");
                    //  Serial.print("deleteMissesIndex: ");
                    //   Serial.print(deleteMissesIndex);
                    //   Serial.print("\n\r");
                    for ( int i = deleteMissesIndex - 1; i >= 0; i-- ) {

                        b.printRawRow( 0b00000100, deleteMisses[ i ] - 1,
                                       deleteFade[ map( i, 0, deleteMissesIndex, 0, 12 ) ],
                                       0xfffffe );
                        //   Serial.print(i);
                        //   Serial.print("   ");
                        //   Serial.print(deleteMisses[i]);
                        //   Serial.print("    ");
                        //  Serial.println(map(i, 0,deleteMissesIndex, 0, 19));
                    }
                    clearHighlighting( 0 );
                    //  Serial.println();
                    // Remove from RAM state - let auto-save handle persistence
                    // This removes ALL connections containing nodesToConnect[0]
                    bool removed = removeBridgeFromState( nodesToConnect[ 0 ], -1 );

                    // The number of removed connections is tracked in lastRemovedNodesIndex
                    int rowsRemoved = removed ? lastRemovedNodesIndex : 0;
                    if ( removed ) {
                        numberOfLocalChanges += rowsRemoved;
                    }
                    // if ( rowsRemoved > 0 ) {

                    // Serial.print("connectionsThisSession: ");
                    // Serial.print(connectionsThisSession);
                    // Serial.flush( );
                    // }

                    // waitCore2();
                    if ( rowsRemoved > 0 ) {
                        connectionsThisSession++;
                        // connectionsThisSession++;
                        removeFade = 10;

                        // goto restartProbing;

                        // Print the disconnected nodes using our helper function
                        Serial.print( ", " );
                        int charCount = 0;
                        for ( int i = 0; i < lastRemovedNodesIndex; i++ ) {
                            charCount += printNodeOrName( disconnectedNode( ), 1 );
                            if ( i < lastRemovedNodesIndex - 1 ) {
                                Serial.print( ", " );
                                charCount += 2;
                            }
                        }
                        for ( int i = 0; i < 8 - charCount; i++ ) {
                            Serial.print( " " );
                        }
                        Serial.print( " cleared" );
                        if ( lastRemovedNodesIndex > 0 ) {
                            Serial.print( " " );
                            Serial.print( lastRemovedNodesIndex + 1 );
                            Serial.print( " nodes" );
                        }
                        Serial.println( );
                        Serial.flush( );

                        sprintf( node1Name, "%s cleared", definesToChar( nodesToConnect[ 0 ] ) );

                        oled.clearPrintShow( node1Name, 2, true, true, true );
                        // oled.clearPrintShow("cleared  ", 1, false, true, true);
                        // oled.setTextSize(2);

                        // Serial.println(numberOfLocalChanges);
                        // clearLEDsExceptMiddle(1,60);

                        // NOTE: refreshLocalConnections() is already called inside removeBridgeFromState()
                        // No need to call it again here - that was causing double refresh delay!

                        // delay(10);
                        // waitCore2( );
                        showLEDsCore2 = -1;

                        // showLEDsCore2 = -1;

                        // else {

                        // showLEDsCore2 = -1;
                        // }
                        //  refreshLocalConnections(1);
                        //   deleteMissesIndex = 0;
                        //   for (int i = 0; i < 20; i++) {
                        //     deleteMisses[i] = -1;
                        //   }
                        //   delay(20);
                        //  showLEDsCore2 = -1;
                        fadeClear = 0;
                        fadeTimer = 0;
                    } else {
                        oled.clear( );
                        oled.clearPrintShow( node1Name, 2, true, true, true );
                    }
                }

                if ( firstConnection == -3 ) {
                    //
                    // showLEDsCore2 = 1;
                    // firstConnection = -1;

                    break;
                }

                node1or2 = 0;
                nodesToConnect[ 0 ] = -1;
                nodesToConnect[ 1 ] = -1;
                // row[1] = -2;
                doubleSelectTimeout = millis( );
            }

            row[ 1 ] = row[ 0 ];
        }
        // Serial.print("\n\r");
        // Serial.print(" ");
        // Serial.println(row[0]);

        if ( millis( ) - doubleSelectTimeout > 700 ) {
            // Serial.println("doubleSelectCountdown");
            row[ 1 ] = -2;
            lastReadRaw = 0;
            lastProbedRows[ 0 ] = 0;
            lastProbedRows[ 1 ] = 0;
            doubleSelectTimeout = millis( );
            doubleSelectCountdown = 700;
        }

        // Serial.println(doubleSelectCountdown);

        if ( doubleSelectCountdown <= 0 ) {

            doubleSelectCountdown = 0;
        } else {
            doubleSelectCountdown =
                doubleSelectCountdown - ( millis( ) - doubleSelectTimeout );

            doubleSelectTimeout = millis( );
        }

        probeTimeout = millis( );

        // NOTE: Old encoder exit code - now handled by cursor system above
        // These should never trigger since encoder returns are filtered out
        // But kept for safety in case cursor is disabled
        if ( encoderDirectionState == UP ) {
            // Clear any cursor before exiting based on zone
            if ( cursorZone == ZONE_BREADBOARD && encoderCursorNode >= 0 ) {
                b.printRawRow( 0b00000100, encoderCursorNode, 0x000000, 0x000000 );
            } else if ( cursorZone == ZONE_NANO && encoderCursorNode >= 0 ) {
                int pixel = getNanoHeaderPixel( encoderCursorNode );
                if ( pixel >= 0 )
                    leds.setPixelColor( pixel, 0x000000 );
            } else if ( cursorZone >= ZONE_RAILS ) {
                clearLEDsExceptRails( );
            }

            globalEncoderCursorNode = -1;
            globalEncoderCursorInHeader = 0;

            // Clear all color overrides using helper function
            clearColorOverrides( true, true, false );

            // Clear inPadMenu flag
            inPadMenu = 0;

            node1or2 = 0;
            nodesToConnect[ 0 ] = -1;
            nodesToConnect[ 1 ] = -1;
            row[ 0 ] = -1;
            break;
        }

        if ( encoderDirectionState == DOWN ) {
            // Clear any cursor before exiting based on zone
            if ( cursorZone == ZONE_BREADBOARD && encoderCursorNode >= 0 ) {
                b.printRawRow( 0b00000100, encoderCursorNode, 0x000000, 0x000000 );
            } else if ( cursorZone == ZONE_NANO && encoderCursorNode >= 0 ) {
                int pixel = getNanoHeaderPixel( encoderCursorNode );
                if ( pixel >= 0 )
                    leds.setPixelColor( pixel, 0x000000 );
            } else if ( cursorZone >= ZONE_RAILS ) {
                clearLEDsExceptRails( );
            }

            globalEncoderCursorNode = -1;
            globalEncoderCursorInHeader = 0;

            clearColorOverrides( 1, 1, 0 );
            // Clear inPadMenu flag
            inPadMenu = 0;

            node1or2 = 0;
            nodesToConnect[ 0 ] = -1;
            nodesToConnect[ 1 ] = -1;
            row[ 0 ] = -1;
            break;
        }

        if ( firstConnection == -2 ) {
            firstConnection = -1;
            break;
        }
    }
    // Serial.println("fuck you");
    //  digitalWrite(RESETPIN, LOW);
    node1or2 = 0;
    nodesToConnect[ 0 ] = -1;
    nodesToConnect[ 1 ] = -1;
    // row[1] = -2;
    connectedRowsIndex = 0;
    connectedRows[ 0 ] = -1;
    probeActive = false;
    probeHighlight = -1;
    showProbeLEDs = 4;
    brightenNet( -1 );
    // showLEDsCore2 = 1;
    //  Serial.print("millis() - timer[0] = ");
    //  Serial.println(millis() - timer[0]);
    //  Serial.print("millis() - timer[1] = ");
    //  Serial.println(millis() - timer[1]);
    //  Serial.print("millis() - timer[2] = ");
    //  Serial.println(millis() - timer[2]);
    //  Serial.print("millis() - timer[3] = ");
    //  Serial.println(millis() - timer[3]);

    if ( connectionsThisSession == 0 ) {
        Serial.print( "\x1b[3A\x1b[0J" ); // Clear the line and return cursor to start
        Serial.flush( );
        connectionsThisSession = 0;
    }

    Serial.flush( );

    // showLEDsCore2 = -1;
    // refreshLocalConnections(-1);
    // delay(10);
    if ( numberOfLocalChanges > 0 ) {
        // Serial.print( "Accumulated " );
        // Serial.print( numberOfLocalChanges );
        // Serial.println( " changes in RAM (will auto-save)" );
        // Serial.flush( );
        // Don't save immediately - let auto-save scheduler handle it after 2 seconds
        // This keeps probing responsive
    }
    // delay(10);
    refreshConnections( 1, 1, 0 );
    row[ 0 ] = -1;
    row[ 1 ] = -2;
    // showLEDsCore2 = -1;
    // sprintf(oledBuffer, "        ");
    // drawchar();

    // rotaryEncoderMode = wasRotaryMode;
    // routableBufferPower(0);
    // delay(10);
    // showLEDsCore2 = -1;
    oled.showJogo32h( );

    // Restore rotary divider
    rotaryDivider = savedRotaryDivider;

    // Clear any visible cursor LED before exiting based on zone
    if ( cursorZone == ZONE_BREADBOARD && encoderCursorNode >= 0 ) {
        b.printRawRow( 0b00000100, encoderCursorNode, 0x000000, 0x000000 );
    } else if ( cursorZone == ZONE_NANO && encoderCursorNode >= 0 ) {
        int pixel = getNanoHeaderPixel( encoderCursorNode );
        if ( pixel >= 0 )
            leds.setPixelColor( pixel, 0x000000 );
    } else if ( cursorZone >= ZONE_RAILS ) {
        clearLEDsExceptRails( );
    }

    // Clear encoder cursor globals and highlighting
    globalEncoderCursorNode = -1;
    globalEncoderCursorInHeader = 0;
    Highlighting::getInstance( ).clearHighlighting( );

    // Clear all color overrides using helper function
    // clearColorOverrides( true, true, false );

    // Clear inPadMenu flag
    inPadMenu = 0;

    // Reset first entry flag so next entrance starts at row 15 in breadboard
    firstProbeEntry = true;

    // Force LED update to clear cursor
    showLEDsCore2 = 2;

    // Wait for button to be released before exiting
    // This prevents the press that triggered probeMode from being detected again
    // while (probeButton.getButtonState() != 0) {
    //     delay(10);  // Wait for user to release button
    // }

    // Clear any residual button state and block for safety
    probeButton.clearButtonState( );
    blockProbeButton = 1000; // Extra 100ms safety margin
    blockProbeButtonTimer = millis( );
    clearColorOverrides( 1, 1, 0 );

    // Disable text layer when exiting probe mode

    return 1;
}

volatile int measureModeActive = 0;

float Probing::measureMode( int updateSpeed ) {
    measureModeActive = 1;
    // Wait for button release (use state-based check, doesn't consume event)
    // while ( checkProbeButtonState( ) != 0 ) {
    //     delay( 1 );
    // }
    //   removeBridgeFromNodeFile(ROUTABLE_BUFFER_IN, -1, netSlot, 1);
    // removeBridgeFromNodeFile(ROUTABLE_BUFFER_OUT, -1, netSlot, 1);
    //   addBridgeToNodeFile(ROUTABLE_BUFFER_OUT, ADC3, netSlot, 1);

    // refreshLocalConnections();
    float measurement = 0.0;
    // Loop until button pressed (use state-based check, doesn't consume event)
    while ( checkProbeButtonState( ) == 0 ) {
        measurement = ( readAdc( 7, 16 ) * ( 16.0 / 4090 ) ) - 8.0;
        if ( measurement > -0.05 && measurement < 0.05 ) {
            measurement = 0.0;
            delay( 1 );
        }
        uint32_t measColor = scaleBrightness(
            logoColors8vSelect[ map( (long)( measurement * 10 ), -80, 80, 0, 59 ) ], -50 );
        // Serial.println(map((long)(measurement*10), -80, 80, 0, 59));
        char measChar[ 10 ] = "         ";
        // b.print("        ", (uint32_t)0x000000,(uint32_t)0xffffff);
        b.clear( 0 );
        sprintf( measChar, "% .1f V", measurement );

        b.print( measChar, (uint32_t)measColor, (uint32_t)0xfffffe );

        Serial.print( "                        \r" );
        Serial.print( measChar );

        delay( updateSpeed );
    }
    // removeBridgeFromNodeFile(ROUTABLE_BUFFER_OUT, -1, netSlot, 1);

    // addBridgeToNodeFile(ROUTABLE_BUFFER_IN, RP_GPIO_23, netSlot, 1);

    // refreshLocalConnections(-1);
    // delay(20);
    measureModeActive = 0;
    showProbeLEDs = 3;
    return measurement;
}

unsigned long blinkTimer = 0;

int Probing::selectSFprobeMenu( int function ) {

    if ( checkingPads == 1 ) {
        // inPadMenu = 0;
        return function;
    }

    // bool selectFunction = false;
    inPadMenu = 1;
    switch ( function ) {

    case ADC_PAD: {
        inPadMenu = 1;
        function = chooseADC( );
        blockProbing = 800;
        blockProbingTimer = millis( );
        // delay(10);
        inPadMenu = 0;
        clearColorOverrides( 1, 1, 0 );
        setLogoOverride( ADC_0, -2 );
        setLogoOverride( ADC_1, -2 );
        break;
    }
    case DAC_PAD: {
        inPadMenu = 1;
        function = chooseDAC( );
        blockProbing = 800;
        blockProbingTimer = millis( );
        // delay(10);
        inPadMenu = 0;
        clearColorOverrides( 1, 1, 0 );
        setLogoOverride( DAC_0, -2 );
        setLogoOverride( DAC_1, -2 );
        break;
    }
    case GPIO_PAD: {

        function = chooseGPIO( );
        blockProbing = 800;
        blockProbingTimer = millis( );
        // delay(10);
        clearColorOverrides( 1, 1, 0 );
        setLogoOverride( GPIO_0, -2 );
        setLogoOverride( GPIO_1, -2 );
        break;
    }
    case BUILDING_PAD_TOP:
    case BUILDING_PAD_BOTTOM:
    case LOGO_PAD_TOP:
    case LOGO_PAD_BOTTOM: {

        // b.clear();
        //     function = -1;
        // break;
        // b.clear();
        clearLEDsExceptRails( );
        switch ( function ) {
        case LOGO_PAD_TOP: {
            inPadMenu = 1;
            b.print( "UART", sfOptionColors[ 3 ], 0xFFFFFF, 0, 0, 0 );
            b.print( "Tx", sfOptionColors[ 7 ], 0xFFFFFF, 0, 1, 0 );
            b.printRawRow( 0b00000001, 23, 0x400014, 0xffffff );
            b.printRawRow( 0b00000011, 24, 0x400014, 0xffffff );
            b.printRawRow( 0b00011111, 25, 0x400014, 0xffffff );
            b.printRawRow( 0b00011011, 26, 0x400014, 0xffffff );
            b.printRawRow( 0b00000001, 27, 0x400014, 0xffffff );

            b.printRawRow( 0b00011100, 53, 0x400014, 0xffffff );
            b.printRawRow( 0b00011000, 54, 0x400014, 0xffffff );
            b.printRawRow( 0b00010000, 55, 0x400014, 0xffffff );
            function = resolveLogoPadAssignment( jumperlessConfig.logo_pads.top_guy, RP_UART_TX );
            clearColorOverrides( 1, 1, 0 );
            setLogoOverride( LOGO_TOP, -2 );

            break;
        }
        case LOGO_PAD_BOTTOM: {
            inPadMenu = 1;
            b.print( "UART", sfOptionColors[ 3 ], 0xFFFFFF, 0, 0, -1 );
            b.print( "Rx", sfOptionColors[ 5 ], 0xFFFFFF, 0, 1, -1 );

            b.printRawRow( 0b00000000, 25, 0x280032, 0xffffff );
            b.printRawRow( 0b00000001, 26, 0x280032, 0xffffff );
            b.printRawRow( 0b00000011, 27, 0x280032, 0xffffff );

            b.printRawRow( 0b00001110, 53, 0x280032, 0xffffff );
            b.printRawRow( 0b00011110, 54, 0x280032, 0xffffff );
            b.printRawRow( 0b00010000, 55, 0x280032, 0xffffff );
            b.printRawRow( 0b00011111, 56, 0x280032, 0xffffff );
            b.printRawRow( 0b00011111, 57, 0x280032, 0xffffff );
            b.printRawRow( 0b00000011, 58, 0x280032, 0xffffff );

            b.printRawRow( 0b00000001, 52, 0x050500, 0xfffffe );
            b.printRawRow( 0b00000001, 53, 0x050500, 0xfffffe );
            b.printRawRow( 0b00000001, 54, 0x050500, 0xfffffe );
            b.printRawRow( 0b00000001, 55, 0x050500, 0xfffffe );
            b.printRawRow( 0b00000001, 59, 0x050500, 0xfffffe );
            function = resolveLogoPadAssignment( jumperlessConfig.logo_pads.bottom_guy, RP_UART_RX );
            clearColorOverrides( 1, 1, 0 );
            setLogoOverride( LOGO_BOTTOM, -2 );

            break;
        }
            // }
        case BUILDING_PAD_TOP:
        case BUILDING_PAD_BOTTOM: {
            // Both building pads now use the same chooser function
            function = chooseIsense( );
            break;
        }
        }

        // showLEDsCore2 = 2;
        // delayWithButton( 900 );

        // b.clear();
        clearLEDsExceptRails( );
        blockProbing = 800;
        blockProbingTimer = millis( );

        // lastReadRaw = 0;
        // b.print("Attach", sfOptionColors[0], 0xFFFFFF, 0, 0, -1);
        // b.print("to Pad", sfOptionColors[2], 0xFFFFFF, 0, 1, -1);
        // showLEDsCore2 = 2;

        // delayWithButton(800);

        // delay(800);

        // function = attachPadsToSettings(function);

        if (node1or2 == 0) {
            node1or2 = 1;
            nodesToConnect[ 0 ] = function;
            nodesToConnect[ 1 ] = -1;
            connectedRowsIndex = 1;
        } else {
            nodesToConnect[ 1 ] = function;
            
            //connectedRowsIndex = 0;
        }

        // Serial.print("function!!!!!: ");
        // printNodeOrName(function, 1);
        showLEDsCore2 = 1;
        lightUpRail( );
        // delay( 200 );
        inPadMenu = 0;
        sfProbeMenu = 0;
        // return function;

        // delay( 100 );

        break;
    }

    case 0: {
        Serial.print( "function: " );
        printNodeOrName( function, 1 );
        Serial.print( function );
        Serial.println( );
        function = -1;
        break;
    }
    case TOP_RAIL_GND:
    case BOTTOM_RAIL_GND: {
        function = 100;
        break;
    }
    default: {
        // inPadMenu = 0;
    }
    }
    connectedRows[ 0 ] = function;
    connectedRowsIndex = 1;
    lightUpRail( );
    // delay(500);
    // showLEDsCore2 = 1;
    // delayWithButton(900);
    sfProbeMenu = 0;
    inPadMenu = 0;

    return function;
}

int Probing::attachPadsToSettings( int pad ) {
    int function = -1;
    int functionSetting = -1; // 0 = DAC, 1 = ADC, 2 = GPIO
    int settingOption =
        -1; // 0 = toggle, 1 = up/down, 2 = pwm, 3 = set voltage, 4 = input
    int dacChosen = -1;
    int adcChosen = -1;
    int gpioChosen = -1;
    connectedRowsIndex = 0;
    connectedRows[ 0 ] = -1;
    node1or2 = 0;
    unsigned long skipTimer = millis( );
    inPadMenu = 1;
    b.clear( );
    clearLEDsExceptRails( );
    // showLEDsCore2 = 2;
    //   lastReadRaw = 0;
    b.print( "DAC", sfOptionColors[ 0 ], 0xFFFFFF, 0, 0, -1 );
    b.print( "ADC", sfOptionColors[ 1 ], 0xFFFFFF, 4, 0, 0 );
    b.print( "GPIO", sfOptionColors[ 2 ], 0xFFFFFF, 8, 1, 1 );

    int selected = -1;

    while ( selected == -1 && longShortPress( 500 ) != 1 &&
            longShortPress( 500 ) != 2 ) {
        int reading = justReadProbe( );
        if ( reading != -1 ) {
            switch ( reading ) {
            case 1 ... 13: {
                selected = 0;
                functionSetting = 0;
                dacChosen = chooseDAC( 1 );
                Serial.print( "dacChosen: " );
                Serial.println( dacChosen );
                // b.clear();
                settingOption = dacChosen - DAC0;
                clearLEDsExceptRails( );
                // showLEDsCore2 = 1;

                break;
            }
            case 18 ... 30: {
                selected = 1;
                functionSetting = 1;
                adcChosen = chooseADC( );
                Serial.print( "adcChosen: " );
                Serial.println( adcChosen );
                settingOption = adcChosen - ADC0;

                // b.clear();
                clearLEDsExceptRails( );
                delayWithButton( 400 );
                // showLEDsCore2 = 1;

                break;
            }
            case 37 ... 53: {
                selected = 2;
                functionSetting = 2;
                // b.clear();
                clearLEDsExceptRails( );
                // showLEDsCore2 = 2;

                gpioChosen = chooseGPIO( 1 );
                // b.clear();
                clearLEDsExceptRails( );
                // showLEDsCore2 = 2;
                // if (gpioChosen >= 122 && gpioChosen <= 125) {
                //   gpioChosen = gpioChosen - 122 + 5;
                //   } else if (gpioChosen >= 135 && gpioChosen <= 138) {
                //     gpioChosen = gpioChosen - 134;
                //     }
                if ( gpioChosen >= RP_GPIO_1 && gpioChosen <= RP_GPIO_8 ) {
                    gpioChosen = gpioChosen - RP_GPIO_1 + 1;
                }

                // Serial.print( "gpioChosen: " );
                // Serial.println( gpioChosen );
                // Serial.print( "gpioState[gpioChosen]: " );
                // Serial.println( gpioState[ gpioChosen - 1 ] );
                if ( gpioState[ gpioChosen - 1 ] != 0 ) {
                    clearLEDsExceptRails( );
                    // showLEDsCore2 = 2;
                    Serial.print( "Set GP" );
                    Serial.print( gpioChosen );
                    Serial.println( " to Output" );
                    char gpString[ 4 ];
                    itoa( gpioChosen, gpString, 10 );

                    b.print( "GPIO", sfOptionColors[ ( gpioChosen + 1 ) % 7 ], 0xFFFFFF, 0, 0,
                             0 );
                    b.print( gpString, sfOptionColors[ gpioChosen - 1 ], 0xFFFFFF, 4, 0, 3 );
                    // b.print(" ", sfOptionColors[0], 0xFFFFFF, 0, 1, -2);
                    b.print( "Output", sfOptionColors[ ( gpioChosen + 3 ) % 7 ], 0xFFFFFF, 1,
                             1, 1 );
                    b.printRawRow( 0b00000100, 31, 0x200010, 0xffffff );
                    b.printRawRow( 0b00000100, 32, 0x200010, 0xffffff );
                    b.printRawRow( 0b00010101, 33, 0x200010, 0xffffff );
                    b.printRawRow( 0b00001110, 34, 0x200010, 0xffffff );
                    b.printRawRow( 0b00000100, 35, 0x200010, 0xffffff );
                    // showLEDsCore2 = 2;
                    delayWithButton( 400 );

                } else {
                }
                Serial.print( "gpioChosen - 1: " );
                Serial.println( gpioChosen - 1 );
                Serial.flush( );
                gpioState[ gpioChosen - 1 ] = 0;
                settingOption = gpioChosen - 1;
                setGPIO( );
                clearLEDsExceptRails( );

                // showLEDsCore2 = 2;
                b.print( "Tap to", sfOptionColors[ ( gpioChosen + 1 ) % 7 ], 0xFFFFFF, 0, 0,
                         1 );
                b.print( "toggle", sfOptionColors[ ( gpioChosen + 2 ) % 7 ], 0xFFFFFF, 0, 1,
                         1 );
                delayWithButton( 500 );
                clearLEDsExceptRails( );
                // showLEDsCore2 = 1;
                // inPadMenu = 0;

                break;
            }
            }
        }
    }
    // inPadMenu = 0;
    Serial.print( "pad: " );
    Serial.println( pad );
    Serial.print( "functionSetting: " );
    Serial.println( functionSetting );
    Serial.print( "settingOption: " );
    Serial.println( settingOption );
    switch ( functionSetting ) {
    case 2: {
        switch ( gpioChosen ) {
        case 1: {
            function = RP_GPIO_1;
            break;
        }
        case 2: {
            function = RP_GPIO_2;
            break;
        }
        case 3: {
            function = RP_GPIO_3;
            break;
        }
        case 4: {
            function = RP_GPIO_4;
            break;
        }
        case 5: {
            function = RP_GPIO_5;
            break;
        }
        case 6: {
            function = RP_GPIO_6;
            break;
        }
        case 7: {
            function = RP_GPIO_7;
            break;
        }
        case 8: {
            function = RP_GPIO_8;
            break;
        }
        }
        break;
    }
    case 1: {
        function = adcChosen;
        break;
    }
    case 0: {
        function = dacChosen;
        break;
    }
    }

    switch ( pad ) {
    case LOGO_PAD_TOP: {
        jumperlessConfig.logo_pads.top_guy = nodeToLogoPadConfig( function, jumperlessConfig.logo_pads.top_guy );
        // jumperlessConfig.logo_pads.top_guy = settingOption;

        break;
    }
    case LOGO_PAD_BOTTOM: {
        jumperlessConfig.logo_pads.bottom_guy = nodeToLogoPadConfig( function, jumperlessConfig.logo_pads.bottom_guy );
        // jumperlessConfig.logo_pads.bottom_guy = settingOption;
        break;
    }
    case BUILDING_PAD_TOP: {
        jumperlessConfig.logo_pads.building_pad_top = nodeToLogoPadConfig( function, jumperlessConfig.logo_pads.building_pad_top );
        // jumperlessConfig.logo_pads.building_pad_top= settingOption;
        break;
    }
    case BUILDING_PAD_BOTTOM: {
        jumperlessConfig.logo_pads.building_pad_bottom = nodeToLogoPadConfig( function, jumperlessConfig.logo_pads.building_pad_bottom );
        // jumperlessConfig.logo_pads.building_pad_bottom_setting = settingOption;
        break;
    }
    }
    saveLogoBindings( );
    delay( 3 );
    inPadMenu = 0;
    showLEDsCore2 = 1;
    return function;
}

int Probing::delayWithButton( int delayTime ) {
    // Rewritten to use state-based API (doesn't consume button events)
    // This allows button presses to propagate to outer probe mode logic
    unsigned long skipTimer = millis( );
    int lastSeenState = 0;

    while ( millis( ) - skipTimer < delayTime ) {
        // Check CURRENT state without consuming events
        int currentState = probeButton.getButtonState( );

        // Detect button press (transition from 0 to pressed)
        if ( currentState != 0 && lastSeenState == 0 ) {
            // Button just pressed - return which button it was
            // Serial.print("delayWithButton detected press: ");
            // Serial.println(currentState);
            return currentState;
        }

        lastSeenState = currentState;
        delayMicroseconds( 100 );
    }

    // Timeout - no button pressed
    return 0;
}

int Probing::chooseDAC( int justPickOne ) {
    int function = -1;
    // b.clear();
    clearLEDsExceptRails( );
    showLEDsCore2 = 2;

    // lastReadRaw = 0;
    b.print( "DAC", scaleDownBrightness( rawOtherColors[ 9 ], 4, 22 ), 0xFFFFFF, 1, 0,
             3 );

    b.print( "0", sfOptionColors[ 0 ], 0xFFFFFF, 0, 1, 3 );
    // b.print("5v", sfOptionColors[0], 0xFFFFFF, 0, 0, -2);
    //  b.printRawRow(0b00011000, 31, sfOptionColors[7], 0xffffff);
    //  b.printRawRow(0b00000100, 32, sfOptionColors[7], 0xffffff);
    //  b.printRawRow(0b00000100, 33, sfOptionColors[7], 0xffffff);
    //  b.printRawRow(0b00010101, 34, sfOptionColors[7], 0xffffff);
    //  b.printRawRow(0b00001110, 35, sfOptionColors[7], 0xffffff);
    //  b.printRawRow(0b00000100, 36, sfOptionColors[7], 0xffffff);
    //  b.printRawRow(0b00011100, 32,sfOptionColors[0], 0xffffff);
    //   b.printRawRow(0b00011100, 33,sfOptionColors[0], 0xffffff);

    b.print( "1", sfOptionColors[ 2 ], 0xFFFFFF, 5, 1, 0 );
    // b.print("8v", sfOptionColors[2], 0xFFFFFF, 5, 0, 1);
    // b.printRawRow(0b00011000, 58, sfOptionColors[4], 0xffffff);
    // b.printRawRow(0b00000100, 57, sfOptionColors[4], 0xffffff);
    // b.printRawRow(0b00000100, 56, sfOptionColors[4], 0xffffff);
    // b.printRawRow(0b00010101, 55, sfOptionColors[4], 0xffffff);
    // b.printRawRow(0b00001110, 54, sfOptionColors[4], 0xffffff);
    // b.printRawRow(0b00000100, 53, sfOptionColors[4], 0xffffff);

    sfProbeMenu = 2;

    int selected = -1;
    function = 0;
    while ( selected == -1 ) {

        jOS.serviceCritical( );

        if ( ProbeButton::getInstance( ).getButtonState( ) != 0 ) {
            selected = DAC1;
            function = DAC1;
            break;
        }
        int reading = justReadProbe( );
        if ( reading != -1 ) {
            switch ( reading ) {
            case 31 ... 43: {
                selected = DAC0;
                function = DAC0;
                if ( justPickOne == 1 ) {
                    return function;
                }

                // Use new unified voltage adjuster with probe support
                VoltageAdjustConfig config;
                config.minVoltage = -8.0;
                config.maxVoltage = 8.0;
                config.initialValue = globalState.power.dac0;
                config.label = "DAC 0";
                config.enableSnap = false;
                config.liveUpdateInRange = true;
                config.liveUpdateMin = 0.0;
                config.liveUpdateMax = 5.0;
                config.callback = []( float newValue, bool isLive, void* context ) {
                    setDac0voltage( newValue, 1, 0, false );
                    globalState.power.dac0 = newValue;
                };

                AdjustResult result = VoltageAdjuster::adjust( config );
                if ( result == AdjustResult::CONFIRMED ) {
                    // Save to persistent storage
                    saveVoltages( globalState.power.topRail, globalState.power.bottomRail,
                                  globalState.power.dac0, globalState.power.dac1 );
                }

                // showNets();
                showLEDsCore2 = -1;
                delay( 100 );

                break;
            }
            case 48 ... 60: {
                selected = 107;
                function = 107;
                if ( justPickOne == 1 ) {
                    return function;
                    // break;
                }

                // Use new unified voltage adjuster with probe support
                VoltageAdjustConfig config;
                config.minVoltage = -8.0;
                config.maxVoltage = 8.0;
                config.initialValue = globalState.power.dac1;
                config.label = "DAC 1";
                config.enableSnap = false;
                config.liveUpdateInRange = true;
                config.liveUpdateMin = 0.0;
                config.liveUpdateMax = 5.0;
                config.callback = []( float newValue, bool isLive, void* context ) {
                    setDac1voltage( newValue, 1, 0, false );
                    globalState.power.dac1 = newValue;
                };

                AdjustResult result = VoltageAdjuster::adjust( config );
                if ( result == AdjustResult::CONFIRMED ) {
                    // Save to persistent storage
                    saveVoltages( globalState.power.topRail, globalState.power.bottomRail,
                                  globalState.power.dac0, globalState.power.dac1 );
                }

                // showNets();
                blockProbeButton = 2000;
                blockProbeButtonTimer = millis( );
                showLEDsCore2 = -1;
                probeButton.clearButtonState( );
                delay( 100 );
                break;
            }
            }
        }
    }

    return function;
}

int Probing::chooseIsense( void ) {
    int function = -1;
    int selectedOption = 0; // 0 = ISENSE_PLUS (default), 1 = ISENSE_MINUS

    // Prevent net LEDs from overwriting our menu
    inPadMenu = 1;

    // Clear LEDs before showing menu
    clearLEDsExceptRails( );
    b.clear( );
    showLEDsCore2 = 2;

    // Track encoder position for selection with accumulator
    long lastEncPos = encoderPosition;
    int lastSelectedOption = -1;  // Track if we need to redraw
    int encoderAccumulator = 0;   // Accumulate encoder clicks
    const int clicksToSwitch = 5; // Require 8 clicks to switch

    // Serial.println( "Choose Current Sense" );
    // Serial.println( "  I+ (ISENSE_PLUS) or I- (ISENSE_MINUS)" );
    // Serial.println( "  Encoder: rotate to select, press to confirm" );

    // Color definitions for I+ (red) and I- (green)
    const uint32_t plusBrightColor = 0x2A0002;  // Bright red for selected I+
    const uint32_t plusDimColor = 0x0a0000;     // Dim red for unselected I+
    const uint32_t minusBrightColor = 0x002A02; // Bright green for selected I-
    const uint32_t minusDimColor = 0x000A00;    // Dim green for unselected I-

    // Initial display
    b.clear( );
    clearLEDsExceptRails( );
    uint32_t plusColor = ( selectedOption == 0 ) ? plusBrightColor : plusDimColor;
    uint32_t minusColor = ( selectedOption == 1 ) ? minusBrightColor : minusDimColor;
    b.print( "Current", sfOptionColors[ 6 ], 0xFFFFFF, 0, 0, 1 );
    b.print( "I+", plusColor, 0xFFFFFF, 1, 1, -2 );
    b.print( "I-", minusColor, 0xFFFFFF, 4, 1, 2 );
    oled.clearPrintShow( "Current\n I +     I -", 2, 100 );

    showLEDsCore2 = 2;
    lastSelectedOption = selectedOption;

    int selected = -1;
    while ( selected == -1 ) {
        // Keep critical services running
        jOS.serviceCritical( );

        // Check for encoder movement and accumulate
        long currentEncPos = encoderPosition;
        long encDelta = currentEncPos - lastEncPos;
        if ( encDelta != 0 ) {
            encoderAccumulator += encDelta;
            lastEncPos = currentEncPos;

            // Switch option when accumulator reaches threshold
            if ( encoderAccumulator >= clicksToSwitch ) {
                selectedOption = 0; // ISENSE_MINUS
                encoderAccumulator = 0;
            } else if ( encoderAccumulator <= -clicksToSwitch ) {
                selectedOption = 1; // ISENSE_PLUS
                encoderAccumulator = 0;
            }
        }

        // Only update display if selection changed
        if ( selectedOption != lastSelectedOption ) {
            b.clear( );
            clearLEDsExceptRails( );
            if ( selectedOption == 0 ) {
                oled.clearPrintShow( "I Sense +", 2, 100 );
            } else {
                oled.clearPrintShow( "I Sense -", 2, 100 );
                // oled.clearPrintShow( "I+", 2, 100 );
            }

            plusColor = ( selectedOption == 0 ) ? plusBrightColor : plusDimColor;
            minusColor = ( selectedOption == 1 ) ? minusBrightColor : minusDimColor;

            b.print( "Current", sfOptionColors[ 6 ], 0xFFFFFF, 0, 0, 1 );
            b.print( "I+", plusColor, 0xFFFFFF, 1, 1, -2 );
            b.print( "I-", minusColor, 0xFFFFFF, 4, 1, 2 );
            showLEDsCore2 = 2;

            lastSelectedOption = selectedOption;
        }

        // Check for encoder button press
        if ( encoderButtonState == PRESSED && lastButtonEncoderState == IDLE ) {
            encoderButtonState = IDLE;
            lastButtonEncoderState = IDLE;
            selected = selectedOption;
            break;
        }

        // Also check for probe touch for backward compatibility
        int reading = justReadProbe( );
        if ( reading != -1 ) {
            switch ( reading ) {
            case 31 ... 43: {
                selected = 0; // ISENSE_PLUS
                break;
            }
            case 47 ... 60: {
                selected = 1; // ISENSE_MINUS
                break;
            }
            }
        }

        // Check for button press to exit
        if ( probeButton.getButtonState( ) == 1 ) {
            probeButton.clearButtonState( );
            blockProbeButton = 1000;
            blockProbeButtonTimer = millis( );
            // selected = -1;
            break;
        } else if ( probeButton.getButtonState( ) == 2 ) {
            probeButton.clearButtonState( );
            blockProbeButton = 1000;
            blockProbeButtonTimer = millis( );
            //   selected = -1;
            break;
        }

        delayMicroseconds( 200 );
    }

    // Map selection to function
    if ( selected == 0 ) {
        function = ISENSE_PLUS;
    } else if ( selected == 1 ) {
        function = ISENSE_MINUS;
    } else {
        function = -1;
    }

    delay( 100 );

    // Serial.print( "Current Sense selected: " );
    // Serial.println( function == ISENSE_PLUS ? "ISENSE_PLUS (+)" : function == ISENSE_MINUS ? "ISENSE_MINUS (-)" : "None" );
    // Serial.flush( );

    clearLEDsExceptRails( );
    b.clear( );
    showLEDsCore2 = -1;

    // Clear inPadMenu flag
    inPadMenu = 0;

    return function;
}

int Probing::chooseADC( void ) {
    int function = -1;
    // b.clear();

    // probeActive = false;
    clearLEDsExceptRails( );

    // lastReadRaw = 0;
    // inPadMenu = 0;
    // showLEDsCore2 = 2;

    // waitCore2();

    // inPadMenu = 1;

    // sfProbeMenu = 1;
    // delay(100);
    // clearLEDsExceptRails();

    // core1busy = 1;
    b.print( " ADC", scaleDownBrightness( rawOtherColors[ 8 ], 4, 22 ), 0xFFFFFF, 0, 0,
             3 );

    //  delay(1000);
    //  function = 111;
    b.print( "0", sfOptionColors[ 0 ], 0xFFFFFF, 0, 1, -1 );
    b.print( "1", sfOptionColors[ 1 ], 0xFFFFFF, 1, 1, 0 );
    b.print( "2", sfOptionColors[ 2 ], 0xFFFFFF, 2, 1, 1 );
    b.print( "3", sfOptionColors[ 3 ], 0xFFFFFF, 3, 1, 2 );
    b.print( "4", sfOptionColors[ 4 ], 0xFFFFFF, 4, 1, 3 );
    b.print( "P", sfOptionColors[ 5 ], 0xFFFFFF, 5, 1, 4 );

    showLEDsCore2 = 2;
    // Serial.print("inPadMenu: ");
    // Serial.println(inPadMenu);
    // Serial.print("sfProbeMenu: ");
    // Serial.println(sfProbeMenu);
    // Serial.print("probeActive: ");
    // Serial.println(probeActive);
    // while (true);
    int selected = -1;
    while ( selected == -1 && longShortPress( 500 ) != 1 ) {
        int reading = justReadProbe( );
        // Serial.print("reading: ");
        // Serial.println(reading);
        if ( reading != -1 ) {
            //       Serial.print("reading: ");
            // Serial.println(reading);
            switch ( reading ) {
            case 31 ... 35: {
                selected = ADC0;
                function = ADC0;

                break;
            }
            case 36 ... 40: {
                selected = ADC1;
                function = ADC1;
                // while (justReadProbe() == reading) {
                //   // Serial.print("reading: ");
                //   // Serial.println(justReadProbe());
                //   delay(10);
                // }
                break;
            }
            case 41 ... 45: {
                selected = ADC2;
                function = ADC2;
                // while (justReadProbe() == reading) {
                //   // Serial.print("reading: ");
                //   // Serial.println(justReadProbe());
                //   delay(10);
                // }
                break;
            }
            case 46 ... 50: {
                selected = ADC3;
                function = ADC3;
                // while (justReadProbe() == reading) {
                //   // Serial.print("reading: ");
                //   // Serial.println(justReadProbe());
                //   delay(10);
                // }
                break;
            }
            case 51 ... 55: {
                selected = ADC4;
                function = ADC4;
                // while (justReadProbe() == reading) {
                //   // Serial.print("reading: ");
                //   // Serial.println(justReadProbe());
                //   delay(10);
                // }
                break;
            }
            case 56 ... 60: {
                selected = ADC7;
                function = ADC7;
                // while (justReadProbe() == reading) {
                //   // Serial.print("reading: ");
                //   // Serial.println(justReadProbe());
                //   delay(10);
                // }
                break;
            }
            }
            // while (justReadProbe() == reading) {
            //   Serial.print("reading: ");
            //   Serial.println(justReadProbe());
            //   delay(100);
            // }
        }
    }

    clearLEDsExceptRails( );
    // showNets();
    // showLEDsCore2 = 1;
    return function;
}

int Probing::chooseGPIOinputOutput( int gpioChosen ) {
    int settingOption = -1;
    int selectedOption = 0; // 0 = input (default), 1 = output

    // Prevent net LEDs from overwriting our menu
    inPadMenu = 1;

    // Clear LEDs before showing menu
    clearLEDsExceptRails( );
    b.clear( );
    showLEDsCore2 = 2;

    // Show initial display
    char gpioNumStr[ 3 ];
    snprintf( gpioNumStr, sizeof( gpioNumStr ), "%d", gpioChosen );

    // Track encoder position for selection with accumulator
    long lastEncPos = encoderPosition;
    int lastSelectedOption = -1;  // Track if we need to redraw
    int encoderAccumulator = 0;   // Accumulate encoder clicks
    const int clicksToSwitch = 8; // Require 8 clicks to switch

    // Serial.print( "GPIO " );
    // Serial.print( gpioChosen );
    // Serial.print( " - Select Input or Output" );
    // Serial.println( "  Encoder: rotate to select, press to confirm" );

    // Initial display
    b.clear( );
    clearLEDsExceptRails( );
    uint32_t inputColor = ( selectedOption == 0 ) ? 0x4500e8 : 0x150050;
    uint32_t outputColor = ( selectedOption == 1 ) ? 0x4500e8 : 0x150050;
    b.print( "Input", inputColor, 0xFFFFFF, 1, 0, 3 );
    b.print( gpioNumStr, sfOptionColors[ gpioChosen - 1 ], 0xFFFFFF, 0, 0, -2 );
    b.print( "Output", outputColor, 0xFFFFFF, 0, 1, 3 );
    showLEDsCore2 = 2;
    lastSelectedOption = selectedOption;

    while ( settingOption == -1 ) {
        // Keep critical services running
        jOS.serviceCritical( );

        // Check for encoder movement and accumulate
        long currentEncPos = encoderPosition;
        long encDelta = currentEncPos - lastEncPos;
        if ( encDelta != 0 ) {
            encoderAccumulator += encDelta;
            lastEncPos = currentEncPos;

            // Switch option when accumulator reaches threshold
            if ( encoderAccumulator >= clicksToSwitch ) {
                selectedOption = 1; // Output
                encoderAccumulator = 0;
            } else if ( encoderAccumulator <= -clicksToSwitch ) {
                selectedOption = 0; // Input
                encoderAccumulator = 0;
            }
        }

        // Only update display if selection changed
        if ( selectedOption != lastSelectedOption ) {
            b.clear( );
            clearLEDsExceptRails( );

            inputColor = ( selectedOption == 0 ) ? 0x4500e8 : 0x150050;
            outputColor = ( selectedOption == 1 ) ? 0x4500e8 : 0x150050;

            b.print( "Input", inputColor, 0xFFFFFF, 1, 0, 3 );
            b.print( gpioNumStr, sfOptionColors[ gpioChosen - 1 ], 0xFFFFFF, 0, 0, -2 );
            b.print( "Output", outputColor, 0xFFFFFF, 0, 1, 3 );
            showLEDsCore2 = 2;

            lastSelectedOption = selectedOption;
        }

        // Check for encoder button press
        if ( encoderButtonState == PRESSED && lastButtonEncoderState == IDLE ) {
            encoderButtonState = IDLE;
            lastButtonEncoderState = IDLE;
            settingOption = selectedOption;
            break;
        }

        // Also check for probe touch for backward compatibility
        int reading = justReadProbe( );
        if ( reading != -1 ) {
            switch ( reading ) {
            case 9 ... 29: {
                settingOption = 0; // Input
                break;
            }
            case 35 ... 59: {
                settingOption = 1; // Output
                break;
            }
            }
            blockProbing = 1000;
            blockProbingTimer = millis( );
        }

        // // Check for button press to exit
        // if ( longShortPress( 500 ) == 1 ) {
        //     blockProbing = 1000;
        //     blockProbingTimer = millis( );
        //     break;
        // }

        delayMicroseconds( 200 );
    }

    // Apply the selection
    if ( settingOption == 0 ) {
        // Input selected
        gpioState[ gpioChosen - 1 ] = 4;
        if ( globalState.config.gpioDirection[ gpioChosen - 1 ] == 0 ) {
            globalState.config.gpioDirection[ gpioChosen - 1 ] = 1;
            globalState.markDirty( );
            configChanged = true;
        }
    } else if ( settingOption == 1 ) {
        // Output selected
        gpioState[ gpioChosen - 1 ] = 0;
        if ( globalState.config.gpioDirection[ gpioChosen - 1 ] == 1 ) {
            globalState.config.gpioDirection[ gpioChosen - 1 ] = 0;
            globalState.markDirty( );
            configChanged = true;
        }
    }

    // Serial.print( "gpioChosen (chooseGPIOinputOutput): " );
    // Serial.print( gpioChosen );
    // Serial.print( " -> " );
    // Serial.println( settingOption == 0 ? "Input" : "Output" );
    // Serial.flush( );

    clearLEDsExceptRails( );
    b.clear( );
    showLEDsCore2 = -1;

    // Clear inPadMenu flag
    inPadMenu = 0;

    return settingOption;
}

int Probing::chooseGPIO( int skipInputOutput ) {
    int function = -1;

    b.clear( );
    clearLEDsExceptRails( );
    showLEDsCore2 = 2;
    sfProbeMenu = 3;
    // lastReadRaw = 0;
    // b.print("3v", 0x0f0002, 0xFFFFFF, 0, 0, -2);

    uint32_t inColor = 0x000606;
    uint32_t outColor = 0x060100;
    if ( connectOrClearProbe == 0 ) {
        inColor = 0x000000;
        outColor = 0x000000;
    }
    if ( node1or2 == 0 ) {
        Serial.println( "           Choose GPIO" );
    } else {
        Serial.println( "Choose GPIO" );
    }
    Serial.println( "  tap pads near numbers to choose" );
    Serial.println( "       ⁱ = input, ⁰ = output\n\r" );
    Serial.println( "      ┌─┬─┐ ┌─┬─┐ ┌─┬─┐ ┌─┬─┐" );
    Serial.println( "     ╭───────────────────────╮" );
    Serial.println( "     │ ⁱ1⁰   ⁱ2⁰   ⁱ3⁰   ⁱ4⁰ │" );
    Serial.println( "     ├───────────────────────┤" );
    Serial.println( "     │ ᵢ5₀   ᵢ6₀   ᵢ7₀   ᵢ8₀ │" );
    Serial.println( "     ╰───────────────────────╯" );
    Serial.println( "      └─┴─┘ └─┴─┘ └─┴─┘ └─┴─┘" );

    b.printRawRow( 0b00011000, 2, inColor, 0xFFFFFF );
    b.print( "1", sfOptionColors[ 0 ], 0xFFFFFF, 0, 0, 1 );
    b.printRawRow( 0b00011000, 6, outColor, 0xFFFFFF );
    b.printRawRow( 0b00011000, 7, outColor, 0xFFFFFF );
    b.printRawRow( 0b00011000, 9, inColor, 0xFFFFFF );
    b.print( "2", sfOptionColors[ 1 ], 0xFFFFFF, 2, 0, 0 );
    b.printRawRow( 0b00011000, 13, outColor, 0xFFFFFF );
    b.printRawRow( 0b00011000, 14, outColor, 0xFFFFFF );
    b.printRawRow( 0b00011000, 16, inColor, 0xFFFFFF );
    b.print( "3", sfOptionColors[ 2 ], 0xFFFFFF, 4, 0, -1 );
    b.printRawRow( 0b00011000, 20, outColor, 0xFFFFFF );
    b.printRawRow( 0b00011000, 21, outColor, 0xFFFFFF );
    b.printRawRow( 0b00011000, 23, inColor, 0xFFFFFF );
    b.print( "4", sfOptionColors[ 3 ], 0xFFFFFF, 6, 0, -2 );
    b.printRawRow( 0b00011000, 27, outColor, 0xFFFFFF );
    b.printRawRow( 0b00011000, 28, outColor, 0xFFFFFF );

    // b.print("5v", 0x0f0200, 0xFFFFFF, 0, 1, -2);
    b.printRawRow( 0b00000011, 32, inColor, 0xFFFFFF );
    b.print( "5", sfOptionColors[ 4 ], 0xFFFFFF, 0, 1, 1 );
    b.printRawRow( 0b00000011, 36, outColor, 0xFFFFFF );
    b.printRawRow( 0b00000011, 37, outColor, 0xFFFFFF );
    b.printRawRow( 0b00000011, 39, inColor, 0xFFFFFF );
    b.print( "6", sfOptionColors[ 5 ], 0xFFFFFF, 2, 1, 0 );
    b.printRawRow( 0b00000011, 43, outColor, 0xFFFFFF );
    b.printRawRow( 0b00000011, 44, outColor, 0xFFFFFF );
    b.printRawRow( 0b00000011, 46, inColor, 0xFFFFFF );
    b.print( "7", sfOptionColors[ 6 ], 0xFFFFFF, 4, 1, -1 );
    b.printRawRow( 0b00000011, 50, outColor, 0xFFFFFF );
    b.printRawRow( 0b00000011, 51, outColor, 0xFFFFFF );
    b.printRawRow( 0b00000011, 53, inColor, 0xFFFFFF );

    b.print( "8", sfOptionColors[ 7 ], 0xFFFFFF, 6, 1, -2 );
    b.printRawRow( 0b00000011, 57, outColor, 0xFFFFFF );
    b.printRawRow( 0b00000011, 58, outColor, 0xFFFFFF );

    int selected = -1;
    // delayWithButton(300);
    //  return 0;
    int outIn = 2;
    // Loop until GPIO selected or button pressed to exit
    // Use state-based check - doesn't consume events
    while ( selected == -1 && checkProbeButtonState( ) == 0 ) {
        int reading = justReadProbe( );
        if ( reading != -1 ) {
            switch ( reading ) {
            case 3 ... 8: {

                selected = RP_GPIO_1;
                function = RP_GPIO_1;
                if ( reading >= 2 && reading <= 4 ) {
                    outIn = 1;
                } else if ( reading >= 6 && reading <= 8 ) {
                    outIn = 0;
                } else {
                    outIn = 2;
                }

                break;
            }
            case 10 ... 15: {
                selected = RP_GPIO_2;
                function = RP_GPIO_2;
                if ( reading >= 10 && reading <= 12 ) {
                    outIn = 1;
                } else if ( reading >= 14 && reading <= 15 ) {
                    outIn = 0;
                } else {
                    outIn = 2;
                }
                break;
            }
            case 17 ... 22: {
                selected = RP_GPIO_3;
                function = RP_GPIO_3;
                if ( reading >= 17 && reading <= 19 ) {
                    outIn = 1;
                } else if ( reading >= 21 && reading <= 22 ) {
                    outIn = 0;
                } else {
                    outIn = 2;
                }
                break;
            }
            case 24 ... 29: {
                selected = RP_GPIO_4;
                function = RP_GPIO_4;
                if ( reading >= 24 && reading <= 26 ) {
                    outIn = 1;
                } else if ( reading >= 28 && reading <= 29 ) {
                    outIn = 0;
                } else {
                    outIn = 2;
                }
                break;
            }
            case 33 ... 38: {
                selected = RP_GPIO_5;
                function = RP_GPIO_5;
                if ( reading >= 33 && reading <= 35 ) {
                    outIn = 1;
                } else if ( reading >= 37 && reading <= 38 ) {
                    outIn = 0;
                } else {
                    outIn = 2;
                }
                break;
            }
            case 40 ... 45: {
                selected = RP_GPIO_6;
                function = RP_GPIO_6;
                if ( reading >= 40 && reading <= 42 ) {
                    outIn = 1;
                } else if ( reading >= 44 && reading <= 45 ) {
                    outIn = 0;
                } else {
                    outIn = 2;
                }
                break;
            }
            case 47 ... 52: {
                selected = RP_GPIO_7;
                function = RP_GPIO_7;
                if ( reading >= 47 && reading <= 49 ) {
                    outIn = 1;
                } else if ( reading >= 51 && reading <= 52 ) {
                    outIn = 0;
                } else {
                    outIn = 2;
                }
                break;
            }
            case 54 ... 59: {
                selected = RP_GPIO_8;
                function = RP_GPIO_8;
                if ( reading >= 54 && reading <= 56 ) {
                    outIn = 1;
                } else if ( reading >= 58 && reading <= 59 ) {
                    outIn = 0;
                } else {
                    outIn = 2;
                }
                break;
            }
            }
        }
    }

    if ( function == -1 ) {
        return function;
    }
    if ( selected == -1 ) {
        return function;
    }
    if ( skipInputOutput == 0 && connectOrClearProbe == 1 ) {

        int gpioChosen = -1;

        for ( int i = 0; i < 10; i++ ) {
            if ( gpioDef[ i ][ 1 ] == function ) {
                gpioChosen = gpioDef[ i ][ 2 ];
                break;
            }
        }
        // Serial.print("gpioChosen (chooseGPIO): ");
        // Serial.println(gpioChosen);
        // Serial.flush();
        // switch (function) {
        //   case RP_GPIO_1 ... RP_GPIO_8: {
        //   gpioChosen = function - RP_GPIO_1 + 1;
        //   break;
        //   }
        // case 122 ... 125: {
        // gpioChosen = function - 117;
        // break;
        // }
        //}

        if ( outIn == 2 ) {
            chooseGPIOinputOutput( gpioChosen );
        } else if ( outIn == 1 ) {
            gpioState[ gpioDef[ gpioChosen ][ 2 ] ] = 0;
            // if (globalState.config.gpioDirection[gpioChosen - 1] == 0) {
            globalState.config.gpioDirection[ gpioChosen ] = 1;
            updateStateFromGPIOConfig( );
            // gpioState[gpioChosen] = 4;
            // updateGPIOConfigFromState();
            // configChanged = true;
            // printGPIOState();
            //  }
        } else if ( outIn == 0 ) {
            gpioState[ gpioDef[ gpioChosen ][ 2 ] ] = 4;
            // if (globalState.config.gpioDirection[gpioChosen - 1] == 1) {
            globalState.config.gpioDirection[ gpioChosen ] = 0;
            updateStateFromGPIOConfig( );
            // gpioState[gpioChosen] = 0;
            // updateGPIOConfigFromState();
            // configChanged = true;
            // printGPIOState();
            //}
        }

        // Serial.print("gpioChosen (chooseGPIO): ");
        // Serial.print(gpioChosen);
        // Serial.print(" outIn: ");
        // Serial.println(outIn);
        // Serial.flush();
        clearLEDsExceptRails( );
        // printConfigSectionToSerial(7);
    }
    // clearLEDsExceptRails();
    //  showNets();

    showLEDsCore2 = -1;
    // updateGPIOConfigFromState();

    return function;
}

float Probing::voltageSelect( int fiveOrEight ) {
    float voltageProbe = 0.0;
    uint32_t color = 0x000000;

    // fiveOrEight = 8; // they're both 8v now
    if ( fiveOrEight == 5 && false ) {

        b.clear( );
        clearLEDsExceptRails( );

        uint8_t step = 0b0000000;
        for ( int i = 31; i <= 60; i++ ) {
            if ( ( i - 1 ) % 6 == 0 ) {
                step = step << 1;
                step = step | 0b00000001;
            }

            b.printRawRow( step, i - 1, logoColors8vSelect[ ( i - 31 ) * 2 ], 0xffffff );
        }
        // b.print("Set", scaleDownBrightness(rawOtherColors[9], 4, 22),
        //         0xFFFFFF, 1, 0, 3);
        b.print( "Set", scaleDownBrightness( rawOtherColors[ 9 ], 4, 22 ), 0xFFFFFF, 1,
                 0, 3 );
        b.print( "0v", sfOptionColors[ 7 ], 0xFFFFFF, 0, 0, -2 );
        b.print( "5v", sfOptionColors[ 7 ], 0xFFFFFF, 5, 0, 1 );
        int vSelected = -1;
        int encoderReadingPos = 45;
        rotaryDivider = 4;
        while ( vSelected == -1 ) {
            jOS.serviceCritical( );
            int reading = justReadProbe( );
            // rotaryEncoderStuff();
            int encodeEdit = 0;
            if ( encoderDirectionState == UP || reading == -19 ) {
                encoderDirectionState = NONE;
                voltageProbe = voltageProbe + 0.1;
                encodeEdit = 1;
                // Serial.println(reading);

            } else if ( encoderDirectionState == DOWN || reading == -17 ) {
                encoderDirectionState = NONE;
                voltageProbe = voltageProbe - 0.1;

                encodeEdit = 1;
                // Serial.println(voltageProbe);

            } else if ( encoderButtonState == PRESSED &&
                            lastButtonEncoderState == IDLE ||
                        reading == -10 ) {
                encodeEdit = 1;
                encoderButtonState = IDLE;
                vSelected = 10;
            }
            if ( voltageProbe < 0.0 ) {
                voltageProbe = 0.0;
            } else if ( voltageProbe > 5.0 ) {
                voltageProbe = 5.0;
            }
            // Serial.println(reading);
            if ( reading > 0 && reading >= 31 && reading <= 60 || encodeEdit == 1 ) {
                //
                b.clear( 1 );

                char voltageString[ 7 ] = " 0.0 V";

                if ( voltageProbe < 0.0 ) {
                    voltageProbe = 0.0;
                } else if ( voltageProbe > 5.0 ) {
                    voltageProbe = 5.0;
                }

                if ( encodeEdit == 0 ) {
                    voltageProbe = ( reading - 31 ) * ( 5.0 / 29 );

                } else {
                    reading = 31 + ( voltageProbe + 8.0 ) * ( 29.0 / 16.0 );
                }
                // Serial.println(voltageProbe);
                color = logoColors8vSelect[ ( reading - 31 ) * 2 ];

                snprintf( voltageString, 7, "%0.1f v", voltageProbe );
                b.print( voltageString, color, 0xFFFFFF, 0, 1, 3 );
                showLEDsCore2 = -2;
                delay( 10 );
            }
            // Check button state to exit voltage selection (state-based, doesn't consume event)
            if ( checkProbeButtonState( ) > 0 || vSelected == 10 ) {
                // Serial.println("button\n\r");

                rawSpecialNetColors[ 4 ] = color;
                rgbColor rg = unpackRgb( color );
                specialNetColors[ 4 ].r = rg.r;
                specialNetColors[ 4 ].g = rg.g;
                specialNetColors[ 4 ].b = rg.b;
                b.clear( );
                // clearLEDsExceptRails();
                // showLEDsCore2 = 1;
                if ( vSelected != 10 ) {
                    vSelected = 1;
                } else {
                    vSelected = 10;
                    Serial.println( "encoder button\n\r" );
                    delay( 500 );
                }
                // if (checkProbeButtonState() == 2) {
                //   vSelected = 10;
                // }
                vSelected = 1;
                probeButton.clearButtonState( );

                return voltageProbe;
                showLEDsCore2 = -1;
                break;
            }
        }

    } else if ( fiveOrEight == 8 || true ) { // they're both 8v now
        b.clear( );
        clearLEDsExceptRails( );

        uint8_t step = 0b00011111;
        for ( int i = 31; i <= 60; i++ ) {
            if ( ( i - 1 ) % 3 == 0 && i < 46 && i > 32 ) {
                step = step >> 1;
                step = step & 0b01111111;

            } else if ( ( i ) % 3 == 1 && i > 46 ) {
                step = step << 1;
                step = step | 0b00000001;
            }

            b.printRawRow( step, i - 1, logoColors8vSelect[ ( i - 31 ) * 2 ], 0xffffff );
        }
        // b.print("Set", scaleDownBrightness(rawOtherColors[9], 4, 22),
        //         0xFFFFFF, 1, 0, 3);
        b.print( "-8v", sfOptionColors[ 0 ], 0xFFFFFF, 0, 0, -2 );
        b.print( "+8v", sfOptionColors[ 1 ], 0xFFFFFF, 4, 0, 1 );
        int vSelected = -1;
        int encoderReadingPos = 45;
        rotaryDivider = 4;

        float lastVoltageProbe = -10.0;

        while ( vSelected == -1 ) {

            jOS.serviceCritical( );
            int reading = justReadProbe( );
            rotaryEncoderStuff( );

            int encodeEdit = 0;
            if ( encoderDirectionState == UP || reading == -19 ) {
                encoderDirectionState = NONE;
                voltageProbe = voltageProbe + 0.1;
                encodeEdit = 1;
                // Serial.println(reading);

            } else if ( encoderDirectionState == DOWN || reading == -17 ) {
                encoderDirectionState = NONE;
                voltageProbe = voltageProbe - 0.1;
                encodeEdit = 1;
                // Serial.println(voltageProbe);

            } else if ( encoderButtonState == PRESSED &&
                            lastButtonEncoderState == IDLE ||
                        reading == -10 ) {
                encodeEdit = 1;
                encoderButtonState = IDLE;
                vSelected = 10;
            }
            // Serial.println(reading);
            if ( reading > 0 && reading >= 31 && reading <= 60 || encodeEdit == 1 ) {
                //
                b.clear( 1 );

                char voltageString[ 7 ] = " 0.0 V";

                if ( voltageProbe < -8.0 ) {
                    voltageProbe = -8.0;
                } else if ( voltageProbe > 8.0 ) {
                    voltageProbe = 8.0;
                }

                if ( encodeEdit == 0 ) {
                    voltageProbe = ( reading - 31 ) * ( 16.0 / 29 );
                    voltageProbe = voltageProbe - 8.0;
                    if ( voltageProbe < 0.4 && voltageProbe > -0.4 ) {
                        voltageProbe = 0.0;
                    }
                } else {
                    reading = 31 + ( voltageProbe + 8.0 ) * ( 29.0 / 16.0 );
                }
                //

                color = logoColors8vSelect[ ( reading - 31 ) * 2 ];

                snprintf( voltageString, 7, "%0.1f v", voltageProbe );
                b.print( voltageString, color, 0xFFFFFF, 0, 1, 3 );
                showLEDsCore2 = 2;
                Serial.print( "\r                                           \r" );
                Serial.print( "DAC " );
                Serial.print( fiveOrEight ? "1:  " : "0:  " );
                Serial.print( voltageProbe, 1 );
                Serial.print( " V" );
                // delay(10);
            }

            // Check button state to exit voltage selection (state-based, doesn't consume event)
            if ( checkProbeButtonState( ) > 0 || vSelected == 10 ) {
                Serial.println( " " );

                rawSpecialNetColors[ 4 ] = color;
                rgbColor rg = unpackRgb( color );
                specialNetColors[ 4 ].r = rg.r;
                specialNetColors[ 4 ].g = rg.g;
                specialNetColors[ 4 ].b = rg.b;
                b.clear( );
                // clearLEDsExceptRails();
                // showLEDsCore2 = 1;
                if ( vSelected != 10 ) {
                    vSelected = 1;
                } else {
                    vSelected = 10;
                    // Serial.println("encoder button\n\r");
                    // delay(500);
                }
                vSelected = 1;
                showLEDsCore2 = -1;
                probeButton.clearButtonState( );
                blockProbeButton = 2000;
                blockProbeButtonTimer = millis( );
                return voltageProbe;
                break;
            }
        }
    }

    blockProbeButton = 2000;
    blockProbeButtonTimer = millis( );
    // Serial.println(" ");
    return 0.0;
}

int Probing::checkSwitchPosition( ) { // 0 = measure, 1 = select

    // Debounce/glitch filter and timing: only sample at a fixed interval.
    static bool have_previous_read = false;
    static float previous_current_mA[ 5 ] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    static int last_candidate_position = -1; // -1 unknown, 0 measure, 1 select
    static int stable_read_count = 0;        // consecutive close readings on same side of threshold
    static unsigned long last_check_millis = 0;

    float tolerance = 0.25;

    // Use the global interval if available; otherwise default to 50ms.
    // unsigned long switchPositionCheckInterval = 200;
    unsigned long interval_ms = 1500; // switchPositionCheckInterval;

    if ( checkingButton == 1 ) {
        Serial.println( "checkingButton" );
        return switchPosition;
    }

    // Timing gate: exit early if interval hasn't elapsed.
    unsigned long now_ms = millis( );
    if ( ( now_ms - last_check_millis ) < interval_ms ) {
        return switchPosition;
    }
    last_check_millis = now_ms;

    checkingButton = 0;
    // digitalWrite(10, LOW);

    if ( probePowerDAC == 0 ) {
        setDac0voltage( 3.33, 0, 0, false );
    } else if ( probePowerDAC == 1 ) {
        setDac1voltage( 3.33, 0, 0, false );
    }

    float current_mA = checkProbeCurrent( ); // - currentReadingOffset1_mA;

    // HYSTERESIS LOGIC to prevent oscillation:
    // Use different thresholds depending on current state to create a "dead zone"
    //
    // State transitions:
    //   MEASURE -> SELECT: only when current > HIGH threshold (0.90 mA)
    //   SELECT -> MEASURE: only when current < LOW threshold (0.70 mA)
    //
    // This prevents oscillation because changing the LED mode affects current draw,
    // but the new current will still be within the hysteresis band so no state change occurs.

    // Serial.print("Switch position (before): ");
    // Serial.print(switchPosition);
    // Serial.print("  Current: ");
    // Serial.println(current_mA);

    if ( switchPosition == 0 ) {
        // Currently in MEASURE mode - only switch to SELECT if current exceeds HIGH threshold
        if ( current_mA > jumperlessConfig.calibration.probe_switch_threshold_high ) {
            switchPosition = 1;
            //  Serial.println("Switching to SELECT mode (HIGH threshold exceeded)");
        }
    } else {
        // Currently in SELECT mode - only switch to MEASURE if current falls below LOW threshold
        if ( current_mA < jumperlessConfig.calibration.probe_switch_threshold_low ) {
            switchPosition = 0;
            // Serial.println("Switching to MEASURE mode (LOW threshold crossed)");
        }
    }

    // Serial.print("Switch position (after): ");
    // Serial.println(switchPosition);

    if ( switchPosition == 0 ) {
        showProbeLEDs = 3; // measure
    } else {
        showProbeLEDs = 4; // select idle
    }

    // for (int i = 0; i < 5; i++) {
    //   Serial.print(previous_current_mA[i]);
    //   Serial.print(" ");
    // }
    // Serial.println();
    Serial.flush( );

    return switchPosition;
}

float Probing::checkProbeCurrent( void ) {
    // showProbeLEDs = 10;
    // probeLEDs.setPixelColor( 0, 0x010101 );
    // probeLEDs.show( );
    int bs = 0;
    int div = 1;

    float lastDac = globalState.power.dac0;

    float current = 0.0;

    // Wait for INA219 conversion to complete (~8.5ms with 16 sample averaging)
    // The conversion flag indicates when a new reading is ready
    unsigned long timeout_start = millis( );
    while ( !INA1.getConversionFlag( ) && ( millis( ) - timeout_start < 20 ) ) {
        delayMicroseconds( 100 );
    }

    // Take fewer samples since INA219 is already doing 16x averaging internally
    for ( int i = 0; i < div; i++ ) {
        // Wait for conversion flag before each read
        timeout_start = millis( );
        while ( !INA1.getConversionFlag( ) && ( millis( ) - timeout_start < 20 ) ) {
            delayMicroseconds( 100 );
        }
        current += INA1.getCurrent_mA( );
        // delayMicroseconds( 2000 );  // Allow time for next conversion (~8.5ms needed)
    }
    current = current / (float)div;
    // Serial.print("current (before zero) = ");
    // Serial.println(current);
    // Serial.flush();
    current = current - jumperlessConfig.calibration.probe_current_zero;

    // Serial.print("current (after zero) = ");
    // Serial.println(current);
    // Serial.flush();

    if ( showProbeCurrent == 1 ) {
        Serial.print( "                          \rProbe current: " );
        Serial.print( current );
        Serial.print( " mA" );
        Serial.flush( );

        // for (int i = 0; i < (int)(current*10.0); i++) {
        //   Serial.print("*");
        // }
        // Serial.println();
        // Serial.flush();
    }
    // Serial.println();
    // Serial.flush();
    // if (millis() % 1000 < 10) {
    //   Serial.print("current: ");
    //   Serial.print(current);
    //   Serial.println(" mA\n\r");
    //   Serial.flush();
    //  }

    // for (int i = 1; i < 4; i++) {
    //   Serial.print("timer[");
    //   Serial.print(i);
    //   Serial.print("]: ");

    //     Serial.println(timer[i] - timer[i - 1]);
    //     //Serial.print("\t");
    //   }

    // digitalWrite(10, HIGH);

    return current;
}

float Probing::checkProbeCurrentZero( void ) {
    // return 0.0f;

    showProbeLEDs = 10;
    probeLEDs.setPixelColor( 0, 0x000000 );
    probeLEDs.show( );
    delayMicroseconds( 100 );
    bool bridgeExists = checkIfBridgeExistsLocal( DAC0, -1 );
    if ( bridgeExists == true ) {
        removeBridgeFromState( DAC0, -1, true );
    }
    int div = 8;

    float current = 0.0;
    float currentSum = 0.0;

    // With 16x averaging in the INA219, we can take fewer samples here
    // Wait for first conversion to complete
    unsigned long timeout_start = millis( );
    while ( !INA1.getConversionFlag( ) && ( millis( ) - timeout_start < 20 ) ) {
        delayMicroseconds( 100 );
    }

    for ( int i = 0; i < div; i++ ) {
        // Wait for conversion flag before each read
        timeout_start = millis( );
        while ( !INA1.getConversionFlag( ) && ( millis( ) - timeout_start < 20 ) ) {
            delayMicroseconds( 100 );
        }
        currentSum += INA1.getCurrent_mA( );
        // delayMicroseconds( 2000 );  // Allow time for next conversion
    }

    // Serial.print("currentSum = ");
    // Serial.println(currentSum);

    current = currentSum / (float)div;

    jumperlessConfig.calibration.probe_current_zero = current;

    // Serial.print("Zero calibration current = ");
    // Serial.println(current);

    // saveConfig();

    if ( bridgeExists == true ) {
        addBridgeToState( DAC0, ROUTABLE_BUFFER_IN );
    }

    showProbeLEDs = 4;
    return current;
}

void Probing::routableBufferPower( int offOn, int flash, int force ) {
    int flashOrLocal;

    if ( flash == 1 ) {
        flashOrLocal = 0;
    } else {
        flashOrLocal = 1;
    }
    // Serial.print("probePowerDAC = "); Serial.println(probePowerDAC);
    // Serial.print("offOn = "); Serial.println(offOn);
    // Serial.print("flash = "); Serial.println(flash);
    // Serial.print("flashOrLocal = "); Serial.println(flashOrLocal);
    bool needToRefresh = true;

    bufferPowerConnected = false;

    if ( probePowerDAC == 0 ) {
        if ( checkIfBridgeExistsLocal( ROUTABLE_BUFFER_IN, DAC0 ) == 0 ) {
            // Serial.print("bufferPowerConnected dac 0 = "); Serial.println(bufferPowerConnected);
            bufferPowerConnected = false;
            needToRefresh = true;
        } else if ( getDacVoltage( 0 ) < jumperlessConfig.calibration.measure_mode_output_voltage - 0.02 || getDacVoltage( 0 ) > jumperlessConfig.calibration.measure_mode_output_voltage + 0.02 && offOn == 1 ) {
            // Serial.println("DAC 0 voltage is out of range, setting to 3.30 V");
            // Serial.print("getDacVoltage(0) = ");
            // Serial.println(getDacVoltage(0));
            setDac0voltage( jumperlessConfig.calibration.measure_mode_output_voltage, 1, 0 );
            return;
        } else if ( offOn == 1 ) {
            bufferPowerConnected = true;
            if ( force == 0 ) {
                return;
            }
        }

    } else if ( probePowerDAC == 1 ) {
        if ( checkIfBridgeExistsLocal( ROUTABLE_BUFFER_IN, DAC1 ) == 0 ) {
            //   Serial.print("bufferPowerConnected dac 1 = "); Serial.println(bufferPowerConnected);
            bufferPowerConnected = false;
            needToRefresh = true;
        } else if ( getDacVoltage( 1 ) < 2.9 || getDacVoltage( 1 ) > 3.64 && offOn == 1 ) {
            // Serial.println("DAC 1 voltage is out of range, setting to 3.30 V");
            // Serial.print("getDacVoltage(1) = ");
            // Serial.println(getDacVoltage(1));
            setDac1voltage( jumperlessConfig.calibration.measure_mode_output_voltage, 1, 0 );
            return;
        } else if ( offOn == 1 ) {
            bufferPowerConnected = true;
            if ( force == 0 ) {
                return;
            }
        }
    }
    // Serial.print("bufferPowerConnected = "); Serial.println(bufferPowerConnected);

    if ( offOn == 1 ) {
        // Serial.println("power on\n\r");
        //  delay(10);
        if ( probePowerDAC == 0 ) {
            setDac0voltage( jumperlessConfig.calibration.measure_mode_output_voltage, 0, 0 );
            if ( probePowerDACChanged == true ) {
                removeBridgeFromState( ROUTABLE_BUFFER_IN, DAC1 );
                addBridgeToState( ROUTABLE_BUFFER_IN, DAC0, 1 );
                // State functions already call refresh, no need to set needToRefresh
                needToRefresh = false; // Already refreshed by state functions
            }
        } else if ( probePowerDAC == 1 ) {
            setDac1voltage( jumperlessConfig.calibration.measure_mode_output_voltage, 0, 0 );
            if ( probePowerDACChanged == true ) {
                removeBridgeFromState( ROUTABLE_BUFFER_IN, DAC0 );
                addBridgeToState( ROUTABLE_BUFFER_IN, DAC1, 1 );
                // State functions already call refresh, no need to set needToRefresh
                needToRefresh = false; // Already refreshed by state functions
            }
        }

        // removeBridgeFromNodeFile(DAC0, -1, netSlot, 1);
        //   pinMode(27, OUTPUT);
        //    digitalWrite(27, HIGH);

        // Add buffer power connection to state (RAM-based)
        // No need to distinguish flash vs local - state system handles it
        if ( probePowerDAC == 0 ) {
            if ( bufferPowerConnected == false ) {
                addBridgeToState( ROUTABLE_BUFFER_IN, DAC0, 1 );
                // State function already refreshes, but force if needed
                if ( force == 1 ) {
                    if ( flash == 1 ) {
                        refreshConnections( 0, 0, 0 );
                    } else {
                        refreshLocalConnections( 0, 0, 0 );
                    }
                }
            }
        } else if ( probePowerDAC == 1 ) {
            if ( bufferPowerConnected == false ) {
                addBridgeToState( ROUTABLE_BUFFER_IN, DAC1, 1 );
                // State function already refreshes, but force if needed
                if ( force == 1 ) {
                    if ( flash == 1 ) {
                        refreshConnections( 0, 0, 0 );
                    } else {
                        refreshLocalConnections( 0, 0, 0 );
                    }
                }
            }
        }

        bufferPowerConnected = true;

    } else {

        if ( bufferPowerConnected == true ) {
            if ( checkIfBridgeExistsLocal( ROUTABLE_BUFFER_IN, DAC0 ) == 1 ) {

                if ( probePowerDAC == 0 ) {
                    if ( bufferPowerConnected == true ) {
                        removeBridgeFromState( ROUTABLE_BUFFER_IN, DAC0 );
                        // State function already handles both RAM and refresh
                    }
                } else if ( probePowerDAC == 1 ) {
                    if ( bufferPowerConnected == true ) {
                        removeBridgeFromState( ROUTABLE_BUFFER_IN, DAC1 );
                        // State function already handles both RAM and refresh
                    }
                }

                // if (probePowerDAC == 0) {
                //   setDac0voltage(0.0, 1);
                // } else if (probePowerDAC == 1) {
                //   setDac1voltage(0.0, 1);
                // }

                // Extra refresh to ensure everything is synced
                refreshConnections( 0, 0, 0 );
            }
        }
        bufferPowerConnected = false;
    }

    lastProbePowerDAC = probePowerDAC;
    probePowerDACChanged = false;
}

int probeADCmap[ 102 ];

int nothingTouchedReading = 15;
int mapFrom = 15;
// int calibrateProbe() {
//   /* clang-format off */

//   int probeRowMap[102] = {

//       0,	      1,	      2,	      3,	      4,
//       5,	      6,	      7,	      8, 9,	      10,
//       11,	      12,	      13,	      14,	      15,
//       16,	      17, 18,	      19,	      20,	      21,
//       22,	      23,	      24,	      25,	      26, 27,
//       28,	      29,	      30,	      TOP_RAIL, TOP_RAIL_GND,
//       BOTTOM_RAIL,	      BOTTOM_RAIL_GND, 31,	      32, 33, 34,
//       35,	      36,	      37,	      38,	      39, 40,
//       41,	      42,	      43,	      44,	      45,
//       46,	      47,	      48, 49,	      50,	      51,
//       52,	      53,	      54,	      55,	      56,
//       57, 58,	      59,	      60,	      NANO_D1, NANO_D0,
//       NANO_RESET_1,	      NANO_GND_1,	      NANO_D2,	      NANO_D3,
//       NANO_D4,	      NANO_D5,	      NANO_D6,	      NANO_D7, NANO_D8,
//       NANO_D9,	      NANO_D10,	      NANO_D11,	      NANO_D12,
//       NANO_D13,	      NANO_3V3,	      NANO_AREF,	      NANO_A0,
//       NANO_A1,	      NANO_A2,	      NANO_A3,	      NANO_A4, NANO_A5,
//       NANO_A6,	      NANO_A7,	      NANO_5V,	      NANO_RESET_0,
//       NANO_GND_0,	      NANO_VIN, LOGO_PAD_BOTTOM, LOGO_PAD_TOP,
//       GPIO_PAD, DAC_PAD,	      ADC_PAD,	      BUILDING_PAD_TOP,
//       BUILDING_PAD_BOTTOM,
//   };
//   /* clang-format on */

int nothingTouchedSamples[ 16 ] = { 0 };

int Probing::getNothingTouched( int samples ) {

    startProbe( );
    int rejects = 0;
    int loops = 0;

    for ( int i = 0; i < 16; i++ ) {

        nothingTouchedSamples[ i ] = 0;
    }
    do {

        // samples = 2;

        int sampleAverage = 0;
        rejects = 0;
        nothingTouchedReading = 0;
        for ( int i = 0; i < samples; i++ ) {
            // int reading = readProbeRaw(1);
            int readNoth = readAdc( 5, 8 );
            nothingTouchedSamples[ i ] = readNoth;
            //   delayMicroseconds(50);
            //   Serial.print("nothingTouchedSample ");
            //   Serial.print(i);
            //   Serial.print(": ");
            // Serial.println(readNoth);
        }
        loops++;

        for ( int i = 0; i < samples; i++ ) {

            if ( nothingTouchedSamples[ i ] < 100 ) {
                sampleAverage += nothingTouchedSamples[ i ];
            } else {
                rejects++;
            }
        }
        if ( samples - rejects <= 1 ) {
            Serial.println( "All nothing touched samples rejected, check sense pad "
                            "connections\n\r" );
            nothingTouchedReading = 36;
            return 0;
            break;
        }
        sampleAverage = sampleAverage / ( samples - rejects );
        rejects = 0;

        for ( int i = 0; i < samples; i++ ) {
            if ( abs( nothingTouchedSamples[ i ] - sampleAverage ) < 15 ) {
                nothingTouchedReading += nothingTouchedSamples[ i ];
                //       Serial.print("nothingTouchedSample ");
                //   Serial.print(i);
                //   Serial.print(": ");
                // Serial.println(nothingTouchedSamples[i]);
            } else {
                rejects++;
            }
        }

        nothingTouchedReading = nothingTouchedReading / ( samples - rejects );
        mapFrom = nothingTouchedReading;
        // Serial.print("mapFrom: ");
        // Serial.println(mapFrom);
        jumperlessConfig.calibration.probe_min = mapFrom;

        if ( loops > 5 ) {
            break;
        }

    } while ( ( nothingTouchedReading > 80 || rejects > samples / 2 ) && loops < 4 );
    //  Serial.print("nothingTouchedReading: ");
    //  Serial.println(nothingTouchedReading);
    return nothingTouchedReading;
}

unsigned long doubleTimeout = 0;

unsigned long padTimeout = 0;
int padTimeoutLength = 50;

int state = 0;
int lastPadTouched = 0;
unsigned long padNoTouch = 0;
int lastPadTouchedTime = 0;
int samePadCount = 0;

void Probing::checkPads( void ) {
    // startProbe();
    checkingPads = 1;

    int probeReadings[ 8 ] = { 0 };

    for ( int i = 0; i < 8; i++ ) {
        probeReadings[ i ] = readProbeRaw( 0, 1 );
    }

    int probeReading = 0;
    int numberOfGoodReadings = 0;
    for ( int i = 0; i < 8; i++ ) {
        if ( probeReadings[ i ] > 0 ) {
            probeReading += probeReadings[ i ];
            numberOfGoodReadings++;
        }
    }
    // Serial.print("probeReadings: ");
    // for (int i = 0; i < 4; i++) {
    //   Serial.print(probeReadings[i]);
    //   Serial.print(" ");
    // }
    // Serial.println();
    // Serial.flush();
    probeReading = probeReading / numberOfGoodReadings;

    // lastReadRaw = -1;
    if ( probeReading <= jumperlessConfig.calibration.probe_min ) {
        checkingPads = 0;
        return;
    }

    padNoTouch = 0;

    // probeReading = probeRowMap[map(probeReading, 30, 4050, 101, 0)];
    probeReading = probeRowMap[ map( probeReading, jumperlessConfig.calibration.probe_min, jumperlessConfig.calibration.probe_max, 101, 0 ) ];
    // stopProbe();

    if ( probeReading != lastPadTouched ) {
        lastPadTouchedTime = millis( );
        samePadCount = 0;
    } else {
        samePadCount++;
    }
    lastPadTouched = probeReading;
    // Serial.print("probeReading: ");
    // Serial.println(probeReading);
    // Serial.flush();
    // if (probeReading < LOGO_PAD_TOP || probeReading > BUILDING_PAD_BOTTOM) {
    //   padTimeout = millis();
    //   lastReadRaw = 0;
    //   checkingPads = 0;
    //   return;
    // }

    padTimeout = millis( );
    // Serial.print("probeReading: ");
    // Serial.println(probeReading);
    int foundGpio = 0;
    int foundAdc = 0;
    int foundDac = 0;
    // inPadMenu = 1;

    Serial.print( "\r                                 \r" );

    switch ( probeReading ) {
    case LOGO_PAD_TOP:
        Serial.print( "Top guy" );
        clearColorOverrides( 1, 1, 0 );
        setLogoOverride( LOGO_TOP, -2 );
        break;
    case LOGO_PAD_BOTTOM:
        Serial.print( "Bottom guy" );
        clearColorOverrides( 1, 1, 0 );
        setLogoOverride( LOGO_BOTTOM, -2 );
        break;
    case ADC_PAD:
        Serial.print( "ADC pad" );
        clearColorOverrides( 1, 1, 0 );
        setLogoOverride( ADC_0, -2 );
        setLogoOverride( ADC_1, -2 );
        break;
    case DAC_PAD:
        Serial.print( "DAC pad" );
        clearColorOverrides( 1, 1, 0 );
        setLogoOverride( DAC_0, -2 );
        setLogoOverride( DAC_1, -2 );
        break;
    case GPIO_PAD:
        Serial.print( "GPIO pad" );
        clearColorOverrides( 1, 1, 0 );
        setLogoOverride( GPIO_0, -2 );
        setLogoOverride( GPIO_1, -2 );
        break;
    case BUILDING_PAD_TOP:
        Serial.print( "Building top" );
        clearColorOverrides( 1, 1, 0 ); //! highlighted net
        if ( brightenedNet != -1 ) {
            hsvColor hsv = RgbToHsv( netColors[ brightenedNet ] );
            changedNetColors[ brightenedNet ].color = colorPicker( hsv.h, jumperlessConfig.display.led_brightness );
            changedNetColors[ brightenedNet ].node1 = brightenedNode;
            changedNetColors[ brightenedNet ].fromBridge = false; // User manually set this color
            netColors[ brightenedNet ] = unpackRgb( changedNetColors[ brightenedNet ].color );
            Serial.print( "changedNetColors[" );
            Serial.print( brightenedNet );
            Serial.print( "]: " );
            Serial.printf( "%06x\n", changedNetColors[ brightenedNet ].color );
            clearHighlighting( );
            checkChangedNetColors( -1 );
            // Note: No need to call assignNetColors() here - core 2's showNets() recomputes colors every frame
            showLEDsCore2 = 1; // Trigger LED update on core 2
            // saveChangedNetColorsToFile( netSlot, 0 ); // DEPRECATED: Colors now saved via YAML state

        } else {
            // colorPicker(45, jumperlessConfig.display.led_brightness);
        }
        break;
    case BUILDING_PAD_BOTTOM:
        Serial.print( "Building bottom" );
        clearColorOverrides( 1, 1, 0 );
        if ( brightenedNet != -1 ) {
            hsvColor hsv = RgbToHsv( netColors[ brightenedNet ] );
            changedNetColors[ brightenedNet ].color = colorPicker( hsv.h, jumperlessConfig.display.led_brightness );
            changedNetColors[ brightenedNet ].node1 = brightenedNode;
            changedNetColors[ brightenedNet ].fromBridge = false; // User manually set this color
            netColors[ brightenedNet ] = unpackRgb( changedNetColors[ brightenedNet ].color );
            Serial.print( "changedNetColors[" );
            Serial.print( brightenedNet );
            Serial.print( "]: " );
            Serial.printf( "%06x\n", changedNetColors[ brightenedNet ].color );
            clearHighlighting( );
            checkChangedNetColors( -1 );
            // Note: No need to call assignNetColors() here - core 2's showNets() recomputes colors every frame
            showLEDsCore2 = 1; // Trigger LED update on core 2
            // saveChangedNetColorsToFile( netSlot, 0 ); // DEPRECATED: Colors now saved via YAML state

        } else {
            // colorPicker(225, jumperlessConfig.display.led_brightness);
        }
        break;
    default:
        // clearColorOverrides(1, 1, 0);
        break;
    }

    // Serial.print("\t");
    // Serial.print(samePadCount);
    // Serial.print("\t");
    // Serial.print(millis() - lastPadTouchedTime);
    Serial.flush( );

    // Serial.println();

    checkingPads = 0;
}

float longRunningAverage = 0.0;
int longRunningAverageDamping = 10;

int Probing::readProbeRaw( int readNothingTouched, bool allowDuplicates ) {
    // nothingTouchedReading = 165;
    // lastReadRaw = 0;

    int numberOfReads = 4;
    int lowReads = 0;

    int measurements[ 16 ] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    // digitalWrite(PROBE_PIN, HIGH);
    if ( connectOrClearProbe == 1 ) {

        for ( int i = 0; i < numberOfReads; i++ ) {
            measurements[ i ] = readAdc( 5, 12 );
            if ( measurements[ i ] < 300 && i < 4 ) {
                lowReads++;
            }
            if ( lowReads > 2 ) {
                // numberOfReads = 8;
            }
            delayMicroseconds( 5 );
        }
        // Serial.print("connect: ");
    } else if ( checkingPads == 1 ) {
        for ( int i = 0; i < numberOfReads; i++ ) {
            measurements[ i ] = readAdc( 5, 8 );
            if ( measurements[ i ] < 300 && i < 4 ) {
                lowReads++;
            }
            if ( lowReads > 2 ) {
                // numberOfReads = 8;
            }
            delayMicroseconds( 5 );
        }
        // Serial.print("Pads: ");

    } else {
        for ( int i = 0; i < numberOfReads; i++ ) {
            measurements[ i ] = readAdc( 5, 6 );
            if ( measurements[ i ] < 300 && i < 4 ) {
                lowReads++;
            }
            if ( lowReads > 2 ) {
                // numberOfReads = 8;
            }
            delayMicroseconds( 5 );
        }
    }

    int sum = 0;
    int maxVariance = 0;
    int variance = 0;
    for ( int i = 0; i < numberOfReads; i++ ) {
        sum += measurements[ i ];
        if ( i < 3 ) {
            variance = abs( measurements[ i ] - measurements[ i + 1 ] );
            if ( variance > maxVariance ) {
                maxVariance = variance;
            }
        }
    }
    int average = sum / numberOfReads;
    // Serial.print("average ");
    // Serial.println(average);
    int rowProbed = -1;
    // if (average < 90 && abs(average - nothingTouchedReading) > 10) {
    // Serial.print("var: ");
    // Serial.print(maxVariance);
    // Serial.print(" average: ");
    // Serial.println(average);
    // }

    // if (allowDuplicates){
    //   Serial.print("allowDuplicates: ");
    //   Serial.println(average);
    //   return average;
    //   //lastReadRaw = 4096;
    // }

    if ( maxVariance <= 4 && ( ( abs( average - lastReadRaw ) > 5 ) || checkingPads == 1 ) && ( average >= MINIMUM_PROBE_READING ) ) {

        // if (checkingPads != 1) {
        lastReadRaw = average;
        // }
        //      Serial.println("  ");

        // Serial.print("average: ");
        // Serial.print(average);
        // Serial.print(" numberOfReads: ");
        // Serial.println(numberOfReads);
        // Serial.flush();
        return average;

    } else {

        if ( ( abs( average - lastReadRaw ) < 5 ) && allowDuplicates && ( average >= MINIMUM_PROBE_READING ) ) {
            // Serial.print("allowDuplicates: ");
            // Serial.println(average);
            return average;
        }

        // Serial.print("nothingTouchedReading: ");
        // Serial.println(nothingTouchedReading);
        // longRunningAverageCount = 10;
        // longRunningAverage += ((float)average - longRunningAverage) / longRunningAverageCount;
        // Serial.print("longRunningAverage: ");
        // Serial.println(longRunningAverage);

        // Serial.print("average ");
        // Serial.println(average);
        return -1;
    }
}

int convertPadsToRows( int pad ) {
    int row = pad;
    if ( pad == LOGO_PAD_BOTTOM ) {
        row = RP_UART_TX;
    } else if ( pad == LOGO_PAD_TOP ) {
        row = RP_UART_RX;
    } else if ( pad == GPIO_PAD ) {
        row = GPIO_PAD;
    } else if ( pad == DAC_PAD ) {
        row = DAC_PAD;
    } else if ( pad == ADC_PAD ) {
        row = ADC_PAD;
    } else if ( pad == BUILDING_PAD_TOP ) {
        row = ISENSE_PLUS;
    } else if ( pad == BUILDING_PAD_BOTTOM ) {
        row = ISENSE_MINUS;
    }
    return row;
}
unsigned long lastProbeTime = millis( );
int lastProbeRead = 0;
int lastRowProbed = -1;

unsigned long lastDuplicateTime = millis( );
int lastDuplicateRead = 0;

int Probing::justReadProbe( bool allowDuplicates, int rawPad ) {

    // Check if probing is blocked and if the block timer has expired
    if ( blockProbing > 0 && ( millis( ) - blockProbingTimer < blockProbing ) ) {
        return -1; // Still blocked
    }
    // Block expired, clear it
    if ( blockProbing > 0 ) {
        blockProbing = 0;
    }

    int probeRead = readProbeRaw( 0, allowDuplicates );

    if ( probeRead <= 0 ) {

        return -1;
    }
    //   Serial.print("probeRead: ");
    // Serial.println(probeRead);

    // int rowProbed = map(probeRead, mapFrom, 4045, 101, 0);
    int rowProbed = map( probeRead, jumperlessConfig.calibration.probe_min, jumperlessConfig.calibration.probe_max, 101, 0 );
    // Serial.print("rowProbed: ");
    // Serial.println(rowProbed);

    if ( rowProbed <= 0 || rowProbed > sizeof( probeRowMap ) ) {
        if ( debugProbing == 1 ) {
            Serial.print( "out of bounds of probeRowMap[" );
            Serial.println( rowProbed );
        }
        return -1;
    }

    if ( allowDuplicates ) {
        // Check if the mapped row is the same, not just the raw reading
        if ( probeRowMapByPad[ rowProbed ] == lastRowProbed ) {
            // If the timer hasn't elapsed, reject the duplicate reading
            if ( millis( ) - lastDuplicateTime < 500 ) {
                if ( debugProbing == 1 ) {
                    Serial.print( "Rejected duplicate row: " );
                    Serial.println( probeRowMap[ rowProbed ] );
                }
                return -1;
            } else {
                // Timer has elapsed, accept the reading and reset timer
                lastDuplicateTime = millis( );
                lastProbeRead = probeRead;
                // Return the same reading now that timer is complete
                if ( rawPad == 1 ) {
                    return probeRowMapByPad[ rowProbed ];
                } else {
                    return probeRowMap[ rowProbed ];
                }
            }
        } else {
            // Different row, reset timer and update last values
            lastDuplicateTime = millis( );
            lastProbeRead = probeRead;
            lastRowProbed = probeRowMapByPad[ rowProbed ];
            if ( rawPad == 1 ) {
                return probeRowMapByPad[ rowProbed ];
            } else {
                return probeRowMap[ rowProbed ];
            }
        }
    }

    // For non-allowDuplicates case
    lastProbeRead = probeRead;
    lastRowProbed = probeRowMapByPad[ rowProbed ];
    if ( rawPad == 1 ) {
        return probeRowMapByPad[ rowProbed ];
    } else {
        return probeRowMap[ rowProbed ];
    }
}
/// @brief returns the row probed plus checks for button presses, or -1 if nothing
/// @return -16 connect, -18 remove, -19 encoder up, -17 encoder down, -10 encoder pressed
int Probing::readProbe( ) {
    int found = -1;
    // connectedRows[0] = -1;
    unsigned long buttonCheck = 0;
    // if (checkProbeButton() == 1) {
    //   return -18;
    // }
    // Check if probing is blocked and if the block timer has expired
    if ( blockProbing > 0 && ( millis( ) - blockProbingTimer < blockProbing ) ) {
        return -1; // Still blocked
    }
    // Block expired, clear it
    if ( blockProbing > 0 ) {
        blockProbing = 0;
    }

    int probeRead = readProbeRaw( );
    // delay(100);
    // Serial.println(probeRead);
    // Serial.println(debugLEDs);
    while ( probeRead <= 0 ) {
        /// delay(50);
        // return -1;
        // Serial.println(debugLEDs);

        probeRead = readProbeRaw( );
        // rotaryEncoderStuff();

        // Return encoder state for handling in main probe loop
        if ( encoderDirectionState != NONE ) {
            if ( encoderDirectionState == UP ) {
                // Serial.println("encoder up");
                // Serial.flush();
                return -19;
            } else if ( encoderDirectionState == DOWN ) {
                // Serial.println("encoder down");
                // Serial.flush();
                return -17;
            }
        } else if ( encoderButtonState == PRESSED &&
                    lastButtonEncoderState == IDLE ) {
            // Serial.println("encoder pressed");
            // Serial.flush();
            return -10;
        }

        // buttonCheck = millis();
        // Check button state (blocking is handled by ProbeButton service)
        int buttonState = checkProbeButton( );
        if ( buttonState == 1 ) {
            return -18;
        } else if ( buttonState == 2 ) {
            return -16;
        }

        // delayMicroseconds(200);

        if ( millis( ) - doubleTimeout > 1000 ) {
            doubleTimeout = millis( );
            lastReadRaw = 0;
        }

        if ( millis( ) - lastProbeTime > 50 ) {
            lastProbeTime = millis( );
            // Serial.println("probe timeout");
            return -1;
        }
    }
    doubleTimeout = millis( );
    if ( debugProbing == 1 ) {
        // Serial.print("probeRead: ");
        // Serial.println(probeRead);
    }
    if ( probeRead == -1 ) {
        return -1;
    }

    // int padRead = 0;
    if ( probeRead < 300 ) { // take an average if it's a logo pad
        int probeReadings[ 4 ] = { 0 };
        for ( int i = 0; i < 4; i++ ) {
            probeReadings[ i ] = readProbeRaw( 0, 1 );
        }
        int probeReading = 0;
        int numberOfGoodReadings = 0;
        for ( int i = 0; i < 4; i++ ) {
            if ( probeReadings[ i ] > 0 ) {
                probeReading += probeReadings[ i ];
                numberOfGoodReadings++;
            }
        }
        probeRead = probeReading / numberOfGoodReadings;
        // padRead = 1;
        // Serial.print("probeRead: ");
        // Serial.println(probeRead);
        // Serial.flush();
    }

    int rowProbed = map( probeRead, jumperlessConfig.calibration.probe_min, jumperlessConfig.calibration.probe_max, 101, 0 );
    // Serial.print("\n\n\rprobeRead: ");
    // Serial.println(probeRead);
    // rowProbed = convertPadsToRows( rowProbed );

    if ( rowProbed <= 0 || rowProbed >= sizeof( probeRowMap ) ) {
        if ( debugProbing == 1 ) {
            // Serial.print("out of bounds of probeRowMap[");
            // Serial.println(rowProbed);
        }
        return -1;
    }
    if ( debugProbing == 1 ) {
        Serial.print( "probeRowMap[" );
        Serial.print( rowProbed );
        Serial.print( "]: " );
        Serial.println( probeRowMap[ rowProbed ] );
    }

    rowProbed = selectSFprobeMenu( probeRowMap[ rowProbed ] );
    if ( debugProbing == 1 ) {
        Serial.print( "rowProbed: " );
        Serial.println( rowProbed );
    }
    connectedRows[ 0 ] = rowProbed;
    connectedRowsIndex = 1;

    // Serial.print("maxVariance: ");
    // Serial.println(maxVariance);
    return rowProbed;
    // return probeRowMap[rowProbed];
}

int hsvProbe = 0;
int hsvProbe2 = 0;
unsigned long probeRainbowTimer = 0;

void Probing::probeLEDhandler( void ) {

    // core2busy = true;
    //  pinMode(2, OUTPUT);
    //  pinMode(9, INPUT);
    showingProbeLEDs = 1;
    // if (showProbeLEDs != 0) {
    //         Serial.print("showProbeLEDs = ");
    // Serial.println(showProbeLEDs);
    // }
    switch ( showProbeLEDs ) {
    case 1:
        if ( connectOrClearProbe == 1 && node1or2 == 1 ) {
            probeLEDs.setPixelColor( 0, 0x0f0fc6 ); // connect bright
        } else {
            probeLEDs.setPixelColor( 0, 0x000032 ); // connect
        }
        // probeLEDs[0].setColorCode(0x000011);
        //  Serial.println(showProbeLEDs);
        //   probeLEDs.show();
        // showProbeLEDs = 0;
        break;
    case 2: {

        // if (connectOrClearProbe == 0) {
        //  Serial.print("removeFade = ");
        //  Serial.println(removeFade);
        switch ( removeFade ) {
        case 0:
            probeLEDs.setPixelColor( 0, 0x280000 ); // remove
            break;
        case 1:
            probeLEDs.setPixelColor( 0, 0x330101 ); // remove
            break;
        case 2:
            probeLEDs.setPixelColor( 0, 0x3c0202 ); // remove
            break;
        case 3:
            probeLEDs.setPixelColor( 0, 0x450303 ); // remove
            break;
        case 4:
            probeLEDs.setPixelColor( 0, 0x4e0404 ); // remove
            break;
        case 5:
            probeLEDs.setPixelColor( 0, 0x570505 ); // remove
            break;
        case 6:
            probeLEDs.setPixelColor( 0, 0x600707 ); // remove
            break;
        case 7:
            probeLEDs.setPixelColor( 0, 0x690909 ); // remove
            break;
        case 8:
            probeLEDs.setPixelColor( 0, 0x820a0a ); // remove
            break;
        case 9:
            probeLEDs.setPixelColor( 0, 0xab1010 ); // remove
            break;
        case 10:
            probeLEDs.setPixelColor( 0, 0xff1a1a ); // remove
            break;
        default:
            probeLEDs.setPixelColor( 0, 0x280000 ); // remove
            break;
        }
        // } else {
        //   probeLEDs.setPixelColor(0, 0x360000); // remove
        // }
        // probeLEDs.setPixelColor(0, 0x360000); // remove
        // probeLEDs[0].setColorCode(0x110000);
        // probeLEDs.show();
        //  Serial.println(showProbeLEDs);
        showProbeLEDs = 0;
        break;
    }
    case 3:
        probeLEDs.setPixelColor( 0, 0x003600 ); // measure
        // probeLEDs[0].setColorCode(0x001100);
        //  probeLEDs.show();
        //  Serial.println(showProbeLEDs);
        break;
    case 4:

        probeLEDs.setPixelColor( 0, 0x170c17 ); // select idle
        // probeLEDs[0].setColorCode(0x110011);
        //  probeLEDs.show();
        //  Serial.println(showProbeLEDs);
        break;
    case 5:
        probeLEDs.setPixelColor( 0, 0x111111 ); // all
        // probeLEDs[0].setColorCode(0x111111);
        //  Serial.println(showProbeLEDs);
        break;
    case 6:
        probeLEDs.setPixelColor( 0, 0x0c190c ); // measure dim
        break;
    case 7: {
        // hsvProbe++;
        if ( hsvProbe > 255 ) {
            hsvProbe = 0;
            hsvProbe2 -= 8;
            if ( hsvProbe2 < 15 ) {
                hsvProbe2 = 255;
            }
        }
        hsvColor probeColor;
        probeColor.h = hsvProbe;
        probeColor.s = hsvProbe2;
        probeColor.v = 25;

        uint32_t colorp = packRgb( HsvToRgb( probeColor ) );
        probeLEDs.setPixelColor( 0, colorp ); // select idle dim
        break;
    }
    case 8:
        probeLEDs.setPixelColor( 0, 0xffffff ); // max
        showProbeLEDs = 9;
        while ( showProbeLEDs == 9 ) {
            probeLEDs.show( );
            delayMicroseconds( 100 );
            // Serial.println("max");
        }
        showProbeLEDs = 0;
        // Serial.println("max");
        break;

    case 10:
        probeLEDs.setPixelColor( 0, 0x000000 ); // off
        showProbeLEDs = 0;
        break;
    default:
        break;
    }
    lastProbeLEDs = showProbeLEDs;

    probeLEDs.show( );
    showingProbeLEDs = 0;
}

// highlightNets function moved to Highlighting.cpp

// Double click detector state machine

// C wrapper functions for MicroPython module
extern "C" int jl_probe_button_nonblocking( void ) {
    // checkProbeButton() is already non-blocking, so just call it directly
    // Returns: 0=NONE, 1=REMOVE(rear), 2=CONNECT(front)
    return checkProbeButton( );
}

extern "C" int jl_probe_button_blocking( void ) {
    // Loop until any button is pressed
    // Returns: 1=REMOVE(rear), 2=CONNECT(front) (never returns 0)
    int button_state = 0;
    while ( button_state == 0 ) {
        mp_hal_check_interrupt( );

        // Check if interrupt was requested and return special value
        if ( mp_interrupt_requested ) {
            mp_interrupt_requested = false; // Clear the flag
            return -999;                    // Special return value indicating interrupt
        }

        button_state = checkProbeButton( );
        if ( button_state != 0 ) {
            return button_state;
        }
        delay( 1 ); // Small delay to prevent busy-waiting
    }
    return button_state; // This should never be reached, but just in case
}

//! legacy functions from OG probing

int Probing::selectFromLastFound( void ) {

    rawOtherColors[ 1 ] = 0x0010ff;

    blinkTimer = 0;
    int selected = 0;
    int selectionConfirmed = 0;
    int selected2 = connectedRows[ selected ];
    Serial.print( "\n\r" );
    Serial.print( "      multiple nodes found\n\n\r" );
    Serial.print( "  short press = cycle through nodes\n\r" );
    Serial.print( "  long press  = select\n\r" );

    Serial.print( "\n\r " );
    for ( int i = 0; i < connectedRowsIndex; i++ ) {

        printNodeOrName( connectedRows[ i ] );
        if ( i < connectedRowsIndex - 1 ) {
            Serial.print( ", " );
        }
    }
    Serial.print( "\n\n\r" );
    delay( 10 );

    uint32_t previousColor[ 10 ];

    for ( int i = 0; i < connectedRowsIndex; i++ ) {
        previousColor[ i ] = leds.getPixelColor( nodesToPixelMap[ connectedRows[ i ] ] );
    }
    int lastSelected = -1;

    while ( selectionConfirmed == 0 ) {
        probeTimeout = millis( );
        // if (millis() - blinkTimer > 100)
        // {
        if ( lastSelected != selected && selectionConfirmed == 0 ) {
            for ( int i = 0; i < connectedRowsIndex; i++ ) {
                if ( i == selected ) {
                    leds.setPixelColor( nodesToPixelMap[ connectedRows[ i ] ],
                                        rainbowList[ 1 ][ 0 ], rainbowList[ 1 ][ 1 ],
                                        rainbowList[ 1 ][ 2 ] );
                } else {
                    // uint32_t previousColor =
                    // leds.getPixelColor(nodesToPixelMap[connectedRows[i]]);
                    if ( previousColor[ i ] != 0 ) {
                        int r = ( previousColor[ i ] >> 16 ) & 0xFF;
                        int g = ( previousColor[ i ] >> 8 ) & 0xFF;
                        int b = ( previousColor[ i ] >> 0 ) & 0xFF;
                        leds.setPixelColor( nodesToPixelMap[ connectedRows[ i ] ], ( r / 4 ) + 5,
                                            ( g / 4 ) + 5, ( b / 4 ) + 5 );
                    } else {

                        leds.setPixelColor( nodesToPixelMap[ connectedRows[ i ] ],
                                            rainbowList[ 1 ][ 0 ] / 8, rainbowList[ 1 ][ 1 ] / 8,
                                            rainbowList[ 1 ][ 2 ] / 8 );
                    }
                }
            }
            lastSelected = selected;

            Serial.print( " \r" );
            // Serial.print("");
            printNodeOrName( connectedRows[ selected ] );
            Serial.print( "  " );
        }
        // leds.show();
        showLEDsCore2 = 2;
        blinkTimer = millis( );
        //  }
        delay( 30 );
        int longShort = longShortPress( );
        delay( 5 );
        if ( longShort == 1 ) {
            selectionConfirmed = 1;
            // for (int i = 0; i < connectedRowsIndex; i++)
            // {
            //     if (i == selected)
            //     // if (0)
            //     {
            //         leds.setPixelColor(nodesToPixelMap[connectedRows[i]],
            //         rainbowList[rainbowIndex][0], rainbowList[rainbowIndex][1],
            //         rainbowList[rainbowIndex][2]);
            //     }
            //     else
            //     {
            //         leds.setPixelColor(nodesToPixelMap[connectedRows[i]], 0, 0, 0);
            //     }
            // }
            // showLEDsCore2 = 1;
            // selected = lastFound[node1or2][selected];
            //  clearLastFound();

            // delay(500);
            selected2 = connectedRows[ selected ];
            // return selected2;
            break;
        } else if ( longShort == 0 ) {

            selected++;
            blinkTimer = 0;

            if ( selected >= connectedRowsIndex ) {

                selected = 0;
            }
            // delay(100);
        }
        delay( 15 );
        //  }
        //}

        // showLEDsCore2 = 1;
    }
    selected2 = connectedRows[ selected ];

    for ( int i = 0; i < connectedRowsIndex; i++ ) {
        if ( i == selected ) {
            leds.setPixelColor( nodesToPixelMap[ connectedRows[ selected ] ],
                                rainbowList[ 0 ][ 0 ], rainbowList[ 0 ][ 1 ],
                                rainbowList[ 0 ][ 2 ] );
        } else if ( previousColor[ i ] != 0 ) {

            int r = ( previousColor[ i ] >> 16 ) & 0xFF;
            int g = ( previousColor[ i ] >> 8 ) & 0xFF;
            int b = ( previousColor[ i ] >> 0 ) & 0xFF;
            leds.setPixelColor( nodesToPixelMap[ connectedRows[ i ] ], r, g, b );
        } else {

            leds.setPixelColor( nodesToPixelMap[ connectedRows[ i ] ], 0, 0, 0 );
        }
    }

    // leds.setPixelColor(nodesToPixelMap[selected2], rainbowList[0][0],
    // rainbowList[0][1], rainbowList[0][2]); leds.show(); showLEDsCore2 = 1;
    probeButtonTimer = millis( );
    // connectedRowsIndex = 0;
    // justSelectedConnectedNodes = 1;
    return selected2;
}

int Probing::longShortPress( int pressLength ) {
    // Rewritten to use state-based API (doesn't consume button events)
    // Returns: -1 = no press, 1 = short remove press, 2 = short connect press,
    //          3 = long remove press, 4 = long connect press
return -1;
    unsigned long clickTimer = millis( );

    // Wait for initial button press (check state, don't consume event)
    int initialState = probeButton.getButtonState( );
    if ( initialState == 0 ) {
        return -1; // No button currently pressed
    }

    // Button is pressed - track which button it was
    int whichButton = initialState; // 1=remove, 2=connect

    // Wait to see if it's held for pressLength duration
    while ( millis( ) - clickTimer < pressLength ) {
        int currentState = probeButton.getButtonState( );

        // If button released before timeout, it's a short press
        if ( currentState == 0 ) {
            // Serial.print("Short press detected: ");
            // Serial.println(whichButton);
            return whichButton; // Return 1 or 2 for short press
        }

        delay( 5 );
    }

    // Button held for full duration - it's a long press
    // Return 3 for long remove, 4 for long connect
    int longPressCode = ( whichButton == 1 ) ? 3 : 4;
    // Serial.print("Long press detected: ");
    // Serial.println(longPressCode);

    // Wait for button release to avoid repeated detection
    while ( probeButton.getButtonState( ) != 0 ) {
        delay( 5 );
    }

    return longPressCode;
}

int countLED = 0;
int lastProbeButtonState = 0;

/// @brief Checks for button press EVENTS (event-based, consumes event)
/// Use this when you want to detect a button press once and have it consumed.
/// For use in loops where you continuously check, use checkProbeButtonState() instead.
/// Now delegates to high-frequency ProbeButton service for instant response
/// @return 0 = neither pressed, 1 = remove button, 2 = connect button
int Probing::checkProbeButton( void ) {
    // Check for button press EVENTS, not current state
    // This prevents detecting the same press multiple times
    // Note: getButtonPress() consumes the event, so it only returns non-zero once per press
    int press = probeButton.getButtonPress( );

    // If we got a press event, return it
    if ( press != 0 ) {
        // Serial.println("\n\n\n\rcheckProbeButton press = " + String(press));
        return press;
    }

    // Otherwise check if we're in a blocking period (prevents rapid re-checking)
    if ( blockProbeButton > 0 && ( millis( ) - blockProbeButtonTimer < blockProbeButton ) ) {
        return 0; // Still blocked, no button state
    }

    // No press event and not blocked
    return 0;
}

/// @brief Checks CURRENT button state (state-based, doesn't consume events)
/// Use this in loops where you continuously check button state.
/// This won't consume button press events, allowing them to propagate to outer logic.
/// @return 0 = neither pressed, 1 = remove button, 2 = connect button
int Probing::checkProbeButtonState( void ) {
    // Simply return the current hardware state without consuming events
    // This allows the event to propagate to other parts of the code
    return probeButton.getButtonState( );
}

int Probing::readFloatingOrState( int pin, int rowBeingScanned ) { // this is the old probe reading code
    // return 0;
    enum measuredState state = unknownState;
    // enum measuredState state2 = floating;

    int readingPullup = 0;
    int readingPullup2 = 0;
    int readingPullup3 = 0;

    int readingPulldown = 0;
    int readingPulldown2 = 0;
    int readingPulldown3 = 0;

    // pinMode(pin, INPUT_PULLUP);

    if ( rowBeingScanned != -1 ) {

        analogWrite( PROBE_PIN, 128 );

        while ( 1 ) // this is the silliest way to align to the falling edge of the
                    // probe PWM signal
        {
            if ( gpio_get( PROBE_PIN ) != 0 ) {
                if ( gpio_get( PROBE_PIN ) == 0 ) {
                    break;
                }
            }
        }
    }

    delayMicroseconds( ( probeHalfPeriodus * 5 ) + ( probeHalfPeriodus / 2 ) );

    readingPullup = digitalRead( pin );
    delayMicroseconds( probeHalfPeriodus * 3 );
    readingPullup2 = digitalRead( pin );
    delayMicroseconds( probeHalfPeriodus * 1 );
    readingPullup3 = digitalRead( pin );

    // pinMode(pin, INPUT_PULLDOWN);

    if ( rowBeingScanned != -1 ) {
        while ( 1 ) // this is the silliest way to align to the falling edge of the
                    // probe PWM signal
        {
            if ( gpio_get( PROBE_PIN ) != 0 ) {
                if ( gpio_get( PROBE_PIN ) == 0 ) {
                    break;
                }
            }
        }
    }

    delayMicroseconds( ( probeHalfPeriodus * 5 ) + ( probeHalfPeriodus / 2 ) );

    readingPulldown = digitalRead( pin );
    delayMicroseconds( probeHalfPeriodus * 3 );
    readingPulldown2 = digitalRead( pin );
    delayMicroseconds( probeHalfPeriodus * 1 );
    readingPulldown3 = digitalRead( pin );

    // if (readingPullup == 0 && readingPullup2 == 1 && readingPullup3 == 0 &&
    // readingPulldown == 1 && readingPulldown2 == 0 && readingPulldown3 == 1)
    // {
    //     state = probe;
    // }

    if ( ( readingPullup != readingPullup2 || readingPullup2 != readingPullup3 ) &&
         ( readingPulldown != readingPulldown2 ||
           readingPulldown2 != readingPulldown3 ) &&
         rowBeingScanned != -1 ) {
        state = probe;

        // if (readingPulldown != readingPulldown2 || readingPulldown2 !=
        // readingPulldown3)
        // {
        //     state = probe;

        // } else
        // {
        //     Serial.print("!");
        // }
    } else {

        if ( readingPullup2 == 1 && readingPulldown2 == 0 ) {

            state = floating;
        } else if ( readingPullup2 == 1 && readingPulldown2 == 1 ) {
            //              Serial.print(readingPullup);
            // // Serial.print(readingPullup2);
            // // Serial.print(readingPullup3);
            // // //Serial.print(" ");
            //  Serial.print(readingPulldown);
            // // Serial.print(readingPulldown2);
            // // Serial.print(readingPulldown3);
            //  Serial.print("\n\r");

            state = high;
        } else if ( readingPullup2 == 0 && readingPulldown2 == 0 ) {
            //  Serial.print(readingPullup);
            // // Serial.print(readingPullup2);
            // // Serial.print(readingPullup3);
            // // //Serial.print(" ");
            //  Serial.print(readingPulldown);
            // // Serial.print(readingPulldown2);
            // // Serial.print(readingPulldown3);
            //  Serial.print("\n\r");
            state = low;
        } else if ( readingPullup == 0 && readingPulldown == 1 ) {
            // Serial.print("shorted");
        }
    }

    // Serial.print("\n");
    // showLEDsCore2 = 1;
    // leds.show();
    // delayMicroseconds(100);

    return state;
}

void Probing::startProbe( long probeSpeed ) {

    // pinMode(PROBE_PIN, OUTPUT_4MA);
    // // pinMode(BUTTON_PIN, INPUT_PULLDOWN);
    // // pinMode(ADC0_PIN, INPUT);
    // digitalWrite(PROBE_PIN, HIGH);
}

void Probing::stopProbe( ) {
    // pinMode(PROBE_PIN, INPUT);
    // pinMode(BUTTON_PIN, INPUT);
}

int Probing::checkLastFound( int found ) {
    int found2 = 0;
    return found2;
}

void Probing::clearLastFound( ) {}

int Probing::scanRows( int pin ) {
    // return readProbe();
    return 0;
    int found = -1;
    connectedRows[ 0 ] = -1;

    if ( checkProbeButton( ) == 1 ) {
        return -18;
    }

    // pin = ADC1_PIN;

    // digitalWrite(RESETPIN, HIGH);
    // delayMicroseconds(20);
    // digitalWrite(RESETPIN, LOW);
    // delayMicroseconds(20);

    pinMode( PROBE_PIN, INPUT );
    delayMicroseconds( 400 );
    int probeRead = readFloatingOrState( PROBE_PIN, -1 );

    if ( probeRead == high ) {
        found = voltageSelection;
        connectedRows[ connectedRowsIndex ] = found;
        connectedRowsIndex++;
        found = -1;
        // return connectedRows[connectedRowsIndex];
        // Serial.print("high");
        // return found;
    }

    else if ( probeRead == low ) {
        found = GND;
        connectedRows[ connectedRowsIndex ] = found;
        connectedRowsIndex++;
        // return found;
        found = -1;
        // return connectedRows[connectedRowsIndex];
        // Serial.print(connectedRows[connectedRowsIndex]);

        // return connectedRows[connectedRowsIndex];
    }

    startProbe( );
    int chipToConnect = 0;
    int rowBeingScanned = 0;

    int xMapRead = 15;

    if ( pin == ADC0_PIN ) {
        xMapRead = 2;
    } else if ( pin == ADC1_PIN ) {
        xMapRead = 3;
    } else if ( pin == ADC2_PIN ) {
        xMapRead = 4;
    } else if ( pin == ADC3_PIN ) {
        xMapRead = 5;
    }

    for ( int chipScan = CHIP_A; chipScan < 8;
          chipScan++ ) // scan the breadboard (except the corners)
    {

        sendXYraw( CHIP_L, xMapRead, chipScan, 1 );

        for ( int yToScan = 1; yToScan < 8; yToScan++ ) {

            sendXYraw( chipScan, 0, 0, 1 );
            sendXYraw( chipScan, 0, yToScan, 1 );

            rowBeingScanned = globalState.connections.chipStates[ chipScan ].yMap[ yToScan ];
            if ( readFloatingOrState( pin, rowBeingScanned ) == probe ) {
                found = rowBeingScanned;

                if ( found != -1 ) {
                    connectedRows[ connectedRowsIndex ] = found;
                    connectedRowsIndex++;
                    found = -1;
                    // delayMicroseconds(100);
                    // stopProbe();
                    // break;
                }
            }

            sendXYraw( chipScan, 0, 0, 0 );
            sendXYraw( chipScan, 0, yToScan, 0 );
        }
        sendXYraw( CHIP_L, 2, chipScan, 0 );
    }

    int corners[ 4 ] = { 1, 30, 31, 60 };
    sendXYraw( CHIP_L, xMapRead, 0, 1 );
    for ( int cornerScan = 0; cornerScan < 4; cornerScan++ ) {

        sendXYraw( CHIP_L, cornerScan + 8, 0, 1 );

        rowBeingScanned = corners[ cornerScan ];
        if ( readFloatingOrState( pin, rowBeingScanned ) == probe ) {
            found = rowBeingScanned;
            // if (nextIsSupply)
            // {
            //     leds.setPixelColor(nodesToPixelMap[rowBeingScanned], 65, 10, 10);
            // }
            // else if (nextIsGnd)
            // {
            //     leds.setPixelColor(nodesToPixelMap[rowBeingScanned], 10, 65, 10);
            // }
            // else
            // {
            //     leds.setPixelColor(nodesToPixelMap[rowBeingScanned],
            //     rainbowList[rainbowIndex][0], rainbowList[rainbowIndex][1],
            //     rainbowList[rainbowIndex][2]);
            // }
            // showLEDsCore2 = 1;
            if ( found != -1 ) {
                connectedRows[ connectedRowsIndex ] = found;
                connectedRowsIndex++;
                found = -1;
                // stopProbe();
                // break;
            }
        }

        sendXYraw( CHIP_L, cornerScan + 8, 0, 0 );
    }
    sendXYraw( CHIP_L, xMapRead, 0, 0 );

    for ( int chipScan2 = CHIP_I; chipScan2 <= CHIP_J;
          chipScan2++ ) // scan the breadboard (except the corners)
    {

        int pinHeader = ADC0_PIN + ( chipScan2 - CHIP_I );

        for ( int xToScan = 0; xToScan < 12; xToScan++ ) {

            sendXYraw( chipScan2, xToScan, 0, 1 );
            sendXYraw( chipScan2, 13, 0, 1 );

            // analogRead(ADC0_PIN);

            rowBeingScanned = globalState.connections.chipStates[ chipScan2 ].xMap[ xToScan ];
            //   Serial.print("rowBeingScanned: ");
            //     Serial.println(rowBeingScanned);
            //     Serial.print("chipScan2: ");
            //     Serial.println(chipScan2);
            //     Serial.print("xToScan: ");
            //     Serial.println(xToScan);

            if ( readFloatingOrState( pinHeader, rowBeingScanned ) == probe ) {

                found = rowBeingScanned;

                // if (nextIsSupply)
                // {
                //     //leds.setPixelColor(nodesToPixelMap[rowBeingScanned], 65, 10,
                //     10);
                // }
                // else if (nextIsGnd)
                // {
                //    // leds.setPixelColor(nodesToPixelMap[rowBeingScanned], 10, 65,
                //    10);
                // }
                // else
                // {
                //     //leds.setPixelColor(nodesToPixelMap[rowBeingScanned],
                //     rainbowList[rainbowIndex][0], rainbowList[rainbowIndex][1],
                //     rainbowList[rainbowIndex][2]);
                // }
                // //showLEDsCore2 = 1;
                // // leds.show();

                if ( found != -1 ) {
                    connectedRows[ connectedRowsIndex ] = found;
                    connectedRowsIndex++;
                    found = -1;
                    // stopProbe();
                    // break;
                }
            }
            sendXYraw( chipScan2, xToScan, 0, 0 );
            sendXYraw( chipScan2, 13, 0, 0 );
        }
    }

    // stopProbe();
    // probeTimeout = millis();

    digitalWrite( RESETPIN, HIGH );
    delayMicroseconds( 20 );
    digitalWrite( RESETPIN, LOW );
    return connectedRows[ 0 ];
    // return found;

    // return 0;
}

int Probing::readRails( int pin ) {
    int state = -1;

    // Serial.print("adc0 \t");
    // Serial.println(adcReadings[0]);
    // Serial.print("adc1 \t");
    // Serial.println(adcReadings[1]);
    // Serial.print("adc2 \t");
    // Serial.println(adcReadings[2]);
    // Serial.print("adc3 \t");
    // Serial.println(adcReadings[3]);

    return state;
}