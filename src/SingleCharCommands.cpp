/**
 * @file SingleCharCommands.cpp
 * @brief Implementation of single-character command system
 */

#include "SingleCharCommands.h"
#include "Apps.h"
#include "AsyncPassthrough.h"
#include "CH446Q.h"
#include "Commands.h"
#include "Debugs.h"
#include "FileParsing.h"
#include "FilesystemStuff.h"
#include "GraphicOverlays.h"
#include "Graphics.h"
#include "HelpDocs.h"
#include "Highlighting.h"
#include "Jerial.h" // TermControl is now part of Jerial
#include "JulseView.h"
#include "JumperlOS.h"
#include "JumperlessDefines.h"
#include "LEDs.h"
#include "LogicAnalyzer.h"
#include "MCP4728.h"
#include "MatrixState.h"
#include "Menus.h"
#include "NetManager.h"
#include "NetsToChipConnections.h"
#include "Peripherals.h"
#include "PersistentStuff.h"
#include "Probing.h"
#include "Python_Proper.h"
#include "RotaryEncoder.h"
#include "States.h"
#include "FakeGpio.h"
#include "USBfs.h"
#include "WokwiParser.h"
#include "externVars.h"
#include "hardware/gpio.h"
#include "oled.h"
#include "user_functions.h"
#include "JsonState.h"

#include <algorithm>

// Global instance
SingleCharCommands singleCharCommands;

// Global state
volatile bool inMainMenu = false;

// External variables
extern int showExtraMenu;
extern int netSlot;
extern volatile int slotChanged;
extern int dontShowMenu;
extern int firstLoop;
extern char connectFromArduino;
extern String currentCommandLine;
extern TermControl termJerial;
extern int termInInteractiveMode;
extern const int highSaturationBrightColorsCount;
extern const int highSaturationSpectrumColorsCount;
extern const int highSaturationSpectrumColors[];
extern const int highSaturationBrightColors[];

// Forward declarations for command handlers
CommandResult cmd_showCrossbarFull( char c, const String& line );
CommandResult cmd_fakeGpioDebug( char c, const String& line );

// ============================================================================
// SingleCharCommands Class Implementation
// ============================================================================

SingleCharCommands::SingleCharCommands( ) {
    commandCount = 0;
    initializeCommands( );
}

ServiceStatus SingleCharCommands::service( ) {
    // This service doesn't need periodic servicing
    // Commands are executed synchronously when input is received
    return ServiceStatus::IDLE;
}

bool SingleCharCommands::registerCommand( const Command& cmd ) {
    if ( commandCount >= MAX_COMMANDS ) {
        return false;
    }

    // Check if command already exists
    int existingIndex = findCommandIndex( cmd.trigger );
    if ( existingIndex >= 0 ) {
        // Update existing command
        commands[ existingIndex ] = cmd;
        return true;
    }

    // Add new command
    commands[ commandCount++ ] = cmd;
    sortCommands( );
    return true;
}

bool SingleCharCommands::registerCommand( char trigger, const char* shortDesc,
                                          const char* helpText, CommandCallback callback,
                                          MenuLevel level, CommandCategory category,
                                          bool showInMenu ) {
    Command cmd( trigger, shortDesc, helpText, callback, level, category, showInMenu );
    return registerCommand( cmd );
}

bool SingleCharCommands::unregisterCommand( char trigger ) {
    int index = findCommandIndex( trigger );
    if ( index < 0 ) {
        return CMD_DONT_SHOW_MENU;
    }

    // Shift remaining commands
    for ( int i = index; i < commandCount - 1; i++ ) {
        commands[ i ] = commands[ i + 1 ];
    }
    commandCount--;
    return CMD_SHOW_MENU;
}

CommandResult SingleCharCommands::executeCommand( char cmdChar, const String& commandLine ) {
    const Command* cmd = getCommand( cmdChar );
    if ( cmd == nullptr || cmd->callback == nullptr ) {
        return CMD_SHOW_MENU; // Command not found, show menu
    }

    // Execute the callback
    return cmd->callback( cmdChar, commandLine );
}

