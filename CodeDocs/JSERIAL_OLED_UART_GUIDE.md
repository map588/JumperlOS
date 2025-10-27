// File truncated for brevity - creating comprehensive OLED/UART guide
# jSerial OLED & UART Guide

## Overview

jSerial now supports **OLED output** and **enhanced UART capabilities**, making it easy to route serial communication to displays and Arduino devices. This guide covers the new features added to jSerial.

## ✨ New Features

### 1. **OLED Display Output**
- Print text to OLED using small fonts (5pt)
- Automatic line wrapping and scrolling  
- 4-line display support for 128x32 OLED
- Works with all standard print/println/printf functions

### 2. **Arduino Serial1 Support**
- Direct routing to/from Arduino via UART
- Behaves exactly like USB connection
- Full bidirectional communication
- Compatible with Arduino upload protocols

### 3. **Hardware UART Access**
- Direct control of uart0 and uart1
- Configurable baud rate, parity, stop bits
- Hardware flow control support (RTS/CTS)
- Independent from Arduino Serial1

## OLEDStream Class

### Basic Usage

```cpp
#include "OLEDStream.h"

// Global instance available
OLEDOut.println("Hello OLED!");
OLEDOut.printf("Value: %d\n", 42);
```

### Configuration

```cpp
// Set small font
OLEDOut.setSmallFont(SMALL_FONT_PRAGMATISM_5PT);

// Enable/disable auto-update
OLEDOut.setAutoUpdate(false);
OLEDOut.print("Buffered text");
OLEDOut.flush();  // Update now

// Enable/disable scrolling
OLEDOut.setScrollEnabled(true);

// Clear display
OLEDOut.clear();
```

### Available Small Fonts

```cpp
SMALL_FONT_UBUNTU          // Ubuntu 5pt
SMALL_FONT_DOTGOTHIC       // DotGothic 4pt
SMALL_FONT_JOKERMAN        // Jokerman 8pt
SMALL_FONT_ANDALE_MONO     // Andale Mono 5pt (monospace)
SMALL_FONT_IOSEVKA_REGULAR // Iosevka 9pt
SMALL_FONT_IOSEVKA_5PT     // Iosevka 5pt
SMALL_FONT_PRAGMATISM_5PT  // Pragmatism 5pt (default)
SMALL_FONT_FREEMONO_5PT    // FreeMono 5pt
SMALL_FONT_ENVYCODE_5PT    // EnvyCode 5pt
```

### OLEDStream Properties

- **Max Lines**: 4 lines on 128x32 display
- **Line Length**: ~32 characters per line
- **Auto Scrolling**: Enabled by default
- **Auto Update**: Enabled by default
- **Read-only**: OLED is output only (no input)

## UARTStream Class

### Basic Setup

```cpp
#include "UARTStream.h"

// Create UART stream for uart0, TX=GP0, RX=GP1
UARTStream uart0Stream(uart0, 0, 1);

// Initialize with default settings (115200, 8N1)
uart0Stream.begin();

// Use like any Stream
uart0Stream.println("Hello UART!");
```

### Advanced Configuration

```cpp
// Custom baud rate and format
uart0Stream.begin(9600, 8, 1, UART_PARITY_EVEN);

// Change baud rate
uart0Stream.setBaudRate(115200);

// Enable hardware flow control
uart0Stream.setFlowControl(true, 2, 3);  // CTS=GP2, RTS=GP3

// Check status
if (uart0Stream.isWritable()) {
    uart0Stream.println("Ready to send");
}

if (uart0Stream.isReadable()) {
    char c = uart0Stream.read();
}
```

### UART Instances

```cpp
// UART0 (default Arduino Serial1)
UARTStream uart0(uart0, 0, 1);  // GP0=TX, GP1=RX

// UART1 (additional UART)
UARTStream uart1(uart1, 4, 5);  // GP4=TX, GP5=RX
```

## jSerial Integration

### OLED Routing

```cpp
#include "jSerial.h"

jSerial js;

// Single output to OLED
js.setOutputStream(jSerialEndpoint::OLED);
js.println("OLED output only");

// Broadcast to OLED and Serial
js.addOutputStream(jSerialEndpoint::USB_SERIAL);
js.addOutputStream(jSerialEndpoint::OLED);
js.println("Appears on both!");
```

### Serial1 (Arduino UART) Routing

