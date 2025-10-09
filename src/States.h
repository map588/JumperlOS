// SPDX-License-Identifier: MIT
#ifndef STATES_H
#define STATES_H

#include <Arduino.h>
#include <string.h>
#include "JumperlessDefines.h"
#include "MatrixState.h"
#include "LEDs.h"
#include "CH446Q.h"
#include <YAMLDuino.h>


/*
This is the new target YAML format for the states system

everything is optional, it'll default to the config values

make sure it's easy to add new fields in the future

Example YAML format:
version: 2
sourceOfTruth: bridges  # or 'nets'

bridges:
  - {n1: 1, n2: 5, dup: 2}
  - {n1: 10, n2: 20, dup: 3}

nets:  # optional, for colors/names
  - {num: 6, name: "Signal A", color: 0xFF0000, nodes: [1, 5]}
  - {num: 7, name: "Signal B", nodes: [10, 20]}

power:
  topRail: 5.0
  bottomRail: 0.0
  dac0: 3.33
  dac1: 0.0

config:
  routing: {stackPaths: 2, stackRails: 3, stackDacs: 0, railPriority: 1}
  gpio:
    direction: [1,1,1,1,1,1,1,1,1,1]
    pulls: [0,0,0,0,0,0,0,0,0,0]
  uart: {txFunction: 0, rxFunction: 1}
  oled: {connected: false, lockConnection: false}

*/




// History buffer size (adjustable for undo/redo)
#ifndef STATE_HISTORY_SIZE
#define STATE_HISTORY_SIZE 0
#endif

// Forward declarations
class JumperlessState;
class SlotManager;

// Source of truth for state reconciliation
enum SourceOfTruth {
    BRIDGES_PRIMARY,  // Bridges define connections, nets computed from bridges
    NETS_PRIMARY      // Nets define connections, bridges generated from nets
};

/**
 * @brief Stores the connection topology: bridges, nets, and routing paths
 * This is the core of what makes a "slot" unique
 */
struct ConnectionState {
    // STORED: Essential data (saved to YAML)
    int bridges[MAX_BRIDGES][3];  // [bridge_index][node1, node2, duplicates]
    int numBridges;
    
    netStruct nets[MAX_NETS];  // User-defined nets (optional, for colors/names/reference)
    int numNets;  // Derived count
    
    // COMPUTED: Runtime caches (NOT saved to YAML, computed from bridges/nets)
    pathStruct paths[MAX_BRIDGES];
    int numPaths;
    bool pathsCacheValid;  // Set to false when connections change
    
    chipStatus chipStates[12];  // Derived from paths
    struct justXY chipXY[12];   // Crossbar switch states (reconstructed from paths)
    bool chipStatesCacheValid;
    
    ConnectionState();
    void clear();
    void invalidateCache(bool autoRefresh = true);  // Optional auto-refresh on invalidate
    void recomputePaths();  // Calls bridgesToPaths() to rebuild paths from current state
    void syncBridgesFromNets();  // Generate bridges from net node lists
    void syncNetsFromBridges();  // Generate nets from bridges
};

/**
 * @brief Power supply voltages for all DACs and rails
 */
struct PowerState {
    float topRail;      // Top breadboard rail voltage
    float bottomRail;   // Bottom breadboard rail voltage
    float dac0;         // DAC 0 voltage
    float dac1;         // DAC 1 voltage
    
    PowerState();
    void setDefaults();
    bool validate(String& errorMsg) const;
};

/**
 * @brief Display-related state (colors, brightness, etc.)
 */
struct DisplayState {
    // Net colors - only stored if manually changed from default
    struct NetColorEntry {
        int netNumber;
        rgbColor color;
        uint32_t rawColor;
        char colorName[32];
    };
    
    NetColorEntry customColors[MAX_NETS];
    int numCustomColors;
    
    DisplayState();
    void clear();
    bool hasCustomColors() const { return numCustomColors > 0; }
    void setNetColor(int netNum, rgbColor color, uint32_t raw, const char* name);
    bool getNetColor(int netNum, rgbColor& color, uint32_t& raw, char* name) const;
    void removeNetColor(int netNum);
};

/**
 * @brief Configuration options that affect routing and behavior
 */
struct ConfigState {
    // State reconciliation mode
    SourceOfTruth sourceOfTruth;  // Which representation is authoritative
    
