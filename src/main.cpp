// SPDX-License-Identifier: MIT

/*
Kevin Santo Cappuccio
Architeuthis Flux

KevinC@ppucc.io

5/28/2024

*/

#include "hardware/pio.h"
#include "Jerial.h"
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
// TermControl is now part of Jerial.h
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

// Jerial global instance is defined in Jerial.cpp

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

unsigned long startupTimers[ 16 ];

volatile int dumpLED = 0;
unsigned long dumpLEDTimer = 0;
unsigned long dumpLEDrate = 150;

const char firmwareVersion[] = "5.5.0.7"; //! remember to update this

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

    // Check for firmware updates and provision new files if needed
    checkAndHandleFirmwareUpdate();

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
    
    // Configure Jerial for both input and output
    // Jerial automatically enables terminal control (line editing, history) for USB Serial endpoints
    Jerial.setInputStream(JerialEndpoint::USB_SERIAL);  // Input with terminal control
    Jerial.addOutputStream(JerialEndpoint::USB_SERIAL); // Output to USB
    //Jerial.addOutputStream(JerialEndpoint::USB_SER2);  // Output to Serial1
    //Jerial.addOutputStream(JerialEndpoint::OLED);     // Optional: also show on OLED
    //Jerial.addOutputStream(JerialEndpoint::SERIAL1);  // Optional: UART to Arduino
    
    // Enable automatic tag stripping for input
    // This removes <j> and </j> tags from incoming USB commands to prevent weird behavior
    Jerial.setAutoStripTags(true);
    
    initDAC( );
    // Serial.println("DAC initialized");
    // Serial.flush();

    pinMode( PROBE_PIN, OUTPUT_8MA );
    pinMode( BUTTON_PIN, INPUT_PULLDOWN );
    // pinMode(buttonPin, INPUT_PULLDOWN);
    digitalWrite( PROBE_PIN, HIGH );

    // digitalWrite(BUTTON_PIN, HIGH);
    startupTimers[ 2 ] = millis( );

    //initINA219( );

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

   // routableBufferPower( 1, 0 );
    
    // Serial.println("Routable buffer power initialized");
    // Serial.flush();
    if ( jumperlessConfig.serial_1.async_passthrough == true ) {
        AsyncPassthrough::begin( 115200 );
    }
startupTimers[ 4 ] = millis( );
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

    initINA219( );

    // Serial.println("currentReadingOffset0_mA = " + String(currentReadingOffset0_mA));
    // Serial.println("currentReadingOffset1_mA = " + String(currentReadingOffset1_mA));
    // Serial.flush();

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
    termSerialService.setTermControl( &Jerial );
    //oledService.setOledDisplay( &oled );

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
startupCore2timers[ 7 ] = millis( );
    while ( startupAnimationFinished == 0 ) {
        // delayMicroseconds(1);
        // if (Serial.available() > 0) {
        //   char c = Serial.read();
        //  // Serial.print(c);
        //   //Serial.flush();
        //   }
    }

    startupCore2timers[ 8 ] = millis( );
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

