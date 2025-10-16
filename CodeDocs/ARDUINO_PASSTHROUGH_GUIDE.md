# Arduino Flash and Passthrough System - Complete Guide

## Overview

The Jumperless provides Arduino flashing and serial passthrough capabilities, allowing seamless programming and communication with Arduino Nano boards connected via the breadboard. This system bridges TinyUSB CDC interface (USBSer1) and Hardware UART (Serial1) using either synchronous or asynchronous passthrough modes.

**Key Components:**
- **AsyncPassthrough**: Core passthrough service (uart0 <→ USBSer1 CDC)  
- **ArduinoStuff.cpp**: Flash operations, DTR handling, connection management
- **Config system**: Runtime configuration via `jumperlessConfig.serial_1`

---

## Architecture

### System Components

| Component | Purpose | File |
|-----------|---------|------|
| **AsyncPassthrough** | Asynchronous UART<→USB bridge with ring buffer | `AsyncPassthrough.cpp/h` |
| **flashArduino()** | DTR-triggered Arduino programming | `ArduinoStuff.cpp` |
| **DTR pulse detection** | Triggers flash on USB DTR signal | `ArduinoStuff.cpp` |
| **Connection management** | Crosspoint routing for UART | `Ardu inoStuff.cpp` |
| **Serial passthrough** | Bidirectional data forwarding | `ArduinoStuff.cpp` |

### Data Flow

```
avrdude → USBSer1 (CDC1) → AsyncPassthrough → Serial1 (UART0) → Arduino
                                       ↕
                            Ring Buffer (4KB)
                                       ↕
                         Crosspoint Matrix
```

### Operating Modes

**1. Sync Passthrough** (`async_passthrough: false`):
- Direct blocking read/write in flash loop
- Simple, predictable behavior
- Lower overhead

**2. Async Passthrough** (`async_passthrough: true`):
- IRQ-driven ring buffer (4096 bytes)
- Non-blocking operation
- Higher throughput, handles floods better

---

## Arduino Flash Operation

### DTR Pulse Detection

When avrdude initiates flash, it pulses the DTR line. Jumperless detects this and triggers `flashArduino()`:

```cpp
void checkForDTRpulse() {
    int dtrStatus = USBSer1.dtr();
    if (dtrStatus != lastDTRStatus) {
        lastDTRStatus = dtrStatus;
        if (dtrStatus == 0 && FirstDTR == false) {
            arduinoDTRpulse = true;  // Trigger flash
        }
        FirstDTR = false;
    }
}
```

**Detection window:**
- DTR goes LOW (0) → Flash triggered
- First DTR transition ignored (initialization)
- Subsequent LOW transitions trigger flash

### Flash Sequence (Current Implementation)

```cpp
void flashArduino(unsigned long timeoutTime) {
    1. Check if Arduino is connected (UART routing)
    2. Auto-connect if configured (autoconnect_flashing)
    3. Set flashingArduino = true (global state)
    4. Configure Serial1: 115200 baud, 8N1
    5. Reset Arduino (pulse reset line)
    6. Enter passthrough loop:
       - handleSerialPassthrough() for bidirectional data
       - 15 second total timeout
       - 800ms inactivity timeout after initial data
    7. Restore previous baud rate and configuration
    8. Auto-disconnect if not originally connected
    9. Set flashingArduino = false
}
```

**Timing parameters:**
- `timeoutTime`: Default 800ms inactivity timeout
- Total timeout: 15 seconds
- Initial sync timeout: 3200ms (bootloader wake-up)

### Reset Timing

The Arduino is reset using GPIO control of the reset line:

```cpp
void resetArduino() {
    SetArduinoResetLine(LOW, 2);   // Pull reset LOW (both top/bottom)
    delayMicroseconds(800);        // Hold for 800µs
    SetArduinoResetLine(HIGH, 2);  // Release reset
}
```

**Reset line control:**
- `topBottomBoth = 0`: Top Arduino only
- `topBottomBoth = 1`: Bottom Arduino only  
- `topBottomBoth = 2`: Both Arduinos (default for flashing)

---

## AsyncPassthrough Service

### Ring Buffer

**Size:** 4096 bytes (2^12, allows efficient masking)

**Structure:**
```cpp
uint8_t uartReceived[4096];
volatile uint16_t uartReceivedHead;  // Write pointer (IRQ)
volatile uint16_t uartReceivedTail;  // Read pointer (main loop)
```

**Access pattern:**
- IRQ handler writes to `uartReceivedHead`
- `task()` reads from `uartReceivedTail`
- Circular buffer with masking: `index & 0xFFF`

