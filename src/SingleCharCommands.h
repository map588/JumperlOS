/**
 * @file SingleCharCommands.h
 * @brief Single-character command system for Jumperless main menu
 * 
 * This file provides a clean, OOP-based command registration and execution system.
 * Commands can be easily added/removed and automatically appear in the menu with
 * their descriptions and help text.
 */

#ifndef SINGLE_CHAR_COMMANDS_H
#define SINGLE_CHAR_COMMANDS_H

#include <Arduino.h>
#include "JumperlOS.h"
#include "Jerial.h"

// Forward declarations
class SingleCharCommands;

/**
 * @brief Command execution result
 */
enum CommandResult {
    CMD_DONT_SHOW_MENU = 0,  // Skip menu display (goto dontshowmenu)
    CMD_SHOW_MENU = 1,       // Show menu (goto menu)
    CMD_LOAD_FILE = 2        // Load file and refresh connections (goto loadfile)
};

/**
 * @brief Command callback function type
 * @param cmdChar The character that triggered this command
 * @param commandLine The full command line (for line-buffering mode)
 * @return CommandResult indicating next action
 */
typedef CommandResult (*CommandCallback)(char cmdChar, const String& commandLine);

/**
 * @brief Menu category levels (for progressive disclosure)
 */
enum MenuLevel {
    MENU_BASIC = 0,      // Always shown
    MENU_STANDARD = 1,   // Shown at 'e' level 1
    MENU_ADVANCED = 2,   // Shown at 'e' level 2
    MENU_DEBUG = 3       // Shown at 'e' level 3
};

/**
 * @brief Command category for organization
 */
enum CommandCategory {
    CAT_CONNECTIONS,     // Connection management (f, +, -, x, y)
    CAT_DISPLAY,         // Display and UI (m, n, b, c, s)
    CAT_PYTHON,          // Python/scripting (p, P, >)
    CAT_FILE_SYSTEM,     // File operations (/, U, u, G)
    CAT_HARDWARE,        // Hardware control (r, a, A, v, @)
    CAT_DEBUG,           // Debug and diagnostics (d, ?, X, g)
    CAT_SETTINGS,        // Settings and config (`, ~, l, e, C, E)
    CAT_APPS,            // Apps and special modes (w, ., k, R)
    CAT_ADVANCED         // Advanced/experimental
};

/**
 * @brief Command registration structure
 * 
 * This structure defines everything needed for a single-character command:
 * - What character triggers it
 * - What it does (description)
 * - How to use it (help text)
 * - Where to show it (menu level and category)
 * - What to execute (callback function)
 */
struct Command {
    char trigger;                    // Character that triggers this command
    const char* shortDesc;           // Short description for menu (e.g., "show this menu")
    const char* helpText;            // Detailed help text
    CommandCallback callback;        // Function to execute
    MenuLevel menuLevel;             // Minimum menu level to display
    CommandCategory category;        // Category for organization
    bool showInMenu;                 // Whether to display in menu
    
    Command() : trigger('\0'), shortDesc(nullptr), helpText(nullptr), 
                callback(nullptr), menuLevel(MENU_BASIC), 
                category(CAT_CONNECTIONS), showInMenu(true) {}
    
    Command(char t, const char* desc, const char* help, CommandCallback cb, 
            MenuLevel level = MENU_BASIC, CommandCategory cat = CAT_CONNECTIONS, 
            bool show = true)
        : trigger(t), shortDesc(desc), helpText(help), callback(cb),
          menuLevel(level), category(cat), showInMenu(show) {}
};

/**
 * @brief Single-character command service
 * 
 * This service handles all single-character commands in the main menu.
 * It manages command registration, menu display, and command execution.
 * 
 * Features:
 * - Easy command registration with descriptions and help
 * - Automatic menu generation from registered commands
 * - Progressive disclosure (extra menu levels)
 * - Clean separation of concerns (callbacks in appropriate files)
 * - Integration with jumperlOS service system
 */
class SingleCharCommands : public Service {
public:
    static const int MAX_COMMANDS = 128;
    
    SingleCharCommands();
    ~SingleCharCommands() override = default;
    
    // Service interface
    ServiceStatus service() override;
    const char* getName() const override { return "SingleCharCommands"; }
    ServicePriority getPriority() const override { return ServicePriority::NORMAL; }
    
    /**
     * @brief Register a new command
     * @param cmd Command structure with all details
     * @return true if registered successfully
     */
    bool registerCommand(const Command& cmd);
    
    /**
     * @brief Register a command with all parameters
     */
    bool registerCommand(char trigger, const char* shortDesc, const char* helpText,
                        CommandCallback callback, MenuLevel level = MENU_BASIC,
                        CommandCategory category = CAT_CONNECTIONS, bool showInMenu = true);
    
    /**
     * @brief Unregister a command by character
     */
    bool unregisterCommand(char trigger);
    
    /**
     * @brief Execute a command by character
     * @param cmdChar Character that triggered the command
     * @param commandLine Full command line (for line-buffering mode)
     * @return CommandResult indicating next action
     */
    CommandResult executeCommand(char cmdChar, const String& commandLine);
    
    /**
     * @brief Print the main menu with current commands
     * @param extraMenuLevel Current extra menu level (0-3)
     */
    void printMenu(int extraMenuLevel);
    
    /**
     * @brief Print help for a specific command
     * @param cmdChar Command character
     */
    void printCommandHelp(char cmdChar);
    
    /**
     * @brief Print help for all commands (or by category)
     * @param category Optional category filter (-1 for all)
     */
    void printAllHelp(int category = -1);
    
    /**
     * @brief Get command by character
     * @return Pointer to command, or nullptr if not found
     */
    const Command* getCommand(char trigger) const;
    
