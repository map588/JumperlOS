// SPDX-License-Identifier: MIT

/*
Kevin Santo Cappuccio
Architeuthis Flux

KevinC@ppucc.io

5/28/2024

*/

#include "hardware/pio.h"
#include "pico.h"
#define PICO_RP2350A 0
// #include <pico/stdlib.h>
#include "user_functions.h"
#include <Arduino.h>

#ifdef USE_TINYUSB
#include "tusb.h" // For tud_task() function
#include <Adafruit_TinyUSB.h>
#endif

#include "ArduinoStuff.h"
#include "CH446Q.h"
#include "Commands.h"
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <SPI.h>
#include <Wire.h>

#include "Apps.h"
#include "ArduinoStuff.h"
#include "AsyncPassthrough.h"
#include "Debugs.h"
#include "FileParsing.h"
#include "FilesystemStuff.h"
#include "Graphics.h"
#include "HelpDocs.h"
#include "Highlighting.h"
#include "JulseView.h"
#include "JumperlOS.h"
#include "JumperlessDefines.h"
#include "LEDs.h"
#include "LogicAnalyzer.h"
#include "MatrixState.h"
#include "Menus.h"
#include "NetManager.h"
#include "NetsToChipConnections.h"
#include "Peripherals.h"
#include "PersistentStuff.h"
#include "Probing.h"
#include "Python_Proper.h"
#include "RotaryEncoder.h"
#include "States.h" // New state management system
#include "TermControl.h"
#include "TuiGlue.h"
#include "USBfs.h"
#include "configManager.h"
#include "externVars.h"
#include "oled.h"
#include "user_functions.h"
#include <hardware/adc.h>

#include "SingleCharCommands.h" // Single-character command system
#include "WaveGen.h"            // New async wavegen
#include "externVars.h"

bread b;

// Debug flags
bool debugWaitLoopTiming = false;
bool debugUSB = false; // USB mass storage debug output

// Tui UI/UX System
TuiGlue tuiGlue;

// Global async waveform generator
WaveGen wavegen;

int supplySwitchPosition = 0;
volatile bool core1busy = false;
volatile bool core2busy = false;

// void lastNetConfirm(int forceLastNet = 0);
void rotaryEncoderStuff( void );
void initRotaryEncoder( void );
void printDirectoryContents( const char* dirname, int level );

void core2stuff( void );

// volatile uint8_t pauseCore2 = 0;

volatile int loadingFile = 0;

unsigned long lastNetConfirmTimer = 0;
// int machineMode = 0;

// https://wokwi.com/projects/367384677537829889

volatile bool core2initFinished = false;

volatile bool configLoaded = false;

volatile int startupAnimationFinished = 0;

unsigned long startupTimers[ 12 ];

volatile int dumpLED = 0;
unsigned long dumpLEDTimer = 0;
unsigned long dumpLEDrate = 50;

const char firmwareVersion[] = "5.4.0.5"; //! remember to update this

bool newConfigOptions = false; //! set to true with new config options //!

// julseview julseview;
LogicAnalyzer logicAnalyzer;

void setup( ) {
    pinMode( RESETPIN, OUTPUT_12MA );

    digitalWrite( RESETPIN, HIGH );

    // FatFS.begin();
    if ( !FatFS.begin( ) ) {
        Serial.println( "Failed to initialize FatFS" );
    } else {
        Serial.println( "FatFS initialized successfully" );
    }

    startupTimers[ 0 ] = millis( );

    loadConfig( );

    configLoaded = 1;
    startupTimers[ 1 ] = millis( );
    delayMicroseconds( 200 );

    initNets( );
    backpowered = 0;

    // delay(1000);

    if ( jumperlessConfig.serial_1.function >= 5 &&
         jumperlessConfig.serial_1.function <= 6 ) {
        dumpLED = 1;
    }
    if ( jumperlessConfig.serial_2.function >= 5 &&
         jumperlessConfig.serial_2.function <= 6 ) {
        dumpLED = 1;
    }

    if ( jumperlessConfig.serial_1.function == 4 ||
         jumperlessConfig.serial_1.function == 6 ) {
        jumperlessConfig.top_oled.show_in_terminal = 2;
    }
    if ( jumperlessConfig.serial_2.function == 4 ||
         jumperlessConfig.serial_2.function == 6 ) {
        jumperlessConfig.top_oled.show_in_terminal = 3;
    }

    Serial.begin( 115200 );

    initDAC( );
    // Serial.println("DAC initialized");
    // Serial.flush();

    pinMode( PROBE_PIN, OUTPUT_8MA );
    pinMode( BUTTON_PIN, INPUT_PULLDOWN );
    // pinMode(buttonPin, INPUT_PULLDOWN);
    digitalWrite( PROBE_PIN, HIGH );

    // digitalWrite(BUTTON_PIN, HIGH);
    startupTimers[ 2 ] = millis( );

    initINA219( );

    // Serial.println("INA219 initialized");
    // Serial.flush();

    delayMicroseconds( 100 );

    digitalWrite( RESETPIN, LOW );

    while ( core2initFinished == 0 ) {
        tight_loop_contents( );
        // delayMicroseconds(1);
    }
    startupTimers[ 3 ] = millis( );
    // Serial.println("Core2 initialized");
    // Serial.flush();

    routableBufferPower( 1, 0 );
    startupTimers[ 4 ] = millis( );
    // Serial.println("Routable buffer power initialized");
    // Serial.flush();
    if ( jumperlessConfig.serial_1.async_passthrough == true ) {
        AsyncPassthrough::begin( 115200 );
    }

    drawAnimatedImage( 0 );
    startupAnimationFinished = 1;
    // Serial.println("Startup animation finished");
    // Serial.flush();
    clearAllNTCC( );

    startupTimers[ 5 ] = millis( );
    // Serial.println("NTCC initialized");
    // Serial.flush();
    delayMicroseconds( 100 );
    initArduino( );
    // Serial.println("Arduino initialized");
    // Serial.flush();
    // delay(100);
    initMenu( );
    startupTimers[ 6 ] = millis( );
    initADC( );
    startupTimers[ 7 ] = millis( );
    // Serial.println("ADC initialized");
    // Serial.flush();

    getNothingTouched( );

    checkProbeCurrentZero( );
    startupTimers[ 8 ] = millis( );
    // createSlots( -1, 0 );
    //  Serial.println("Slots created");
    //  Serial.flush();
    //  Note: YAML slot files are now created on-demand in States.cpp when first accessed

    initializeNetColorTracking( );   // Initialize net color tracking after slots are
                                     // created
    initializeValidationTracking( ); // Initialize validation tracking
    startupTimers[ 9 ] = millis( );
    // Serial.println("Net color tracking initialized");
    // Serial.flush();
    //  setupLogicAnalyzer();

    // tuiGlue.setSerial( &USBSer3 );
    //  Defer TuiGlue activation to first loop() call to avoid DTR wait and terminal probing delays
    //  tuiGlue.openOnDemand();
    //  Serial.println("TuiGlue initialized");
    //  Serial.flush();

    // Initialize and register services with jOSmanager
    // Serial.println("Registering services with jOSmanager...");

    // Wire up system services
    termSerialService.setTermControl( &termSerial );
    oledService.setOledDisplay( &oled );

    // Register all services in priority order using clean global names
    // CRITICAL priority services - run every loop for instant response
    jOS.registerService( &probeButton );       // CRITICAL - high-frequency button checking
    jOS.registerService( &termSerialService ); // CRITICAL - terminal input (when line buffering enabled)
    jOS.registerService( &menus );             // CRITICAL - direct user input

    // HIGH priority services - time-sensitive operations
    jOS.registerService( &tinyUSBService ); // HIGH - USB communication
    jOS.registerService( &slotManager );    // HIGH - states auto-save
    jOS.registerService( &probing );        // HIGH - user interaction sensitive (probe reading)
    jOS.registerService( &highlighting );   // HIGH - visual feedback

    // NORMAL priority services - periodic tasks
    jOS.registerService( &usbPeriodicService ); // NORMAL - USB housekeeping (when MSC enabled)
    jOS.registerService( &peripherals );        // NORMAL - periodic monitoring
    jOS.registerService( &singleCharCommands ); // NORMAL - command execution (synchronous, not periodic)

    // LOW priority services - background tasks
    jOS.registerService( &oledService ); // LOW - display updates
    jOS.registerService( &probeSwitch ); // LOW - switch position (not time-critical)
    jOS.registerService( &probePads );   // LOW - expensive ADC pad reading

    // Serial.println("Service registration complete");
    // Serial.flush();
}

unsigned long startupCore2timers[ 10 ];

void setupCore2stuff( ) {
    // delay(2000);
    startupCore2timers[ 0 ] = millis( );
    initCH446Q( );
    startupCore2timers[ 1 ] = millis( );
    // delay(1);

    while ( configLoaded == 0 ) {
        delayMicroseconds( 1 );
    }

    initLEDs( );

    startupCore2timers[ 2 ] = millis( );
    initRowAnimations( );
    startupCore2timers[ 3 ] = millis( );
    setupSwirlColors( );
    startupCore2timers[ 4 ] = millis( );

    startupCore2timers[ 5 ] = millis( );
    initRotaryEncoder( );
    startupCore2timers[ 6 ] = millis( );
    initSecondSerial( );
    core2initFinished = 1;
    // delay(4);
}

void setup1( ) {
    // flash_safe_execute_core_init();

    setupCore2stuff( );

    while ( startupAnimationFinished == 0 ) {
        // delayMicroseconds(1);
        // if (Serial.available() > 0) {
        //   char c = Serial.read();
        //  // Serial.print(c);
        //   //Serial.flush();
        //   }
    }

    startupCore2timers[ 7 ] = millis( );
}

char connectFromArduino = '\0';

int input = '\0';

int serSource = 0;
int readInNodesArduino = 0;

int firstLoop = 1;

volatile int probeActive = 0;

int showExtraMenu = 0;

int lastHighlightedNet = -1;
int lastBrightenedNet = -1;
int lastWarningNet = -1;

int dontShowMenu = 0;

unsigned long timer = 0;
int lastProbeButton = 0;
unsigned long waitTimer = 0;
unsigned long switchTimer = 0;

int attract = 0;

unsigned long switchPositionCheckTimer = 0;

unsigned long mscModeRefreshTimer = 0;
unsigned long mscModeRefreshInterval = 2000;

