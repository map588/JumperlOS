# States System Integration Guide

## Overview

The new States system provides object-oriented state management for Jumperless slots, replacing the old text-file-only approach with a structured JSON-based format that keeps the entire system state together.

## Key Features

- **JSON Format**: Human-readable slot files with structured data
- **Nested State**: Organized into ConnectionState, PowerState, DisplayState, ConfigState
- **Backwards Compatible**: Automatically migrates old `nodeFileSlot*.txt` files
- **Undo/Redo**: Built-in history buffer (10 steps by default, adjustable)
- **Validation**: Connection validation with specific error messages
- **Cached Paths**: Paths calculated on-demand and cached for performance
- **RAM Efficient**: Only active slot kept in RAM, inactive slots in flash

## Quick Start

### Basic Usage

```cpp
#include "States.h"

// Get singleton instance
SlotManager& mgr = SlotManager::getInstance();

// Access active state
JumperlessState& state = mgr.getActiveState();

// Add connections (with validation)
String error;
if (!state.addConnection(1, 5, error)) {
    Serial.println("Error: " + error);
}

// Set voltages
state.setDacVoltage(0, 3.3f);
state.setRailVoltage(true, 5.0f);  // true = top rail

// Configure path stacking
state.setPathStacking(2, 3, 0);  // paths, rails, dacs

// Save to slot
if (!mgr.saveSlot(0, error)) {
    Serial.println("Save failed: " + error);
}
```

### Using Helper Functions

```cpp
#include "States.h"

using namespace StateHelpers;

// Quick operations
addConnection(10, 20);
removeConnection(10, 20);
setDac(0, 3.3f);
setRail(true, 5.0f);

// Undo/redo
undo();
redo();

// Slot operations
saveSlot(0);
loadSlot(1);
```

## Slot File Format

New slot files are stored as JSON in `/slots/slot*.json`:

```json
{
  "version": 1,
  "power": {
    "topRail": 5.0,
    "bottomRail": 0.0,
    "dac0": 3.3,
    "dac1": 0.0
  },
  "connections": {
    "numBridges": 3,
    "bridges": [
      {"n1": 1, "n2": 5, "dup": 2},
      {"n1": 10, "n2": 20, "dup": 1},
      {"n1": 15, "n2": 30, "dup": 3}
    ]
  },
  "config": {
    "stackPaths": 2,
    "stackRails": 3,
    "stackDacs": 0,
    "railPriority": 1,
    "gpioDirection": [1,1,1,1,1,1,1,1,0,1],
    "gpioPulls": [0,0,0,0,0,0,0,0,2,2],
    "uartTxFunction": 0,
    "uartRxFunction": 1,
    "oledConnected": false,
    "oledLockConnection": false,
    "autoRefresh": false
  },
  "display": {
    "customColors": [
      {
        "net": 5,
        "r": 255,
        "g": 0,
        "b": 0,
        "raw": 16711680,
        "name": "red"
      }
    ]
  },
  "chipXY": {
    "chips": [
      {"c": 0, "x": 1188, "y": 26}
    ]
  }
}
```

**Compact formatting:**
- GPIO arrays on single lines: `[1,1,1,1,1,1,1,1,0,1]`
- Bridge objects on single lines: `{"n1":1,"n2":5,"dup":2}`
- Connections section right after power for logical grouping
- `chipXY` uses bitfields: `{"c": 0, "x": 1188, "y": 26}`
  - `"c"`: Chip number (0-11)
  - `"x"`: X connections as uint16_t bitfield
  - `"y"`: Y connections as uint8_t bitfield
  - Only non-empty chips stored

This reduces file size by ~60-90% and improves readability. See [STATES_FORMAT_IMPROVEMENTS.md](STATES_FORMAT_IMPROVEMENTS.md) for details.

## Migration from Old System

### Automatic Migration

Old slot files are automatically detected and migrated when you load them:

```cpp
// This will automatically migrate if slot0 is in old format
String error;
if (mgr.loadSlot(0, error)) {
    Serial.println("Slot loaded (and migrated if needed)");
    
    // New JSON file is created, old file remains
    mgr.saveSlot(0, error);  // Save in new format
}
```

### What Gets Migrated

- ✅ Node connections from `{ 1-5, 10-20, ... }`
- ✅ DAC/Rail voltages from global config
- ✅ Path stacking settings from global config
- ❌ Net colors (need to be re-applied, or load from old net colors file)

