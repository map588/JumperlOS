# State and Slot Management System - Complete Guide

## Overview

Jumperless uses a unified state management system where **all state lives in RAM** in the `globalState` singleton, with YAML files providing persistence across reboots. The system includes:

- **State Management**: `JumperlessState` structure holding all connections, power, config, and display data
- **Slot System**: 10 slots (0-9) for storing different connection configurations  
- **YAML Format**: Human-readable, user-editable state files
- **SlotManager**: Singleton for file I/O and state operations
- **Active-Only Updates**: Only the currently active slot receives updates from external apps

---

## Architecture

### Single Source of Truth

```cpp
extern JumperlessState globalState;  // THE single state object
```

**Everything uses `globalState`:**
- Old: `net[i]` → New: `globalState.connections.nets[i]`
- Old: `path[i]` → New: `globalState.connections.paths[i]`
- Old: `ch[i]` → New: `globalState.connections.chipStates[i]`

### State Structure

```
globalState (JumperlessState)
├── connections (ConnectionState)
│   ├── bridges[192][3]       ← PRIMARY: Raw connections (node1, node2, duplicates)
│   ├── bridgeColors[192]     ← Bridge wire colors (from Wokwi)
│   ├── nets[60]              ← COMPUTED: Networks from bridges
│   ├── paths[192]            ← COMPUTED: Routing paths from nets
│   ├── chipXY[12]            ← COMPUTED: Crossbar switch states
│   ├── chipStates[12]        ← COMPUTED: Chip status
│   └── numBridges            ← Count of active bridges
├── power (PowerState)
│   ├── topRail               ← Top power rail voltage (V)
│   ├── bottomRail            ← Bottom power rail voltage (V)
│   ├── dac0                  ← DAC 0 output voltage (V)
│   └── dac1                  ← DAC 1 output voltage (V)
├── config (ConfigState)
│   ├── stackPaths            ← Enable path stacking
│   ├── stackRails/Dacs       ← Enable rail/DAC stacking
│   ├── gpioDirection[10]     ← GPIO pin directions
│   ├── gpioPulls[10]         ← GPIO pull resistors
│   ├── uartTxFunction        ← UART TX configuration
│   ├── uartRxFunction        ← UART RX configuration
│   ├── oledConnected         ← OLED display connected
│   └── oledLock              ← OLED update lock
└── display (DisplayState)
    └── customColors[60]      ← Custom net colors (if changed)
```

### Source of Truth

```cpp
enum SourceOfTruth {
    BRIDGES_PRIMARY,  // bridges[] define truth, compute nets/paths from them
    NETS_PRIMARY      // nets[] define truth, derive bridges from them
};
```

**Current default:** `BRIDGES_PRIMARY`

**Why:** Bridges are the user's intent (what they want connected). Nets and paths are computed to implement that intent on the hardware.

---

## YAML File Format

### File Location

**Pattern:** `/slots/slotN.yaml` where N = 0-9

**Active slot:** `SlotManager::getInstance().getActiveSlot()`

**Legacy format:** `/nodeFileSlotN.txt` (old text format, auto-migrated)

### YAML Structure

```yaml
version: 2
sourceOfTruth: bridges

bridges:
  - {n1: 1, n2: 10, dup: 2}
  - {n1: 5, n2: 20, dup: 1, color: magenta}
  - {n1: NANO_D5, n2: 30, dup: 2}
  - {n1: TOP_RAIL, n2: GND, dup: 2}

power:
  topRail: 3.30
  bottomRail: 2.50
  dac0: 3.33
  dac1: 0.00

config:
  routing:
    stackPaths: true
    stackRails: false
    stackDacs: false
  gpio:
    - {pin: 0, dir: INPUT, pull: NONE}
    - {pin: 1, dir: OUTPUT, pull: NONE}
  uart:
    tx: UART_TX_DEFAULT
    rx: UART_RX_DEFAULT
  oled:
    connected: true
    lock: false

display:
  customColors:
    - {net: 6, color: "#FF00FF"}
    - {net: 7, color: chartreuse}
```