// Jerial is now used for all serial communications (input and output)

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
                delay( 3000 );
            }
            calibrateDacs( );
            firstStart = false;
        }
        firstLoop = 2;

        // Serial.println("--------------------------------");
        // loadChangedNetColorsFromFile( netSlot, 0 );

        // routableBufferPower(1, 1);

        // Serial.println("waiting for core 2 to finish initializing " + String(millis()));
        // while (core2initFinished == 0) {
        //     delayMicroseconds(1000);
        //     Serial.println("core2initFinished = 0 waiting for core 2 to finish initializing " + String(millis()));
        //     Serial.flush();
        // }

        // // CRITICAL: Also wait for loop1() to actually start before proceeding
        // // This prevents race condition where we set sendAllPathsCore2 before loop1() can process it
        // Serial.println("waiting for core 2 loop to start " + String(millis()));
        // while (core2loopStarted == 0) {
        //     delayMicroseconds(1000);
        // }
        // Serial.println("core 2 loop started " + String(millis()));

        goto loadfile;
    }

    if ( firstLoop == 2 ) {
        // Serial.println("initializing oled");
        // Serial.flush();
        // Serial.println("millis = " + String(millis()));

        if ( jumperlessConfig.top_oled.connect_on_boot == 1 ) {
             //Serial.println("Initializing OLED");
            oled.init( );
        }
        // Serial.println("millis = " + String(millis()));
        // Serial.println("oled initialized");
        // Serial.flush();

        //Jerial.addInputSource(JerialEndpoint::SERIAL1);

        // runApp(-1, "jdi MIPdisplay");
        printColorJogoSmall();

        firstLoop = 0;

#if SETUP_LOGIC_ANALYZER_ON_BOOT == 1
        goto setupla;
#endif
    }

    if ( Jerial.available( ) >
         20 ) { // this is so if you dump a lot of data into the serial buffer, it
                // will consume it and not keep looping
        while ( Jerial.available( ) > 0 ) {
            char c = Jerial.read( );
            // Jerial.print(c);
            // Jerial.flush();
        }
    }

    if ( lastProbePowerDAC != probePowerDAC ) {
        probePowerDACChanged = true;
        // delay(1000);
        Jerial.print( "probePowerDACChanged = " );
        Jerial.println( probePowerDACChanged );
        routableBufferPower( 1, 1 );
    }

    // Jerial.print("clearing highlighting");
    // Jerial.flush();

    clearHighlighting( );

    // Jerial.print("clearHighlighting");
    // Jerial.flush();

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
        // USBSer2.print( "printMenu" );
        // USBSer2.flush( );

        