    // Routing preferences
    int stackPaths;     // How many paths to stack (0-3)
    int stackRails;     // How many rail connections to stack (0-3)
    int stackDacs;      // How many DAC connections to stack (0-3)
    int railPriority;   // Priority for rail connections
    
    // GPIO configuration (10 GPIOs: GP1-GP8, UART_TX, UART_RX)
    int gpioDirection[10];      // 0 = output, 1 = input
    int gpioPulls[10];          // 0 = pull down, 1 = pull up, 2 = none, 3 = bus keeper
    float gpioPwmFrequency[10]; // PWM frequency in Hz
    float gpioPwmDutyCycle[10]; // PWM duty cycle (0.0 to 1.0)
    bool gpioPwmEnabled[10];    // PWM enabled flag
    
    // UART configuration
    int uartTxFunction;  // 0 = tx, 1 = rx, 2 = gpio_in, 3 = gpio_out
    int uartRxFunction;
    
    // OLED state
    bool oledConnected;
    bool oledLockConnection;
    
    // Auto-refresh behavior
    bool autoRefreshOnChange;  // Automatically call refreshConnections() when cache is invalidated
    
    ConfigState();
    void setDefaults();
};

/**
 * @brief Complete Jumperless state for a single slot
 * This is the main state container that gets saved/loaded
 */
class JumperlessState {
public:
    ConnectionState connections;
    PowerState power;
    DisplayState display;
    ConfigState config;
    
    // Metadata
    int version;  // State format version for future compatibility
    
    JumperlessState();
    
    // Connection management
    bool addConnection(int node1, int node2, String& errorMsg, int duplicates = -1);  // -1 = use default from config
    bool removeConnection(int node1, int node2, String& errorMsg);
    bool hasConnection(int node1, int node2) const;
    int getConnectionDuplicates(int node1, int node2) const;  // Get number of parallel copies
    bool setConnectionDuplicates(int node1, int node2, int duplicates, String& errorMsg);
    void clearAllConnections();
    
    // Power management
    void setDacVoltage(int dacNum, float voltage);
    float getDacVoltage(int dacNum) const;
    void setRailVoltage(bool isTopRail, float voltage);
    float getRailVoltage(bool isTopRail) const;
    
    // Configuration
    void setPathStacking(int paths, int rails, int dacs);
    void getPathStacking(int& paths, int& rails, int& dacs) const;
    void setAutoRefresh(bool enabled) { config.autoRefreshOnChange = enabled; }
    bool getAutoRefresh() const { return config.autoRefreshOnChange; }
    void setSourceOfTruth(SourceOfTruth source) { config.sourceOfTruth = source; markDirty(); }
    SourceOfTruth getSourceOfTruth() const { return config.sourceOfTruth; }
    
    // GPIO
    void setGpioDirection(int gpio, int direction);
    int getGpioDirection(int gpio) const;
    void setGpioPull(int gpio, int pull);
    int getGpioPull(int gpio) const;
    
    // Display
    void setNetColor(int netNum, rgbColor color, uint32_t raw, const char* name);
    bool getNetColor(int netNum, rgbColor& color, uint32_t& raw, char* name) const;
    
    // Dirty flag management (for lazy writes)
    void markDirty() { dirty = true; lastModifiedTime = millis(); }
    bool isDirty() const { return dirty; }
    void clearDirty() { dirty = false; }
    unsigned long getLastModifiedTime() const { return lastModifiedTime; }
    
    // Validation
    bool validate(String& errorMsg) const;
    
    // Serialization (YAML format)
    bool toYAML(String& output) const;
    bool fromYAML(const String& input, String& errorMsg);
    
    // Legacy format support
    bool fromLegacyNodeFile(const String& nodeFileContent, String& errorMsg);
    
    // State comparison
    bool operator==(const JumperlessState& other) const;
    bool operator!=(const JumperlessState& other) const { return !(*this == other); }
    
    // Utility
    void clear();
    size_t estimateRAMUsage() const;
    
private:
    // Dirty flag infrastructure
    bool dirty;
    unsigned long lastModifiedTime;
    
    // Helper for validation
    bool isNodeValid(int node) const;
    bool isConnectionAllowed(int node1, int node2, String& errorMsg) const;
    
    // YAML serialization helpers
    void serializeBridges(String& output) const;
    bool deserializeBridges(const char* yamlContent, String& errorMsg);
    void serializeNets(String& output) const;
    bool deserializeNets(const char* yamlContent, String& errorMsg);
    void serializePower(String& output) const;
    bool deserializePower(const char* yamlContent, String& errorMsg);
    void serializeConfig(String& output) const;
    bool deserializeConfig(const char* yamlContent, String& errorMsg);
};