### Bridge Format

**Full syntax:**
```yaml
- {n1: <node>, n2: <node>, dup: <count>, color: <color>}
```

**Fields:**
- `n1`, `n2`: Node numbers or names (see Node Names below)
- `dup`: Number of parallel paths (1-4, default 2)
- `color`: Wire color (optional) - Wokwi color name or hex

**Minimal syntax:**
```yaml
- {n1: 1, n2: 10}  # Defaults: dup=2, no color
```

### Node Names

**Breadboard nodes:** `1-60` (rows 1-30 top/bottom)

**Special nodes:**
```yaml
GND             # Ground (100)
TOP_RAIL        # Top power rail (101)
BOTTOM_RAIL     # Bottom power rail (102)
DAC0            # DAC 0
DAC1            # DAC 1
```

**Arduino Nano:**
```yaml
NANO_D0 - NANO_D13   # Digital pins (120-133)
NANO_A0 - NANO_A7    # Analog pins (134-141)
NANO_RESET           # Reset pin (142)
NANO_AREF            # Analog reference (143)
```

**GPIO:**
```yaml
GPIO_1 - GPIO_8          # Header GPIO (131-138, alias for RP_GPIO_1-8)
```

**Current Sense:**
```yaml
CURRENT_SENSE_PLUS  
CURRENT_SENSE_MINUS 
```

---

## API Reference

### SlotManager (Singleton)

```cpp
SlotManager& mgr = SlotManager::getInstance();
```

#### Save/Load Operations

```cpp
// Save current globalState to slot
bool saveSlot(int slotNum, String& errorMsg);

// Load slot into globalState
bool loadSlot(int slotNum, String& errorMsg);

// Get currently active slot
int getActiveSlot() const;

// Set active slot (doesn't load, just marks)
void setActiveSlot(int slotNum);
```

#### File Operations

```cpp
// Get slot filename
String getSlotFilename(int slotNum) const;  // "/slots/slotN.yaml"

// List all slots
void listSlots() const;  // Prints to Serial

// Delete slot file
bool deleteSlot(int slotNum, String& errorMsg);

// Write YAML directly to slot (for parsers)
bool writeSlotFile(int slotNum, const String& yamlContent, String& errorMsg);
```

### JumperlessState API

```cpp
// Add connection
bool addConnection(int node1, int node2, String& errorMsg, int duplicates = 2);

// Remove connection
bool removeConnection(int node1, int node2, String& errorMsg);

// Remove all bridges containing a node
int removeBridgesByNode(int node, String& errorMsg);

// Clear all connections
void clear();

// Direct access to state components
globalState.connections.bridges[i][0]  // node1
globalState.connections.bridges[i][1]  // node2
globalState.connections.bridges[i][2]  // duplicates
globalState.connections.bridgeColors[i]  // color (0xFFFFFFFF = no color)
```

### StateHelpers (Convenience Functions)

```cpp
// Add connection to globalState
bool addConnectionToState(int node1, int node2, String& errorMsg, int dups = 2);

// Remove connection from globalState
bool removeConnectionFromState(int node1, int node2, String& errorMsg);

// Remove all connections with node
int removeBridgesByNodeFromState(int node, String& errorMsg);

// Load bridges from globalState into legacy arrays
void loadBridgesFromState();
```

---

## Workflow Examples

### Example 1: Add Connection and Apply

```cpp
String errorMsg;

// 1. Add connection to RAM
globalState.addConnection(1, 10, errorMsg, 2);
globalState.addConnection(5, 20, errorMsg, 3);

// 2. Apply to hardware
refreshConnections(-1);  // Reads from globalState, programs crossbar

// 3. Save to slot (optional - for persistence)
SlotManager& mgr = SlotManager::getInstance();
mgr.saveSlot(0, errorMsg);
```

**Result:** Connections active in hardware, saved to slot 0

---

### Example 2: Load Slot and Modify

