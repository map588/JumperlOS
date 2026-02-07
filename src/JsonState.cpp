/*
 * JsonState.cpp
 * 
 * Implementation of JSON state generation.
 * Reuses logic from listNets to ensure consistency.
 */

 /*
 * JsonStateParser.cpp
 * 
 * Parser for applying JSON state to Jumperless hardware.
 * Parses nets, power, and gpio sections from JSON and applies to hardware.
 */



#include "FileParsing.h"

#include "JsonState.h"
#include "JumperlessDefines.h"
#include "States.h"
#include "Peripherals.h"
#include "NetManager.h"
#include "FakeGpio.h"
#include "TimeDomainMultiplexer.h"
#include "hardware/gpio.h"

// Helper to escape JSON strings
String escapeJson(String s) {
    s.replace("\"", "\\\"");
    s.replace("\n", "\\n");
    s.replace("\r", "\\r");
    return s;
}

// Helper to get pull state name
static const char* getPullName(int pin) {
    uint8_t gpio_pin = pin + 20;
    if (pin == 8) gpio_pin = 0;
    else if (pin == 9) gpio_pin = 1;
    
    bool pullup = gpio_is_pulled_up(gpio_pin);
    bool pulldown = gpio_is_pulled_down(gpio_pin);
    
    if (pullup && pulldown) return "keeper";
    if (pullup) return "up";
    if (pulldown) return "down";
    return "none";
}

// Helper to get reading state name
static const char* getReadingName(int reading) {
    switch (reading) {
        case 0: return "low";
        case 1: return "high";
        case 2: return "float";
        default: return "unknown";
    }
}

