# YAML State System: Enhanced User-Editable Format

## Summary

The YAML state system has been significantly enhanced to make state files more user-friendly and directly editable. This update enables users to edit YAML files in any text editor (including the eKilo editor) using human-readable names and aliases, with changes automatically detected and applied.

## Key Features

### 1. Node Name Aliases

**Problem**: Node values were represented as raw integers (e.g., `101`), making files hard to read and edit.

**Solution**: Nodes are now represented by their canonical names (e.g., `TOP_RAIL`) and accept multiple alias variations:

**Examples of accepted aliases:**
- `TOP_RAIL`, `topRail`, `t_rAiL`, `TOPRAIL`, `T_R`, `TOP_R` → all resolve to `101`
- `BOTTOM_RAIL`, `bottomRail`, `BOTRAIL`, `B_R`, `BOT_R` → all resolve to `102`
- `GROUND`, `GND` → both resolve to `100`
- `3V3`, `3.3V`, `SUPPLY_3V3` → all resolve to `103`
- Raw integers still work: `101` → `101`

**Implementation**: `parseNodeName()` function normalizes input and searches through `nanoDefines[]` and `specialDefines[]` arrays.

### 2. Boolean Value Aliases

**Problem**: Boolean values only accepted `true`/`false`.

**Solution**: Multiple boolean aliases now accepted:

**True values**: `true`, `1`, `on`, `yes`, `enabled`
**False values**: `false`, `0`, `off`, `no`, `disabled`

**Implementation**: `parseBoolean()` function normalizes and validates boolean values.

### 3. Color Names

**Problem**: Colors were stored as hex values (e.g., `0xFF0000`), not user-friendly.

**Solution**: Colors can now be specified by name or hex:

**Accepted formats:**
- Color names: `red`, `blue`, `green`, `cyan`, `magenta`, `yellow`, etc.
- Hex: `0xFF0000`
- Decimal: `16711680`

**Available colors** (from `namedColors[]` array):
- red, orange, amber, yellow, chartreuse, green, seafoam, cyan
- blue, royal blue, indigo, violet, purple, pink, magenta
- white, black, grey

**Implementation**: 
- `parseColorValue()` converts names/hex to uint32_t
- `colorValueToName()` converts uint32_t back to canonical name
- Colors are stored internally as `dimColor` for optimal LED display

### 4. Complete Net Information

**Problem**: Only custom net colors were saved. Users couldn't see computed nets or edit their colors.

**Solution**: ALL computed nets are now serialized with full metadata:

**New nets section format:**
```yaml
nets:
  - {num: 2, nodes: [TOP_R, D5, A0], color: red, user: true}
  - {num: 6, nodes: [D3, D4], color: cyan}
  - {num: 8, nodes: [GP_7, D2], anim: true}
  - {num: 10, nodes: [DAC_0, BUF_IN], name: "Power Supply", anim: true}
```

**Fields:**
- `num`: Net number (integer) - always shown
- `nodes`: List of all nodes in this net (SHORT names) - always shown
- `color`: Color name - shown ONLY if not animated
- `name`: Custom name - shown ONLY if not auto-generated "Net X"
- `user`: Flag for user-assigned color - shown ONLY if true
- `anim`: Flag for animated nets - shown ONLY if true

**Formatting rules:**
- Nets with only one node are **not serialized** (not useful for connections)
- Node names use **short format** (e.g., `D5` instead of `NANO_D5`, `TOP_R` instead of `TOP_RAIL`)
- Default/false values are **omitted** for cleaner files
- Animated nets **don't show color** (color changes during animation)
- Auto-generated names like "Net 6" are **omitted**
- Optional fields appear **at the end** of each line

**Benefits:**
- Users can see exactly what nets exist
- Users can edit any net's color by changing the `color` field and setting `user: true`
- Auto-generated colors (`user: false`) are regenerated on reload
- User-assigned colors (`user: true`) are preserved

### 5. Live File Watching

**Problem**: YAML files had to be manually reloaded after editing.

**Solution**: File modification detection in the service loop:

**How it works:**
- Every 1 second, SlotManager checks if the active slot file has been modified
- Uses FatFS `f_stat()` to compare file modification timestamps
- If file changed externally and no unsaved changes exist, auto-reloads the file
- Displays console messages: `⚡ Slot X file changed externally, reloading...`