```cpp
String errorMsg;
SlotManager& mgr = SlotManager::getInstance();

// 1. Load existing slot
mgr.loadSlot(3, errorMsg);  // Loads into globalState

// 2. Add new connection
globalState.addConnection(15, 25, errorMsg);

// 3. Apply changes
refreshConnections(-1);

// 4. Save back to slot
mgr.saveSlot(3, errorMsg);
```

**Result:** Slot 3 updated with new connection

---

### Example 3: Cycle Slots (User Command: '<')

```cpp
// User presses '<'
int currentSlot = SlotManager::getInstance().getActiveSlot();
int nextSlot = (currentSlot + 1) % NUM_SLOTS;

String errorMsg;
if (SlotManager::getInstance().loadSlot(nextSlot, errorMsg)) {
    // Slot loaded successfully
    refreshConnections(-1);  // Apply to hardware
} else {
    Serial.println("Error loading slot: " + errorMsg);
}
```

**Result:** Next slot becomes active, hardware updated

---

### Example 4: Import from Wokwi (Active Slot)

```cpp
String jsonContent = "{ ... Wokwi diagram.json ... }";
String errorMsg;
SlotManager& mgr = SlotManager::getInstance();

// Get current active slot
int activeSlot = mgr.getActiveSlot();

// Parse directly into globalState
if (parseWokwiDiagram(jsonContent, globalState, activeSlot, errorMsg)) {
    // Save to file
    mgr.saveSlot(activeSlot, errorMsg);
    
    // Apply to hardware
    refreshConnections(-1);
} else {
    Serial.println("Parse error: " + errorMsg);
}
```

**Result:** Wokwi diagram loaded, applied to hardware

---

### Example 5: Import from Wokwi (Inactive Slot - Zero-Copy)

```cpp
String jsonContent = "{ ... Wokwi diagram.json ... }";
String errorMsg;
int inactiveSlot = 5;  // Not the active slot

// Parse directly to file (no JumperlessState created!)
if (parseWokwiDiagramDirectToFile(jsonContent, inactiveSlot, errorMsg, true)) {
    Serial.println("Saved to slot 5 (inactive)");
} else {
    Serial.println("Parse error: " + errorMsg);
}

// Later: Cycle to slot 5 to activate it
SlotManager::getInstance().loadSlot(5, errorMsg);
refreshConnections(-1);
```

**Result:** Wokwi diagram saved to inactive slot without affecting hardware

---

## Active Slot Only Updates

### Overview

The Jumperless Bridge app (Python) **only updates the currently active slot**. This eliminates:
- Background updates to inactive slots
- Hardware glitches from accidentally updating wrong slot
- Complex differential update logic

### Firmware Commands

**Query active slot:**
```
Q    # Returns: ACTIVE_SLOT:0 (for example)
```

**Slot change notification:**
```
# Firmware automatically sends when slot changes:
SLOT_CHANGED:3
```

### App Synchronization

**Python app (JumperlessWokwiBridge.py):**
```python
currentActiveSlot = 0  # Track active slot

# Query periodically
send_command('Q')
response = read_response()  # "ACTIVE_SLOT:0"
currentActiveSlot = parse_slot_number(response)

# Listen for slot change notifications
if "SLOT_CHANGED:" in response:
    currentActiveSlot = parse_slot_number(response)
    
# Only update current active slot
if diagram_changed:
    send_wokwi_diagram(currentActiveSlot, diagram_json)
```

### Zero-Copy Inactive Slot Updates

When app needs to update an inactive slot:

```
W 5    # Wokwi command to slot 5
{ ... paste JSON ... }
```

**Firmware behavior:**
1. Detects slot 5 is inactive
2. Calls `parseWokwiDiagramDirectToFile()`
3. Never creates `JumperlessState` object (saves ~50KB memory)
4. Builds minimal YAML with just `bridges` and `power`
5. Writes directly to `/slots/slot5.yaml`
6. Hardware unchanged (slot 5 not active)

**Memory savings:**
- Old approach: ~55KB (50KB state + 5KB strings)
- New approach: ~5-10KB (strings only, no state object)

---

## Memory Safety