```cpp
// Output to Arduino via UART
js.setOutputStream(jSerialEndpoint::SERIAL1);
js.println("Hello Arduino!");

// Bidirectional communication
js.setInputStream(jSerialEndpoint::SERIAL1);
if (js.available()) {
    String response = js.readStringUntil('\n');
    Serial.println("Arduino: " + response);
}
```

### Direct UART Routing

```cpp
// Create UART stream
UARTStream uart0(uart0, 0, 1);
uart0.begin(115200);

// Add to jSerial
js.addOutputStream(&uart0);
js.setInputStream(&uart0);

// Use normally
js.println("Direct UART access");
```

## Complete Examples

### Example 1: Arduino Bridge with OLED Monitor

```cpp
jSerial bridge;

void setup() {
    Serial.begin(115200);
    Serial1.begin(115200);
    
    // USB <-> UART bridge with OLED monitor
    bridge.setInputStream(jSerialEndpoint::USB_SERIAL);
    bridge.addOutputStream(jSerialEndpoint::SERIAL1);
    bridge.addOutputStream(jSerialEndpoint::OLED);
    
    Serial.println("Arduino Bridge Active");
    Serial.println("OLED showing all traffic");
}

void loop() {
    // Forward USB to UART (visible on OLED)
    if (bridge.available()) {
        while (bridge.available()) {
            char c = bridge.read();
            bridge.write(c);
        }
    }
    
    // Forward UART to USB (also show on OLED)
    if (Serial1.available()) {
        jSerial toUSB;
        toUSB.addOutputStream(&Serial);
        toUSB.addOutputStream(&OLEDOut);
        
        while (Serial1.available()) {
            char c = Serial1.read();
            toUSB.write(c);
        }
    }
}
```

### Example 2: Multi-Channel Debug with OLED

```cpp
jSerial mainPort;
jSerial debugPort;

void setup() {
    Serial.begin(115200);
    
    // Main interface
    mainPort.setOutputStream(&Serial);
    mainPort.setInputStream(&Serial);
    
    // Debug output (broadcast)
    debugPort.addOutputStream(&Serial);
    debugPort.addOutputStream(&USBSer1);
    debugPort.addOutputStream(&OLEDOut);
    
    mainPort.println("System ready");
    debugPort.println("Debug mode active");
}

void loop() {
    // User commands
    if (mainPort.available()) {
        String cmd = mainPort.readStringUntil('\n');
        mainPort.println("Command: " + cmd);
        debugPort.printf("Processing: %s\n", cmd.c_str());
    }
    
    // Periodic debug info (visible everywhere)
    static uint32_t last = 0;
    if (millis() - last > 5000) {
        debugPort.printf("Uptime: %lu\n", millis()/1000);
        last = millis();
    }
}
```

### Example 3: OLED Status Display

```cpp
void updateOLEDStatus() {
    static uint32_t last = 0;
    if (millis() - last < 1000) return;
    last = millis();
    
    jSerial oledOut;
    oledOut.setOutputStream(&OLEDOut);
    
    OLEDOut.clear();
    oledOut.println("System Status");
    oledOut.printf("CPU: %.1f%%\n", getCPUUsage());
    oledOut.printf("Temp: %.1fC\n", getTemperature());
    oledOut.printf("RAM: %d%%\n", getRAMUsage());
}
```

### Example 4: Dual UART Configuration

```cpp
UARTStream uart0(uart0, 0, 1);  // Device A
UARTStream uart1(uart1, 4, 5);  // Device B

jSerial deviceA, deviceB;

void setup() {
    uart0.begin(115200);
    uart1.begin(9600);
    
    deviceA.setOutputStream(&uart0);
    deviceA.setInputStream(&uart0);
    
    deviceB.setOutputStream(&uart1);
    deviceB.setInputStream(&uart1);
    
    deviceA.println("Hello Device A");
    deviceB.println("Hello Device B");
}

void loop() {
    // Handle Device A
    if (deviceA.available()) {
        String msg = deviceA.readStringUntil('\n');
        Serial.println("Device A: " + msg);
    }
    
    // Handle Device B
    if (deviceB.available()) {
        String msg = deviceB.readStringUntil('\n');
        Serial.println("Device B: " + msg);
    }
}
```

## Use Cases

### 1. **Arduino Upload Monitoring**
See Arduino upload traffic in real-time on OLED while it's happening.

