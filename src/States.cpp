// SPDX-License-Identifier: MIT
#include "States.h"
#include "MatrixState.h"
#include "NetManager.h"
#include "FileParsing.h"
#include "Commands.h"
#include "Peripherals.h"
#include "PersistentStuff.h"
#include "EkiloEditor.h"
#include "Colors.h"
#include "config.h"
#include "FakeGpio.h"
#include <string.h>
#include <vector>
#include <FatFS.h>
#include <hardware/gpio.h>
#include "externVars.h"  // For fs_mutex filesystem synchronization
#include "FilesystemStuff.h"  // For safe file operations

// ============================================================================
// CRITICAL MEMORY SAFETY NOTES
// ============================================================================
//
// JumperlessState and ConnectionState contain MASSIVE arrays:
//   - bridges[192][3]        = 2,304 bytes
//   - bridgeColors[192]      = 768 bytes
//   - nets[60]               = ~30 KB (depends on netStruct size)
//   - paths[192]             = ~20 KB (depends on pathStruct size)
//   
// TOTAL SIZE: ~50+ KB per JumperlessState instance!
//
// **NEVER COPY THESE OBJECTS!**
//   - Copying exhausts limited stack memory on embedded systems
//   - Results in crashes, hard faults, or unpredictable behavior
//   - Always use references (&) or pointers (*)
//   - Copy constructor and assignment operator are DELETED to enforce this
//
// CORRECT:   JumperlessState& state = SlotManager::getInstance().getActiveState();
// WRONG:     JumperlessState state = SlotManager::getInstance().getActiveState();  // CRASH!
//
// If you need to pass state to a function:
// CORRECT:   void processState(JumperlessState& state) { ... }
// WRONG:     void processState(JumperlessState state) { ... }  // CRASH!
//
// ============================================================================

extern struct config jumperlessConfig;
extern volatile bool core1busy;
extern volatile bool core2busy;
extern int netSlot;  // Global slot number (defined in RotaryEncoder.cpp)
extern const int gpioDef[10][3];  // GPIO pin definitions (defined in Peripherals.h)
extern uint8_t gpioState[42];  // GPIO state for animations - 10 real + 32 fake (defined in Peripherals.cpp)
extern bool debugFP;  // Debug flag for file parsing (defined in FileParsing.cpp)

// Forward declarations for YAML parsing helpers
int parseNodeName(const String& nodeName);
String nodeValueToString(int nodeValue);
uint32_t parseColorValue(const String& colorStr, bool& success);
String colorValueToName(uint32_t color);
bool parseBoolean(const String& val, bool& success);
String booleanToString(bool value);

// Global singleton - THE single source of truth for all Jumperless state
JumperlessState globalState;

// Set custom net name - stored by NET NUMBER in DisplayState
// Pass empty string or nullptr to clear
// Tracking by net number means names follow nets when they shift!
void setCustomNetName(int netNum, const char* name) {
    if (netNum < 0 || netNum >= MAX_NETS) return;
    
    if (name == nullptr || name[0] == '\0') {
        // Clear custom name
        globalState.display.removeNetName(netNum);
    } else {
        // Store custom name (tracked by net number)
        globalState.display.setNetName(netNum, name);
    }
}

// Check if net has custom name
bool hasCustomNetName(int netNum) {
    if (netNum < 0 || netNum >= MAX_NETS) return false;
    return (globalState.display.getNetName(netNum) != nullptr);
}

// Clear all custom net names
void clearAllCustomNetNames() {
    globalState.display.numCustomNames = 0;
}

// ============================================================================
// ConnectionState Implementation
// ============================================================================

ConnectionState::ConnectionState() {
    clear();
}

void ConnectionState::clear() {
    memset(bridges, 0, sizeof(bridges));
    // Initialize bridgeColors to 0xFFFFFFFF (no color sentinel)
    for (int i = 0; i < MAX_BRIDGES; i++) {
        bridgeColors[i] = 0xFFFFFFFF;
    }
    numBridges = 0;
    numNets = 0;
    numPaths = 0;
    pathsCacheValid = false;
    chipStatesCacheValid = false;
    clearAllNTCC();
    
    // Restore locked connections after clearing
    extern int handleLockedConnections();
    handleLockedConnections();
    return;
    // Clear nets
    for (int i = 0; i < MAX_NETS; i++) {
        memset(&nets[i], 0, sizeof(netStruct));
    }
    
    // Clear paths
    memset(paths, 0, sizeof(paths));
    
    // Clear chip states
    for (int i = 0; i < 12; i++) {
        memset(&chipStates[i], 0, sizeof(chipStatus));
        memset(&chipXY[i], 0, sizeof(struct justXY));
    }
}

void ConnectionState::invalidateCache(bool autoRefresh) {
    pathsCacheValid = false;
    chipStatesCacheValid = false;
    
    // Optionally trigger hardware refresh
    if (autoRefresh) {
        refreshConnections(-1);
    }
}

void ConnectionState::recomputePaths() {
    // This will be called to rebuild paths from bridges
    // The actual computation is done by bridgesToPaths() in NetsToChipConnections.cpp
    pathsCacheValid = false;
    chipStatesCacheValid = false;
}

void ConnectionState::syncBridgesFromNets() {
    // Generate bridges from net node lists
    // Used when NETS_PRIMARY mode is active
    numBridges = 0;
    
    for (int netIdx = 0; netIdx < numNets && netIdx < MAX_NETS; netIdx++) {
        netStruct& currentNet = nets[netIdx];
        
        // Skip empty nets
        if (currentNet.nodes[0] == 0) continue;
        
        // Connect all nodes in this net
        for (int i = 0; i < MAX_NODES && currentNet.nodes[i] != 0; i++) {
            for (int j = i + 1; j < MAX_NODES && currentNet.nodes[j] != 0; j++) {
                if (numBridges >= MAX_BRIDGES) {
                    Serial.println("Warning: Maximum bridges reached during sync");
                    return;
                }
                
                // Add bridge
                bridges[numBridges][0] = currentNet.nodes[i];
                bridges[numBridges][1] = currentNet.nodes[j];
                bridges[numBridges][2] = currentNet.numberOfDuplicates;
                numBridges++;
            }
        }
    }
    
    invalidateCache(false);
}

void ConnectionState::syncNetsFromBridges() {
    // Generate nets from bridges
    // Used when BRIDGES_PRIMARY mode is active (default)
    // This is typically done by the existing net management code
    // We just invalidate to trigger recomputation
    invalidateCache(false);
}

// ============================================================================
// PowerState Implementation
// ============================================================================

PowerState::PowerState() {
    setDefaults();
}

void PowerState::setDefaults() {
    topRail = 0.0f;
    bottomRail = 0.0f;
    dac0 = 3.33f;
    dac1 = 0.0f;
}

bool PowerState::validate(String& errorMsg) const {
    // Check voltage limits (±8V typical for DACs)
    if (dac0 < -8.0f || dac0 > 8.0f) {
        errorMsg = "DAC 0 voltage out of range (-8V to +8V): " + String(dac0, 2) + "V";
        return false;
    }
    if (dac1 < -8.0f || dac1 > 8.0f) {
        errorMsg = "DAC 1 voltage out of range (-8V to +8V): " + String(dac1, 2) + "V";
        return false;
    }
    if (topRail < -8.0f || topRail > 8.0f) {
        errorMsg = "Top rail voltage out of range (-8V to +8V): " + String(topRail, 2) + "V";
        return false;
    }
    if (bottomRail < -8.0f || bottomRail > 8.0f) {
        errorMsg = "Bottom rail voltage out of range (-8V to +8V): " + String(bottomRail, 2) + "V";
        return false;
    }
    return true;
}

// ============================================================================
// DisplayState Implementation
// ============================================================================

DisplayState::DisplayState() {
    clear();
}

void DisplayState::clear() {
    memset(customColors, 0, sizeof(customColors));
    numCustomColors = 0;
    memset(customNames, 0, sizeof(customNames));
    numCustomNames = 0;
}

void DisplayState::setNetColor(int netNum, rgbColor color, uint32_t raw, const char* name) {
    // Get the first node of this net for reconciliation after rebuilds
    int firstNode = 0;
    if (netNum >= 0 && netNum < MAX_NETS && globalState.connections.nets[netNum].nodes[0] != 0) {
        firstNode = globalState.connections.nets[netNum].nodes[0];
    }
    
    // Check if already exists
    for (int i = 0; i < numCustomColors; i++) {
        if (customColors[i].netNumber == netNum) {
            customColors[i].color = color;
            customColors[i].rawColor = raw;
            customColors[i].firstNode = firstNode;
            strncpy(customColors[i].colorName, name, 31);
            customColors[i].colorName[31] = '\0';
            return;
        }
    }
    
    // Add new if space available
    if (numCustomColors < MAX_NETS) {
        customColors[numCustomColors].netNumber = netNum;
        customColors[numCustomColors].firstNode = firstNode;
        customColors[numCustomColors].color = color;
        customColors[numCustomColors].rawColor = raw;
        strncpy(customColors[numCustomColors].colorName, name, 31);
        customColors[numCustomColors].colorName[31] = '\0';
        numCustomColors++;
    }
}

bool DisplayState::getNetColor(int netNum, rgbColor& color, uint32_t& raw, char* name) const {
    for (int i = 0; i < numCustomColors; i++) {
        if (customColors[i].netNumber == netNum) {
            color = customColors[i].color;
            raw = customColors[i].rawColor;
            if (name) {
                strcpy(name, customColors[i].colorName);
            }
            return true;
        }
    }
    return false;
}

void DisplayState::removeNetColor(int netNum) {
    for (int i = 0; i < numCustomColors; i++) {
        if (customColors[i].netNumber == netNum) {
            // Shift remaining entries down
            for (int j = i; j < numCustomColors - 1; j++) {
                customColors[j] = customColors[j + 1];
            }
            numCustomColors--;
            return;
        }
    }
}

void DisplayState::setNetName(int netNum, const char* name) {
    // Get the first node of this net for reconciliation after rebuilds
    int firstNode = 0;
    if (netNum >= 0 && netNum < MAX_NETS && globalState.connections.nets[netNum].nodes[0] != 0) {
        firstNode = globalState.connections.nets[netNum].nodes[0];
    }
    
    // Check if already exists - update it
    for (int i = 0; i < numCustomNames; i++) {
        if (customNames[i].netNumber == netNum) {
            strncpy(customNames[i].name, name, 31);
            customNames[i].name[31] = '\0';
            customNames[i].firstNode = firstNode;
            return;
        }
    }
    
    // Add new entry if space available
    if (numCustomNames < MAX_NETS) {
        customNames[numCustomNames].netNumber = netNum;
        customNames[numCustomNames].firstNode = firstNode;
        strncpy(customNames[numCustomNames].name, name, 31);
        customNames[numCustomNames].name[31] = '\0';
        numCustomNames++;
    }
}

const char* DisplayState::getNetName(int netNum) const {
    for (int i = 0; i < numCustomNames; i++) {
        if (customNames[i].netNumber == netNum) {
            return customNames[i].name;
        }
    }
    return nullptr;  // Not found
}

void DisplayState::removeNetName(int netNum) {
    for (int i = 0; i < numCustomNames; i++) {
        if (customNames[i].netNumber == netNum) {
            // Shift remaining entries down
            for (int j = i; j < numCustomNames - 1; j++) {
                customNames[j] = customNames[j + 1];
            }
            numCustomNames--;
            return;
        }
    }
}

// Debug flag for DisplayState reconciliation
static bool debugReconcile = false;

void DisplayState::reconcileAfterRebuild() {
    // After nets are rebuilt from bridges, net numbers may have changed.
    // Use firstNode to find where each custom entry's net ended up.
    extern int findNodeInNet(int node);
    
    if (debugReconcile) Serial.printf("reconcileAfterRebuild: %d names, %d colors\n", numCustomNames, numCustomColors);
    
    // Reconcile custom names
    for (int i = numCustomNames - 1; i >= 0; i--) {
        if (debugReconcile) Serial.printf("  Name[%d]: net=%d, firstNode=%d, name=%s\n", 
            i, customNames[i].netNumber, customNames[i].firstNode, customNames[i].name);
        
        if (customNames[i].firstNode <= 0) {
            // Entry has no firstNode (loaded from YAML before nets were built)
            // Check if the net still exists at this number - if so, fix the firstNode
            int netNum = customNames[i].netNumber;
            if (netNum >= 0 && netNum < MAX_NETS && 
                globalState.connections.nets[netNum].number > 0 &&
                globalState.connections.nets[netNum].nodes[0] != 0) {
                // Net exists - fix the firstNode
                customNames[i].firstNode = globalState.connections.nets[netNum].nodes[0];
                if (debugReconcile) Serial.printf("    -> FIX firstNode to %d (net exists)\n", customNames[i].firstNode);
            } else {
                if (debugReconcile) Serial.println("    -> SKIP (no firstNode, net doesn't exist)");
            }
            continue;
        }
        
        int newNetNum = findNodeInNet(customNames[i].firstNode);
        if (debugReconcile) Serial.printf("    -> findNodeInNet(%d) = %d\n", customNames[i].firstNode, newNetNum);
        
        if (newNetNum <= 0) {
            if (debugReconcile) Serial.println("    -> REMOVE (node not in any net)");
            for (int j = i; j < numCustomNames - 1; j++) {
                customNames[j] = customNames[j + 1];
            }
            numCustomNames--;
        } else if (newNetNum != customNames[i].netNumber) {
            if (debugReconcile) Serial.printf("    -> UPDATE netNumber %d -> %d\n", customNames[i].netNumber, newNetNum);
            customNames[i].netNumber = newNetNum;
        } else {
            if (debugReconcile) Serial.println("    -> KEEP (unchanged)");
        }
    }
    
    // Reconcile custom colors
    for (int i = numCustomColors - 1; i >= 0; i--) {
        if (debugReconcile) Serial.printf("  Color[%d]: net=%d, firstNode=%d, color=%s\n", 
            i, customColors[i].netNumber, customColors[i].firstNode, customColors[i].colorName);
        
        if (customColors[i].firstNode <= 0) {
            // Entry has no firstNode (loaded from YAML before nets were built)
            // Check if the net still exists at this number - if so, fix the firstNode
            int netNum = customColors[i].netNumber;
            if (netNum >= 0 && netNum < MAX_NETS && 
                globalState.connections.nets[netNum].number > 0 &&
                globalState.connections.nets[netNum].nodes[0] != 0) {
                // Net exists - fix the firstNode
                customColors[i].firstNode = globalState.connections.nets[netNum].nodes[0];
                if (debugReconcile) Serial.printf("    -> FIX firstNode to %d (net exists)\n", customColors[i].firstNode);
            } else {
                if (debugReconcile) Serial.println("    -> SKIP (no firstNode, net doesn't exist)");
            }
            continue;
        }
        
        int newNetNum = findNodeInNet(customColors[i].firstNode);
        if (debugReconcile) Serial.printf("    -> findNodeInNet(%d) = %d\n", customColors[i].firstNode, newNetNum);
        
        if (newNetNum <= 0) {
            if (debugReconcile) Serial.println("    -> REMOVE (node not in any net)");
            for (int j = i; j < numCustomColors - 1; j++) {
                customColors[j] = customColors[j + 1];
            }
            numCustomColors--;
        } else if (newNetNum != customColors[i].netNumber) {
            if (debugReconcile) Serial.printf("    -> UPDATE netNumber %d -> %d\n", customColors[i].netNumber, newNetNum);
            customColors[i].netNumber = newNetNum;
        } else {
            if (debugReconcile) Serial.println("    -> KEEP (unchanged)");
        }
    }
    
    // Clean up stale entries: remove entries with firstNode=0 if a proper entry exists for same net
    for (int i = numCustomNames - 1; i >= 0; i--) {
        if (customNames[i].firstNode != 0) continue;  // Skip proper entries
        // Check if there's a better entry for this netNumber
        for (int j = 0; j < numCustomNames; j++) {
            if (j != i && customNames[j].netNumber == customNames[i].netNumber && customNames[j].firstNode > 0) {
                // Remove the stale entry
                if (debugReconcile) Serial.printf("  Removing stale name entry for net %d (has no firstNode)\n", customNames[i].netNumber);
                for (int k = i; k < numCustomNames - 1; k++) {
                    customNames[k] = customNames[k + 1];
                }
                numCustomNames--;
                break;
            }
        }
    }
    
    for (int i = numCustomColors - 1; i >= 0; i--) {
        if (customColors[i].firstNode != 0) continue;  // Skip proper entries
        // Check if there's a better entry for this netNumber
        for (int j = 0; j < numCustomColors; j++) {
            if (j != i && customColors[j].netNumber == customColors[i].netNumber && customColors[j].firstNode > 0) {
                // Remove the stale entry
                if (debugReconcile) Serial.printf("  Removing stale color entry for net %d (has no firstNode)\n", customColors[i].netNumber);
                for (int k = i; k < numCustomColors - 1; k++) {
                    customColors[k] = customColors[k + 1];
                }
                numCustomColors--;
                break;
            }
        }
    }
    
    if (debugReconcile) Serial.printf("reconcileAfterRebuild DONE: %d names, %d colors\n", numCustomNames, numCustomColors);
}

