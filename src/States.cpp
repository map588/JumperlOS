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

// ============================================================================
// ConnectionState Implementation
// ============================================================================

ConnectionState::ConnectionState() {
    clear();
}

void ConnectionState::clear() {
    memset(bridges, 0, sizeof(bridges));
    memset(bridgeDuplicates, 0, sizeof(bridgeDuplicates));
    numBridges = 0;
    numNets = 0;
    numPaths = 0;
    pathsCacheValid = false;
    chipStatesCacheValid = false;
    
    // Clear chip XY states
    for (int i = 0; i < 12; i++) {
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
    version = 1;  // Current format version
    clear();
}

void JumperlessState::clear() {
    connections.clear();
    power.setDefaults();
    display.clear();
    config.setDefaults();
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
            connections.bridgeDuplicates[i]++;
            connections.invalidateCache(config.autoRefreshOnChange);
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
    connections.bridges[idx][2] = -1;  // Net number assigned later
    connections.bridgeDuplicates[idx] = numDuplicates;
    connections.numBridges++;
    
    // Invalidate caches - paths need to be recalculated
    connections.invalidateCache(config.autoRefreshOnChange);
    
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
}

int JumperlessState::getConnectionDuplicates(int node1, int node2) const {
    for (int i = 0; i < connections.numBridges; i++) {
        if ((connections.bridges[i][0] == node1 && connections.bridges[i][1] == node2) ||
            (connections.bridges[i][0] == node2 && connections.bridges[i][1] == node1)) {
            return connections.bridgeDuplicates[i];
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
            connections.bridgeDuplicates[i] = duplicates;
            connections.invalidateCache(config.autoRefreshOnChange);
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
}

float JumperlessState::getRailVoltage(bool isTopRail) const {
    return isTopRail ? power.topRail : power.bottomRail;
}

// Configuration
void JumperlessState::setPathStacking(int paths, int rails, int dacs) {
    config.stackPaths = paths;
    config.stackRails = rails;
    config.stackDacs = dacs;
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
// JSON Serialization
// ============================================================================

bool JumperlessState::toJSON(String& output, bool pretty) const {
    JsonDocument doc;
    
    doc["version"] = version;
    
    // Serialize in logical order: power → connections → config → display → chipXY
    JsonObject powerObj = doc["power"].to<JsonObject>();
    if (!serializePower(powerObj)) return false;
    
    JsonObject connectionsObj = doc["connections"].to<JsonObject>();
    if (!serializeConnections(connectionsObj)) return false;
    
    JsonObject configObj = doc["config"].to<JsonObject>();
    if (!serializeConfig(configObj)) return false;
    
    // Only include display if there are custom colors
    if (display.hasCustomColors()) {
        JsonObject displayObj = doc["display"].to<JsonObject>();
        if (!serializeDisplay(displayObj)) return false;
    }
    
    // Serialize chip XY states
    JsonObject chipXYObj = doc["chipXY"].to<JsonObject>();
    if (!serializeChipXY(chipXYObj)) return false;
    
    // Convert to string
    if (pretty) {
        serializeJsonPretty(doc, output);
    } else {
        serializeJson(doc, output);
    }
    
    return true;
}

bool JumperlessState::fromJSON(const String& input, String& errorMsg) {
    JsonDocument doc;
    
    DeserializationError error = deserializeJson(doc, input);
    if (error) {
        errorMsg = "JSON parse error: " + String(error.c_str());
        return false;
    }
    
    // Check version
    if (doc["version"].is<int>()) {
        int ver = doc["version"];
        if (ver > version) {
            errorMsg = "File version " + String(ver) + " newer than supported version " + String(version);
            return false;
        }
    }
    
    // Deserialize each section
    if (doc["power"].is<JsonObject>()) {
        JsonObject powerObj = doc["power"].as<JsonObject>();
        if (!deserializePower(powerObj, errorMsg)) {
            return false;
        }
    }
    
    if (doc["config"].is<JsonObject>()) {
        JsonObject configObj = doc["config"].as<JsonObject>();
        if (!deserializeConfig(configObj, errorMsg)) {
            return false;
        }
    }
    
    if (doc["connections"].is<JsonObject>()) {
        JsonObject connectionsObj = doc["connections"].as<JsonObject>();
        if (!deserializeConnections(connectionsObj, errorMsg)) {
            return false;
        }
    }
    
    if (doc["display"].is<JsonObject>()) {
        JsonObject displayObj = doc["display"].as<JsonObject>();
        if (!deserializeDisplay(displayObj, errorMsg)) {
            return false;
        }
    }
    
    if (doc["chipXY"].is<JsonObject>()) {
        JsonObject chipXYObj = doc["chipXY"].as<JsonObject>();
        if (!deserializeChipXY(chipXYObj, errorMsg)) {
            return false;
        }
    }
    
    return validate(errorMsg);
}

// JSON serialization helpers
bool JumperlessState::serializePower(JsonObject& obj) const {
    obj["topRail"] = power.topRail;
    obj["bottomRail"] = power.bottomRail;
    obj["dac0"] = power.dac0;
    obj["dac1"] = power.dac1;
    return true;
}

bool JumperlessState::deserializePower(JsonObject& obj, String& errorMsg) {
    if (obj["topRail"].is<float>()) power.topRail = obj["topRail"];
    if (obj["bottomRail"].is<float>()) power.bottomRail = obj["bottomRail"];
    if (obj["dac0"].is<float>()) power.dac0 = obj["dac0"];
    if (obj["dac1"].is<float>()) power.dac1 = obj["dac1"];
    return power.validate(errorMsg);
}

bool JumperlessState::serializeConfig(JsonObject& obj) const {
    obj["stackPaths"] = config.stackPaths;
    obj["stackRails"] = config.stackRails;
    obj["stackDacs"] = config.stackDacs;
    obj["railPriority"] = config.railPriority;
    
    // Build GPIO arrays as compact single-line strings
    obj["gpioDirection"] = serialized("[" + 
        String(config.gpioDirection[0]) + "," + String(config.gpioDirection[1]) + "," + 
        String(config.gpioDirection[2]) + "," + String(config.gpioDirection[3]) + "," + 
        String(config.gpioDirection[4]) + "," + String(config.gpioDirection[5]) + "," + 
        String(config.gpioDirection[6]) + "," + String(config.gpioDirection[7]) + "," + 
        String(config.gpioDirection[8]) + "," + String(config.gpioDirection[9]) + "]");
    
    obj["gpioPulls"] = serialized("[" + 
        String(config.gpioPulls[0]) + "," + String(config.gpioPulls[1]) + "," + 
        String(config.gpioPulls[2]) + "," + String(config.gpioPulls[3]) + "," + 
        String(config.gpioPulls[4]) + "," + String(config.gpioPulls[5]) + "," + 
        String(config.gpioPulls[6]) + "," + String(config.gpioPulls[7]) + "," + 
        String(config.gpioPulls[8]) + "," + String(config.gpioPulls[9]) + "]");
    
    obj["uartTxFunction"] = config.uartTxFunction;
    obj["uartRxFunction"] = config.uartRxFunction;
    obj["oledConnected"] = config.oledConnected;
    obj["oledLockConnection"] = config.oledLockConnection;
    obj["autoRefresh"] = config.autoRefreshOnChange;
    
    return true;
}

bool JumperlessState::deserializeConfig(JsonObject& obj, String& errorMsg) {
    if (obj["stackPaths"].is<int>()) config.stackPaths = obj["stackPaths"];
    if (obj["stackRails"].is<int>()) config.stackRails = obj["stackRails"];
    if (obj["stackDacs"].is<int>()) config.stackDacs = obj["stackDacs"];
    if (obj["railPriority"].is<int>()) config.railPriority = obj["railPriority"];
    
    if (obj["gpioDirection"].is<JsonArray>()) {
        JsonArray arr = obj["gpioDirection"];
        for (int i = 0; i < 10 && i < (int)arr.size(); i++) {
            config.gpioDirection[i] = arr[i];
        }
    }
    
    if (obj["gpioPulls"].is<JsonArray>()) {
        JsonArray arr = obj["gpioPulls"];
        for (int i = 0; i < 10 && i < (int)arr.size(); i++) {
            config.gpioPulls[i] = arr[i];
        }
    }
    
    if (obj["uartTxFunction"].is<int>()) config.uartTxFunction = obj["uartTxFunction"];
    if (obj["uartRxFunction"].is<int>()) config.uartRxFunction = obj["uartRxFunction"];
    if (obj["oledConnected"].is<bool>()) config.oledConnected = obj["oledConnected"];
    if (obj["oledLockConnection"].is<bool>()) config.oledLockConnection = obj["oledLockConnection"];
    if (obj["autoRefresh"].is<bool>()) config.autoRefreshOnChange = obj["autoRefresh"];
    
    return true;
}

bool JumperlessState::serializeConnections(JsonObject& obj) const {
    obj["numBridges"] = connections.numBridges;
    
    JsonArray bridgesArray = obj["bridges"].to<JsonArray>();
    for (int i = 0; i < connections.numBridges; i++) {
        JsonObject bridgeObj = bridgesArray.add<JsonObject>();
        bridgeObj["n1"] = connections.bridges[i][0];
        bridgeObj["n2"] = connections.bridges[i][1];
        bridgeObj["dup"] = connections.bridgeDuplicates[i];
    }
    
    return true;
}

bool JumperlessState::deserializeConnections(JsonObject& obj, String& errorMsg) {
    connections.clear();
    
    if (!obj["bridges"].is<JsonArray>()) {
        errorMsg = "Missing 'bridges' array in connections";
        return false;
    }
    
    JsonArray bridgesArray = obj["bridges"];
    for (JsonVariant v : bridgesArray) {
        // Support both old format (string "1-5") and new format (object {"n1":1,"n2":5,"dup":2})
        if (v.is<String>()) {
            // Legacy format
            String bridge = v.as<String>();
            int dashIdx = bridge.indexOf('-');
            if (dashIdx == -1) {
                errorMsg = "Invalid bridge format: " + bridge;
                return false;
            }
            
            int node1 = bridge.substring(0, dashIdx).toInt();
            int node2 = bridge.substring(dashIdx + 1).toInt();
            
            if (!addConnection(node1, node2, errorMsg, -1)) {  // Use default duplicates
                return false;
            }
        } else if (v.is<JsonObject>()) {
            // New format
            JsonObject bridgeObj = v.as<JsonObject>();
            int node1 = bridgeObj["n1"];
            int node2 = bridgeObj["n2"];
            int duplicates = bridgeObj["dup"] | -1;  // Default to -1 if not present
            
            if (!addConnection(node1, node2, errorMsg, duplicates)) {
                return false;
            }
        } else {
            errorMsg = "Invalid bridge entry format";
            return false;
        }
    }
    
    return true;
}

bool JumperlessState::serializeDisplay(JsonObject& obj) const {
    if (!display.hasCustomColors()) {
        return true;  // Nothing to serialize
    }
    
    JsonArray colorsArray = obj["customColors"].to<JsonArray>();
    for (int i = 0; i < display.numCustomColors; i++) {
        JsonObject colorObj = colorsArray.add<JsonObject>();
        colorObj["net"] = display.customColors[i].netNumber;
        colorObj["r"] = display.customColors[i].color.r;
        colorObj["g"] = display.customColors[i].color.g;
        colorObj["b"] = display.customColors[i].color.b;
        colorObj["raw"] = display.customColors[i].rawColor;
        colorObj["name"] = display.customColors[i].colorName;
    }
    
    return true;
}

bool JumperlessState::deserializeDisplay(JsonObject& obj, String& errorMsg) {
    display.clear();
    
    if (obj["customColors"].is<JsonArray>()) {
        JsonArray colorsArray = obj["customColors"];
        for (JsonVariant v : colorsArray) {
            JsonObject colorObj = v.as<JsonObject>();
            int netNum = colorObj["net"];
            rgbColor color;
            color.r = colorObj["r"];
            color.g = colorObj["g"];
            color.b = colorObj["b"];
            uint32_t raw = colorObj["raw"];
            const char* name = colorObj["name"];
            
            display.setNetColor(netNum, color, raw, name);
        }
    }
    
    return true;
}

bool JumperlessState::serializeChipXY(JsonObject& obj) const {
    JsonArray chipsArray = obj["chips"].to<JsonArray>();
    
    for (int chip = 0; chip < 12; chip++) {
        // Check if this chip has any connections
        uint16_t xBits = 0;
        uint8_t yBits = 0;
        
        for (int x = 0; x < 16; x++) {
            for (int y = 0; y < 8; y++) {
                if (connections.chipXY[chip].connected[x][y]) {
                    xBits |= (1 << x);  // Set bit for this X
                    yBits |= (1 << y);  // Set bit for this Y
                }
            }
        }
        
        // Only store chips that have connections
        if (xBits != 0 || yBits != 0) {
            JsonObject chipObj = chipsArray.add<JsonObject>();
            chipObj["c"] = chip;     // Chip number (shortened key)
            chipObj["x"] = xBits;    // X connections as bitfield
            chipObj["y"] = yBits;    // Y connections as bitfield
        }
    }
    
    return true;
}

bool JumperlessState::deserializeChipXY(JsonObject& obj, String& errorMsg) {
    // Clear all chip states
    for (int i = 0; i < 12; i++) {
        memset(&connections.chipXY[i], 0, sizeof(struct justXY));
    }
    
    if (obj["chips"].is<JsonArray>()) {
        JsonArray chipsArray = obj["chips"];
        for (JsonVariant v : chipsArray) {
            JsonObject chipObj = v.as<JsonObject>();
            
            // Support both old format ("chip") and new format ("c")
            int chip = -1;
            if (chipObj["c"].is<int>()) {
                chip = chipObj["c"];
            } else if (chipObj["chip"].is<int>()) {
                chip = chipObj["chip"];
            }
            
            if (chip < 0 || chip >= 12) {
                errorMsg = "Invalid chip number: " + String(chip);
                return false;
            }
            
            // New compact format: bitfields
            if (chipObj["x"].is<int>() && chipObj["y"].is<int>()) {
                uint16_t xBits = chipObj["x"];
                uint8_t yBits = chipObj["y"];
                
                // Reconstruct connections from bitfields
                for (int x = 0; x < 16; x++) {
                    if (xBits & (1 << x)) {
                        for (int y = 0; y < 8; y++) {
                            if (yBits & (1 << y)) {
                                connections.chipXY[chip].connected[x][y] = true;
                            }
                        }
                    }
                }
            }
            // Old verbose format: array of connection objects
            else if (chipObj["connections"].is<JsonArray>()) {
                JsonArray connectionsArray = chipObj["connections"];
                for (JsonVariant c : connectionsArray) {
                    JsonObject connObj = c.as<JsonObject>();
                    int x = connObj["x"];
                    int y = connObj["y"];
                    
                    if (x >= 0 && x < 16 && y >= 0 && y < 8) {
                        connections.chipXY[chip].connected[x][y] = true;
                    }
                }
            }
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
    : activeSlotNumber(-1), historySize(STATE_HISTORY_SIZE), 
      historyHead(0), historyCount(0), historyPosition(0) {
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
    return "/slots/slot" + String(slotNum) + ".json";
}

String SlotManager::getLegacySlotFilename(int slotNum) const {
    return "/nodeFileSlot" + String(slotNum) + ".txt";
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
    
    // Try new format first
    if (FatFS.exists(filename.c_str())) {
        if (!readSlotFile(slotNum, content, errorMsg)) {
            return false;
        }
        
        if (!activeState.fromJSON(content, errorMsg)) {
            errorMsg = "Failed to parse slot " + String(slotNum) + ": " + errorMsg;
            return false;
        }
        
        activeSlotNumber = slotNum;
        return true;
    }
    
    // Try legacy format
    String legacyFilename = getLegacySlotFilename(slotNum);
    if (FatFS.exists(legacyFilename.c_str())) {
        return migrateOldSlotFile(slotNum, errorMsg);
    }
    
    // Slot doesn't exist - create empty slot
    activeState.clear();
    activeSlotNumber = slotNum;
    return true;
}

bool SlotManager::saveSlot(int slotNum, String& errorMsg) {
    if (slotNum < 0 || slotNum >= NUM_SLOTS) {
        errorMsg = "Invalid slot number: " + String(slotNum);
        return false;
    }
    
    // Validate state before saving
    if (!activeState.validate(errorMsg)) {
        errorMsg = "Cannot save invalid state: " + errorMsg;
        return false;
    }
    
    // Convert to JSON
    String jsonContent;
    if (!activeState.toJSON(jsonContent, true)) {  // pretty format
        errorMsg = "Failed to serialize state to JSON";
        return false;
    }
    
    // Write to file
    if (!writeSlotFile(slotNum, jsonContent, errorMsg)) {
        return false;
    }
    
    activeSlotNumber = slotNum;
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
    }
    
    return true;
}

void SlotManager::clearActiveSlot() {
    activeState.clear();
}

bool SlotManager::ensureSlotExists(int slotNum) {
    if (slotExists(slotNum)) {
        return true;
    }
    
    // Create empty slot
    JumperlessState emptyState;
    String errorMsg;
    
    String jsonContent;
    if (!emptyState.toJSON(jsonContent, true)) {
        return false;
    }
    
    return writeSlotFile(slotNum, jsonContent, errorMsg);
}

// File I/O helpers
bool SlotManager::readSlotFile(int slotNum, String& content, String& errorMsg) {
    String filename = getSlotFilename(slotNum);
    
    while (core2busy) {
        delay(1);
    }
    core1busy = true;
    
    File file = FatFS.open(filename.c_str(), "r");
    if (!file) {
        errorMsg = "Failed to open slot file: " + filename;
        core1busy = false;
        return false;
    }
    
    content = "";
    while (file.available()) {
        content += (char)file.read();
    }
    
    file.close();
    core1busy = false;
    
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
    
    while (core2busy) {
        delay(1);
    }
    core1busy = true;
    
    File file = FatFS.open(filename.c_str(), "w");
    if (!file) {
        errorMsg = "Failed to open slot file for writing: " + filename;
        core1busy = false;
        return false;
    }
    
    file.print(content);
    file.close();
    core1busy = false;
    
    return true;
}

bool SlotManager::migrateOldSlotFile(int slotNum, String& errorMsg) {
    String legacyFilename = getLegacySlotFilename(slotNum);
    
    while (core2busy) {
        delay(1);
    }
    core1busy = true;
    
    File file = FatFS.open(legacyFilename.c_str(), "r");
    if (!file) {
        errorMsg = "Failed to open legacy slot file: " + legacyFilename;
        core1busy = false;
        return false;
    }
    
    String content = "";
    while (file.available()) {
        content += (char)file.read();
    }
    file.close();
    core1busy = false;
    
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