### IRQ Handler

UART RX interrupt fires when data arrives:

```cpp
static void async_uart_irq_handler(void) {
    while (uart_is_readable(ASYNC_PASSTHROUGH_UART)) {
        uint8_t byte = uart_getc(ASYNC_PASSTHROUGH_UART);
        
        // Push to ring buffer
        uint16_t next_head = (uartReceivedHead + 1) & 0xFFF;
        uartReceived[uartReceivedHead] = byte;
        uartReceivedHead = next_head;
    }
}
```

### Task Loop

`AsyncPassthrough::task()` is called frequently from `loop()`:

```cpp
void AsyncPassthrough::task() {
    // 1. Apply pending line coding changes (baud, parity, etc.)
    applyPendingLineCoding();
    
    // 2. Bridge UART → USB
    bridge_uart_to_usb(ASYNC_PASSTHROUGH_CDC_ITF);
    
    // 3. Bridge USB → UART
    bridge_usb_to_uart(ASYNC_PASSTHROUGH_CDC_ITF);
    
    // 4. Service USB stack
    tud_task();
}
```

**Data movement:**
- UART → USB: Drain ring buffer, write to CDC
- USB → UART: Read CDC, write to UART TX
- Rate: Depends on USB polling (~1ms), baud rate, buffer fullness

### Line Coding

AsyncPassthrough dynamically adjusts to USB host baud rate changes:

```cpp
void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const* line_coding) {
    if (itf == ASYNC_PASSTHROUGH_CDC_ITF) {
        // Store pending change
        s_pending_line_coding = *line_coding;
        s_line_coding_pending = true;
    }
}

// Applied in task() outside ISR:
void applyLineCodingOverride(uint32_t baud, uint8_t data_bits, 
                              uint8_t parity, uint8_t stop_bits) {
    uart_set_baudrate(ASYNC_PASSTHROUGH_UART, baud);
    uart_set_format(ASYNC_PASSTHROUGH_UART, data_bits, stop_bits, parity);
    set_micros_per_byte(...);  // Update timing calculations
}
```

---

## Connection Management

### Arduino Connection States

| State | Meaning | UART Routed? |
|-------|---------|--------------|
| `arduinoConnected = 0` | Not connected | No |
| `arduinoConnected = 1` | Connected | Yes (crosspoint closed) |
| `flashingArduino = true` | Flashing in progress | Yes (temporarily) |

### Connecting Arduino

```cpp
void connectArduino(int topBottomBoth, int attemptAutoConnect) {
    // 1. Configure crosspoint routing (D0/D1 → Arduino TX/RX)
    // 2. Close switches for UART path
    // 3. Update arduinoConnected state
    // 4. Print connection status
}
```

**Crosspoint routing:**
- Maps breadboard D0/D1 to Arduino UART pins
- Uses crosspoint switch matrix
- Configured via `sendAllPathsCore2()`

### Disconnecting Arduino

```cpp
void disconnectArduino(int topBottomBoth) {
    // 1. Open crosspoint switches
    // 2. Clear routing
    // 3. Update arduinoConnected = 0
    // 4. Print disconnection status
}
```

### Auto-Connect for Flashing

**Config:** `jumperlessConfig.serial_1.autoconnect_flashing`

If enabled and Arduino not connected:
```cpp
if (arduinoConnected == 0) {
    if (jumperlessConfig.serial_1.autoconnect_flashing == 1) {
        connectArduino(1, 1);
        while (arduinoConnected == 0) {
            arduinoConnected = checkIfArduinoIsConnected();
        }
    }
}
```

**Then after flash:**
```cpp
if (arduinoWasConnected == 0) {
    disconnectArduino(1);  // Restore original state
}
```

---

## Synchronous vs Asynchronous Passthrough

### Sync Mode (`async_passthrough: false`)

**Used during:** Normal flashing operation  
**Implementation:** `handleSerialPassthrough()`

```cpp
int handleSerialPassthrough(int serialSource, int nodeAddy, 
                             int verbose, int whichSerial) {
    int bytesTransferred = 0;
    
    // USB → UART
    while (USBSer1.available() > 0) {
        char c = USBSer1.read();
        Serial1.write(c);
        bytesTransferred++;
    }
    
    // UART → USB
    while (Serial1.available() > 0) {
        char c = Serial1.read();
        USBSer1.write(c);
        bytesTransferred--;  // Negative = received
    }
    
    return bytesTransferred;  // >0 sent, <0 received
}
```