// ============================================================================
// ConfigState Implementation
// ============================================================================

ConfigState::ConfigState() {
    setDefaults();
}

void ConfigState::setDefaults() {
    // Source of truth defaults
    sourceOfTruth = BRIDGES_PRIMARY;  // Default: bridges define connections
    
    // Routing preferences
    stackPaths = 2;
    stackRails = 3;
    stackDacs = 0;
    railPriority = 1;
    
    // Default GPIO to inputs with pull-down
    for (int i = 0; i < 10; i++) {
        gpioDirection[i] = 1;  // input
        gpioPulls[i] = 0;      // pull down
        gpioPwmFrequency[i] = 1.0f;
        gpioPwmDutyCycle[i] = 0.5f;
        gpioPwmEnabled[i] = false;
        gpioPythonOwned[i] = false;  // No pins owned by MicroPython by default
    }
    
    // UART defaults
    uartTxFunction = 0;  // TX
    uartRxFunction = 1;  // RX
    
    // OLED defaults
    oledConnected = false;
    oledLockConnection = false;
    
    // Auto-refresh defaults
    autoRefreshOnChange = false;  // Default: don't auto-refresh (user controls when)
}

// ============================================================================
// FakeGPIO Restoration
// ============================================================================

// Global list of pending FakeGPIO restorations (populated during YAML load, applied after bridges are restored)
std::vector<FakeGpioRestorationInfo> pendingFakeGpioRestorations;

// ============================================================================
// JumperlessState Implementation
// ============================================================================

JumperlessState::JumperlessState() {
    version = 2;  // Current format version (2 = YAML format)
    dirty = false;
    lastModifiedTime = 0;
    numEphemeralConnections = 0;
    clear();
}

void JumperlessState::clear() {
    connections.clear();
    power.setDefaults();
    display.clear();
    config.setDefaults();
    clearAllCustomNetNames();  // Reset all custom net names to defaults
    dirty = false;
    lastModifiedTime = 0;
}

void JumperlessState::markDirty() {
    if (!dirty && debugWaitLoopTiming) {
        Serial.printf("DEBUG: State marked dirty at %lu ms\n", millis());
        // Print stack trace to help debug what's causing spurious dirty marks
        Serial.println("DEBUG: markDirty() called from:");
        // Print return address to help identify caller
        void* caller = __builtin_return_address(0);
        Serial.printf("  Caller address: %p\n", caller);
    }
    dirty = true;
    lastModifiedTime = millis();
}

void JumperlessState::clearDirty() {
    if (dirty && debugWaitLoopTiming) {
        Serial.printf("DEBUG: State cleared dirty at %lu ms (was dirty for %lu ms)\n", 
                     millis(), millis() - lastModifiedTime);
    }
    dirty = false;
}

// Connection management
bool JumperlessState::addConnection(int node1, int node2, String& errorMsg, int duplicates) {
    // Validate nodes
    if (!isNodeValid(node1)) {
        errorMsg = "Invalid node 1: " + String(node1);
        return false;
    }
    if (!isNodeValid(node2)) {
        errorMsg = "Invalid node 2: " + String(node2);
        return false;
    }
    
    // Check if connection is allowed
    if (!isConnectionAllowed(node1, node2, errorMsg)) {
        return false;
    }
    // Serial.println("Adding connection: " + String(node1) + " - " + String(node2));
    // Serial.println("numBridges: " + String(connections.numBridges));
    // Serial.flush();
    // Check for duplicate - if it exists, update the duplicate count
    for (int i = 0; i < connections.numBridges; i++) {
        if ((connections.bridges[i][0] == node1 && connections.bridges[i][1] == node2) ||
            (connections.bridges[i][0] == node2 && connections.bridges[i][1] == node1)) {
            // Connection exists - update duplicates based on parameter
            if (duplicates >= 0) {
                // Specific duplicate count provided - replace the value
                connections.bridges[i][2] = duplicates;
            } else {
                // No duplicate count specified - increment existing count
                connections.bridges[i][2]++;
            }
            connections.invalidateCache(config.autoRefreshOnChange);
            markDirty();
            return true;
        }
    }
    
    // Check if we have space
    if (connections.numBridges >= MAX_BRIDGES) {
        errorMsg = "Maximum number of connections reached (" + String(MAX_BRIDGES) + ")";
        return false;
    }
    
    // Determine number of duplicates: -1 means use default from config
    int numDuplicates;
    if (duplicates < 0) {
        numDuplicates = config.stackPaths;  // Use default from config
    } else {
        numDuplicates = duplicates;
    }
    
    // Add the bridge
    int idx = connections.numBridges;
    connections.bridges[idx][0] = node1;
    connections.bridges[idx][1] = node2;
    connections.bridges[idx][2] = numDuplicates;  // Store duplicates
    connections.numBridges++;
    
    // Invalidate caches - paths need to be recalculated
    connections.invalidateCache(config.autoRefreshOnChange);
    markDirty();
    
    return true;
}

bool JumperlessState::removeConnection(int node1, int node2, String& errorMsg) {
    // Find the connection
    int foundIdx = -1;
    for (int i = 0; i < connections.numBridges; i++) {
        if ((connections.bridges[i][0] == node1 && connections.bridges[i][1] == node2) ||
            (connections.bridges[i][0] == node2 && connections.bridges[i][1] == node1)) {
            foundIdx = i;
            break;
        }
    }
    
    if (foundIdx == -1) {
        errorMsg = "Connection " + String(node1) + "-" + String(node2) + " not found";
        return false;
    }
    
    // Remove by shifting remaining entries
    for (int i = foundIdx; i < connections.numBridges - 1; i++) {
        connections.bridges[i][0] = connections.bridges[i + 1][0];
        connections.bridges[i][1] = connections.bridges[i + 1][1];
        connections.bridges[i][2] = connections.bridges[i + 1][2];
        connections.bridgeColors[i] = connections.bridgeColors[i + 1];  // CRITICAL: Shift colors too!
    }
    connections.numBridges--;
    connections.bridgeColors[connections.numBridges] = 0xFFFFFFFF;  // Clear the last color slot
    
    // Invalidate caches
    connections.invalidateCache(config.autoRefreshOnChange);
    markDirty();
    
    return true;
}

bool JumperlessState::hasConnection(int node1, int node2) const {
    for (int i = 0; i < connections.numBridges; i++) {
        if ((connections.bridges[i][0] == node1 && connections.bridges[i][1] == node2) ||
            (connections.bridges[i][0] == node2 && connections.bridges[i][1] == node1)) {
            return true;
        }
    }
    return false;
}

void JumperlessState::clearAllConnections() {
    connections.clear();
    markDirty();
    
    // Restore locked connections after clearing
    extern int handleLockedConnections();
    handleLockedConnections();
}

int JumperlessState::getConnectionDuplicates(int node1, int node2) const {
    for (int i = 0; i < connections.numBridges; i++) {
        if ((connections.bridges[i][0] == node1 && connections.bridges[i][1] == node2) ||
            (connections.bridges[i][0] == node2 && connections.bridges[i][1] == node1)) {
            return connections.bridges[i][2];  // Return duplicates from third element
        }
    }
    return 0;  // Not found
}

bool JumperlessState::setConnectionDuplicates(int node1, int node2, int duplicates, String& errorMsg) {
    if (duplicates < 0) {
        errorMsg = "Duplicates must be non-negative: " + String(duplicates);
        return false;
    }
    
    for (int i = 0; i < connections.numBridges; i++) {
        if ((connections.bridges[i][0] == node1 && connections.bridges[i][1] == node2) ||
            (connections.bridges[i][0] == node2 && connections.bridges[i][1] == node1)) {
            connections.bridges[i][2] = duplicates;  // Store duplicates in third element
            connections.invalidateCache(config.autoRefreshOnChange);
            markDirty();
            return true;
        }
    }
    
    errorMsg = "Connection " + String(node1) + "-" + String(node2) + " not found";
    return false;
}

// ============================================================================
// Ephemeral Connection Management
// ============================================================================
// Ephemeral connections are temporary - they are added to the bridges array
// for routing but are NEVER saved to YAML and do NOT trigger markDirty().
//
// Key features:
// - Are physically routed through the crossbar switches (so signals pass through)
// - Are NEVER saved to flash/YAML (filtered out in serializeBridges)
// - Do NOT trigger markDirty() (won't cause auto-save)
// - Can optionally apply routing immediately with optional LED updates
// - Support optional color for visual feedback during measurements

bool JumperlessState::addEphemeralConnection(int node1, int node2, String& errorMsg,
                                             bool applyRouting, int ledShowOption,
                                             uint32_t color) {
    // Validate nodes
    if (!isNodeValid(node1)) {
        errorMsg = "Invalid node 1: " + String(node1);
        return false;
    }
    if (!isNodeValid(node2)) {
        errorMsg = "Invalid node 2: " + String(node2);
        return false;
    }
    
    // Check if connection is allowed
    if (!isConnectionAllowed(node1, node2, errorMsg)) {
        return false;
    }
    
    // Check if already exists as regular connection
    if (hasConnection(node1, node2)) {
        // Connection already exists - just track it as ephemeral
        // (might have been added by user before measure mode)
        // Still apply routing if requested (in case hardware state is stale)
        if (applyRouting) {
            refreshLocalConnections(ledShowOption, 0, 0);
            waitCore2();
        }
        return true;
    }
    
    // Check if already tracked as ephemeral
    for (int i = 0; i < numEphemeralConnections; i++) {
        if ((ephemeralConnections[i].node1 == node1 && ephemeralConnections[i].node2 == node2) ||
            (ephemeralConnections[i].node1 == node2 && ephemeralConnections[i].node2 == node1)) {
            // Already tracked - still apply routing if requested
            if (applyRouting) {
                refreshLocalConnections(ledShowOption, 0, 0);
                waitCore2();
            }
            return true;  // Already tracked
        }
    }
    
    // Check if we have space for ephemeral tracking
    if (numEphemeralConnections >= MAX_EPHEMERAL_CONNECTIONS) {
        errorMsg = "Maximum ephemeral connections reached";
        return false;
    }
    
    // Check if we have space in bridges array
    if (connections.numBridges >= MAX_BRIDGES) {
        errorMsg = "Maximum number of connections reached";
        return false;
    }
    
    // Add the bridge (with 0 duplicates for minimal routing overhead)
    int idx = connections.numBridges;
    connections.bridges[idx][0] = node1;
    connections.bridges[idx][1] = node2;
    connections.bridges[idx][2] = 0;  // No duplicate paths needed for measurement
    connections.bridgeColors[idx] = color;  // Set optional color for visual feedback
    connections.numBridges++;
    
    // Track as ephemeral
    ephemeralConnections[numEphemeralConnections] = EphemeralConnection(node1, node2, idx);
    numEphemeralConnections++;
    
    // Invalidate caches to trigger routing, but DON'T mark dirty
    // Note: We disable autoRefresh here since we'll handle it manually if requested
    connections.invalidateCache(false);
    // NOTE: No markDirty() call - this is the key difference from addConnection()
    
    // Apply routing to hardware if requested
    if (applyRouting) {
        refreshLocalConnections(ledShowOption, 0, 0);
        waitCore2();
    }
    
    return true;
}

bool JumperlessState::removeEphemeralConnection(int node1, int node2, String& errorMsg,
                                                bool applyRouting, int ledShowOption) {
    // Find in ephemeral tracking
    int ephIdx = -1;
    for (int i = 0; i < numEphemeralConnections; i++) {
        if ((ephemeralConnections[i].node1 == node1 && ephemeralConnections[i].node2 == node2) ||
            (ephemeralConnections[i].node1 == node2 && ephemeralConnections[i].node2 == node1)) {
            ephIdx = i;
            break;
        }
    }
    
    if (ephIdx == -1) {
        errorMsg = "Ephemeral connection not found";
        return false;
    }
    
    // Find and remove from bridges array
    int bridgeIdx = -1;
    for (int i = 0; i < connections.numBridges; i++) {
        if ((connections.bridges[i][0] == node1 && connections.bridges[i][1] == node2) ||
            (connections.bridges[i][0] == node2 && connections.bridges[i][1] == node1)) {
            bridgeIdx = i;
            break;
        }
    }
    
    if (bridgeIdx != -1) {
        // Remove bridge by shifting remaining entries
        for (int i = bridgeIdx; i < connections.numBridges - 1; i++) {
            connections.bridges[i][0] = connections.bridges[i + 1][0];
            connections.bridges[i][1] = connections.bridges[i + 1][1];
            connections.bridges[i][2] = connections.bridges[i + 1][2];
            connections.bridgeColors[i] = connections.bridgeColors[i + 1];
        }
        connections.numBridges--;
        connections.bridgeColors[connections.numBridges] = 0xFFFFFFFF;
        
        // Update bridge indices in remaining ephemeral connections
        for (int i = 0; i < numEphemeralConnections; i++) {
            if (ephemeralConnections[i].bridgeIndex > bridgeIdx) {
                ephemeralConnections[i].bridgeIndex--;
            }
        }
    }
    
    // Remove from ephemeral tracking
    for (int i = ephIdx; i < numEphemeralConnections - 1; i++) {
        ephemeralConnections[i] = ephemeralConnections[i + 1];
    }
    numEphemeralConnections--;
    
    // Invalidate caches, but DON'T mark dirty
    // Note: We disable autoRefresh here since we'll handle it manually if requested
    connections.invalidateCache(false);
    
    // Apply routing to hardware if requested
    if (applyRouting) {
        refreshLocalConnections(ledShowOption, 0, 0);
        waitCore2();
    }
    
    return true;
}

void JumperlessState::clearAllEphemeralConnections(bool applyRouting, int ledShowOption) {
    // Track if we actually removed anything
    int removedCount = numEphemeralConnections;
    
    // Remove all ephemeral connections from bridges (in reverse order to avoid index issues)
    for (int e = numEphemeralConnections - 1; e >= 0; e--) {
        int node1 = ephemeralConnections[e].node1;
        int node2 = ephemeralConnections[e].node2;
        
        // Find and remove from bridges array
        for (int i = 0; i < connections.numBridges; i++) {
            if ((connections.bridges[i][0] == node1 && connections.bridges[i][1] == node2) ||
                (connections.bridges[i][0] == node2 && connections.bridges[i][1] == node1)) {
                // Remove bridge by shifting
                for (int j = i; j < connections.numBridges - 1; j++) {
                    connections.bridges[j][0] = connections.bridges[j + 1][0];
                    connections.bridges[j][1] = connections.bridges[j + 1][1];
                    connections.bridges[j][2] = connections.bridges[j + 1][2];
                    connections.bridgeColors[j] = connections.bridgeColors[j + 1];
                }
                connections.numBridges--;
                connections.bridgeColors[connections.numBridges] = 0xFFFFFFFF;
                break;
            }
        }
    }
    
    numEphemeralConnections = 0;
    
    // Only invalidate and apply routing if we actually removed something
    if (removedCount > 0) {
        connections.invalidateCache(false);  // Don't auto-refresh, we'll handle it
        
        if (applyRouting) {
            refreshLocalConnections(ledShowOption, 0, 0);
            waitCore2();
        }
    }
}

bool JumperlessState::isEphemeralConnection(int node1, int node2) const {
    for (int i = 0; i < numEphemeralConnections; i++) {
        if ((ephemeralConnections[i].node1 == node1 && ephemeralConnections[i].node2 == node2) ||
            (ephemeralConnections[i].node1 == node2 && ephemeralConnections[i].node2 == node1)) {
            return true;
        }
    }
    return false;
}

int JumperlessState::getEphemeralConnectionCount() const {
    return numEphemeralConnections;
}

// Power management
void JumperlessState::setDacVoltage(int dacNum, float voltage) {
    if (dacNum == 0) {
        power.dac0 = voltage;
    } else if (dacNum == 1) {
        power.dac1 = voltage;
    }
    markDirty();
}

float JumperlessState::getDacVoltage(int dacNum) const {
    return (dacNum == 0) ? power.dac0 : power.dac1;
}

void JumperlessState::setRailVoltage(bool isTopRail, float voltage) {
    if (isTopRail) {
        power.topRail = voltage;
    } else {
        power.bottomRail = voltage;
    }
    markDirty();
}

float JumperlessState::getRailVoltage(bool isTopRail) const {
    return isTopRail ? power.topRail : power.bottomRail;
}