## Integration Steps

### 1. Update Connection Management

**Old way:**
```cpp
addBridgeToNodeFile(node1, node2, netSlot, 0);
refreshConnections();
```

**New way:**
```cpp
String error;
if (mgr.getActiveState().addConnection(node1, node2, error)) {
    // Paths automatically invalidated
    // Call existing refresh logic as needed
    refreshConnections();
}
```

### 2. Update Slot Loading

**Old way:**
```cpp
openNodeFile(slotNum, 0);
parseStringToBridges();
refreshConnections();
```

**New way:**
```cpp
String error;
if (mgr.loadSlot(slotNum, error)) {
    // State is now loaded
    // Apply to hardware with existing functions
    applyStateToHardware();
}
```

### 3. Update Slot Saving

**Old way:**
```cpp
saveCurrentSlotToSlot(netSlot, slotNum);
```

**New way:**
```cpp
String error;
if (!mgr.saveSlot(slotNum, error)) {
    Serial.println("Save failed: " + error);
}
```

## Advanced Features

### Undo/Redo with History

```cpp
// Make changes
mgr.getActiveState().addConnection(1, 5, error);
mgr.pushHistory();  // Save to history

mgr.getActiveState().addConnection(10, 20, error);
mgr.pushHistory();

// Undo last change
if (mgr.canUndo()) {
    mgr.undo(error);
    // State restored to after first connection
}

// Redo
if (mgr.canRedo()) {
    mgr.redo(error);
    // State restored
}
```

### Custom Color Management

```cpp
// Only stored if manually set (not default colors)
rgbColor red = {255, 0, 0};
state.setNetColor(5, red, 0xFF0000, "red");

// Retrieve
rgbColor color;
uint32_t raw;
char name[32];
if (state.getNetColor(5, color, raw, name)) {
    Serial.println("Net 5 color: " + String(name));
}
```

### GPIO Configuration

```cpp
// Set GPIO direction
state.setGpioDirection(0, 0);  // GPIO 0 = output
state.setGpioPull(0, 1);       // pull-up

// Get settings
int dir = state.getGpioDirection(0);
int pull = state.getGpioPull(0);
```

### State Validation

```cpp
String error;
if (!state.validate(error)) {
    Serial.println("Invalid state: " + error);
    // Specific error messages like:
    // - "DAC 0 voltage out of range (-8V to +8V): 12.50V"
    // - "Invalid node in bridge 5: 999"
    // - "Connection not allowed between 100 and 101 (power/ground conflict)"
}
```

## Memory Usage

```cpp
// Check RAM usage
size_t activeRAM = mgr.getActiveStateRAMUsage();
Serial.println("State system using: " + String(activeRAM) + " bytes");

// Individual state size
size_t stateSize = state.estimateRAMUsage();
```

Typical usage:
- Active state: ~15-20KB
- History buffer (10 states): ~150-200KB
- Total system: ~200-250KB

## Performance Considerations

### Path Calculation Caching

Paths are cached and only recalculated when connections change:

```cpp
// First time - calculates paths
state.addConnection(1, 5, error);

// Access connections - no recalculation needed
bool exists = state.hasConnection(1, 5);

// Modify again - invalidates cache
state.removeConnection(1, 5, error);
```

### Lazy Loading

Slots are only loaded when accessed:

```cpp
// Doesn't load from flash
if (mgr.slotExists(5)) {
    // Only now reads from flash
    mgr.loadSlot(5, error);
}
```

## API Reference

### JumperlessState Class

**Connection Management:**
- `bool addConnection(int node1, int node2, String& errorMsg)`
- `bool removeConnection(int node1, int node2, String& errorMsg)`
- `bool hasConnection(int node1, int node2) const`
- `void clearAllConnections()`

**Power Management:**
- `void setDacVoltage(int dacNum, float voltage)`
- `float getDacVoltage(int dacNum) const`
- `void setRailVoltage(bool isTopRail, float voltage)`
- `float getRailVoltage(bool isTopRail) const`

**Configuration:**
- `void setPathStacking(int paths, int rails, int dacs)`
- `void getPathStacking(int& paths, int& rails, int& dacs) const`