String JsonState::getJumperlessStateJSON() {
    String json;
    // Reserve specific memory to avoid frequent reallocations
    // An empty state is ~1KB, a full state could be 10-20KB
    json.reserve(8192); 

    json = "{\n";
    


    bool firstNet = true;
        // --- Power Rails & DACs ---
    json += "  \"power\": {\n";
    json += "    \"top_rail\": " + String(globalState.power.topRail, 3) + ",\n";
    json += "    \"bottom_rail\": " + String(globalState.power.bottomRail, 3) + ",\n";
    json += "    \"dac0\": " + String(globalState.power.dac0, 3) + ",\n";
    json += "    \"dac1\": " + String(globalState.power.dac1, 3) + "\n";
    json += "  },\n";
    // --- Nets ---
    json += "  \"nets\": [\n";
    // Iterate nets similarly to listNets
    // We iterate up to MAX_NETS but stop when we hit an unallocated one
    // Note: numberOfNets is not extern, so we rely on the loop condition used in listNets
    for (int i = 1; i < MAX_NETS; i++) {
        if (globalState.connections.nets[i].number == 0 && globalState.connections.nets[i].nodes[0] <= 0) {
            // End of active nets
            break;
        }

        if (!firstNet) json += ",\n";
        firstNet = false;
        
        json += "    {\n";
        json += "      \"index\": " + String(i) + ",\n";
        
        // Name
        const char* customName = globalState.display.getNetName(i);
        String name = customName ? String(customName) : String(globalState.connections.nets[i].name);
        name.trim();
        json += "      \"name\": \"" + escapeJson(name) + "\",\n";
        
        // Nodes
        json += "      \"nodes\": [";
        bool firstNode = true;
        int maxCharLen = 0;
        
        // Gather nodes
        for (int j = 0; j < MAX_NODES; j++) {
            int node = globalState.connections.nets[i].nodes[j];
            if (node <= 0) break;
            
            if (!firstNode) json += ", ";
            firstNode = false;
            
            String nodeStr;
            if (node >= 100 || node >= NANO_D0) {
                 // Defines text
                 nodeStr = String(definesToChar(node, 0));
            } else {
                 // Raw row number
                 nodeStr = String(node);
            }
            json += "\"" + nodeStr + "\"";
        }
        json += "],\n";

        // Color (Effective color)
        // We use the same logic as listNets to show the color
        // Note: We might want the hex code
        // For now, let's just note special properties like voltage/GPIO
        
        // Special Status (Voltage, GPIO, etc.)
        // Logic adapted from listNets
        bool isSpecial = false;
        String specialType = "none";
        float voltage = 0.0f;
        int specialInt = 0; // State 0/1 for digital
        
        int gpioOrAdcIndex = -1;
        
        for (int j = 0; j < MAX_NODES; j++) {
             int node = globalState.connections.nets[i].nodes[j];
             if (node <= 0) break;
             
             // ADC
             if ((node >= ADC0 && node <= ADC4) || node == ADC7) {
                 specialType = "ADC";
                 gpioOrAdcIndex = (node == ADC7) ? 7 : (node - ADC0);
                 voltage = adcReadings[gpioOrAdcIndex];
                 if (voltage > -0.03 && voltage < 0.03) voltage = 0.0f;
                 isSpecial = true;
             }
             
             // GPIO / UART
             if ((node >= RP_GPIO_1 && node <= RP_GPIO_8)) {
                 specialType = "GPIO";
                 gpioOrAdcIndex = node - RP_GPIO_1; // 0-7
                 specialInt = gpioReading[gpioOrAdcIndex]; // 0 or 1
                 isSpecial = true;
             }
             if (node == RP_UART_TX) {
                 specialType = "UART_TX";
                 gpioOrAdcIndex = 8; 
                 isSpecial = true;
             }
             if (node == RP_UART_RX) {
                 specialType = "UART_RX";
                 gpioOrAdcIndex = 9;
                 isSpecial = true;
             }

             // Fake GPIO Input
             if (node >= FAKE_GP_IN_0 && node <= FAKE_GP_IN_31) {
                 int inputIdx = node - FAKE_GP_IN_0;
                 if (inputIdx < MAX_FAKE_GP_IN && fakeGpioInputs[inputIdx].active) {
                     specialType = "FAKE_GPIO_IN";
                     int tdmSlot = fakeGpioInputs[inputIdx].tdmSlot;
                     if (tdmSlot >= 0 && tdmSlot < TDM_MAX_CHANNELS) {
                         voltage = tdmInputs.channels[tdmSlot].lastVoltage;
                         if (voltage > -0.03 && voltage < 0.03) voltage = 0.0f;
                     }
                     isSpecial = true;
                 }
             }
             
             // DAC
             if (node == DAC0) {
                 specialType = "DAC";
                 voltage = globalState.power.dac0;
                 isSpecial = true;
             }
             if (node == DAC1) {
                 specialType = "DAC";
                 voltage = globalState.power.dac1;
                 isSpecial = true;
             }
             
             // Top Rail
             if (node == TOP_RAIL) {
                 specialType = "RAIL";
                 voltage = globalState.power.topRail;
                 isSpecial = true;
             }
             
             // Bottom Rail
             if (node == BOTTOM_RAIL) {
                 specialType = "RAIL";
                 voltage = globalState.power.bottomRail;
                 isSpecial = true;
             }
        }
        
        json += "      \"special\": \"" + specialType + "\"";
        
        if (specialType == "ADC" || specialType == "FAKE_GPIO_IN" || 
            specialType == "DAC" || specialType == "RAIL") {
            json += ",\n      \"voltage\": " + String(voltage, 3);
        } else if (specialType == "GPIO" || specialType.startsWith("UART")) {
             json += ",\n      \"logic\": " + String(specialInt);
        }

        json += "\n    }";
    }
    json += "\n  ],\n";


    // --- GPIO Config (enhanced with net, function, pull, reading) ---
    json += "  \"gpio\": [\n";
    bool firstGpio = true;
    for (int i = 0; i < 10; i++) { // 0-7 = GPIO, 8 = TX, 9 = RX
        if (!firstGpio) json += ",\n";
        firstGpio = false;
        
        uint8_t gpio_pin = gpioDef[i][0];
        gpio_function_t func = gpio_get_function(gpio_pin);
        
        json += "    {\n";
        
        // Pin name
        if (i < 8) {
            json += "      \"pin\": " + String(i + 1) + ",\n";
        } else if (i == 8) {
            json += "      \"pin\": \"TX\",\n";
        } else {
            json += "      \"pin\": \"RX\",\n";
        }
        
        // Net association
        if (gpioNet[i] != -1) {
            json += "      \"net\": " + String(gpioNet[i]) + ",\n";
        } else {
            json += "      \"net\": null,\n";
        }
        
        // Function name
        const char* funcName = gpio_function_name_for_pin(gpio_pin, func);
        json += "      \"function\": \"" + String(funcName ? funcName : "SIO") + "\",\n";
        
        // Direction
        if (i < 8) {
            bool isOutput = (globalState.config.gpioDirection[i] == 0);
            json += "      \"direction\": \"" + String(isOutput ? "OUTPUT" : "INPUT") + "\",\n";
        } else {
            // TX/RX are special
            json += "      \"direction\": \"" + String(i == 8 ? "OUTPUT" : "INPUT") + "\",\n";
        }
        
        // Pull state
        json += "      \"pull\": \"" + String(getPullName(i)) + "\",\n";
        
        // Reading (low/high/float/unknown)
        json += "      \"reading\": \"" + String(getReadingName(gpioReading[i])) + "\"";
        
        json += "\n    }";
    }
    json += "\n  ]\n";
    
    json += "}";
    return json;
}



