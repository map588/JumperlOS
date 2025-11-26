# AsyncPassthrough Architecture and UART Framing

## Current Status: STABLE ✅

The UART passthrough system is now stable with comprehensive framing error detection, automatic resync, and garbage filtering.

## Architecture Overview

### Command Flow
```
Arduino sends: <p>adc_get(0)</p>
         ↓
UART IRQ → uartReceived[] ring buffer (4KB)
         ↓
AsyncPassthrough::task() → parses tags
         ↓
CommandBuffer::setPendingPCommand("adc_get(0)")
         ↓
Main loop → CommandBuffer::hasPendingCommand()
         ↓
consumePendingCommand() → executeCommand()
         ↓
MicroPython executes → output to Serial AND UART
```

### Two Tag Types
- **`<j>` tags**: Raw commands (run like typed in menu)
- **`<p>` tags**: Python commands (auto-prepends `>` prefix)

Supported tag names: `<j>`, `<jumperless>`, `<jumperlessCommand>`, `<p>`

---

## UART Framing and Synchronization

### The Problem
On startup or during communication, UART framing can get misaligned:
- Receiver starts sampling mid-byte
- Produces garbage data (high-bit bytes, framing errors)
- Can cause crashes or corrupt command parsing

### Solution: Multi-Layer Protection

#### 1. Aggressive Startup Resync
In `AsyncPassthrough::begin()`, we perform 3 complete resync cycles:
```cpp
for (int resync_attempt = 0; resync_attempt < 3; resync_attempt++) {
    // Drain RX FIFO
    // Disable receiver
    // Send break condition (500µs)
    // Wait for stabilization (1000µs)
    // Drain again
    // Re-enable receiver
}
// Final 2ms stabilization
// Clear ring buffer
```

#### 2. Framing Error Detection
The IRQ handler reads per-byte error flags from `UARTDR`:
- `UART_UARTDR_FE_BITS` - Framing error
- `UART_UARTDR_OE_BITS` - Overrun error
- `UART_UARTDR_PE_BITS` - Parity error
- `UART_UARTDR_BE_BITS` - Break error

After 3 consecutive framing errors → automatic resync

#### 3. Garbage Detection (ASCII Mode Only)
When tag parsing is enabled, bytes outside printable ASCII range trigger resync:
```cpp
// Valid: 0x20-0x7E (printable), \n, \r, \t, 0x00
// Garbage: everything else, especially 0x80-0xFF
if (asyncPassthroughTagParsingEnabled && is_garbage_byte(c)) {
    // Don't push to ring buffer
    s_uart_garbage_count++;
}
```
After 4 consecutive garbage bytes → automatic resync

**IMPORTANT**: Garbage detection is DISABLED during Arduino flashing (when `asyncPassthroughTagParsingEnabled == false`). This allows binary STK500 bootloader protocol to pass through.

#### 4. Resync Clears Ring Buffer
When resync is triggered, the entire ring buffer is cleared - any garbage that accumulated is discarded.

---

## Key Functions

### Public API (AsyncPassthrough namespace)

```cpp
// Error statistics
void getUARTErrorStats(uint32_t* framing_errors, uint32_t* overruns, uint32_t* resyncs);
void resetUARTErrorStats();
void printUARTErrorStats();
void printFullDiagnostics();

// Manual resync
void forceUARTResync();           // Resync local receiver
void sendBreakToRemote(uint32_t duration_us = 100);  // Force Arduino to resync
void fullBidirectionalResync();   // Both directions

// Idle detection
bool isLineIdle();
uint32_t getTimeSinceLastRxUs();
bool waitForLineIdle(uint32_t timeout_ms);
void getTimingStats(uint32_t* idle_periods, uint32_t* bytes_since_idle, 
                    uint32_t* timing_anomalies, uint32_t* last_inter_byte_us);
```

### Internal Functions

```cpp
static inline void uart_force_receiver_resync(void);  // Disable/enable RX, clear buffer
static inline bool is_garbage_byte(uint8_t c);        // Check if byte is suspicious
static void async_uart_irq_handler(void);             // Main UART interrupt handler
```

---

## Tracking Variables

```cpp
// Error counts
s_uart_framing_error_count      // Consecutive FE errors (triggers resync at 3)
s_uart_framing_error_total      // Lifetime FE count
s_uart_garbage_count            // Consecutive garbage bytes (triggers resync at 4)
s_uart_garbage_total            // Lifetime garbage count
s_uart_garbage_resyncs          // Resyncs triggered by garbage
s_uart_overrun_count            // HW FIFO overruns
s_uart_resync_count             // Total resyncs performed

// Timing
s_last_rx_byte_time_us          // For idle detection
s_idle_periods_detected         // Natural sync points
s_timing_anomaly_count          // Impossible timing detected
```

---

## Arduino Flashing

When DTR pulse is detected:
1. `asyncPassthroughTagParsingEnabled` set to `false`
2. Garbage detection is disabled (binary STK500 passes through)
3. Arduino is reset via GPIO
4. After inactivity timeout, tag parsing re-enables

---

## Previously Fixed Issues

1. **Startup crashes** - Fixed with aggressive 3-cycle resync sequence
2. **Mid-stream misalignment** - Fixed with framing error + garbage detection
3. **Garbage in ring buffer** - Fixed by clearing buffer on resync
4. **Flashing broken** - Fixed by disabling garbage detection in binary mode
5. **`saveStateToSlot()` bottleneck** - Removed from `jl_nodes_disconnect()`
6. **`nlr_jump_fail` bug** - Function now loops forever instead of returning
7. **Duplicate heap allocation** - Removed from micropython_embed.c

---

## Files Modified

### AsyncPassthrough.cpp
- Aggressive startup resync sequence (3 cycles)
- Framing error detection from UARTDR
- Garbage detection with `is_garbage_byte()`
- Ring buffer cleared on resync
- Break condition transmission
- Full diagnostics API

### AsyncPassthrough.h
- New public API functions for stats, resync, timing
- Documentation for all functions

### CommandBuffer.cpp/h
- Simple synchronous command buffer
- UART response routing

---

## Diagnostic Output

Call `AsyncPassthrough::printFullDiagnostics()` to see:

```
=== UART Full Diagnostics ===
Error Statistics:
  Framing errors: 0 (consecutive: 0)
  Garbage bytes: 0 (consecutive: 0)
  Overrun errors: 0
  Receiver resyncs: 3 (garbage-triggered: 0)
  Ring buffer overflow: 0
Timing Statistics:
  Idle periods detected: 42
  Bytes since last idle: 15
  Timing anomalies: 0
  Last inter-byte time: 87 us
  Expected byte time: 87 us
Current State:
  Line idle: YES
  Time since last RX: 1523 us
  Baud rate: 115200
  Ring buffer: 0 bytes pending
=============================
```
