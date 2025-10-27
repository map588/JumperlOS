// Serial Input Tag Stripping Example
// 
// This example demonstrates how TermControl automatically strips <j> and </j> tags
// from incoming USB serial data to prevent weird behavior when commands are wrapped in tags.

#include "TermControl.h"

extern TermControl termSerial;

void setup() {
    Serial.begin(115200);
    
    // Enable automatic tag stripping for incoming commands
    // When enabled, <j> and </j> tags are transparently removed from input
    // This is already enabled by default in main.cpp, but shown here for reference
    termSerial.setAutoStripTags(true);
    
    Serial.println("Tag stripping enabled!");
    Serial.println("Try sending: <j>+ 1-2</j>");
}

void loop() {
    // The tag stripping happens automatically in termSerial.service()
    // which is called by the main loop in JumperlOS
    
    // When you read from Serial or termSerial, tags are already stripped
    // Your existing command parsing code works unchanged
    
    if (Serial.available()) {
        char c = Serial.read();
        
        // This will receive the command WITHOUT the tags
        // Input:  "<j>+ 1-2</j>"
        // Output: "+ 1-2"
        
        Serial.print("Received: ");
        Serial.println(c);
    }
}

// ============================================================================
// Example Input/Output
// ============================================================================
//
// When auto strip tags is ENABLED:
//   Send:    <j>+ 1-2</j>
//   Receive: + 1-2
//
//   Send:    <j>- 1-2</j>
//   Receive: - 1-2
//
//   Send:    <j>c</j>
//   Receive: c
//
// When auto strip tags is DISABLED:
//   Send:    <j>+ 1-2</j>
//   Receive: <j>+ 1-2</j>  (tags preserved)
//
// ============================================================================
// Disabling Tag Stripping
// ============================================================================
//
// If you need to receive tags as-is (for debugging):
//
//   termSerial.setAutoStripTags(false);  // Disable automatic stripping
//
// Re-enable:
//
//   termSerial.setAutoStripTags(true);   // Enable for all input
//
// ============================================================================
// Edge Cases Handled Correctly
// ============================================================================
//
// 1. Incomplete tags are not treated as tags:
//    Input: "<j+ 1-2"    → Output: "<j+ 1-2"  (no closing >)
//    Input: "< j>+ 1-2"  → Output: "< j>+ 1-2"  (space in tag)
//
// 2. Multiple tags in sequence:
//    Input: "<j>a</j><j>b</j>" → Output: "ab"
//
// 3. Nested or malformed tags:
//    Input: "<<j>test</j>" → Output: "<test"  (first < not part of tag)
//
// 4. Only <j> and </j> are stripped, other tags are preserved:
//    Input: "<j>test</j><other>tag</other>" → Output: "test<other>tag</other>"
//