// Configuration
void JumperlessState::setPathStacking(int paths, int rails, int dacs) {
    config.stackPaths = paths;
    config.stackRails = rails;
    config.stackDacs = dacs;
    markDirty();
}

void JumperlessState::getPathStacking(int& paths, int& rails, int& dacs) const {
    paths = config.stackPaths;
    rails = config.stackRails;
    dacs = config.stackDacs;
}

// GPIO
void JumperlessState::setGpioDirection(int gpio, int direction) {
    if (gpio >= 0 && gpio < 10) {
        config.gpioDirection[gpio] = direction;
        markDirty();
    }
}

int JumperlessState::getGpioDirection(int gpio) const {
    if (gpio >= 0 && gpio < 10) {
        return config.gpioDirection[gpio];
    }
    return 1;  // default to input
}

void JumperlessState::setGpioPull(int gpio, int pull) {
    if (gpio >= 0 && gpio < 10) {
        config.gpioPulls[gpio] = pull;
        markDirty();
    }
}

int JumperlessState::getGpioPull(int gpio) const {
    if (gpio >= 0 && gpio < 10) {
        return config.gpioPulls[gpio];
    }
    return 0;  // default to pull-down
}

void JumperlessState::setGpioPwmFrequency(int gpio, float frequency) {
    if (gpio >= 0 && gpio < 10) {
        config.gpioPwmFrequency[gpio] = frequency;
        markDirty();
    }
}

float JumperlessState::getGpioPwmFrequency(int gpio) const {
    if (gpio >= 0 && gpio < 10) {
        return config.gpioPwmFrequency[gpio];
    }
    return 1.0f;  // default frequency
}

void JumperlessState::setGpioPwmDutyCycle(int gpio, float dutyCycle) {
    if (gpio >= 0 && gpio < 10) {
        config.gpioPwmDutyCycle[gpio] = dutyCycle;
        markDirty();
    }
}

float JumperlessState::getGpioPwmDutyCycle(int gpio) const {
    if (gpio >= 0 && gpio < 10) {
        return config.gpioPwmDutyCycle[gpio];
    }
    return 0.5f;  // default 50% duty cycle
}

void JumperlessState::setGpioPwmEnabled(int gpio, bool enabled) {
    if (gpio >= 0 && gpio < 10) {
        config.gpioPwmEnabled[gpio] = enabled;
        markDirty();
    }
}

bool JumperlessState::getGpioPwmEnabled(int gpio) const {
    if (gpio >= 0 && gpio < 10) {
        return config.gpioPwmEnabled[gpio];
    }
    return false;  // default disabled
}

// UART
void JumperlessState::setUartTxFunction(int function) {
    config.uartTxFunction = function;
    markDirty();
}

int JumperlessState::getUartTxFunction() const {
    return config.uartTxFunction;
}

void JumperlessState::setUartRxFunction(int function) {
    config.uartRxFunction = function;
    markDirty();
}

int JumperlessState::getUartRxFunction() const {
    return config.uartRxFunction;
}

// Display
void JumperlessState::setNetColor(int netNum, rgbColor color, uint32_t raw, const char* name) {
    display.setNetColor(netNum, color, raw, name);
    markDirty();
}

bool JumperlessState::getNetColor(int netNum, rgbColor& color, uint32_t& raw, char* name) const {
    return display.getNetColor(netNum, color, raw, name);
}

// Validation
bool JumperlessState::validate(String& errorMsg) const {
    // Validate power settings
    if (!power.validate(errorMsg)) {
        return false;
    }
    
    // Validate connections
    for (int i = 0; i < connections.numBridges; i++) {
        int n1 = connections.bridges[i][0];
        int n2 = connections.bridges[i][1];
        
        if (!isNodeValid(n1)) {
            errorMsg = "Invalid node in bridge " + String(i) + ": " + String(n1);
            return false;
        }
        if (!isNodeValid(n2)) {
            errorMsg = "Invalid node in bridge " + String(i) + ": " + String(n2);
            return false;
        }
    }
    
    return true;
}

// Validation helpers
bool JumperlessState::isNodeValid(int node) const {
    // Use the existing validation from FileParsing
    extern int isNodeValid(int node);
    return ::isNodeValid(node) == 1;
}

bool JumperlessState::isConnectionAllowed(int node1, int node2, String& errorMsg) const {
    // Check for same node
    if (node1 == node2) {
        errorMsg = "Cannot connect node to itself: " + String(node1);
        return false;
    }
    
    // Use existing validation logic
    extern bool connectionAllowed(int node1, int node2);
    if (!::connectionAllowed(node1, node2)) {
        errorMsg = "Connection not allowed between " + String(node1) + " and " + String(node2) + 
                   " (likely power/ground conflict)";
        return false;
    }
    
    return true;
}

// State comparison
bool JumperlessState::operator==(const JumperlessState& other) const {
    // Compare number of bridges
    if (connections.numBridges != other.connections.numBridges) {
        return false;
    }
    
    // Compare bridges
    for (int i = 0; i < connections.numBridges; i++) {
        if (connections.bridges[i][0] != other.connections.bridges[i][0] ||
            connections.bridges[i][1] != other.connections.bridges[i][1]) {
            return false;
        }
    }
    
    // Compare power
    if (power.dac0 != other.power.dac0 || power.dac1 != other.power.dac1 ||
        power.topRail != other.power.topRail || power.bottomRail != other.power.bottomRail) {
        return false;
    }
    
    // Compare config (just the important bits)
    if (config.stackPaths != other.config.stackPaths ||
        config.stackRails != other.config.stackRails ||
        config.stackDacs != other.config.stackDacs) {
        return false;
    }
    
    return true;
}

size_t JumperlessState::estimateRAMUsage() const {
    return sizeof(JumperlessState);
}

// ============================================================================
// YAML Serialization
// ============================================================================

bool JumperlessState::toYAML(String& output) const {
    output = "";
    
    // Pre-allocate buffer to avoid repeated reallocations during concatenation
    // Estimate: ~50 bytes per bridge + ~500 bytes overhead = ~3KB typical
    output.reserve(connections.numBridges * 50 + 500);
    
    // Header
    output += "version: " + String(version) + "\n";
    output += "sourceOfTruth: " + String(config.sourceOfTruth == BRIDGES_PRIMARY ? "bridges" : "nets") + "\n\n";
    
    // Bridges section
    serializeBridges(output);
    
    // Nets section (optional, for colors/names)
    serializeNets(output);
    
    // Power section
    serializePower(output);
    
    // Config section
    serializeConfig(output);
    
    return true;
}

bool JumperlessState::fromYAML(const String& input, String& errorMsg) {
    // Simple YAML parser for our specific format
    // Parse line by line
    int lineStart = 0;
    String currentSection = "";
    
    clear();
    
    // Clear pending FakeGPIO restorations from previous load
    pendingFakeGpioRestorations.clear();
    
    while (lineStart < (int)input.length()) {
        int lineEnd = input.indexOf('\n', lineStart);
        if (lineEnd == -1) lineEnd = input.length();
        
        String line = input.substring(lineStart, lineEnd);
        line.trim();
        
        // Skip empty lines and comments
        if (line.length() == 0 || line.startsWith("#")) {
            lineStart = lineEnd + 1;
            continue;
        }
        
        // Check for section headers
        if (line.startsWith("version:")) {
            int colonIdx = line.indexOf(':');
            String val = line.substring(colonIdx + 1);
            val.trim();
            version = val.toInt();
        }
        else if (line.startsWith("sourceOfTruth:")) {
            int colonIdx = line.indexOf(':');
            String val = line.substring(colonIdx + 1);
            val.trim();
            config.sourceOfTruth = (val == "nets") ? NETS_PRIMARY : BRIDGES_PRIMARY;
        }
        else if (line.startsWith("bridges:")) {
            currentSection = "bridges";
        }
        else if (line.startsWith("nets:")) {
            currentSection = "nets";
        }
        else if (line.startsWith("power:")) {
            currentSection = "power";
        }
        else if (line.startsWith("config:")) {
            currentSection = "config";
        }
        else if (line.startsWith("fakeGpio:")) {
            currentSection = "fakeGpio";
        }
        // Parse section content
        else if (line.startsWith("- {") || line.startsWith("-{")) {
            // Try to parse line even if incomplete (missing closing brace)
            // The deserializer will extract whatever fields it can find
            
            // Bridge or net entry
            if (currentSection == "bridges") {
                String tempError;
                if (!deserializeBridges(line.c_str(), tempError)) {
                    // Skip invalid lines in preview mode - don't fail entire parse
                    // This allows partially edited files to still work
                    lineStart = lineEnd + 1;
                    continue;
                }
            } else if (currentSection == "nets") {
                String tempError;
                if (!deserializeNets(line.c_str(), tempError)) {
                    // Skip invalid lines in preview mode
                    lineStart = lineEnd + 1;
                    continue;
                }
            } else if (currentSection == "fakeGpio") {
                String tempError;
                if (!deserializeFakeGpio(line.c_str(), tempError)) {
                    // Skip invalid fakeGpio lines - don't fail entire parse
                    lineStart = lineEnd + 1;
                    continue;
                }
            }
        }
        else if (currentSection == "power") {
            String tempError;
            if (!deserializePower(line.c_str(), tempError)) {
                // Skip invalid power lines - don't fail entire parse
                lineStart = lineEnd + 1;
                continue;
            }
        }
        else if (currentSection == "config") {
            String tempError;
            if (!deserializeConfig(line.c_str(), tempError)) {
                // Skip invalid config lines - don't fail entire parse
                lineStart = lineEnd + 1;
                continue;
            }
        }
    
        lineStart = lineEnd + 1;
    }
    
    // Reconcile bridges and nets based on source of truth
    if (config.sourceOfTruth == NETS_PRIMARY) {
        connections.syncBridgesFromNets();
    } else {
        connections.syncNetsFromBridges();
    }
    
    return validate(errorMsg);
}

// YAML serialization helpers
void JumperlessState::serializeBridges(String& output) const {
    // Count non-ephemeral bridges first
    int persistentBridges = 0;
    for (int i = 0; i < connections.numBridges; i++) {
        int node1 = connections.bridges[i][0];
        int node2 = connections.bridges[i][1];
        // Skip ephemeral connections - they should NEVER be saved to flash
        if (!isEphemeralConnection(node1, node2)) {
            persistentBridges++;
        }
    }
    
    if (persistentBridges == 0) {
        return;  // Don't output empty bridges section
    }
    
    output += "bridges:\n";
    for (int i = 0; i < connections.numBridges; i++) {
        int node1 = connections.bridges[i][0];
        int node2 = connections.bridges[i][1];
        
        // Skip ephemeral connections - they should NEVER be saved to flash
        if (isEphemeralConnection(node1, node2)) {
            continue;
        }
        
        // Use node names instead of raw numbers for readability
        String n1Name = nodeValueToString(node1);
        String n2Name = nodeValueToString(node2);
        
        output += "  - {n1: " + n1Name + 
                  ", n2: " + n2Name + 
                  ", dup: " + String(connections.bridges[i][2]);
        
        // Add color field if this bridge has a color (0xFFFFFFFF = no color)
        if (connections.bridgeColors[i] != 0xFFFFFFFF) {
            // Use rgbToWokwiColorName to preserve original Wokwi color names
            String colorName = rgbToWokwiColorName(connections.bridgeColors[i]);
            output += ", color: " + colorName;
        }
        
        output += "}\n";
    }
    output += "\n";
}

bool JumperlessState::deserializeBridges(const char* yamlContent, String& errorMsg) {
    // Parse bridge entry: - {n1: TOP_RAIL, n2: NANO_D5, dup: 2, color: red}
    // Also accepts: - {n1: 101, n2: 75, dup: 2}
    // Tolerant of incomplete lines for live editing
    String line = String(yamlContent);
    line.trim();
    
    // Validate line has opening brace at minimum
    if (line.indexOf('{') == -1) {
        errorMsg = "Missing opening brace in bridge entry";
        return false;
    }
    
    // Extract values (tolerant of incomplete/missing fields)
    int n1 = -1, n2 = -1, dup = -1;
    uint32_t color = 0xFFFFFFFF;  // Default to "no color"
    
    // Parse n1 field
    int n1Idx = line.indexOf("n1:");
    if (n1Idx >= 0) {
        // Find delimiter: comma, closing brace, or end of line
        int commaIdx = line.indexOf(',', n1Idx);
        int braceIdx = line.indexOf('}', n1Idx);
        int endIdx = line.length();
        
        // Use the first valid delimiter found
        if (commaIdx > n1Idx + 3 && (braceIdx == -1 || commaIdx < braceIdx)) {
            endIdx = commaIdx;
        } else if (braceIdx > n1Idx + 3) {
            endIdx = braceIdx;
        }
        
        if (endIdx > n1Idx + 3) {
            String val = line.substring(n1Idx + 3, endIdx);
            val.trim();
            
            // Only parse if we have actual content
            if (val.length() > 0) {
                n1 = parseNodeName(val);
            }
        }
    }
    
    // Parse n2 field
    int n2Idx = line.indexOf("n2:");
    if (n2Idx >= 0) {
        // Find delimiter: comma, closing brace, or end of line
        int commaIdx = line.indexOf(',', n2Idx);
        int braceIdx = line.indexOf('}', n2Idx);
        int endIdx = line.length();
        
        // Use the first valid delimiter found
        if (commaIdx > n2Idx + 3 && (braceIdx == -1 || commaIdx < braceIdx)) {
            endIdx = commaIdx;
        } else if (braceIdx > n2Idx + 3) {
            endIdx = braceIdx;
        }
        
        if (endIdx > n2Idx + 3) {
            String val = line.substring(n2Idx + 3, endIdx);
            val.trim();
            
            // Only parse if we have actual content
            if (val.length() > 0) {
                n2 = parseNodeName(val);
            }
        }
    }
    
    // Parse dup field (optional)
    int dupIdx = line.indexOf("dup:");
    if (dupIdx >= 0) {
        int commaIdx = line.indexOf(',', dupIdx);
        int braceIdx = line.indexOf('}', dupIdx);
        int endIdx = line.length();
        
        // Use the first valid delimiter
        if (commaIdx > dupIdx + 4 && (braceIdx == -1 || commaIdx < braceIdx)) {
            endIdx = commaIdx;
        } else if (braceIdx > dupIdx + 4) {
            endIdx = braceIdx;
        }
        
        if (endIdx > dupIdx + 4) {
            String val = line.substring(dupIdx + 4, endIdx);
            val.trim();
            if (val.length() > 0) {
                dup = val.toInt();
            }
        }
    }
    
    // Parse color field (optional)
    int colorIdx = line.indexOf("color:");
    if (colorIdx >= 0) {
        int braceIdx = line.indexOf('}', colorIdx);
        int endIdx = (braceIdx > colorIdx + 6) ? braceIdx : line.length();
        
        if (endIdx > colorIdx + 6) {
            String val = line.substring(colorIdx + 6, endIdx);
            val.trim();
            if (val.length() > 0) {
                bool parseSuccess;
                color = parseColorValue(val, parseSuccess);
                if (!parseSuccess) {
                    color = 0;  // Ignore invalid colors
                }
            }
        }
    }
    
    // Only add connection if we have both nodes
    if (n1 < 0 || n2 < 0) {
        errorMsg = "Incomplete bridge (missing n1 or n2): " + line;
        return false;
    }
    
    // Add the connection
    bool success = addConnection(n1, n2, errorMsg, dup);
    
    // Store the color if provided and connection was added
    // 0xFFFFFFFF means no color was specified
    if (success && color != 0xFFFFFFFF) {
        // The bridge was just added, so it's at index (numBridges - 1)
        int bridgeIdx = connections.numBridges - 1;
        if (bridgeIdx >= 0 && bridgeIdx < MAX_BRIDGES) {
            connections.bridgeColors[bridgeIdx] = color;
        }
    }
    
    return success;
}

