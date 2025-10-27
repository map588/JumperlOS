// SPDX-License-Identifier: MIT
/**
 * jSerial Usage Examples
 * 
 * This file demonstrates various ways to use jSerial for routing
 * serial communication to different USB endpoints and UART.
 */

#include "jSerial.h"
#include "Colors.h"

// Create jSerial instances for different purposes
jSerial mainSerial;      // Main user interface
jSerial debugSerial;     // Debug output
jSerial logSerial;       // Data logging

// ============================================================================
// Example 1: Basic Setup - Single Output
// ============================================================================

void example1_basic_setup() {
    // Simple drop-in replacement for Serial
    mainSerial.setOutputStream(&Serial);
    mainSerial.setInputStream(&Serial);
    
    mainSerial.println("Hello from jSerial!");
    mainSerial.printf("System uptime: %lu ms\n", millis());
}

// ============================================================================
// Example 2: Broadcast Mode - Multiple Outputs
// ============================================================================

void example2_broadcast_mode() {
    // Send output to multiple USB endpoints simultaneously
    mainSerial.clearOutputStreams();
    mainSerial.addOutputStream(&Serial);
    mainSerial.addOutputStream(&USBSer1);
    mainSerial.addOutputStream(&USBSer2);
    
    // This message appears on all three endpoints!
    mainSerial.println("Broadcasting to Serial, USBSer1, and USBSer2");
    
    // Input still comes from main Serial
    mainSerial.setInputStream(&Serial);
}

// ============================================================================
// Example 3: Using Endpoint Enums
// ============================================================================

void example3_endpoint_enums() {
    // Cleaner syntax using endpoint enums
    debugSerial.addOutputStream(jSerialEndpoint::USB_SERIAL);
    debugSerial.addOutputStream(jSerialEndpoint::USB_SER1);
    debugSerial.setInputStream(jSerialEndpoint::USB_SERIAL);
    
    debugSerial.println("Using endpoint enums is cleaner!");
}

// ============================================================================
// Example 4: Dynamic Routing
// ============================================================================

void example4_dynamic_routing() {
    // Switch routing at runtime based on conditions
    
    // Normal mode - route to main Serial
    mainSerial.setOutputStream(jSerialEndpoint::USB_SERIAL);
    mainSerial.println("Normal operation");
    
    // Enter debug mode - route to debug port
    mainSerial.setOutputStream(jSerialEndpoint::USB_SER1);
    mainSerial.println("Debug mode active");
    
    // Back to normal
    mainSerial.setOutputStream(jSerialEndpoint::USB_SERIAL);
    mainSerial.println("Back to normal operation");
}

// ============================================================================
// Example 5: Multi-Channel Debug System
// ============================================================================

void example5_multi_channel_debug() {
    // User interface on main Serial
    mainSerial.setOutputStream(&Serial);
    mainSerial.setInputStream(&Serial);
    
    // Debug output broadcasts to Serial and USBSer1
    debugSerial.clearOutputStreams();
    debugSerial.addOutputStream(&Serial);
    debugSerial.addOutputStream(&USBSer1);
    
    // User sees this
    mainSerial.println("System Status: OK");
    
    // Debug output goes to both endpoints
    debugSerial.printf("DEBUG: Free memory: %d bytes\n", 123456);
    debugSerial.printf("DEBUG: Temperature: %.2f C\n", 23.5);
}

// ============================================================================
// Example 6: Data Logging
// ============================================================================

void example6_data_logging() {
    // Log all I/O to a dedicated endpoint
    logSerial.addOutputStream(&Serial);     // Normal operation
    logSerial.addOutputStream(&USBSer2);    // Logging port
    logSerial.setInputStream(&Serial);
    
    // All communication is automatically logged to USBSer2
    logSerial.println("Command: STATUS");
    logSerial.println("Response: OK");
    // Both messages appear on Serial and USBSer2
}

// ============================================================================
// Example 7: Selective Broadcasting
// ============================================================================

void example7_selective_broadcast() {
    mainSerial.addOutputStream(&Serial);
    mainSerial.addOutputStream(&USBSer1);
    
    // Enable broadcast mode
    mainSerial.setBroadcastMode(true);
    mainSerial.println("This goes to all endpoints");
    
    // Disable broadcast (only first endpoint)
    mainSerial.setBroadcastMode(false);
    mainSerial.println("This goes to Serial only");
    
    // Re-enable broadcast
    mainSerial.setBroadcastMode(true);
    mainSerial.println("Broadcasting again");
}

// ============================================================================
// Example 8: Working with Colors
// ============================================================================

void example8_colored_output() {
    mainSerial.setOutputStream(&Serial);
    
    // Works seamlessly with existing color functions!
    // Just cast to Stream* when needed
    changeTerminalColor(79, true, (Stream*)&mainSerial);
    mainSerial.println("Colored text!");
    changeTerminalColor(-1, false, (Stream*)&mainSerial);
    
    // Or use directly with cycleTerminalColor
    cycleTerminalColor(true, 5.0, true, (Stream*)&mainSerial, 0, 0);
    mainSerial.println("Rainbow text!");
}

// ============================================================================
// Example 9: Command Routing Based on Source
// ============================================================================

void example9_command_routing() {
    // Check which endpoint has data and route accordingly
    
    if (Serial.available()) {
        mainSerial.setInputStream(&Serial);
        String cmd = mainSerial.readStringUntil('\n');
        mainSerial.println("Main: " + cmd);
    }
    
    if (USBSer1.available()) {
        mainSerial.setInputStream(&USBSer1);
        String cmd = mainSerial.readStringUntil('\n');
        mainSerial.setOutputStream(&USBSer1);
        mainSerial.println("Debug: " + cmd);
    }
}

