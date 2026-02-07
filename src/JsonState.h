/*
 * JsonState.h
 * 
 * Generates JSON representation of the current Jumperless state
 * for LLM and external tool consumption.
 */

#ifndef JSONSTATE_H
#define JSONSTATE_H

#include <Arduino.h>

struct PowerState;

class JsonState {
public:
    static String getJumperlessStateJSON();
};



class JsonStateParser {
public:
    // Main entry point - parses JSON and applies state
    // Returns true on success, false on parse error
    static bool applyJSONState(const String& json, bool clearFirst = true);
    
    // Get last error message
    static const char* getLastError();
    
private:
    static String lastError;
    
    // Section parsers
    static bool parseNetsSection(const String& json, const struct PowerState& oldPower);
    static bool parsePowerSection(const String& json);
    static bool parseGpioSection(const String& json);
    
    // JSON helpers
    static String extractArray(const String& json, const char* key);
    static String extractObject(const String& json, const char* key);
    static String extractString(const String& json, const char* key);
    static float extractFloat(const String& json, const char* key, float defaultVal = 0.0f);
    static int extractInt(const String& json, const char* key, int defaultVal = -1);
    
    // Get next object from array (for iteration)
    static String getNextArrayElement(const String& array, int& startPos);
};

#endif // JSON_STATE_PARSER_H

