# jSerial - Advanced Serial Routing Library

## Overview

`jSerial` is a drop-in replacement for Arduino's `Serial` class that provides advanced routing capabilities. It allows you to:

- **Broadcast output** to multiple USB CDC endpoints simultaneously
- **Route input** from a specific endpoint
- **Dynamically switch** between different serial interfaces
- Maintain **full compatibility** with existing Serial-based code

## Features

- ✅ Full `Stream` interface compatibility
- ✅ Support for `print()`, `println()`, `printf()`, `write()`
- ✅ Broadcast mode - write to multiple endpoints simultaneously
- ✅ Input routing - read from a specific source
- ✅ Support for USB CDC (Serial, USBSer1, USBSer2, USBSer3)
- ✅ Easy-to-use API with endpoint enums
- ✅ Dynamic routing - change endpoints at runtime

## Quick Start

### Basic Usage

```cpp
#include "jSerial.h"

// Create a jSerial instance
jSerial js;

void setup() {
    // Route to main Serial USB
    js.setOutputStream(&Serial);
    js.setInputStream(&Serial);
    
    // Now use it like Serial
    js.println("Hello, World!");
}

void loop() {
    if (js.available()) {
        char c = js.read();
        js.print("You typed: ");
        js.println(c);
    }
}
```

### Broadcast Mode

Write to multiple USB endpoints simultaneously:

```cpp
#include "jSerial.h"

jSerial js;

void setup() {
    // Add multiple output streams
    js.addOutputStream(&Serial);    // USB CDC 0
    js.addOutputStream(&USBSer1);   // USB CDC 1
    js.addOutputStream(&USBSer2);   // USB CDC 2
    
    // Set input to main Serial
    js.setInputStream(&Serial);
    
    // This will be sent to all three endpoints!
    js.println("Broadcasting to all endpoints!");
}
```

### Using Endpoint Enums

For cleaner code, use the built-in endpoint enums:

```cpp
void setup() {
    // Using endpoint enums instead of stream pointers
    js.addOutputStream(jSerialEndpoint::USB_SERIAL);
    js.addOutputStream(jSerialEndpoint::USB_SER1);
    js.addOutputStream(jSerialEndpoint::USB_SER2);
    
    js.setInputStream(jSerialEndpoint::USB_SERIAL);
}
```

### Dynamic Routing

Change routing at runtime:

```cpp
void switchToDebugPort() {
    // Clear current outputs
    js.clearOutputStreams();
    
    // Route to debug port
    js.setOutputStream(jSerialEndpoint::USB_SER1);
    js.println("Now on debug port!");
}

void switchToMainPort() {
    js.setOutputStream(jSerialEndpoint::USB_SERIAL);
    js.println("Back on main port!");
}
```

### Selective Broadcasting

Control when to broadcast:

```cpp
// Enable broadcast mode
js.setBroadcastMode(true);
js.println("Goes to all endpoints");

// Disable broadcast (only goes to first output)
js.setBroadcastMode(false);
js.println("Goes to first endpoint only");
```

## API Reference

### Output Routing Methods

```cpp
// Add an output stream
bool addOutputStream(Stream* stream);
bool addOutputStream(jSerialEndpoint endpoint);

// Remove an output stream
bool removeOutputStream(Stream* stream);
bool removeOutputStream(jSerialEndpoint endpoint);

// Clear all output streams
void clearOutputStreams();

// Set single output (clears existing and adds one)
void setOutputStream(Stream* stream);
void setOutputStream(jSerialEndpoint endpoint);

// Query output state
int getOutputCount() const;
bool hasOutputStream(Stream* stream) const;
```

### Input Routing Methods

```cpp
// Set input stream
void setInputStream(Stream* stream);
void setInputStream(jSerialEndpoint endpoint);

// Get current input stream
Stream* getInputStream() const;
```

### Stream Interface Methods

All standard Arduino Stream methods are supported:

```cpp
// Read operations
int available();
int read();
int peek();

// Write operations
size_t write(uint8_t byte);
size_t write(const uint8_t *buffer, size_t size);
void flush();
int availableForWrite();

// Print operations (inherited from Stream/Print)
size_t print(...);
size_t println(...);
size_t printf(...);
```

### Utility Methods

