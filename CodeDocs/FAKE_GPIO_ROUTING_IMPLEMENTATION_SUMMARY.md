# Fake GPIO Routing Implementation Summary

## Current Status: Infrastructure Complete, Routing Integration Needed

## What We've Implemented (COMPLETE)

### 1. Memory-Efficient ChipXY State Storage
**Files Modified**: `src/CH446Q.h`, `src/CH446Q.cpp`

Added bitfield-based chipXY state storage:
- `struct chipXYBitfield` - 16 bytes per chip (vs 128 bytes for bool array)
- 87.5% memory reduction: 192 bytes per pin vs 1536 bytes
- Functions: `captureCurrentChipXYState()`, `applyChipXYState()`
- Only sends changed connections during state application (fast switching)

### 2. Expanded Fake GPIO System to 32 Pins
**Files Modified**: `src/JumperlessMicroPythonAPI.cpp`

- Expanded `FakeGpioPinConfig` array from 16 to 32 slots
- Added `chipXYBitfield chipXYState[12]` to store complete crossbar state
- Added `hasStoredState` flag to track valid snapshots
- Updated all loop bounds to handle 32 pins
- Added `MAX_FAKE_GPIO` constant (32)

### 3. Visual Animation System Integration
**Files Modified**: `src/Peripherals.h`, `src/Peripherals.cpp`, `src/Graphics.h`, `src/Graphics.cpp`, `src/States.cpp`

Expanded arrays to support 32 fake GPIO:
- `gpioState[42]` - 10 real + 32 fake
- `gpioReading[42]` - 10 real + 32 fake  
- `gpioNet[42]` - 10 real + 32 fake
- `gpioReadingColors[42]` - 10 real + 32 fake

Added visual functions:
- `fakeGpioSlotToAnimationIndex()` - maps slot 0-31 to indices 10-41
- `updateFakeGpioReading()` - updates reading and colors
- `assignFakeGpioToVisualSlot()` - finds net and assigns animation

Updated `assignRowAnimations()` to handle fake GPIO with keeper/idle animations.

### 4. Optimized INPUT Reading
**Files Modified**: `src/JumperlessMicroPythonAPI.cpp`

- `jl_fake_gpio_config_input()` captures complete chipXY state snapshot
- `jl_fake_gpio_read()` uses caching:
  - First read: ~5-10ms (apply state)
  - Same pin repeat: ~0.1ms (just read ADC)
  - Different pin: ~1-2ms (apply new state)

### 5. OUTPUT Mode Visual Integration
**Files Modified**: `src/JumperlessMicroPythonAPI.cpp`

- Added chipXY state capture to `jl_fake_gpio_config_output()`
- Added visual updates to `jl_fake_gpio_write()`
- OUTPUT mode already worked, now has visual feedback

### 6. Debug Infrastructure
**Files Modified**: `src/Peripherals.h`, `src/Peripherals.cpp`

- Added `debugFakeGpio` flag (default: false)
- Debug output in visual assignment functions
- Debug output in animation assignment

## Current Problem: FAKE_GPIO Nodes Not Routable

### The Issue

`FAKE_GPIO_1` through `FAKE_GPIO_32` (nodes 140-171) are defined in `JumperlessDefines.h` but are **not recognized as connectable nodes** by the routing system.

**What happens now**:
```python
# Configure 12 fake GPIO inputs on rows 8-19
# Code tries to create: node → FAKE_GPIO_x → ADC0
addBridgeToState(8, FAKE_GPIO_1, 0, true)   # row 8 → FAKE_GPIO_1
addBridgeToState(FAKE_GPIO_1, ADC0, 0, true) # FAKE_GPIO_1 → ADC0
```

**Result**: Routing system ignores these bridges because `FAKE_GPIO_x` nodes are not in any chip's xMap or yMap arrays.

**Output**: `numberOfPaths: 0`, `numberOfNets: 6` (only special function nets remain)

### Why FAKE_GPIO Nodes Need to Be Routable

1. **Separate Nets**: Each fake GPIO needs its own net for independent visual display
2. **Visual Identity**: Without nets, animations can't be assigned
3. **Logical Isolation**: Each pin should have its own routing path for the chipXY snapshot

### Architecture Vision

```
User Node (Row 8) ──[Bridge 1]──> FAKE_GPIO_1 (140) ──[Bridge 2]──> ADC0 (108)
                    [Net 7]                           [Internal]
                    
User Node (Row 9) ──[Bridge 1]──> FAKE_GPIO_2 (141) ──[Bridge 2]──> ADC0 (108)
                    [Net 8]                           [Internal]
```

- **Bridge 1**: User node → FAKE_GPIO_x (creates unique net, stored in state, visible)
- **Bridge 2**: FAKE_GPIO_x → ADC0 (internal, for routing only)
- **Reading**: Use chipXY snapshot (bypasses bridges, fast switching between pins)

## What Needs to Be Implemented

### Task: Make FAKE_GPIO_1 through FAKE_GPIO_32 Routable Nodes

**Location**: `src/MatrixState.cpp` function `isConnectable()` (lines 429-450)

The routing system uses chip xMap/yMap arrays to determine if a node is connectable. FAKE_GPIO nodes need to be added to one of the special function chips (likely Chip K or L).

#### Option 1: Add to Chip K xMap (Recommended)
Chip K already handles special nodes (ADC0-3, DAC0-1, etc.). Add FAKE_GPIO to unused X positions.

