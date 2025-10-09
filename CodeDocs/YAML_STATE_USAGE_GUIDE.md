# YAML State System Usage Guide

## Overview

The Jumperless firmware now uses a unified YAML-based state management system. **ALL state lives in RAM** in the `globalState` singleton, and YAML files are used only for persistence across reboots.

## Architecture

### Single Source of Truth

```cpp
JumperlessState globalState;  // THE single state object - all data here!
```

**Everything uses `globalState`:**
- Old: `net[i]` → New: `globalState.connections.nets[i]`
- Old: `path[i]` → New: `globalState.connections.paths[i]`  
- Old: `ch[i]` → New: `globalState.connections.chipStates[i]`

### State Structure

```
globalState
├── connections
│   ├── bridges[MAX_BRIDGES][3]  ← STORED (node1, node2, duplicates)
│   ├── nets[MAX_NETS]           ← COMPUTED from bridges
│   ├── paths[MAX_BRIDGES]       ← COMPUTED from nets
│   ├── chipStates[12]           ← COMPUTED from paths
│   └── chipXY[12]               ← COMPUTED from paths
├── power
│   ├── topRail, bottomRail      ← STORED
│   └── dac0, dac1               ← STORED
├── config
│   ├── routing (stacking)       ← STORED
│   ├── gpio[10]                 ← STORED
│   ├── uart                     ← STORED
│   └── oled                     ← STORED
└── display
    └── customColors[]           ← STORED (optional)
```

## New Connection Flow

### 1. Adding Connections (RAM Only)

```cpp
String errorMsg;
// Add connection to globalState (no file I/O!)
globalState.addConnection(1, 2, errorMsg);
globalState.addConnection(10, 20, errorMsg, 3);  // With 3 duplicates

// globalState.connections.bridges[] now contains these connections
```

### 2. Applying to Hardware

```cpp
// Refresh to apply connections to physical crossbar switches
refreshConnections(-1);  // This now reads from globalState!
```

**What happens:**
1. `refreshConnections()` calls `loadBridgesFromState()`
2. `loadBridgesFromState()` copies `globalState.connections.bridges[]` → `newBridge[]`
3. `getNodesToConnect()` processes bridges into nets
4. `bridgesToPaths()` computes routing paths
5. `sendPaths()` programs the CH446Q crossbar switches

**No file operations!** Everything happens in RAM.

### 3. Saving to Persistent Storage (Optional)

```cpp
String errorMsg;
SlotManager& mgr = SlotManager::getInstance();

// Save current state to slot
mgr.saveSlot(7, errorMsg);  // Writes /slots/slot7.yaml

// The YAML file is human-readable:
// version: 2
// bridges:
//   - {n1: 1, n2: 2, dup: 2}
//   - {n1: 10, n2: 20, dup: 3}
// power:
//   topRail: 5.00
//   ...
```

### 4. Loading from Storage

```cpp
// Load a saved slot
mgr.loadSlot(7, errorMsg);  // Reads /slots/slot7.yaml into globalState

// Now globalState contains the loaded state
refreshConnections(-1);  // Apply to hardware
```

## Command Usage

### J Command - Test & Add Connections

```bash
J 1-2              # Add single connection
J 1-5,10-20        # Add multiple connections  
J 1-2,1-2,1-2      # Add duplicates (increments count)
```

**What it does:**
1. Parses connections from command
2. Adds each to `globalState` via `addConnection()`
3. **Automatically calls `refreshConnections()`** to apply to hardware
4. Shows YAML output
5. Tests save/load to slot 7

**Output:**
```
╭────────────────────────────────────╮
│   States System Test (J command)  │
╰────────────────────────────────────╯

Parsing connections: 1-2
  Adding connection: 1-2... ✓ Success

─── Applying to Hardware ───
Refreshing connections... ✓ Done

─── Current State ───
Connections: 1
Active Slot: -1

─── Testing YAML Serialization ───
YAML output:
version: 2
sourceOfTruth: bridges

bridges:
  - {n1: 1, n2: 2, dup: 2}

power:
  topRail: 0.00
  bottomRail: 0.00
  dac0: 3.33
  dac1: 0.00

config:
  routing: {stackPaths: 2, stackRails: 3, stackDacs: 0, railPriority: 1}
  ...

─── Testing Slot Save ───
Saving to slot 7... ✓ Success
  File: /slots/slot7.yaml
```