volatile int core1passthrough = 1;
int switchPosCount = 0;

#include <pico/stdlib.h>

#include <hardware/gpio.h>

unsigned long core1Timeout = millis( );

#define SETUP_LOGIC_ANALYZER_ON_BOOT 0

#define debug_startup_timers 0
#define debug_busy_timers 0

unsigned long busyPrintTime = 0;
unsigned long busyPrintInterval = 3000;
unsigned long busyTimers[ 10 ] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

TermControl termSerial( &Serial ); // Creates its own history instance

// Global storage for current command line (for backwards compatibility with parsers)
String currentCommandLine = "";

void loop( ) {

menu:

    // Serial.print("firstLoop = ");
    // Serial.println(firstLoop);
    // Serial.flush();
    if ( firstLoop == 1 ) {

        if ( firstStart == true || autoCalibrationNeeded == true ) {
            if ( autoCalibrationNeeded == true ) {
                Serial.println( "New calibration options detected in config.txt. "
                                "Running automatic calibration..." );
                delay( 2000 );
            }
            calibrateDacs( );
            firstStart = false;
        }
        firstLoop = 2;

        // Serial.println("--------------------------------");
        // loadChangedNetColorsFromFile( netSlot, 0 );

        // routableBufferPower(1, 1);

        goto loadfile;
    }

    if ( firstLoop == 2 ) {
        // Serial.println("initializing oled");
        // Serial.flush();

        if ( jumperlessConfig.top_oled.connect_on_boot == 1 ) {
            // Serial.println("Initializing OLED");
            oled.init( );
        }
        // Serial.println("oled initialized");
        // Serial.flush();

        // runApp(-1, "jdi MIPdisplay");

        firstLoop = 0;

#if SETUP_LOGIC_ANALYZER_ON_BOOT == 1
        goto setupla;
#endif
    }

    if ( Serial.available( ) >
         20 ) { // this is so if you dump a lot of data into the serial buffer, it
                // will consume it and not keep looping
        while ( Serial.available( ) > 0 ) {
            char c = Serial.read( );
            // Serial.print(c);
            // Serial.flush();
        }
    }

    if ( lastProbePowerDAC != probePowerDAC ) {
        probePowerDACChanged = true;
        // delay(1000);
        Serial.print( "probePowerDACChanged = " );
        Serial.println( probePowerDACChanged );
        routableBufferPower( 1, 1 );
    }

    // Serial.print("clearing highlighting");
    // Serial.flush();

    clearHighlighting( );

    // Serial.print("clearHighlighting");
    // Serial.flush();

    if ( termInInteractiveMode == 0 && jumperlessConfig.display.terminal_line_buffering == 1 ) {
        Serial.write( 0x0E ); // Turn ON interactive mode
        Serial.print( "Turning on interactive mode\n\r" );
        Serial.flush( );
        termInInteractiveMode = 1;
    } else if ( termInInteractiveMode == 1 && jumperlessConfig.display.terminal_line_buffering == 0 ) {
        Serial.write( 0x0F ); // Turn OFF interactive mode
        Serial.print( "Turning off interactive mode\n\r" );
        Serial.flush( );
        termInInteractiveMode = 0;
    }
    // Serial.print("termInInteractiveMode = ");
    // Serial.println(termInInteractiveMode);
    // Serial.flush();

    if ( dontShowMenu == 0 ) {
    forceprintmenu:
        // Use the new SingleCharCommands menu system
        singleCharCommands.printMenu( showExtraMenu );

#if debug_startup_timers == 1
        for ( int i = 1; i < 12; i++ ) {
            Serial.print( "startupTimer[" );
            Serial.print( i - 1 );
            Serial.print( " - " );
            Serial.print( i );
            Serial.print( "] = " );
            Serial.println( startupTimers[ i ] - startupTimers[ i - 1 ] );
            Serial.flush( );
        }

        for ( int i = 1; i < 12; i++ ) {
            Serial.print( "startupCore2Timer[" );
            Serial.print( i - 1 );
            Serial.print( " - " );
            Serial.print( i );
            Serial.print( "] = " );
            Serial.println( startupCore2timers[ i ] - startupCore2timers[ i - 1 ] );
            Serial.flush( );
        }
#endif
    }

    if ( configChanged == true && millis( ) > 3000 ) {
        Serial.print( "config changed, saving..." );
        saveConfig( );
        // Serial.println("\r                             \rconfig saved!\n\r");
        // Serial.flush();
        configChanged = false;
    }

dontshowmenu:

    connectFromArduino = '\0';
    firstConnection = -1;
    core1passthrough = 1;

#if debug_busy_timers == 1
    Serial.println( "Starting main loop: " + String( millis( ) ) + " ms" );
    Serial.flush( );
#endif
    busyPrintTime = millis( );

    //! This is the main busy wait loop waiting for input

    while ( ( ( jumperlessConfig.display.terminal_line_buffering == 1 && !termSerial.hasCompletedLine( ) ) ||
              ( jumperlessConfig.display.terminal_line_buffering == 0 && Serial.available( ) == 0 ) ) &&
            connectFromArduino == '\0' && slotChanged == 0 ) {

        unsigned long loopStart = debugWaitLoopTiming ? micros( ) : 0;

        busyTimers[ 0 ] = micros( );

        // Service all registered subsystems via jOSmanager
        // This now includes: termSerial, tud_task, usbPeriodic, oledPeriodic, and all other services
        jOS.serviceAll( );

        busyTimers[ 1 ] = micros( );

        // Check if logic analyzer is active (blocks normal operation)
        if ( logicAnalyzer.is_running( ) == true || logicAnalyzer.is_armed( ) == true ) {
            delay( 100 );
            continue;
        }

        // Check for menu activation (goto loadfile)
        // Note: clickMenu() is called within menus.service(), but we need to detect result
        // This will be refactored when we remove gotos entirely
        if ( menus.inClickMenu != 0 ) {
            core1passthrough = 0;
            goto loadfile;
        }

        // Check if terminal has completed line (for line buffering mode)
        if ( jumperlessConfig.display.terminal_line_buffering == 1 && termSerial.hasCompletedLine( ) ) {
            break; // Line is ready for processing
        }

        busyTimers[ 9 ] = micros( );

        if ( debugWaitLoopTiming ) {
            unsigned long loopEnd = micros( );
            if ( ( loopEnd - loopStart ) > 100000 ) { // More than 100ms (adjust threshold as needed)
                Serial.printf( "DEBUG: *** FULL LOOP took %lu us (%.2f ms) ***\n",
                               loopEnd - loopStart, ( loopEnd - loopStart ) / 1000.0 );
            }
        }
#if debug_busy_timers == 1
        if ( millis( ) - busyPrintTime > busyPrintInterval ) {
            busyPrintTime = millis( );
            for ( int i = 1; i < 10; i++ ) {
                Serial.print( "busyTimer " );
                Serial.print( i );
                Serial.print( ": " );
                Serial.print( busyTimers[ i ] - busyTimers[ i - 1 ] );
                Serial.println( " us" );
            }
            Serial.print( "total: " );
            Serial.print( busyTimers[ 9 ] - busyTimers[ 0 ] );
            Serial.print( " us\t\ttotal system time: " );
            Serial.print( millis( ) );
            Serial.println( " ms" );
            Serial.println( "\n\r" );
            Serial.flush( );
            // delay(100);
        }
#endif
        if ( jumperlessConfig.display.terminal_line_buffering == 1 ) {
            // Optionally service again at the end of the loop body
            termSerial.service( );
        }
    }
    if ( jumperlessConfig.display.terminal_line_buffering == 1 ) {
        // Only proceed when a full line is ready; then parse it
        if ( termSerial.hasCompletedLine( ) ) {
            String cmdLine = termSerial.getCompletedLine( ); // Get and consume the line
            cmdLine.trim( );
            currentCommandLine = cmdLine; // Store for backwards compatibility with parsers

            if ( cmdLine.length( ) > 0 ) {
                input = cmdLine[ 0 ];
            } else {
                input = '\n';
            }
        }
    } else {
        // Fallback mode: read single character like the old method
        if ( Serial.available( ) > 0 ) {
            input = Serial.read( );
            // Set currentCommandLine with just the single character for backwards compatibility
            // CRITICAL: Cast to char first! String(int) creates decimal string "87", not "W"
            currentCommandLine = String( (char)input );
        }
    }

    // Service incoming serial and use our line buffer instead of direct Serial.read

    // timer = millis( );
    // // Serial.print("input = ");
    // Serial.println(input);
    // Serial.flush();

    // Handle multi-character help commands
    if ( input == 'h' ) {
        // Check if next character is available (for "help" command)
        unsigned long helpTimer = millis( );
        while ( Serial.available( ) == 0 && millis( ) - helpTimer < 100 ) {
            // Small timeout for typing "help"
        }
        if ( Serial.available( ) > 0 ) {
            String helpString = "h";
            while ( Serial.available( ) > 0 && helpString.length( ) < 50 ) {
                char c = Serial.read( );
                if ( c == '\n' || c == '\r' )
                    break;
                helpString += c;
            }
            if ( helpString == "help" ) {
                showGeneralHelp( );
                goto dontshowmenu;
            } else if ( helpString.startsWith( "help " ) ) {
                String category = helpString.substring( 5 );
                category.trim( );
                showCategoryHelp( category.c_str( ) );
                goto dontshowmenu;
            }
        } else {
            // Just 'h' alone, let it fall through to normal processing
        }
    }

    // Handle command? help requests
    if ( input != '\n' && input != '\r' && input != ' ' ) {
        // Check if next character is '?' for command-specific help
        unsigned long helpTimer = millis( );
        while ( Serial.available( ) == 0 && millis( ) - helpTimer < 100 ) {
            // Small timeout for typing command?
        }
        if ( Serial.available( ) > 0 ) {
            char nextChar = Serial.peek( );
            if ( nextChar == '?' && ( input != 'A' && input != 'a' ) ) {
                Serial.read( ); // consume the '?'
                showCommandHelp( input );
                goto dontshowmenu;
            }
        }
    }

    if ( input == ' ' || input == '\n' || input == '\r' ) {
        // Serial.print(input);
        // Serial.flush();
        goto dontshowmenu;
    }
skipinput:

    // ========================================================================
    // Execute command using SingleCharCommands service
    // ========================================================================
    {
        inMainMenu = true;
        CommandResult cmdResult = singleCharCommands.executeCommand( (char)input, currentCommandLine );
        inMainMenu = false;

        // Handle command result
        switch ( cmdResult ) {
        case CMD_LOAD_FILE:
            goto loadfile;
        case CMD_DONT_SHOW_MENU:
            goto dontshowmenu;
        case CMD_SHOW_MENU:
        default:
            // Clean up serial buffer
            delayMicroseconds( 1000 );
            while ( Serial.available( ) > 5 ) {
                Serial.read( );
                delayMicroseconds( 1000 );
            }
            Serial.flush( );
            goto menu;
        }
    }

    // ========================================================================
    // OBSOLETE CODE BELOW - KEPT FOR REFERENCE ONLY
    // ========================================================================
    // The giant switch statement has been refactored into SingleCharCommands
    // All command handlers are now in SingleCharCommands.cpp with proper OOP design
    // This code below should NEVER execute - it's kept temporarily for reference
    // TODO: Delete lines 710-2238 (the old switch statement) after testing
    // ========================================================================

    goto menu; // Safety: skip old code and go back to menu

loadfile:
    loadingFile = 1;

    // Just clear preview mode flag - don't restore original slot
    // Let the normal load below handle loading the selected slot
    SlotManager& mgr = SlotManager::getInstance( );
    if ( mgr.isPreviewMode( ) ) {
        // Serial.println("Clearing preview mode");
        //  Clear preview flag without loading anything
        mgr.clearPreviewMode( );
    }

    // Save current state if dirty before reloading to prevent data loss
    // BUT skip this on the very first load (firstLoop == 1) to avoid overwriting
    // the saved slot with an empty/uninitialized state
    if ( globalState.isDirty( ) && firstLoop == 0 ) {
        //  Serial.println( "Saving dirty state before reload" );
        String saveError;
        if ( mgr.saveActiveSlot( saveError ) ) {
            if ( debugFP ) {
                Serial.println( "✓ Auto-saved dirty state before reload" );
            }
        } else if ( debugFP ) {
            Serial.println( "Warning: Failed to auto-save: " + saveError );
        }
    }

    // Load YAML state from slot file into globalState
    String loadError;
    if ( !mgr.loadSlot( netSlot, loadError ) ) {
        // if ( debugFP ) {
        Serial.print( "Warning: Failed to load slot " );
        Serial.print( netSlot );
        Serial.print( ": " );
        Serial.println( loadError );
        Serial.println( "Starting with empty slot" );
        // }
        // Empty slot is OK - just start fresh
        mgr.clearActiveSlot( );
    }

    if ( slotChanged == 1 ) {
        // clearChangedNetColors(0);
        // loadChangedNetColorsFromFile( netSlot, 0 );
    }

    slotChanged = 0;
    loadingFile = 0;

    refreshConnections( -1 );

    /*
    case 'z': {
        handleUserFunction( );
        goto dontshowmenu;
        break;
    }

    case '|': {
        erattaClearGPIO( -1 );
        Serial.println( "Eratta cleared" );
        Serial.flush( );
        goto dontshowmenu;
    }

    case 'w': //! w - Setup logic analyzer
    {
        // int tempReading = adc_read_blocking(8);
    setupla:

        if ( la_enabled ) {
            Serial.println( "Logic analyzer disabled, deinitializing..." );
            // julseview.deinit();
            la_enabled = false;
            goto dontshowmenu;
        } else {
            changeTerminalColor( 196, true, &Serial );
            Serial.println( "Logic analyzer enabled" );
            Serial.println( "Note: Logic analyzer is not yet fully functional and can make a mess of your memory" );
            Serial.println( "Make sure to save anything important on the file system before playing with it" );
            Serial.println( "Worst case, you can use this to nuke the flash and start fresh:" );
            changeTerminalColor( 39, true, &Serial );
            Serial.println( "https://github.com/Gadgetoid/pico-universal-flash-nuke/releases/latest" );

            changeTerminalColor( -1, true, &Serial );
            la_enabled = true;
        }

        break;
    }

    case 'X': { //! X - Resource Status
        Serial.println( "Resource Allocation Status:" );
        Serial.println( "==========================" );

        Serial.print( "Free Heap: " );
        Serial.println( rp2040.getFreeHeap( ) );

        Serial.println( "Total memory: " + String( rp2040.getTotalHeap( ) ) );
        Serial.println( "Free memory: " + String( rp2040.getFreeHeap( ) ) );

        // Check rotary encoder status
        if ( isRotaryEncoderInitialized( ) ) {
            Serial.println( "✓ Rotary Encoder: Initialized" );
            printRotaryEncoderStatus( );
        } else {
            Serial.println( "✗ Rotary Encoder: Not initialized" );
        }

        // Check for conflicts
        Serial.println( "\nConflict Detection:" );
        Serial.println( "Logic Analyzer conflicts: N/A (removed)" );

        printPIOStateMachines( );
        Serial.print( "rotary divider = " );
        Serial.println( rotaryDivider );

        Serial.println( "gpio    up dn\tfunction\tfunction_hex" );
        for ( int i = 0; i < 48; i++ ) {
            int pull = gpio_is_pulled_up( i );
            Serial.print( "gpio " );
            Serial.print( i );
            Serial.print( ":  " );
            if ( i < 10 ) {
                Serial.print( " " );
            }
            Serial.print( pull );
            Serial.print( "  " );

            pull = gpio_is_pulled_down( i );
            Serial.print( pull );

            Serial.print( "\t" );
            Serial.print( gpio_function_names[ gpio_get_function( i ) ].name );
            Serial.print( "\t" );
            Serial.print( gpio_get_function( i ), HEX );
            Serial.println( );
            Serial.flush( );
        }
        Serial.println( );
        break;
    }

    case 'G': { //! G - Load config.txt changes

        // pauseCore2 = true;

        float wavegen_frequency = 1000.0f;

        // Initialize async wavegen
        if ( !wavegen.begin( ) ) {
            Serial.println( "Failed to initialize wavegen" );
            Serial.flush( );
        } else {
            Serial.println( "wavegen initialized successfully" );
            Serial.print( "Fallback mode: " );
            Serial.println( wavegen.isFallbackMode( ) ? "ON" : "OFF" );

            // Keep synchronous mode (no Wire.writeAsync)
            wavegen.setFallbackMode( true );
            Serial.println( "Staying in synchronous fallback mode (no writeAsync)" );
            Serial.flush( );

            // Configure wavegen for ±8V range
            wavegen.setChannel( WAVEGEN_DAC1 );
            wavegen.setWaveform( WAVEGEN_SINE );
            float actual_freq = wavegen.setFrequencyAdjusted( wavegen_frequency );
            wavegen.setAmplitude( 4.0f ); // 4V amplitude for ±4V swing
            wavegen.setOffset( 0.0f );    // 0V offset (centered)

            Serial.print( "Initial frequency: " );
            Serial.print( wavegen_frequency );
            Serial.print( " Hz -> " );
            Serial.print( actual_freq );
            Serial.print( " Hz (" );
            Serial.print( wavegen.getTableSize( ) );
            Serial.print( " pts, " );
            Serial.print( wavegen.getBufferSize( ) );
            Serial.print( " bytes, " );
            Serial.print( wavegen.getBufferCycles( ) );
            Serial.println( " cycles)" );

            Serial.print( "Waveform: ±" );
            Serial.print( 4.0f );
            Serial.println( "V sine wave" );
            Serial.flush( );

            // Start wavegen
            Serial.println( "About to call wavegen.start()" );
            Serial.flush( );

            // Add some debug info before the call
            Serial.println( "Debug: About to enter wavegen.start()" );
            Serial.flush( );

            if ( wavegen.start( ) ) {
                Serial.println( "wavegen started successfully" );
                Serial.flush( );

                // Service the wavegen immediately to start async transfers
                Serial.println( "About to call wavegen.service()" );
                Serial.flush( );

                Serial.println( "Initial wavegen service completed" );
                Serial.flush( );

                while ( Serial.available( ) == 0 ) {
                    // Service wavegen frequently while waiting for keypress
                    rotaryEncoderStuff( );

                    if ( encoderDirectionState == UP ) {
                        wavegen_frequency *= 1.1f;
                        float actual_freq = wavegen.setFrequencyAdjusted( wavegen_frequency );
                        Serial.print( "Set: " );
                        Serial.print( wavegen_frequency );
                        Serial.print( " Hz, Actual: " );
                        Serial.print( actual_freq );
                        Serial.print( " Hz, Table: " );
                        Serial.print( wavegen.getTableSize( ) );
                        Serial.print( " pts, Buffer: " );
                        Serial.print( wavegen.getBufferSize( ) );
                        Serial.print( " bytes" );
                        Serial.println( );
                        Serial.flush( );
                    } else if ( encoderDirectionState == DOWN ) {
                        wavegen_frequency *= 0.9f;
                        float actual_freq = wavegen.setFrequencyAdjusted( wavegen_frequency );
                        Serial.print( "Set: " );
                        Serial.print( wavegen_frequency );
                        Serial.print( " Hz, Actual: " );
                        Serial.print( actual_freq );
                        Serial.print( " Hz, Table: " );
                        Serial.print( wavegen.getTableSize( ) );
                        Serial.print( " pts, Buffer: " );
                        Serial.print( wavegen.getBufferSize( ) );
                        Serial.print( " bytes" );
                        Serial.println( );
                        Serial.flush( );
                    }

                    // Service the async wavegen
                    //  wavegen.service();

                    if ( encoderButtonState == HELD ) {
                        wavegen.stop( );
                        Serial.print( "Final stats - Success: " );
                        Serial.print( wavegen.getSuccessfulWrites( ) );
                        Serial.print( ", Failed: " );
                        Serial.println( wavegen.getFailedWrites( ) );
                        break;
                    }
                }

                wavegen.stop( );
                Serial.println( "wavegen stopped" );
            } else {
                Serial.println( "Failed to start wavegen" );
            }
            Serial.flush( );
        }

        // pauseCore2 = false;

        Serial.println( "Reloading config.txt..." );
        configChanged = true;

        break;
    }
    case 'S': { //! S - raw speed test
        Serial.println( "Raw speed test..." );
        Serial.println( "Read frequency on row 29\n\n\r" );

        pauseCore2 = true;
        unsigned long cycles = 1000000;
        unsigned long start = micros( );
        sendXYraw( 10, 0, 4, 1 );
        for ( int i = 0; i < cycles; i++ ) {
            sendXYraw( 10, 0, 0, 1 );
            sendXYraw( 10, 0, 0, 0 );
        }
        unsigned long end = micros( );
        Serial.print( "Time for " );
        Serial.print( cycles );
        Serial.print( " on off cycles: " );
        Serial.print( end - start );
        Serial.println( " microseconds" );
        Serial.print( "Time per cycle: " );
        Serial.print( ( end - start ) / cycles );
        Serial.println( " microseconds" );
        Serial.print( "Frequency: " );
        Serial.print( ( (float)cycles / (float)( end - start ) ) * 1000 );
        Serial.println( " kHz\n\r" );
        Serial.flush( );
        pauseCore2 = false;

        break;
    }

    case 'j':

        for ( int i = 0; i < highSaturationSpectrumColorsCount; i++ ) {
            changeTerminalColorHighSat( i, true, &Serial, 0 );
            Serial.print( i );
            Serial.print( ": " );
            if ( i < 10 ) {
                Serial.print( " " );
            }
            Serial.print( highSaturationSpectrumColors[ i ] );

            Serial.print( "\t\t" );
            if ( i < highSaturationBrightColorsCount ) {
                changeTerminalColorHighSat( i, true, &Serial, 1 );
                Serial.print( i );
                Serial.print( ": " );
                if ( i < 10 ) {
                    Serial.print( " " );
                }
                Serial.print( highSaturationBrightColors[ i ] );
            }
            Serial.println( );
        }

        goto dontshowmenu;
        break;

    case 'J': { //! J - Test new States system (e.g., "J 1-2" or "J 1-5,10-20")

        Serial.println( "\n\r╭────────────────────────────────────╮" );
        Serial.println( "│   States System Test (J command)  │" );
        Serial.println( "╰────────────────────────────────────╯\n\r" );

        // Get the SlotManager instance
        SlotManager& mgr = SlotManager::getInstance( );
        JumperlessState& state = mgr.getActiveState( );

        // Parse the command line (skip first character 'J')
        String commandLine = currentCommandLine;
        if ( commandLine.length( ) > 1 ) {
            commandLine = commandLine.substring( 1 ); // Remove 'J'
            commandLine.trim( );

            if ( commandLine.length( ) > 0 ) {
                Serial.println( "Parsing connections: " + commandLine );

                // Parse connections (format: "1-2" or "1-5,10-20,15-30")
                int startIdx = 0;
                int connectionsAdded = 0;
                String errorMsg;

                while ( startIdx < (int)commandLine.length( ) ) {
                    int commaIdx = commandLine.indexOf( ',', startIdx );
                    if ( commaIdx == -1 ) {
                        commaIdx = commandLine.length( );
                    }

                    String conn = commandLine.substring( startIdx, commaIdx );
                    conn.trim( );

                    if ( conn.length( ) > 0 ) {
                        int dashIdx = conn.indexOf( '-' );
                        if ( dashIdx != -1 ) {
                            int node1 = conn.substring( 0, dashIdx ).toInt( );
                            int node2 = conn.substring( dashIdx + 1 ).toInt( );

                            Serial.print( "  Adding connection: " + String( node1 ) + "-" + String( node2 ) + "... " );

                            if ( state.addConnection( node1, node2, errorMsg ) ) {
                                Serial.println( "✓ Success" );
                                connectionsAdded++;
                            } else {
                                Serial.println( "✗ Failed" );
                                Serial.println( "    Error: " + errorMsg );
                            }
                        } else {
                            Serial.println( "  Invalid format: " + conn + " (should be N1-N2)" );
                        }
                    }

                    startIdx = commaIdx + 1;
                }

                // Apply connections to hardware if any were added
                if ( connectionsAdded > 0 ) {
                    Serial.println( "\n\r─── Applying to Hardware ───" );
                    Serial.print( "Refreshing connections... " );
                    state.markDirty( );       // Mark for auto-save
                    refreshConnections( -1 ); // Apply to hardware
                    Serial.println( "✓ Done" );
                }

                // Display current state
                Serial.println( "\n\r─── Current State ───" );
                Serial.println( "Connections: " + String( state.connections.numBridges ) );
                Serial.println( "Active Slot: " + String( mgr.getActiveSlot( ) ) );

                // List all connections
                if ( state.connections.numBridges > 0 ) {
                    Serial.println( "\n\rConnections in state:" );
                    for ( int i = 0; i < state.connections.numBridges; i++ ) {
                        int n1 = state.connections.bridges[ i ][ 0 ];
                        int n2 = state.connections.bridges[ i ][ 1 ];
                        int dup = state.connections.bridges[ i ][ 2 ];
                        Serial.print( "  " + String( i + 1 ) + ". " );
                        Serial.print( String( n1 ) + "-" + String( n2 ) );
                        if ( dup > 1 ) {
                            Serial.print( " (x" + String( dup ) + " duplicates)" );
                        }
                        Serial.println( );
                    }
                }

                // Test YAML serialization
                Serial.println( "\n\r─── Testing YAML Serialization ───" );
                String yamlOutput;
                if ( state.toYAML( yamlOutput ) ) {
                    Serial.println( "YAML output:" );

                    Serial.println( yamlOutput );

                    // Test saving to slot 7 (test slot)
                    Serial.println( "\n\r─── Testing Slot Save ───" );
                    Serial.print( "Saving to slot 7... " );
                    if ( mgr.saveSlot( 7, errorMsg ) ) {
                        Serial.println( "✓ Success" );
                        Serial.println( "  File: /slots/slot7.yaml" );

                        // Test loading it back
                        Serial.print( "Loading from slot 7... " );
                        if ( mgr.loadSlot( 7, errorMsg ) ) {
                            Serial.println( "✓ Success" );
                            Serial.println( "  Loaded " + String( mgr.getActiveState( ).connections.numBridges ) + " connections" );
                        } else {
                            Serial.println( "✗ Failed" );
                            Serial.println( "  Error: " + errorMsg );
                        }
                    } else {
                        Serial.println( "✗ Failed" );
                        Serial.println( "  Error: " + errorMsg );
                    }
                } else {
                    Serial.println( "Failed to serialize to JSON" );
                }

                // RAM usage estimate
                Serial.println( "\n\r─── Memory Usage ───" );
                Serial.println( "Active state RAM: ~" + String( mgr.getActiveStateRAMUsage( ) ) + " bytes" );
                Serial.println( "State object size: ~" + String( state.estimateRAMUsage( ) ) + " bytes" );

                Serial.println( "\n\r─── Test Complete ───" );

            } else {
                Serial.println( "No connections specified!" );
                Serial.println( "Usage: J 1-2  or  J 1-5,10-20,15-30" );
            }
        } else {
            // No arguments - show help
            Serial.println( "States System Test Command" );
            Serial.println( "\n\rUsage:" );
            Serial.println( "  J 1-2              - Add connection 1-2" );
            Serial.println( "  J 1-5,10-20        - Add multiple connections" );
            Serial.println( "  J 1-5,1-5,1-5      - Add duplicates (increments count)" );
            Serial.println( "\n\rFeatures:" );
            Serial.println( "  • Validates connections" );
            Serial.println( "  • Tracks duplicate counts" );
            Serial.println( "  • JSON serialization" );
            Serial.println( "  • Save/load from slots" );
            Serial.println( "  • Undo/redo history" );
            Serial.println( "\n\rExample:" );
            Serial.println( "  J 1-5              - Creates connection 1-5" );
            Serial.println( "  J TOP_RAIL-10      - Connects top rail to row 10" );
            Serial.println( "  J GND-32           - Connects ground to row 32" );
        }

        goto dontshowmenu;
        break;
    }

    case 'U': { //! U - Enable USB Mass Storage drive\n

        if ( mscModeEnabled == false ) {
            Serial.println( "Enabling USB Mass Storage drive..." );
            if ( initUSBMassStorage( ) ) {
                Serial.println( "USB Mass Storage enabled - device will appear as "
                                "'JUMPERLESS' drive\n\r" );
                Serial.println( "\tu = disable USB Mass Storage" );
                Serial.println( "\tG = reload config.txt" );
                Serial.println( "\ty = refresh connections when files change" );
                Serial.println( "\tS = show status" );
                Serial.println( "\n\r" );
                Serial.flush( );
                delay( 3000 );
            } else {
                Serial.println( "USB Mass Storage initialization failed" );
                Serial.flush( );
            }
        } else {
            Serial.println( "USB Mass Storage is already enabled" );
            printUSBMassStorageStatus( );
            refreshConnections( -1 );
            Serial.flush( );
        }

        delay( 3000 );
        unsigned long mscModeTimer = millis( );
        unsigned long lastServiceCallTime = 0;
        const unsigned long SERVICE_CALL_INTERVAL = 1000;  // Call service every 1 second

        Serial.println("◆ USB Mode: Starting service loop for live file monitoring");
        Serial.flush();

        unsigned long loopIterations = 0;

        while ( mscModeEnabled == true ) {
            loopIterations++;

            // Debug every 100 iterations (~1 second)
            extern bool debugUSB;
            if (debugUSB && loopIterations % 100 == 0) {
                Serial.print("◆ USB loop iteration ");
                Serial.println(loopIterations);
                Serial.flush();
            }

            // Check for serial input without blocking
            if ( Serial.available( ) > 0 ) {
                char c = Serial.read( );
                if ( c == 'u' ) {
                    Serial.println( "Disabling USB Mass Storage" );
                    disableUSBMassStorage( );
                    mscModeEnabled = false;
                    Serial.flush( );
                    refreshConnections( -1, 1, 1 );
                }
                if ( c == 'U' ) {
                    Serial.println( "Enabling USB Mass Storage" );
                    initUSBMassStorage( );
                    printUSBMassStorageStatus( );
                    Serial.flush( );
                }
                if ( c == 'y' || c == 'Y' ) {
                    Serial.println( "Refreshing connections" );
                    manualRefreshFromUSB( );
                    delay( 100 );
                    refreshConnections( -1 );

                    Serial.flush( );
                }
                if ( c == 'G' || c == 'g' ) {
                    Serial.println( "Reloading config.txt" );

                    Serial.flush( );
                    manualRefreshFromUSB( );
                    delay( 100 );
                    loadConfig( );
                    Serial.flush( );
                }
                if ( c == 's' || c == 'S' ) {
                    Serial.println( "Showing status" );
                    printUSBMassStorageStatus( );
                    Serial.flush( );
                }
            }

            // Service SlotManager and USB while waiting for input
            if (millis() - lastServiceCallTime > SERVICE_CALL_INTERVAL) {
                if (debugUSB) {
                    Serial.println("◆ USB loop: About to call SlotManager::service()");
                    Serial.flush();
                }
                SlotManager::getInstance().service();
                if (debugUSB) {
                    Serial.println("◆ USB loop: SlotManager::service() returned");
                    Serial.flush();
                }
                lastServiceCallTime = millis();
            }

            // Keep USB alive
            tud_task();

            // Small delay to prevent tight loop consuming CPU
            delay(10);
        }
        goto dontshowmenu;
        break;
    }

    case 'Z': { //! Z - Toggle USB debug mode
        Serial.println( "╭─────────────────────────────────╮" );
        Serial.println( "│        USB Debug Control        │" );
        Serial.println( "├─────────────────────────────────┤" );
        Serial.println( "│ 1. Toggle USB debug mode        │" );
        Serial.println( "│ 2. Manual refresh from USB      │" );
        Serial.println( "│ 3. Validate all slots           │" );
        Serial.println( "│ Any other key - Cancel          │" );
        Serial.println( "╰─────────────────────────────────╯" );
        Serial.print( "Choose option: " );
        Serial.flush( );

        // Wait for input
        while ( Serial.available( ) == 0 ) {
            delay( 1 );
        }
        char choice = Serial.read( );
        Serial.println( choice );

        switch ( choice ) {
        case '1':
            Serial.println( "\nToggling USB debug mode..." );
            setUSBDebug( !usb_debug_enabled );
            break;
        case '2':
            if ( isUSBMassStorageMounted( ) ) {
                Serial.println( "\nPerforming manual refresh from USB..." );
                manualRefreshFromUSB( );
            } else {
                Serial.println( "\nUSB drive not mounted" );
            }
            break;
        case '3':
            Serial.println( "\nValidating all slot files..." );
            // validateAllSlots(true);
            break;
        default:
            Serial.println( "\nCancelled" );
            break;
        }

        Serial.flush( );
        goto dontshowmenu;
        break;
    }

    case 'u': { //! u - Disable USB Mass Storage drive
        if ( mscModeEnabled == true ) {
            Serial.println( "Disabling USB Mass Storage drive..." );
            if ( disableUSBMassStorage( ) ) {
                Serial.println(
                    "USB Mass Storage disabled - device no longer appears as drive" );
                Serial.println( "Use 'U' command to re-enable when needed" );
            } else {
                Serial.println( "USB Mass Storage disable failed" );
            }
        } else {
            Serial.println( "USB Mass Storage is already disabled" );
            Serial.println( "Use 'U' command to enable" );
        }

        goto dontshowmenu;
        break;
    }

    case '/': { //!  /

        runApp( -1, (char*)"File Manager" );
        Serial.write( 0x0F );
        termInInteractiveMode = 0;
        Serial.flush( );
        break;
    }

    case 'C': { //!  C
        disableTerminalColors = !disableTerminalColors;
        if ( disableTerminalColors ) {
            Serial.println( "Terminal colors disabled" );
        } else {
            Serial.println( "Terminal colors enabled" );
        }
        Serial.flush( );
        break;
    }
    case 'E': { //!  E
        if ( dontShowMenu == 0 ) {
            dontShowMenu = 1;
        } else {
            dontShowMenu = 0;
        }
        break;
    }
    case 'k': {

        // Call the demo function directly - it will check for range input itself
        // Serial.println("Displaying color names (enter range like '10-200' for
        // specific range)"); colorPicker(0, 255);
        if ( jumperlessConfig.top_oled.show_in_terminal > 0 ) {
            jumperlessConfig.top_oled.show_in_terminal = 1;
        } else {
            jumperlessConfig.top_oled.show_in_terminal = 0;
        }
        configChanged = true;

        break;
    }

    case 0x10: { //! DLE
        input = '\0';
        dumpLED = 0;
        goto dontshowmenu;
    }
    case 'R': { //!  R
                // printWireStatus();

        // for (int i = 0; i < 10; i++) {
        if ( dumpLED == 1 ) {
            dumpLED = 0;
        } else {
            dumpLED = 1;
        }
        // }
        // printSerial1stuff();
        // printAllRLEimageData();
        goto dontshowmenu;
        break;
    }

        // Add this case for single Python command
    case '>': { //! > - Execute single Python command

        String pythonCommand = "";
        // Use our buffer instead of reading from Serial again
        if ( jumperlessConfig.display.terminal_line_buffering == 1 ) {
            pythonCommand = currentCommandLine;
            pythonCommand = pythonCommand.substring( 1 ); // Remove the '>' prefix
            pythonCommand.trim( );

        } else {
            while ( Serial.available( ) > 0 ) {
                pythonCommand += Serial.readString( );
            }
            // Serial.println(pythonCommand);
            // Serial.flush();
        }
        if ( pythonCommand.length( ) > 1 ) {

            // Serial.print("Python> ");
            // Apply Python syntax highlighting
            // displayStringWithSyntaxHighlighting(pythonCommand, &Serial);
            // Serial.println();
            // Execute the command
            executeSinglePythonCommand( pythonCommand.c_str( ) );
        } else {
            Serial.println( "Usage: > <python_command>" );
        }
        Serial.flush( );
        goto dontshowmenu;
        break;
    }

    // Modify the existing P case for Python command mode
    case 'P': { //! P - Deinitialize MicroPython to free memory
        Serial.println(
            "Deinitializing MicroPython to free memory... Total memory: " +
            String( rp2040.getTotalHeap( ) ) );
        Serial.println( "Free memory: " + String( rp2040.getFreeHeap( ) ) );
        deinitMicroPythonProper( );
        Serial.println( "MicroPython deinitialized. Memory freed." );

        Serial.println( "Total memory: " + String( rp2040.getTotalHeap( ) ) );
        Serial.println( "Free memory: " + String( rp2040.getFreeHeap( ) ) );
        Serial.println( "Use 'p' to reinitialize and enter REPL again." );
        goto dontshowmenu;
        break;
    }

    case 'p': {
        enterMicroPythonREPL( );

        refreshConnections( -1, 1, 1 );
        Serial.write( 0x0F );
        termInInteractiveMode = 0;
        Serial.flush( );
        // printAllConnectableNodes();
        break;
    }
    case '.': { //!  .
        // initOLED();
        if ( jumperlessConfig.top_oled.enabled == 0 ) {
            Serial.println( "oled enabled" );
            oled.init( );
            jumperlessConfig.top_oled.enabled = 1;
            configChanged = true;
        } else {
            oled.disconnect( );
            jumperlessConfig.top_oled.enabled = 0;
            oled.oledConnected = false;

            configChanged = true;
            Serial.println( "oled disconnected" );
        }

        goto dontshowmenu;
        break;
    }

    case 'c': { //!  c
        printChipStateArray( );
        goto dontshowmenu;
        break;
    }

    case '_': { //!  _
        printMicrosPerByte( );
        goto dontshowmenu;
        break;
    }

    case 'g': { //!  g
        printGPIOState( );
        break;
    }
    case '&': { //!  &
        // loadChangedNetColorsFromFile( netSlot, 0 );
        goto dontshowmenu;
        break;
        int node1 = -1;
        int node2 = -1;
        while ( Serial.available( ) == 0 ) {
        }
        // char c = Serial.read();
        node1 = Serial.parseInt( );
        node2 = Serial.parseInt( );
        Serial.print( "node1 = " );
        Serial.println( node1 );
        Serial.print( "node2 = " );
        Serial.println( node2 );
        Serial.print( "checkIfBridgeExistsLocal(node1, node2) = " );
        long unsigned int timer = micros( );
        Serial.println( checkIfBridgeExistsLocal( node1, node2 ) );
        Serial.print( "time taken = " );
        Serial.print( micros( ) - timer );
        Serial.println( " microseconds" );

        Serial.flush( );

        break;
    }

    case '\'': { //!  '
        pauseCore2 = 1;
        delay( 1 );
        drawAnimatedImage( 0 );
        pauseCore2 = 0;
        goto dontshowmenu;
        break;
    }
    case 'x': { //!  x
        digitalWrite( RESETPIN, HIGH );
        delay( 1 );
        refreshPaths( );
        clearAllNTCC( );
        // oled.oledConnected = false;

        clearNodeFile( netSlot, 0 );
        refreshConnections( -1, 1, 1 );
        digitalWrite( RESETPIN, LOW );

        Serial.println( "Cleared all connections" );

        goto dontshowmenu;

        break;
    }

    case '+': { //!  +

        readStringFromSerial( jumperlessConfig.display.terminal_line_buffering == 1 ? 3 : 0, 0 );
        goto loadfile;

        break;
    }

    case '-': { //!  -
        readStringFromSerial( jumperlessConfig.display.terminal_line_buffering == 1 ? 3 : 0, 1 );
        goto loadfile;
        break;
    }

    case '~': { //!  ~
        core1busy = 1;
        waitCore2( );
        printConfigToSerial( );
        core1busy = 0;
        Serial.flush( );
        goto dontshowmenu;
        break;
    }
    case '`': { //!  `
        core1busy = 1;
        waitCore2( );
        readConfigFromSerial( );
        core1busy = 0;
        Serial.flush( );
        goto dontshowmenu;
        break;
    }
        // case '2': {
        // runApp(2);
        // break;
        // }

    case '^': { //!  ^
        // doomOn = 1;
        // Serial.println(yesNoMenu());
        // break;
        char f[ 8 ] = { ' ' };
        int index = 0;
        float f1 = 0.0;
        unsigned long timer = millis( );
        while ( Serial.available( ) == 0 && millis( ) - timer < 1000 ) {
        }
        while ( index < 8 ) {
            f[ index ] = Serial.read( );
            index++;
        }

        f1 = atof( f );
        // Serial.print("f = ");
        // Serial.println(f1);
        if ( probePowerDAC == 1 ) {
            setDac0voltage( f1, 1, 1 );
        } else if ( probePowerDAC == 0 ) {
            setDac1voltage( f1, 1, 1 );
        }
        configChanged = true;
        Serial.printf( "DAC %d = %0.2f V\n", !probePowerDAC, f1 );
        Serial.flush( );
        goto dontshowmenu;
        break;
    }

    case '?': { //!  ?
        Serial.print( "Jumperless firmware version: " );
        Serial.println( firmwareVersion );
        Serial.flush( );
        goto dontshowmenu;
        break;
    }
    case '@': { //!  @
        Serial.flush( );

        if ( Serial.available( ) > 0 ) {
            String input = Serial.readString( );
            input.trim( ); // Remove whitespace

            if ( input.indexOf( ',' ) != -1 ) {
                // Format: @5,10 - SDA at row 5, SCL at row 10
                int commaIndex = input.indexOf( ',' );
                int sdaRow = input.substring( 0, commaIndex ).toInt( );
                int sclRow = input.substring( commaIndex + 1 ).toInt( );

                changeTerminalColor( 69, true );
                Serial.print( "I2C scan with SDA=" );
                Serial.print( sdaRow );
                Serial.print( ", SCL=" );
                Serial.println( sclRow );
                changeTerminalColor( 38, true );

                if ( i2cScan( sdaRow, sclRow, 26, 27, 1 ) > 0 ) {
                    Serial.println( "Found devices" );
                    return;
                } else {
                    removeBridgeFromState( RP_GPIO_26, sdaRow, true );
                    removeBridgeFromState( RP_GPIO_27, sclRow, true );
                }
            } else if ( input.length( ) > 0 && isdigit( input[ 0 ] ) ) {
                // Format: @5 - try all 4 combinations around row 5
                int baseRow = input.toInt( );

                changeTerminalColor( 69, true );
                Serial.print( "I2C scan trying all combinations around row " );
                Serial.println( baseRow );
                changeTerminalColor( 38, true );

                // Try all 4 combinations: SDA=base SCL=base+1, SDA=base+1 SCL=base,
                // SDA=base SCL=base-1, SDA=base-1 SCL=base
                int combinations[ 4 ][ 2 ] = {
                    { baseRow, baseRow + 1 }, // SDA=base, SCL=base+1
                    { baseRow + 1, baseRow }, // SDA=base+1, SCL=base
                    { baseRow, baseRow - 1 }, // SDA=base, SCL=base-1
                    { baseRow - 1, baseRow }  // SDA=base-1, SCL=base
                };

                for ( int i = 0; i < 4; i++ ) {
                    int sdaRow = combinations[ i ][ 0 ];
                    int sclRow = combinations[ i ][ 1 ];

                    // // Skip invalid row numbers (must be 1-60)
                    // if (sdaRow < 1 || sdaRow > 60 || sclRow < 1 || sclRow > 60) {
                    //   continue;
                    // }

                    changeTerminalColor( 202, true );
                    Serial.print( "\nTrying SDA=" );
                    Serial.print( sdaRow );
                    Serial.print( ", SCL=" );
                    Serial.print( sclRow );
                    Serial.println( ":" );
                    changeTerminalColor( 38, true );
                    int devicesFound = i2cScan( sdaRow, sclRow, 26, 27, 0 );
                    if ( devicesFound > 0 ) {
                        changeTerminalColor( 199, true );
                        Serial.printf(
                            "\n\rfound %d devices: SDA at row %d, SCL at row %d\n\r",
                            devicesFound, sdaRow, sclRow );
                        changeTerminalColor( -1 );
                        return;
                    }
                    delay( 1 ); // Small delay between scans
                }
            }
        } else {
            // Interactive mode - prompt for SDA and SCL
            Serial.print( "Enter SDA row: " );
            Serial.flush( );
            while ( Serial.available( ) == 0 ) {
            }
            int rowSDA = Serial.parseInt( );
            Serial.print( "Enter SCL row: " );
            Serial.flush( );
            while ( Serial.available( ) == 0 ) {
            }
            int rowSCL = Serial.parseInt( );

            changeTerminalColor( 69, true );
            Serial.print( "I2C scan with SDA=" );
            Serial.print( rowSDA );
            Serial.print( ", SCL=" );
            Serial.println( rowSCL );
            changeTerminalColor( 38, true );

            if ( i2cScan( rowSDA, rowSCL, 26, 27, 1 ) > 0 ) {
                // Serial.println("Found devices");
            } else {
                removeBridgeFromState( RP_GPIO_26, rowSDA, true );
                removeBridgeFromState( RP_GPIO_27, rowSCL, true );
            }
        }

        goto dontshowmenu;
        break;
    }
    case '$': { //!  $
        // return current slot number
        for ( int d = 0; d < 4; d++ ) {
            Serial.print( "dacSpread[" );
            Serial.print( d );
            Serial.print( "] = " );
            Serial.println( dacSpread[ d ] );
        }

        for ( int d = 0; d < 4; d++ ) {
            Serial.print( "dacZero[" );
            Serial.print( d );
            Serial.print( "] = " );
            Serial.println( dacZero[ d ] );
        }

        calibrateDacs( );
        // Serial.println(netSlot);
        break;
    }
    case 'r': { //!  r
        if ( Serial.available( ) > 0 ) {
            char c = Serial.read( );
            if ( c == '0' || c == '2' || c == 't' ) {
                resetArduino( 0 );
            }
            if ( c == '1' || c == '2' || c == 'b' ) {
                resetArduino( 1 );
            }
        } else {
            resetArduino( );
        }
        goto dontshowmenu;
        break;
    }

    case 'A': { //!  A
        // delay(100);
        int justAsk = 0;
        if ( Serial.available( ) > 0 ) {
            // Serial.print("checking for arduino connection");
            char c = Serial.read( );
            // if (c == ' ') {
            //   continue;
            //   }
            if ( c == '?' ) {
                if ( checkIfArduinoIsConnected( ) == 1 ) {
                    justAsk = 1;
                    Serial.println( "Y" );
                    Serial.flush( );
                    // break;
                } else {
                    justAsk = 1;
                    Serial.println( "n" );
                    Serial.flush( );
                    // break;
                }
            } else {
                // break;
            }
        }
        if ( justAsk == 0 ) {
            connectArduino( 0 );
            Serial.println( "UART connected to Arduino D0 and D1" );
            Serial.flush( );
        }
        goto dontshowmenu;
        break;
    }
    case 'a': { //!  a
        // delay(100);
        int justAsk = 0;
        while ( Serial.available( ) > 0 ) {
            // Serial.print("checking for arduino connection");
            char c = Serial.read( );
            // if (c == ' ') {
            //   continue;
            //   }
            if ( c == '?' ) {
                if ( checkIfArduinoIsConnected( ) == 1 ) {
                    justAsk = 1;
                    Serial.println( "Y" );
                    Serial.flush( );
                    // break;
                } else {
                    justAsk = 1;
                    Serial.println( "n" );
                    Serial.flush( );
                    // break;
                }
            } else {
                // break;
            }
        }
        if ( justAsk == 0 ) {
            disconnectArduino( 0 );
            Serial.println( "UART disconnected from Arduino D0 and D1" );
            Serial.flush( );
        }
        // goto loadfile;
        goto dontshowmenu;
        break;
    }

    case 'F': //!  F
        oled.cycleFont( );
        break;

    case '=': {
        Serial.println( "\n\r" );

        oled.dumpFrameBuffer( );

        goto dontshowmenu;
        break;
    }

    case 'i': { //!  i
        if ( oled.isConnected( ) == false ) {
            if ( oled.init( ) == false ) {
                Serial.println( "Failed to initialize OLED" );
                break;
            }
        }

        break;
    }

    case '#': {

        while ( Serial.available( ) == 0 && slotChanged == 0 ) {
            if ( slotChanged == 1 ) {
                // b.print("Jumperless", 0x101000, 0x020002, 0);
                // delay(100);
               // goto menu;
            }
        }
        printTextFromMenu( );

        clearLEDs( );
        showLEDsCore2 = 1;
        defconDisplay = -1;

        break;
    }
    case 'e': { //!  e
        showExtraMenu++;
        if ( showExtraMenu > 3 ) {
            showExtraMenu = 0;
        }
        break;
    }

    case 's': { //!  s
        printSlots( -1 );

        break;
    }
    case 'v': { //!  v
        if ( Serial.available( ) > 0 ) {
            char c = Serial.read( );

            if ( isdigit( c ) == 1 ) {
                int adc = c - '0';
                if ( adc >= 0 && adc <= 4 ) {
                    Serial.print( " adc" );
                    Serial.print( adc );
                    Serial.print( " = " );
                    float adcVoltage = readAdcVoltage( adc, 32 );
                    if ( adcVoltage > 0.00 ) {
                        Serial.print( " " );
                    }
                    Serial.println( adcVoltage );
                } else if ( c == 'p' ) {
                    Serial.print( " probe = " );
                    float probeVoltage = readAdcVoltage( 7, 32 );
                    if ( probeVoltage > 0.00 ) {
                        Serial.print( " " );
                    }
                    Serial.println( probeVoltage );
                }
            } else if ( c == 'i' ) {
                if ( Serial.available( ) > 0 ) {
                    char c = Serial.read( );
                    if ( c == '1' ) {
                        float iSense = INA1.getCurrent_mA( );
                        Serial.print( "ina1 = " );
                        Serial.print( iSense );
                        Serial.println( "mA" );
                    }
                } else {
                    float iSense = INA0.getCurrent_mA( );
                    Serial.print( "ina0 = " );
                    Serial.print( iSense );
                    Serial.print( "mA \t" );

                    iSense = INA0.getBusVoltage( );
                    Serial.print( iSense );
                    Serial.print( "V \t" );

                    iSense = INA0.getPower_mW( );
                    Serial.print( iSense );
                    Serial.println( "mW" );
                }
            } else if ( c == 'l' ) {

                if ( showReadings == 1 ) {
                    showReadings = 0;
                    Serial.println( "showReadings = 0" );
                } else {
                    showReadings = 1;
                    Serial.println( "showReadings = 1" );
                }
                chooseShownReadings( );
            }
            Serial.flush( );
        } else {
            Serial.println( );
            for ( int i = 0; i < 5; i++ ) {
                Serial.print( "adc" );
                Serial.print( i );
                Serial.print( " = " );
                float adcVoltage = readAdcVoltage( i, 32 );
                if ( adcVoltage > 0.00 ) {
                    Serial.print( " " );
                }
                Serial.println( adcVoltage );
            }
            Serial.print( "probe = " );
            float probeVoltage = readAdcVoltage( 7, 32 );
            if ( probeVoltage > 0.00 ) {
                Serial.print( " " );
            }
            Serial.println( probeVoltage );
        }
        Serial.flush( );
        goto dontshowmenu;
        break;

        if ( showReadings >= 3 || ( inaConnected == 0 && showReadings >= 1 ) ) {
            showReadings = 0;
            break;
        } else {
            showReadings++;

            chooseShownReadings( );

            goto dontshowmenu;
            break;
        }
    }
    case '}': {
        // Probe connect button - now handled in Probing service
        // This case is kept for backward compatibility but does nothing
        // The actual logic runs in Probing::handleProbeButtonActions()
        goto menu;
    }
    case '{': {
        // Probe clear button - now handled in Probing service
        // This case is kept for backward compatibility but does nothing
        // The actual logic runs in Probing::handleProbeButtonActions()
        goto menu;
    }
    case 'n':
        couldntFindPath( 1 );
        core1passthrough = 0;
        Serial.print( "\n\n\rnetlist\n\r" );

        listNets( anythingInteractiveConnected( -1 ) );

        break;
    case 'b': {
        int showDupes = 1;
        char in = Serial.read( );
        if ( in == '0' ) {
        } else if ( in == '2' ) {
            showDupes = 2;
        }
        Serial.print( "\n\rpathDuplicates: " );
        Serial.println( jumperlessConfig.routing.stack_paths );
        Serial.print( "dacDuplicates: " );
        Serial.println( jumperlessConfig.routing.stack_dacs );
        Serial.print( "railsDuplicates: " );
        Serial.println( jumperlessConfig.routing.stack_rails );
        Serial.print( "railPriority: " );
        Serial.println( jumperlessConfig.routing.rail_priority );
        couldntFindPath( 1 );
        Serial.print( "\n\rBridge Array\n\r" );
        printBridgeArray( );
        Serial.print( "\n\n\n\rPaths\n\r" );
        printPathsCompact( showDupes );
        Serial.print( "\n\n\rChip Status\n\r" );
        printChipStatus( );
        Serial.print( "\n\n\r" );

        Serial.print( "\n\n\r" );
        break;
    }
    case 'm':
        goto forceprintmenu;
        break;

    case '!':
        printNodeFile( netSlot, 0, 0, 0, true );
        break;

    case 'Y': { //! Y - Print current YAML state
        Serial.println( "\n\r╭────────────────────────────────────╮" );
        Serial.println( "│      Current YAML State (RAM)     │" );
        Serial.println( "╰────────────────────────────────────╯\n\r" );

        Serial.print( "Active Slot: " );
        Serial.println( netSlot );
        Serial.print( "Dirty Flag: " );
        Serial.println( globalState.isDirty( ) ? "YES (will auto-save)" : "NO (saved)" );

        if ( globalState.isDirty( ) ) {
            unsigned long timeSince = millis( ) - globalState.getLastModifiedTime( );
            Serial.print( "Time since last change: " );
            Serial.print( timeSince / 1000 );
            Serial.println( " seconds" );
        }

        Serial.println( "\n\r─── YAML Output ───\n\r" );

        String yamlOutput;
        if ( globalState.toYAML( yamlOutput ) ) {
            Serial.println( yamlOutput );
        } else {
            Serial.println( "✗ Failed to generate YAML" );
        }

        Serial.println( "\n\r─── Memory Usage ───" );
        Serial.print( "Connections: " );
        Serial.println( globalState.connections.numBridges );
        Serial.print( "State RAM: ~" );
        Serial.print( globalState.estimateRAMUsage( ) );
        Serial.println( " bytes" );

        Serial.println( "\n\r" );
        break;
    }

    case 'o': {
        // probeActive = 1;
        inputNodeFileList( rotaryEncoderMode );

        // input = ' ';
        showLEDsCore2 = -1;
        // probeActive = 0;
        goto loadfile;
        // goto dontshowmenu;
        break;
    }

    case '<': {

        if ( netSlot == 0 ) {
            netSlot = NUM_SLOTS - 1;
        } else {
            netSlot--;
        }
        Serial.print( "Slot " );
        Serial.println( netSlot );

        // Send slot change notification for app synchronization
        Serial.print("SLOT_CHANGED:");
        Serial.println(netSlot);
        Serial.flush();

        slotPreview = netSlot;
        slotChanged = 1;

        goto loadfile;
    }
    case 'y': {
    loadfile:
        loadingFile = 1;

        // Just clear preview mode flag - don't restore original slot
        // Let the normal load below handle loading the selected slot
        SlotManager& mgr = SlotManager::getInstance( );
        if ( mgr.isPreviewMode( ) ) {
            // Serial.println("Clearing preview mode");
            //  Clear preview flag without loading anything
            mgr.clearPreviewMode( );
        }

        // Save current state if dirty before reloading to prevent data loss
        // BUT skip this on the very first load (firstLoop == 1) to avoid overwriting
        // the saved slot with an empty/uninitialized state
        if ( globalState.isDirty( ) && firstLoop == 0 ) {
         //  Serial.println( "Saving dirty state before reload" );
            String saveError;
            if ( mgr.saveActiveSlot( saveError ) ) {
                if ( debugFP ) {
                    Serial.println( "✓ Auto-saved dirty state before reload" );
                }
            } else if ( debugFP ) {
                Serial.println( "Warning: Failed to auto-save: " + saveError );
            }
        }

        // Load YAML state from slot file into globalState
        String loadError;
        if ( !mgr.loadSlot( netSlot, loadError ) ) {
            if ( debugFP ) {
                Serial.print( "Warning: Failed to load slot " );
                Serial.print( netSlot );
                Serial.print( ": " );
                Serial.println( loadError );
                Serial.println( "Starting with empty slot" );
            }
            // Empty slot is OK - just start fresh
            mgr.clearActiveSlot( );
        }

        if ( slotChanged == 1 ) {
            // clearChangedNetColors(0);
            // loadChangedNetColorsFromFile( netSlot, 0 );
        }

        slotChanged = 0;
        loadingFile = 0;

        refreshConnections( -1 );

        break;
    }
    case 'f': {

        probeActive = 1;
        readInNodesArduino = 1;

        savePreformattedNodeFile( serSource, netSlot, rotaryEncoderMode );

        // Validate the saved node file
        int validation_result = validateNodeFileSlot( netSlot, false );
        if ( validation_result == 0 ) {
            if ( debugFP ) {
                Serial.println( "NodeFile validated successfully" );
            }
            refreshConnections( -1 );
        } else {
            if ( debugFP ) {
                Serial.println( "NodeFile validation failed: " +
                                String( getNodeFileValidationError( validation_result ) ) );
                Serial.println( "Connections not refreshed due to invalid node file" );
            }
        }

        input = ' ';

        probeActive = 0;
        if ( connectFromArduino != '\0' ) {
            connectFromArduino = '\0';
            input = ' ';
            readInNodesArduino = 0;

            goto dontshowmenu;
        }

        connectFromArduino = '\0';
        readInNodesArduino = 0;
        break;
    }

    case 't': { //! t - Test MSC callbacks
        // Test function disabled
        goto dontshowmenu;
        break;
    }

    case 'T': { //! T - Show netlist info
#ifdef FSSTUFF
        openNodeFile( );
        getNodesToConnect( );
#endif
        Serial.println( "\n\n\rnetlist\n\n\r" );

        bridgesToPaths( );

        listSpecialNets( );
        listNets( );
        printBridgeArray( );
        Serial.print( "\n\n\r" );
        Serial.print( numberOfNets );

        Serial.print( "\n\n\r" );
        Serial.print( numberOfPaths );
        checkChangedNetColors( -1 );

        assignNetColors( );
#ifdef PIOSTUFF
        sendAllPaths( );
#endif

        break;
    }

    case 'l':
        if ( LEDbrightnessMenu( ) == '!' ) {
            clearLEDs( );
            delayMicroseconds( 9200 );
            sendAllPathsCore2 = 1;
        }
        break;

        goto dontshowmenu;

        break;

    case 'd': {
        // debugFlagInit();
        debugFlagsMenu( );
    }

    case ':':

        if ( Serial.read( ) == ':' ) {
            // Serial.print("\n\r");
            // Serial.print("entering machine mode\n\r");
            // machineMode();
            showLEDsCore2 = 1;
            goto dontshowmenu;
            break;
        } else {
            break;
        }

    default:
        while ( Serial.available( ) > 0 ) {
            int f = Serial.read( );
            // delayMicroseconds(30);
        }

        break;
    }
    delayMicroseconds( 1000 );
    while ( Serial.available( ) > 5 ) {
        Serial.read( );
        delayMicroseconds( 1000 );
    }
    Serial.flush( );
    goto menu;

    */
}