void JumperlessState::serializeNets(String& output) const {
    // Serialize ALL computed nets with full metadata
    // This allows users to edit colors and see what nets exist
    
    // ARCHITECTURE NOTE: We use globalState.connections.nets[] which is where
    // all net computation is now done. The old global net[] array is deprecated.
    // The nets are computed by getNodesToConnect() which populates globalState.
    // This approach is optimal because:
    //   1. Nets are computed once on load, not repeatedly on save
    //   2. We reuse the existing optimized net computation code
    //   3. Serialization is fast (just reading already-computed data)
    
    // Use the state's nets (access globalState since this is a const method)
    JumperlessState& state = const_cast<JumperlessState&>(globalState);
    
    // Count the number of active nets (since numNets might not be set by NetManager)
    // Scan through nets array to find the highest numbered net
    int maxNetNumber = 0;
    for (int i = 1; i < MAX_NETS; i++) {
        if (state.connections.nets[i].number > 0 && state.connections.nets[i].nodes[0] != 0) {
            maxNetNumber = i;
        }
    }
    
    // Update numNets for the state
    state.connections.numNets = maxNetNumber + 1;
    
    // If no nets were found, nothing to serialize
    if (maxNetNumber == 0) {
        return;  // Don't output anything for empty nets section
    }
    
    output += "nets:\n";
    
    // Animation order for detecting animated nets
    extern int animationOrder[26];
    
    for (int i = 1; i <= maxNetNumber; i++) {  // Start at 1, skip net 0
        if (state.connections.nets[i].number == 0 || state.connections.nets[i].nodes[0] == 0) {
            continue;  // Skip empty nets
        }
        
        // Count nodes in this net
        int nodeCount = 0;
        for (int j = 0; j < MAX_NODES && state.connections.nets[i].nodes[j] != 0; j++) {
            nodeCount++;
        }
        
        // Skip nets with only one node
        if (nodeCount <= 1) {
            continue;
        }
        
        // Collect node list for this net (using SHORT names)
        String nodesList = "[";
        bool firstNode = true;
        for (int j = 0; j < MAX_NODES && state.connections.nets[i].nodes[j] != 0; j++) {
            if (!firstNode) nodesList += ", ";
            // Use short name (0 = short, 1 = long)
            const char* shortName = definesToChar(state.connections.nets[i].nodes[j], 0);
            if (shortName && strlen(shortName) > 0) {
                nodesList += String(shortName);
            } else {
                nodesList += String(state.connections.nets[i].nodes[j]);
            }
            firstNode = false;
        }
        nodesList += "]";
        
        // Get color information
        String colorName = "";
        bool userAssigned = false;
        bool animated = false;
        uint32_t netColor = 0;
        
        // Check if this net has a custom color
        for (int j = 0; j < display.numCustomColors; j++) {
            if (display.customColors[j].netNumber == state.connections.nets[i].number) {
                colorName = String(display.customColors[j].colorName);
                colorName.trim();
                netColor = display.customColors[j].rawColor;
                userAssigned = true;  // Custom colors are user-assigned
                break;
            }
        }
        
        // If no custom color, get the auto-generated color
        if (colorName.length() == 0) {
            // Use the net's assigned color
            rgbColor netRgb = state.connections.nets[i].color;
            netColor = packRgb(netRgb.r, netRgb.g, netRgb.b);
            colorName = colorValueToName(netColor);
        }
        
        // Check if net is animated (check if any of its nodes are in the animation order)
        for (int j = 0; j < 26; j++) {
            for (int k = 0; k < MAX_NODES && state.connections.nets[i].nodes[k] != 0; k++) {
                if (state.connections.nets[i].nodes[k] == animationOrder[j]) {
                    animated = true;
                    break;
                }
            }
            if (animated) break;
        }
        
        // Build the output line with required fields first
        output += "  - {num: " + String(state.connections.nets[i].number);
        output += ", nodes: " + nodesList;
        
        // Only print color if not animated
        if (!animated) {
            output += ", color: " + colorName;
        }
        
        // Optional fields at the end (only if not default)
        
        // Check DisplayState for custom name first
        String netName = "";
        bool hasCustomName = false;
        for (int j = 0; j < display.numCustomNames; j++) {
            if (display.customNames[j].netNumber == state.connections.nets[i].number) {
                netName = String(display.customNames[j].name);
                hasCustomName = true;
                break;
            }
        }
        
        // Only print name if it's a custom name from DisplayState
        if (hasCustomName && netName.length() > 0) {
            output += ", name: \"" + netName + "\"";
        }
        
        // Only print user flag if true
        if (userAssigned) {
            output += ", user: true";
        }
        
        // Only print anim flag if true
        if (animated) {
            output += ", anim: true";
        }
        
        output += "}\n";
    }
    output += "\n";
}

bool JumperlessState::deserializeNets(const char* yamlContent, String& errorMsg) {
    // Parse net entry: - {num: 6, nodes: [TOP_RAIL, NANO_D5], color: red, user: true, anim: false, name: "Power"}
    String line = String(yamlContent);
    line.trim();
    
    int netNum = -1;
    String netName = "";
    String colorStr = "";
    bool userAssigned = false;
    bool animated = false;
    
    // Parse num field
    int numIdx = line.indexOf("num:");
    if (numIdx >= 0) {
        int commaIdx = line.indexOf(',', numIdx);
        String val = line.substring(numIdx + 4, commaIdx);
        val.trim();
        netNum = val.toInt();
    }
    
    // Parse nodes field (we don't use this for loading, just for reference)
    // The bridges define the actual connections
    
    // Parse color field (can be name or hex)
    int colorIdx = line.indexOf("color:");
    if (colorIdx >= 0) {
        int endIdx = line.indexOf(',', colorIdx);
        if (endIdx == -1) endIdx = line.indexOf('}', colorIdx);
        String val = line.substring(colorIdx + 6, endIdx);
        val.trim();
        colorStr = val;
    }
    
    // Parse user field
    int userIdx = line.indexOf("user:");
    if (userIdx >= 0) {
        int endIdx = line.indexOf(',', userIdx);
        if (endIdx == -1) endIdx = line.indexOf('}', userIdx);
        String val = line.substring(userIdx + 5, endIdx);
        val.trim();
        bool parseSuccess;
        userAssigned = parseBoolean(val, parseSuccess);
    }
    
    // Parse anim field
    int animIdx = line.indexOf("anim:");
    if (animIdx >= 0) {
        int endIdx = line.indexOf(',', animIdx);
        if (endIdx == -1) endIdx = line.indexOf('}', animIdx);
        String val = line.substring(animIdx + 5, endIdx);
        val.trim();
        bool parseSuccess;
        animated = parseBoolean(val, parseSuccess);
    }
    
    // Parse name field (optional)
    int nameIdx = line.indexOf("name:");
    if (nameIdx >= 0) {
        int startQuote = line.indexOf('"', nameIdx);
        int endQuote = line.indexOf('"', startQuote + 1);
        if (startQuote >= 0 && endQuote > startQuote) {
            netName = line.substring(startQuote + 1, endQuote);
        }
    }
    
    // Store the custom net name if provided
    // This allows user-defined names like "Power", "Signal", etc.
    if (netNum >= 0 && netName.length() > 0) {
        setCustomNetName(netNum, netName.c_str());
    }
    
    // Only store color if it was user-assigned
    // Auto-generated colors will be recomputed
    if (netNum >= 0 && userAssigned && colorStr.length() > 0) {
        bool parseSuccess;
        uint32_t rawColor = parseColorValue(colorStr, parseSuccess);
        
        if (parseSuccess) {
            rgbColor color;
            color.r = (rawColor >> 16) & 0xFF;
            color.g = (rawColor >> 8) & 0xFF;
            color.b = rawColor & 0xFF;
            
            // Use color name for the display system
            String colorName = colorValueToName(rawColor);
            display.setNetColor(netNum, color, rawColor, colorName.c_str());
        }
    }
    
    return true;
}

void JumperlessState::serializePower(String& output) const {
    output += "power:\n";
    output += "  topRail: " + String(power.topRail, 2) + "\n";
    output += "  bottomRail: " + String(power.bottomRail, 2) + "\n";
    output += "  dac0: " + String(power.dac0, 2) + "\n";
    output += "  dac1: " + String(power.dac1, 2) + "\n\n";
}

bool JumperlessState::deserializePower(const char* yamlContent, String& errorMsg) {
    String line = String(yamlContent);
    line.trim();
    
    if (line.startsWith("topRail:")) {
        int colonIdx = line.indexOf(':');
        String val = line.substring(colonIdx + 1);
        val.trim();
        power.topRail = val.toFloat();
    }
    else if (line.startsWith("bottomRail:")) {
        int colonIdx = line.indexOf(':');
        String val = line.substring(colonIdx + 1);
        val.trim();
        power.bottomRail = val.toFloat();
    }
    else if (line.startsWith("dac0:")) {
        int colonIdx = line.indexOf(':');
        String val = line.substring(colonIdx + 1);
        val.trim();
        power.dac0 = val.toFloat();
    }
    else if (line.startsWith("dac1:")) {
        int colonIdx = line.indexOf(':');
        String val = line.substring(colonIdx + 1);
        val.trim();
        power.dac1 = val.toFloat();
    }
    
    return power.validate(errorMsg);
}

void JumperlessState::serializeConfig(String& output) const {
    output += "config:\n";
    output += "  routing: {stackPaths: " + String(config.stackPaths) + 
              ", stackRails: " + String(config.stackRails) + 
              ", stackDacs: " + String(config.stackDacs) + 
              ", railPriority: " + String(config.railPriority) + "}\n";
    
    // GPIO direction array
    output += "  gpio:\n";
    output += "    direction:    [";
    for (int i = 0; i < 10; i++) {
        output += String(config.gpioDirection[i]);
        if (i < 9) output += ",";
    }
    output += "]\n";
    
    // GPIO pulls array
    output += "    pulls:        [";
    for (int i = 0; i < 10; i++) {
        output += String(config.gpioPulls[i]);
        if (i < 9) output += ",";
    }
    output += "]\n";
    
    // PWM frequency array
    output += "    pwmFrequency: [";
    for (int i = 0; i < 10; i++) {
        output += String(config.gpioPwmFrequency[i], 2);
        if (i < 9) output += ",";
    }
    output += "]\n";
    
    // PWM duty cycle array
    output += "    pwmDutyCycle: [";
    for (int i = 0; i < 10; i++) {
        output += String(config.gpioPwmDutyCycle[i], 2);
        if (i < 9) output += ",";
    }
    output += "]\n";
    
    // PWM enabled array
    output += "    pwmEnabled:   [";
    for (int i = 0; i < 10; i++) {
        output += String(config.gpioPwmEnabled[i] ? "1" : "0");
        if (i < 9) output += ",";
    }
    output += "]\n";
    
    // UART and OLED
    output += "  uart: {txFunction: " + String(config.uartTxFunction) + 
              ", rxFunction: " + String(config.uartRxFunction) + "}\n";
    output += "  oled: {connected: " + String(config.oledConnected ? "true" : "false") + 
              ", lockConnection: " + String(config.oledLockConnection ? "true" : "false") + "}\n";
    
    // Fake GPIO configurations
    serializeFakeGpio(output);
}

bool JumperlessState::deserializeConfig(const char* yamlContent, String& errorMsg) {
    String line = String(yamlContent);
    line.trim();
    
    // Parse routing line
    if (line.startsWith("routing:")) {
        int stackPathsIdx = line.indexOf("stackPaths:");
        if (stackPathsIdx >= 0) {
            int commaIdx = line.indexOf(',', stackPathsIdx);
            String val = line.substring(stackPathsIdx + 11, commaIdx);
            val.trim();
            config.stackPaths = val.toInt();
        }
        
        int stackRailsIdx = line.indexOf("stackRails:");
        if (stackRailsIdx >= 0) {
            int commaIdx = line.indexOf(',', stackRailsIdx);
            String val = line.substring(stackRailsIdx + 11, commaIdx);
            val.trim();
            config.stackRails = val.toInt();
        }
        
        int stackDacsIdx = line.indexOf("stackDacs:");
        if (stackDacsIdx >= 0) {
            int commaIdx = line.indexOf(',', stackDacsIdx);
            String val = line.substring(stackDacsIdx + 10, commaIdx);
            val.trim();
            config.stackDacs = val.toInt();
        }
        
        int railPriorityIdx = line.indexOf("railPriority:");
        if (railPriorityIdx >= 0) {
            int endIdx = line.indexOf('}', railPriorityIdx);
            String val = line.substring(railPriorityIdx + 13, endIdx);
            val.trim();
            config.railPriority = val.toInt();
        }
    }
    // Parse GPIO direction array
    else if (line.startsWith("direction:")) {
        int startIdx = line.indexOf('[');
        int endIdx = line.indexOf(']');
        if (startIdx >= 0 && endIdx > startIdx) {
            String arrayStr = line.substring(startIdx + 1, endIdx);
            int idx = 0;
            int pos = 0;
            while (pos < (int)arrayStr.length() && idx < 10) {
                int commaIdx = arrayStr.indexOf(',', pos);
                if (commaIdx == -1) commaIdx = arrayStr.length();
                String val = arrayStr.substring(pos, commaIdx);
                val.trim();
                config.gpioDirection[idx++] = val.toInt();
                pos = commaIdx + 1;
            }
        }
    }
    // Parse GPIO pulls array
    else if (line.startsWith("pulls:")) {
        int startIdx = line.indexOf('[');
        int endIdx = line.indexOf(']');
        if (startIdx >= 0 && endIdx > startIdx) {
            String arrayStr = line.substring(startIdx + 1, endIdx);
            int idx = 0;
            int pos = 0;
            while (pos < (int)arrayStr.length() && idx < 10) {
                int commaIdx = arrayStr.indexOf(',', pos);
                if (commaIdx == -1) commaIdx = arrayStr.length();
                String val = arrayStr.substring(pos, commaIdx);
                val.trim();
                config.gpioPulls[idx++] = val.toInt();
                pos = commaIdx + 1;
            }
        }
    }
    // Parse PWM frequency array
    else if (line.startsWith("pwmFrequency:")) {
        int startIdx = line.indexOf('[');
        int endIdx = line.indexOf(']');
        if (startIdx >= 0 && endIdx > startIdx) {
            String arrayStr = line.substring(startIdx + 1, endIdx);
            int idx = 0;
            int pos = 0;
            while (pos < (int)arrayStr.length() && idx < 10) {
                int commaIdx = arrayStr.indexOf(',', pos);
                if (commaIdx == -1) commaIdx = arrayStr.length();
                String val = arrayStr.substring(pos, commaIdx);
                val.trim();
                config.gpioPwmFrequency[idx++] = val.toFloat();
                pos = commaIdx + 1;
            }
        }
    }
    // Parse PWM duty cycle array
    else if (line.startsWith("pwmDutyCycle:")) {
        int startIdx = line.indexOf('[');
        int endIdx = line.indexOf(']');
        if (startIdx >= 0 && endIdx > startIdx) {
            String arrayStr = line.substring(startIdx + 1, endIdx);
            int idx = 0;
            int pos = 0;
            while (pos < (int)arrayStr.length() && idx < 10) {
                int commaIdx = arrayStr.indexOf(',', pos);
                if (commaIdx == -1) commaIdx = arrayStr.length();
                String val = arrayStr.substring(pos, commaIdx);
                val.trim();
                config.gpioPwmDutyCycle[idx++] = val.toFloat();
                pos = commaIdx + 1;
            }
        }
    }
    // Parse PWM enabled array
    else if (line.startsWith("pwmEnabled:")) {
        int startIdx = line.indexOf('[');
        int endIdx = line.indexOf(']');
        if (startIdx >= 0 && endIdx > startIdx) {
            String arrayStr = line.substring(startIdx + 1, endIdx);
            int idx = 0;
            int pos = 0;
            while (pos < (int)arrayStr.length() && idx < 10) {
                int commaIdx = arrayStr.indexOf(',', pos);
                if (commaIdx == -1) commaIdx = arrayStr.length();
                String val = arrayStr.substring(pos, commaIdx);
                val.trim();
                bool parseSuccess;
                config.gpioPwmEnabled[idx++] = parseBoolean(val, parseSuccess);
                pos = commaIdx + 1;
            }
        }
    }
    // Parse UART
    else if (line.startsWith("uart:")) {
        int txIdx = line.indexOf("txFunction:");
        if (txIdx >= 0) {
            int commaIdx = line.indexOf(',', txIdx);
            String val = line.substring(txIdx + 11, commaIdx);
            val.trim();
            config.uartTxFunction = val.toInt();
        }
        
        int rxIdx = line.indexOf("rxFunction:");
        if (rxIdx >= 0) {
            int endIdx = line.indexOf('}', rxIdx);
            String val = line.substring(rxIdx + 11, endIdx);
            val.trim();
            config.uartRxFunction = val.toInt();
        }
    }
    // Parse OLED
    else if (line.startsWith("oled:")) {
        int connectedIdx = line.indexOf("connected:");
        if (connectedIdx >= 0) {
            int commaIdx = line.indexOf(',', connectedIdx);
            String val = line.substring(connectedIdx + 10, commaIdx);
            val.trim();
            bool parseSuccess;
            config.oledConnected = parseBoolean(val, parseSuccess);
        }
        
        int lockIdx = line.indexOf("lockConnection:");
        if (lockIdx >= 0) {
            int endIdx = line.indexOf('}', lockIdx);
            String val = line.substring(lockIdx + 15, endIdx);
            val.trim();
            bool parseSuccess;
            config.oledLockConnection = parseBoolean(val, parseSuccess);
        }
    }
    
    return true;
}