// Forward declarations from States.cpp
extern int parseNodeName(const String& nodeName);
extern void refreshConnections(int ledShowOption = -1, int fillUnused = 0, int clean = 0);

String JsonStateParser::lastError = "";

const char* JsonStateParser::getLastError() {
    return lastError.c_str();
}

// Helper: Extract JSON array by key name
String JsonStateParser::extractArray(const String& json, const char* key) {
    String searchKey = String("\"") + key + "\"";
    int keyPos = json.indexOf(searchKey);
    if (keyPos < 0) return "";
    
    int bracketStart = json.indexOf('[', keyPos);
    if (bracketStart < 0) return "";
    
    // Find matching close bracket
    int depth = 1;
    int pos = bracketStart + 1;
    while (pos < (int)json.length() && depth > 0) {
        char c = json.charAt(pos);
        if (c == '[') depth++;
        else if (c == ']') depth--;
        pos++;
    }
    
    return json.substring(bracketStart, pos);
}

// Helper: Extract JSON object by key name
String JsonStateParser::extractObject(const String& json, const char* key) {
    String searchKey = String("\"") + key + "\"";
    int keyPos = json.indexOf(searchKey);
    if (keyPos < 0) return "";
    
    int braceStart = json.indexOf('{', keyPos);
    if (braceStart < 0) return "";
    
    // Find matching close brace
    int depth = 1;
    int pos = braceStart + 1;
    while (pos < (int)json.length() && depth > 0) {
        char c = json.charAt(pos);
        if (c == '{') depth++;
        else if (c == '}') depth--;
        pos++;
    }
    
    return json.substring(braceStart, pos);
}

// Helper: Extract string value by key
String JsonStateParser::extractString(const String& json, const char* key) {
    String searchKey = String("\"") + key + "\"";
    int keyPos = json.indexOf(searchKey);
    if (keyPos < 0) return "";
    
    int colonPos = json.indexOf(':', keyPos);
    if (colonPos < 0) return "";
    
    int quoteStart = json.indexOf('"', colonPos);
    if (quoteStart < 0) return "";
    
    int quoteEnd = json.indexOf('"', quoteStart + 1);
    if (quoteEnd < 0) return "";
    
    return json.substring(quoteStart + 1, quoteEnd);
}

// Helper: Extract float value by key
float JsonStateParser::extractFloat(const String& json, const char* key, float defaultVal) {
    String searchKey = String("\"") + key + "\"";
    int keyPos = json.indexOf(searchKey);
    if (keyPos < 0) return defaultVal;
    
    int colonPos = json.indexOf(':', keyPos);
    if (colonPos < 0) return defaultVal;
    
    // Find start of number (skip whitespace)
    int numStart = colonPos + 1;
    while (numStart < (int)json.length() && (json.charAt(numStart) == ' ' || json.charAt(numStart) == '\n')) {
        numStart++;
    }
    
    // Find end of number
    int numEnd = numStart;
    while (numEnd < (int)json.length()) {
        char c = json.charAt(numEnd);
        if (!isdigit(c) && c != '.' && c != '-' && c != '+') break;
        numEnd++;
    }
    
    String numStr = json.substring(numStart, numEnd);
    return numStr.toFloat();
}

// Helper: Extract int value by key
int JsonStateParser::extractInt(const String& json, const char* key, int defaultVal) {
    String searchKey = String("\"") + key + "\"";
    int keyPos = json.indexOf(searchKey);
    if (keyPos < 0) return defaultVal;
    
    int colonPos = json.indexOf(':', keyPos);
    if (colonPos < 0) return defaultVal;
    
    // Find start of number
    int numStart = colonPos + 1;
    while (numStart < (int)json.length() && (json.charAt(numStart) == ' ' || json.charAt(numStart) == '\n')) {
        numStart++;
    }
    
    // Check for null
    if (json.substring(numStart, numStart + 4) == "null") return -1;
    
    // Find end of number
    int numEnd = numStart;
    while (numEnd < (int)json.length()) {
        char c = json.charAt(numEnd);
        if (!isdigit(c) && c != '-') break;
        numEnd++;
    }
    
    String numStr = json.substring(numStart, numEnd);
    return numStr.toInt();
}

