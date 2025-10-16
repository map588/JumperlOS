# Memory Safety Fix: Wokwi Parser Crash

## Problem

When loading Wokwi diagrams, the device would crash with no error message - just disconnect.

## Root Cause

The `parseWokwiDiagram()` function was allocating **~13KB of temporary arrays on the stack**:

```cpp
// BEFORE - CAUSES CRASH:
BridgeColorInfo bridgeColors[MAX_BRIDGES];  // 192 elements × ~32 bytes = ~6KB

struct ColorUsage {
    uint32_t color;
    String colorName;
    int count;
    int nodeList[MAX_NODES];  // 48 ints × 4 bytes = 192 bytes
    int numNodes;
};
ColorUsage colorUsage[32];  // 32 × ~220 bytes = ~7KB
```

**Total stack usage: ~13KB** 

This exhausted the RP2040/RP2350's limited stack space, causing a hard fault.

## Solution

### 1. Removed Temporary Color Tracking Arrays

Colors are now stored directly in the state's `bridgeColors[]` array:

```cpp
// AFTER - NO CRASH:
// Colors stored directly in outState.connections.bridgeColors[]
// No temporary arrays allocated!
```

**Memory saved: ~13KB on stack**

### 2. Deleted Copy Constructors

Added `= delete` to prevent accidental copying of huge state objects:

```cpp
class JumperlessState {
    // ...
    JumperlessState(const JumperlessState&) = delete;              // No copy constructor
    JumperlessState& operator=(const JumperlessState&) = delete;   // No copy assignment
};

struct ConnectionState {
    // ...
    ConnectionState(const ConnectionState&) = delete;
    ConnectionState& operator=(const ConnectionState&) = delete;
};
```

Now if you accidentally try to copy:
```cpp
JumperlessState state = globalState;  // COMPILER ERROR!
```

The compiler will prevent it at compile-time instead of crashing at runtime.

### 3. Added Comprehensive Documentation

- Header comment in `States.cpp` explaining memory constraints
- Warning comments in `States.h` class definitions
- Memory entry to prevent future mistakes

## Memory Size Reference

### JumperlessState Total Size: **~50+ KB**

| Component | Size |
|-----------|------|
| `bridges[192][3]` | 2,304 bytes |
| `bridgeColors[192]` | 768 bytes |
| `nets[60]` | ~30 KB |
| `paths[192]` | ~20 KB |
| Other fields | ~1 KB |

### Stack Memory Limits

- **RP2040:** ~16KB per core
- **RP2350:** Similar constraints
- **Safe stack usage:** < 8KB (leave headroom for interrupts)

### Why This Matters

Embedded systems have **very limited stack space** compared to desktop computers:

| Platform | Typical Stack |
|----------|--------------|
| Desktop PC | 1-8 MB |
| RP2040/RP2350 | 16 KB |

A single copy of `JumperlessState` would use **>300% of available stack!**

## Best Practices

### ✅ ALWAYS Do This:

```cpp
// Use references
JumperlessState& state = SlotManager::getInstance().getActiveState();

// Pass by reference
void processState(JumperlessState& state) { ... }

// Use pointers if needed
JumperlessState* statePtr = &globalState;
```

### ❌ NEVER Do This:

```cpp
// Copy by value (COMPILER ERROR with = delete)
JumperlessState state = globalState;

// Pass by value (COMPILER ERROR)
void processState(JumperlessState state) { ... }

// Return by value (COMPILER ERROR)
JumperlessState getState() { return globalState; }
```

## Testing

### Before Fix:
```
W{...Wokwi JSON...}
Port /dev/cu.usbmodemJLV5port1 disconnected  ← CRASH
```

### After Fix:
```
W{...Wokwi JSON...}
◆ Wokwi parsing complete:
  Connections: 13 added, 0 skipped
  Top rail: 3300mV
  Bottom rail: 2500mV
  ✓ Saved Wokwi diagram to slot 0  ← SUCCESS
```

## Related Files

- `States.h` - Added `= delete` to prevent copying
- `States.cpp` - Added memory safety documentation
- `WokwiParser.cpp` - Removed temporary arrays (13KB saved)

## Lessons Learned

1. **Stack memory is precious** on embedded systems
2. **Large temporary arrays** can silently exhaust stack
3. **Compiler enforcement** (= delete) prevents runtime crashes
4. **Memory profiling** should be done during development
5. **Documentation** helps prevent future mistakes

## Memory Profiling Tips

To check stack usage:
```cpp
// At function entry
char stackMarker;
void* stackTop = &stackMarker;

// Calculate approximate stack usage
size_t stackUsed = ...;  // Compare with start address
Serial.printf("Stack used: %zu bytes\n", stackUsed);
```

For accurate profiling, use:
- GCC's `-fstack-usage` flag
- Static analysis tools
- Runtime stack canaries

## Future Improvements

1. Consider using **heap allocation** for very large temporary structures
2. Add **compile-time size checks** for large stack allocations
3. Implement **stack overflow detection** in debug builds
4. Profile other functions for excessive stack usage