void JumperlessState::serializeFakeGpio(String& output) const {
    // Include FakeGpio.h for access to fakeGpioPins array
    extern FakeGpioPinConfig fakeGpioPins[];
    
    // Count active fake GPIO pins
    int activeCount = 0;
    for (int i = 0; i < MAX_FAKE_GPIO; i++) {
        if (fakeGpioPins[i].active) {
            activeCount++;
        }
    }
    
    if (activeCount == 0) {
        return;  // Don't output empty section
    }
    
    output += "  fakeGpio:\n";
    
    for (int slot = 0; slot < MAX_FAKE_GPIO; slot++) {
        if (!fakeGpioPins[slot].active) continue;
        
        const FakeGpioPinConfig& pin = fakeGpioPins[slot];
        
        output += "    - {slot: " + String(slot);
        output += ", node: " + String(pin.node);
        output += ", mode: " + String(pin.mode);
        
        if (pin.mode == 1) {  // OUTPUT
            output += ", v_high: " + String(pin.v_high, 2);
            output += ", v_low: " + String(pin.v_low, 2);
        }
        
        output += ", th_high: " + String(pin.threshold_high, 2);
        output += ", th_low: " + String(pin.threshold_low, 2);
        output += "}\n";
    }
}

bool JumperlessState::deserializeFakeGpio(const char* yamlContent, String& errorMsg) {
    // Parse fake GPIO entry: - {slot: 0, node: 20, mode: 1, v_high: 8.0, v_low: -8.0, th_high: 2.0, th_low: 0.8}
    String line = String(yamlContent);
    line.trim();
    
    int slot = -1;
    int node = -1;
    int mode = -1;
    float v_high = 0.0;
    float v_low = 0.0;
    float th_high = 2.0;
    float th_low = 0.8;
    
    // Parse slot field
    int slotIdx = line.indexOf("slot:");
    if (slotIdx >= 0) {
        int commaIdx = line.indexOf(',', slotIdx);
        String val = line.substring(slotIdx + 5, commaIdx);
        val.trim();
        slot = val.toInt();
    }
    
    // Parse node field
    int nodeIdx = line.indexOf("node:");
    if (nodeIdx >= 0) {
        int commaIdx = line.indexOf(',', nodeIdx);
        String val = line.substring(nodeIdx + 5, commaIdx);
        val.trim();
        node = val.toInt();
    }
    
    // Parse mode field
    int modeIdx = line.indexOf("mode:");
    if (modeIdx >= 0) {
        int commaIdx = line.indexOf(',', modeIdx);
        if (commaIdx == -1) commaIdx = line.indexOf('}', modeIdx);
        String val = line.substring(modeIdx + 5, commaIdx);
        val.trim();
        mode = val.toInt();
    }
    
    // Parse v_high field (optional, for OUTPUT mode)
    int vHighIdx = line.indexOf("v_high:");
    if (vHighIdx >= 0) {
        int commaIdx = line.indexOf(',', vHighIdx);
        if (commaIdx == -1) commaIdx = line.indexOf('}', vHighIdx);
        String val = line.substring(vHighIdx + 7, commaIdx);
        val.trim();
        v_high = val.toFloat();
    }
    
    // Parse v_low field (optional, for OUTPUT mode)
    int vLowIdx = line.indexOf("v_low:");
    if (vLowIdx >= 0) {
        int commaIdx = line.indexOf(',', vLowIdx);
        if (commaIdx == -1) commaIdx = line.indexOf('}', vLowIdx);
        String val = line.substring(vLowIdx + 6, commaIdx);
        val.trim();
        v_low = val.toFloat();
    }
    
    // Parse th_high field
    int thHighIdx = line.indexOf("th_high:");
    if (thHighIdx >= 0) {
        int commaIdx = line.indexOf(',', thHighIdx);
        if (commaIdx == -1) commaIdx = line.indexOf('}', thHighIdx);
        String val = line.substring(thHighIdx + 8, commaIdx);
        val.trim();
        th_high = val.toFloat();
    }
    
    // Parse th_low field
    int thLowIdx = line.indexOf("th_low:");
    if (thLowIdx >= 0) {
        int endIdx = line.indexOf('}', thLowIdx);
        String val = line.substring(thLowIdx + 7, endIdx);
        val.trim();
        th_low = val.toFloat();
    }
    
    // Store in temporary structure for later restoration
    // We can't directly configure here because bridges might not be loaded yet
    if (slot >= 0 && slot < MAX_FAKE_GPIO && node >= 0 && mode >= 0) {
        // Store in config for later restoration
        FakeGpioRestorationInfo info;
        info.slot = slot;
        info.node = node;
        info.mode = mode;
        info.v_high = v_high;
        info.v_low = v_low;
        info.threshold_high = th_high;
        info.threshold_low = th_low;
        
        // Add to restoration list
        pendingFakeGpioRestorations.push_back(info);
    }
    
    return true;
}

// ============================================================================
// YAML Parsing Helpers - Handle Aliases and Variations
// ============================================================================

/**
 * @brief Parse node name with alias support
 * Accepts variations like: TOP_RAIL, topRail, t_rAiL, 101
 * Returns the defined integer value or -1 if not found
 */
int parseNodeName(const String& nodeName) {
    String normalized = nodeName;
    normalized.trim();
    normalized.toUpperCase();
    normalized.replace(" ", "_");
    
    // Try to parse as integer first
    if (normalized.length() > 0 && (isdigit(normalized.charAt(0)) || normalized.charAt(0) == '-')) {
        int val = normalized.toInt();
        if (val >= 0 && val <= 200) {  // Valid node range
            return val;
        }
    }
    
    // Search through nano defines
    extern const DefineInfo nanoDefines[];
    // Count elements in nanoDefines - it has 35 elements based on the defNanoToCharShort array
    const int numNanoDefines = 35;
    for (int i = 0; i < numNanoDefines; i++) {
        String longName = String(nanoDefines[i].longName);
        String shortName = String(nanoDefines[i].shortName);
        longName.toUpperCase();
        shortName.toUpperCase();
        
        if (normalized == longName || normalized == shortName) {
            return nanoDefines[i].defineValue;
        }
    }
    
    // Search through special defines
    extern const DefineInfo specialDefines[];
    // Count elements in specialDefines - updated to 70 elements (added 32 FAKE_GPIO, removed 7 PAD defines, was 49)
    const int numSpecialDefines = 70;
    for (int i = 0; i < numSpecialDefines; i++) {
        String longName = String(specialDefines[i].longName);
        String shortName = String(specialDefines[i].shortName);
        longName.toUpperCase();
        shortName.toUpperCase();
        
        if (normalized == longName || normalized == shortName) {
            return specialDefines[i].defineValue;
        }
    }
    
    // Handle common variations not in the arrays
    if (normalized == "TOPRAIL" || normalized == "T_R" || normalized == "TOP_R") return TOP_RAIL;
    if (normalized == "BOTTOMRAIL" || normalized == "BOTRAIL" || normalized == "B_R" || normalized == "BOT_R") return BOTTOM_RAIL;
    if (normalized == "GROUND") return GND;
    if (normalized == "3V3" || normalized == "3.3V") return SUPPLY_3V3;
    if (normalized == "5V" || normalized == "+5V") return SUPPLY_5V;
    
    return -1;  // Not found
}

/**
 * @brief Convert node value back to canonical string name
 * Returns the long form name (e.g., TOP_RAIL)
 */
String nodeValueToString(int nodeValue) {
    // Use the existing definesToChar function to get the long name
    const char* name = definesToChar(nodeValue, 1);  // 1 = long name
    if (name != nullptr && strlen(name) > 0) {
        String result = String(name);
        result.trim();
        return result;
    }
    
    // Fallback to number if name not found
    return String(nodeValue);
}

/**
 * @brief Parse color value from name or hex
 * Accepts: "red", "0xFF0000", "16711680"
 * Returns uint32_t color value
 */
uint32_t parseColorValue(const String& colorStr, bool& success) {
    String normalized = colorStr;
    normalized.trim();
    normalized.toLowerCase();
    
    success = true;

   

    
    // Try hex format first
    if (normalized.startsWith("0x") || normalized.startsWith("#")) {
        uint32_t color = scaleBrightness(strtoul(normalized.c_str() + 2, NULL, 16), -80   );
        if (debugFP) {
            Serial.printf("parsed 0x color value: %14s 0x%06X\n", normalized.c_str(), color);
        }
        return color;
    
    }
    
    // Try decimal number
    if (normalized.length() > 0 && isdigit(normalized.charAt(0))) {
        uint32_t color = scaleBrightness(normalized.toInt(), -50);
        if (debugFP) {  
            Serial.printf("parsed decimal color value: %14s 0x%06X\n", normalized.c_str(), color);
        }
        return color;
        
    }
    
    // FIRST: Try Wokwi colors (with proper brightness scaling for wire colors)
    // This ensures consistency between Wokwi JSON import and YAML reload
//     uint32_t wokwiColor = wokwiColorToRGB(normalized);
//    // if (wokwiColor != 0x0a0a0a) {  // 0x0a0a0a is the "not found" default from wokwiColorToRGB
//         return wokwiColor;
//     //}
    
    // SECOND: Try internal named colors for user-assigned colors
    int numColors = sizeof(namedColors) / sizeof(namedColors[0]);
    for (int i = 0; i < numColors; i++) {
        String colorName = String(namedColors[i].name);
        colorName.trim();
        colorName.toLowerCase();
        
        if (normalized == colorName) {
            
            uint32_t color = scaleBrightness(namedColors[i].dimColor, -50);  // Use dimColor for LED display
            if (debugFP) {
                Serial.printf("parsed named color value: %14s 0x%06X\n", colorName.c_str(), color);
            }
            return color;
            
        }
    }
    
    // Not found
    success = false;
    if (debugFP) {
        Serial.println("parsed color value: " + normalized);
    }
    return 0x000000;
}

/**
 * @brief Get color name from value
 * Returns the canonical name from namedColors[]
 */
String colorValueToName(uint32_t color) {
    // Use existing colorToName function
    char* name = colorToName(color, -1);
    if (name != nullptr) {
        String result = String(name);
        result.trim();
        return result;
    }
    return "0x" + String(color, HEX);
}

/**
 * @brief Parse boolean with aliases
 * Accepts: true/false, 1/0, on/off, yes/no, enabled/disabled
 */
bool parseBoolean(const String& val, bool& success) {
    String normalized = val;
    normalized.trim();
    normalized.toLowerCase();
    
    success = true;
    
    // True values
    if (normalized == "true" || normalized == "1" || normalized == "on" || 
        normalized == "yes" || normalized == "enabled") {
        return true;
    }
    
    // False values
    if (normalized == "false" || normalized == "0" || normalized == "off" || 
        normalized == "no" || normalized == "disabled") {
        return false;
    }
    
    // Invalid
    success = false;
    return false;
}

/**
 * @brief Convert boolean to canonical string
 */
String booleanToString(bool value) {
    return value ? "true" : "false";
}

// ============================================================================
// Legacy Format Support
// ============================================================================

bool JumperlessState::fromLegacyNodeFile(const String& nodeFileContent, String& errorMsg) {
    clear();
    
    // Legacy format is just: { node1-node2, node3-node4, ... }
    // We need to parse this and extract connections
    
    String content = nodeFileContent;
    content.trim();
    
    // Remove curly braces
    int openBrace = content.indexOf('{');
    int closeBrace = content.lastIndexOf('}');
    
    if (openBrace == -1 || closeBrace == -1) {
        errorMsg = "Invalid legacy format: missing curly braces";
        return false;
    }
    
    content = content.substring(openBrace + 1, closeBrace);
    content.trim();
    
    if (content.length() == 0) {
        // Empty slot is valid
        return true;
    }
    
    // Parse connections
    int startIdx = 0;
    while (startIdx < (int)content.length()) {
        int commaIdx = content.indexOf(',', startIdx);
        if (commaIdx == -1) {
            commaIdx = content.length();
        }
        
        String bridge = content.substring(startIdx, commaIdx);
        bridge.trim();
        
        if (bridge.length() > 0) {
            int dashIdx = bridge.indexOf('-');
            if (dashIdx == -1) {
                errorMsg = "Invalid bridge format in legacy file: " + bridge;
                return false;
            }
            
            int node1 = bridge.substring(0, dashIdx).toInt();
            int node2 = bridge.substring(dashIdx + 1).toInt();
            
            if (!addConnection(node1, node2, errorMsg)) {
                errorMsg = "Error adding connection from legacy file: " + errorMsg;
                return false;
            }
        }
        
        startIdx = commaIdx + 1;
    }
    
    return true;
}

// ============================================================================
// Helper function to apply loaded state to hardware
// ============================================================================

/**
 * @brief Apply the current globalState to all hardware peripherals
 * 
 * This function is called after loading a slot to ensure that:
 * - DAC voltages match the loaded power state
 * - GPIO directions and pull resistors match the loaded config
 * - All hardware reflects the state loaded from the file
 */
void applyStateToHardware() {
    // Apply power settings (DACs and rails)
    // Note: Pass save=0 to avoid updating globalState (it's already loaded)
    //       and saveEEPROM=0 to avoid writing to EEPROM
    setRailsAndDACs(0);  // This applies topRail, bottomRail, dac0, dac1 from globalState



    // Apply GPIO configurations from globalState to hardware
    for (int i = 0; i < 10; i++) {
        uint8_t gpio_pin = gpioDef[i][0];
        
        // Apply direction to hardware
        if (globalState.config.gpioDirection[i] == 0) {
            gpio_set_dir(gpio_pin, true);  // output
        } else {
            gpio_set_dir(gpio_pin, false);  // input
        }
        
        // Apply pull resistors to hardware and update gpioState for animations
        switch (globalState.config.gpioPulls[i]) {
            case 0: // pulldown
                gpio_set_pulls(gpio_pin, false, true);
                if (globalState.config.gpioDirection[i] == 1) {
                    gpioState[i] = 4;  // input with pulldown
                }
                break;
            case 1: // pullup
                gpio_set_pulls(gpio_pin, true, false);
                if (globalState.config.gpioDirection[i] == 1) {
                    gpioState[i] = 3;  // input with pullup
                }
                break;
            case 2: // no pull
                gpio_set_pulls(gpio_pin, false, false);
                if (globalState.config.gpioDirection[i] == 1) {
                    gpioState[i] = 2;  // input with no pull
                }
                break;
            case 3: // bus keeper
                gpio_set_pulls(gpio_pin, true, true);
                if (globalState.config.gpioDirection[i] == 1) {
                    gpioState[i] = 7;  // bus keeper mode
                }
                break;
            default:
                gpio_set_pulls(gpio_pin, false, false);
                if (globalState.config.gpioDirection[i] == 1) {
                    gpioState[i] = 2;  // input with no pull
                }
                break;
        }
        
        // Set initial output state for output pins
        if (globalState.config.gpioDirection[i] == 0) {
            gpio_put(gpio_pin, gpioState[i]);
        }
    }
    
    if (debugFP) {
        Serial.println("✓ Applied state to hardware (power, GPIO)");
    }
}

// ============================================================================
// SlotManager Implementation
// ============================================================================

SlotManager::SlotManager() 
    : activeState(globalState), activeSlotNumber(0), historySize(STATE_HISTORY_SIZE), 
      historyHead(0), historyCount(0), historyPosition(0),
      previewModeActive(false), previewSlotNumber(-1), originalSlotNumber(-1),
      temporarySlotActive(false), temporarySlotOriginal(-1) {
    // Always initialize to slot 0, sync with netSlot on first use
    netSlot = 0;  // Ensure global is also 0
    initHistory();
}

SlotManager& SlotManager::getInstance() {
    static SlotManager instance;
    return instance;
}

void SlotManager::initHistory() {
    historyBuffer = new JumperlessState[historySize];
}

void SlotManager::cleanupHistory() {
    if (historyBuffer) {
        delete[] historyBuffer;
        historyBuffer = nullptr;
    }
}

JumperlessState& SlotManager::getActiveState() {
    return activeState;
}

const JumperlessState& SlotManager::getActiveState() const {
    return activeState;
}

String SlotManager::getSlotFilename(int slotNum) const {
    // Special case for Python slot (slot 99)
    if (slotNum == 99) {
        return "/slots/slotPython.yaml";
    }
    return "/slots/slot" + String(slotNum) + ".yaml";
}

String SlotManager::getLegacySlotFilename(int slotNum) const {
    return "/nodeFileSlot" + String(slotNum) + ".txt";
}

String SlotManager::getJSONSlotFilename(int slotNum) const {
    return "/slots/slot" + String(slotNum) + ".json";
}