**Characteristics:**
- Blocking reads in tight loop
- Low latency
- Simple, predictable
- Used exclusively during flash

### Async Mode (`async_passthrough: true`)

**Used during:** Normal operation (not flashing)  
**Implementation:** `AsyncPassthrough::task()` + IRQ

```cpp
// IRQ pushes to ring buffer (non-blocking)
// task() drains ring buffer to USB (bulk transfers)
// USB → UART direct writes (non-blocking)
```

**Characteristics:**
- Non-blocking, IRQ-driven
- Higher throughput
- Handles floods better
- Requires `AsyncPassthrough::task()` called frequently

---

## Configuration

### Config File Structure

```yaml
serial_1:
  function: 1                    # 1 = serial passthrough
  async_passthrough: true        # Use async mode
  autoconnect_flashing: 1        # Auto-connect for flash
  baud: 115200                   # Default baud rate
```

### Runtime Configuration

**Check current mode:**
```cpp
if (jumperlessConfig.serial_1.async_passthrough) {
    // Async mode enabled
} else {
    // Sync mode
}
```

**Enable/disable async:**
```cpp
jumperlessConfig.serial_1.async_passthrough = true;
AsyncPassthrough::begin(115200);  // Initialize
```

---

## Bug Fixes and Issues Resolved

### 1. Passthrough Lockup After Flash (FIXED)

**Problem:** Firmware would lock up after Arduino flashing, becoming unresponsive to commands.

**Root Cause:** Debug code left in conditional:
```cpp
// BEFORE (BUG):
if (jumperlessConfig.serial_1.function == 1 && 
    (serial == 0 || serial == 2) || true) {  // ← Debug code!
    // Enter blocking passthrough loop
}
```

The `|| true` made the condition always true, causing passthrough to execute unconditionally after flash.

**Fix:**
```cpp
// AFTER:
if (jumperlessConfig.serial_1.function == 1 && 
    (serial == 0 || serial == 2)) {
    // Only execute when actually configured
}
```

**File:** `src/ArduinoStuff.cpp` line 696

---

### 2. String Constructor Bug (FIXED)

**Problem:** W command was saving to slot 7 instead of slot 0.

**Root Cause:** Arduino's `String` class overloading:
```cpp
// BEFORE (BUG):
input = Serial.read();           // input = 87 (ASCII 'W')
currentCommandLine = String(input);  // Calls String(int) → "87" ❌
```

This called `String(int)` constructor, creating `"87"` instead of `"W"`.

**Fix:**
```cpp
// AFTER:
currentCommandLine = String((char)input);  // Calls String(char) → "W" ✅
```

**Impact:** ALL single-character commands were affected when line buffering was off.

**File:** `src/main.cpp` line 611

---

### 3. Flooding Sketches (PARTIAL - See Proposed Solutions)

**Problem:** Arduino sketches that rapidly print to serial cause:
- Flash failures
- Buffer overflows
- System crashes

**Current Mitigation:**
- 15 second total timeout prevents infinite loops
- AsyncPassthrough ring buffer absorbs some flooding
- Reset stops sketch during flash

**Not Fully Solved:** Aggressive flooding can still cause issues. See "Proposed Improvements" section below.

---

## Proposed Improvements (NOT YET IMPLEMENTED)

The following features were designed and documented but **are not in the current codebase**:

### 1. STK500 Sync Detection

**Proposal:** Wait for avrdude's actual sync sequence (0x30 0x20) before releasing Arduino from reset.

**Benefits:**
- Perfect timing for bootloader entry
- Prevents sketch from running before avrdude ready
- Eliminates race conditions

**Code commented out in ArduinoStuff.cpp (lines 533-547):**
```cpp
// if (USBSer1.peek() == 0x30) {
//   while (USBSer1.available() == 0);
//   peeked = USBSer1.read();
//   if (USBSer1.peek() == 0x20) {
//     Serial1.write(0x30);
//     Serial1.flush();
//     resetArduino();
//   }
// }
```

**Status:** Commented out, not active

---

### 2. Flash Mode API

**Proposal:** Add "flash mode" to AsyncPassthrough that aggressively discards incoming UART data during flash.

**Proposed API:**
```cpp
namespace AsyncPassthrough {
    void setFlashMode(bool enabled);
    bool isFlashMode();
}
```

**Benefits:**
- Prevents flood data from interfering with flash
- Allows circuit breaker reset during flash
- Cleaner buffer state for bootloader communication

**Status:** Not implemented in current code

