# RAM Optimization Summary

## Overview
Successfully optimized RAM usage by converting static buffers to dynamic allocation and implementing lazy initialization. This frees up significant memory for MicroPython heap expansion.

## Changes Implemented

### 1. WaveGen Buffers (12KB saved) ✅
**Files Modified:** `src/WaveGen.h`, `src/WaveGen.cpp`

- Converted `_waveform_table[4096]` (8KB) to dynamic pointer allocation
- Converted `_i2c_buffer[4096]` (4KB) to dynamic pointer allocation
- Added allocation in `begin()` method with error handling
- Added deallocation in `end()` and destructor
- Added null checks in `service()`, `_buildWaveformTable()`, and `_buildI2CBuffer()`

**Impact:** 12KB freed until wavegen is first initialized via `begin()`

### 2. LogicAnalyzer TX Buffer (16KB saved) ✅
**Files Modified:** `src/LogicAnalyzer.h`, `src/LogicAnalyzer.cpp`

- Converted `txbuf[16384]` to dynamic pointer allocation
- Added allocation in `init()` method with error handling
- Added deallocation in `deinit()` method
- Added null checks in `encode_and_queue_digital()`, `encode_and_queue_analog()`, and `send_flush()`

**Impact:** 16KB freed until logic analyzer is initialized

### 3. FileParsing SafeString Buffers (5.1KB - kept for legacy support) ⚠️
**Files Modified:** `src/FileParsing.cpp`

- Added documentation noting these are LEGACY buffers
- Kept static allocation due to SafeString API constraints and extensive usage (334 references)
- Marked with TODO for future removal once all legacy file parsing is migrated to YAML

**Impact:** 0KB immediate savings, but documented for future optimization

### 4. dumpLEDs Screen Buffer (34KB saved) ✅
**Files Modified:** `src/Graphics.cpp`, `src/Graphics.h`

- Converted static `screenLines[50][700]` (34KB) to dynamic 2D array allocation
- Added lazy allocation on first call to `dumpLEDs()`
- Created `freeDumpLEDsBuffer()` helper function for manual cleanup
- Added allocation failure handling with early return

**Impact:** 34KB freed until LED dumping is first used

### 5. JDI Display (5.1KB saved) ✅
**Files Modified:** `src/Apps.cpp`

- Changed global `jdi_display` object to pointer
- Allocate in `jdiMIPdisplay()` function entry
- Deallocate on function exit
- Updated all references to use pointer syntax (`->` instead of `.`)

**Impact:** 5.1KB freed when JDI display app is not running

### 6. DMX Serial App (0.5KB saved) ✅
**Files Modified:** `src/Apps.cpp`

- Moved `universe[513]` buffer from stack to heap allocation
- Added deallocation in exit path
- Reduced stack pressure for this function

**Impact:** 0.5KB freed when DMX app not running, reduces stack pressure

### 7. INPUTBUFFERLENGTH Analysis ✅
**Files Analyzed:** `src/JumperlessDefines.h`, `src/FileParsing.cpp`

- Reviewed usage: only 1 occurrence in `FileParsing.cpp`
- Current size (4000 bytes) is appropriate for legacy file parsing
- No reduction recommended at this time

**Impact:** No change needed

### 8. States.h Field Audit ✅
**Files Analyzed:** `src/States.h`, `src/States.cpp`

**Findings:**
- `stackPaths`, `stackRails`, `stackDacs`: Used in 35 locations - ACTIVE
- `chipXY[12]`: Used in 2 locations - ACTIVE (crossbar switch states)
- GPIO configuration arrays: All actively used for GPIO/PWM/UART management
- OLED state fields: Actively used
- All fields in ConfigState are necessary and actively used

**Impact:** No unused fields found - all are necessary for current functionality

## Total RAM Savings

| Component | Savings | Status |
|-----------|---------|--------|
| WaveGen buffers | 12KB | ✅ Lazy init |
| LogicAnalyzer TX buffer | 16KB | ✅ Lazy init |
| dumpLEDs screen buffer | 34KB | ✅ Lazy alloc |
| JDI Display | 5.1KB | ✅ App-scoped |
| DMX universe buffer | 0.5KB | ✅ App-scoped |
| FileParsing buffers | 0KB | ⚠️ Legacy (future) |
| **TOTAL** | **67.6KB** | **Available for MicroPython** |

## Memory Allocation Strategy

### Lazy Initialization (System Components)
- **WaveGen**: Allocated in `begin()`, freed in `end()`
- **LogicAnalyzer**: Allocated in `init()`, freed in `deinit()`
- **dumpLEDs**: Allocated on first use, manual free via `freeDumpLEDsBuffer()`

### App-Scoped Allocation (Optional Apps)
- **JDI Display**: Allocated on app entry, freed on app exit
- **DMX App**: Universe buffer allocated on entry, freed on exit

## Testing Recommendations

1. **Heap Verification**: Use `rp2040.getFreeHeap()` before/after each feature
2. **Feature Testing**: 
   - Test wavegen with various frequencies
   - Test logic analyzer capture
   - Test LED dumping to serial
   - Test JDI display app
   - Test DMX serial app
3. **Memory Leak Detection**: Monitor heap over extended use
4. **Stress Testing**: Use multiple features sequentially to verify proper cleanup

## Future Optimization Opportunities

1. **FileParsing Legacy Buffers (5.1KB)**: Once all slot files are migrated to YAML format, remove SafeString buffers entirely
2. **SharedBuffer Analysis**: Review if 24KB SharedBuffer can be reduced or made dynamic
3. **State History**: Currently disabled (STATE_HISTORY_SIZE = 0), but if enabled in future, consider dynamic allocation

## Notes

- All changes preserve existing functionality
- Error handling added for all dynamic allocations
- Null checks added to prevent crashes if allocation fails
- Code follows existing patterns and memory [[memory:9923915]]