**Implementation**: Added to `SlotManager::service()` in `States.cpp`

**Safety features:**
- Won't reload if there are unsaved changes (prevents data loss)
- Updates timestamp after auto-save to prevent false reloads
- Handles file stat errors gracefully

### 6. Human-Readable Bridge Definitions

**Before:**
```yaml
bridges:
  - {n1: 101, n2: 75, dup: 2}
  - {n1: 103, n2: 80, dup: 1}
```

**After:**
```yaml
bridges:
  - {n1: TOP_RAIL, n2: NANO_D5, dup: 2}
  - {n1: SUPPLY_3V3, n2: NANO_D10, dup: 1}
```

Users can now understand connections at a glance!

## Performance Optimization

**Concern**: Computing nets from bridges could slow down file loading/saving.

**Solution**: Nets are computed **once** immediately after loading bridges:
1. YAML file is parsed → bridges loaded into `globalState.connections.bridges[]`
2. `loadBridgesFromState()` is called → bridges copied to processing arrays
3. `getNodesToConnect()` is called → nets computed into `globalState.connections.nets[]`
4. Nets are now available for serialization (no recomputation needed)

**Architecture Note**: All net computation is now done on `globalState.connections.nets[]`. The old global `net[]` array is deprecated.

**Net Counting**: The `connections.numNets` field is dynamically computed during serialization by scanning the nets array for the highest numbered active net. This ensures accurate serialization even if the net management code doesn't explicitly update `numNets`.

**Performance characteristics:**
- `loadBridgesFromState()`: O(n) memory copy where n = number of bridges
- `getNodesToConnect()`: Uses existing optimized net computation code
- `serializeNets()`: O(n) read-only iteration over computed nets in `globalState`
- **Total**: Computed once on load, not on every save ✅

**Why this is optimal:**
- Avoids redundant computation (compute once, use many times)
- Reuses battle-tested net computation algorithms
- All data in unified state structure (no duplicate global arrays)
- Serialization is fast (just reading already-computed data)
- No performance impact on save operations

## Example YAML File

Here's what a complete user-edited YAML file looks like:

```yaml
version: 2
sourceOfTruth: bridges

bridges:
  - {n1: TOP_RAIL, n2: NANO_D5, dup: 2}
  - {n1: NANO_D3, n2: NANO_D4, dup: 1}
  - {n1: RP_GPIO_1, n2: NANO_D2, dup: 1}

nets:
  - {num: 1, nodes: [TOP_R, D5], color: red, user: true}
  - {num: 2, nodes: [D3, D4], color: cyan}
  - {num: 3, nodes: [GP_1, D2], anim: true}

power:
  topRail: 5.00
  bottomRail: 0.00
  dac0: 3.33
  dac1: 0.00

config:
  routing: {stackPaths: 2, stackRails: 3, stackDacs: 0, railPriority: 1}
  gpio:
    direction:    [1,1,1,1,1,1,1,1,1,1]
    pulls:        [0,0,0,0,0,0,0,0,0,0]
    pwmFrequency: [1000.00,1000.00,1000.00,1000.00,1000.00,1000.00,1000.00,1000.00,1000.00,1000.00]
    pwmDutyCycle: [0.50,0.50,0.50,0.50,0.50,0.50,0.50,0.50,0.50,0.50]
    pwmEnabled:   [false,false,false,false,false,false,false,false,false,false]
  uart: {txFunction: 0, rxFunction: 1}
  oled: {connected: false, lockConnection: false}
```

## User Workflow

### Editing Colors

1. Open slot file in eKilo editor or any text editor
2. Find the net you want to change in the `nets:` section
3. Change the `color:` field to any color name (e.g., `red`, `blue`, `cyan`)
4. Add `user: true` at the end
5. Save the file
6. Colors update automatically within 1 second!

**Example:**
```yaml
# Before (auto-generated color)
- {num: 6, nodes: [D3, D4], color: cyan}

# After (user-assigned color)
- {num: 6, nodes: [D3, D4], color: purple, user: true}
```

### Adding Connections

1. Add a new bridge in the `bridges:` section:
   ```yaml
   - {n1: GND, n2: NANO_A0, dup: 1}
   ```
