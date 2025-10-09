// SPDX-License-Identifier: MIT
#include "States.h"
#include "MatrixState.h"
#include "NetManager.h"
#include "FileParsing.h"
#include "Commands.h"
#include "config.h"
#include <string.h>
#include <FatFS.h>

extern struct config jumperlessConfig;
extern volatile bool core1busy;
extern volatile bool core2busy;
extern int netSlot;  // Global slot number (defined in RotaryEncoder.cpp)

// Global singleton - THE single source of truth for all Jumperless state
JumperlessState globalState;

// ============================================================================
// ConnectionState Implementation
// ============================================================================

ConnectionState::ConnectionState() {
    clear();
}

void ConnectionState::clear() {
    memset(bridges, 0, sizeof(bridges));
    numBridges = 0;
    numNets = 0;
    numPaths = 0;
    pathsCacheValid = false;
    chipStatesCacheValid = false;
    clearAllNTCC();
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
}

void DisplayState::setNetColor(int netNum, rgbColor color, uint32_t raw, const char* name) {
    // Check if already exists
    for (int i = 0; i < numCustomColors; i++) {
        if (customColors[i].netNumber == netNum) {
            customColors[i].color = color;
            customColors[i].rawColor = raw;
            strncpy(customColors[i].colorName, name, 31);
            customColors[i].colorName[31] = '\0';
            return;
        }
    }
    
    // Add new if space available
    if (numCustomColors < MAX_NETS) {
        customColors[numCustomColors].netNumber = netNum;
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
// JumperlessState Implementation
// ============================================================================

JumperlessState::JumperlessState() {
    version = 2;  // Current format version (2 = YAML format)
    dirty = false;
    lastModifiedTime = 0;
    clear();
}

void JumperlessState::clear() {
    connections.clear();
    power.setDefaults();
    display.clear();
    config.setDefaults();
    dirty = false;
    lastModifiedTime = 0;
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
    
    // Check for duplicate - if it exists, increment the duplicate count instead
    for (int i = 0; i < connections.numBridges; i++) {
        if ((connections.bridges[i][0] == node1 && connections.bridges[i][1] == node2) ||
            (connections.bridges[i][0] == node2 && connections.bridges[i][1] == node1)) {
            // Connection exists - increment duplicates
            connections.bridges[i][2]++;
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
    }
    connections.numBridges--;
    
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
    
    // Header
    output += "version: " + String(version) + "\n\r";
    output += "sourceOfTruth: " + String(config.sourceOfTruth == BRIDGES_PRIMARY ? "bridges" : "nets") + "\n\r\n\r";
    
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
        // Parse section content
        else if (line.startsWith("- {") || line.startsWith("-{")) {
            // Bridge or net entry
            if (currentSection == "bridges") {
                if (!deserializeBridges(line.c_str(), errorMsg)) {
            return false;
        }
            } else if (currentSection == "nets") {
                if (!deserializeNets(line.c_str(), errorMsg)) {
                    return false;
                }
            }
        }
        else if (currentSection == "power") {
            if (!deserializePower(line.c_str(), errorMsg)) {
            return false;
        }
    }
        else if (currentSection == "config") {
            if (!deserializeConfig(line.c_str(), errorMsg)) {
            return false;
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
    if (connections.numBridges == 0) {
        return;  // Don't output empty bridges section
    }
    
    output += "bridges:\n\r";
    for (int i = 0; i < connections.numBridges; i++) {
        output += "  - {n1: " + String(connections.bridges[i][0]) + 
                  ", n2: " + String(connections.bridges[i][1]) + 
                  ", dup: " + String(connections.bridges[i][2]) + "}\n\r";
    }
    output += "\n\r";
}

bool JumperlessState::deserializeBridges(const char* yamlContent, String& errorMsg) {
    // Parse bridge entry: - {n1: 1, n2: 5, dup: 2}
    String line = String(yamlContent);
    line.trim();
    
    // Extract values
    int n1 = -1, n2 = -1, dup = -1;
    
    int n1Idx = line.indexOf("n1:");
    if (n1Idx >= 0) {
        int commaIdx = line.indexOf(',', n1Idx);
        String val = line.substring(n1Idx + 3, commaIdx);
        val.trim();
        n1 = val.toInt();
    }
    
    int n2Idx = line.indexOf("n2:");
    if (n2Idx >= 0) {
        int commaIdx = line.indexOf(',', n2Idx);
        if (commaIdx == -1) commaIdx = line.indexOf('}', n2Idx);
        String val = line.substring(n2Idx + 3, commaIdx);
        val.trim();
        n2 = val.toInt();
    }
    
    int dupIdx = line.indexOf("dup:");
    if (dupIdx >= 0) {
        int endIdx = line.indexOf('}', dupIdx);
        String val = line.substring(dupIdx + 4, endIdx);
        val.trim();
        dup = val.toInt();
    }
    
    if (n1 < 0 || n2 < 0) {
        errorMsg = "Invalid bridge format: " + line;
        return false;
    }
    
    return addConnection(n1, n2, errorMsg, dup);
}

void JumperlessState::serializeNets(String& output) const {
    // Only serialize nets with custom colors or names
    bool hasCustomNets = false;
    for (int i = 0; i < display.numCustomColors; i++) {
        hasCustomNets = true;
        break;
    }
    
    if (!hasCustomNets) {
        return;  // Don't output empty nets section
    }
    
    output += "nets:\n\r";
    for (int i = 0; i < display.numCustomColors; i++) {
        const DisplayState::NetColorEntry& entry = display.customColors[i];
        output += "  - {num: " + String(entry.netNumber) + 
                  ", name: \"" + String(entry.colorName) + "\"" +
                  ", color: 0x" + String(entry.rawColor, HEX) + "}\n\r";
    }
    output += "\n\r";
}

bool JumperlessState::deserializeNets(const char* yamlContent, String& errorMsg) {
    // Parse net entry: - {num: 6, name: "Signal A", color: 0xFF0000}
    String line = String(yamlContent);
    line.trim();
    
    int netNum = -1;
    String netName = "";
    uint32_t rawColor = 0;
    
    int numIdx = line.indexOf("num:");
    if (numIdx >= 0) {
        int commaIdx = line.indexOf(',', numIdx);
        String val = line.substring(numIdx + 4, commaIdx);
        val.trim();
        netNum = val.toInt();
    }
    
    int nameIdx = line.indexOf("name:");
    if (nameIdx >= 0) {
        int startQuote = line.indexOf('"', nameIdx);
        int endQuote = line.indexOf('"', startQuote + 1);
        if (startQuote >= 0 && endQuote > startQuote) {
            netName = line.substring(startQuote + 1, endQuote);
        }
    }
    
    int colorIdx = line.indexOf("color:");
    if (colorIdx >= 0) {
        int endIdx = line.indexOf('}', colorIdx);
        if (endIdx == -1) endIdx = line.length();
        String val = line.substring(colorIdx + 6, endIdx);
        val.trim();
        if (val.startsWith("0x") || val.startsWith("0X")) {
            rawColor = strtoul(val.c_str() + 2, NULL, 16);
        }
    }
    
    if (netNum >= 0) {
            rgbColor color;
        color.r = (rawColor >> 16) & 0xFF;
        color.g = (rawColor >> 8) & 0xFF;
        color.b = rawColor & 0xFF;
        display.setNetColor(netNum, color, rawColor, netName.c_str());
    }
    
    return true;
}

void JumperlessState::serializePower(String& output) const {
    output += "power:\n\r";
    output += "  topRail: " + String(power.topRail, 2) + "\n\r";
    output += "  bottomRail: " + String(power.bottomRail, 2) + "\n\r";
    output += "  dac0: " + String(power.dac0, 2) + "\n\r";
    output += "  dac1: " + String(power.dac1, 2) + "\n\r\n\r";
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
    output += "config:\n\r";
    output += "  routing: {stackPaths: " + String(config.stackPaths) + 
              ", stackRails: " + String(config.stackRails) + 
              ", stackDacs: " + String(config.stackDacs) + 
              ", railPriority: " + String(config.railPriority) + "}\n\r";
    
    // GPIO direction array
    output += "  gpio:\n\r";
    output += "    direction: [";
    for (int i = 0; i < 10; i++) {
        output += String(config.gpioDirection[i]);
        if (i < 9) output += ",";
    }
    output += "]\n\r";
    
    // GPIO pulls array
    output += "    pulls: [";
    for (int i = 0; i < 10; i++) {
        output += String(config.gpioPulls[i]);
        if (i < 9) output += ",";
    }
    output += "]\n\r";
    
    // UART and OLED
    output += "  uart: {txFunction: " + String(config.uartTxFunction) + 
              ", rxFunction: " + String(config.uartRxFunction) + "}\n\r";
    output += "  oled: {connected: " + String(config.oledConnected ? "true" : "false") + 
              ", lockConnection: " + String(config.oledLockConnection ? "true" : "false") + "}\n\r";
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
            config.oledConnected = (val == "true");
        }
        
        int lockIdx = line.indexOf("lockConnection:");
        if (lockIdx >= 0) {
            int endIdx = line.indexOf('}', lockIdx);
            String val = line.substring(lockIdx + 15, endIdx);
            val.trim();
            config.oledLockConnection = (val == "true");
        }
    }
    
    return true;
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
// SlotManager Implementation
// ============================================================================

SlotManager::SlotManager() 
    : activeState(globalState), activeSlotNumber(0), historySize(STATE_HISTORY_SIZE), 
      historyHead(0), historyCount(0), historyPosition(0) {
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
    return "/slots/slot" + String(slotNum) + ".yaml";
}

String SlotManager::getLegacySlotFilename(int slotNum) const {
    return "/nodeFileSlot" + String(slotNum) + ".txt";
}

String SlotManager::getJSONSlotFilename(int slotNum) const {
    return "/slots/slot" + String(slotNum) + ".json";
}

bool SlotManager::slotExists(int slotNum) const {
    if (slotNum < 0 || slotNum >= NUM_SLOTS) {
        return false;
    }
    
    String filename = getSlotFilename(slotNum);
    if (FatFS.exists(filename.c_str())) {
        return true;
    }
    
    // Check for legacy format
    String legacyFilename = getLegacySlotFilename(slotNum);
    return FatFS.exists(legacyFilename.c_str());
}

bool SlotManager::loadSlot(int slotNum, String& errorMsg) {
    if (slotNum < 0 || slotNum >= NUM_SLOTS) {
        errorMsg = "Invalid slot number: " + String(slotNum);
        return false;
    }
    
    String content;
    String filename = getSlotFilename(slotNum);

    Serial.println("Loading slot " + String(slotNum) + " from " + filename);
    Serial.flush();
    
    // Try loading YAML slot file
    if (FatFS.exists(filename.c_str())) {
        Serial.println("  File exists, reading...");
        Serial.flush();
        
        if (!readSlotFile(slotNum, content, errorMsg)) {
            Serial.println("  Read failed: " + errorMsg);
            Serial.flush();
            return false;
        }
        
        Serial.println("  Parsing YAML...");
        Serial.flush();
        
        if (!activeState.fromYAML(content, errorMsg)) {
            errorMsg = "Failed to parse YAML slot " + String(slotNum) + ": " + errorMsg;
            Serial.println("  Parse failed: " + errorMsg);
            Serial.flush();
            return false;
        }
        
        Serial.println("  ✓ Loaded " + String(activeState.connections.numBridges) + " connections");
        Serial.flush();
        
        activeSlotNumber = slotNum;
        netSlot = slotNum;  // Sync global slot tracker
        return true;
    }
    
    // Slot file doesn't exist - start with empty state
    Serial.println("  File doesn't exist, creating empty slot");
    Serial.flush();
    
    activeState.clear();
    activeSlotNumber = slotNum;
    netSlot = slotNum;  // Sync global slot tracker
    
    // Create empty YAML file for this slot
    if (!ensureSlotExists(slotNum)) {
        // Failed to create file, but we can still continue with RAM-only state
        errorMsg = "Warning: Could not create slot file, using RAM only";
    }
    
    return true;
}

bool SlotManager::saveSlot(int slotNum, String& errorMsg) {
    if (slotNum < 0 || slotNum >= NUM_SLOTS) {
        errorMsg = "Invalid slot number: " + String(slotNum);
        return false;
    }
    
    // Serial.println("Saving slot " + String(slotNum) + " with " + String(activeState.connections.numBridges) + " connections");
    // Serial.flush();
    
    // Ensure /slots directory exists (create on-demand)
    if (!FatFS.exists("/slots")) {
        Serial.println("  Creating /slots directory");
        Serial.flush();
        if (!FatFS.mkdir("/slots")) {
            errorMsg = "Failed to create /slots directory";
            return false;
        }
    }
    
    // Validate state before saving
    if (!activeState.validate(errorMsg)) {
        errorMsg = "Cannot save invalid state: " + errorMsg;
        Serial.println("  Validation failed: " + errorMsg);
        Serial.flush();
        return false;
    }
    
    // Convert to YAML
    String yamlContent;
    if (!activeState.toYAML(yamlContent)) {
        errorMsg = "Failed to serialize state to YAML";
        Serial.println("  Serialization failed");
        Serial.flush();
        return false;
    }
    
    // Serial.println("  Writing to file...");
    // Serial.flush();
    
    // Write to file (this will create the file if it doesn't exist)
    if (!writeSlotFile(slotNum, yamlContent, errorMsg)) {
        Serial.println("  Write failed: " + errorMsg);
        Serial.flush();
        return false;
    }
    
    // Serial.println("  ✓ Saved successfully");
    // Serial.flush();
    
    activeSlotNumber = slotNum;
    netSlot = slotNum;  // Sync global slot tracker
    activeState.clearDirty();  // Mark as saved
    return true;
}

bool SlotManager::saveActiveSlot(String& errorMsg) {
    if (activeSlotNumber < 0) {
        errorMsg = "No active slot to save";
        return false;
    }
    return saveSlot(activeSlotNumber, errorMsg);
}

bool SlotManager::deleteSlot(int slotNum, String& errorMsg) {
    if (slotNum < 0 || slotNum >= NUM_SLOTS) {
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
    
    // Also delete legacy format if it exists
    String legacyFilename = getLegacySlotFilename(slotNum);
    if (FatFS.exists(legacyFilename.c_str())) {
        FatFS.remove(legacyFilename.c_str());
    }
    
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

bool SlotManager::ensureSlotExists(int slotNum) {
    if (slotExists(slotNum)) {
        return true;
    }
    
    // Create empty slot
    JumperlessState emptyState;
    String errorMsg;
    
    String yamlContent;
    if (!emptyState.toYAML(yamlContent)) {
        return false;
    }
    
    return writeSlotFile(slotNum, yamlContent, errorMsg);
}

// File I/O helpers
bool SlotManager::readSlotFile(int slotNum, String& content, String& errorMsg) {
    String filename = getSlotFilename(slotNum);
    
    // Detect which core we're running on and synchronize appropriately
    uint coreNum = get_core_num();
    
    // Add timeout to prevent deadlock during boot or race conditions
    unsigned long timeout = millis() + 5000;  // 5 second timeout
    
    if (coreNum == 0) {
        // Running on core0 (core1 in RP2040 terms) - wait for core2, set core1busy
        while (core2busy) {
            if (millis() > timeout) {
                errorMsg = "Timeout waiting for core2 (possible deadlock)";
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
                return false;
            }
            delay(1);
        }
        core2busy = true;
    }
    
    File file = FatFS.open(filename.c_str(), "r");
    if (!file) {
        errorMsg = "Failed to open slot file: " + filename;
        if (coreNum == 0) {
            core1busy = false;
        } else {
            core2busy = false;
        }
        return false;
    }
    
    content = "";
    while (file.available()) {
        content += (char)file.read();
    }
    
    file.close();
    
    if (coreNum == 0) {
        core1busy = false;
    } else {
        core2busy = false;
    }
    
    return true;
}

bool SlotManager::writeSlotFile(int slotNum, const String& content, String& errorMsg) {
    // Ensure slots directory exists
    if (!FatFS.exists("/slots")) {
        if (!FatFS.mkdir("/slots")) {
            errorMsg = "Failed to create /slots directory";
            return false;
        }
    }
    
    String filename = getSlotFilename(slotNum);
    
    // Detect which core we're running on and synchronize appropriately
    uint coreNum = get_core_num();
    
    // Add timeout to prevent deadlock during boot or race conditions
    unsigned long timeout = millis() + 5000;  // 5 second timeout
    
    if (coreNum == 0) {
        // Running on core0 (core1 in RP2040 terms) - wait for core2, set core1busy
        while (core2busy) {
            if (millis() > timeout) {
                errorMsg = "Timeout waiting for core2 (possible deadlock)";
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
                return false;
            }
            delay(1);
        }
        core2busy = true;
    }
    
    File file = FatFS.open(filename.c_str(), "w");
    if (!file) {
        errorMsg = "Failed to open slot file for writing: " + filename;
        if (coreNum == 0) {
            core1busy = false;
        } else {
            core2busy = false;
        }
        return false;
    }
    
    file.write((const uint8_t*)content.c_str(), content.length());
    file.close();
    
    if (coreNum == 0) {
        core1busy = false;
    } else {
        core2busy = false;
    }
    
    return true;
}

bool SlotManager::migrateOldSlotFile(int slotNum, String& errorMsg) {
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
        return false;
    }
    
    String content = "";
    while (file.available()) {
        content += (char)file.read();
    }
    file.close();
    
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
    
    // Load legacy config from global config (voltages, etc.)
    newState.setDacVoltage(0, jumperlessConfig.dacs.dac_0);
    newState.setDacVoltage(1, jumperlessConfig.dacs.dac_1);
    newState.setRailVoltage(true, jumperlessConfig.dacs.top_rail);
    newState.setRailVoltage(false, jumperlessConfig.dacs.bottom_rail);
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

