// SPDX-License-Identifier: MIT
#include "WokwiParser.h"
#include "FileParsing.h"
#include "JumperlessDefines.h"
#include "USBfs.h"
#include "FatFS.h"
#include "Colors.h"

extern bool debugFP;

// Simple JSON helper functions - no external library needed
int findNextChar(const String& str, char searchChar, int startIdx) {
    for (int i = startIdx; i < str.length(); i++) {
        if (str[i] == searchChar) return i;
    }
    return -1;
}

String extractJsonString(const String& json, const String& key, int startIdx) {
    // Find "key": "value" pattern
    String searchPattern = "\"" + key + "\"";
    int keyIdx = json.indexOf(searchPattern, startIdx);
    if (keyIdx == -1) return "";
    
    // Find the colon after the key
    int colonIdx = json.indexOf(':', keyIdx + searchPattern.length());
    if (colonIdx == -1) return "";
    
    // Skip whitespace after colon
    int valueIdx = colonIdx + 1;
    while (valueIdx < json.length() && (json[valueIdx] == ' ' || json[valueIdx] == '\t' || json[valueIdx] == '\n' || json[valueIdx] == '\r')) {
        valueIdx++;
    }
    
    // Check if it's a string (starts with quote)
    if (valueIdx >= json.length()) return "";
    
    if (json[valueIdx] == '"') {
        // Extract string value
        int startQuote = valueIdx;
        int endQuote = json.indexOf('"', startQuote + 1);
        if (endQuote == -1) return "";
        return json.substring(startQuote + 1, endQuote);
    }
    
    return "";
}

// Extract array from JSON
int findJsonArray(const String& json, const String& arrayName, int& startIdx, int& endIdx) {
    String searchPattern = "\"" + arrayName + "\"";
    int keyIdx = json.indexOf(searchPattern);
    if (keyIdx == -1) return -1;
    
    // Find the colon
    int colonIdx = json.indexOf(':', keyIdx);
    if (colonIdx == -1) return -1;
    
    // Find the opening bracket
    startIdx = json.indexOf('[', colonIdx);
    if (startIdx == -1) return -1;
    
    // Find matching closing bracket (count nested brackets)
    int bracketCount = 1;
    endIdx = startIdx + 1;
    while (endIdx < json.length() && bracketCount > 0) {
        if (json[endIdx] == '[') bracketCount++;
        else if (json[endIdx] == ']') bracketCount--;
        endIdx++;
    }
    
    if (bracketCount != 0) return -1;
    
    return 0;
}

// Breadboard mapping: Jumperless uses rows 1-30, columns a-j
// Top section (t): rows 1-30, columns a-e map to nodes 1-30 (column a) through ? 
// Bottom section (b): rows 1-30, columns f-j
// This is a simplified mapping - adjust based on actual Jumperless hardware
int wokwiPinToJumperlessNode(const String& pinStr) {
    // Format: "bb1:8t.c" or "bb1:21b.h" or "bb1:tp.2" (top rail) or "bb1:bp.3" (bottom rail)
    // bb1 = breadboard 1
    // 8 = row number (or 't'/'b' for rails)
    // t/b = top or bottom section (or 'p' for power rail)
    // c/h = column letter
    
    int colonIdx = pinStr.indexOf(':');
    if (colonIdx == -1) return -1;
    
    String pinPart = pinStr.substring(colonIdx + 1);
    
    // Handle special cases for power rails
    if (pinPart.startsWith("tp") || pinPart.startsWith("VCC")) {
        // Top power rail (positive)
        return TOP_RAIL; // Node 101
    }
    if (pinPart.startsWith("bp")) {
        // Bottom power rail (positive)
        return BOTTOM_RAIL; // Node 102
    }
    if (pinPart.startsWith("tn")) {
        // Top negative rail (ground)
        return GND; // Node 100
    }
    if (pinPart.startsWith("bn")) {
        // Bottom negative rail (ground)
        return GND; // Node 100
    }
    
    // Extract row number
    int row = 0;
    int i = 0;
    while (i < pinPart.length() && isDigit(pinPart[i])) {
        row = row * 10 + (pinPart[i] - '0');
        i++;
    }
    
    if (row < 1 || row > 30) return -1;
    
    // Extract section (t/b)
    char section = pinPart[i];
    if (section != 't' && section != 'b') return -1;
    i++;
    
    // Skip the dot and column letter - in Jumperless, all columns in a row are connected!
    // Column letters (a-e, f-j) are just physical positions but share the same electrical node
    
    // Map to Jumperless node numbers
    // Jumperless breadboard: nodes 1-60
    // TOP section (rows 1-30): nodes 1-30 (one node per row, all columns connected)
    // BOTTOM section (rows 1-30): nodes 31-60 (one node per row, all columns connected)
    
    if (section == 't') {
        // Top section: row N → node N
        // TOP_1 = 1, TOP_2 = 2, ..., TOP_30 = 30
        return row;
    } else {
        // Bottom section: row N → node (30 + N)
        // BOTTOM_1 = 31, BOTTOM_2 = 32, ..., BOTTOM_30 = 60
        return 30 + row;
    }
}