## API Reference

### Adding Connections

```cpp
String errorMsg;

// Basic connection
globalState.addConnection(node1, node2, errorMsg);

// With custom duplicates
globalState.addConnection(node1, node2, errorMsg, 3);

// With default duplicates (-1 = use config.stackPaths)
globalState.addConnection(node1, node2, errorMsg, -1);
```

### Removing Connections

```cpp
String errorMsg;
globalState.removeConnection(node1, node2, errorMsg);
```

### Checking Connections

```cpp
// Check if connection exists
if (globalState.hasConnection(1, 2)) {
    // Connection exists
}

// Get duplicate count
int dups = globalState.getConnectionDuplicates(1, 2);
```

### Power Management

```cpp
// Set voltages
globalState.setDacVoltage(0, 3.3f);
globalState.setDacVoltage(1, 1.5f);
globalState.setRailVoltage(true, 5.0f);   // Top rail
globalState.setRailVoltage(false, 0.0f);  // Bottom rail

// Get voltages
float dac0 = globalState.getDacVoltage(0);
float topRail = globalState.getRailVoltage(true);
```

### Configuration

```cpp
// Set routing config
globalState.setPathStacking(2, 3, 0);  // paths, rails, dacs

// GPIO
globalState.setGpioDirection(0, 1);  // Pin 0 as input
globalState.setGpioPull(0, 0);       // Pull-down

// Source of truth mode
globalState.setSourceOfTruth(BRIDGES_PRIMARY);  // Default
globalState.setSourceOfTruth(NETS_PRIMARY);     // Alternative
```

### Slot Management

```cpp
SlotManager& mgr = SlotManager::getInstance();
String errorMsg;

// Load slot (from YAML file into globalState)
mgr.loadSlot(3, errorMsg);

// Save slot (from globalState to YAML file)
mgr.saveSlot(3, errorMsg);

// Check if slot exists
if (mgr.slotExists(3)) {
    // Slot file exists
}

// Delete slot
mgr.deleteSlot(3, errorMsg);
```

## Key Differences from Old System

### Old System (File-Based)
```
User input → Write to nodeFileSlot0.txt
            ↓
refresh() → Read nodeFileSlot0.txt
            ↓
Process into nets/paths → Apply to hardware
```

**Problems:**
- File I/O on every refresh
- Data scattered across files
- Hard to manage state
- Slow flash writes

### New System (RAM-Based)
```
User input → Add to globalState (RAM)
            ↓
refresh() → Read from globalState (RAM)
           ↓
Process into nets/paths → Apply to hardware

(Optional) Save → Write YAML to flash when needed
```

**Benefits:**
- ✅ **No file I/O** during normal operations
- ✅ **Fast** - everything in RAM
- ✅ **Single source of truth** - `globalState`
- ✅ **Clean API** - explicit methods
- ✅ **Human-readable** YAML when saved
- ✅ **Minimal storage** - only essential data

## Migration from Legacy Format

The system automatically migrates old `.txt` format files:

```cpp
// When loading a slot:
1. Check for /slots/slot3.yaml (new format) ✓
2. If not found, check /nodeFileSlot3.txt (legacy) ✓
3. If found, parse legacy format and convert
4. Save as YAML on next save
```

**Legacy format** (nodeFileSlot0.txt):
```
{1-2, 10-20, 15-30}
```

**New format** (slot0.yaml):
```yaml
version: 2
sourceOfTruth: bridges
bridges:
  - {n1: 1, n2: 2, dup: 2}
  - {n1: 10, n2: 20, dup: 2}
  - {n1: 15, n2: 30, dup: 2}
power:
  topRail: 5.00
  bottomRail: 0.00
  dac0: 3.33
  dac1: 0.00
config:
  routing: {stackPaths: 2, stackRails: 3, stackDacs: 0, railPriority: 1}
  ...
```