```cpp
js.addOutputStream(&Serial1);    // To Arduino
js.addOutputStream(&OLEDOut);    // Monitor
// Watch upload progress on OLED!
```

### 2. **Wireless Debug Display**
Use OLED as a wireless debug display - no need to plug in USB to see status.

```cpp
debugPort.addOutputStream(&OLEDOut);
debugPort.printf("Sensor: %.2f\n", value);
// Check OLED instead of connecting to Serial
```

### 3. **Multi-Device Testing**
Test multiple Arduino/devices simultaneously with visual feedback.

```cpp
device1.addOutputStream(&Serial);
device1.addOutputStream(&OLEDOut);
device2.addOutputStream(&USBSer1);
// See device1 status on OLED, device2 on USBSer1
```

### 4. **Headless Operation**
Run Jumperless without a computer - status visible on OLED.

```cpp
statusDisplay.setOutputStream(&OLEDOut);
statusDisplay.println("Running...");
statusDisplay.printf("Connections: %d\n", count);
```

### 5. **Protocol Debugging**
Monitor serial protocols (UART) with OLED overlay.

```cpp
monitor.addOutputStream(&targetDevice);
monitor.addOutputStream(&OLEDOut);
// See protocol commands on OLED as they're sent
```

## Performance Considerations

### OLED Update Speed
- OLED updates take ~20-50ms for full refresh
- Use `setAutoUpdate(false)` for batch operations
- Call `flush()` when ready to update display

### UART Buffering
- Hardware FIFO is 32 bytes
- Blocking writes may occur if FIFO fills
- Use `availableForWrite()` to check space

### Broadcast Performance
- Writing to multiple outputs is sequential
- Slow endpoints (like OLED) affect all outputs
- Consider broadcast mode control for performance

## API Reference

### OLEDStream Methods

```cpp
// Output
size_t write(uint8_t byte);
size_t print(...);
size_t println(...);
size_t printf(...);
void flush();

// Configuration
void setSmallFont(SmallFont font);
void setAutoUpdate(bool enabled);
void setScrollEnabled(bool enabled);
void clear();

// Query
SmallFont getSmallFont();
bool isAutoUpdate();
bool isScrollEnabled();
int getCurrentLine();
int getCurrentColumn();
bool isConnected();
```

### UARTStream Methods

```cpp
// Initialization
void begin(uint32_t baud = 115200, 
           uint data_bits = 8,
           uint stop_bits = 1,
           uart_parity_t parity = UART_PARITY_NONE);
void end();
void setBaudRate(uint32_t baud);
void setFlowControl(bool enable, uint cts_pin, uint rts_pin);

// Stream Interface
int available();
int read();
int peek();
void flush();
size_t write(uint8_t byte);
size_t write(const uint8_t *buffer, size_t size);
int availableForWrite();

// Status
bool isInitialized();
bool isWritable();
bool isReadable();
uint32_t getBaudRate();
uart_inst_t* getUARTInstance();
```

## Troubleshooting

### OLED Not Showing Text
- Check `oled.isConnected()` returns true
- Verify OLED is initialized with `oled.init()`
- Try calling `OLEDOut.clear()` first
- Check if auto-update is enabled

### UART Not Working
- Verify correct GPIO pins for TX/RX
- Check baud rate matches other device
- Ensure UART is initialized with `.begin()`
- Test with loopback (connect TX to RX)

### Arduino Not Responding
- Check Serial1 baud rate (usually 115200)
- Verify TX/RX pins are correct (GP0, GP1)
- Ensure Arduino code is running
- Try direct Serial1 instead of jSerial first

### Garbled Text on OLED
- Text too long for line - will wrap
- Try smaller font or shorter text
- Check buffer isn't overflowing
- Clear display before writing new content

## Best Practices

1. **OLED Updates**: Use `setAutoUpdate(false)` for multiple prints, then `flush()`
2. **Error Handling**: Always check `isConnected()` before OLED operations
3. **Baud Rates**: Match baud rates between devices (115200 is standard)
4. **Flow Control**: Enable for high-speed UART to prevent data loss
5. **Broadcast Control**: Disable broadcast for time-critical single outputs

## Examples Location

Complete working examples:
- `/CodeDocs/examples/jSerial_oled_uart_example.cpp`
- `/CodeDocs/examples/jSerial_simple_test.cpp`
- `/CodeDocs/examples/jSerial_example.cpp`

## License

SPDX-License-Identifier: MIT