int arduinoPinToJumperlessNode(const String& pinStr) {
    // Format: "nano:13" or "nano:A0" or "nano:GND"
    int colonIdx = pinStr.indexOf(':');
    if (colonIdx == -1) return -1;
    
    String pin = pinStr.substring(colonIdx + 1);
    pin.trim();
    
    // Handle special pins
    if (pin == "GND" || pin == "gnd") {
        return GND; // Ground node
    }
    if (pin == "5V" || pin == "VCC" || pin == "vcc") {
        return TOP_RAIL; // 5V supply
    }
    if (pin == "3V3" || pin == "3.3V") {
        return BOTTOM_RAIL; // 3.3V supply
    }
    
    // Handle analog pins (A0-A7)
    if (pin[0] == 'A' || pin[0] == 'a') {
        int analogNum = pin.substring(1).toInt();
        if (analogNum >= 0 && analogNum <= 7) {
            return NANO_A0 + analogNum; // NANO_A0 through NANO_A7
        }
        return -1;
    }
    
    // Handle digital pins (0-13)
    int digitalNum = pin.toInt();
    if (digitalNum >= 0 && digitalNum <= 13) {
        return NANO_D0 + digitalNum; // NANO_D0 through NANO_D13
    }
    
    return -1;
}

int logicAnalyzerPinToGPIO(const String& pinStr) {
    // Format: "logic1:D0" through "logic1:D7"
    // Maps to RP_GPIO_1 through RP_GPIO_8 (nodes 131-138)
    int colonIdx = pinStr.indexOf(':');
    if (colonIdx == -1) return -1;
    
    String pin = pinStr.substring(colonIdx + 1);
    pin.trim();
    
    if (pin.length() >= 2 && (pin[0] == 'D' || pin[0] == 'd')) {
        int channelNum = pin.substring(1).toInt();
        if (channelNum >= 0 && channelNum <= 7) {
            // D0→RP_GPIO_1 (131), D1→RP_GPIO_2 (132), ..., D7→RP_GPIO_8 (138)
            return RP_GPIO_1 + channelNum;
        }
    }
    
    return -1;
}

int parseVoltageString(const String& voltageStr) {
    // Parse strings like "3.3V", "5V", "2.5V"
    String str = voltageStr;
    str.trim();
    str.toUpperCase();
    
    // Remove 'V' suffix
    if (str.endsWith("V")) {
        str = str.substring(0, str.length() - 1);
    }
    
    // Parse as float and convert to millivolts
    float voltage = str.toFloat();
    if (voltage > 0 && voltage <= 12) { // Reasonable voltage range
        return (int)(voltage * 1000); // Convert to millivolts
    }
    
    return -1;
}

