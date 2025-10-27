// SPDX-License-Identifier: MIT
/**
 * jSerial OLED and UART Examples
 * 
 * Demonstrates the new OLED and UART routing capabilities of jSerial
 */

#include "jSerial.h"
#include "OLEDStream.h"
#include "UARTStream.h"

// Create jSerial instances
jSerial mainSerial;
jSerial debugSerial;

// ============================================================================
// Example 1: Output to OLED Display
// ============================================================================

void example1_oled_output() {
    // Route output to OLED
    mainSerial.setOutputStream(jSerialEndpoint::OLED);
    
    // Print to OLED using small text
    mainSerial.println("Hello OLED!");
    mainSerial.printf("Uptime: %lu\n", millis());
    mainSerial.println("Line 3");
    mainSerial.println("Line 4");
    
    // Text automatically scrolls and uses small font
}

// ============================================================================
// Example 2: Broadcast to OLED and Serial
// ============================================================================

void example2_oled_and_serial() {
    // Output to both Serial and OLED simultaneously
    mainSerial.addOutputStream(jSerialEndpoint::USB_SERIAL);
    mainSerial.addOutputStream(jSerialEndpoint::OLED);
    
    // This appears on both Serial terminal and OLED
    mainSerial.println("Status: System OK");
    mainSerial.printf("Temperature: %.1f C\n", 23.5);
    
    // User sees it on their terminal AND on the OLED!
}

// ============================================================================
// Example 3: UART Passthrough with OLED Monitor
// ============================================================================

void example3_uart_with_oled_monitor() {
    // Input from Serial, output to UART and OLED
    mainSerial.setInputStream(jSerialEndpoint::USB_SERIAL);
    mainSerial.addOutputStream(jSerialEndpoint::SERIAL1);  // To Arduino
    mainSerial.addOutputStream(jSerialEndpoint::OLED);     // Monitor on OLED
    
    // Everything sent to Arduino is shown on OLED
    if (mainSerial.available()) {
        String cmd = mainSerial.readStringUntil('\n');
        mainSerial.println("Sent: " + cmd);
        // Appears on OLED and goes to Arduino via UART
    }
}

// ============================================================================
// Example 4: Arduino UART Communication
// ============================================================================

void example4_arduino_uart() {
    // Set up Serial1 for Arduino communication
    mainSerial.setOutputStream(jSerialEndpoint::SERIAL1);
    mainSerial.setInputStream(jSerialEndpoint::SERIAL1);
    
    // Now Arduino on UART behaves exactly like USB connection!
    mainSerial.println("AT");  // Send to Arduino
    
    if (mainSerial.available()) {
        String response = mainSerial.readStringUntil('\n');
        Serial.println("Arduino says: " + response);
    }
}

// ============================================================================
// Example 5: Direct Hardware UART Access
// ============================================================================

void example5_direct_uart() {
    // Create a UARTStream for direct hardware access
    static UARTStream uart0Stream(uart0, 0, 1);  // GP0=TX, GP1=RX
    uart0Stream.begin(115200);
    
    // Add to jSerial routing
    mainSerial.addOutputStream(&uart0Stream);
    mainSerial.setInputStream(&uart0Stream);
    
    // Full control over hardware UART
    mainSerial.println("Direct UART output");
}

// ============================================================================
// Example 6: Multi-Channel Debug System with OLED
// ============================================================================

void example6_multi_channel_with_oled() {
    // User interface on Serial
    mainSerial.setOutputStream(jSerialEndpoint::USB_SERIAL);
    mainSerial.setInputStream(jSerialEndpoint::USB_SERIAL);
    
    // Debug output to Serial, USBSer1, and OLED
    debugSerial.addOutputStream(jSerialEndpoint::USB_SERIAL);
    debugSerial.addOutputStream(jSerialEndpoint::USB_SER1);
    debugSerial.addOutputStream(jSerialEndpoint::OLED);
    
    // User commands
    mainSerial.println("Enter command:");
    
    // Debug info visible everywhere
    debugSerial.printf("DEBUG: Free RAM: %d\n", 123456);
    // ^ Appears on Serial terminal, USBSer1, AND OLED!
}

// ============================================================================
// Example 7: OLED Status Display
// ============================================================================

void example7_oled_status_display() {
    jSerial oledOut;
    oledOut.setOutputStream(jSerialEndpoint::OLED);
    
    // Clear and show status
    OLEDOut.clear();
    oledOut.println("System Status:");
    oledOut.println("CPU: OK");
    oledOut.printf("Temp: %.1f C\n", 25.3);
    oledOut.printf("RAM: %d%%\n", 75);
    
    // OLED shows a nice status display!
}

// ============================================================================
// Example 8: UART Echo with Serial Monitor
// ============================================================================

void example8_uart_echo_monitor() {
    jSerial uartPort;
    jSerial monitor;
    
    // UART bidirectional
    uartPort.setOutputStream(jSerialEndpoint::SERIAL1);
    uartPort.setInputStream(jSerialEndpoint::SERIAL1);
    
    // Monitor on Serial and OLED
    monitor.addOutputStream(jSerialEndpoint::USB_SERIAL);
    monitor.addOutputStream(jSerialEndpoint::OLED);
    
    // Echo from UART to monitor
    if (uartPort.available()) {
        char c = uartPort.read();
        monitor.printf("UART RX: %c\n", c);
        uartPort.write(c);  // Echo back
    }
}

// ============================================================================
// Example 9: OLEDStream Direct Usage
// ============================================================================

