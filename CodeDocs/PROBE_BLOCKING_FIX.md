# Probe Blocking Timer Fix

## Problem

When selecting special functions (GPIO, DAC, ADC) as the first connection in probe mode, after making the selection, the system would lock up and not allow selecting the second node. The display showed the selected function but probe touches were ignored.

## Root Cause

The `blockProbing` variable was being set to block probe reading for 800ms after special function selection:

```cpp
case GPIO_PAD: {
    function = chooseGPIO();
    blockProbing = 800;           // Set blocking duration
    blockProbingTimer = millis(); // Set start time
    break;
}
```

However, the blocking check in `justReadProbe()` and `readProbe()` was incorrectly implemented:

**Before (BROKEN):**
```cpp
if (blockProbing > 0) {
    return -1;  // Blocked forever!
}
```

This created a permanent block because:
1. `blockProbing` was set to 800 after special function selection
2. The check only looked at `if (blockProbing > 0)`
3. **The timer expiration was never checked**
4. `blockProbing` was never cleared
5. All subsequent probe reads returned -1 immediately
6. User couldn't select second node

## Solution

Implemented proper timer-based blocking that automatically expires:

**After (FIXED):**
```cpp
// Check if probing is blocked and if the block timer has expired
if (blockProbing > 0 && (millis() - blockProbingTimer < blockProbing)) {
    return -1;  // Still blocked
}
// Block expired, clear it
if (blockProbing > 0) {
    blockProbing = 0;
}
```

Now the blocking:
1. Checks both that blocking is active AND timer hasn't expired
2. Returns -1 only if still within the 800ms window
3. Auto-clears after timeout expires
4. Allows probe reading to resume normally

## Changes Made

### 1. Fixed `justReadProbe()` - Probing.cpp:3420
Added proper timer-based expiration check before the existing blocking check.

### 2. Fixed `readProbe()` - Probing.cpp:3503  
Added identical timer-based expiration check for consistency.

## Pattern

This follows the same pattern used for `blockProbeButton`:
```cpp
if (blockProbeButton > 0 && (millis() - blockProbeButtonTimer < blockProbeButton)) {
    return 0;  // Still blocked
}
```

The pattern is:
- **Variable stores duration** (e.g., `blockProbing = 800` means 800ms)
- **Timer stores start time** (e.g., `blockProbingTimer = millis()`)
- **Check compares elapsed time** (e.g., `millis() - blockProbingTimer < blockProbing`)
- **Auto-clear when expired** (e.g., `blockProbing = 0`)

## Testing

After this fix:
1. ✅ Select special function (GPIO/DAC/ADC) as first connection
2. ✅ Choose specific option (which GPIO, voltage level, etc.)
3. ✅ Wait 800ms for block to automatically expire
4. ✅ Probe reads resume working normally
5. ✅ Select second node successfully
6. ✅ Connection completes

## Related Fixes

This fix complements the earlier button handling fix (PROBE_BUTTON_LOOP_FIX.md) which addressed event consumption in loops. Together, these fixes restore full special function selection capability:

- **Button fix**: Prevents events from being consumed in menu loops
- **Blocking fix**: Allows probe reading to resume after selection

Both were needed for the complete solution.