// Helper function to extract slot number from filename
// Returns -1 if not a slot file
static int extractSlotNumberFromFilename(const char* filename) {
    if (!filename) return -1;
    
    String fname(filename);
    
    // Special case for Python slot
    if (fname == "/slots/slotPython.yaml") {
        return 99;
    }
    
    // Check if filename matches pattern "/slots/slotN.yaml"
    if (!fname.startsWith("/slots/slot") || !fname.endsWith(".yaml")) {
        return -1;
    }
    
    // Extract number between "slot" and ".yaml"
    int slotStart = fname.indexOf("slot") + 4;
    int yamlStart = fname.indexOf(".yaml");
    if (slotStart < 4 || yamlStart <= slotStart) {
        return -1;
    }
    
    String numStr = fname.substring(slotStart, yamlStart);
    int slotNum = numStr.toInt();
    
    // Validate it's a valid slot number (0-7 for normal slots, 99 for Python)
    if (slotNum < 0 || (slotNum >= NUM_SLOTS && slotNum != 99)) {
        return -1;
    }
    
    return slotNum;
}

bool SlotManager::slotExists(int slotNum) const {
    // Allow slot 99 (Python slot) in addition to 0-7
    if (slotNum < 0 || (slotNum >= NUM_SLOTS && slotNum != 99)) {
        return false;
    }
    
    String filename = getSlotFilename(slotNum);
    if (FatFS.exists(filename.c_str())) {
        return true;
    }
    
    // Check for legacy format (not applicable to Python slot)
    if (slotNum != 99) {
        String legacyFilename = getLegacySlotFilename(slotNum);
        return FatFS.exists(legacyFilename.c_str());
    }
    
    return false;
}

bool SlotManager::loadSlot(int slotNum, String& errorMsg) {
    // Allow slot 99 (Python slot) in addition to 0-7
    if (slotNum < 0 || (slotNum >= NUM_SLOTS && slotNum != 99)) {
        errorMsg = "Invalid slot number: " + String(slotNum);
        return false;
    }
    
    String content;
    String filename = getSlotFilename(slotNum);

    // Serial.println("Loading slot " + String(slotNum) + " from " + filename);
    // Serial.flush();
    
    // Try loading YAML slot file
    if (FatFS.exists(filename.c_str())) {
        // Serial.println("  File exists, reading...");
        // Serial.flush();
        
        if (!readSlotFile(slotNum, content, errorMsg)) {
            Serial.println("  Read failed: " + errorMsg);
            Serial.flush();
            return false;
        }
        
        // Serial.println("  Parsing YAML...");
        // Serial.flush();
        
        if (!activeState.fromYAML(content, errorMsg)) {
            errorMsg = "Failed to parse YAML slot " + String(slotNum) + ": " + errorMsg;
            Serial.println("  Parse failed: " + errorMsg);
            Serial.flush();
            return false;
        }
        
        // Serial.println("  ✓ Loaded " + String(activeState.connections.numBridges) + " connections");
        // Serial.flush();
        
        // Compute nets from bridges and commit to hardware
        // refreshLocalConnections does: loadBridgesFromState, getNodesToConnect, 
        // bridgesToPaths, commitPaths, sendPaths, and LED updates
        extern void refreshConnections(int ledShowOption, int fillUnused, int clean);
        refreshConnections(-1, 1, 0);  // Update connections, show LEDs, clean commit
        
        // In normal mode (not preview), also apply DAC/GPIO settings
        if (!previewModeActive) {
            applyStateToHardware();  // Apply power rails and GPIO configs
            if (debugFP) {
                Serial.println("  ✓ Applied state to hardware (connections, LEDs, power, GPIO)");
            }
        } else {
            if (debugFP) {
                Serial.println("  ✓ Preview mode - connections and LEDs updated (power/GPIO unchanged)");
            }
        }
        
        activeSlotNumber = slotNum;
        netSlot = slotNum;  // Sync global slot tracker
        return true;
    }
    
    // Slot file doesn't exist - start with empty state
    // Serial.println("  File doesn't exist, using empty slot");
    // Serial.flush();
    
    activeState.clear();
    activeSlotNumber = slotNum;
    netSlot = slotNum;  // Sync global slot tracker
    
    // DON'T create the file yet - only create when something is saved
    // This prevents crashes from creating large objects on the stack
    
    return true;
}

// Static cache for /slots directory existence (avoid repeated FS checks)
static bool slotsDirectoryExists = false;

bool SlotManager::saveSlot(int slotNum, String& errorMsg, bool skipValidation) {
    // Allow slot 99 (Python slot) in addition to 0-7
    if (slotNum < 0 || (slotNum >= NUM_SLOTS && slotNum != 99)) {
        errorMsg = "Invalid slot number: " + String(slotNum);
        return false;
    }
    
    // Cache /slots directory existence check (avoid FS call on every save)
    if (!slotsDirectoryExists) {
        if (!FatFS.exists("/slots")) {
            if (!FatFS.mkdir("/slots")) {
                errorMsg = "Failed to create /slots directory";
                return false;
            }
        }
        slotsDirectoryExists = true;
    }
    
    // Optionally skip validation for auto-saves (state is validated on connection add/remove)
    if (!skipValidation) {
        if (!activeState.validate(errorMsg)) {
            errorMsg = "Cannot save invalid state: " + errorMsg;
            Serial.println("  Validation failed: " + errorMsg);
            Serial.flush();
            return false;
        }
    }
    
    // Convert to YAML
    String yamlContent;
    if (!activeState.toYAML(yamlContent)) {
        errorMsg = "Failed to serialize state to YAML";
        Serial.println("  Serialization failed");
        Serial.flush();
        return false;
    }
    
    // Write to file (this will create the file if it doesn't exist)
    if (!writeSlotFile(slotNum, yamlContent, errorMsg)) {
        Serial.println("  Write failed: " + errorMsg);
        Serial.flush();
        return false;
    }
    
    activeSlotNumber = slotNum;
    netSlot = slotNum;  // Sync global slot tracker
    activeState.clearDirty();  // Mark as saved
    return true;
}

bool SlotManager::saveActiveSlot(String& errorMsg, bool skipValidation) {
    if (activeSlotNumber < 0) {
        errorMsg = "No active slot to save";
        return false;
    }
    return saveSlot(activeSlotNumber, errorMsg, skipValidation);
}

bool SlotManager::deleteSlot(int slotNum, String& errorMsg) {
    // Allow slot 99 (Python slot) in addition to 0-7
    if (slotNum < 0 || (slotNum >= NUM_SLOTS && slotNum != 99)) {
        errorMsg = "Invalid slot number: " + String(slotNum);
        return false;
    }
    
    String filename = getSlotFilename(slotNum);
    if (FatFS.exists(filename.c_str())) {
        if (!FatFS.remove(filename.c_str())) {
            errorMsg = "Failed to delete slot file: " + filename;
            return false;
        }
    }
    
    // // Also delete legacy format if it exists
    // String legacyFilename = getLegacySlotFilename(slotNum);
    // if (FatFS.exists(legacyFilename.c_str())) {
    //     FatFS.remove(legacyFilename.c_str());
    // }
    
    // If this was the active slot, clear it
    if (activeSlotNumber == slotNum) {
        activeState.clear();
        activeSlotNumber = -1;
        netSlot = 0;  // Reset to slot 0
    }
    
    return true;
}

void SlotManager::clearActiveSlot() {
    activeState.clear();
}

void SlotManager::setActiveSlot(int slotNum) {
    activeSlotNumber = slotNum;
    netSlot = slotNum;  // Keep global slot tracker in sync
}

void SlotManager::syncFromGlobalNetSlot() {
    // Call this when netSlot changes externally (e.g., rotary encoder)
    if (activeSlotNumber != netSlot) {
        activeSlotNumber = netSlot;
        
        // Optionally auto-load the new slot
        // String errorMsg;
        // loadSlot(netSlot, errorMsg);
    }
}

// ============================================================================
// Temporary Slot Mode - For apps that need a working slot and restore when done
// Unlike preview mode, this clears the temp slot and applies changes to hardware
// Optimized for speed - skips unnecessary file I/O and LED updates
// ============================================================================

bool SlotManager::enterTemporarySlot(int tempSlot, bool saveCurrentFirst) {
    // Validate temp slot number (allow slot 8 and slot 99 as special temp slots)
    if (tempSlot < 0 || tempSlot >= NUM_SLOTS) {
        // Allow slot 8 specifically as the app temporary slot
        if (tempSlot != 8 && tempSlot != 99) {
            return false;
        }
    }
    
    // Don't nest temporary slot modes
    if (temporarySlotActive) {
        return false;
    }
    
    // Only save if state has actually changed (skip expensive filesystem write)
    if (saveCurrentFirst && activeState.isDirty()) {
        String errorMsg;
        saveActiveSlot(errorMsg);
    }
    
    // Remember where we came from
    temporarySlotOriginal = activeSlotNumber;
    
    // Switch to the temporary slot - fast, just updates slot tracking
    netSlot = tempSlot;
    activeSlotNumber = tempSlot;
    
    // Clear the active state for fresh start (fast - no file I/O)
    // Skip loading temp slot since apps will set up their own connections anyway
    clearActiveSlot();
    
    temporarySlotActive = true;
    return true;
}

bool SlotManager::exitTemporarySlot(bool refreshHardware) {
    if (!temporarySlotActive) {
        return false;
    }
    
    // Restore slot tracking
    netSlot = temporarySlotOriginal;
    activeSlotNumber = temporarySlotOriginal;
    
    // Load the original slot data from file
    // This is necessary to restore the user's connections
    String content;
    String errorMsg;
    String filename = getSlotFilename(temporarySlotOriginal);
    
    if (FatFS.exists(filename.c_str())) {
        if (readSlotFile(temporarySlotOriginal, content, errorMsg)) {
            activeState.fromYAML(content, errorMsg);
        }
    }
    
    temporarySlotActive = false;
    temporarySlotOriginal = -1;
    
    // Refresh hardware connections (caller can skip if they'll do it manually)
    if (refreshHardware) {
        extern void refreshConnections(int ledShowOption, int fillUnused, int clean);
        refreshConnections(-1, 0, 1);
    }
    
    return true;
}

bool SlotManager::ensureSlotExists(int slotNum) {
    if (slotExists(slotNum)) {
        return true;
    }
    
    // Create empty slot by serializing the current globalState
    // This avoids creating a large JumperlessState on the stack
    String errorMsg;
    String yamlContent;
    
    if (!globalState.toYAML(yamlContent)) {
        return false;
    }
    
    return writeSlotFile(slotNum, yamlContent, errorMsg);
}

// File I/O helpers
bool SlotManager::readSlotFile(int slotNum, String& content, String& errorMsg) {
    // THREAD SAFETY: Acquire filesystem mutex first
    fs_mutex_acquire();
    
    String filename = getSlotFilename(slotNum);
    
    extern bool mscModeEnabled;
    if (mscModeEnabled && debugUSB) {
        Serial.print("USB: readSlotFile() called for slot ");
        Serial.print(slotNum);
        Serial.print(" (");
        Serial.print(filename);
        Serial.println(")");
        Serial.flush();
    }
    
    // Detect which core we're running on and synchronize appropriately
    uint coreNum = get_core_num();
    
    // Add timeout to prevent deadlock during boot or race conditions
    unsigned long timeout = millis() + 5000;  // 5 second timeout
    
    if (coreNum == 0) {
        // Running on core0 (core1 in RP2040 terms) - wait for core2, set core1busy
        while (core2busy) {
            if (millis() > timeout) {
                errorMsg = "Timeout waiting for core2 (possible deadlock)";
                fs_mutex_release();  // THREAD SAFETY: Release mutex before early return
                return false;
            }
            delay(1);
        }
        core1busy = true;
    } else {
        // Running on core1 (core2 in RP2040 terms) - wait for core1, set core2busy
        while (core1busy) {
            if (millis() > timeout) {
                errorMsg = "Timeout waiting for core1 (possible deadlock)";
                fs_mutex_release();  // THREAD SAFETY: Release mutex before early return
                return false;
            }
            delay(1);
        }
        core2busy = true;
    }
    
    if (mscModeEnabled && debugUSB) {
        Serial.println("USB: Core sync acquired, attempting to open file...");
        Serial.flush();
    }
    
    // CRITICAL: In USB mode, add extra delay before opening
    // NOTE: We don't sync here because we already synced before f_stat in the caller.
    // Multiple rapid disk_ioctl(CTRL_SYNC) calls cause crashes.
    if (mscModeEnabled && debugUSB) {
        delay(50);
    }
    
    File file = FatFS.open(filename.c_str(), "r");
    if (!file) {
        errorMsg = "Failed to open slot file: " + filename;
        if (mscModeEnabled && debugUSB) {
            Serial.print("USB: ✗ Failed to open file: ");
            Serial.println(errorMsg);
            Serial.flush();
        }
        if (coreNum == 0) {
            core1busy = false;
        } else {
            core2busy = false;
        }
        fs_mutex_release();  // THREAD SAFETY: Release mutex before early return
        return false;
    }
    
    if (mscModeEnabled && debugUSB) {
        Serial.println("USB: File opened successfully, reading content...");
        Serial.flush();
    }
    
    content = "";
    size_t bytesRead = 0;
    const size_t maxBytes = 65536;  // 64KB safety limit
    
    while (file.available() && bytesRead < maxBytes) {
        int c = file.read();
        if (c >= 0) {
            content += (char)c;
            bytesRead++;
        } else {
            // Read error
            if (mscModeEnabled && debugUSB) {
                Serial.println("USB: ⚠ Read error during file read");
                Serial.flush();
            }
            break;
        }
    }
    
    if (mscModeEnabled && debugUSB) {
        Serial.print("USB: Read ");
        Serial.print(bytesRead);
        Serial.println(" bytes, closing file...");
        Serial.flush();
    }
    
    file.close();
    
    if (mscModeEnabled && debugUSB) {
        Serial.println("USB: File closed successfully");
        Serial.flush();
    }
    
    if (coreNum == 0) {
        core1busy = false;
    } else {
        core2busy = false;
    }
    
    fs_mutex_release();  // THREAD SAFETY: Release mutex
    return true;
}

bool SlotManager::writeSlotFile(int slotNum, const String& content, String& errorMsg) {
    // Ensure slots directory exists using safe function
    if (!safeMkdir("/slots", 2000)) {
        errorMsg = "Failed to create /slots directory";
        return false;
    }
    
    String filename = getSlotFilename(slotNum);
    
    // Use safe file write which handles Core2 pause and mutex internally
    if (!safeFileWriteAll(filename.c_str(), content.c_str(), content.length(), 2000)) {
        errorMsg = "Failed to write slot file: " + filename;
        return false;
    }
    
    return true;
}

bool SlotManager::migrateOldSlotFile(int slotNum, String& errorMsg) {
    // THREAD SAFETY: Acquire filesystem mutex first
    fs_mutex_acquire();
    
    String legacyFilename = getLegacySlotFilename(slotNum);
    
    // Detect which core we're running on and synchronize appropriately
    uint coreNum = get_core_num();
    
    if (coreNum == 0) {
        // Running on core0 (core1 in RP2040 terms) - wait for core2, set core1busy
        while (core2busy) {
            delay(1);
        }
        core1busy = true;
    } else {
        // Running on core1 (core2 in RP2040 terms) - wait for core1, set core2busy
        while (core1busy) {
            delay(1);
        }
        core2busy = true;
    }
    
    File file = FatFS.open(legacyFilename.c_str(), "r");
    if (!file) {
        errorMsg = "Failed to open legacy slot file: " + legacyFilename;
        if (coreNum == 0) {
            core1busy = false;
        } else {
            core2busy = false;
        }
        fs_mutex_release();  // THREAD SAFETY: Release mutex before early return
        return false;
    }
    
    String content = "";
    while (file.available()) {
        content += (char)file.read();
    }
    file.close();
    fs_mutex_release();  // THREAD SAFETY: Release mutex after file operations
    
    if (coreNum == 0) {
        core1busy = false;
    } else {
        core2busy = false;
    }
    
    // Parse legacy format
    JumperlessState newState;
    if (!newState.fromLegacyNodeFile(content, errorMsg)) {
        errorMsg = "Failed to migrate legacy slot " + String(slotNum) + ": " + errorMsg;
        return false;
    }
    
    // Set default voltages for migrated old slot files (voltages no longer in config)
    newState.setDacVoltage(0, 3.3);  // Default DAC0 voltage
    newState.setDacVoltage(1, 0.0);  // Default DAC1 voltage
    newState.setRailVoltage(true, 0.0);   // Default top rail voltage
    newState.setRailVoltage(false, 0.0);  // Default bottom rail voltage
    newState.setPathStacking(jumperlessConfig.routing.stack_paths, 
                             jumperlessConfig.routing.stack_rails,
                             jumperlessConfig.routing.stack_dacs);
    
    // Save in new format
    activeState = newState;
    if (!saveSlot(slotNum, errorMsg)) {
        return false;
    }
    
    activeSlotNumber = slotNum;
    netSlot = slotNum;  // Sync global slot tracker
    
    Serial.println("✓ Migrated legacy slot " + String(slotNum) + " to new format");
    return true;
}