/**
 * @brief Manages slot files and active state
 * Singleton pattern - use SlotManager::getInstance()
 */
class SlotManager {
public:
    // Get singleton instance
    static SlotManager& getInstance();
    
    // Prevent copying
    SlotManager(const SlotManager&) = delete;
    SlotManager& operator=(const SlotManager&) = delete;
    
    // Active state access
    JumperlessState& getActiveState();
    const JumperlessState& getActiveState() const;
    int getActiveSlot() const { return activeSlotNumber; }
    
    // Slot management
    bool loadSlot(int slotNum, String& errorMsg);
    bool saveSlot(int slotNum, String& errorMsg);
    bool saveActiveSlot(String& errorMsg);
    bool slotExists(int slotNum) const;
    bool deleteSlot(int slotNum, String& errorMsg);
    void clearActiveSlot();
    
    // Slot tracking synchronization
    void setActiveSlot(int slotNum);  // Sets activeSlotNumber and syncs with global netSlot
    void syncFromGlobalNetSlot();     // Updates activeSlotNumber from netSlot (for external changes)
    
    // Create slot files on-demand (not pre-created)
    bool ensureSlotExists(int slotNum);
    
    // History management (undo/redo)
    void pushHistory();
    bool canUndo() const;
    bool canRedo() const;
    bool undo(String& errorMsg);
    bool redo(String& errorMsg);
    void clearHistory();
    int getHistorySize() const { return STATE_HISTORY_SIZE; }
    int getHistoryDepth() const;  // How many undo steps are available
    
    // Legacy support
    bool migrateOldSlotFile(int slotNum, String& errorMsg);
    
    // Utility
    void printSlotInfo(int slotNum);
    void listSlots();
    size_t getActiveStateRAMUsage() const;
    
private:
    SlotManager();
    ~SlotManager() = default;
    
    // State storage - reference to globalState (no duplication!)
    JumperlessState& activeState;
    int activeSlotNumber;
    
    // History buffer (circular buffer for undo/redo)
    JumperlessState* historyBuffer;
    int historySize;
    int historyHead;       // Points to next write position
    int historyCount;      // Number of valid history entries
    int historyPosition;   // Current position in history (for redo)
    
    // File I/O helpers
    String getSlotFilename(int slotNum) const;  // Returns .yaml filename
    String getLegacySlotFilename(int slotNum) const;  // Returns old .txt filename
    String getJSONSlotFilename(int slotNum) const;  // Returns old .json filename
    bool readSlotFile(int slotNum, String& content, String& errorMsg);
    bool writeSlotFile(int slotNum, const String& content, String& errorMsg);
    
    // History helpers
    void initHistory();
    void cleanupHistory();
    int historyIndex(int offset) const;  // Helper for circular buffer indexing
};

// Global singleton declaration - THE single source of truth for all Jumperless state
// This replaces the old global arrays (net[], path[], ch[]) with a unified state object
extern JumperlessState globalState;

// Global helpers for easy access
namespace StateHelpers {
    // Quick access to common operations
    inline JumperlessState& getState() { return SlotManager::getInstance().getActiveState(); }
    inline bool addConnection(int n1, int n2, int dup = -1) { String err; return getState().addConnection(n1, n2, err, dup); }
    inline bool removeConnection(int n1, int n2) { String err; return getState().removeConnection(n1, n2, err); }
    inline void setDac(int dac, float voltage) { getState().setDacVoltage(dac, voltage); }
    inline void setRail(bool top, float voltage) { getState().setRailVoltage(top, voltage); }
    inline int getDuplicates(int n1, int n2) { return getState().getConnectionDuplicates(n1, n2); }
    inline bool setDuplicates(int n1, int n2, int dup) { String err; return getState().setConnectionDuplicates(n1, n2, dup, err); }
    
    // Undo/redo shortcuts
    inline bool undo() { String err; return SlotManager::getInstance().undo(err); }
    inline bool redo() { String err; return SlotManager::getInstance().redo(err); }
    
    // Slot operations
    inline bool saveSlot(int slot) { String err; return SlotManager::getInstance().saveSlot(slot, err); }
    inline bool loadSlot(int slot) { String err; return SlotManager::getInstance().loadSlot(slot, err); }
}

#endif // STATES_H