## Performance Characteristics

### Memory Usage
- **GlobalState:** ~57KB (single instance)
- **Computed data:** Regenerated when needed (no duplication)
- **History buffer:** Configurable (currently disabled: STATE_HISTORY_SIZE = 0)

### Speed
- **Connection add:** ~1μs (array operation)
- **Refresh:** Same as before (computation time unchanged)
- **Save to YAML:** ~50ms (only when explicitly saved)
- **Load from YAML:** ~30ms (only on boot/slot change)

## Dirty Flag System (Future: Lazy Writes)

Infrastructure is in place for automatic background saves:

```cpp
// Connections are automatically marked dirty
globalState.addConnection(1, 2, errorMsg);
// → globalState.isDirty() == true
// → globalState.getLastModifiedTime() == current time

// Future implementation in main loop:
if (globalState.isDirty()) {
    unsigned long idle = millis() - globalState.getLastModifiedTime();
    if (idle > 2000) {  // 2 seconds idle
        mgr.saveActiveSlot(errorMsg);
        globalState.clearDirty();
    }
}
```

## Bidirectional Sync (Advanced)

The system supports two modes:

### BRIDGES_PRIMARY (Default)
- User specifies bridges (connections between nodes)
- System computes nets automatically
- YAML nets section is optional (for colors/names only)

### NETS_PRIMARY (Alternative)
- User specifies nets with node lists
- System generates bridges automatically
- Useful for net-centric workflows

**Switch mode:**
```cpp
globalState.setSourceOfTruth(NETS_PRIMARY);
globalState.connections.syncBridgesFromNets();  // Generate bridges from nets
```

## Best Practices

### DO:
✅ Modify `globalState` for all connection changes
✅ Call `refreshConnections()` to apply to hardware
✅ Save to slots when needed (manual or via future scheduler)
✅ Use the J command for quick testing

### DON'T:
❌ Modify `net[]`, `path[]`, `ch[]` directly (they're just views of `globalState`)
❌ Write to node files manually (deprecated)
❌ Assume file operations happen automatically (they don't - it's RAM-based now!)

## Example Workflow

```cpp
// 1. Clear current connections
globalState.clearAllConnections();

// 2. Add your connections
String err;
globalState.addConnection(1, 5, err);
globalState.addConnection(10, 20, err);
globalState.setDacVoltage(0, 3.3);

// 3. Apply to hardware
refreshConnections(-1);

// 4. Test it out...

// 5. Save if you like it
SlotManager::getInstance().saveSlot(1, err);

// 6. Load it later
SlotManager::getInstance().loadSlot(1, err);
refreshConnections(-1);
```

## Troubleshooting

**Q: My connections aren't showing up on hardware**
A: Did you call `refreshConnections()` after adding them?

**Q: My connections disappeared after reboot**
A: Did you save to a slot? Changes are in RAM only until saved.

**Q: Can I still use the old text files?**
A: Yes! They auto-migrate to YAML on first load. But going forward, edit globalState directly.

**Q: How do I see what's in globalState?**
A: Use the `J` command with no arguments, or add debug prints.

## Future Enhancements

- [ ] Lazy write scheduler (save after 2s idle)
- [ ] Undo/redo commands (infrastructure exists)
- [ ] State comparison/diff
- [ ] Preset templates
- [ ] Network sync/sharing
- [ ] YAML import/export commands

---

## Summary

The new system gives you:
- 🚀 **Fast:** All operations in RAM
- 💾 **Efficient:** 100KB RAM freed
- 📝 **Readable:** YAML format
- 🎯 **Simple:** Single `globalState` object
- 🔄 **Flexible:** Bidirectional bridge/net sync
- ⚡ **Future-ready:** Infrastructure for auto-save, undo/redo

**The key insight:** State lives in RAM (`globalState`), files are just persistence. Work with `globalState`, refresh to hardware, save when you want.