### Critical: Avoid Copying JumperlessState

**`JumperlessState` is ~50KB:**
- `bridges[192][3]` = ~2.3KB
- `nets[60]` with nodes = ~14KB
- `paths[192]` = ~9KB
- Other arrays = ~24KB

### ✅ ALWAYS Use References

```cpp
// GOOD:
JumperlessState& state = globalState;
SlotManager& mgr = SlotManager::getInstance();
JumperlessState& activeState = mgr.getActiveState();

// GOOD: Pass by reference
void processState(JumperlessState& state) {
    state.addConnection(1, 2, errorMsg);
}
```

### ❌ NEVER Copy

```cpp
// BAD - 50KB stack allocation!
JumperlessState state = globalState;

// BAD - copies on pass by value
void processState(JumperlessState state) {  // Copies 50KB!
    // ...
}
```

### Copy Constructors Deleted

```cpp
class JumperlessState {
    // Explicitly deleted to prevent accidents
    JumperlessState(const JumperlessState&) = delete;
    JumperlessState& operator=(const JumperlessState&) = delete;
};
```

**If you need a copy** (e.g., undo/redo), use explicit methods:
```cpp
JumperlessState backup;
backup.copyFrom(globalState);  // Explicit, intentional copy
```

---

## SlotManager Service Optimization

### Service Architecture

SlotManager implements the `IService` interface and runs as a Core 1 service:

```cpp
class SlotManager : public IService {
    ServiceStatus service() override;
    // ...
};
```

**Service tasks:**
1. Monitor slot files for changes (auto-refresh)
2. Periodic integrity checks
3. Background optimization
4. Statistics tracking

### Auto-Refresh on File Change

**Config:**
```yaml
config:
  autoRefreshOnChange: true  # Enable auto-refresh
```

**Behavior:**
- SlotManager monitors active slot file modification time
- If file changes externally (user edited, app updated), automatically reload
- Applies changes to hardware without user intervention

**Use case:** Edit slot file on PC via USB mass storage, changes apply automatically

---

## Commands

### Query Active Slot ('Q')

```
Q    # Returns: ACTIVE_SLOT:0
```

**Use:** External apps query which slot is active for synchronization

**Implementation:** `cmd_queryActiveSlot()` in `SingleCharCommands.cpp`

### Cycle Slots ('<')

```
<    # Cycle to next slot (0→1→2...→9→0)
```

**Behavior:**
1. Get current slot
2. Increment (wrap at 9)
3. Load next slot
4. Apply to hardware
5. Send `SLOT_CHANGED:X` notification

### Load Slot ('l')

```
l 5    # Load slot 5
```

**Behavior:**
1. Load specified slot into globalState
2. Apply to hardware
3. Update active slot marker

### Test State System ('J')

```
J    # Run comprehensive state system tests
```

**Tests:**
- Bridge add/remove
- Serialization/deserialization
- File I/O
- Memory safety
- Legacy migration

---

## Migration from Legacy Format

### Legacy Text Format

**Old:** `/nodeFileSlotN.txt`
```
1-10
5-20
NANO_D5-30
```

**Problems:**
- No structure (just node pairs)
- No metadata (duplicates, colors, config)
- Hard to parse
- Not user-friendly

### Auto-Migration

**Trigger:** Loading a legacy `.txt` file

**Process:**
1. Detect `.txt` extension
2. Parse node pairs into bridges
3. Create JumperlessState with defaults
4. Serialize to YAML
5. Save as `/slots/slotN.yaml`
6. Keep original `.txt` as backup

**User transparent:** Just works!

---

## Troubleshooting

### Issue: Slot Won't Load

**Symptoms:**
```
Error loading slot: Parse error at line 42
```

**Common causes:**
1. Syntax error in YAML (indentation, colons)
2. Invalid node name
3. Missing required fields
4. Corrupted file

**Solutions:**
1. Check YAML syntax (use validator)
2. Verify node names against docs
3. Check for required fields: `version`, `sourceOfTruth`, `bridges`
4. Restore from backup or regenerate