unsigned long lastSwirlTime = 0;

int swirlCount = 42;
int spread = 13;

int readcounter = 0;
unsigned long schedulerTimer = 0;
unsigned long schedulerUpdateTime = 6300;

int swirled = 0;
int countsss = 0;

int probeCycle = 0;
int netUpdateRefreshCount = 0;

int tempDD = 0;
int clearBeforeSend = 0;

unsigned long tempTimer = 0;
int lastTemp = 0;

int passthroughStatus = 0;

unsigned long serialInfoTimer = 0;

unsigned long la_timer = 0;
unsigned long uartTaskTimer = 0;

void loop1( ) {

    while ( pauseCore2 == true ) {
        tight_loop_contents( );
        // replyWithSerialInfo( );
    }

    // Core 2 timing instrumentation (only when debug enabled)
    static unsigned long core2LoopStart = 0;
    if ( debugWaitLoopTiming ) {
        core2LoopStart = micros( );
    }

    // Only call logic analyzer if it's enabled and there's USB activity
    static uint32_t last_la_check = 0;
    uint32_t current_time = millis( );

    // ENHANCED STATE-BASED HANDLER CALLING

    // Priority order:
    // 1) High: path/LED refresh triggered by core1 (handled in core2stuff)
    // 2) Medium: wavegen_service (function generator streaming)
    // 3) Medium-low: rotary encoder
    // 4) Low: logo swirls/animations

    // Medium: service wavegen on core2 if running
    // if (wavegen_is_running()) {
    //     wavegen_service();
    // }

    // Use the new state variables to make smarter decisions about when to call the handler
    bool should_call_handler = false;

    // Route PulseView traffic to the new logic analyzer
    // OPTIMIZATION: Only poll USB when logic analyzer is actually running or recently active
    // USB polling is expensive (1-55ms!), so skip it when LA is idle
    unsigned long t0 = debugWaitLoopTiming ? micros( ) : 0;
    bool laActive = logicAnalyzer.is_running( ) || logicAnalyzer.is_armed( );
    // Check if LA had recent command (but ignore initial boot where last_command_time = 0)
    bool laRecentlyActive = ( logicAnalyzer.last_command_time > 0 ) &&
                            ( millis( ) - logicAnalyzer.last_command_time < 3000 );

    if ( laRecentlyActive || laActive ) {
        // Only check every 20ms when potentially active
        if ( millis( ) - last_la_check >= 20 ) {
            last_la_check = millis( );
            logicAnalyzer.handler( );
        }
    }

    if ( debugWaitLoopTiming ) {
        unsigned long t1 = micros( );
        if ( ( t1 - t0 ) > 1000 ) {
            Serial.printf( "CORE2: logicAnalyzer.handler() took %lu us\n", t1 - t0 );
        }
    }

    // OPTIMIZATION: Only service wavegen when it's actually running
    // wavegen.service() contains a blocking while() loop for I2C streaming!
    unsigned long t2 = debugWaitLoopTiming ? micros( ) : 0;
    if ( wavegen.isRunning( ) ) {
        wavegen.service( );
    }
    if ( debugWaitLoopTiming ) {
        unsigned long t3 = micros( );
        if ( ( t3 - t2 ) > 1000 ) {
            Serial.printf( "CORE2: wavegen.service() took %lu us\n", t3 - t2 );
        }
    }

    if ( doomOn == 1 ) {
        playDoom( );
        doomOn = 0;
    } else if ( pauseCore2 == 0 && logicAnalyzer.getIsRunning( ) == false ) {
        unsigned long t4 = debugWaitLoopTiming ? micros( ) : 0;
        core2stuff( );
        if ( debugWaitLoopTiming ) {
            unsigned long t5 = micros( );
            if ( ( t5 - t4 ) > 5000 ) { // Report if core2stuff takes > 5ms
                Serial.printf( "CORE2: core2stuff() took %lu us (%.2f ms)\n", t5 - t4, ( t5 - t4 ) / 1000.0 );
            }
        }
    }

    if ( millis( ) - uartTaskTimer > 10 ) {
        uartTaskTimer = millis( );
        if ( jumperlessConfig.serial_1.async_passthrough == true ) {
            AsyncPassthrough::task( );
        }

        passthroughStatus = secondSerialHandler( );
    }

    replyWithSerialInfo( );

    if ( dumpLED == 1 ) {

        if ( millis( ) - dumpLEDTimer > dumpLEDrate ) {
            if ( core1busy == false ) {
                core2busy = true;
                core1busy = true;
                delayMicroseconds( 2000 );
                dumpLEDs( );
                delayMicroseconds( 1000 );
                core2busy = false;
                core1busy = false;
            }

            dumpLEDTimer = millis( );
        }
    }

    // Core 2 total loop timing
    if ( debugWaitLoopTiming && core2LoopStart > 0 ) {
        unsigned long core2LoopEnd = micros( );
        if ( ( core2LoopEnd - core2LoopStart ) > 20000 ) { // Report if loop takes > 20ms
            Serial.printf( "CORE2: *** FULL LOOP took %lu us (%.2f ms) ***\n",
                           core2LoopEnd - core2LoopStart, ( core2LoopEnd - core2LoopStart ) / 1000.0 );
        }
    }
}