---

### 3. True Circular Buffer with IRQ Throttling

**Proposal:** Redesign ring buffer to overwrite old data when full, with IRQ rate limiting.

**Features:**
- Circular buffer that never rejects data (overwrites oldest)
- IRQ throttling (max 10kHz rate) to prevent CPU monopolization
- Statistics: overwrite count, throttle count

**Benefits:**
- Self-regulating, no emergency stops needed
- Simpler than circuit breaker system
- Data flows even during floods

**Status:** Described in `ASYNC_PASSTHROUGH_REDESIGN.md` but not implemented

---

### 4. Delayed Reset Release

**Proposal:** Hold Arduino in reset until avrdude sends first byte (proven ready).

**Implementation:**
```cpp
SetArduinoResetLine(LOW, 2);  // Hold in reset
while (USBSer1.available() == 0) {
    // Wait for avrdude to send data
}
delayMicroseconds(2000);
SetArduinoResetLine(HIGH, 2);  // Release when ready
```

**Benefits:**
- Arduino can't flood before avrdude ready
- Perfect synchronization
- Works with any sketch

**Status:** Partially described but not fully implemented

---

## Testing

### Test Sketch: Serial Flood

```cpp
// flood_test.ino - Worst case serial flooding
void setup() {
    Serial.begin(115200);
}

void loop() {
    Serial.println("FLOOD FLOOD FLOOD");  // No delay!
}
```

**Expected behavior (current implementation):**
- Flash may fail intermittently with aggressive flooding
- AsyncPassthrough ring buffer absorbs some data
- 15-second timeout prevents infinite hang

**With proposed improvements:**
- Flash would succeed reliably
- Flash mode would discard flood data
- STK500 sync detection would ensure perfect timing

### Test Sketch: Immediate Output

```cpp
// immediate_test.ino - Tests bootloader timing
void setup() {
    Serial.begin(115200);
    Serial.println("IMMEDIATE OUTPUT");  // No delay!
}

void loop() {
    delay(1000);
    Serial.println("Loop");
}
```

**Expected behavior:**
- Flash usually succeeds (small amount of output)
- May occasionally fail if timing unlucky

### Test Commands

**Connect Arduino:**
```
A 1    # Connect UART for top Arduino
A 2    # Connect UART for bottom Arduino
```

**Check connection:**
```
A?     # Returns "Y,Y" if connected and detected
```

**Disconnect:**
```
a 1    # Disconnect top Arduino
a 2    # Disconnect bottom Arduino
```

**Flash from IDE/avrdude:**
- Flash triggers automatically on DTR pulse
- No manual intervention needed

---

## Performance Characteristics

### Async Mode Throughput

| Metric | Value | Notes |
|--------|-------|-------|
| Ring buffer size | 4096 bytes | Powers of 2 for efficient masking |
| USB poll rate | ~1ms | TinyUSB CDC polling |
| UART baud | 115200 default | Configurable via USB line coding |
| Max throughput | ~11.5 KB/s | Limited by baud rate |
| IRQ latency | <10µs | Hardware FIFO + ring buffer |

### Sync Mode Throughput

| Metric | Value | Notes |
|--------|-------|-------|
| Poll rate | Loop dependent | Depends on other tasks in loop() |
| Latency | ~100µs | Direct read/write, no buffering |
| Throughput | ~11.5 KB/s | Same as async (baud-limited) |

### Flash Timing

| Phase | Duration | Notes |
|-------|----------|-------|
| DTR pulse | <1ms | Detection latency |
| Reset pulse | 800µs | Fixed delay |
| Bootloader init | ~10-20ms | Arduino Nano Optiboot |
| avrdude handshake | ~50-100ms | STK500 sync |
| Flash operation | ~2-5s | Depends on sketch size |
| Total | ~3-6s | Typical flash cycle |

---

## Debug Output

Enable Arduino debugging:
```cpp
#define ARDUINO_DEBUG 1  // In JumperlessDefines.h
```

**Output during flash:**
```
Arduino DTR pulse detected
Arduino connected
Flash Arduino started
Flash Arduino done
totalBytesTransferred: 2847
totalBytesSent: 1423
totalBytesReceived: 1424
```

**During passthrough:**
```cpp
// Enable verbose passthrough:
printSerial1Passthrough = 2;
```

---

## API Reference

### flashArduino()

```cpp
void flashArduino(unsigned long timeoutTime);
```

Perform Arduino flash operation triggered by DTR pulse.

