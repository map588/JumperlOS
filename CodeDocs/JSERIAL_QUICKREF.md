# jSerial Quick Reference

## 🚀 Quick Start

```cpp
#include "jSerial.h"

jSerial js;

void setup() {
    // Route to main Serial
    js.setOutputStream(&Serial);
    js.setInputStream(&Serial);
    js.println("Hello from jSerial!");
}
```

## 📝 Common Patterns

### Single Output
```cpp
js.setOutputStream(&Serial);
```

### Broadcast to Multiple Outputs
```cpp
js.addOutputStream(&Serial);
js.addOutputStream(&USBSer1);
js.addOutputStream(&USBSer2);
// Now all writes go to all three!
```

### Using Endpoint Enums
```cpp
js.addOutputStream(jSerialEndpoint::USB_SERIAL);
js.addOutputStream(jSerialEndpoint::USB_SER1);
```

### Set Input Source
```cpp
js.setInputStream(&Serial);
```

### Dynamic Switching
```cpp
// Switch to debug port
js.setOutputStream(jSerialEndpoint::USB_SER1);
js.println("Now on debug port");

// Back to main
js.setOutputStream(jSerialEndpoint::USB_SERIAL);
```

## 🔧 Key Methods

| Method | Description |
|--------|-------------|
| `addOutputStream()` | Add an output stream for broadcasting |
| `removeOutputStream()` | Remove a specific output stream |
| `setOutputStream()` | Clear and set single output |
| `clearOutputStreams()` | Remove all outputs |
| `setInputStream()` | Set input source |
| `print()`, `println()`, `printf()` | Standard output methods |
| `read()`, `available()`, `peek()` | Standard input methods |
| `flush()` | Flush all output streams |
| `setBroadcastMode()` | Enable/disable broadcasting |
| `getOutputCount()` | Get number of active outputs |
| `hasOutputStream()` | Check if stream is registered |

## 🎯 Available Endpoints

```cpp
jSerialEndpoint::NONE
jSerialEndpoint::USB_SERIAL   // Main Serial (CDC 0)
jSerialEndpoint::USB_SER1     // USBSer1 (CDC 1)
jSerialEndpoint::USB_SER2     // USBSer2 (CDC 2)
jSerialEndpoint::USB_SER3     // USBSer3 (CDC 3)
jSerialEndpoint::SERIAL1      // Arduino Serial1 (UART) ✨ NEW
jSerialEndpoint::UART0        // Direct hardware UART0 (via UARTStream)
jSerialEndpoint::UART1        // Direct hardware UART1 (via UARTStream)
jSerialEndpoint::OLED         // OLED display output ✨ NEW
jSerialEndpoint::CUSTOM       // Custom stream
```

## 💡 Use Cases

### OLED Output ✨ NEW
```cpp
js.addOutputStream(jSerialEndpoint::OLED);
js.println("Visible on OLED!");
js.printf("Temp: %.1f C\n", 23.5);
```

### Arduino UART Bridge ✨ NEW
```cpp
js.setOutputStream(jSerialEndpoint::SERIAL1);
js.println("Hello Arduino!");
// Arduino on UART behaves exactly like USB!
```

### OLED + Serial Monitor ✨ NEW
```cpp
js.addOutputStream(&Serial);
js.addOutputStream(&OLEDOut);
js.println("Appears on both!");
```

### Debug System
```cpp
debugPort.addOutputStream(&Serial);
debugPort.addOutputStream(&USBSer1);
debugPort.addOutputStream(&OLEDOut);  // ✨ NEW
debugPort.println("Debug info everywhere");
```

### UART Passthrough with OLED Monitor ✨ NEW
```cpp
js.setInputStream(&Serial);
js.addOutputStream(&Serial1);     // To Arduino
js.addOutputStream(&OLEDOut);     // Monitor
// See all traffic on OLED!
```

### Data Logging
```cpp
logger.addOutputStream(&Serial);     // Normal
logger.addOutputStream(&USBSer2);    // Log mirror
logger.addOutputStream(&OLEDOut);    // ✨ Visual monitor
```

### Direct Hardware UART ✨ NEW
```cpp
UARTStream uart0(uart0, 0, 1);
uart0.begin(115200);
js.addOutputStream(&uart0);
js.println("Direct UART access");
```

## 🔍 Query State

```cpp
// How many outputs?
int count = js.getOutputCount();

// Is stream registered?
if (js.hasOutputStream(&Serial)) { ... }

// Broadcast mode?
if (js.isBroadcastMode()) { ... }

// Get endpoint name
auto ep = jSerial::getEndpointType(&Serial);
const char* name = jSerial::endpointToString(ep);
```

## ⚙️ Configuration

```cpp
// Enable broadcast (default)
js.setBroadcastMode(true);

// Disable (only first output)
js.setBroadcastMode(false);
```

## 🎨 Integration with Colors

```cpp
#include "Colors.h"

changeTerminalColor(79, true, (Stream*)&js);
js.println("Colored text!");
changeTerminalColor(-1, false, (Stream*)&js);
```

## 📊 Complete Example

```cpp
#include "jSerial.h"

jSerial mainPort;
jSerial debugPort;

void setup() {
    Serial.begin(115200);
    USBSer1.begin(115200);
    
    // Main user interface
    mainPort.setOutputStream(&Serial);
    mainPort.setInputStream(&Serial);
    
    // Debug output (broadcasts)
    debugPort.addOutputStream(&Serial);
    debugPort.addOutputStream(&USBSer1);
    
    mainPort.println("System ready");
    debugPort.printf("Debug active on %d ports\n", 
                     debugPort.getOutputCount());
}

void loop() {
    if (mainPort.available()) {
        String cmd = mainPort.readStringUntil('\n');
        mainPort.println("Command: " + cmd);
        debugPort.printf("Processing: %s\n", cmd.c_str());
    }
}
```

## 📚 Full Documentation

See [JSERIAL_GUIDE.md](JSERIAL_GUIDE.md) for complete documentation and advanced examples.

## ⚠️ Important Notes

- **Maximum 8 output streams** (configurable via `JSERIAL_MAX_OUTPUTS`)
- **Only one input stream** at a time
- **Not thread-safe** - don't use from multiple cores without synchronization
- **Broadcast is sequential** - blocking on one endpoint affects others
- **Full Stream interface** - works anywhere Serial works