void core2stuff( ) // core 2 handles the LEDs and the CH446Q8
{
    core2busy = false;

    if ( showLEDsCore2 < 0 ) {
        showLEDsCore2 = abs( showLEDsCore2 );
        // Serial.println("clearBeforeSend = 1");

        clearBeforeSend = 1;
    }

    if ( micros( ) - schedulerTimer > schedulerUpdateTime || showLEDsCore2 == 3 ||

         showLEDsCore2 == 4 ||
         showLEDsCore2 == 6 && core1busy == false && core1request == 0 ) {

        if ( ( ( ( showLEDsCore2 >= 1 && loadingFile == 0 ) || showLEDsCore2 == 3 ||
                 ( swirled == 1 ) && sendAllPathsCore2 == 0 ) ||
               showProbeLEDs != lastProbeLEDs ) &&
             sendAllPathsCore2 == 0 ) {

            if ( showLEDsCore2 == 6 ) {
                showLEDsCore2 = 1;
            }

            int rails =
                showLEDsCore2; // 3 doesn't show nets and keeps control of the LEDs

            if ( rails != 3 ) {
                core2busy = true;
                lightUpRail( -1, -1, 1 );
                logoSwirl( swirlCount, spread, probeActive );
                core2busy = false;
            }

            if ( rails == 5 || rails == 3 ) {
                core2busy = true;

                logoSwirl( swirlCount, spread, probeActive );
                core2busy = false;
            }

            // Allow showing nets if not in menu OR if in preview mode
            if ( rails != 2 && rails != 5 && rails != 3 &&
                 ( inClickMenu == 0 || SlotManager::getInstance( ).isPreviewMode( ) ) &&
                 inPadMenu == 0 && hideNets == 0 ) {

                // Skip defcon display when previewing slots - always show nets
                if ( defconDisplay >= 0 && probeActive == 0 && !SlotManager::getInstance( ).isPreviewMode( ) ) {

                    // core2busy = true;
                    defcon( swirlCount, spread, defconDisplay );
                    // core2busy = false;
                } else {

                    while ( core1busy == true ) {
                        // core2busy = false;
                    }
                    core2busy = true;

                    if ( clearBeforeSend == 1 ) {
                        clearLEDsExceptRails( );
                        // Serial.println("clearing");
                        clearBeforeSend = 0;
                    }

                    unsigned long t0 = debugWaitLoopTiming ? micros( ) : 0;
                    showNets( );
                    if ( debugWaitLoopTiming ) {
                        unsigned long t1 = micros( );
                        if ( ( t1 - t0 ) > 5000 ) {
                            Serial.printf( "CORE2:   showNets() took %lu us\n", t1 - t0 );
                        }
                    }

                    unsigned long t2 = debugWaitLoopTiming ? micros( ) : 0;
                    readGPIO( ); // if want, I can make this update the LEDs like 10 times
                                 // faster by putting outside this loop,
                    showLEDmeasurements( );
                    if ( debugWaitLoopTiming ) {
                        unsigned long t3 = micros( );
                        if ( ( t3 - t2 ) > 2000 ) {
                            Serial.printf( "CORE2:   readGPIO+showLEDmeasurements() took %lu us\n", t3 - t2 );
                        }
                    }

                    unsigned long t4 = debugWaitLoopTiming ? micros( ) : 0;
                    showAllRowAnimations( );
                    if ( debugWaitLoopTiming ) {
                        unsigned long t5 = micros( );
                        if ( ( t5 - t4 ) > 2000 ) {
                            Serial.printf( "CORE2:   showAllRowAnimations() took %lu us\n", t5 - t4 );
                        }
                    }

                    core2busy = false;
                    netUpdateRefreshCount = 0;
                }
            }

            core2busy = true;

            unsigned long t6 = debugWaitLoopTiming ? micros( ) : 0;
            leds.show( );
            if ( debugWaitLoopTiming ) {
                unsigned long t7 = micros( );
                if ( ( t7 - t6 ) > 3000 ) { // leds.show() is typically the slowest!
                    Serial.printf( "CORE2:   leds.show() took %lu us (%.2f ms) ⚠️ BLOCKING\n",
                                   t7 - t6, ( t7 - t6 ) / 1000.0 );
                }
            }

            // probeLEDs.clear();

            // Update probe LEDs to reflect current state
            if ( checkingButton == 0 || showProbeLEDs == 2 ) {
                unsigned long t8 = debugWaitLoopTiming ? micros( ) : 0;
                probeLEDhandler( );
                if ( debugWaitLoopTiming ) {
                    unsigned long t9 = micros( );
                    if ( ( t9 - t8 ) > 2000 ) {
                        Serial.printf( "CORE2:   probeLEDhandler() took %lu us\n", t9 - t8 );
                    }
                }
                // core2busy = false;
            }
            core2busy = false;
            if ( rails != 3 && swirled == 0 ) {
                showLEDsCore2 = 0;

                // delayMicroseconds(3200);
            }

            swirled = 0;
            if ( inClickMenu == 1 ) {
                unsigned long t10 = debugWaitLoopTiming ? micros( ) : 0;
                rotaryEncoderStuff( );
                if ( debugWaitLoopTiming ) {
                    unsigned long t11 = micros( );
                    if ( ( t11 - t10 ) > 2000 ) {
                        Serial.printf( "CORE2:   rotaryEncoderStuff() took %lu us ⚠️ ENCODER\n", t11 - t10 );
                    }
                }
            }
            core2busy = false;

        } else if ( sendAllPathsCore2 != 0 ) {

            if ( sendAllPathsCore2 == 1 ) {
                sendPaths( 0 );
            } else if ( sendAllPathsCore2 == -1 ) {
                sendPaths( 1 );
            } else {
                sendPaths( sendAllPathsCore2 );
            }
            sendAllPathsCore2 = 0;

        } else if ( millis( ) - lastSwirlTime > 51 && loadingFile == 0 &&
                    showLEDsCore2 == 0 && core1busy == false ) {
            readcounter++;

            lastSwirlTime = millis( );

            if ( swirlCount >= LOGO_COLOR_LENGTH - 1 ) {
                swirlCount = 0;
            } else {
                swirlCount++;
            }

            if ( swirlCount % 20 == 0 ) {
                countsss++;
            }

            if ( showLEDsCore2 == 0 && !wavegen.isRunning( ) ) {
                swirled = 1; // only swirl when wavegen not streaming
            }

            // leds.show();
        } else if ( inClickMenu == 0 && probeActive == 0 ) {

            if ( ( ( countsss > 8 && defconDisplay >= 0 ) || countsss > 10 ) &&
                 defconDisplay != -1 ) {
                countsss = 0;

                if ( defconDisplay != -1 ) {
                    tempDD++;

                    if ( tempDD > 6 ) {
                        tempDD = 0;
                    }
                    defconDisplay = tempDD;
                }

                if ( defconDisplay > 5 ) {
                    defconDisplay = 0;
                }
            }

            if ( readcounter > 100 ) {
                readcounter = 0;
                if ( probeCycle > 4 ) {
                    probeCycle = 1;
                }
            }

            rotaryEncoderStuff( );

        } else {
            rotaryEncoderStuff( );
        }

        schedulerTimer = micros( );
        core2busy = false;
    }
}