// Helper: Get next element from JSON array
String JsonStateParser::getNextArrayElement(const String& array, int& startPos) {
    if (startPos >= (int)array.length()) return "";
    
    // Find opening brace of object
    int braceStart = array.indexOf('{', startPos);
    if (braceStart < 0) return "";
    
    // Find matching close brace
    int depth = 1;
    int pos = braceStart + 1;
    while (pos < (int)array.length() && depth > 0) {
        char c = array.charAt(pos);
        if (c == '{') depth++;
        else if (c == '}') depth--;
        pos++;
    }
    
    startPos = pos;
    return array.substring(braceStart, pos);
}

// Parse nets section and add connections
bool JsonStateParser::parseNetsSection(const String& json, const PowerState& oldPower) {
    String netsArray = extractArray(json, "nets");
    if (netsArray.length() == 0) {
        // No nets section is OK - may be partial update
        return true;
    }
    
    int pos = 0;
    String netObj;
    while ((netObj = getNextArrayElement(netsArray, pos)).length() > 0) {
        // Extract nodes array
        String nodesArray = extractArray(netObj, "nodes");
        if (nodesArray.length() == 0) continue;
        
        // Parse node strings
        int nodeCount = 0;
        int nodes[MAX_NODES];
        
        // Simple parsing: find quoted strings
        int nodePos = 0;
        while (nodePos < (int)nodesArray.length()) {
            int quoteStart = nodesArray.indexOf('"', nodePos);
            if (quoteStart < 0) break;
            int quoteEnd = nodesArray.indexOf('"', quoteStart + 1);
            if (quoteEnd < 0) break;
            
            String nodeStr = nodesArray.substring(quoteStart + 1, quoteEnd);
            int nodeVal = parseNodeName(nodeStr);
            if (nodeVal > 0 && nodeCount < MAX_NODES) {
                nodes[nodeCount++] = nodeVal;
            }
            nodePos = quoteEnd + 1;
        }
        
        // Create bridges for all pairs of consecutive nodes
        for (int i = 0; i < nodeCount - 1; i++) {
            // Use existing connection logic
            addBridgeToState(nodes[i], nodes[i + 1], -1, false);
        }

        // Check for voltage override in net object
        // Only applies to special nets: TOP_RAIL (2), BOTTOM_RAIL (3), DAC0 (4), DAC1 (5) via their indices
        int netIndex = extractInt(netObj, "index", -1);
        
        if (netIndex >= 2 && netIndex <= 5) { // Special voltage nets
            float voltage = extractFloat(netObj, "voltage", -999.0f); // Default to impossible value
            
            if (voltage > -100.0f) { // If voltage was present
                // Determine which power rail this corresponds to
                float* currentValPtr = nullptr;
                float oldVal = 0.0f;
                
                switch (netIndex) {
                    case 2: // Top Rail
                        currentValPtr = &globalState.power.topRail;
                        oldVal = oldPower.topRail;
                        break;
                    case 3: // Bottom Rail
                        currentValPtr = &globalState.power.bottomRail;
                        oldVal = oldPower.bottomRail;
                        break;
                    case 4: // DAC 0
                        currentValPtr = &globalState.power.dac0;
                        oldVal = oldPower.dac0;
                        break;
                    case 5: // DAC 1
                        currentValPtr = &globalState.power.dac1;
                        oldVal = oldPower.dac1;
                        break;
                }
                
                if (currentValPtr) {
                    float currentVal = *currentValPtr;
                    
                    // Conflict resolution:
                    // logic defines that if conflict, we use whichever differs from current state.
                    // But effectively, 'currentVal' has JUST been updated by parsePowerSection above.
                    // 'oldVal' is what it was before this whole JSON apply process.
                    
                    // If the parsed voltage is different from what we sought (which might be from power section)
                    if (fabs(voltage - currentVal) > 0.001f) {
                        // Conflict!
                        // If power section kept it same as old (meaning user didn't change power section, or omitted it)
                        // Then we respect this net section change.
                        if (fabs(currentVal - oldVal) < 0.001f) {
                            *currentValPtr = voltage;
                        }
                        // Else: Power section ALSO changed it. We prioritize Power section (keep currentVal).
                    }
                }
            }
        }
    }
    
    return true;
}