bool parseWokwiDiagram(const String& jsonContent, JumperlessState& outState, 
                       int slotNum, String& errorMsg, bool quietMode) {
    // Preserve power settings to prevent LED flickering during updates
    // Only clear connections, not power - parser will update voltages if found in diagram
    float savedTopRail = outState.power.topRail;
    float savedBottomRail = outState.power.bottomRail;
    float savedDac0 = outState.power.dac0;
    float savedDac1 = outState.power.dac1;
    
    // Clear only connections, preserve power and other settings
    outState.connections.clear();
    
    // Restore power settings - will be overwritten if diagram specifies new values
    outState.power.topRail = savedTopRail;
    outState.power.bottomRail = savedBottomRail;
    outState.power.dac0 = savedDac0;
    outState.power.dac1 = savedDac1;
    
    if (debugFP) {
        Serial.println("◆ JSON length: " + String(jsonContent.length()) + " bytes");
        Serial.println("◆ First 100 chars: " + jsonContent.substring(0, 100));
        Serial.println("◆ Last 100 chars: " + jsonContent.substring(jsonContent.length() > 100 ? jsonContent.length() - 100 : 0));
    }
    
    // Custom JSON parser - no external library needed
    // We only need to extract connections and text labels from Wokwi diagram format
    
    // Extract rail voltages from text labels
    int topRailVoltage = -1;
    int bottomRailVoltage = -1;
    
    // Find all text parts and parse voltage info
    int textIdx = 0;
    while ((textIdx = jsonContent.indexOf("\"wokwi-text\"", textIdx)) != -1) {
        // Find the "text": "..." value for this part
        int textKeyIdx = jsonContent.indexOf("\"text\"", textIdx);
        if (textKeyIdx != -1 && textKeyIdx < textIdx + 500) { // Within 500 chars of part type
            String textValue = extractJsonString(jsonContent, "text", textKeyIdx);
            textValue.toLowerCase();
            
            if (debugFP) {
                Serial.println("  Found text: " + textValue);
            }
            
            // Parse "top rail 3.3V" or "VCC = 5V" style labels
            // Be very permissive - handle various formats and multi-line text
            if (textValue.indexOf("top") >= 0 && textValue.indexOf("rail") >= 0) {
                // Find "top" and search for "rail" AFTER it
                int topIdx = textValue.indexOf("top");
                int railIdx = textValue.indexOf("rail", topIdx);
                if (railIdx > topIdx) {
                    String voltPart = textValue.substring(railIdx + 4);
                    topRailVoltage = parseVoltageString(voltPart);
                    if (debugFP && topRailVoltage > 0) {
                        Serial.println("  Top rail: " + String(topRailVoltage) + "mV");
                    }
                }
            }
            
            if (textValue.indexOf("bottom") >= 0 && textValue.indexOf("rail") >= 0) {
                // Find "bottom" and search for "rail" AFTER it (not before!)
                int bottomIdx = textValue.indexOf("bottom");
                int railIdx = textValue.indexOf("rail", bottomIdx);
                if (railIdx > bottomIdx) {
                    String voltPart = textValue.substring(railIdx + 4);
                    bottomRailVoltage = parseVoltageString(voltPart);
                    if (debugFP && bottomRailVoltage > 0) {
                        Serial.println("  Bottom rail: " + String(bottomRailVoltage) + "mV");
                    }
                }
            }
            
            // Parse "VCC = 5V" or "VCC=5V" style
            if (textValue.indexOf("vcc") >= 0) {
                int eqIdx = textValue.indexOf('=');
                if (eqIdx >= 0) {
                    String voltPart = textValue.substring(eqIdx + 1);
                    int voltage = parseVoltageString(voltPart);
                    if (voltage > 0 && topRailVoltage == -1) {
                        topRailVoltage = voltage;
                        if (debugFP) {
                            Serial.println("  VCC: " + String(topRailVoltage) + "mV");
                        }
                    }
                }
            }
        }
        textIdx++;
    }
    
    // Parse connections array
    int connectionsStart = 0, connectionsEnd = 0;
    if (findJsonArray(jsonContent, "connections", connectionsStart, connectionsEnd) != 0) {
        errorMsg = "No connections array found in diagram";
        return false;
    }
    
    int connCount = 0;
    int skippedCount = 0;
    
    // Track colors for "all green" detection (Wokwi's default when user doesn't customize)
    int userColorCount = 0;  // Non-GND/VCC bridges with colors
    int greenColorCount = 0; // How many are green
    
    // Parse each connection: [ "source", "target", "color", [...] ]
    int connIdx = connectionsStart + 1; // Skip opening '['
    
    while (connIdx < connectionsEnd) {
        // Find next connection array (starts with '[')
        int connStart = jsonContent.indexOf('[', connIdx);
        if (connStart == -1 || connStart >= connectionsEnd) break;
        
        // Find end of this connection array
        int connEnd = jsonContent.indexOf(']', connStart);
        if (connEnd == -1 || connEnd >= connectionsEnd) break;
        
        // Extract the 4 values: source, target, color, wire placement
        String connStr = jsonContent.substring(connStart + 1, connEnd);
        
        // Parse quoted strings from the connection array
        String source = "", target = "", colorName = "";
        int quoteCount = 0;
        int quoteStart = -1;
        
        for (int i = 0; i < connStr.length(); i++) {
            if (connStr[i] == '"') {
                if (quoteStart == -1) {
                    quoteStart = i;
                } else {
                    // End of quoted string
                    String value = connStr.substring(quoteStart + 1, i);
                    if (quoteCount == 0) source = value;
                    else if (quoteCount == 1) target = value;
                    else if (quoteCount == 2) colorName = value;
                    
                    quoteCount++;
                    quoteStart = -1;
                    
                    if (quoteCount >= 3) break; // Got all we need
                }
            }
        }
        
        if (source.length() == 0 || target.length() == 0) {
            // Couldn't parse the connection strings
            connIdx = connEnd + 1;
            skippedCount++;
            continue;
        }
        
        if (debugFP) {
            Serial.println("  Connection: " + source + " → " + target + " (" + colorName + ")");
        }
        
        // Convert source and target to Jumperless node numbers
        int sourceNode = -1;
        int targetNode = -1;
        
        // Detect component types and map accordingly
        if (source.startsWith("bb") || source.startsWith("BB")) {
            sourceNode = wokwiPinToJumperlessNode(source);
        } else if (source.startsWith("nano") || source.startsWith("NANO")) {
            sourceNode = arduinoPinToJumperlessNode(source);
        } else if (source.startsWith("logic")) {
            sourceNode = logicAnalyzerPinToGPIO(source);
        } else if (source.startsWith("vcc") || source.indexOf("VCC") >= 0) {
            sourceNode = TOP_RAIL; // or use detected topRailVoltage
        } else if (source.startsWith("gnd") || source.indexOf("GND") >= 0) {
            sourceNode = GND;
        }
        
        if (target.startsWith("bb") || target.startsWith("BB")) {
            targetNode = wokwiPinToJumperlessNode(target);
        } else if (target.startsWith("nano") || target.startsWith("NANO")) {
            targetNode = arduinoPinToJumperlessNode(target);
        } else if (target.startsWith("logic")) {
            targetNode = logicAnalyzerPinToGPIO(target);
        } else if (target.startsWith("vcc") || target.indexOf("VCC") >= 0) {
            targetNode = TOP_RAIL;
        } else if (target.startsWith("gnd") || target.indexOf("GND") >= 0) {
            targetNode = GND;
        }
        
        if (sourceNode > 0 && targetNode > 0 && sourceNode != targetNode) {
            // Add connection to state
            // bridges is int[MAX_BRIDGES][3] where [i][0]=node1, [i][1]=node2, [i][2]=duplicates
            if (outState.connections.numBridges < MAX_BRIDGES) {
                int bridgeIdx = outState.connections.numBridges;
                outState.connections.bridges[bridgeIdx][0] = sourceNode;
                outState.connections.bridges[bridgeIdx][1] = targetNode;
                outState.connections.bridges[bridgeIdx][2] = -1; // duplicates
                outState.connections.numBridges++;
                connCount++;
                
                if (debugFP) {
                    Serial.println("    ✓ Mapped: " + String(sourceNode) + " ↔ " + String(targetNode));
                }
                
                // Store wire color for this bridge if color was specified
                if (colorName.length() > 0) {
                    // Track color names for "all green" detection
                    // Skip GND/VCC connections (Wokwi sets these to red/black by default)
                    bool isSpecialNet = (sourceNode == GND || targetNode == GND ||
                                        sourceNode == TOP_RAIL || targetNode == TOP_RAIL ||
                                        sourceNode == BOTTOM_RAIL || targetNode == BOTTOM_RAIL);
                    
                    if (!isSpecialNet) {
                        userColorCount++;
                        if (colorName.equalsIgnoreCase("green") || colorName.equalsIgnoreCase("limegreen")) {
                            greenColorCount++;
                        }
                    }
                    
                    uint32_t color = wokwiColorToRGB(colorName);
                    
                    // Convert black to "no color" for better visibility
                    if (colorName.equalsIgnoreCase("black") || color == 0x000000) {
                        outState.connections.bridgeColors[bridgeIdx] = 0xFFFFFFFF;
                        if (debugFP) {
                            Serial.println("      Color: black → no color (for visibility)");
                        }
                    } else {
                        outState.connections.bridgeColors[bridgeIdx] = color;
                        if (debugFP) {
                            String internalName = wokwiColorToInternalName(colorName);
                            Serial.printf("      Color: %s → 0x%06X (%s)\n", 
                                        colorName.c_str(), color, internalName.c_str());
                        }
                    }
                } else {
                    if (debugFP) {
                        Serial.println("      No color specified");
                    }
                }
            } else {
                Serial.println("    ✗ Max bridges reached!");
                break;
            }
        } else {
            if (debugFP) {
                Serial.println("    ✗ Skipped: couldn't map nodes (" + 
                             String(sourceNode) + ", " + String(targetNode) + ")");
            }
            skippedCount++;
        }
        
        // Move to next connection
        connIdx = connEnd + 1;
    }
    
    // Check if all user-defined colors are green (Wokwi's default)
    // If so, clear all bridge colors to trigger auto-color assignment
    if (userColorCount > 0 && greenColorCount == userColorCount) {
        if (debugFP || !quietMode) {
            Serial.println("◆ All wires are green (Wokwi default) - enabling auto-color assignment");
        }
        // Clear all bridge colors except GND/VCC connections
        for (int b = 0; b < outState.connections.numBridges; b++) {
            int n1 = outState.connections.bridges[b][0];
            int n2 = outState.connections.bridges[b][1];
            bool isSpecialNet = (n1 == GND || n2 == GND ||
                                n1 == TOP_RAIL || n2 == TOP_RAIL ||
                                n1 == BOTTOM_RAIL || n2 == BOTTOM_RAIL);
            if (!isSpecialNet) {
                outState.connections.bridgeColors[b] = 0xFFFFFFFF; // Mark as "no color"
            }
        }
    }
    
    // Only show parsing summary if not in quiet mode or if debugFP is on
    if (!quietMode || debugFP) {
        Serial.println("◆ Wokwi parsing complete:");
        Serial.println("  Connections: " + String(connCount) + " added, " + String(skippedCount) + " skipped");
        if (userColorCount > 0) {
            Serial.println("  Colors: " + String(userColorCount) + " user-defined (" + 
                         String(greenColorCount) + " green)");
        }
        if (topRailVoltage > 0) {
            Serial.println("  Top rail: " + String(topRailVoltage) + "mV");
        }
        if (bottomRailVoltage > 0) {
            Serial.println("  Bottom rail: " + String(bottomRailVoltage) + "mV");
        }
    }
    
    // Apply rail voltages to PowerState if detected
    if (topRailVoltage > 0) {
        outState.power.topRail = topRailVoltage / 1000.0f; // Convert mV to V
        if (debugFP) {
            Serial.println("  Set top rail to " + String(outState.power.topRail, 2) + "V");
        }
    }
    if (bottomRailVoltage > 0) {
        outState.power.bottomRail = bottomRailVoltage / 1000.0f; // Convert mV to V
        if (debugFP) {
            Serial.println("  Set bottom rail to " + String(outState.power.bottomRail, 2) + "V");
        }
    }
    
    // Debug: Show all bridge colors that were stored
    if (debugFP) {
        Serial.println("◆ Bridge colors stored:");
        for (int i = 0; i < outState.connections.numBridges; i++) {
            if (outState.connections.bridgeColors[i] != 0xFFFFFFFF) {
                Serial.printf("  Bridge %d: 0x%06X (%s)\n", i, 
                            outState.connections.bridgeColors[i],
                            rgbToWokwiColorName(outState.connections.bridgeColors[i]).c_str());
            }
        }
    }
    
    // Bridge colors are now stored in outState.connections.bridgeColors[]
    // The actual net-to-color mapping will be done after nets are generated
    // by the system when the state is loaded and nets are computed
    
    return connCount > 0;
}