### Issue: Hardware Not Updating

**Symptoms:**
- Slot loads successfully
- No error messages
- Hardware unchanged

**Cause:** Forgot to call `refreshConnections()`

**Solution:**
```cpp
mgr.loadSlot(5, errorMsg);
refreshConnections(-1);  // REQUIRED!
```

### Issue: Memory Crash / Reboot

**Symptoms:**
- System reboots when loading slot
- Watchdog timeout
- Stack overflow error

**Cause:** Copying JumperlessState (50KB on stack)

**Solution:** Use references (see Memory Safety section)

### Issue: Slot File Missing

**Symptoms:**
```
Error loading slot: File not found
```

**Cause:** Slot never saved or deleted

**Solution:**
1. Check if file exists: USB mass storage → `/slots/slotN.yaml`
2. Load different slot
3. Create new slot with `W` command or manual edits

---

## Performance Characteristics

### File I/O

| Operation | Time | Notes |
|-----------|------|-------|
| Save slot | ~50-100ms | YAML serialization + flash write |
| Load slot | ~30-70ms | YAML parsing + validation |
| List slots | ~10-20ms | Directory scan |

### Memory Usage

| Component | Size | Location |
|-----------|------|----------|
| globalState | ~50KB | BSS (static) |
| YAML file | ~2-5KB | Flash (/slots/) |
| Parse buffer | ~8KB | Heap (temporary) |
| Serialization | ~8KB | Heap (temporary) |

### Refresh Performance

| Operation | Time | Notes |
|-----------|------|-------|
| bridgesToNets | ~5-10ms | Network analysis |
| netsToPaths | ~10-20ms | Path finding |
| sendPaths | ~20-50ms | Crossbar programming |
| Total refresh | ~35-80ms | Full connection update |

---

## Advanced Features

### Duplicate Paths (Stacking)

**Purpose:** Parallel paths for lower resistance

**Config:**
```yaml
config:
  routing:
    stackPaths: true  # Allow multiple paths per connection
```

**Usage:**
```yaml
bridges:
  - {n1: 1, n2: 10, dup: 4}  # 4 parallel paths
```

**Hardware:** Uses multiple CH446Q switches in parallel

### Custom Net Colors

**Purpose:** User-defined net colors for visualization

**Format:**
```yaml
display:
  customColors:
    - {net: 6, color: "#FF00FF"}
    - {net: 7, color: magenta}
    - {net: 8, color: chartreuse}
```

**Precedence:**
1. Bridge colors (from Wokwi)
2. Custom colors (user-defined)
3. Auto-generated colors (hue distribution)

### GPIO Configuration

**Purpose:** Configure RP2040 GPIO pins

**Format:**
```yaml
config:
  gpio:
    - {pin: 0, dir: INPUT, pull: PULLUP}
    - {pin: 1, dir: OUTPUT, pull: NONE}
    - {pin: 2, dir: INPUT, pull: PULLDOWN}
```

**Directions:** `INPUT`, `OUTPUT`, `INPUT_PULLUP`, `INPUT_PULLDOWN`
**Pulls:** `NONE`, `PULLUP`, `PULLDOWN`

---

## Related Documentation

- `States.h` - State structure definitions
- `States.cpp` - Implementation
- `WokwiParser.h/cpp` - Wokwi diagram import
- `WOKWI_PARSER_GUIDE.md` - Complete Wokwi import guide

---

## Future Enhancements

### Planned Features
1. [ ] Slot templates (starter configurations)
2. [ ] Slot metadata (name, description, author)
3. [ ] Diff/merge operations
4. [ ] Compressed storage for large configs
5. [ ] Cloud sync (via app)
6. [ ] Version history (git-style)

### Under Consideration
1. Multiple active slots (split breadboard)
2. Conditional connections (if-then-else)
3. Connection groups (named sub-circuits)
4. Auto-routing hints
5. Simulation mode (validate before apply)

---

**Status:** Production ready, actively maintained  
**Format Version:** 2 (current)  
**Last Updated:** October 2024