// ============================================================================
// Example 10: Querying Endpoint State
// ============================================================================

void example10_query_state() {
    mainSerial.setOutputStream(&Serial);
    
    // Check how many outputs are registered
    mainSerial.printf("Output count: %d\n", mainSerial.getOutputCount());
    
    // Check if a specific stream is registered
    if (mainSerial.hasOutputStream(&Serial)) {
        mainSerial.println("Serial is registered as output");
    }
    
    // Check broadcast mode
    if (mainSerial.isBroadcastMode()) {
        mainSerial.println("Broadcast mode is enabled");
    }
    
    // Get endpoint name
    jSerialEndpoint ep = jSerial::getEndpointType(&Serial);
    mainSerial.printf("Serial is: %s\n", jSerial::endpointToString(ep));
}

// ============================================================================
// Example 11: Error Handling and Validation
// ============================================================================

void example11_error_handling() {
    mainSerial.setOutputStream(&Serial);
    
    // Try to add an output
    if (mainSerial.addOutputStream(&USBSer1)) {
        mainSerial.println("Successfully added USBSer1");
    } else {
        mainSerial.println("Failed to add USBSer1 (maybe already added?)");
    }
    
    // Try to remove an output
    if (mainSerial.removeOutputStream(&USBSer1)) {
        mainSerial.println("Successfully removed USBSer1");
    } else {
        mainSerial.println("Failed to remove USBSer1 (maybe not registered?)");
    }
}

// ============================================================================
// Example 12: Integration with TermControl
// ============================================================================

#include "TermControl.h"

void example12_termcontrol_integration() {
    // jSerial can be wrapped by TermControl!
    mainSerial.setOutputStream(&Serial);
    mainSerial.setInputStream(&Serial);
    
    // Create TermControl wrapper
    TermControl term((Stream*)&mainSerial);
    
    // Now you get line buffering + routing!
    term.setPrompt("$ ");
    term.service();
    
    if (term.hasCompletedLine()) {
        String cmd = term.getCompletedLine();
        mainSerial.println("Command: " + cmd);
    }
}

// ============================================================================
// Example 13: Pass-through with Monitoring
// ============================================================================

void example13_passthrough_monitor() {
    // Set up pass-through from Serial to USBSer1 with monitoring
    
    // Input from Serial
    mainSerial.setInputStream(&Serial);
    
    // Output to USBSer1 and log to USBSer2
    mainSerial.addOutputStream(&USBSer1);
    mainSerial.addOutputStream(&USBSer2);  // Monitor here
    
    // Everything is passed through and logged
    if (mainSerial.available()) {
        char c = mainSerial.read();
        mainSerial.write(c);  // Forwarded to USBSer1 and logged to USBSer2
    }
}

// ============================================================================
// Example 14: Conditional Output Routing
// ============================================================================

bool debugMode = false;
bool verboseMode = false;

void example14_conditional_routing() {
    mainSerial.clearOutputStreams();
    mainSerial.addOutputStream(&Serial);  // Always output to main Serial
    
    // Add additional outputs based on mode
    if (debugMode) {
        mainSerial.addOutputStream(&USBSer1);  // Debug port
    }
    
    if (verboseMode) {
        mainSerial.addOutputStream(&USBSer2);  // Verbose logging port
    }
    
    mainSerial.println("This message routing depends on current mode");
}

// ============================================================================
// Example 15: Complete Application Setup
// ============================================================================

void setup() {
    // Initialize all USB endpoints
    Serial.begin(115200);
    USBSer1.begin(115200);
    USBSer2.begin(115200);
    USBSer3.begin(115200);
    
    // Set up main serial for user interface
    mainSerial.setOutputStream(&Serial);
    mainSerial.setInputStream(&Serial);
    
    // Set up debug serial (broadcasts to Serial and USBSer1)
    debugSerial.addOutputStream(&Serial);
    debugSerial.addOutputStream(&USBSer1);
    
    // Set up log serial (only to USBSer2)
    logSerial.setOutputStream(&USBSer2);
    
    mainSerial.println("jSerial System Initialized");
    debugSerial.println("DEBUG: All endpoints configured");
    logSerial.println("LOG: System started");
}

void loop() {
    // Handle user input from main Serial
    if (mainSerial.available()) {
        String cmd = mainSerial.readStringUntil('\n');
        cmd.trim();
        
        if (cmd == "debug on") {
            debugMode = true;
            mainSerial.println("Debug mode enabled");
        } else if (cmd == "debug off") {
            debugMode = false;
            mainSerial.println("Debug mode disabled");
        } else if (cmd == "status") {
            mainSerial.printf("Outputs: %d, Broadcast: %s\n",
                            mainSerial.getOutputCount(),
                            mainSerial.isBroadcastMode() ? "ON" : "OFF");
        } else {
            mainSerial.println("Unknown command: " + cmd);
        }
        
        // Log the command
        logSerial.printf("CMD: %s\n", cmd.c_str());
    }
    
    // Debug output
    if (debugMode) {
        static uint32_t lastDebug = 0;
        if (millis() - lastDebug > 5000) {
            debugSerial.printf("DEBUG: Uptime %lu ms\n", millis());
            lastDebug = millis();
        }
    }
}