void example9_oledstream_direct() {
    // Use OLEDStream directly without jSerial
    OLEDOut.clear();
    OLEDOut.setSmallFont(SMALL_FONT_PRAGMATISM_5PT);
    OLEDOut.setScrollEnabled(true);
    OLEDOut.setAutoUpdate(true);
    
    // Print like any Stream
    OLEDOut.println("Direct OLED");
    OLEDOut.printf("Value: %d\n", 42);
    
    // Fine-grained control
    OLEDOut.setAutoUpdate(false);
    OLEDOut.print("Buffered");
    OLEDOut.flush();  // Update now
}

// ============================================================================
// Example 10: UARTStream Configuration
// ============================================================================

void example10_uart_configuration() {
    UARTStream uart0Stream(uart0, 0, 1);
    
    // Custom configuration
    uart0Stream.begin(9600, 8, 1, UART_PARITY_EVEN);
    
    // Use with jSerial
    jSerial js;
    js.addOutputStream(&uart0Stream);
    js.println("Custom UART config");
    
    // Change baud rate on the fly
    uart0Stream.setBaudRate(115200);
    
    // Enable hardware flow control
    uart0Stream.setFlowControl(true, 2, 3);  // CTS=GP2, RTS=GP3
}

// ============================================================================
// Example 11: Arduino Upload Monitor on OLED
// ============================================================================

void example11_arduino_upload_monitor() {
    jSerial arduinoPort;
    
    // Route Arduino Serial1 to both Serial and OLED
    arduinoPort.addOutputStream(jSerialEndpoint::USB_SERIAL);
    arduinoPort.addOutputStream(jSerialEndpoint::OLED);
    arduinoPort.setInputStream(jSerialEndpoint::USB_SERIAL);
    
    // All communication during upload is visible on OLED
    // Perfect for debugging Arduino uploads!
}

// ============================================================================
// Example 12: Multi-UART Setup
// ============================================================================

void example12_multi_uart() {
    // Two independent UART connections
    static UARTStream uart0Stream(uart0, 0, 1);
    static UARTStream uart1Stream(uart1, 4, 5);  // Different pins
    
    uart0Stream.begin(115200);
    uart1Stream.begin(9600);
    
    jSerial port1, port2;
    
    port1.setOutputStream(&uart0Stream);
    port1.setInputStream(&uart0Stream);
    
    port2.setOutputStream(&uart1Stream);
    port2.setInputStream(&uart1Stream);
    
    // Two independent serial ports!
    port1.println("UART0 message");
    port2.println("UART1 message");
}

// ============================================================================
// Example 13: Complete Arduino Bridge with OLED Display
// ============================================================================

jSerial arduinoBridge;
jSerial oledMonitor;

void setup() {
    Serial.begin(115200);
    Serial1.begin(115200);
    
    // Bridge: USB <-> UART
    arduinoBridge.setInputStream(jSerialEndpoint::USB_SERIAL);
    arduinoBridge.addOutputStream(jSerialEndpoint::SERIAL1);
    
    // Also send to OLED for monitoring
    arduinoBridge.addOutputStream(jSerialEndpoint::OLED);
    
    // Separate OLED status display
    oledMonitor.setOutputStream(jSerialEndpoint::OLED);
    
    Serial.println("Arduino Bridge Active");
    Serial.println("OLED showing all traffic");
    
    oledMonitor.clear();
    oledMonitor.println("Bridge Ready");
}

void loop() {
    // USB -> UART (with OLED display)
    if (arduinoBridge.available()) {
        while (arduinoBridge.available()) {
            char c = arduinoBridge.read();
            arduinoBridge.write(c);
        }
    }
    
    // UART -> USB (with OLED display)
    if (Serial1.available()) {
        jSerial toUSB;
        toUSB.addOutputStream(jSerialEndpoint::USB_SERIAL);
        toUSB.addOutputStream(jSerialEndpoint::OLED);
        
        while (Serial1.available()) {
            char c = Serial1.read();
            toUSB.write(c);
        }
    }
    
    // Periodic status on OLED
    static uint32_t lastUpdate = 0;
    if (millis() - lastUpdate > 5000) {
        oledMonitor.printf("Up: %lu s\n", millis() / 1000);
        lastUpdate = millis();
    }
}

// ============================================================================
// Example 14: OLED Logger
// ============================================================================

void example14_oled_logger() {
    jSerial logger;
    
    // Log everything to OLED and file
    logger.addOutputStream(jSerialEndpoint::OLED);
    logger.addOutputStream(jSerialEndpoint::USB_SER2);  // Log port
    
    logger.printf("[%lu] System started\n", millis());
    logger.printf("[%lu] Sensor: %.2f\n", millis(), 23.45);
    logger.printf("[%lu] Error: XYZ\n", millis());
    
    // OLED shows scrolling log, USBSer2 captures everything
}

// ============================================================================
// Example 15: Conditional Routing with OLED
// ============================================================================

bool showOnOLED = true;
bool debugMode = false;

void example15_conditional_routing() {
    mainSerial.clearOutputStreams();
    mainSerial.addOutputStream(jSerialEndpoint::USB_SERIAL);
    
    // Add OLED if enabled
    if (showOnOLED) {
        mainSerial.addOutputStream(jSerialEndpoint::OLED);
    }
    
    // Add debug port if in debug mode
    if (debugMode) {
        mainSerial.addOutputStream(jSerialEndpoint::USB_SER1);
    }
    
    mainSerial.println("Flexible routing!");
}