2. Save the file
3. Connection is established automatically!

### Changing Settings

All boolean values accept aliases (true/false, 1/0, on/off, yes/no):
```yaml
config:
  oled: {connected: yes, lockConnection: off}  # Works!
  gpio:
    pwmEnabled: [on, off, on, off, on, off, on, off, on, off]  # Works!
```

But the canonical format uses numbers:
```yaml
config:
  gpio:
    pwmEnabled: [1, 0, 1, 0, 1, 0, 1, 0, 1, 0]  # Preferred format
```

## Implementation Details

### New Functions in States.cpp

**Parsing functions:**
- `parseNodeName(const String&)` - Converts name/alias to node value
- `nodeValueToString(int)` - Converts node value to canonical name
- `parseColorValue(const String&, bool&)` - Converts color name/hex to uint32_t
- `colorValueToName(uint32_t)` - Converts uint32_t to color name
- `parseBoolean(const String&, bool&)` - Converts boolean alias to bool
- `booleanToString(bool)` - Converts bool to canonical string

**Updated serialization:**
- `serializeBridges()` - Now writes node names instead of numbers
- `serializeNets()` - Now writes ALL nets with full metadata (nodes, color, user, anim)
- `serializeConfig()` - Now uses `booleanToString()` for boolean values

**Updated deserialization:**
- `deserializeBridges()` - Now parses node names using `parseNodeName()`
- `deserializeNets()` - Now parses color names and boolean flags
- `deserializeConfig()` - Now uses `parseBoolean()` for all boolean fields

**File watching:**
- `SlotManager::service()` - Added file modification detection and auto-reload

### Integration with Existing Systems

**Runtime storage:**
- The `changedNetColors[]` array is still used at runtime
- YAML system reads from and writes to this array
- File-based persistence functions in FileParsing.cpp are deprecated but kept for migration

**Migration notes:**
- Old file-based net color functions marked as DEPRECATED
- `loadChangedNetColorsFromFile()` - No longer called
- `saveChangedNetColorsToFile()` - No longer called
- Net colors now persist only via YAML state files
- Legacy functions remain for potential file migration

## Benefits

1. **User-Friendly**: Human-readable names instead of magic numbers
2. **Flexible**: Multiple alias variations accepted
3. **Transparent**: Users can see all computed nets and their properties
4. **Live**: Changes apply automatically without manual reload
5. **Safe**: Won't overwrite unsaved changes
6. **Backward Compatible**: Raw integers still work for advanced users
7. **Documented**: Color names match the existing `namedColors[]` palette

## Testing

To test the new system:

1. Load a slot
2. Save it (it will now be in the new format)
3. Open the slot file in eKilo editor
4. Try editing:
   - Change a bridge node name (e.g., `101` → `TOP_RAIL`)
   - Change a net color (e.g., `color: 0xFF0000` → `color: blue`)
   - Set `user: true` for that net
   - Change boolean values (e.g., `false` → `off`)
5. Save the file
6. Watch console for reload message
7. Verify changes applied!

## Future Enhancements

Potential improvements for future versions:

1. **Node validation**: Warn if node name is invalid
2. **Color validation**: Suggest closest match if color name not found
3. **Auto-completion**: Provide hints for valid node names in eKilo editor
4. **Conflict detection**: Warn if manual edits conflict with hardware state
5. **Migration tool**: Convert old net color files to new YAML format
6. **Diff viewer**: Show what changed when file is reloaded

## Files Modified

- `States.cpp` - Added parsing functions, updated serialization, file watching
- `States.h` - Added forward declarations
- `FileParsing.cpp` - Deprecated old net color functions
- `FileParsing.h` - Marked old functions as deprecated
- Created `YAML_ENHANCED_USER_EDITABLE.md` (this file)

## Commit Message

```
feat: Enhanced YAML state system for user editing

- Add node name aliases (TOP_RAIL, topRail, etc.)
- Add boolean value aliases (true/1/on/yes)
- Add color name support (red, blue, green, etc.)
- Serialize ALL computed nets with metadata
- Add user/anim flags for net tracking
- Add live file watching for auto-reload
- Deprecate old file-based net color persistence
- Make YAML files fully human-readable and editable

Users can now edit state files in plain text with
readable names and have changes apply automatically!
```