// History management
void SlotManager::pushHistory() {
    // Save current state to history buffer
    historyBuffer[historyHead] = activeState;
    
    // Move head forward
    historyHead = (historyHead + 1) % historySize;
    
    // Update count
    if (historyCount < historySize) {
        historyCount++;
    }
    
    // Reset position to head (can't redo after new change)
    historyPosition = historyHead;
}

bool SlotManager::canUndo() const {
    return historyCount > 0;
}

bool SlotManager::canRedo() const {
    return historyPosition != historyHead;
}

bool SlotManager::undo(String& errorMsg) {
    if (!canUndo()) {
        errorMsg = "Nothing to undo";
        return false;
    }
    
    // Move position back
    historyPosition = (historyPosition - 1 + historySize) % historySize;
    
    // Restore state
    activeState = historyBuffer[historyPosition];
    
    return true;
}

bool SlotManager::redo(String& errorMsg) {
    if (!canRedo()) {
        errorMsg = "Nothing to redo";
        return false;
    }
    
    // Move position forward
    historyPosition = (historyPosition + 1) % historySize;
    
    // Restore state
    activeState = historyBuffer[historyPosition];
    
    return true;
}

void SlotManager::clearHistory() {
    historyHead = 0;
    historyCount = 0;
    historyPosition = 0;
}

int SlotManager::getHistoryDepth() const {
    if (historyPosition <= historyHead) {
        return historyHead - historyPosition;
    } else {
        return historySize - historyPosition + historyHead;
    }
}

// Utility
void SlotManager::printSlotInfo(int slotNum) {
    Serial.println("=== Slot " + String(slotNum) + " Info ===");
    
    if (!slotExists(slotNum)) {
        Serial.println("Slot does not exist");
        return;
    }
    
    String errorMsg;
    JumperlessState tempState;
    
    // Save current active state
    JumperlessState savedState = activeState;
    int savedSlot = activeSlotNumber;
    
    // Load the slot
    if (!loadSlot(slotNum, errorMsg)) {
        Serial.println("Error loading slot: " + errorMsg);
        activeState = savedState;
        activeSlotNumber = savedSlot;
        return;
    }
    
    Serial.println("Connections: " + String(activeState.connections.numBridges));
    Serial.println("Power:");
    Serial.println("  Top Rail:    " + String(activeState.power.topRail, 2) + "V");
    Serial.println("  Bottom Rail: " + String(activeState.power.bottomRail, 2) + "V");
    Serial.println("  DAC 0:       " + String(activeState.power.dac0, 2) + "V");
    Serial.println("  DAC 1:       " + String(activeState.power.dac1, 2) + "V");
    Serial.println("Custom Colors: " + String(activeState.display.numCustomColors));
    Serial.println("RAM Usage: ~" + String(activeState.estimateRAMUsage()) + " bytes");
    
    // Restore active state
    activeState = savedState;
    activeSlotNumber = savedSlot;
}

void SlotManager::listSlots() {
    Serial.println("\n=== Available Slots ===");
    for (int i = 0; i < NUM_SLOTS; i++) {
        if (slotExists(i)) {
            String marker = (i == activeSlotNumber) ? " [ACTIVE]" : "";
            Serial.println("Slot " + String(i) + marker);
        }
    }
    Serial.println("=======================\n");
}

size_t SlotManager::getActiveStateRAMUsage() const {
    size_t total = sizeof(SlotManager);
    total += activeState.estimateRAMUsage();
    total += historySize * sizeof(JumperlessState);
    return total;
}

// ============================================================================
// Preview Mode - Load slots without applying to hardware
// Just tracks which slot to return to when done (no state copying!)
// ============================================================================

bool SlotManager::enterPreviewMode(int slotToPreview, String& errorMsg) {
    // Allow slot 99 (Python slot) in addition to 0-7
    if (slotToPreview < 0 || (slotToPreview >= NUM_SLOTS && slotToPreview != 99)) {
        errorMsg = "Invalid slot number: " + String(slotToPreview);
        return false;
    }
    
    // Remember which slot we're currently on (to return to it later)
    if (!previewModeActive) {
        originalSlotNumber = activeSlotNumber;
        // Save original rail voltage settings
        originalRailVoltages[0] = activeState.power.topRail;
        originalRailVoltages[1] = activeState.power.bottomRail;
    }
    
    String filename = getSlotFilename(slotToPreview);
    
    // Check if slot file exists
    if (FatFS.exists(filename.c_str())) {
        // Slot exists - load it normally
        if (!loadSlot(slotToPreview, errorMsg)) {
            return false;
        }
    } else {
        // Slot doesn't exist - just show empty state (DON'T create file!)
        activeState.clear();
        activeSlotNumber = slotToPreview;
        netSlot = slotToPreview;
    }
    
    // activeState.power is now the single source of truth
    
    // Mark that we're in preview mode
    previewModeActive = true;
    previewSlotNumber = slotToPreview;
    
    return true;
}

void SlotManager::clearPreviewMode() {
    // Just clear the preview flag without loading anything
    // Used when user selects a slot from menu - we want to keep the previewed state
    // and let the normal load path handle it
    if (previewModeActive) {
        previewModeActive = false;
        previewSlotNumber = -1;
        originalSlotNumber = -1;
    }
}

bool SlotManager::exitPreview(bool applyPreview, String& errorMsg) {
    if (!previewModeActive) {
        errorMsg = "Not in preview mode";
        return false;
    }
    
    if (applyPreview) {
        // User wants to keep the previewed slot
        // If the slot file doesn't exist yet, create it now
        String filename = getSlotFilename(previewSlotNumber);
        if (!FatFS.exists(filename.c_str())) {
            // Slot was empty during preview - save it now
            if (!saveSlot(previewSlotNumber, errorMsg)) {
                previewModeActive = false;
                previewSlotNumber = -1;
                originalSlotNumber = -1;
                return false;
            }
        }
        
        // Exit preview mode and apply the previewed state to hardware
        previewModeActive = false;
        previewSlotNumber = -1;
        originalSlotNumber = -1;
        
        // Now apply the previewed state to hardware (power, GPIO, etc.)
        applyStateToHardware();
        
        // activeSlotNumber and netSlot are already set to the previewed slot
        return true;
    } else {
        // User wants to cancel - restore original slot AND rail voltages
        previewModeActive = false;
        previewSlotNumber = -1;
        
        // Restore original rail voltage settings
        activeState.power.topRail = originalRailVoltages[0];
        activeState.power.bottomRail = originalRailVoltages[1];
        
        // Load the original slot back
        int slotToRestore = originalSlotNumber;
        originalSlotNumber = -1;
        
        return loadSlot(slotToRestore, errorMsg);
    }
}

// ============================================================================
// State Backup/Restore Functions (for MicroPython entry/exit, undo, etc.)
// Uses compressed YAML format - only stores actual connections, not empty array slots
// ============================================================================

static String* stateBackupYamlPtr = nullptr;
static bool stateBackupStored = false;

void storeStateBackup(void) {
    // Store a compressed YAML snapshot of the current globalState
    // This only includes actual connections, not empty array slots
    if (stateBackupYamlPtr == nullptr) {
        stateBackupYamlPtr = new String();
    }
    
    *stateBackupYamlPtr = "";
    
    // Serialize current state to compressed YAML
    if (!globalState.toYAML(*stateBackupYamlPtr)) {
        Serial.println("Warning: Failed to serialize state backup");
        stateBackupStored = false;
        return;
    }
    
    stateBackupStored = true;
}

void restoreStateBackup(bool autoSave) {
    // Restore globalState from compressed YAML backup
    if (stateBackupStored && stateBackupYamlPtr != nullptr && stateBackupYamlPtr->length() > 0) {
        String errorMsg;
        
        if (!globalState.fromYAML(*stateBackupYamlPtr, errorMsg)) {
            Serial.println("✗ Error restoring state backup: " + errorMsg);
            return;
        }
        
        if (autoSave) {
            SlotManager& mgr = SlotManager::getInstance();
            String err;
            mgr.saveSlot(netSlot, err);
        }
    }
}

void restoreAndSaveStateBackup(void) {
    // Restore and immediately save to current slot
    restoreStateBackup(true);
}

void clearStateBackup(void) {
    // Clear the backup
    stateBackupStored = false;
    if (stateBackupYamlPtr != nullptr) {
        *stateBackupYamlPtr = "";
        delete stateBackupYamlPtr;
        stateBackupYamlPtr = nullptr;
    }
}

bool hasStateBackup(void) {
    return stateBackupStored && (stateBackupYamlPtr != nullptr) && (stateBackupYamlPtr->length() > 0);
}

bool hasStateChanges(void) {
    // Compare current state with backup by serializing and comparing YAML
    if (!hasStateBackup()) {
        return false;  // No backup means no changes to compare against
    }
    
    String currentYaml;
    if (!globalState.toYAML(currentYaml)) {
        return false;  // Can't compare if serialization fails
    }
    
    // Compare YAML representations (ignores whitespace differences in serialization)
    // For more robust comparison, we could deserialize and compare connection counts
    return (currentYaml != *stateBackupYamlPtr);
}

size_t getStateBackupSize(void) {
    // Return the size of the backup in bytes (for diagnostics/debugging)
    if (!hasStateBackup()) {
        return 0;
    }
    
    return stateBackupYamlPtr->length();
}

void printStateBackupInfo(void) {
    // Print diagnostic information about state backup memory usage
    Serial.println("\n╔═══════════════════════════════╗");
    Serial.println("║  State Backup Memory Info     ║");
    Serial.println("╚═══════════════════════════════╝");
    
    if (!hasStateBackup()) {
        Serial.println("  No backup stored");
        return;
    }
    
    size_t backupSize = stateBackupYamlPtr->length();
    size_t fullStateSize = sizeof(JumperlessState);
    size_t savings = fullStateSize - backupSize;
    float savingsPercent = (float)savings / fullStateSize * 100.0f;
    
    Serial.println("  Connections: " + String(globalState.connections.numBridges));
    Serial.println("  Nets:        " + String(globalState.connections.numNets));
    Serial.println("");
    Serial.println("  Compressed YAML: " + String(backupSize) + " bytes");
    Serial.println("  Full state:      " + String(fullStateSize) + " bytes");
    Serial.println("  Memory saved:    " + String(savings) + " bytes (" + String(savingsPercent, 1) + "%)");
    
    Serial.println("═══════════════════════════════\n");
}

/**
 * @brief Service method for auto-saving dirty state
 * 
 * This handles automatic state persistence after modifications.
 * Called periodically by the ServiceManager in the main loop.
 * 
 * IMPORTANT USB BEHAVIOR:
 * - When USB is mounted: Allows READ operations (for live file monitoring)
 *                        Blocks WRITE operations (host has exclusive write access)
 * - When USB is not mounted: Normal operation (both read and write)
 * 
 * This enables live updates when host edits files while preventing corruption.
 */