```cpp
// Enable/disable broadcast mode
void setBroadcastMode(bool enabled);
bool isBroadcastMode() const;

// Static helpers
static jSerialEndpoint getEndpointType(Stream* stream);
static Stream* getStreamForEndpoint(jSerialEndpoint endpoint);
static const char* endpointToString(jSerialEndpoint endpoint);
```

## Available Endpoints

```cpp
enum class jSerialEndpoint {
    NONE,           // No endpoint
    USB_SERIAL,     // Main Serial (USB CDC 0)
    USB_SER1,       // USBSer1 (USB CDC 1)
    USB_SER2,       // USBSer2 (USB CDC 2)
    USB_SER3,       // USBSer3 (USB CDC 3)
    UART0,          // Hardware UART0 (future support)
    UART1,          // Hardware UART1 (future support)
    CUSTOM          // Custom stream pointer
};
```

## Advanced Examples

### Multi-Channel Debug System

```cpp
jSerial debugPort;
jSerial userPort;

void setup() {
    // User interactions on main Serial
    userPort.setOutputStream(&Serial);
    userPort.setInputStream(&Serial);
    
    // Debug output broadcasts to Serial and USBSer1
    debugPort.addOutputStream(&Serial);
    debugPort.addOutputStream(&USBSer1);
    
    userPort.println("User interface ready");
    debugPort.println("Debug output active on Serial and USBSer1");
}

void loop() {
    // User sees normal output
    userPort.println("Status: OK");
    
    // Debug info goes to debug channels
    debugPort.printf("Debug: Free RAM: %d\n", getFreeRam());
}
```

### Pass-through with Logging

```cpp
jSerial logger;

void setup() {
    // Log all communication to USBSer2
    logger.addOutputStream(&Serial);    // Normal operation
    logger.addOutputStream(&USBSer2);   // Logging port
    logger.setInputStream(&Serial);
}

void loop() {
    // All I/O is automatically logged to USBSer2
    if (logger.available()) {
        String cmd = logger.readStringUntil('\n');
        logger.println("Command received: " + cmd);
        // Appears on both Serial and USBSer2
    }
}
```

### Color Output with Stream Routing

```cpp
#include "Colors.h"

void colorfulOutput() {
    jSerial js;
    js.setOutputStream(&Serial);
    
    // Works with existing color functions!
    changeTerminalColor(79, true, &js);
    js.println("Colored text!");
    changeTerminalColor(-1, false, &js);
}
```

## Integration with Existing Code

### Replacing Serial

To convert existing code, simply replace `Serial` with your `jSerial` instance:

```cpp
// Before:
Serial.begin(115200);
Serial.println("Hello");
if (Serial.available()) {
    char c = Serial.read();
}

// After:
jSerial js;
js.setOutputStream(&Serial);
js.setInputStream(&Serial);
js.println("Hello");
if (js.available()) {
    char c = js.read();
}
```

### Using with TermControl

```cpp
jSerial js;
js.setOutputStream(&Serial);

// TermControl can wrap jSerial!
TermControl term(&js);
```

## Performance Considerations

- **Broadcast mode**: Writing to multiple endpoints is sequential, not parallel
- **Buffering**: Each endpoint has its own buffer managed by the underlying Stream
- **Blocking**: If one endpoint blocks, it may affect others in broadcast mode
- **Memory**: Very low overhead - only stores stream pointers (no data buffering)

## Limitations

- Maximum of 8 output streams (configurable via `JSERIAL_MAX_OUTPUTS`)
- Only one input stream at a time
- UART support requires additional wrapper implementation
- No automatic flow control between endpoints

## Future Enhancements

Potential additions:
- UART wrapper for hardware serial
- Async/non-blocking broadcast mode
- Per-endpoint filtering/routing rules
- Automatic endpoint detection
- Statistics and monitoring

## Example Use Cases

1. **Development & Debugging**: Output to multiple terminals simultaneously
2. **Data Logging**: Log all communication to a dedicated endpoint
3. **Protocol Analysis**: Mirror traffic to analysis tools
4. **Multi-User Access**: Different users on different USB endpoints
5. **Fallback Routing**: Switch to backup endpoint if primary fails
6. **Testing**: Verify output across multiple endpoints

## Thread Safety

⚠️ **Warning**: `jSerial` is **not** thread-safe. Do not call methods from multiple cores/threads without external synchronization.

## License

SPDX-License-Identifier: MIT