    /**
     * @brief Get all commands for a category
     */
    int getCommandsByCategory(CommandCategory cat, const Command** outCommands, int maxCount) const;
    
    /**
     * @brief Get number of registered commands
     */
    int getCommandCount() const { return commandCount; }
    
    /**
     * @brief Initialize all built-in commands
     * Called automatically in constructor
     */
    void initializeCommands();
    
private:
    Command commands[MAX_COMMANDS];
    int commandCount;
    
    /**
     * @brief Find command index by character
     * @return Index, or -1 if not found
     */
    int findCommandIndex(char trigger) const;
    
    /**
     * @brief Sort commands by category and trigger
     */
    void sortCommands();
    
    // /**
    //  * @brief Print a menu line with color cycling
    //  */
    // int printMenuLine(const char* text, int extraMenuLevel, MenuLevel requiredLevel);
};

// Global instance
extern SingleCharCommands singleCharCommands;

// Global state tracking
extern volatile bool inMainMenu;     // True when main menu can accept input
extern String currentCommandLine;    // Current command line being processed

// ============================================================================
// Command Callback Function Declarations
// ============================================================================
// These are implemented in their respective files or in SingleCharCommands.cpp

// Connection commands (Commands.cpp)
CommandResult cmd_clearConnections(char c, const String& line);
CommandResult cmd_addConnections(char c, const String& line);
CommandResult cmd_removeConnections(char c, const String& line);
CommandResult cmd_loadNodeFile(char c, const String& line);
CommandResult cmd_refreshConnections(char c, const String& line);
CommandResult cmd_cycleSlots(char c, const String& line);
CommandResult cmd_loadSlot(char c, const String& line);
CommandResult cmd_parseWokwi(char c, const String& line);

// Display commands (main.cpp / Display related)
CommandResult cmd_showMenu(char c, const String& line);
CommandResult cmd_showNetlist(char c, const String& line);
CommandResult cmd_showJsonState(char c, const String& line);
CommandResult cmd_loadJsonState(char c, const String& line);
CommandResult cmd_showBridgeArray(char c, const String& line);
CommandResult cmd_showCrossbar(char c, const String& line);
CommandResult cmd_showSlots(char c, const String& line);
CommandResult cmd_queryActiveSlot(char c, const String& line);
CommandResult cmd_toggleExtraMenu(char c, const String& line);

// Python commands (JumperlessMicroPythonAPI.cpp)
CommandResult cmd_pythonREPL(char c, const String& line);
CommandResult cmd_psramTest(char c, const String& line);
CommandResult cmd_pythonCommand(char c, const String& line);

// File system commands
CommandResult cmd_showFilesystem(char c, const String& line);
CommandResult cmd_enableUSBStorage(char c, const String& line);
CommandResult cmd_disableUSBStorage(char c, const String& line);
CommandResult cmd_listFilesystem(char c, const String& line);

// Config commands (PersistentStuff.cpp)
CommandResult cmd_printConfig(char c, const String& line);
CommandResult cmd_editConfig(char c, const String& line);
CommandResult cmd_reloadConfig(char c, const String& line);

// Hardware commands (Peripherals.cpp, etc)
CommandResult cmd_resetArduino(char c, const String& line);
CommandResult cmd_connectArduino(char c, const String& line);
CommandResult cmd_disconnectArduino(char c, const String& line);
CommandResult cmd_readADC(char c, const String& line);
CommandResult cmd_setDAC(char c, const String& line);
CommandResult cmd_i2cScan(char c, const String& line);
CommandResult cmd_calibrateDACs(char c, const String& line);

// Debug commands
CommandResult cmd_showVersion(char c, const String& line);
CommandResult cmd_setDebugFlags(char c, const String& line);
CommandResult cmd_statusDiagnosticsMenu(char c, const String& line);
CommandResult cmd_resourceStatus(char c, const String& line);
CommandResult cmd_gpioState(char c, const String& line);
CommandResult cmd_usbDebugMenu(char c, const String& line);
CommandResult cmd_uartStats(char c, const String& line);
CommandResult cmd_printWireStatus(char c, const String& line);
// Settings commands
CommandResult cmd_ledBrightness(char c, const String& line);
CommandResult cmd_toggleOLED(char c, const String& line);
CommandResult cmd_toggleTerminalColors(char c, const String& line);
CommandResult cmd_dontShowMenu(char c, const String& line);
CommandResult cmd_oledInTerminal(char c, const String& line);
CommandResult cmd_cycleFont(char c, const String& line);

// App/Special mode commands
CommandResult cmd_logicAnalyzer(char c, const String& line);
CommandResult cmd_showBoardLEDs(char c, const String& line);
CommandResult cmd_startupAnimation(char c, const String& line);
CommandResult cmd_cycleSlots(char c, const String& line);
CommandResult cmd_loadSlot(char c, const String& line);

// Advanced/Test commands
CommandResult cmd_testStates(char c, const String& line);
CommandResult cmd_testOverlay(char c, const String& line);
CommandResult cmd_printYAML(char c, const String& line);
CommandResult cmd_rawSpeedTest(char c, const String& line);
CommandResult cmd_printColorSpectrum(char c, const String& line);
CommandResult cmd_dumpOLED(char c, const String& line);
CommandResult cmd_printMicrosPerByte(char c, const String& line);
CommandResult cmd_printTextFromMenu(char c, const String& line);
CommandResult cmd_wavegen(char c, const String& line);
CommandResult cmd_dmxSerial(char c, const String& line);
CommandResult cmd_userFunction(char c, const String& line);
CommandResult cmd_erattaClear(char c, const String& line);
CommandResult cmd_printTextFromTerminal(char c, const String& line);



CommandResult cmd_showSwitchPosition(char c, const String& line);
#endif // SINGLE_CHAR_COMMANDS_H