ServiceStatus SlotManager::service() {
    // return ServiceStatus::IDLE; //< this isn't the problem, not running slot manager service still freezes
    //return ServiceStatus::IDLE;
    lastStatus = ServiceStatus::IDLE;
    unsigned long serviceStart = micros();
    
    extern bool usbMountedByHost;
    extern bool usbFilesystemBusy;
    extern bool mscModeEnabled;
    extern bool debugUSB;
    
    // ============================================================================
    // FAST PATH: Early exit when there's absolutely nothing to do
    // This makes the common case extremely fast (< 1 microsecond)
    // ============================================================================
    
    // Check if we have any work to do (ordered by likelihood)
    bool hasDirtyState = activeState.isDirty();
    bool hasEditorOpen = (ekilo_get_currently_editing_file() != nullptr);
    bool inPreviewMode = previewModeActive;
    
    // CRITICAL FIX: Only monitor files when host is ACTUALLY mounted, not just when MSC is enabled
    // mscModeEnabled = USB MSC feature is on (always true after boot)
    // usbMountedByHost = host computer is actually accessing files (only when connected to PC)
    // File monitoring is EXPENSIVE (~100ms blocking) and should ONLY run when:
    // 1. Host is editing files via USB MSC (usbMountedByHost)
    // 2. User is editing files via Ekilo (hasEditorOpen)
    bool needsFileMonitoring = usbMountedByHost || hasEditorOpen;
    
    // OPTIMIZATION: During rapid command processing, skip expensive file monitoring
    // Only check files if it's been >2s since last check (prevents 600-700ms delays)
    // CRITICAL FIX: Check BOTH refreshInProgress AND refreshLocalInProgress!
    // Commands call refreshConnections() which sets refreshInProgress,
    // NOT refreshLocalInProgress. Missing this caused the 1-second freeze bug.
    extern volatile bool refreshLocalInProgress;
    extern volatile bool refreshInProgress;
    extern volatile bool core1busy;
    static unsigned long lastFileMonitorTime = 0;
    bool skipFileMonitoring = (millis() - lastFileMonitorTime < 2000) || 
                              refreshLocalInProgress || 
                              refreshInProgress ||
                              core1busy;  // Skip if ANY command processing is active
    
    // FAST EXIT: If nothing is dirty, no editor, no preview, and no need for file monitoring
    // This is the common case during UART command processing - exit immediately
    if (!hasDirtyState && !hasEditorOpen && !inPreviewMode && !needsFileMonitoring) {
        return ServiceStatus::IDLE;  // Ultra-fast return for the common case
    }
    
    // FAST EXIT: If only monitoring files but we're in rapid command mode, skip it
    if (!hasDirtyState && skipFileMonitoring) {
        return ServiceStatus::IDLE;  // Skip expensive file monitoring during rapid commands
    }
    
    // DIAGNOSTIC: Track what work we're doing if service is slow
    const char* slowReason = nullptr;
    
    // Debug output (only when we're actually doing work)
    static unsigned long lastServiceDebug = 0;
    if (mscModeEnabled && debugUSB && (millis() - lastServiceDebug > 2000)) {
        #if debugFP
        Serial.println("◆ SlotManager::service() - active work detected");
        Serial.flush();
        #endif
        lastServiceDebug = millis();
    }
    
    // Mark that we're about to do filesystem operations (if we're doing file work)
    // This prevents host from mounting mid-operation via driveReady callback
    if (needsFileMonitoring) {
        usbFilesystemBusy = true;
    }
    
    static unsigned long lastFileCheckTime = 0;
    static unsigned long lastFileModTime = 0;
    static int lastEditingSlotNumber = -1;  // Track which slot is being edited
    
    // Get editor state once at the start (used by multiple sections below)
    const char* currentlyEditing = hasEditorOpen ? ekilo_get_currently_editing_file() : nullptr;
    int editingSlotNumber = extractSlotNumberFromFilename(currentlyEditing);
    
    // ============================================================================
    // PREVIEW MODE MANAGEMENT: Only run if editor is open or already in preview mode
    // ============================================================================
    if (hasEditorOpen || inPreviewMode) {
        
        // Handle preview mode state transitions
        if (editingSlotNumber >= 0) {
            // A slot file is being edited
            if (!previewModeActive) {
                // Enter preview mode
                #if debugFP
                Serial.print("Slot ");
                Serial.print(editingSlotNumber);
                Serial.println(" opened in editor - entering preview mode");
                #endif
                
                String errorMsg;
                if (enterPreviewMode(editingSlotNumber, errorMsg)) {
                    lastEditingSlotNumber = editingSlotNumber;
                    lastFileModTime = 0;  // Reset to trigger initial load
                } else {
                    #if debugFP
                    Serial.print("Failed to enter preview mode: ");
                    Serial.println(errorMsg);
                    #endif
                }
            }
        } else {
            // No slot file is being edited
            if (previewModeActive && lastEditingSlotNumber >= 0) {
                // Exit preview mode (user closed editor without applying)
                #if debugFP
                Serial.print("Editor closed - exiting preview mode for slot ");
                Serial.println(lastEditingSlotNumber);
                #endif
                
                String errorMsg;
                exitPreview(false, errorMsg);  // Don't apply changes
                lastEditingSlotNumber = -1;
                lastFileModTime = 0;  // Reset for next edit
            }
        }
    }
    
    // ============================================================================
    // FILE MONITORING: Only check files if editor is open, in preview mode, or USB mode is active
    // This is the most expensive operation, so we skip it when not needed
    // ============================================================================
    unsigned long timeSinceLastFileCheck = millis() - lastFileCheckTime;
    
    // OPTIMIZATION: Only monitor files when we have a REAL reason to:
    // - Editor is open (internal editing via Ekilo)  
    // - Preview mode is active (viewing changes in slot browser)
    // - Host is actually mounted and editing files (USB MSC mounted by PC)
    // This avoids ~60 f_stat() calls per minute during normal UART operation
    // CRITICAL: needsFileMonitoring is already the correct check (usbMountedByHost || hasEditorOpen)
    bool shouldMonitorFiles = (needsFileMonitoring || inPreviewMode) && !skipFileMonitoring;
    
    // Update file monitor time if we're actually checking
    if (shouldMonitorFiles && timeSinceLastFileCheck > 1000) {
        lastFileMonitorTime = millis();
    }
    
    if (mscModeEnabled && debugUSB && shouldMonitorFiles && timeSinceLastFileCheck > 5000) {
        Serial.print("USB: ⚠ File check interval: ");
        Serial.print(timeSinceLastFileCheck);
        Serial.println("ms (should be ~1000ms)");
        Serial.flush();
    }
    
    if (shouldMonitorFiles && timeSinceLastFileCheck > 1000 && false) {
        lastFileCheckTime = millis();
        slowReason = "file monitoring";
        
        if (mscModeEnabled && debugUSB) {
            Serial.println("USB: ▶ File check cycle starting");
            Serial.flush();
        }
        
        // Determine which slot to monitor
        int slotToMonitor = netSlot;
        bool useEditorBuffer = false;
        
        if (mscModeEnabled && debugUSB) {
            Serial.print("USB:   netSlot=");
            Serial.print(netSlot);
            Serial.print(", activeSlotNumber=");
            Serial.print(activeSlotNumber);
            Serial.print(", previewMode=");
            Serial.println(previewModeActive ? "true" : "false");
            Serial.flush();
        }
        
        if (previewModeActive && editingSlotNumber >= 0) {
            // In preview mode - monitor the editor buffer or file
            slotToMonitor = editingSlotNumber;
            useEditorBuffer = (currentlyEditing != nullptr);  // Use buffer if editor is open
        } else if (!activeState.isDirty() && activeSlotNumber >= 0) {
            // Normal mode - monitor active slot file if no unsaved changes
            slotToMonitor = activeSlotNumber;
            useEditorBuffer = false;
        }
        
        if (mscModeEnabled && debugUSB) {
            Serial.print("USB:   Monitoring slot ");
            Serial.print(slotToMonitor);
            Serial.print(", useEditorBuffer=");
            Serial.println(useEditorBuffer ? "true" : "false");
            Serial.flush();
        }
        
        if (slotToMonitor >= 0) {
            static String lastBufferContent = "";  // Track last buffer content
            static unsigned long lastReloadTime = 0;  // Debounce reload operations
            static bool reloadInProgress = false;  // Prevent concurrent reloads
            const unsigned long RELOAD_COOLDOWN = 500; // Wait 500ms between reloads
            bool contentChanged = false;
            String newContent;
            
            // CRITICAL: If a reload is already in progress, skip this cycle
            // This prevents cascading reloads that can cause crashes
            if (reloadInProgress) {
                if (mscModeEnabled && debugUSB && (millis() - lastReloadTime > 5000)) {
                    Serial.println("USB: ⚠ Reload still in progress after 5s - forcing reset");
                    reloadInProgress = false;  // Force reset after timeout
                }
                return ServiceStatus::BUSY;
            }
            
            if (useEditorBuffer) {
                // Get content directly from editor buffer (unsaved changes)
                extern String ekilo_get_current_buffer_content();
                newContent = ekilo_get_current_buffer_content();
                
                // Check if buffer content changed
                if (newContent != lastBufferContent) {
                    contentChanged = true;
                    lastBufferContent = newContent;
                }
            } else {
                // Get file modification time (saved changes)
                if (mscModeEnabled && debugUSB) {
                    Serial.println("USB:   Getting filename for slot...");
                    Serial.flush();
                }
                
                String filename = getSlotFilename(slotToMonitor);
                
                if (mscModeEnabled && debugUSB) {
                    Serial.print("USB:   Filename: ");
                    Serial.println(filename);
                    Serial.println("USB:   Checking f_stat (no sync yet to avoid filesystem strain)...");
                    Serial.flush();
                }
                
                // OPTIMIZATION: Check file mod time WITHOUT syncing first
                // We only sync if we detect a change. This dramatically reduces
                // disk_ioctl(CTRL_SYNC) calls from ~60/minute to only when files change.
                // Excessive SYNC calls cause filesystem crashes after many operations.
                
                // CRITICAL FIX: Service USB before potentially blocking filesystem operation
                // f_stat() can take 10-100ms and block USB servicing
                #ifdef USE_TINYUSB
                extern void tud_task(void);
                tud_task();
                #endif
                
                fatfs::FILINFO fno;
                memset(&fno, 0, sizeof(fno));  // Zero out structure for safety
                
                fatfs::FRESULT stat_result = fatfs::f_stat(filename.c_str(), &fno);
                
                // CRITICAL FIX: Service USB after filesystem operation
                #ifdef USE_TINYUSB
                tud_task();
                #endif
                
                if (mscModeEnabled && debugUSB) {
                    Serial.print("USB:   f_stat returned: ");
                    Serial.println(stat_result);
                    Serial.flush();
                }
                
                if (stat_result == fatfs::FR_OK) {
                    unsigned long fileModTime = ((unsigned long)fno.fdate << 16) | fno.ftime;
                    
                    if (lastFileModTime == 0) {
                        // First time - just record the time
                        lastFileModTime = fileModTime;
                        if (mscModeEnabled && debugUSB) {
                            Serial.print("USB: Initial mod time for ");
                            Serial.print(filename);
                            Serial.print(" = ");
                            Serial.println(fileModTime, HEX);
                        }
                    } else if (fileModTime != lastFileModTime) {
                        // Potential change detected! Try to sync and re-check to confirm
                        // BUT: disk_ioctl(CTRL_SYNC) can crash after many operations in USB mode
                        // So we make it optional - if it fails, we still proceed with the reload
                        
                        if (mscModeEnabled && debugUSB) {
                            Serial.println("USB: Potential file change detected - attempting sync...");
                            Serial.flush();
                        }
                        
                        delay(150);  // Let host finish writing
                        
                        // CRITICAL FIX: Service USB before blocking sync operation
                        #ifdef USE_TINYUSB
                        tud_task();
                        #endif
                        
                        // Track sync failures to avoid repeated crashes
                        static int consecutiveSyncFailures = 0;
                        const int MAX_SYNC_FAILURES = 3;
                        bool syncSucceeded = false;
                        
                        // Only attempt sync if we haven't had too many recent failures
                        if (consecutiveSyncFailures < MAX_SYNC_FAILURES) {
                            // CRITICAL: Pause Core2 during disk_ioctl(CTRL_SYNC)
                            bool was_paused_sync = pauseCore2ForFlash(100);
                            
                            // Attempt sync with basic error detection
                            // Note: disk_ioctl may crash instead of returning error, but we try
                            fatfs::DRESULT sync_result = fatfs::disk_ioctl(0, CTRL_SYNC, nullptr);
                            
                            // Restore Core2 state
                            unpauseCore2ForFlash(was_paused_sync);
                            
                            if (sync_result == fatfs::RES_OK) {
                                __sync_synchronize();  // Memory barrier
                                syncSucceeded = true;
                                consecutiveSyncFailures = 0;  // Reset on success
                                
                                if (mscModeEnabled && debugUSB) {
                                    Serial.println("USB: Sync succeeded");
                                    Serial.flush();
                                }
                            } else {
                                consecutiveSyncFailures++;
                                if (mscModeEnabled && debugUSB) {
                                    Serial.print("USB: ⚠ Sync failed with code ");
                                    Serial.print(sync_result);
                                    Serial.print(" (failure #");
                                    Serial.print(consecutiveSyncFailures);
                                    Serial.println(")");
                                    Serial.flush();
                                }
                            }
                        } else {
                            if (mscModeEnabled && debugUSB) {
                                Serial.println("USB: ⚠ Skipping sync (too many recent failures)");
                                Serial.flush();
                            }
                        }
                        
                        // Re-check file mod time (even if sync failed)
                        memset(&fno, 0, sizeof(fno));
                        stat_result = fatfs::f_stat(filename.c_str(), &fno);
                        
                        if (stat_result == fatfs::FR_OK) {
                            unsigned long confirmedModTime = ((unsigned long)fno.fdate << 16) | fno.ftime;
                            
                            if (confirmedModTime != lastFileModTime) {
                                // Change confirmed! Proceed with reload even if sync failed
                                contentChanged = true;
                                if (mscModeEnabled && debugUSB) {
                                    Serial.print("USB: File change confirmed");
                                    if (!syncSucceeded) {
                                        Serial.print(" (without sync)");
                                    }
                                    Serial.print("! ");
                                    Serial.print(filename);
                                    Serial.print(" old=");
                                    Serial.print(lastFileModTime, HEX);
                                    Serial.print(" new=");
                                    Serial.println(confirmedModTime, HEX);
                                }
                                lastFileModTime = confirmedModTime;
                                lastBufferContent = "";  // Clear buffer cache
                            } else {
                                // False alarm - mod time returned to original
                                if (mscModeEnabled && debugUSB) {
                                    Serial.println("USB: False alarm - file unchanged");
                                }
                            }
                        } else {
                            if (mscModeEnabled && debugUSB) {
                                Serial.print("USB: Re-check f_stat failed: ");
                                Serial.println(stat_result);
                            }
                        }
                    }
                    // If fileModTime == lastFileModTime, no change, no sync needed
                } else {
                    // File stat failed - might be mid-write by host
                    if (mscModeEnabled && debugUSB) {
                        Serial.print("USB: f_stat failed with error ");
                        Serial.print(stat_result);
                        Serial.println(" - file might be locked by host, will retry");
                    }
                    // Don't mark as changed, just skip this check cycle
                    contentChanged = false;
                }
            }
            
            // Reload if content changed AND we're past the cooldown period
            if (contentChanged && (millis() - lastReloadTime > RELOAD_COOLDOWN)) {
                // Mark reload as in progress to prevent concurrent operations
                reloadInProgress = true;
                lastReloadTime = millis();
                String errorMsg;
                
                if (mscModeEnabled && debugUSB) {
                    Serial.println("USB: Starting file reload...");
                }
                
                if (useEditorBuffer && newContent.length() > 0) {
                    // Parse YAML directly from editor buffer
                    if (activeState.fromYAML(newContent, errorMsg)) {
                        // Compute nets and refresh hardware
                        extern void refreshConnections(int ledShowOption, int fillUnused, int clean);
                        refreshConnections(-1, 1, 0);
                        lastStatus = ServiceStatus::BUSY;
                        
                        if (mscModeEnabled && debugUSB) {
                            Serial.println("USB: ✓ Buffer reload complete");
                        }
                    } else {
                        // Parse error - don't spam, just skip this update
                        if (mscModeEnabled && debugUSB) {
                            Serial.print("USB: ✗ Parse error: ");
                            Serial.println(errorMsg);
                        }
                        lastStatus = ServiceStatus::IDLE;
                    }
                } else {
                    // Load from disk (normal file change or save)
                    // Add small delay to ensure host has finished writing
                    if (mscModeEnabled && debugUSB) {
                        Serial.println("USB: About to reload file - delaying 100ms for host write completion");
                        Serial.flush();
                        delay(100);  // Give host time to finish write
                        // NOTE: We already called disk_ioctl(CTRL_SYNC) before f_stat above,
                        // so we don't need to sync again here. Calling it twice in quick succession
                        // causes crashes during the second reload.
                        Serial.println("USB: Calling loadSlot()...");
                        Serial.flush();
                    }
                    
                    // Defensive: Check slot number is valid before attempting load
                    if (slotToMonitor < 0 || slotToMonitor >= NUM_SLOTS) {
                        if (mscModeEnabled && debugUSB) {
                            Serial.print("USB: ✗ Invalid slot number: ");
                            Serial.println(slotToMonitor);
                        }
                        lastStatus = ServiceStatus::ERROR;
                    } else if (loadSlot(slotToMonitor, errorMsg)) {
                        if (!previewModeActive) {
                            Serial.print("✓ Slot ");
                            Serial.print(slotToMonitor);
                            Serial.println(" reloaded from disk");
                        }
                        lastStatus = ServiceStatus::BUSY;
                        
                        if (mscModeEnabled && debugUSB) {
                            Serial.println("USB: ✓ File reload complete");
                            Serial.flush();
                        }
                    } else {
                        if (!previewModeActive || mscModeEnabled) {
                            Serial.print("✗ Failed to reload slot: ");
                            Serial.println(errorMsg);
                            Serial.flush();
                        }
                        lastStatus = ServiceStatus::ERROR;
                        
                        // Reset file mod time on error to force retry next cycle
                        lastFileModTime = 0;
                    }
                }
                
                // Clear reload in progress flag
                reloadInProgress = false;
                
            } else if (contentChanged && (millis() - lastReloadTime <= RELOAD_COOLDOWN)) {
                // Change detected but we're in cooldown period
                if (mscModeEnabled && debugUSB) {
                    Serial.println("USB: Change detected but in cooldown period - will reload next cycle");
                }
            }
        }
    }
    
    // ============================================================================
    // AUTO-SAVE: Only runs if state is dirty and USB is not mounted
    // This section was already optimized with the early exit check above
    // ============================================================================
    
    // CRITICAL: Don't auto-save while command processing is active (prevents deadlock)
    // Both refreshLocalConnections() and saveSlot() need core synchronization
    extern volatile bool refreshLocalInProgress;
    extern volatile bool core1busy;
    
    if (hasDirtyState && !usbMountedByHost && !refreshLocalInProgress && !core1busy) {
        unsigned long timeSinceModified = millis() - activeState.getLastModifiedTime();
        if (timeSinceModified > 2000) {  // 5 second delay for rapid command bursts (Arduino uploads, etc)
            slowReason = "auto-save";
            unsigned long saveStart = micros();
            
            if (debugWaitLoopTiming) {
                Serial.printf("DEBUG: Auto-save triggered (dirty for %lu ms)\n", timeSinceModified);
            }
            
            String errorMsg;
            
            // Ensure we're using the current slot (sync with netSlot)
            syncFromGlobalNetSlot();
            
            // saveSlot will handle core synchronization and clearDirty
            // Skip validation on auto-save (state is validated when connections are added/removed)
            if (saveSlot(activeSlotNumber, errorMsg, true)) {
                unsigned long saveTime = micros() - saveStart;
                if (debugWaitLoopTiming) {
                if (saveTime > 100000) {
                    Serial.printf("⏱️  saveSlot took %lu ms\n", saveTime / 1000);
                }
                }
                // Successfully auto-saved
                lastStatus = ServiceStatus::BUSY;
                if (debugWaitLoopTiming) {
                    Serial.printf("DEBUG: Auto-save completed\n");
                }
                
                // Update file mod time after save
                String filename = getSlotFilename(activeSlotNumber);
                fatfs::FILINFO fno;
                if (fatfs::f_stat(filename.c_str(), &fno) == fatfs::FR_OK) {
                    lastFileModTime = ((unsigned long)fno.fdate << 16) | fno.ftime;
                }
            } else {
                // Don't clear dirty on failure - retry on next loop
                Serial.print("✗ Auto-save failed (slot ");
                Serial.print(activeSlotNumber);
                Serial.print("): ");
                Serial.println(errorMsg);
                lastStatus = ServiceStatus::ERROR;
            }
        }
    } else if (hasDirtyState && usbMountedByHost) {
        // Dirty state exists but can't save while USB is mounted
        // This is normal - we'll save when USB is unmounted
        if (debugWaitLoopTiming) {
            static unsigned long lastUsbBlockMsg = 0;
            if (millis() - lastUsbBlockMsg > 5000) {  // Print every 5 seconds
                Serial.println("DEBUG: Auto-save blocked - USB mounted by host");
                lastUsbBlockMsg = millis();
            }
        }
    }
    
    // Clear busy flag to allow host mounting if requested (only if we set it)
    if (needsFileMonitoring) {
        usbFilesystemBusy = false;
    }
    
    if (mscModeEnabled && debugUSB) {
        static unsigned long lastServiceCompleteDebug = 0;
        if (millis() - lastServiceCompleteDebug > 5000) {
            Serial.println("◆ SlotManager::service() completed normally");
            Serial.flush();
            lastServiceCompleteDebug = millis();
        }
    }
    
    // DIAGNOSTIC: Report if service took too long
    unsigned long serviceTime = micros() - serviceStart;
    if (debugWaitLoopTiming) {
    if (serviceTime > 100000) {  // > 100ms
        Serial.printf("⏱️  States::service() took %lu ms", serviceTime / 1000);
        if (slowReason) {
            Serial.printf(" [%s]", slowReason);
        }
        Serial.println();
        Serial.printf("    hasDirty=%d hasEditor=%d preview=%d needsFileMon=%d skipFile=%d\n",
            hasDirtyState, hasEditorOpen, inPreviewMode, needsFileMonitoring, skipFileMonitoring);
        Serial.flush();
    }
    }
    return lastStatus;
}