void SingleCharCommands::printMenu( int extraMenuLevel ) {
    if ( Jerial.available( ) >
         20 ) { // this is so if you dump a lot of data into the Jerial buffer, it
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
        Jerial.write( 0x0E ); // Turn ON interactive mode
        Jerial.print( "Turning on interactive mode\n\r" );
        Jerial.flush( );
        termInInteractiveMode = 1;
    } else if ( termInInteractiveMode == 1 && jumperlessConfig.display.terminal_line_buffering == 0 ) {
        Jerial.write( 0x0F ); // Turn OFF interactive mode
        Jerial.print( "Turning off interactive mode\n\r" );
        Jerial.flush( );
        termInInteractiveMode = 0;
    }
    // Jerial.print("termInInteractiveMode = ");
    // Jerial.println(termInInteractiveMode);
    // Jerial.flush();

    int shownMenuItems = 0;
    int menuItemCount[ 4 ] = { 0, 0, 0, 0 };
    int menuItemCounts[ 4 ] = { 14, 22, 37, 46 };

    if ( dontShowMenu == 0 ) {
    forceprintmenu:

        int numberOfMenuItems = menuItemCounts[ showExtraMenu ];
        float steps =
            (float)highSaturationBrightColorsCount / ( (float)numberOfMenuItems );
        // Jerial.print("steps = ");
        // Jerial.println(steps);
        int shownMenuItems = 0;
        // printSpectrumOrderedColorCube();
        cycleTerminalColor( true, steps, true, &Jerial );
        shownMenuItems += printMenuLine( "\n\n\r\t\tMenu\n\r\n\r" );
        shownMenuItems += printMenuLine( "\t'help' for docs or [command]?\n\r" );
        shownMenuItems += printMenuLine( "\n\r" );
        shownMenuItems += printMenuLine( "\tm = show this menu\n\r" );

        shownMenuItems += printMenuLine( showExtraMenu, 0, "\te = show extra options (%d)\n\r", showExtraMenu );

        //  Jerial.println();

        shownMenuItems += printMenuLine( showExtraMenu, 0, "\tn = show net list\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 1, "\tb = show bridge array\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 1, "\tc = show crossbar status\n\r" );

        // shownMenuItems += printMenuLine( showExtraMenu, 1, "\ts = show all slot files\n\r" );
        if ( showExtraMenu >= 0 ) {
            Jerial.println( );
        }

        // Jerial.println();

        shownMenuItems += printMenuLine( showExtraMenu, 2, "\t? = show firmware version\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 2, "\t' = show startup animation\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 2, "\td = set debug flags\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 2, "\tD = show debug menu\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 2, "\tl = LED brightness / test\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 0, "\t\b\b`/~ = edit / print config\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 0, "\tp = microPython REPL\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 0, "\t> = send Python formatted command\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 0, "\t/ = show filesystem / run script\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 0, "\t\b\bU/u = enable/disable USB Mass Storage\n\r" );
        // shownMenuItems += printMenuLine( showExtraMenu, 1, "\tw = enable logic analyzer\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 3, "\tX = resource status\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 1, "\tj = graphic overlay test menu\n\r" );
 

        // Jerial.print("\tu = disable USB Mass Storage drive\n\r");
        // cycleTerminalColor();

        shownMenuItems += printMenuLine( showExtraMenu, 1, "\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 1, "\tJ = print JSON state\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 1, "\tL = load JSON state (paste)\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 1, "\tY = print YAML (0/1/2)\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 1, "\tS = load YAML state (paste)\n\r" );
// shownMenuItems += printMenuLine( showExtraMenu, 1, "\n\r" );

        shownMenuItems += printMenuLine( showExtraMenu, 2, "\ty = refresh connections\n\r" );
        // shownMenuItems++;
        shownMenuItems += printMenuLine( showExtraMenu, 2, "\t< = cycle slots\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 2, "\tG = reload config.txt\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 2, "\to = load node file by slot\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 2, "\tP = PSRAM test\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 3, "\tF = cycle font\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 3, "\t_ = print micros per byte\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 2, "\t@ = scan I2C (@[sda],[scl] or @[row])\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 3, "\t$ = calibrate DACs\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 3, "\t= = dump oled frame buffer\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 2, "\tk = show oled in terminal\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 2, "\tt = OLED terminal mode\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 1, "\tR = show board LEDs\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 2, "\t* = raw speed test\n\r" );
        // shownMenuItems += printMenuLine( showExtraMenu, 3, "\t% = list all filesystem contents\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 3, "\tE = don't show this menu\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 3, "\tW = disable terminal colors\n\r" );

        if ( showExtraMenu >= 2 ) {

            // Jerial.print("\n\r");
        }
        Jerial.println( );
        // shownMenuItems += printMenuLine(showExtraMenu, 1, "\n\r");
        //  Jerial.print("\t$ = calibrate DACs\n\r");
        if ( probePowerDAC == 0 ) {
            shownMenuItems += printMenuLine( showExtraMenu, 3, "\t^ = set DAC 1 voltage\n\r" );
        } else if ( probePowerDAC == 1 ) {
            shownMenuItems += printMenuLine( showExtraMenu, 3, "\t^ = set DAC 0 voltage\n\r" );
        }
        shownMenuItems += printMenuLine( showExtraMenu, 1, "\tv = get ADC reading\n\r" );
        // Jerial.println();

        shownMenuItems += printMenuLine( showExtraMenu, 3, "\t# = print text from menu\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 2, "\tg = print gpio state\n\r" );
        // Jerial.print("\t\b\b\b\b[0-9] = run app by index\n\r");
        shownMenuItems += printMenuLine( showExtraMenu, 1, "\t. = connect oled\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 2, "\tr = reset Arduino (rt/rb)\n\r" );

        shownMenuItems += printMenuLine( showExtraMenu, 1, "\t\b\ba/A = dis/connect UART to D0/D1\n\r" );

        shownMenuItems += printMenuLine( showExtraMenu, 1, "\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 1, "\tf = load node file\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 0, "\tx = clear all connections\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 0, "\t+ = add connections\n\r" );
        shownMenuItems += printMenuLine( showExtraMenu, 0, "\t- = remove connections\n\r" );
        // Jerial.print("\te = extra menu options\n\r");
        // Jerial.println();

        Jerial.println( );

        Jerial.flush( );

        menuItemCount[ showExtraMenu ] = shownMenuItems;
    }
    /*
    int numberOfMenuItems = 0;

    // Count items at this level
    for (int i = 0; i < commandCount; i++) {
        if (commands[i].showInMenu && commands[i].menuLevel <= extraMenuLevel) {
            numberOfMenuItems++;
        }
    }

    float steps = (float)highSaturationBrightColorsCount / (float)(numberOfMenuItems + 5);
    cycleTerminalColor(true, steps, true, &Jerial);

    Jerial.println("\n\n\r\t\tMenu\n\r");
    Jerial.println("\t'help' for docs or [command]?\n\r");
    Jerial.println();

    // Group by category and print
    CommandCategory currentCat = CAT_CONNECTIONS;
    bool firstInCategory = true;

    for (int i = 0; i < commandCount; i++) {
        if (!commands[i].showInMenu || commands[i].menuLevel > extraMenuLevel) {
            continue;
        }

        // Add spacing between categories
        if (commands[i].category != currentCat) {
            Jerial.println();
            currentCat = commands[i].category;
            firstInCategory = true;
        }

        cycleTerminalColor(true, steps, true, &Jerial);
        Jerial.print("\t");
        Jerial.print(commands[i].trigger);
        Jerial.print(" = ");
        Jerial.println(commands[i].shortDesc);
    }

    Jerial.println();
    Jerial.flush();
    */
}

void SingleCharCommands::printCommandHelp( char cmdChar ) {
    const Command* cmd = getCommand( cmdChar );
    if ( cmd == nullptr ) {
        Jerial.print( "Command '" );
        Jerial.print( cmdChar );
        Jerial.println( "' not found" );
        return;
    }

    Jerial.println( "\n\r╭────────────────────────────────────╮" );
    Jerial.print( "│   Command: " );
    Jerial.print( cmd->trigger );
    Jerial.println( "                       │" );
    Jerial.println( "╰────────────────────────────────────╯\n\r" );

    Jerial.print( "Description: " );
    Jerial.println( cmd->shortDesc );
    Jerial.println( );

    if ( cmd->helpText != nullptr && cmd->helpText[ 0 ] != '\0' ) {
        Jerial.print( "Details: " );
        Jerial.println( cmd->helpText );
    }

    Jerial.println( );
}

void SingleCharCommands::printAllHelp( int category ) {
    Jerial.println( "\n\r╭────────────────────────────────────╮" );
    Jerial.println( "│        Command Reference           │" );
    Jerial.println( "╰────────────────────────────────────╯\n\r" );

    for ( int i = 0; i < commandCount; i++ ) {
        if ( category >= 0 && commands[ i ].category != category ) {
            continue;
        }

        Jerial.print( commands[ i ].trigger );
        Jerial.print( " - " );
        Jerial.println( commands[ i ].shortDesc );
    }
}

const Command* SingleCharCommands::getCommand( char trigger ) const {
    int index = findCommandIndex( trigger );
    return ( index >= 0 ) ? &commands[ index ] : nullptr;
}

int SingleCharCommands::getCommandsByCategory( CommandCategory cat, const Command** outCommands, int maxCount ) const {
    int count = 0;
    for ( int i = 0; i < commandCount && count < maxCount; i++ ) {
        if ( commands[ i ].category == cat ) {
            outCommands[ count++ ] = &commands[ i ];
        }
    }
    return count;
}

int SingleCharCommands::findCommandIndex( char trigger ) const {
    for ( int i = 0; i < commandCount; i++ ) {
        if ( commands[ i ].trigger == trigger ) {
            return i;
        }
    }
    return -1;
}

void SingleCharCommands::sortCommands( ) {
    // Simple bubble sort by category, then by trigger
    for ( int i = 0; i < commandCount - 1; i++ ) {
        for ( int j = 0; j < commandCount - i - 1; j++ ) {
            bool shouldSwap = false;

            if ( commands[ j ].category > commands[ j + 1 ].category ) {
                shouldSwap = true;
            } else if ( commands[ j ].category == commands[ j + 1 ].category &&
                        commands[ j ].trigger > commands[ j + 1 ].trigger ) {
                shouldSwap = true;
            }

            if ( shouldSwap ) {
                Command temp = commands[ j ];
                commands[ j ] = commands[ j + 1 ];
                commands[ j + 1 ] = temp;
            }
        }
    }
}

// int SingleCharCommands::printMenuLine(const char* text, int extraMenuLevel, MenuLevel requiredLevel) {
//     if (extraMenuLevel >= requiredLevel) {
//         Jerial.print(text);
//         return 1;
//     }
//     return 0;
// }

// ============================================================================
// Command Initialization
// ============================================================================

void SingleCharCommands::initializeCommands( ) {
    // Connection commands
    registerCommand( 'f', "load node file",
                     "Load connections from a node file. Prompts for file selection.",
                     cmd_loadNodeFile, MENU_STANDARD, CAT_CONNECTIONS );

    registerCommand( 'x', "clear all connections",
                     "Clears all connections and resets the board.",
                     cmd_clearConnections, MENU_BASIC, CAT_CONNECTIONS );

    registerCommand( '+', "add connections",
                     "Add new connections. Format: node1-node2,node3-node4",
                     cmd_addConnections, MENU_BASIC, CAT_CONNECTIONS );

    registerCommand( '-', "remove connections",
                     "Remove existing connections. Format: node1-node2,node3-node4",
                     cmd_removeConnections, MENU_BASIC, CAT_CONNECTIONS );

    registerCommand( 'y', "refresh connections",
                     "Reload and refresh all connections from current slot.",
                     cmd_refreshConnections, MENU_ADVANCED, CAT_CONNECTIONS );

    registerCommand( '<', "cycle slots",
                     "Cycle through saved connection slots.",
                     cmd_cycleSlots, MENU_ADVANCED, CAT_CONNECTIONS );

    registerCommand( 'o', "load node file by slot",
                     "Load a specific slot by number.",
                     cmd_loadSlot, MENU_ADVANCED, CAT_CONNECTIONS );

    registerCommand( 'W', "parse Wokwi diagram",
                     "Paste or load Wokwi diagram.json. Usage: W [slot], W [file], W [file] [slot]",
                     cmd_parseWokwi, MENU_ADVANCED, CAT_CONNECTIONS );

    // Display commands
    registerCommand( 'm', "show this menu",
                     "Display the main menu with all available commands.",
                     cmd_showMenu, MENU_BASIC, CAT_DISPLAY );

    registerCommand( 'e', "show extra options",
                     "Toggle through extra menu levels (0-3) for more commands.",
                     cmd_toggleExtraMenu, MENU_BASIC, CAT_DISPLAY );

    registerCommand( 'n', "show net list",
                     "Display current network connections and routing.",
                     cmd_showNetlist, MENU_BASIC, CAT_DISPLAY );


    registerCommand( 'b', "show bridge array",
                     "Display the internal bridge array and paths.",
                     cmd_showBridgeArray, MENU_STANDARD, CAT_DISPLAY );

    registerCommand( 'c', "show crossbar (c! live)",
                     "Display crossbar - compact view. Use c! to toggle live mode.",
                     cmd_showCrossbar, MENU_STANDARD, CAT_DISPLAY );

    registerCommand( 'C', "show crossbar (full)",
                     "Display crossbar switches - full color view with details.",
                     cmd_showCrossbarFull, MENU_STANDARD, CAT_DISPLAY );

    registerCommand( 's', "show all slot files",
                     "List all saved connection slots.",
                     cmd_showSlots, MENU_STANDARD, CAT_DISPLAY );

    registerCommand( 'Q', "query active slot",
                     "Return the currently active slot number.",
                     cmd_queryActiveSlot, MENU_STANDARD, CAT_DISPLAY );

    // Python commands
    registerCommand( 'p', "microPython REPL",
                     "Enter MicroPython REPL interactive mode.",
                     cmd_pythonREPL, MENU_BASIC, CAT_PYTHON );

    registerCommand( 'P', "PSRAM test (memory integrity + speed)",
                     "Run comprehensive PSRAM tests: size info, integrity check, speed comparison vs SRAM.",
                     cmd_psramTest, MENU_ADVANCED, CAT_HARDWARE );

    registerCommand( '>', "send Python formatted command",
                     "Execute a single Python command. Usage: > print('hello')",
                     cmd_pythonCommand, MENU_BASIC, CAT_PYTHON );

    // File system commands
    registerCommand( '/', "show filesystem / run script",
                     "Open file manager, or /filename.py to run a script directly.",
                     cmd_showFilesystem, MENU_BASIC, CAT_FILE_SYSTEM );

    registerCommand( 'U', "enable USB Mass Storage",
                     "Enable USB drive mode for file access from computer.",
                     cmd_enableUSBStorage, MENU_BASIC, CAT_FILE_SYSTEM );

    registerCommand( 'u', "disable USB Mass Storage",
                     "Disable USB drive mode.",
                     cmd_disableUSBStorage, MENU_BASIC, CAT_FILE_SYSTEM );

    registerCommand( '%', "list all filesystem contents",
                     "Recursively list all files on the filesystem.",
                     cmd_listFilesystem, MENU_DEBUG, CAT_FILE_SYSTEM );

    // Config commands
    registerCommand( '`', "edit config",
                     "Enter config editor to modify configuration.",
                     cmd_editConfig, MENU_BASIC, CAT_SETTINGS );

    registerCommand( '~', "print config",
                     "Display current configuration to serial.",
                     cmd_printConfig, MENU_BASIC, CAT_SETTINGS );

    registerCommand( 'W', "reload config.txt",
                     "Reload configuration from config.txt file.",
                     cmd_reloadConfig, MENU_ADVANCED, CAT_SETTINGS );

    // Hardware commands
    registerCommand( 'r', "reset Arduino (rt/rb)",
                     "Reset Arduino. Use 'rt' for top, 'rb' for bottom.",
                     cmd_resetArduino, MENU_ADVANCED, CAT_HARDWARE );

    registerCommand( 'a', "disconnect UART from D0/D1",
                     "Disconnect Arduino UART from D0 and D1.",
                     cmd_disconnectArduino, MENU_STANDARD, CAT_HARDWARE );

    registerCommand( 'A', "connect UART to D0/D1",
                     "Connect Arduino UART to D0 and D1.",
                     cmd_connectArduino, MENU_STANDARD, CAT_HARDWARE );

    registerCommand( 'v', "get ADC reading",
                     "Read voltage from ADC. Usage: v[0-4] or vi for current.",
                     cmd_readADC, MENU_STANDARD, CAT_HARDWARE );

    registerCommand( '^', "set DAC voltage",
                     "Set DAC output voltage. Usage: ^ followed by voltage.",
                     cmd_setDAC, MENU_DEBUG, CAT_HARDWARE );

    registerCommand( '@', "scan I2C",
                     "Scan for I2C devices. Usage: @[row] or @[sda],[scl]",
                     cmd_i2cScan, MENU_ADVANCED, CAT_HARDWARE );

    registerCommand( '$', "calibrate DACs",
                     "Run DAC calibration routine.",
                     cmd_calibrateDACs, MENU_DEBUG, CAT_HARDWARE );

    // Debug commands
    registerCommand( '?', "show firmware version",
                     "Display current firmware version.",
                     cmd_showVersion, MENU_ADVANCED, CAT_DEBUG );

    registerCommand( 'd', "set debug flags",
                     "Open debug flags menu.",
                     cmd_setDebugFlags, MENU_ADVANCED, CAT_DEBUG );

    registerCommand( 'X', "resource status",
                     "Show system resource allocation and status.",
                     cmd_resourceStatus, MENU_DEBUG, CAT_DEBUG );

    registerCommand( 'g', "print gpio state",
                     "Display state of all GPIO pins.",
                     cmd_gpioState, MENU_ADVANCED, CAT_DEBUG );

    registerCommand( 'Z', "USB debug menu",
                     "Open USB debugging options menu.",
                     cmd_usbDebugMenu, MENU_DEBUG, CAT_DEBUG );

    registerCommand( ';', "print wire status",
                     "Print wire status to terminal.",
                     cmd_printWireStatus, MENU_DEBUG, CAT_DEBUG );

    registerCommand( 'H', "fakeGPIO debug (live)",
                     "Live-updating FakeGPIO status showing TDM voltages and pin states.",
                     cmd_fakeGpioDebug, MENU_ADVANCED, CAT_DEBUG );

    registerCommand( 'D', "status diagnostics menu",
                     "Interactive status & diagnostics menu with arrow key navigation.",
                     cmd_statusDiagnosticsMenu, MENU_STANDARD, CAT_DEBUG );

    // Settings commands
    registerCommand( 'l', "LED brightness / test",
                     "Adjust LED brightness or run LED test.",
                     cmd_ledBrightness, MENU_ADVANCED, CAT_SETTINGS );

    registerCommand( '.', "connect oled",
                     "Connect/disconnect OLED display.",
                     cmd_toggleOLED, MENU_STANDARD, CAT_SETTINGS );

    registerCommand( 'G', "disable terminal colors",
                     "Toggle terminal color output on/off.",
                     cmd_toggleTerminalColors, MENU_DEBUG, CAT_SETTINGS );

    registerCommand( 'E', "don't show this menu",
                     "Toggle automatic menu display.",
                     cmd_dontShowMenu, MENU_DEBUG, CAT_SETTINGS );

    registerCommand( 'k', "show oled in terminal",
                     "Toggle OLED mirroring to terminal.",
                     cmd_oledInTerminal, MENU_ADVANCED, CAT_SETTINGS );

    registerCommand( 'F', "cycle font",
                     "Cycle through available OLED fonts.",
                     cmd_cycleFont, MENU_DEBUG, CAT_SETTINGS );

    // // App/Special mode commands
    // registerCommand( 'L', "enable logic analyzer",
    //                  "Enable logic analyzer mode.",
    //                  cmd_logicAnalyzer, MENU_STANDARD, CAT_APPS );
    registerCommand( 'J', "show JSON state",
                     "Display state as JSON. J = full; J power|nets|gpio|overlays = that section only.",
                     cmd_showJsonState, MENU_STANDARD, CAT_DISPLAY );

    registerCommand( 'L', "load JSON state",
                     "Load state from JSON. L = full (paste all); L overlays|power|... = paste that section only.",
                     cmd_loadJsonState, MENU_STANDARD, CAT_CONNECTIONS );

    registerCommand( 'R', "show board LEDs",
                     "Display board LEDs in terminal.",
                     cmd_showBoardLEDs, MENU_ADVANCED, CAT_APPS );

    registerCommand( '\'', "show startup animation",
                     "Play the startup animation.",
                     cmd_startupAnimation, MENU_ADVANCED, CAT_APPS );

    // Advanced commands
    // registerCommand( 'J', "test States system",
    //                  "Test the new States system. Usage: J 1-2,3-4",
    //                  cmd_testStates, MENU_DEBUG, CAT_ADVANCED );

    registerCommand( 'Y', "print YAML (Y0=plain Y1=colored hex Y2=colored blocks)",
                     "Display current state in YAML format.",
                     cmd_printYAML, MENU_DEBUG, CAT_ADVANCED );

    registerCommand( 'S', "load YAML state",
                     "Paste YAML state (end with empty line). Same format as Y output.",
                     cmd_loadYAMLState, MENU_STANDARD, CAT_CONNECTIONS );

    registerCommand( '*', "raw speed test",
                     "Run raw crossbar switching speed test.",
                     cmd_rawSpeedTest, MENU_DEBUG, CAT_ADVANCED );

    // registerCommand( 'j', "print color spectrum",
    //                  "Display color spectrum codes.",
    //                  cmd_printColorSpectrum, MENU_DEBUG, CAT_ADVANCED );

    registerCommand( '=', "dump oled frame buffer",
                     "Dump OLED frame buffer contents.",
                     cmd_dumpOLED, MENU_DEBUG, CAT_ADVANCED );

    registerCommand( '_', "print micros per byte",
                     "Display timing information.",
                     cmd_printMicrosPerByte, MENU_DEBUG, CAT_ADVANCED );

    registerCommand( '#', "print text from menu",
                     "Print text from menu system.",
                     cmd_printTextFromMenu, MENU_DEBUG, CAT_ADVANCED );

    registerCommand( 'q', "DMX Serial mode",
                     "Enter DMX Serial application mode.",
                     cmd_dmxSerial, MENU_DEBUG, CAT_APPS );

    registerCommand( 'z', "user function",
                     "Execute user-defined function.",
                     cmd_userFunction, MENU_DEBUG, CAT_ADVANCED );

    registerCommand( '|', "eratta clear GPIO",
                     "Clear GPIO eratta workaround.",
                     cmd_erattaClear, MENU_DEBUG, CAT_ADVANCED );

    registerCommand( 'w', "wavegen",
                     "Wavegen test.",
                     cmd_wavegen, MENU_DEBUG, CAT_ADVANCED );

    registerCommand( 't', "OLED terminal mode",
                     "Interactive OLED terminal - type text to display on OLED. Press ESC to exit, 'c' to clear.",
                     cmd_printTextFromTerminal, MENU_ADVANCED, CAT_SETTINGS );
    registerCommand( 'T', "show switch position",
                     "Show switch position.",
                     cmd_showSwitchPosition, MENU_DEBUG, CAT_ADVANCED );

    registerCommand( 'j', "Test overlay",
                     "Test overlay.",
                     cmd_testOverlay, MENU_DEBUG, CAT_ADVANCED );
}

CommandResult cmd_showSwitchPosition( char c, const String& line ) {

    unsigned long currentTime = millis();

    for (int j = 0; j < 10; j++) {
    for (int i = 0; i < 10; i++) {
        showSwitchPosition(i, " ", 0x000000, 0x000000);
        showLEDsCore2 = 2;
        delay(100);
    }
    for (int i = 9; i >= 0; i--) {
        showSwitchPosition(i, "Fuck", 0x000000, 0x000000);
        showLEDsCore2 = 2;
        delay(100);
    }
}
        
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_testOverlay( char c, const String& line ) {
    graphicOverlayState.debugMenu();
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_printTextFromTerminal( char c, const String& line ) {
    // Interactive OLED terminal mode
    Jerial.println( "\n\r╭────────────────────────────────────╮" );
    Jerial.println( "│     OLED Terminal Mode             │" );
    Jerial.println( "├────────────────────────────────────┤" );
    Jerial.println( "│ Type text to display on OLED       │" );
    Jerial.println( "│ Press Ctrl+Q to exit               │" );
    Jerial.println( "│ Ctrl+A to clear display            │" );
    Jerial.println( "╰────────────────────────────────────╯\n\r" );
    
    if (!oled.isConnected()) {
        Jerial.println( "✗ OLED not connected" );
        Jerial.println( "  Use '.' command to connect OLED first" );
        return CMD_DONT_SHOW_MENU;
    }
    Serial.write(0x0E); // interactive mode on
    delay(10);

    // Clear OLED and prepare for text
    OLEDOut.clear();
    OLEDOut.println( "OLED Terminal" );
    OLEDOut.println( "Type below:" );
    
    Jerial.println( "Ready. Type your text:" );
    
    String inputLine = "";
    bool exitMode = false;
    
    while (!exitMode) {
        if (Jerial.available() > 0) {
            char ch = Jerial.read();
            
            // Check for exit conditions
            if (ch == 17) {  // Ctrl+Q 
                exitMode = true;
                Jerial.println( "\n\r✓ Exiting OLED terminal mode" );
                break;
            }
            
            // Handle special commands
            if (ch == 0x01) {
                // Clear display
                OLEDOut.clear();
                Jerial.println( "[Display cleared]" );
                continue;
            }


            

            OLEDOut.write(ch);

            if (ch == '\n') {
                Serial.println(" -");
                Serial.flush();
                //continue;
            } else if (ch == ' ' || ch == '\t' || ch == 20) {
                Serial.print(" ");
                Serial.flush();
            }else {
                Serial.print(ch);
                Serial.flush();
            }
            // Echo to serial
  
            
            // Jerial.write(ch);
            // Jerial.flush();
            
            // Handle newline
            // if (ch == '\n' || ch == '\r') {
            //     if (inputLine.length() > 0) {
            //         // Send line to OLED
            //         OLEDOut.println(inputLine);
            //         inputLine = "";
            //     }
            //     continue;
            // }
            
            // Handle backspace

            
            // // Add character to line buffer
            // if (ch >= 32 && ch < 127) {  // Printable characters
            //     inputLine += ch;
                
            //     // Auto-send if line gets too long
            //     if (inputLine.length() >= 21) {  // Max chars per line on OLED
            //         OLEDOut.println(inputLine);
            //         Jerial.println();  // Newline on serial
            //         inputLine = "";
            //     }
            // }
        }
        
        // Small delay to prevent busy-waiting
        delay(1);
    }
    Serial.write(0x0F); // interactive mode off
    delay(10);
    return CMD_SHOW_MENU;
}
// ============================================================================
// Command Callback Implementations
// ============================================================================
// These are the actual functions that get called when a command is executed.
// Many of these will just call existing functions from other files.

// Connection commands
CommandResult cmd_clearConnections( char c, const String& line ) {
    // RESETPIN is a macro defined in JumperlessDefines.h
    pinMode( RESETPIN, OUTPUT );
    digitalWrite( RESETPIN, HIGH );
    delay( 6 );
    refreshPaths( );
    clearAllNTCC( );
    clearNodeFile( netSlot, 0 );
    refreshConnections( -1, 1, 1 );
    digitalWrite( RESETPIN, LOW );
    Jerial.println( "Cleared all connections" );
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_addConnections( char c, const String& line ) {
    // Use source 3 if we have a complete command line (from line buffering or injection)
    // Otherwise use source 0 to read interactively from Jerial
    // Serial.println("Adding connections");
    int source = ( currentCommandLine.length( ) > 1 ) ? 3 : 0;
    // Serial.println("source = ");
    // Serial.print(source);
    // Serial.print("currentCommandLine = ");
    // Serial.println(currentCommandLine);
    // unsigned long startTime = micros();
    readStringFromSerial( source, 0 );
    // After reading connections, they need to be loaded
    firstLoop = 0; // Prevent first-loop logic
    // unsigned long endTime = micros();
    // unsigned long duration = endTime - startTime;
    // Serial.print("Time taken: ");
    // Serial.print(duration);
    // Serial.println(" us");
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_removeConnections( char c, const String& line ) {
    // Use source 3 if we have a complete command line (from line buffering or injection)
    // Otherwise use source 0 to read interactively from Jerial
    int source = ( currentCommandLine.length( ) > 1 ) ? 3 : 0;
    readStringFromSerial( source, 1 );
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_loadNodeFile( char c, const String& line ) {
    extern volatile int probeActive;
    extern int readInNodesArduino;
    extern int serSource;
    extern volatile int rotaryEncoderMode;
    extern int input;

    probeActive = 1;
    readInNodesArduino = 1;
    // Jerial.println("Loading node file...");
    // Jerial.println(line);

    // savePreformattedNodeFile handles parsing to RAM, marking dirty, and refresh
    // No need to call refreshConnections again - that would do the work twice!
    savePreformattedNodeFile( serSource, netSlot, rotaryEncoderMode, line );

    // Validation happens inside savePreformattedNodeFile via refreshLocalConnections
    // which calls the same validation logic. Don't duplicate the work here.

    input = ' ';
    probeActive = 0;

    if ( connectFromArduino != '\0' ) {
        connectFromArduino = '\0';
        readInNodesArduino = 0;
        return CMD_DONT_SHOW_MENU;
    }

    connectFromArduino = '\0';
    readInNodesArduino = 0;
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_refreshConnections( char c, const String& line ) {
    return CMD_LOAD_FILE;
}

CommandResult cmd_cycleSlots( char c, const String& line ) {
    extern volatile int slotPreview;
    if ( netSlot == 7 ) {
        netSlot = 0;
    } else {
        netSlot++;
    }
    Jerial.print( "Slot " );
    Jerial.println( netSlot );

    // Send slot change notification for app synchronization
    Jerial.print( "SLOT_CHANGED:" );
    Jerial.println( netSlot );
    Jerial.flush( );

    slotPreview = netSlot;
    slotChanged = 1;
    return CMD_LOAD_FILE;
}

CommandResult cmd_loadSlot( char c, const String& line ) {
    extern volatile int rotaryEncoderMode;
    inputNodeFileList( rotaryEncoderMode );
    extern volatile int showLEDsCore2;
    showLEDsCore2 = -1;
    return CMD_LOAD_FILE;
}

CommandResult cmd_parseWokwi( char c, const String& line ) {
    // Parse Wokwi diagram.json and save to slot
    // Format:
    //   "W"              - Wait for user to paste JSON, save to current slot
    //   "W [slot]"       - Wait for paste, save to specified slot
    //   "W [filename]"   - Load from file, save to current slot
    //   "W [filename] [slot]" - Load from file, save to specified slot

    String filename = "";
    // Use ACTIVE slot (what's loaded), not netSlot (which might be preview/cycling)
    SlotManager& mgr = SlotManager::getInstance( );
    int slotNum = mgr.getActiveSlot( ); // Default to currently LOADED slot
    bool waitForPaste = true;
    bool fromApp = false; // True if JSON was pasted immediately after W

    // Clear any stale command line data to prevent parameter pollution
    extern String currentCommandLine;
    if ( currentCommandLine.length( ) > 10 || currentCommandLine.indexOf( '{' ) >= 0 ) {
        // Command line contains JSON or is suspiciously long - clear it
        currentCommandLine = String( (char)c ); // Reset to just the W command
    }

    if ( debugFP ) {
        Jerial.println( "◆ W command: netSlot=" + String( netSlot ) +
                        ", activeSlot=" + String( slotNum ) +
                        ", previewMode=" + String( mgr.isPreviewMode( ) ? "YES" : "NO" ) );
        Jerial.println( "  Input line: '" + line + "' (length=" + String( line.length( ) ) + ")" );
    }

    // Detect if JSON was pasted immediately (from app) vs interactive user
    // If line contains JSON or Jerial data available within 100ms, it's from app
    if ( line.length( ) > 1 && ( line.indexOf( '{' ) > 0 || line.indexOf( '[' ) > 0 ) ) {
        fromApp = true;
        if ( debugFP ) {
            Jerial.println( "  Detected app paste (JSON in line)" );
        }
    } else {
        // Check if more data is coming soon (app sends it all at once)
        delay( 100 );
        if ( Jerial.available( ) > 0 ) {
            fromApp = true;
            if ( debugFP ) {
                Jerial.println( "  Detected app paste (data available after 100ms)" );
            }
        }
    }

    // Parse command line if provided
    // IMPORTANT: When user pastes JSON after "W", the line may be "W{...}" or contain garbage
    // We need to detect if params starts with '{' or other non-slot characters
    // and ignore them (they're part of the JSON paste, not command args)
    if ( line.length( ) > 1 ) {
        String params = line.substring( 1 );
        params.trim( );

        if ( debugFP ) {
            Jerial.println( "  Parsing params: '" + params + "' (length=" + String( params.length( ) ) +
                            ", first char='" + ( params.length( ) > 0 ? String( params[ 0 ] ) : "" ) + "')" );
        }

        // Skip JSON content: if params starts with '{', '[', or '"', it's JSON not args
        if ( params.length( ) > 0 && ( params[ 0 ] == '{' || params[ 0 ] == '[' || params[ 0 ] == '"' ) ) {
            if ( debugFP ) {
                Jerial.println( "  → Detected JSON in params, ignoring - using slot " + String( slotNum ) );
            }
            // Don't parse parameters - user is pasting JSON
            // Keep slotNum at default (activeSlot)
        }
        // Check if parameters provided
        else if ( params.length( ) > 0 ) {
            int spaceIdx = params.indexOf( ' ' );
            if ( spaceIdx > 0 ) {
                // Two parameters: filename and slot
                filename = params.substring( 0, spaceIdx );
                filename.trim( );
                String slotStr = params.substring( spaceIdx + 1 );
                slotStr.trim( );
                slotNum = slotStr.toInt( );
                waitForPaste = false;
                if ( debugFP ) {
                    Jerial.println( "  Parsed: filename='" + filename + "', slot=" + String( slotNum ) );
                }
            } else {
                // Single parameter: filename or slot number
                if ( params[ 0 ] >= '0' && params[ 0 ] <= '9' ) {
                    // It's a slot number - wait for paste
                    slotNum = params.toInt( );
                    waitForPaste = true;
                    if ( debugFP ) {
                        Jerial.println( "  Parsed slot number: " + String( slotNum ) );
                    }
                } else {
                    // It's a filename
                    filename = params;
                    waitForPaste = false;
                    if ( debugFP ) {
                        Jerial.println( "  Parsed filename: '" + filename + "'" );
                    }
                }
            }
        }
    }

    if ( debugFP ) {
        Jerial.println( "  Final target slot: " + String( slotNum ) +
                        " (waitForPaste=" + String( waitForPaste ) +
                        ", filename='" + filename + "', fromApp=" + String( fromApp ) + ")" );
    }

    // Validate slot number
    if ( slotNum < 0 || slotNum >= NUM_SLOTS ) {
        Jerial.println( "◇ Invalid slot number: " + String( slotNum ) );
        return CMD_SHOW_MENU;
    }

    String errorMsg;
    bool success = false;

    if ( waitForPaste ) {
        // Only show prompts if this is interactive (not from app)
        if ( !fromApp ) {
            Jerial.println( "◆ Paste Wokwi diagram.json content (ends with '}')\n" );
            Jerial.println( "  Target slot: " + String( slotNum ) );
        } else if ( debugFP ) {
            Jerial.println( "  Target slot: " + String( slotNum ) + " (app mode - no prompt)" );
        }

        // Wait for input (only show help prompt if interactive)
        unsigned long humanTime = millis( );
        int shown = 0;
        while ( Jerial.available( ) == 0 ) {
            if ( !fromApp && millis( ) - humanTime == 2000 && shown == 0 ) {
                Jerial.println( "\n  Waiting for JSON paste..." );
                Jerial.println( "  (Copy from Wokwi editor: diagram.json tab)" );
                shown = 1;
            }
        }

        // Read pasted JSON content
        String jsonContent = "";
        jsonContent.reserve( 8192 ); // Pre-allocate to avoid fragmentation

        unsigned long lastCharTime = millis( );
        bool foundOpenBrace = false;
        int braceCount = 0;

        while ( true ) {
            if ( Jerial.available( ) > 0 ) {
                char c = Jerial.read( );
                jsonContent += c;
                lastCharTime = millis( );

                // Track braces to detect complete JSON
                if ( c == '{' ) {
                    foundOpenBrace = true;
                    braceCount++;
                } else if ( c == '}' ) {
                    braceCount--;
                    // If we found opening brace and brace count is back to 0, we're done
                    if ( foundOpenBrace && braceCount == 0 ) {
                        if ( !fromApp || debugFP ) {
                            Jerial.print( "." );
                        }
                        delay( 100 ); // Allow any trailing characters
                        // Consume any trailing whitespace/newlines
                        while ( Jerial.available( ) > 0 ) {
                            char trailing = Jerial.read( );
                            if ( trailing == '\n' || trailing == '\r' || trailing == ' ' ) {
                                continue;
                            } else {
                                jsonContent += trailing; // Might be more JSON
                            }
                        }
                        break;
                    }
                }

                // Show progress every 256 bytes (only if interactive or debug)
                if ( !fromApp || debugFP ) {
                    if ( jsonContent.length( ) % 256 == 0 ) {
                        Jerial.print( "." );
                    }
                }
            } else {
                // No data available
                if ( jsonContent.length( ) > 0 ) {
                    // Check timeout (500ms after last character)
                    if ( millis( ) - lastCharTime > 500 ) {
                        if ( debugFP ) {
                            Jerial.println( "\n  Timeout: 500ms since last character" );
                        }
                        break;
                    }
                    delay( 10 ); // Small delay waiting for more data
                } else {
                    delay( 10 ); // Waiting for first character
                }
            }

            // Safety: max 32KB
            if ( jsonContent.length( ) > 32000 ) {
                Jerial.println( "\n◇ Warning: JSON too large (>32KB), truncating" );
                break;
            }
        }

        jsonContent.trim( );

        // Only show "Received" message if interactive or debug
        if ( !fromApp ) {
            Jerial.println( "\n◆ Received " + String( jsonContent.length( ) ) + " bytes" );
        } else if ( debugFP ) {
            Jerial.println( "◆ Received " + String( jsonContent.length( ) ) + " bytes (from app)" );
        }

        // Debug: Show what we received if debugFP is on
        if ( debugFP ) {
            Jerial.println( "◆ First 200 chars:" );
            Jerial.println( jsonContent.substring( 0, 200 ) );
        }

        // Parse into target slot WITHOUT affecting active state or hardware
        // mgr already declared earlier in function
        int currentActiveSlot = mgr.getActiveSlot( );
        bool isActiveSlot = ( currentActiveSlot == slotNum );

        if ( isActiveSlot ) {
            // ========== ACTIVE SLOT: Parse directly and apply to hardware ==========
            // Only clear connections, not power settings to prevent LED flicker
            // When we avoid clearing power, the LEDs won't blink to 0V between updates
            mgr.getActiveState( ).connections.clear( );
            // Power settings remain unchanged unless the parser explicitly updates them
            // This prevents LEDs from briefly showing 0V on rails during updates

            // Parse directly into active state
            if ( parseWokwiDiagram( jsonContent, mgr.getActiveState( ), slotNum, errorMsg, fromApp ) ) {
                if ( mgr.saveSlot( slotNum, errorMsg ) ) {
                    if ( !fromApp ) {
                        Jerial.println( "  ✓ Saved and applied to slot " + String( slotNum ) );
                    } else if ( debugFP ) {
                        Jerial.println( "  ✓ Saved and applied to slot " + String( slotNum ) + " (app mode)" );
                    }
                    success = true;

                    // Apply to hardware
                    if ( !fromApp || debugFP ) {
                        Jerial.println( "  ↻ Applying to hardware..." );
                    }
                    if ( mgr.loadSlot( slotNum, errorMsg ) ) {
                        if ( !fromApp || debugFP ) {
                            Jerial.println( "  ✓ Applied to hardware" );
                        }
                    } else {
                        Jerial.println( "  ✗ Failed to apply: " + errorMsg );
                    }
                } else {
                    Jerial.println( "  ✗ Failed to save: " + errorMsg );
                }
            } else {
                Jerial.println( "  ✗ Parse error: " + errorMsg );
            }
        } else {
            // ========== INACTIVE SLOT: Parse directly to file (ZERO-COPY) ==========
            // This avoids creating ANY JumperlessState objects (50KB each)
            // parseWokwiDiagramDirectToFile builds minimal YAML and writes to file
            if ( parseWokwiDiagramDirectToFile( jsonContent, slotNum, errorMsg, fromApp ) ) {
                if ( !fromApp ) {
                    Jerial.println( "  ✓ Saved to inactive slot " + String( slotNum ) );
                } else if ( debugFP ) {
                    Jerial.println( "  ✓ Saved to inactive slot " + String( slotNum ) + " (no hardware change)" );
                }
                success = true;
            } else {
                Jerial.println( "  ✗ Parse error: " + errorMsg );
            }
        }

    } else {
        // Load from file
        Jerial.println( "◆ Parsing Wokwi diagram: " + filename );
        Jerial.println( "  Target slot: " + String( slotNum ) );

        extern bool parseWokwiDiagramFromFile( const String&, int, String& );
        success = parseWokwiDiagramFromFile( filename, slotNum, errorMsg );

        if ( success ) {
            Jerial.println( "◆ Wokwi diagram successfully converted and saved!" );
        } else {
            Jerial.println( "◇ Failed to parse Wokwi diagram: " + errorMsg );
        }
    }

    // Only show hint if interactive and slot needs to be cycled to
    if ( !fromApp && success && slotNum != netSlot ) {
        Jerial.println( "  Use '<' to cycle to slot " + String( slotNum ) + " to activate it" );
    }

    return CMD_DONT_SHOW_MENU;
}

// Display commands
CommandResult cmd_showMenu( char c, const String& line ) {
    return CMD_SHOW_MENU;
}

CommandResult cmd_toggleExtraMenu( char c, const String& line ) {
    showExtraMenu++;
    if ( showExtraMenu > 3 ) {
        showExtraMenu = 0;
    }
    return CMD_SHOW_MENU;
}

CommandResult cmd_showNetlist( char c, const String& line ) {
    extern volatile int core1passthrough;
    couldntFindPath( 1 );
  
    Jerial.print( "\n\n\rnetlist\n\r" );
    listNets( anythingInteractiveConnected( -1 ) );
    return CMD_SHOW_MENU;
}

// Read optional argument from serial (e.g. " overlays" after "J") when line buffering is off
static String readOptionalArgFromSerial( unsigned int timeoutMs = 50 ) {
    String arg;
    unsigned long start = millis( );
    while ( millis( ) - start < timeoutMs ) {
        while ( Jerial.available( ) > 0 ) {
            char ch = (char) Jerial.read( );
            if ( ch == '\n' || ch == '\r' ) return arg;
            arg += ch;
        }
        delay( 1 );
    }
    return arg;
}

CommandResult cmd_showJsonState( char c, const String& line ) {
    // Optional section: J or J power | J nets | J gpio | J overlays
    // line often has only "J" when not using line buffering, so read from serial
    String section;
    if ( line.length( ) > 1 ) {
        section = line.substring( 1 );
        section.trim( );
    }
    if ( section.length( ) == 0 )
        section = readOptionalArgFromSerial( );
    section.trim( );
    const char* sectionPtr = ( section.length( ) > 0 ) ? section.c_str( ) : nullptr;
    Jerial.print( "\n\n\r" );
    Jerial.print( JsonState::getJumperlessStateJSON( sectionPtr ) );
    Jerial.print( "\n\r" );
    return CMD_SHOW_MENU;
}

CommandResult cmd_loadJsonState( char c, const String& line ) {
    // Optional section: L or L overlays | L power | etc. — load only that section (partial update)
    // line often has only "L" when not using line buffering, so read from serial
    String section;
    if ( line.length( ) > 1 ) {
        section = line.substring( 1 );
        section.trim( );
    }
    // if ( section.length( ) == 0 )
    //     section = readOptionalArgFromSerial( );
    // section.trim( );
    bool partialLoad = ( section.length( ) > 0 );
    if ( partialLoad )
        Jerial.print( "\n\rPaste JSON for '" + section + "' (end with empty line):\n\r" );
    else
        Jerial.print( "\n\rPaste JSON state (end with empty line):\n\r" );

    String jsonBuffer;
    jsonBuffer.reserve(8192);

    // Read lines until empty line or timeout
    unsigned long startTime = millis();
    const unsigned long timeout = 30000; // 30 second timeout

    while (millis() - startTime < timeout) {
        if (Serial.available()) {
            String inputLine = Serial.readStringUntil('\n');
            inputLine.trim();

            // Empty line signals end of input
            if (inputLine.length() == 0 && jsonBuffer.length() > 0) {
                break;
            }

            jsonBuffer += inputLine + "\n";
            startTime = millis(); // Reset timeout on each line
        }
        delay(1);
    }

    if (jsonBuffer.length() == 0) {
        Jerial.print( "\r\nNo JSON received\n\r" );
        return CMD_SHOW_MENU;
    }

    // Normalize pasted JSON so the parser sees an object
    jsonBuffer.trim();
    if ( jsonBuffer.length( ) > 0 && jsonBuffer.charAt( 0 ) != '{' ) {
        if ( section.equalsIgnoreCase( "overlays" ) && jsonBuffer.charAt( 0 ) == '[' )
            jsonBuffer = "{\"overlays\":" + jsonBuffer + "}";
        else if ( jsonBuffer.charAt( 0 ) == '"' )
            jsonBuffer = "{" + jsonBuffer + "}";
    }

    Jerial.print( "\r\nApplying state...\n\r" );

    // Partial load: only update the given section, do not clear connections
    bool success = JsonStateParser::applyJSONState( jsonBuffer, !partialLoad );

    if (success) {
        Jerial.print( "State applied successfully!\n\r" );
    } else {
        Jerial.print( "Error: " );
        Jerial.print( JsonStateParser::getLastError() );
        Jerial.print( "\n\r" );
    }

    return CMD_SHOW_MENU;
}

CommandResult cmd_showBridgeArray( char c, const String& line ) {
    int showDupes = 1;
    delay(2);
    if ( Jerial.available( ) > 0 ) {
        char in = Jerial.read( );
        if ( in == '0' ) {
            showDupes = 0;
        } else if ( in == '2' ) {
            showDupes = 2;
        }
    }

    Jerial.print( "\n\rpathDuplicates: " );
    Jerial.println( jumperlessConfig.routing.stack_paths );
    Jerial.print( "dacDuplicates: " );
    Jerial.println( jumperlessConfig.routing.stack_dacs );
    Jerial.print( "railsDuplicates: " );
    Jerial.println( jumperlessConfig.routing.stack_rails );
    Jerial.print( "railPriority: " );
    Jerial.println( jumperlessConfig.routing.rail_priority );
    couldntFindPath( 1 );
    Jerial.print( "\n\rBridge Array\n\r" );
    printBridgeArray( );
    Jerial.print( "\n\n\n\rPaths\n\r" );
    printPathsCompact( showDupes );
    Jerial.print( "\n\n\rChip Status\n\r" );
    printChipStatus( );
    Jerial.print( "\n\n\r" );
    return CMD_SHOW_MENU;
}

CommandResult cmd_showCrossbar( char c, const String& line ) {
    // Check if user typed 'c!' to toggle live mode
    // Need to peek at serial buffer for the '!' since command system only consumed 'c'
    
    // Small delay to let the '!' arrive if user typed quickly
    delay( 10 );
    
    if ( Jerial.available( ) > 0 ) {
        char nextChar = Jerial.peek( );
        if ( nextChar == '!' ) {
            Jerial.read( );  // Consume the '!'
            extern bool liveCrossbarEnabled;
            setLiveCrossbarEnabled( !liveCrossbarEnabled );
            return CMD_SHOW_MENU;
        }
    }
    
    // Otherwise show compact crossbar view
    printChipStateArrayColorCompact( 12 , '.');
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_showCrossbarFull( char c, const String& line ) {
    printChipStateArrayColor( );  // Full detailed view with 3-char symbols
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_showSlots( char c, const String& line ) {
    printSlots( -1 );
    return CMD_SHOW_MENU;
}

CommandResult cmd_queryActiveSlot( char c, const String& line ) {
    SlotManager& mgr = SlotManager::getInstance( );
    int activeSlot = mgr.getActiveSlot( );

    // Output in a format easy for the app to parse
    Jerial.print( "ACTIVE_SLOT:" );
    Jerial.println( activeSlot );
    Jerial.flush( );

    return CMD_DONT_SHOW_MENU;
}

// Python commands
CommandResult cmd_pythonREPL( char c, const String& line ) {
    // Set Python output streams to Jerial for the interactive REPL with correct interrupt char
    // global_mp_stream is already declared in Python_Proper.h (included above)
    extern void setGlobalStreamWithInterrupt(Stream *stream);  // From Python_Proper.cpp
    setGlobalStreamWithInterrupt(&Jerial);

    changeTerminalColor(208, true, &Jerial);
    Jerial.print( "\n\n\rThere's now a better way to do this! ");
    changeTerminalColor(214, true, &Jerial);
    Jerial.println( "Go to:\n\r" );

    changeTerminalColor(190, true, &Jerial);
    Jerial.println( "https://viper-ide.org/\n\r");

    changeTerminalColor(112, true, &Jerial);
    Jerial.println( "and connect to the 3rd Jumperless serial port.\n\n\n\r");

    delay(1000);

    enterMicroPythonREPL( );
    refreshConnections( -1, 1, 1 );
    Jerial.write( 0x0F );
    extern int termInInteractiveMode;
    termInInteractiveMode = 0;
    Jerial.flush( );
    return CMD_SHOW_MENU;
}

CommandResult cmd_psramTest( char c, const String& line ) {
    Serial.println( "\n=== PSRAM Test Suite ===" );
    Serial.flush();
    Serial.println( "Config psram_installed: " + String( jumperlessConfig.hardware.psram_installed ) );
    Serial.flush();
    
    // Show regular SRAM info first (this is always safe)
    Serial.println( "\n--- SRAM Info ---" );
    Serial.println( "SRAM Total: " + String( rp2040.getTotalHeap() / 1024 ) + " KB" );
    Serial.println( "SRAM Free: " + String( rp2040.getFreeHeap() / 1024 ) + " KB" );
    Serial.flush();
    
    // Try to get PSRAM size - this may crash if no PSRAM is present
    Serial.println( "\n--- PSRAM Detection ---" );
    Serial.println( "Checking PSRAM size..." );
    Serial.flush();
    
    size_t psramSize = rp2040.getPSRAMSize();
    Serial.println( "PSRAM Chip Size: " + String( psramSize / 1024 / 1024 ) + " MB (" + String( psramSize ) + " bytes)" );
    Serial.flush();
    
    if ( psramSize == 0 ) {
        Serial.println( "\nNo PSRAM detected!" );
        Serial.println( "If you have installed the PSRAM mod, check:" );
        Serial.println( "  - PSRAM chip is properly soldered" );
        Serial.println( "  - CS pin (GPIO 19) connection" );
        Serial.println( "  - Power and ground connections" );
        Serial.flush();
        return CMD_DONT_SHOW_MENU;
    }
    
    // PSRAM detected - get heap info
    Serial.println( "Getting PSRAM heap info..." );
    Serial.flush();
    
    size_t psramTotal = rp2040.getTotalPSRAMHeap();
    size_t psramUsed = rp2040.getUsedPSRAMHeap();
    size_t psramFree = rp2040.getFreePSRAMHeap();
    
    Serial.println( "\n--- PSRAM Info ---" );
    Serial.println( "PSRAM Heap Total: " + String( psramTotal / 1024 ) + " KB" );
    Serial.println( "PSRAM Heap Used: " + String( psramUsed / 1024 ) + " KB" );
    Serial.println( "PSRAM Heap Free: " + String( psramFree / 1024 ) + " KB" );
    Serial.flush();
    
    // Memory integrity test - start small
    Serial.println( "\n--- Memory Integrity Test ---" );
    Serial.flush();
    
    // Try a small allocation first
    Serial.println( "Testing small allocation (256 bytes)..." );
    Serial.flush();
    
    uint32_t* testSmall = (uint32_t*)pmalloc( 256 );
    if ( testSmall == nullptr ) {
        Serial.println( "ERROR: Small pmalloc() failed!" );
        Serial.flush();
        return CMD_DONT_SHOW_MENU;
    }
    
    // Quick write/read test
    testSmall[0] = 0xDEADBEEF;
    testSmall[1] = 0xCAFEBABE;
    Serial.flush();
    
    if ( testSmall[0] != 0xDEADBEEF || testSmall[1] != 0xCAFEBABE ) {
        Serial.println( "ERROR: Basic read/write test FAILED!" );
        Serial.println( "  Wrote: 0xDEADBEEF, Read: 0x" + String( testSmall[0], HEX ) );
        Serial.println( "  Wrote: 0xCAFEBABE, Read: 0x" + String( testSmall[1], HEX ) );
        free( testSmall );
        Serial.flush();
        return CMD_DONT_SHOW_MENU;
    }
    Serial.println( "Small allocation test: PASS" );
    free( testSmall );
    Serial.flush();
    
    // Now try larger test
    const size_t testSize = 64 * 1024; // 64KB test block
    Serial.println( "Allocating " + String( testSize / 1024 ) + " KB test block..." );
    Serial.flush();
    
    uint32_t* psramBlock = (uint32_t*)pmalloc( testSize );
    if ( psramBlock == nullptr ) {
        Serial.println( "ERROR: Failed to allocate PSRAM test block!" );
        Serial.flush();
        return CMD_DONT_SHOW_MENU;
    }
    Serial.println( "Allocation successful at address: 0x" + String( (uint32_t)psramBlock, HEX ) );
    Serial.flush();
    
    size_t numWords = testSize / sizeof(uint32_t);
    int errors = 0;
    
    // Test 1: Sequential pattern
    Serial.print( "Test 1: Sequential pattern... " );
    Serial.flush();
    for ( size_t i = 0; i < numWords; i++ ) {
        psramBlock[i] = i;
    }
    for ( size_t i = 0; i < numWords; i++ ) {
        if ( psramBlock[i] != i ) {
            errors++;
            if ( errors <= 5 ) {
                Serial.println( "Error at " + String(i) + ": expected " + String(i) + ", got " + String(psramBlock[i]) );
            }
        }
    }
    Serial.println( errors == 0 ? "PASS" : "FAIL (" + String(errors) + " errors)" );
    Serial.flush();
    
    // Test 2: Alternating bits pattern (0x55555555 / 0xAAAAAAAA)
    errors = 0;
    Serial.print( "Test 2: Alternating bits (0x55/0xAA)... " );
    Serial.flush();
    for ( size_t i = 0; i < numWords; i++ ) {
        psramBlock[i] = ( i & 1 ) ? 0xAAAAAAAA : 0x55555555;
    }
    for ( size_t i = 0; i < numWords; i++ ) {
        uint32_t expected = ( i & 1 ) ? 0xAAAAAAAA : 0x55555555;
        if ( psramBlock[i] != expected ) {
            errors++;
        }
    }
    Serial.println( errors == 0 ? "PASS" : "FAIL (" + String(errors) + " errors)" );
    Serial.flush();
    
    // Test 3: Walking ones
    errors = 0;
    Serial.print( "Test 3: Walking ones pattern... " );
    Serial.flush();
    for ( size_t i = 0; i < numWords; i++ ) {
        psramBlock[i] = 1 << ( i % 32 );
    }
    for ( size_t i = 0; i < numWords; i++ ) {
        uint32_t expected = 1 << ( i % 32 );
        if ( psramBlock[i] != expected ) {
            errors++;
        }
    }
    Serial.println( errors == 0 ? "PASS" : "FAIL (" + String(errors) + " errors)" );
    Serial.flush();
    
    // Test 4: All zeros and all ones
    errors = 0;
    Serial.print( "Test 4: All zeros/ones... " );
    Serial.flush();
    for ( size_t i = 0; i < numWords; i++ ) {
        psramBlock[i] = 0x00000000;
    }
    for ( size_t i = 0; i < numWords; i++ ) {
        if ( psramBlock[i] != 0x00000000 ) {
            errors++;
        }
    }
    for ( size_t i = 0; i < numWords; i++ ) {
        psramBlock[i] = 0xFFFFFFFF;
    }
    for ( size_t i = 0; i < numWords; i++ ) {
        if ( psramBlock[i] != 0xFFFFFFFF ) {
            errors++;
        }
    }
    Serial.println( errors == 0 ? "PASS" : "FAIL (" + String(errors) + " errors)" );
    Serial.flush();
    
    // Speed test
    Serial.println( "\n--- Speed Comparison Test ---" );
    Serial.flush();
    
    const size_t speedTestSize = 32 * 1024; // 32KB for speed test
    size_t speedWords = speedTestSize / sizeof(uint32_t);
    
    // Allocate SRAM block for comparison
    uint32_t* sramBlock = (uint32_t*)malloc( speedTestSize );
    if ( sramBlock == nullptr ) {
        Serial.println( "Warning: Could not allocate SRAM comparison block" );
        free( psramBlock );
        Serial.flush();
        return CMD_DONT_SHOW_MENU;
    }
    
    unsigned long startTime, endTime;
    
    // PSRAM sequential write speed
    startTime = micros();
    for ( size_t i = 0; i < speedWords; i++ ) {
        psramBlock[i] = i;
    }
    endTime = micros();
    unsigned long psramWriteTime = endTime - startTime;
    float psramWriteSpeed = ( speedTestSize / 1024.0 ) / ( psramWriteTime / 1000000.0 ); // KB/s
    
    // PSRAM sequential read speed
    volatile uint32_t dummy = 0;
    startTime = micros();
    for ( size_t i = 0; i < speedWords; i++ ) {
        dummy += psramBlock[i];
    }
    endTime = micros();
    unsigned long psramReadTime = endTime - startTime;
    float psramReadSpeed = ( speedTestSize / 1024.0 ) / ( psramReadTime / 1000000.0 ); // KB/s
    
    // SRAM sequential write speed
    startTime = micros();
    for ( size_t i = 0; i < speedWords; i++ ) {
        sramBlock[i] = i;
    }
    endTime = micros();
    unsigned long sramWriteTime = endTime - startTime;
    float sramWriteSpeed = ( speedTestSize / 1024.0 ) / ( sramWriteTime / 1000000.0 ); // KB/s
    
    // SRAM sequential read speed  
    startTime = micros();
    for ( size_t i = 0; i < speedWords; i++ ) {
        dummy += sramBlock[i];
    }
    endTime = micros();
    unsigned long sramReadTime = endTime - startTime;
    float sramReadSpeed = ( speedTestSize / 1024.0 ) / ( sramReadTime / 1000000.0 ); // KB/s
    
    Serial.println( "Test block size: " + String( speedTestSize / 1024 ) + " KB" );
    Serial.println( "" );
    Serial.println( "PSRAM Write: " + String( psramWriteTime ) + " us (" + String( psramWriteSpeed / 1024, 2 ) + " MB/s)" );
    Serial.println( "PSRAM Read:  " + String( psramReadTime ) + " us (" + String( psramReadSpeed / 1024, 2 ) + " MB/s)" );
    Serial.println( "SRAM Write:  " + String( sramWriteTime ) + " us (" + String( sramWriteSpeed / 1024, 2 ) + " MB/s)" );
    Serial.println( "SRAM Read:   " + String( sramReadTime ) + " us (" + String( sramReadSpeed / 1024, 2 ) + " MB/s)" );
    Serial.println( "" );
    Serial.println( "Speed ratio (SRAM/PSRAM):" );
    Serial.println( "  Write: " + String( sramWriteSpeed / psramWriteSpeed, 2 ) + "x" );
    Serial.println( "  Read:  " + String( sramReadSpeed / psramReadSpeed, 2 ) + "x" );
    Serial.flush();
    
    // Cleanup
    free( psramBlock );
    free( sramBlock );
    
    Serial.println( "\n=== PSRAM Test Complete ===" );
    Serial.flush();
    
    // Use dummy to prevent optimizer from removing the reads
    (void)dummy;
    
    return CMD_DONT_SHOW_MENU;
}

// Helper function to strip '>' characters from the beginning of lines
static String stripLeadingArrows( const String& input ) {
    String result;
    result.reserve( input.length( ) );

    bool atLineStart = true;
    for ( size_t i = 0; i < input.length( ); i++ ) {
        char c = input[ i ];

        // Skip '>' at the beginning of lines
        if ( atLineStart && c == '>' ) {
            continue;
        }

        // Skip whitespace at line start (after we've removed '>')
        if ( atLineStart && ( c == ' ' || c == '\t' ) ) {
            continue;
        }

        result += c;

        // Track line boundaries
        if ( c == '\n' || c == '\r' ) {
            atLineStart = true;
        } else if ( c != ' ' && c != '\t' ) {
            atLineStart = false;
        }
    }

    return result;
}



CommandResult cmd_pythonCommand( char c, const String& line ) {
    String pythonCommand = "";

    // PRIORITY 1: Use the line parameter if it has content (CommandBuffer path)
    // This ensures commands from UART tags work regardless of terminal_line_buffering setting
    String trimmedLine = line;
    trimmedLine.trim( );

    if ( trimmedLine.length( ) > 1 && trimmedLine[ 0 ] == '>' ) {
        // Line has ACTUAL content after '>' - use it directly (line buffering or CommandBuffer path)
        pythonCommand = trimmedLine.substring( 1 );
        pythonCommand.trim( );
    } else if ( trimmedLine.length( ) > 1 ) {
        // Line has content but no '>' prefix - use as-is
        pythonCommand = trimmedLine;
    } else {
        // Line is empty OR just the trigger character '>'
        // This happens when terminal_line_buffering == 0 (char-by-char mode)
        // Read the rest of the command from Jerial
        // CRITICAL FIX: Only read until the FIRST newline to process one command at a time
        // This prevents long Python execution from blocking DTR checks
        while ( Jerial.available( ) > 0 ) {
            char ch = Jerial.read( );
            if ( ch == '\n' ) {
                break; // Stop at first newline - process one command at a time
            }
            if ( ch != '\r' ) { // Skip carriage returns
                pythonCommand += ch;
            }
        }
    }

    // Strip '>' from the beginning of all lines
    pythonCommand = stripLeadingArrows( pythonCommand );
    pythonCommand.trim( );

    // Get the response target for this command (if any)
    Stream* response_target = Jerial.getResponseTarget( );

#if DEBUG_INJECTED_COMMANDS
    // Debug output - always enabled for now to track command execution
    Serial.print( "cmd_pythonCommand: Received line=[" );
    for ( size_t i = 0; i < line.length( ); i++ ) {
        char c = line[ i ];
        if ( c == '\n' )
            Serial.print( "\\n" );
        else if ( c == '\r' )
            Serial.print( "\\r" );
        else if ( c >= 32 && c < 127 )
            Serial.print( c );
        else
            Serial.printf( "<%02X>", (unsigned char)c );
    }
    Serial.print( "], extracted pythonCommand=[" );
    for ( size_t i = 0; i < pythonCommand.length( ); i++ ) {
        char c = pythonCommand[ i ];
        if ( c == '\n' )
            Serial.print( "\\n" );
        else if ( c == '\r' )
            Serial.print( "\\r" );
        else if ( c >= 32 && c < 127 )
            Serial.print( c );
        else
            Serial.printf( "<%02X>", (unsigned char)c );
    }
    Serial.printf( "] (len=%d), response_target=%p\n", pythonCommand.length( ), response_target );
    Serial.flush( );
#endif

    if ( pythonCommand.length( ) > 0 ) {
        // Execute command (output goes to USB Serial)
        // Note: response_target routing for UART is available but not used for MicroPython output
        // because MicroPython writes directly to global_mp_stream and capturing is complex

        // SAFETY: If command contains newlines, split into individual lines
        // and execute each separately. This way one bad command doesn't prevent
        // the others from running (each gets its own nlr error handler).
        if ( pythonCommand.indexOf( '\n' ) >= 0 ) {
            int lineStart = 0;
            for ( int i = 0; i <= (int)pythonCommand.length( ); i++ ) {
                if ( i == (int)pythonCommand.length( ) || pythonCommand[ i ] == '\n' ) {
                    if ( i > lineStart ) {
                        String singleLine = pythonCommand.substring( lineStart, i );
                        singleLine.trim( );
                        if ( singleLine.length( ) > 0 ) {
                            // Validate: skip lines with no printable content
                            bool hasPrintable = false;
                            for ( size_t j = 0; j < singleLine.length( ); j++ ) {
                                if ( singleLine[ j ] >= ' ' && singleLine[ j ] < 127 ) {
                                    hasPrintable = true;
                                    break;
                                }
                            }
                            if ( hasPrintable ) {
                                executeSinglePythonCommand( singleLine.c_str( ), nullptr, 0 );
                                tud_task( ); // Service USB between commands
                            }
                        }
                    }
                    lineStart = i + 1;
                }
            }
        } else {
            executeSinglePythonCommand( pythonCommand.c_str( ), nullptr, 0 );
        }

    } else {
        Jerial.println( "Usage: > <python_command>" );
    }
    Jerial.flush( );
    tud_task( ); // Service USB before return
    return CMD_DONT_SHOW_MENU;
}

// File system commands
CommandResult cmd_showFilesystem( char c, const String& line ) {
    // Check if a filename/path was provided after '/'
    String arg = line;
    arg.trim( );
    // Strip the leading '/' trigger character
    if ( arg.startsWith( "/" ) )
        arg = arg.substring( 1 );
    arg.trim( );

    // If line only had the trigger char, read the rest from Jerial
    // (happens when terminal_line_buffering == 0, char-by-char mode)
    if ( arg.length( ) == 0 && Jerial.available( ) > 0 ) {
        while ( Jerial.available( ) > 0 ) {
            char ch = Jerial.read( );
            if ( ch == '\n' ) {
                break;
            }
            if ( ch != '\r' ) {
                arg += ch;
            }
        }
        arg.trim( );
    }

    if ( arg.length( ) > 0 ) {
        // User provided a path like "/adc_basics.py" — resolve and run it
        String resolved = resolvePythonScriptPath( arg );
        if ( resolved.length( ) > 0 ) {
            setGlobalStreamWithInterrupt( &Serial );
            runPythonScriptByPath( resolved );
        } else {
            Jerial.println( "Script not found: " + arg );
            Jerial.println( "Searched: /python_scripts, /python_scripts/examples, /python_scripts/lib, /python_scripts/modules, /" );
        }
        Jerial.flush( );
        return CMD_DONT_SHOW_MENU;
    }

    // No argument — open the file manager as before
    runApp( -1, (char*)"File Manager" );
    Jerial.write( 0x0F );
    termInInteractiveMode = 0;
    Jerial.flush( );
    return CMD_SHOW_MENU;
}

CommandResult cmd_enableUSBStorage( char c, const String& line ) {
    extern bool mscModeEnabled;
    if ( mscModeEnabled == false ) {
        Jerial.println( "Enabling USB Mass Storage drive..." );
        if ( initUSBMassStorage( ) ) {
            Jerial.println( "USB Mass Storage enabled - device will appear as 'JUMPERLESS' drive\n\r" );
            Jerial.println( "\tu = disable USB Mass Storage" );
            Jerial.println( "\tG = reload config.txt" );
            Jerial.println( "\ty = refresh connections when files change" );
            Jerial.println( "\tS = show status" );
            Jerial.println( "\n\r" );
            Jerial.flush( );
        } else {
            Jerial.println( "USB Mass Storage initialization failed" );
            Jerial.flush( );
        }
    } else {
        Jerial.println( "USB Mass Storage is already enabled" );
        printUSBMassStorageStatus( );
        refreshConnections( -1 );
        Jerial.flush( );
    }
    // Note: The USB Mass Storage loop is handled in main.cpp
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_disableUSBStorage( char c, const String& line ) {
    extern bool mscModeEnabled;
    if ( mscModeEnabled == true ) {
        Jerial.println( "Disabling USB Mass Storage drive..." );
        if ( disableUSBMassStorage( ) ) {
            Jerial.println( "USB Mass Storage disabled - device no longer appears as drive" );
            Jerial.println( "Use 'U' command to re-enable when needed" );
        } else {
            Jerial.println( "USB Mass Storage disable failed" );
        }
    } else {
        Jerial.println( "USB Mass Storage is already disabled" );
        Jerial.println( "Use 'U' command to enable" );
    }
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_listFilesystem( char c, const String& line ) {
    // Implementation would list all files recursively
    Jerial.println( "Filesystem listing not yet implemented" );
    return CMD_DONT_SHOW_MENU;
}

// Config commands
CommandResult cmd_editConfig( char c, const String& line ) {
    extern volatile bool core1busy;
    // core1busy = 1;
    // waitCore2();
    readConfigFromSerial( );
    // core1busy = 0;
    Jerial.flush( );
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_printConfig( char c, const String& line ) {
    // extern volatile bool core1busy;
    // core1busy = 1;
    // waitCore2();
    printConfigToSerial( );
    // core1busy = 0;
    Jerial.flush( );
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_reloadConfig( char c, const String& line ) {
    // This is the 'G' command which has special wavegen test code
    // For now, we'll just reload the config
    Jerial.println( "Reloading config.txt..." );
    extern bool configChanged;
    configChanged = true;
    return CMD_SHOW_MENU;
}

// Hardware commands
CommandResult cmd_resetArduino( char c, const String& line ) {
    if ( Jerial.available( ) > 0 ) {
        char ch = Jerial.read( );
        if ( ch == '0' || ch == '2' || ch == 't' ) {
            resetArduino( 0 );
        }
        if ( ch == '1' || ch == '2' || ch == 'b' ) {
            resetArduino( 1 );
        }
    } else {
        resetArduino( );
    }
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_connectArduino( char c, const String& line ) {
    int justAsk = 0;
    if ( Jerial.available( ) > 0 ) {
        char ch = Jerial.read( );
        if ( ch == '?' ) {
            justAsk = 1;
            int isConnected = checkIfArduinoIsConnected( );
            int isPresent = checkArduinoPresence( );

            // Response format: "connection,presence"
            // connection: Y=connected, n=not connected
            // presence: Y=detected, n=not detected
            // NOTE: DC4 (0x14) in replyWithJerialInfo() is now the preferred method
            // for presence checks (faster response). This A? handler is kept for
            // backwards compatibility.
            Jerial.print( isConnected ? "Y," : "n," );
            Jerial.println( isPresent ? "Y" : "n" );
            Jerial.flush( );
        }
    }
    if ( justAsk == 0 ) {
        connectArduino( 0 );
        Jerial.println( "UART connected to Arduino D0 and D1" );
        Jerial.flush( );
    }
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_disconnectArduino( char c, const String& line ) {
    int justAsk = 0;
    while ( Jerial.available( ) > 0 ) {
        char ch = Jerial.read( );
        if ( ch == '?' ) {
            if ( checkIfArduinoIsConnected( ) == 1 ) {
                justAsk = 1;
                Jerial.println( "Y" );
                Jerial.flush( );
            } else {
                justAsk = 1;
                Jerial.println( "n" );
                Jerial.flush( );
            }
        }
    }
    if ( justAsk == 0 ) {
        disconnectArduino( 0 );
        Jerial.println( "UART disconnected from Arduino D0 and D1" );
        Jerial.flush( );
    }
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_readADC( char c, const String& line ) {
    if ( Jerial.available( ) > 0 ) {
        char ch = Jerial.read( );

        if ( isdigit( ch ) ) {
            int adc = ch - '0';
            if ( adc >= 0 && adc <= 4 ) {
                Jerial.print( " adc" );
                Jerial.print( adc );
                Jerial.print( " = " );
                float adcVoltage = readAdcVoltage( adc, 32 );
                if ( adcVoltage > 0.00 ) {
                    Jerial.print( " " );
                }
                Jerial.println( adcVoltage );
            }
        } else if ( ch == 'i' ) {
            if ( Jerial.available( ) > 0 ) {
                char ch2 = Jerial.read( );
                if ( ch2 == '1' ) {
                    extern INA219 INA1;
                    float iSense = INA1.getCurrent_mA( );
                    Jerial.print( "ina1 = " );
                    Jerial.print( iSense );
                    Jerial.println( "mA" );
                }
            } else {
                extern INA219 INA0;
                float iSense = INA0.getCurrent_mA( );
                Jerial.print( "ina0 = " );
                Jerial.print( iSense );
                Jerial.print( "mA \t" );

                iSense = INA0.getBusVoltage( );
                Jerial.print( iSense );
                Jerial.print( "V \t" );

                iSense = INA0.getPower_mW( );
                Jerial.print( iSense );
                Jerial.println( "mW" );
            }
        } else if ( ch == 'l' ) {
            // showReadings is defined in Peripherals.h as int&
            if ( showReadings == 1 ) {
                showReadings = 0;
                Jerial.println( "showReadings = 0" );
            } else {
                showReadings = 1;
                Jerial.println( "showReadings = 1" );
            }
            chooseShownReadings( );
        }
        Jerial.flush( );
    } else {
        Jerial.println( );
        for ( int i = 0; i < 5; i++ ) {
            Jerial.print( "adc" );
            Jerial.print( i );
            Jerial.print( " = " );
            float adcVoltage = readAdcVoltage( i, 32 );
            if ( adcVoltage > 0.00 ) {
                Jerial.print( " " );
            }
            Jerial.println( adcVoltage );
        }
        Jerial.print( "probe = " );
        float probeVoltage = readAdcVoltage( 7, 32 );
        if ( probeVoltage > 0.00 ) {
            Jerial.print( " " );
        }
        Jerial.println( probeVoltage );
    }
    Jerial.flush( );
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_setDAC( char c, const String& line ) {
    // probePowerDAC is defined in Probing.h as int&
    extern bool configChanged;

    char f[ 8 ] = { ' ' };
    int index = 0;
    float f1 = 0.0;
    unsigned long timer = millis( );
    while ( Jerial.available( ) == 0 && millis( ) - timer < 1000 ) {
    }
    while ( index < 8 ) {
        f[ index ] = Jerial.read( );
        index++;
    }

    f1 = atof( f );
    if ( probePowerDAC == 1 ) {
        setDac0voltage( f1, 1, 1 );
    } else if ( probePowerDAC == 0 ) {
        setDac1voltage( f1, 1, 1 );
    }
    configChanged = true;
    Jerial.printf( "DAC %d = %0.2f V\n", !probePowerDAC, f1 );
    Jerial.flush( );
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_i2cScan( char c, const String& line ) {
    Jerial.flush( );

    if ( Jerial.available( ) > 0 ) {
        String input = Jerial.readString( );
        input.trim( );
        if ( input.indexOf( 'i' ) != -1 ) {
            i2cScan( 0, 0, 26, 27, 1, 1 );
            return CMD_DONT_SHOW_MENU;
        }
        if ( input.indexOf( ',' ) != -1 ) {
            // Format: @5,10 - SDA at row 5, SCL at row 10
            int commaIndex = input.indexOf( ',' );
            int sdaRow = input.substring( 0, commaIndex ).toInt( );
            int sclRow = input.substring( commaIndex + 1 ).toInt( );

            changeTerminalColor( 69, true );
            Jerial.print( "I2C scan with SDA=" );
            Jerial.print( sdaRow );
            Jerial.print( ", SCL=" );
            Jerial.println( sclRow );
            changeTerminalColor( 38, true );

            if ( i2cScan( sdaRow, sclRow, 26, 27, 1 ) > 0 ) {
                Jerial.println( "Found devices" );
                return CMD_DONT_SHOW_MENU;
            } else {
                removeBridgeFromState( RP_GPIO_26, sdaRow, true );
                removeBridgeFromState( RP_GPIO_27, sclRow, true );
            }
        } else if ( input.length( ) > 0 && isdigit( input[ 0 ] ) ) {
            // Format: @5 - try all 4 combinations around row 5
            int baseRow = input.toInt( );

            changeTerminalColor( 69, true );
            Jerial.print( "I2C scan trying all combinations around row " );
            Jerial.println( baseRow );
            changeTerminalColor( 38, true );

            int combinations[ 4 ][ 2 ] = {
                { baseRow, baseRow + 1 },
                { baseRow + 1, baseRow },
                { baseRow, baseRow - 1 },
                { baseRow - 1, baseRow } };

            for ( int i = 0; i < 4; i++ ) {
                int sdaRow = combinations[ i ][ 0 ];
                int sclRow = combinations[ i ][ 1 ];

                changeTerminalColor( 202, true );
                Jerial.print( "\nTrying SDA=" );
                Jerial.print( sdaRow );
                Jerial.print( ", SCL=" );
                Jerial.print( sclRow );
                Jerial.println( ":" );
                changeTerminalColor( 38, true );
                int devicesFound = i2cScan( sdaRow, sclRow, 26, 27, 0 );
                if ( devicesFound > 0 ) {
                    changeTerminalColor( 199, true );
                    Jerial.printf( "\n\rfound %d devices: SDA at row %d, SCL at row %d\n\r",
                                   devicesFound, sdaRow, sclRow );
                    changeTerminalColor( -1 );
                    return CMD_DONT_SHOW_MENU;
                }
                delay( 1 );
            }
        }
    } else {
        // Interactive mode
        Jerial.print( "Enter SDA row: " );
        Jerial.flush( );
        while ( Jerial.available( ) == 0 ) {
        }
        int rowSDA = Jerial.parseInt( );
        Jerial.print( "Enter SCL row: " );
        Jerial.flush( );
        while ( Jerial.available( ) == 0 ) {
        }
        int rowSCL = Jerial.parseInt( );

        changeTerminalColor( 69, true );
        Jerial.print( "I2C scan with SDA=" );
        Jerial.print( rowSDA );
        Jerial.print( ", SCL=" );
        Jerial.println( rowSCL );
        changeTerminalColor( 38, true );

        if ( i2cScan( rowSDA, rowSCL, 26, 27, 1 ) > 0 ) {
            // Success
        } else {
            removeBridgeFromState( RP_GPIO_26, rowSDA, true );
            removeBridgeFromState( RP_GPIO_27, rowSCL, true );
        }
    }

    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_calibrateDACs( char c, const String& line ) {
    // dacSpread and dacZero are defined in Peripherals.h
    extern float dacSpread[ 4 ];
    extern int dacZero[ 4 ];

    for ( int d = 0; d < 4; d++ ) {
        Jerial.print( "dacSpread[" );
        Jerial.print( d );
        Jerial.print( "] = " );
        Jerial.println( dacSpread[ d ] );
    }

    for ( int d = 0; d < 4; d++ ) {
        Jerial.print( "dacZero[" );
        Jerial.print( d );
        Jerial.print( "] = " );
        Jerial.println( dacZero[ d ] );
    }

    calibrateDacs( );
    return CMD_SHOW_MENU;
}

// Debug commands
CommandResult cmd_showVersion( char c, const String& line ) {
    Jerial.print( "Jumperless firmware version: " );
    Jerial.println( firmwareVersion );
    Jerial.flush( );
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_setDebugFlags( char c, const String& line ) {
    debugFlagsMenu( );
    return CMD_SHOW_MENU;
}

CommandResult cmd_statusDiagnosticsMenu( char c, const String& line ) {
    statusDiagnosticsMenu( );
    return CMD_SHOW_MENU;
}



const char* pinNames[48] = {
    "UART_Tx",
    "UART_Rx",
    "LED_PROBE",
    "LED_TOP",
    "I2C0_SDA",
    "I2C0_SCL",
    "RP6",
    "RP7",
    "LDAC",
    "PROBE_BUTTON",
    "PROBE_PROBE",
    "ENC_PUSH",
    "ENC_A",
    "ENC_B",
    "CH_DATA",
    "CH_CLK",
    "CH_RESET",
    "LED_BB",
    "NANO_RESET_0",
    "NANO_RESET_1",
    "GPIO_1",
    "GPIO_2",
    "GPIO_3",
    "GPIO_4",
    "GPIO_5",
    "GPIO_6",
    "GPIO_7",
    "GPIO_8",
    "CH_CS_A",
    "CH_CS_B",
    "CH_CS_C",
    "CH_CS_D",
    "CH_CS_E",
    "CH_CS_F",
    "CH_CS_G",
    "CH_CS_H",
    "CH_CS_I",
    "CH_CS_J",
    "CH_CS_K",
    "CH_CS_L",
    "ADC_0",
    "ADC_1",
    "ADC_2",
    "ADC_3",
    "ADC_4_5V",
    "PROBE_PAD_SENS",
    "SUPPLY_MONITOR",
    "ADC_PROBE"
};
const char* PSRAM_CS = "PSRAM_CS";




CommandResult cmd_resourceStatus( char c, const String& line ) {
    Jerial.println( "\n\r╭──────────────────────────────────────────────────────────────────────╮" );
    Jerial.println( "│                      SYSTEM RESOURCE STATUS                          │" );
    Jerial.println( "╰──────────────────────────────────────────────────────────────────────╯\n\r" );

    // ═══════════════════════════════════════════════════════════════════════════
    // MEMORY STATUS (2-column layout)
    // ═══════════════════════════════════════════════════════════════════════════
    Jerial.println( "┌──────────────────────────────────┬───────────────────────────────────┐" );
    Jerial.println( "│         SRAM MEMORY (Heap)       │         PSRAM MEMORY              │" );
    Jerial.println( "├──────────────────────────────────┼───────────────────────────────────┤" );
    
    // Get SRAM info
    size_t sramTotal = rp2040.getTotalHeap( );
    size_t sramFree = rp2040.getFreeHeap( );
    size_t sramUsed = sramTotal - sramFree;
    int sramPercent = ( sramUsed * 100 ) / sramTotal;
    
    // Get PSRAM info
    size_t psramSize = rp2040.getPSRAMSize( );
    size_t psramTotal = 0, psramFree = 0, psramUsed = 0;
    int psramPercent = 0;
    bool hasPSRAM = ( psramSize > 0 && jumperlessConfig.hardware.psram_installed == 1 );
    
    if ( hasPSRAM ) {
        psramTotal = rp2040.getTotalPSRAMHeap( );
        psramFree = rp2040.getFreePSRAMHeap( );
        psramUsed = rp2040.getUsedPSRAMHeap( );
        psramPercent = psramTotal > 0 ? ( psramUsed * 100 ) / psramTotal : 0;
    }
    
    // Memory rows with fixed-width formatting
    if ( hasPSRAM ) {
        Jerial.printf( "│ Total: %6u KB (%6u bytes)  │ Chip:     %4u MB (%7u bytes) │\n\r",
                       (unsigned)(sramTotal / 1024), (unsigned)sramTotal,
                       (unsigned)(psramSize / 1024 / 1024), (unsigned)psramSize );
        Jerial.printf( "│ Free:  %6u KB (%6u bytes)  │ Total: %6u KB                  │\n\r",
                       (unsigned)(sramFree / 1024), (unsigned)sramFree,
                       (unsigned)(psramTotal / 1024) );
        Jerial.printf( "│ Used:  %6u KB (%3d%%)          │ Free:  %6u KB (%3d%% used)      │\n\r",
                       (unsigned)(sramUsed / 1024), sramPercent,
                       (unsigned)(psramFree / 1024), psramPercent );
    } else {
        Jerial.printf( "│ Total: %6u KB (%6u bytes)  │ Not installed                     │\n\r",
                       (unsigned)(sramTotal / 1024), (unsigned)sramTotal );
        Jerial.printf( "│ Free:  %6u KB (%6u bytes)  │ Config: psram_installed=%d         │\n\r",
                       (unsigned)(sramFree / 1024), (unsigned)sramFree,
                       jumperlessConfig.hardware.psram_installed );
        Jerial.printf( "│ Used:  %6u KB (%3d%%)          │                                   │\n\r",
                       (unsigned)(sramUsed / 1024), sramPercent );
    }
    
    Jerial.println( "└──────────────────────────────────┴───────────────────────────────────┘" );

    // ═══════════════════════════════════════════════════════════════════════════
    // PERIPHERALS STATUS (2-column layout)
    // ═══════════════════════════════════════════════════════════════════════════
    Jerial.println( "\n\r┌──────────────────────────────────┬───────────────────────────────────┐" );
    Jerial.println( "│         OLED DISPLAY             │           PIO STATUS              │" );
    Jerial.println( "├──────────────────────────────────┼───────────────────────────────────┤" );
    
    // OLED connection type names
    const char* connTypes[] = { "GPIO 7/8 (crossbar)", "RP6/RP7 (hardwired)", "I2C0 (internal)", "Custom" };
    int connType = jumperlessConfig.top_oled.connection_type;
    const char* connName = (connType >= 0 && connType <= 3) ? connTypes[connType] : "Unknown";
    
    Jerial.printf( "│ Status: %-23s  │ PIO0: SM0:%s SM1:%s SM2:%s SM3:%s     │\n\r",
                   oled.isConnected( ) ? "Connected" : "Not connected",
                   pio_sm_is_claimed( pio0, 0 ) ? "●" : "○",
                   pio_sm_is_claimed( pio0, 1 ) ? "●" : "○",
                   pio_sm_is_claimed( pio0, 2 ) ? "●" : "○",
                   pio_sm_is_claimed( pio0, 3 ) ? "●" : "○" );
    
    Jerial.printf( "│ Type: %-25s  │ PIO1: SM0:%s SM1:%s SM2:%s SM3:%s     │\n\r",
                   connName,
                   pio_sm_is_claimed( pio1, 0 ) ? "●" : "○",
                   pio_sm_is_claimed( pio1, 1 ) ? "●" : "○",
                   pio_sm_is_claimed( pio1, 2 ) ? "●" : "○",
                   pio_sm_is_claimed( pio1, 3 ) ? "●" : "○" );
    
    Jerial.printf( "│ SDA: GPIO %2d  SCL: GPIO %2d       │ PIO2: SM0:%s SM1:%s SM2:%s SM3:%s     │\n\r",
                   jumperlessConfig.top_oled.sda_pin,
                   jumperlessConfig.top_oled.scl_pin,
                   pio_sm_is_claimed( pio2, 0 ) ? "●" : "○",
                   pio_sm_is_claimed( pio2, 1 ) ? "●" : "○",
                   pio_sm_is_claimed( pio2, 2 ) ? "●" : "○",
                   pio_sm_is_claimed( pio2, 3 ) ? "●" : "○" );
    
    // Show rows if using crossbar connection
    if ( jumperlessConfig.top_oled.sda_row >= 0 ) {
        Jerial.printf( "│ SDA Row: %3s  SCL Row: %3s       │                                   │\n\r",
                       definesToChar(jumperlessConfig.top_oled.sda_row, 0),
                       definesToChar(jumperlessConfig.top_oled.scl_row, 0));
    }
    
    Jerial.println( "└──────────────────────────────────┴───────────────────────────────────┘" );

    // ═══════════════════════════════════════════════════════════════════════════
    // GPIO PIN STATUS (2-column layout with hex values)
    // ═══════════════════════════════════════════════════════════════════════════
    Jerial.println( "\n\rgpio  up dn  func      hex  name            gpio  up dn  func      hex  name" );
    Jerial.println(     "────  ─────  ────────  ───  ────────────    ────  ─────  ────────  ───  ────────────" );
    
    for ( int row = 0; row < 24; row++ ) {
        int gpio1 = row;
        int gpio2 = row + 24;
        
        // Get actual pull status from hardware registers
        uint32_t pad1 = pads_bank0_hw->io[gpio1];
        uint32_t pad2 = pads_bank0_hw->io[gpio2];
        
        bool up1 = gpio_is_pulled_up(gpio1);
        bool dn1 = gpio_is_pulled_down(gpio1);
        bool up2 = gpio_is_pulled_up(gpio2);
        bool dn2 = gpio_is_pulled_down(gpio2);
        
        gpio_function_t func1 = gpio_get_function( gpio1 );
        gpio_function_t func2 = gpio_get_function( gpio2 );
        const char* funcName1 = gpio_function_name_for_pin( gpio1, func1 );
        const char* funcName2 = gpio_function_name_for_pin( gpio2, func2 );
        
        // Both columns with fixed-width formatting
        Jerial.printf( "%4d   %c  %c  %-8s  %-2X   %-14s  %4d   %c  %c  %-8s  %-2X   %-14s\n\r",
                       gpio1,
                       up1 ? '^' : ' ',
                       dn1 ? 'v' : ' ',
                       funcName1,
                       (int)func1,
                       (gpio1 == 19 && jumperlessConfig.hardware.psram_installed == 1) ? "PSRAM_CS" : pinNames[gpio1],
                       gpio2,
                       up2 ? '^' : ' ',
                       dn2 ? 'v' : ' ',
                       funcName2,
                       (int)func2,
                       pinNames[gpio2] );
        
        Jerial.flush( );
    }
    
    Jerial.println( "\n\r          ^ = pull-up  v = pull-down     ● = claimed, ○ = free\n\r" );
    
    return CMD_SHOW_MENU;
}

CommandResult cmd_gpioState( char c, const String& line ) {
    printGPIOState( );
    return CMD_SHOW_MENU;
}

CommandResult cmd_usbDebugMenu( char c, const String& line ) {
    Jerial.println( "╭─────────────────────────────────╮" );
    Jerial.println( "│        USB Debug Control        │" );
    Jerial.println( "├─────────────────────────────────┤" );
    Jerial.println( "│ 1. Toggle USB debug mode        │" );
    Jerial.println( "│ 2. Manual refresh from USB      │" );
    Jerial.println( "│ 3. Validate all slots           │" );
    Jerial.println( "│ Any other key - Cancel          │" );
    Jerial.println( "╰─────────────────────────────────╯" );
    Jerial.print( "Choose option: " );
    Jerial.flush( );

    while ( Jerial.available( ) == 0 ) {
        delay( 1 );
    }
    char choice = Jerial.read( );
    Jerial.println( choice );

    switch ( choice ) {
    case '1':
        Jerial.println( "\nToggling USB debug mode..." );
        extern bool usb_debug_enabled;
        setUSBDebug( !usb_debug_enabled );
        break;
    case '2':
        if ( isUSBMassStorageMounted( ) ) {
            Jerial.println( "\nPerforming manual refresh from USB..." );
            manualRefreshFromUSB( );
        } else {
            Jerial.println( "\nUSB drive not mounted" );
        }
        break;
    case '3':
        Jerial.println( "\nValidating all slot files..." );
        // validateAllSlots(true);
        break;
    default:
        Jerial.println( "\nCancelled" );
        break;
    }

    Jerial.flush( );
    return CMD_DONT_SHOW_MENU;
}

// Settings commands
CommandResult cmd_ledBrightness( char c, const String& line ) {
    if ( LEDbrightnessMenu( ) == '!' ) {
        clearLEDs( );
        delayMicroseconds( 9200 );
        // sendAllPathsCore2 is defined in Commands.h as volatile int
        extern volatile int sendAllPathsCore2;
        sendAllPathsCore2 = 1;
    }
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_toggleOLED( char c, const String& line ) {
    if ( jumperlessConfig.top_oled.enabled == 0 ) {
        Jerial.println( "oled enabled" );
        oled.init( );
        jumperlessConfig.top_oled.enabled = 1;
        extern bool configChanged;
        configChanged = true;
    } else {
        oled.disconnect( );
        jumperlessConfig.top_oled.enabled = 0;
        oled.oledConnected = false;
        extern bool configChanged;
        configChanged = true;
        Jerial.println( "oled disconnected" );
    }
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_toggleTerminalColors( char c, const String& line ) {
    extern bool disableTerminalColors;
    disableTerminalColors = !disableTerminalColors;
    if ( disableTerminalColors ) {
        Jerial.println( "Terminal colors disabled" );
    } else {
        Jerial.println( "Terminal colors enabled" );
    }
    Jerial.flush( );
    return CMD_SHOW_MENU;
}

CommandResult cmd_dontShowMenu( char c, const String& line ) {
    if ( dontShowMenu == 0 ) {
        dontShowMenu = 1;
    } else {
        dontShowMenu = 0;
    }
    return CMD_SHOW_MENU;
}

CommandResult cmd_oledInTerminal( char c, const String& line ) {
    if ( jumperlessConfig.top_oled.show_in_terminal > 0 ) {
        jumperlessConfig.top_oled.show_in_terminal = 1;
    } else {
        jumperlessConfig.top_oled.show_in_terminal = 0;
    }
    extern bool configChanged;
    configChanged = true;
    return CMD_SHOW_MENU;
}

CommandResult cmd_cycleFont( char c, const String& line ) {
    oled.cycleFont( );
    return CMD_SHOW_MENU;
}

// App/Special mode commands
CommandResult cmd_logicAnalyzer( char c, const String& line ) {
    extern bool la_enabled;
    if ( la_enabled ) {
        Jerial.println( "Logic analyzer disabled, deinitializing..." );
        la_enabled = false;
        return CMD_DONT_SHOW_MENU;
    } else {
        changeTerminalColor( 196, true, &Jerial );
        Jerial.println( "Logic analyzer enabled" );
        Jerial.println( "Note: Logic analyzer is not yet fully functional and can make a mess of your memory" );
        Jerial.println( "Make sure to save anything important on the file system before playing with it" );
        Jerial.println( "Worst case, you can use this to nuke the flash and start fresh:" );
        changeTerminalColor( 39, true, &Jerial );
        Jerial.println( "https://github.com/Gadgetoid/pico-universal-flash-nuke/releases/latest" );
        changeTerminalColor( -1, true, &Jerial );
        la_enabled = true;
    }
    return CMD_SHOW_MENU;
}

CommandResult cmd_showBoardLEDs( char c, const String& line ) {
    // Use scrolling region approach for LED dump display
    // ledDumpEnabled is defined in Graphics.cpp
    setLedDumpEnabled( !ledDumpEnabled );
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_startupAnimation( char c, const String& line ) {
    // pauseCore2 is defined in externVars.h as volatile bool
    pauseCore2 = 1;
    delay( 1 );
    drawAnimatedImage( 0 );
    pauseCore2 = 0;
    return CMD_DONT_SHOW_MENU;
}

// Advanced/Test commands
CommandResult cmd_testStates( char c, const String& line ) {
    Jerial.println( "\n\r╭────────────────────────────────────╮" );
    Jerial.println( "│   States System Test (J command)  │" );
    Jerial.println( "╰────────────────────────────────────╯\n\r" );

    SlotManager& mgr = SlotManager::getInstance( );
    JumperlessState& state = mgr.getActiveState( );

    String commandLine = line;
    if ( commandLine.length( ) > 1 ) {
        commandLine = commandLine.substring( 1 ); // Remove 'J'
        commandLine.trim( );

        if ( commandLine.length( ) > 0 ) {
            Jerial.println( "Parsing connections: " + commandLine );

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

                        Jerial.print( "  Adding connection: " + String( node1 ) + "-" + String( node2 ) + "... " );

                        if ( state.addConnection( node1, node2, errorMsg ) ) {
                            Jerial.println( "✓ Success" );
                            connectionsAdded++;
                        } else {
                            Jerial.println( "✗ Failed" );
                            Jerial.println( "    Error: " + errorMsg );
                        }
                    } else {
                        Jerial.println( "  Invalid format: " + conn + " (should be N1-N2)" );
                    }
                }

                startIdx = commaIdx + 1;
            }

            if ( connectionsAdded > 0 ) {
                Jerial.println( "\n\r─── Applying to Hardware ───" );
                Jerial.print( "Refreshing connections... " );
                state.markDirty( );
                refreshConnections( -1 );
                Jerial.println( "✓ Done" );
            }

            Jerial.println( "\n\r─── Current State ───" );
            Jerial.println( "Connections: " + String( state.connections.numBridges ) );
            Jerial.println( "Active Slot: " + String( mgr.getActiveSlot( ) ) );

            if ( state.connections.numBridges > 0 ) {
                Jerial.println( "\n\rConnections in state:" );
                for ( int i = 0; i < state.connections.numBridges; i++ ) {
                    int n1 = state.connections.bridges[ i ][ 0 ];
                    int n2 = state.connections.bridges[ i ][ 1 ];
                    int dup = state.connections.bridges[ i ][ 2 ];
                    Jerial.print( "  " + String( i + 1 ) + ". " );
                    Jerial.print( String( n1 ) + "-" + String( n2 ) );
                    if ( dup > 1 ) {
                        Jerial.print( " (x" + String( dup ) + " duplicates)" );
                    }
                    Jerial.println( );
                }
            }

            Jerial.println( "\n\r─── Testing YAML Jerialization ───" );
            String yamlOutput;
            if ( state.toYAML( yamlOutput ) ) {
                Jerial.println( "YAML output:" );
                Jerial.println( yamlOutput );

                Jerial.println( "\n\r─── Testing Slot Save ───" );
                Jerial.print( "Saving to slot 7... " );
                if ( mgr.saveSlot( 7, errorMsg ) ) {
                    Jerial.println( "✓ Success" );
                    Jerial.println( "  File: /slots/slot7.yaml" );

                    Jerial.print( "Loading from slot 7... " );
                    if ( mgr.loadSlot( 7, errorMsg ) ) {
                        Jerial.println( "✓ Success" );
                        Jerial.println( "  Loaded " + String( mgr.getActiveState( ).connections.numBridges ) + " connections" );
                    } else {
                        Jerial.println( "✗ Failed" );
                        Jerial.println( "  Error: " + errorMsg );
                    }
                } else {
                    Jerial.println( "✗ Failed" );
                    Jerial.println( "  Error: " + errorMsg );
                }
            } else {
                Jerial.println( "Failed to Jerialize to YAML" );
            }

            Jerial.println( "\n\r─── Memory Usage ───" );
            Jerial.println( "Active state RAM: ~" + String( mgr.getActiveStateRAMUsage( ) ) + " bytes" );
            Jerial.println( "State object size: ~" + String( state.estimateRAMUsage( ) ) + " bytes" );
            Jerial.println( "\n\r─── Test Complete ───" );
        } else {
            Jerial.println( "No connections specified!" );
            Jerial.println( "Usage: J 1-2  or  J 1-5,10-20,15-30" );
        }
    } else {
        Jerial.println( "States System Test Command" );
        Jerial.println( "\n\rUsage:" );
        Jerial.println( "  J 1-2              - Add connection 1-2" );
        Jerial.println( "  J 1-5,10-20        - Add multiple connections" );
        Jerial.println( "  J 1-5,1-5,1-5      - Add duplicates (increments count)" );
        Jerial.println( "\n\rFeatures:" );
        Jerial.println( "  • Validates connections" );
        Jerial.println( "  • Tracks duplicate counts" );
        Jerial.println( "  • YAML Jerialization" );
        Jerial.println( "  • Save/load from slots" );
        Jerial.println( "  • Undo/redo history" );
        Jerial.println( "\n\rExample:" );
        Jerial.println( "  J 1-5              - Creates connection 1-5" );
        Jerial.println( "  J TOP_RAIL-10      - Connects top rail to row 10" );
        Jerial.println( "  J GND-32           - Connects ground to row 32" );
    }

    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_printYAML( char c, const String& line ) {
    extern JumperlessState globalState;
    // Jerial.println( "\n\r╭────────────────────────────────────╮" );
    // Jerial.println( "│      Current YAML State (RAM)      │" );
    // Jerial.println( "╰────────────────────────────────────╯\n\r" );
    Serial.println();

    // Jerial.print( "Active Slot: " );
    // Jerial.println( netSlot );
    // Jerial.print( "Dirty Flag: " );
    // Jerial.println( globalState.isDirty( ) ? "YES (will auto-save)" : "NO (saved)" );

    if ( globalState.isDirty( ) ) {
        unsigned long timeSince = millis( ) - globalState.getLastModifiedTime( );
        Jerial.print( "Time since last change: " );
        Jerial.print( timeSince / 1000 );
        Jerial.println( " seconds" );
    }

    // Jerial.println( "\n\r─── YAML Output ───\n\r" );

    // Parse Y0/Y1/Y2: 0=no color, 1=colored hex (default), 2=colored blocks (read from serial)
    int showANSI = 2;
    if ( Jerial.available( ) > 0 ) {
        char arg = Jerial.read( );
        if ( arg == '0' ) showANSI = 0;
        else if ( arg == '2' ) showANSI = 2;
        else if ( arg == '1' ) showANSI = 1;
        // else: newline/space/other - keep default 1, arg already consumed
    }

    String yamlOutput;
    if ( globalState.toYAML( yamlOutput, showANSI ) ) {
        Jerial.println( yamlOutput );
    } else {
        Jerial.println( "✗ Failed to generate YAML" );
    }

    // Jerial.println( "\n\r─── Memory Usage ───" );
    // Jerial.print( "Connections: " );
    // Jerial.println( globalState.connections.numBridges );
    // Jerial.print( "State RAM: ~" );
    // Jerial.print( globalState.estimateRAMUsage( ) );
    // Jerial.println( " bytes" );

    Jerial.println( "\n\r" );
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_loadYAMLState( char c, const String& line ) {
    Jerial.print( "\n\rPaste YAML state (end with empty line):\n\r" );

    String yamlBuffer;
    yamlBuffer.reserve( 16384 );

    unsigned long startTime = millis( );
    const unsigned long timeout = 30000;

    while ( millis( ) - startTime < timeout ) {
        if ( Serial.available( ) ) {
            String inputLine = Serial.readStringUntil( '\n' );
            inputLine.trim( );

            if ( inputLine.length( ) == 0 && yamlBuffer.length( ) > 0 )
                break;

            yamlBuffer += inputLine + "\n";
            startTime = millis( );
        }
        delay( 1 );
    }

    if ( yamlBuffer.length( ) == 0 ) {
        Jerial.print( "\r\nNo YAML received\n\r" );
        return CMD_SHOW_MENU;
    }

    Jerial.print( "\r\nApplying state...\n\r" );

    String errorMsg;
    if ( !globalState.fromYAML( yamlBuffer, errorMsg ) ) {
        Jerial.print( "Error: " );
        Jerial.print( errorMsg );
        Jerial.print( "\n\r" );
        return CMD_SHOW_MENU;
    }

    initializeFakeGpioFromLoadedState( );
    refreshConnections( -1, 1, 1 );
    finalizeFakeGpioAfterRouting( );
    applyStateToHardware( );

    Jerial.print( "State applied successfully!\n\r" );
    return CMD_SHOW_MENU;
}

CommandResult cmd_rawSpeedTest( char c, const String& line ) {
    Jerial.println( "Raw speed test..." );
    Jerial.println( "Read frequency on row 29\n\n\r" );

    // pauseCore2 is defined in externVars.h as volatile bool
    pauseCore2 = true;
    unsigned long cycles = 1000000;
    unsigned long start = micros( );
    sendXYraw( 10, 0, 4, 1 );
    for ( unsigned long i = 0; i < cycles; i++ ) {
        sendXYraw( 10, 0, 0, 1 );
        sendXYraw( 10, 0, 0, 0 );
    }
    unsigned long end = micros( );
    Jerial.print( "Time for " );
    Jerial.print( cycles );
    Jerial.print( " on off cycles: " );
    Jerial.print( end - start );
    Jerial.println( " microseconds" );
    Jerial.print( "Time per cycle: " );
    Jerial.print( ( end - start ) / cycles );
    Jerial.println( " microseconds" );
    Jerial.print( "Frequency: " );
    Jerial.print( ( (float)cycles / (float)( end - start ) ) * 1000 );
    Jerial.println( " kHz\n\r" );
    Jerial.flush( );
    pauseCore2 = false;

    return CMD_SHOW_MENU;
}

CommandResult cmd_printColorSpectrum( char c, const String& line ) {
    // These are already declared at the top of the file as const
    for ( int i = 0; i < highSaturationSpectrumColorsCount; i++ ) {
        changeTerminalColorHighSat( i, true, &Jerial, 0 );
        Jerial.print( i );
        Jerial.print( ": " );
        if ( i < 10 ) {
            Jerial.print( " " );
        }
        Jerial.print( highSaturationSpectrumColors[ i ] );

        Jerial.print( "\t\t" );
        if ( i < highSaturationBrightColorsCount ) {
            changeTerminalColorHighSat( i, true, &Jerial, 1 );
            Jerial.print( i );
            Jerial.print( ": " );
            if ( i < 10 ) {
                Jerial.print( " " );
            }
            Jerial.print( highSaturationBrightColors[ i ] );
        }
        Jerial.println( );
    }

    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_dumpOLED( char c, const String& line ) {
    Jerial.println( "\n\r" );
    oled.dumpFrameBuffer( );
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_printMicrosPerByte( char c, const String& line ) {
    printMicrosPerByte( );
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_printTextFromMenu( char c, const String& line ) {
    while ( Jerial.available( ) == 0 && slotChanged == 0 ) {
        if ( slotChanged == 1 ) {
            // Early exit handled by slotChanged
        }
    }
    printTextFromMenu( );

    clearLEDs( );
    extern volatile int showLEDsCore2;
    // defconDisplay is defined in Menus.h as int&
    extern int& defconDisplay;
    showLEDsCore2 = 1;
    defconDisplay = -1;

    return CMD_SHOW_MENU;
}

CommandResult cmd_wavegen( char c, const String& line ) {
    // This is the complex wavegen test code from 'G' command
    // For now we'll just call reload config
    return cmd_reloadConfig( c, line );
}

CommandResult cmd_dmxJerial( char c, const String& line ) {
    runApp( -1, (char*)"DMX Jerial" );
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_userFunction( char c, const String& line ) {
    handleUserFunction( );
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_erattaClear( char c, const String& line ) {
    erattaClearGPIO( -1 );
    Jerial.println( "Eratta cleared" );
    Jerial.flush( );
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_printWireStatus( char c, const String& line ) {
    printWireStatus( );
    return CMD_DONT_SHOW_MENU;
}

// Stub implementation for cmd_dmxSerial
CommandResult cmd_dmxSerial( char c, const String& line ) {
    Jerial.println( "DMX Serial functionality not yet implemented" );
    return CMD_DONT_SHOW_MENU;
}

CommandResult cmd_uartStats( char c, const String& line ) {
#if ASYNC_PASSTHROUGH_ENABLED == 1
    // Display AsyncPassthrough statistics
    // AsyncPassthrough::printStatistics();

    // // Offer to clear statistics
    // Jerial.print("Press 'c' to clear statistics, any other key to continue: ");
    // Jerial.flush();

    // unsigned long timer = millis();
    // while (Jerial.available() == 0 && millis() - timer < 3000) {
    //     // Wait for input with timeout
    // }

    // if (Jerial.available() > 0) {
    //     char response = Jerial.read();
    //     Jerial.println(response);
    //     if (response == 'c' || response == 'C') {
    //         AsyncPassthrough::clearStatistics();
    //         Jerial.println("✓ Statistics cleared");
    //     }
    // } else {
    //     Jerial.println("(timeout)");
    // }
#else
    Jerial.println( "AsyncPassthrough is not enabled" );
#endif
    return CMD_DONT_SHOW_MENU;
}