**Current Chip K xMap** (from initialization data):
```
X:  0    1    2    3    4         5           6      7     8     9     10    11    12    13     14     15
   29,  59, BUF_IN, AREF, TOP_R, BOTTOM_R, DAC1, DAC0, ADC0, ADC1, ADC2, ADC3, CH_L, CH_I, CH_J, GND
```

**Proposed**: Replace unused positions or extend array to include FAKE_GPIO nodes.

Alternatively, create a **virtual chip** that doesn't physically exist but serves as a routing endpoint.

#### Option 2: Virtual Routing (Bypass Physical Chips)
Add special handling in routing code to treat FAKE_GPIO_x as "virtual endpoints":
- Don't add to chip maps (they're not physically routable)
- Allow bridges to/from them in `connectionAllowed()`
- Skip them during physical chip routing in `NetsToChipConnections.cpp`
- Use them ONLY for net creation and visual purposes

#### Option 3: Make Them Aliases for ADC Nodes
Map each FAKE_GPIO_x to an ADC dynamically:
- FAKE_GPIO_1 → ADC0
- FAKE_GPIO_2 → ADC1  
- FAKE_GPIO_3 → ADC2
- FAKE_GPIO_4 → ADC3
- FAKE_GPIO_5 → ADC0 (cycle)
- etc.

This uses existing routable nodes but requires more ADCs and may conflict with real ADC usage.

### Files That Need Changes

1. **`src/MatrixState.cpp`**
   - `isConnectable()` - return true for FAKE_GPIO_1 through FAKE_GPIO_32
   - Chip initialization - add FAKE_GPIO nodes to appropriate chip's xMap
   - `connectionAllowed()` - allow connections to/from FAKE_GPIO nodes

2. **`src/NetsToChipConnections.cpp`**
   - Handle FAKE_GPIO nodes in path finding
   - Potentially skip physical routing for FAKE_GPIO (they're virtual)

3. **`src/NetManager.cpp`**
   - Add FAKE_GPIO to `specialDefines` array for name resolution
   - Ensure net search includes FAKE_GPIO nodes

4. **`src/FileParsing.cpp`**
   - Add FAKE_GPIO name mappings if needed for string parsing

### Recommended Implementation Strategy

**Phase 1: Make FAKE_GPIO Connectable**
1. Add `FAKE_GPIO_1` through `FAKE_GPIO_32` to Chip K or L's xMap
2. Update `isConnectable()` to return true for these nodes
3. Test: `addBridgeToState(8, FAKE_GPIO_1, 0, true)` should create a bridge

**Phase 2: Virtual Routing**
1. In `NetsToChipConnections.cpp`, detect when a path involves FAKE_GPIO
2. For such paths, don't compute physical chip coordinates
3. Mark them as "virtual" - they exist for net purposes only
4. The actual physical connection uses the chipXY snapshot from INPUT config

**Phase 3: Test**
1. Run `test_fakegpio_input.py` again
2. Should see 12 separate nets (one per pin)
3. Each pin should have its own LED color/animation
4. Reading should work independently

## Current Code State

### Working:
✅ ChipXY snapshot capture and application  
✅ Fast ADC multiplexing infrastructure  
✅ Visual animation arrays expanded to 42 slots  
✅ Animation assignment logic for fake GPIO  
✅ Debug output infrastructure  

### Not Working Yet:
❌ FAKE_GPIO nodes not recognized by routing system  
❌ Bridges to FAKE_GPIO ignored (numberOfPaths: 0)  
❌ No unique nets created per fake GPIO  
❌ Visual animations don't show (no nets assigned)  

## Test Results

**Before Fix Attempt**:
- All 12 pins in one net (net 6)
- All connected to ADC0 directly
- Readings worked but affected each other
- Visual: all pins same color

**After FAKE_GPIO Routing Attempt**:
- Bridges ignored by routing system
- numberOfPaths: 0 (nothing routed)
- numberOfNets: 6 (no new nets created)
- Visual: no animations (no nets to display)

## Next Steps

Start fresh implementation focusing on making FAKE_GPIO_1 through FAKE_GPIO_32 recognized as valid routable nodes in the crossbar matrix system. The infrastructure is all in place - we just need the routing system to accept these as valid destinations.

## Files Changed in This Implementation

1. `src/CH446Q.h` - Added chipXYBitfield struct and function declarations
2. `src/CH446Q.cpp` - Added state capture/apply functions
3. `src/JumperlessMicroPythonAPI.cpp` - Updated config/read functions, added visual integration
4. `src/Peripherals.h` - Expanded GPIO array declarations
5. `src/Peripherals.cpp` - Expanded GPIO arrays, added debugFakeGpio flag
6. `src/Graphics.h` - Updated gpioReadingColors declaration
7. `src/Graphics.cpp` - Added fake GPIO handling in assignRowAnimations()
8. `src/States.cpp` - Updated extern declaration for gpioState

## Memory Usage

- **32 fake GPIO configs**: 32 × ~200 bytes = 6,400 bytes
- **Expanded GPIO arrays**: 32 × 5 bytes = 160 bytes
- **Total added**: ~6,560 bytes (reasonable for 264KB SRAM)

## Performance Characteristics

- **INPUT config**: ~10ms (one-time, during setup)
- **INPUT read (same pin)**: ~0.1ms (ADC only)
- **INPUT read (different pin)**: ~1-2ms (state switch + ADC)
- **OUTPUT write**: ~0.1ms (chip K switch)