bool parseWokwiDiagramFromFile(const String& filename, int slotNum, String& errorMsg) {
    // Open and read the file
    if (!FatFS.exists(filename.c_str())) {
        errorMsg = "File not found: " + filename;
        return false;
    }
    
    File file = FatFS.open(filename.c_str(), "r");
    if (!file) {
        errorMsg = "Failed to open file: " + filename;
        return false;
    }
    
    // Read entire file content
    String jsonContent;
    while (file.available()) {
        jsonContent += (char)file.read();
    }
    file.close();
    
    if (debugFP) {
        Serial.println("◆ Read " + String(jsonContent.length()) + " bytes from " + filename);
    }
    
    // NEVER copy JumperlessState - work directly with the singleton
    SlotManager& mgr = SlotManager::getInstance();
    
    // Remember which slot was active
    int savedActiveSlot = mgr.getActiveSlot();
    bool needToRestore = (savedActiveSlot != slotNum);
    
    // Clear the active state and parse directly into it (NO COPIES!)
    mgr.getActiveState().clear();
    mgr.setActiveSlot(slotNum);
    
    // Parse directly into the active state (no temp object!)
    if (!parseWokwiDiagram(jsonContent, mgr.getActiveState(), slotNum, errorMsg)) {
        // Restore original slot on parse failure
        if (needToRestore) {
            mgr.loadSlot(savedActiveSlot, errorMsg);
        }
        return false;
    }
    
    // Save it
    bool success = false;
    if (mgr.saveSlot(slotNum, errorMsg)) {
        Serial.println("  ✓ Saved Wokwi diagram to slot " + String(slotNum));
        success = true;
        
        // If we modified a non-active slot, reload the original active slot
        if (needToRestore) {
            mgr.loadSlot(savedActiveSlot, errorMsg);
        } else {
            // We just saved the active slot, reload it to apply changes
            Serial.println("  ↻ Reloading active slot to apply changes...");
            if (mgr.loadSlot(slotNum, errorMsg)) {
                Serial.println("  ✓ Applied to hardware");
            } else {
                Serial.println("  ✗ Failed to apply: " + errorMsg);
            }
        }
    } else {
        Serial.println("  ✗ Failed to save: " + errorMsg);
        // Restore the original slot on failure
        if (needToRestore) {
            mgr.loadSlot(savedActiveSlot, errorMsg);
        }
    }
    
    return success;
}