**GPIO:**
- `void setGpioDirection(int gpio, int direction)`
- `int getGpioDirection(int gpio) const`
- `void setGpioPull(int gpio, int pull)`
- `int getGpioPull(int gpio) const`

**Display:**
- `void setNetColor(int netNum, rgbColor color, uint32_t raw, const char* name)`
- `bool getNetColor(int netNum, rgbColor& color, uint32_t& raw, char* name) const`

**Validation:**
- `bool validate(String& errorMsg) const`

**Serialization:**
- `bool toJSON(String& output, bool pretty = false) const`
- `bool fromJSON(const String& input, String& errorMsg)`
- `bool fromLegacyNodeFile(const String& content, String& errorMsg)`

### SlotManager Class

**Singleton Access:**
- `static SlotManager& getInstance()`

**State Access:**
- `JumperlessState& getActiveState()`
- `const JumperlessState& getActiveState() const`
- `int getActiveSlot() const`

**Slot Operations:**
- `bool loadSlot(int slotNum, String& errorMsg)`
- `bool saveSlot(int slotNum, String& errorMsg)`
- `bool saveActiveSlot(String& errorMsg)`
- `bool slotExists(int slotNum) const`
- `bool deleteSlot(int slotNum, String& errorMsg)`
- `void clearActiveSlot()`

**History:**
- `void pushHistory()`
- `bool canUndo() const`
- `bool canRedo() const`
- `bool undo(String& errorMsg)`
- `bool redo(String& errorMsg)`
- `void clearHistory()`
- `int getHistoryDepth() const`

**Utility:**
- `void printSlotInfo(int slotNum)`
- `void listSlots()`
- `size_t getActiveStateRAMUsage() const`

## Testing

### Example Test Code

```cpp
void testStatesSystem() {
    SlotManager& mgr = SlotManager::getInstance();
    String error;
    
    // Clear active state
    mgr.clearActiveSlot();
    
    // Add some connections
    Serial.println("Adding connections...");
    if (!mgr.getActiveState().addConnection(1, 5, error)) {
        Serial.println("Error: " + error);
        return;
    }
    if (!mgr.getActiveState().addConnection(10, 20, error)) {
        Serial.println("Error: " + error);
        return;
    }
    
    // Set voltages
    mgr.getActiveState().setDacVoltage(0, 3.3f);
    mgr.getActiveState().setRailVoltage(true, 5.0f);
    
    // Save to slot 7
    Serial.println("Saving to slot 7...");
    if (!mgr.saveSlot(7, error)) {
        Serial.println("Save error: " + error);
        return;
    }
    
    // Clear and reload
    mgr.clearActiveSlot();
    Serial.println("Loading slot 7...");
    if (!mgr.loadSlot(7, error)) {
        Serial.println("Load error: " + error);
        return;
    }
    
    // Verify
    if (mgr.getActiveState().hasConnection(1, 5)) {
        Serial.println("✓ Connection 1-5 verified");
    }
    if (abs(mgr.getActiveState().getDacVoltage(0) - 3.3f) < 0.01f) {
        Serial.println("✓ DAC 0 voltage verified");
    }
    
    Serial.println("✓ All tests passed!");
}
```

## Troubleshooting

### Slot Won't Load
- Check error message for specific issue
- Try loading in Serial monitor to see verbose output
- Check if file is corrupted (delete and recreate)

### Migration Issues
- Old file format should have `{ }` with bridges inside
- Check that old file isn't empty or malformed
- Migration preserves old file, so you can retry

### Memory Issues
- Reduce `STATE_HISTORY_SIZE` in States.h (default 10)
- Clear history when not needed: `mgr.clearHistory()`
- Check RAM usage: `mgr.getActiveStateRAMUsage()`

### Performance Issues
- Paths are cached - only recalculated when connections change
- Don't call `validate()` repeatedly - it's expensive
- Use `hasConnection()` for quick checks instead of iterating

## Future Enhancements

Planned features (not yet implemented):
- Async file I/O for non-blocking saves
- Compressed history storage (deltas instead of full states)
- Remote slot storage/sync
- Slot templates and presets
- Automatic conflict resolution for concurrent edits

## Questions?

If you encounter issues or have questions about integration:
1. Check this guide first
2. Look at existing code patterns in FileParsing.cpp and NetManager.cpp
3. Use the test functions to verify functionality
4. Check RAM usage if experiencing crashes