#if debug_startup_timers == 1
        for ( int i = 1; i < 16; i++ ) {
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



dontshowmenu:

if ( configChanged == true && millis( ) > 3000 ) {
    Jerial.print( "config changed, saving..." );
    saveConfig( );
    // Serial.println("\r                             \rconfig saved!\n\r");
    // Serial.flush();
    configChanged = false;
}
    connectFromArduino = '\0';
    firstConnection = -1;
    core1passthrough = 1;

#if debug_busy_timers == 1
    Serial.println( "Starting main loop: " + String( millis( ) ) + " ms" );
    Serial.flush( );
#endif
    busyPrintTime = millis( );



    //! This is the main busy wait loop waiting for input

    while ( ( ( jumperlessConfig.display.terminal_line_buffering == 1 && !Jerial.hasCompletedLine( ) ) ||
              ( jumperlessConfig.display.terminal_line_buffering == 0 && Serial.available( ) == 0 ) ) &&
            connectFromArduino == '\0' && slotChanged == 0 ) {

        unsigned long loopStart = debugWaitLoopTiming ? micros( ) : 0;

        busyTimers[ 0 ] = micros( );

        // Service all registered subsystems via jOSmanager
        // This now includes: Jerial, tud_task, usbPeriodic, oledPeriodic, and all other services
        jOS.serviceAll( );



        // if (configChanged == true) {
        //     Jerial.print("config changed, saving...");
        //     saveConfig();
        //     // Serial.println("\r                             \rconfig saved!\n\r");
        //     // Serial.flush();
        //     configChanged = false;
        // }

        // CRITICAL: Handle Arduino flashing (DTR pulse detection) on Core 0
        // This MUST run on Core 0 because it can call refreshLocalConnections()
        // via flashArduino() -> connectArduino() -> refresh() chain
        static unsigned long lastSecondSerialCheck = 0;
        if (millis() - lastSecondSerialCheck > 10) {
            lastSecondSerialCheck = millis();
            
            // Handle async passthrough (if enabled)
            if (jumperlessConfig.serial_1.async_passthrough == true) {
                AsyncPassthrough::task();
            }
            
            // Handle Arduino flashing and serial passthrough
            secondSerialHandler();
        }

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

        // Check if terminal has completed line (includes injected commands - works regardless of buffering mode)
        if ( Jerial.hasCompletedLine() ) {
            break; // Line is ready for processing (could be user input or injected command)
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
            Jerial.service( );
        }
    }
    // Check for completed lines first (includes both injected and buffered input)
    // This works regardless of line buffering mode - injected commands always work
    if ( Jerial.hasCompletedLine( ) ) {
        String cmdLine = Jerial.getCompletedLine( ); // Get and consume the line
        cmdLine.trim( );
        currentCommandLine = cmdLine; // Store for backwards compatibility with parsers

        if ( cmdLine.length( ) > 0 ) {
            input = cmdLine[ 0 ];
        } else {
            input = '\n';
        }
    } else if ( jumperlessConfig.display.terminal_line_buffering != 1 ) {
        // Only read single character if NOT in line buffering mode
        // (line buffering mode already handled by Jerial.service() above)
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
// Serial.println("loadingFile" + String(millis()));
// Serial.flush();
startupTimers[ 10 ] = millis( );
    // Just clear preview mode flag - don't restore original slot
    // Let the normal load below handle loading the selected slot
    SlotManager& mgr = SlotManager::getInstance( );
    if ( mgr.isPreviewMode( ) ) {
        // Serial.println("Clearing preview mode");
        // Serial.flush();
        //  Clear preview flag without loading anything
        mgr.clearPreviewMode( );
    }
startupTimers[ 11 ] = millis( );
    // Save current state if dirty before reloading to prevent data loss
    // BUT skip this on the very first load (firstLoop == 1) to avoid overwriting
    // the saved slot with an empty/uninitialized state
    // Serial.println("globalState.isDirty( ) = " + String(globalState.isDirty( )));
    // Serial.println("firstLoop = " + String(firstLoop));
    // Serial.flush();
    if ( globalState.isDirty( ) && firstLoop == 0 ) {
        // Serial.println( "Saving dirty state before reload" );
        // Serial.flush();
        String saveError;
        if ( mgr.saveActiveSlot( saveError ) ) {
            if ( debugFP ) {
                Serial.println( "✓ Auto-saved dirty state before reload" );
            }
        } else if ( debugFP ) {
            Serial.println( "Warning: Failed to auto-save: " + saveError );
        }
    }
startupTimers[ 12 ] = millis( );
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
startupTimers[ 13 ] = millis( );
    if ( slotChanged == 1 ) {
        // clearChangedNetColors(0);
        // loadChangedNetColorsFromFile( netSlot, 0 );
    }

    slotChanged = 0;
    loadingFile = 0;
    if (firstLoop == 2) {
        refreshConnections( -1, 1, 1);
    } else {
        refreshConnections( -1, 1, 0 );
    }
startupTimers[ 14 ] = millis( );
   // refreshConnections( -1, 1, 0 );


}

unsigned long lastSwirlTime = 0;

int swirlCount = 42;
int spread = 13;


unsigned long schedulerTimer = 0;
unsigned long schedulerUpdateTime = 5300;

int swirled = 0;
int countsss = 0;

int probeCycle = 0;
int netUpdateRefreshCount = 0;


int clearBeforeSend = 0;

int passthroughStatus = 0;

unsigned long serialInfoTimer = 0;

unsigned long la_timer = 0;

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
        Serial.println("CORE2: wavegen.service() is being called");
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

    // REMOVED: AsyncPassthrough::task() and secondSerialHandler() moved to Core 0
    // They were causing refreshLocalConnections() to be called from Core 2
    // when handling Arduino flashing (DTR pulse detection)

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

    if ( micros( ) - schedulerTimer > schedulerUpdateTime || showLEDsCore2 == 3 ||showLEDsCore2 == 4 ||
         showLEDsCore2 == 6 && core1busy == false && core1request == 0 ) {

        if ( ( ( ( showLEDsCore2 >= 1 && loadingFile == 0 ) || showLEDsCore2 == 3 ||
                 ( swirled == 1 ) && sendAllPathsCore2 == 0 ) || showProbeLEDs != lastProbeLEDs ) &&
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

                    // while ( core1busy == true ) {
                    //     // core2busy = false;
                    // }
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
        // } else if ( inClickMenu == 0 && probeActive == 0 ) {

        //     if ( ( ( countsss > 8 && defconDisplay >= 0 ) || countsss > 10 ) &&
        //          defconDisplay != -1 ) {
        //         countsss = 0;

        //         if ( defconDisplay != -1 ) {
        //             tempDD++;

        //             if ( tempDD > 6 ) {
        //                 tempDD = 0;
        //             }
        //             defconDisplay = tempDD;
        //         }

        //         if ( defconDisplay > 5 ) {
        //             defconDisplay = 0;
        //         }
        //     }

        //     if ( readcounter > 100 ) {
        //         readcounter = 0;
        //         if ( probeCycle > 4 ) {
        //             probeCycle = 1;
        //         }
        //     }

        //     rotaryEncoderStuff( );

        } else {
            rotaryEncoderStuff( );
        }

        schedulerTimer = micros( );
        core2busy = false;
    }
}