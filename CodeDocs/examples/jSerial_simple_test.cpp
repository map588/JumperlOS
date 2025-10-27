// SPDX-License-Identifier: MIT
/**
 * Simple jSerial Test
 * 
 * A minimal example to test jSerial compilation and basic functionality
 */

#include "jSerial.h"

// Create a global jSerial instance
jSerial js;

void setup() {
    // Initialize USB endpoints
    Serial.begin(115200);
    
    // Wait for Serial to connect (optional)
    delay(1000);
    
    // Configure jSerial to route to main Serial
    js.setOutputStream(&Serial);
    js.setInputStream(&Serial);
    
    // Test basic output
    js.println("=================================");
    js.println("jSerial Simple Test");
    js.println("=================================");
    js.println();
    
    // Test various output methods
    js.print("Print test: ");
    js.println("OK");
    
    js.printf("Printf test: %d + %d = %d\n", 2, 2, 4);
    
    js.println();
    js.println("Type something and press Enter:");
}

void loop() {
    // Echo back any input
    if (js.available()) {
        char c = js.read();
        
        // Echo the character
        js.print("You typed: ");
        js.println(c);
        
        // Show ASCII value
        js.printf("ASCII value: %d\n", (int)c);
        js.println();
    }
    
    // Send periodic status updates
    static uint32_t lastStatus = 0;
    if (millis() - lastStatus > 10000) {
        js.printf("Status: Running (uptime: %lu ms)\n", millis());
        lastStatus = millis();
    }
}