// Parse power section and apply DAC/rail settings
bool JsonStateParser::parsePowerSection(const String& json) {
    String powerObj = extractObject(json, "power");
    if (powerObj.length() == 0) return true; // Optional
    
    float topRail = extractFloat(powerObj, "top_rail", globalState.power.topRail);
    float bottomRail = extractFloat(powerObj, "bottom_rail", globalState.power.bottomRail);
    float dac0 = extractFloat(powerObj, "dac0", globalState.power.dac0);
    float dac1 = extractFloat(powerObj, "dac1", globalState.power.dac1);
    
    // Serial.printf("Top Rail: %f, Bottom Rail: %f, DAC0: %f, DAC1: %f\n", topRail, bottomRail, dac0, dac1);


    // Validate ranges
    if (topRail < -8.0 || topRail > 8.0 ||
        bottomRail < -8.0 || bottomRail > 8.0 ||
        dac0 < -8.0 || dac0 > 8.0 ||
        dac1 < -8.0 || dac1 > 8.0) {
        lastError = "Power values out of range (-8V to +8V)";
        return false;
    }
    
    // Apply
    globalState.power.topRail = topRail;
    globalState.power.bottomRail = bottomRail;
    globalState.power.dac0 = dac0;
    globalState.power.dac1 = dac1;
    

    setRailsAndDACs(0); // Apply without saving to EEPROM
    
    // Serial.printf("Top Rail: %f, Bottom Rail: %f, DAC0: %f, DAC1: %f\n", getDacVoltage(2), getDacVoltage(3), getDacVoltage(0), getDacVoltage(1));
    return true;
}

// Parse GPIO section and apply configuration
bool JsonStateParser::parseGpioSection(const String& json) {
    String gpioArray = extractArray(json, "gpio");
    if (gpioArray.length() == 0) return true; // Optional
    
    int pos = 0;
    String gpioObj;
    while ((gpioObj = getNextArrayElement(gpioArray, pos)).length() > 0) {
        int pin = extractInt(gpioObj, "pin", -1);
        if (pin < 1 || pin > 8) continue; // Skip TX/RX, only handle GPIO 1-8
        
        int idx = pin - 1;
        
        // Direction
        String dir = extractString(gpioObj, "direction");
        if (dir.length() > 0) {
            dir.toUpperCase();
            globalState.config.gpioDirection[idx] = (dir == "OUTPUT") ? 0 : 1;
        }
        
        // Pull
        String pull = extractString(gpioObj, "pull");
        if (pull.length() > 0) {
            pull.toLowerCase();
            if (pull == "down") globalState.config.gpioPulls[idx] = 0;
            else if (pull == "up") globalState.config.gpioPulls[idx] = 1;
            else if (pull == "none") globalState.config.gpioPulls[idx] = 2;
            else if (pull == "keeper") globalState.config.gpioPulls[idx] = 3;
        }
    }
    
    // Apply GPIO configuration
    setGPIO();
    
    return true;
}

// Main entry point
bool JsonStateParser::applyJSONState(const String& json, bool clearFirst) {
    lastError = "";
    
    if (json.length() < 2) {
        lastError = "Empty or invalid JSON";
        return false;
    }
    
    // Basic JSON validation - must start with { and end with }
    String trimmed = json;
    trimmed.trim();
    if (trimmed.charAt(0) != '{' || trimmed.charAt(trimmed.length() - 1) != '}') {
        lastError = "Invalid JSON: must be an object";
        return false;
    }
    
    // Must contain at least one expected section
    bool hasNets = (json.indexOf("\"nets\"") >= 0);
    bool hasPower = (json.indexOf("\"power\"") >= 0);
    bool hasGpio = (json.indexOf("\"gpio\"") >= 0);
    
    if (!hasNets && !hasPower && !hasGpio) {
        lastError = "Invalid JSON: no recognized sections (nets, power, gpio)";
        return false;
    }
    
    // Clear existing connections and FakeGPIO state if requested
    if (clearFirst) {
        clearAllFakeGpio();  // Clear FakeGPIO first
        globalState.clearAllConnections();
    }
    
    // Capture old power state for conflict resolution
    PowerState oldPower = globalState.power;

    // Parse each section
    // Parse Power FIRST so it establishes the baseline for conflict resolution
    if (!parsePowerSection(json)) return false;
    
    // Parse Nets (might override power if power section didn't change it)
    if (!parseNetsSection(json, oldPower)) return false;
    
    if (!parseGpioSection(json)) return false;
    
    // Initialize FakeGPIO from loaded state (before routing)
    initializeFakeGpioFromLoadedState();
    
    // Apply connections to hardware
    refreshConnections(-1, 1, 1);
    
    // Finalize FakeGPIO after routing
    finalizeFakeGpioAfterRouting();
    
    return true;
}