bool parseWokwiDiagramDirectToFile(const String& jsonContent, int slotNum, 
                                    String& errorMsg, bool quietMode) {
    // Parse Wokwi JSON and write YAML directly to file without creating JumperlessState
    // This is the most memory-efficient approach for inactive slots
    
    extern bool debugFP;
    
    if (debugFP && !quietMode) {
        Serial.println("◆ Parsing Wokwi diagram directly to file (zero-copy)");
        Serial.println("  JSON length: " + String(jsonContent.length()) + " bytes");
    }
    
    // Extract rail voltages from text labels
    int topRailVoltage = -1;
    int bottomRailVoltage = -1;
    
    // Find all text parts and parse voltage info
    int textIdx = 0;
    while ((textIdx = jsonContent.indexOf("\"wokwi-text\"", textIdx)) != -1) {
        int textKeyIdx = jsonContent.indexOf("\"text\"", textIdx);
        if (textKeyIdx != -1 && textKeyIdx < textIdx + 500) {
            String textValue = extractJsonString(jsonContent, "text", textKeyIdx);
            textValue.toLowerCase();
            
            if (textValue.indexOf("top") >= 0 && textValue.indexOf("rail") >= 0) {
                int topIdx = textValue.indexOf("top");
                int railIdx = textValue.indexOf("rail", topIdx);
                if (railIdx > topIdx) {
                    String voltPart = textValue.substring(railIdx + 4);
                    topRailVoltage = parseVoltageString(voltPart);
                }
            }
            
            if (textValue.indexOf("bottom") >= 0 && textValue.indexOf("rail") >= 0) {
                int bottomIdx = textValue.indexOf("bottom");
                int railIdx = textValue.indexOf("rail", bottomIdx);
                if (railIdx > bottomIdx) {
                    String voltPart = textValue.substring(railIdx + 4);
                    bottomRailVoltage = parseVoltageString(voltPart);
                }
            }
            
            if (textValue.indexOf("vcc") >= 0 && topRailVoltage == -1) {
                int eqIdx = textValue.indexOf('=');
                if (eqIdx >= 0) {
                    String voltPart = textValue.substring(eqIdx + 1);
                    topRailVoltage = parseVoltageString(voltPart);
                }
            }
        }
        textIdx++;
    }
    
    // Parse connections array
    int connectionsStart = 0, connectionsEnd = 0;
    if (findJsonArray(jsonContent, "connections", connectionsStart, connectionsEnd) != 0) {
        errorMsg = "No connections array found in diagram";
        return false;
    }
    
    // Build YAML content incrementally (string building is OK, it's on heap)
    String yamlContent = "version: 2\n";
    yamlContent += "sourceOfTruth: bridges\n";
    yamlContent += "\nbridges:\n";
    
    int connCount = 0;
    int skippedCount = 0;
    
    // Parse each connection and add to YAML
    int connIdx = connectionsStart + 1;
    
    while (connIdx < connectionsEnd) {
        int connStart = jsonContent.indexOf('[', connIdx);
        if (connStart == -1 || connStart >= connectionsEnd) break;
        
        int connEnd = jsonContent.indexOf(']', connStart);
        if (connEnd == -1 || connEnd >= connectionsEnd) break;
        
        String connStr = jsonContent.substring(connStart + 1, connEnd);
        
        // Parse quoted strings from the connection array
        String source = "", target = "", colorName = "";
        int quoteCount = 0;
        int quoteStart = -1;
        
        for (int i = 0; i < connStr.length(); i++) {
            if (connStr[i] == '"') {
                if (quoteStart == -1) {
                    quoteStart = i;
                } else {
                    String value = connStr.substring(quoteStart + 1, i);
                    if (quoteCount == 0) source = value;
                    else if (quoteCount == 1) target = value;
                    else if (quoteCount == 2) colorName = value;
                    
                    quoteCount++;
                    quoteStart = -1;
                    
                    if (quoteCount >= 3) break;
                }
            }
        }
        
        if (source.length() == 0 || target.length() == 0) {
            connIdx = connEnd + 1;
            skippedCount++;
            continue;
        }
        
        // Convert source and target to Jumperless node numbers
        int sourceNode = -1;
        int targetNode = -1;
        
        if (source.startsWith("bb") || source.startsWith("BB")) {
            sourceNode = wokwiPinToJumperlessNode(source);
        } else if (source.startsWith("nano") || source.startsWith("NANO")) {
            sourceNode = arduinoPinToJumperlessNode(source);
        } else if (source.startsWith("logic")) {
            sourceNode = logicAnalyzerPinToGPIO(source);
        } else if (source.startsWith("vcc") || source.indexOf("VCC") >= 0) {
            sourceNode = TOP_RAIL;
        } else if (source.startsWith("gnd") || source.indexOf("GND") >= 0) {
            sourceNode = GND;
        }
        
        if (target.startsWith("bb") || target.startsWith("BB")) {
            targetNode = wokwiPinToJumperlessNode(target);
        } else if (target.startsWith("nano") || target.startsWith("NANO")) {
            targetNode = arduinoPinToJumperlessNode(target);
        } else if (target.startsWith("logic")) {
            targetNode = logicAnalyzerPinToGPIO(target);
        } else if (target.startsWith("vcc") || target.indexOf("VCC") >= 0) {
            targetNode = TOP_RAIL;
        } else if (target.startsWith("gnd") || target.indexOf("GND") >= 0) {
            targetNode = GND;
        }
        
        if (sourceNode > 0 && targetNode > 0 && sourceNode != targetNode) {
            // Add bridge to YAML
            yamlContent += "  - {n1: " + String(sourceNode) + ", n2: " + String(targetNode);
            
            // Add color if specified
            if (colorName.length() > 0) {
                uint32_t color = wokwiColorToRGB(colorName);
                yamlContent += ", color: 0x" + String(color, HEX);
            }
            
            yamlContent += "}\n";
            connCount++;
        } else {
            skippedCount++;
        }
        
        connIdx = connEnd + 1;
    }
    
    // Add power section if voltages were detected
    yamlContent += "\npower:\n";
    if (topRailVoltage > 0) {
        yamlContent += "  topRail: " + String(topRailVoltage / 1000.0f, 2) + "\n";
    } else {
        yamlContent += "  topRail: 0.0\n";
    }
    if (bottomRailVoltage > 0) {
        yamlContent += "  bottomRail: " + String(bottomRailVoltage / 1000.0f, 2) + "\n";
    } else {
        yamlContent += "  bottomRail: 0.0\n";
    }
    yamlContent += "  dac0: 3.33\n";
    yamlContent += "  dac1: 0.0\n";
    
    // Show summary
    if (!quietMode || debugFP) {
        Serial.println("◆ Wokwi parsing complete:");
        Serial.println("  Connections: " + String(connCount) + " added, " + String(skippedCount) + " skipped");
        if (topRailVoltage > 0) {
            Serial.println("  Top rail: " + String(topRailVoltage) + "mV");
        }
        if (bottomRailVoltage > 0) {
            Serial.println("  Bottom rail: " + String(bottomRailVoltage) + "mV");
        }
    }
    
    // Write directly to file
    SlotManager& mgr = SlotManager::getInstance();
    if (!mgr.writeSlotFile(slotNum, yamlContent, errorMsg)) {
        return false;
    }
    
    if (!quietMode || debugFP) {
        Serial.println("  ✓ Wrote " + String(yamlContent.length()) + " bytes to slot file");
    }
    
    return connCount > 0;
}