**Parameters:**
- `timeoutTime`: Inactivity timeout in ms (default 800)

**Global state:**
- Sets `flashingArduino = true` during operation
- Reads `arduinoConnected` for connection state
- May call `connectArduino()` / `disconnectArduino()`

### resetArduino()

```cpp
void resetArduino();
```

Pulse Arduino reset line LOW for 800µs.

### connectArduino()

```cpp
void connectArduino(int topBottomBoth, int attemptAutoConnect);
```

Route UART through crosspoint matrix to Arduino.

**Parameters:**
- `topBottomBoth`: 0=top, 1=bottom, 2=both
- `attemptAutoConnect`: 1=auto-detect and connect

### AsyncPassthrough::begin()

```cpp
void AsyncPassthrough::begin(unsigned long baud = 115200);
```

Initialize async passthrough with specified baud rate.

### AsyncPassthrough::task()

```cpp
void AsyncPassthrough::task();
```

Service async passthrough (call frequently from loop()).

---

## Troubleshooting

### Flash Fails with "programmer not responding"

**Symptoms:**
```
avrdude: stk500_recv(): programmer is not responding
avrdude: stk500_getsync() attempt 1 of 10: not in sync: resp=0x00
```

**Common causes:**
1. Arduino not connected (UART not routed)
2. Arduino flooding serial (aggressive output)
3. Wrong baud rate (should be 115200)
4. Bad USB cable / connection
5. Arduino in infinite loop preventing bootloader entry

**Solutions:**
1. Check `A?` command shows Arduino connected
2. Try flashing simple sketch first (Blink with no Serial)
3. Verify 115200 baud in jumperless config
4. Try different USB cable
5. Manually reset Arduino before flash attempt

### Firmware Locks Up After Flash

**Symptoms:**
- Commands echo but don't execute
- System unresponsive
- Must power cycle to recover

**Cause:** `|| true` debug code in passthrough conditional (FIXED in current code)

**Verify fix:**
```cpp
// Check ArduinoStuff.cpp line 696:
if (jumperlessConfig.serial_1.function == 1 && 
    (serial == 0 || serial == 2)) {  // No "|| true"!
```

### Arduino Keeps Resetting

**Symptoms:**
- Arduino resets repeatedly
- Sketch never runs fully
- LED blinks briefly then resets

**Common causes:**
1. DTR line constantly pulsing
2. Power supply insufficient
3. Short circuit on breadboard
4. Bootloader corrupted

**Solutions:**
1. Check USBSer1 port not open in other programs
2. Verify power supply (5V at TOP_RAIL)
3. Check for shorts with continuity tester
4. Re-burn bootloader if needed

### Passthrough Data Corruption

**Symptoms:**
- Garbled characters
- Missing bytes
- Protocol errors

**Common causes:**
1. Baud rate mismatch
2. Parity/stop bits mismatch
3. USB host buffer overflow
4. Electromagnetic interference

**Solutions:**
1. Verify both sides use same baud (115200 default)
2. Check line coding matches (8N1 default)
3. Reduce data rate or add flow control
4. Keep wires short, away from power lines

---

## Related Documentation

- `AsyncPassthrough.h` - API reference
- `AsyncPassthrough.cpp` - Implementation
- `ArduinoStuff.cpp` - Flash and connection logic
- `jumperless_config.yaml` - Configuration file format
- `ASYNC_PASSTHROUGH_REDESIGN.md` - Proposed improvements (NOT implemented)
- `ARDUINO_FLASH_FLOOD_PROTECTION.md` - Proposed STK500 fixes (NOT implemented)
- `ARDUINO_FLASH_SIMPLIFIED.md` - Proposed flash mode API (NOT implemented)

---

## Future Work

### Short Term (Achievable)
1. Implement delayed reset release (simple, high value)
2. Add flash mode API to AsyncPassthrough
3. Improve debug output formatting
4. Add flash statistics tracking

### Medium Term (More Complex)
1. STK500 sync detection for perfect timing
2. Circular buffer with overwrite (remove rejections)
3. IRQ throttling for flood protection
4. Hardware flow control (RTS/CTS)

### Long Term (Major Features)
1. Support for other bootloaders (STK500v2, AVRISP)
2. ESP32/Pi Pico flash support
3. Automatic protocol detection
4. Wireless Arduino flash (via DMX module)

---

## License

Same as JumperlOS (MIT)

---

**Last Updated:** October 2024  
**Current Status:** Sync and async modes working reliably for normal sketches. Flooding protection partially implemented. Proposed improvements documented but not yet coded.

