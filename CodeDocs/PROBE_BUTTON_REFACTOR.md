# Probe Button System Refactor

## Overview

The probe button handling system has been completely rewritten to provide more intuitive and responsive behavior while adding hold detection capabilities.

## New Behavior

### Press Detection Logic

1. **Hardware Checking**: The system checks button hardware frequently (every 12ms by default, configurable via `checkIntervalMs`)

2. **Press Registration**: When a button press is detected, it:
   - Registers the press event (available via `getButtonPress()`)
   - Starts a block timer (default 1 second, configurable via `blockDurationMs`)
   - Starts tracking hold duration

3. **Blocking Period**: After a press is registered:
   - Subsequent presses are blocked for `blockDurationMs`
   - **Block clears when button is released (after minimum time)** OR when timer expires
   - **Debounce Protection**: Block has a minimum duration (`minimumBlockMs`, default 100ms) that MUST elapse before release can clear it
   - This prevents button bounce from causing rapid re-triggering
   - Allows quick successive clicks while preventing re-triggering during holds

4. **Quick Successive Clicks**: 
   - Click → Release (wait 100ms) → Click → Release works perfectly
   - Each release clears the block after minimum time elapses
   - Each new press registers as a separate event
   - 100ms minimum delay prevents accidental double-clicks from button bounce

5. **Hold Detection**:
   - Holding a button down registers only ONCE (block prevents re-triggering)
   - Hold duration is continuously tracked
   - Hold flags are set when thresholds are reached

## New Features

### Hold Detection Flags

The system now tracks continuous button holds and sets flags when thresholds are reached:

```cpp
// Check if buttons are held
if (probeButton.isConnectHeld()) {
    // Connect button has been held for 800ms+ (default)
}

if (probeButton.isRemoveHeld()) {
    // Remove button has been held for 500ms+ (default)
}

// Get continuous hold duration
unsigned long connectDuration = probeButton.getConnectHoldDuration();
unsigned long removeDuration = probeButton.getRemoveHoldDuration();
```

### Configurable Timing

All timing parameters are adjustable:

```cpp
ProbeButton& pb = ProbeButton::getInstance();

// Adjust hardware check frequency (default 12ms)
pb.checkIntervalMs = 10;

// Adjust block duration after press (default 1000ms)
pb.blockDurationMs = 800;

// Adjust minimum block time for debounce protection (default 100ms)
pb.minimumBlockMs = 150;

// Adjust hold thresholds
pb.connectHoldThresholdMs = 1000;  // Default 800ms
pb.removeHoldThresholdMs = 600;    // Default 500ms
```

## Implementation Details

### State Machine

The service method operates in three main phases:

1. **Button Released (newState == 0)**:
   - Clears hold state immediately (hold flags, timers)
   - Updates button state if changed
   - **Debounce Protection**: Only clears block if minimum time (`minimumBlockMs`) has elapsed
   - This prevents button bounce from causing rapid re-triggering
   - Returns immediately for maximum responsiveness

2. **Button Pressed - Press Detection**:
   - Checks if block timer expired
   - Detects state changes
   - Registers press events ONLY if not blocked
   - Starts new block period when press registered

3. **Button Pressed - Hold Tracking**:
   - Continuously updates hold duration
   - Sets hold flags when thresholds reached
   - Separate tracking for connect (2) vs remove (1) buttons

### Backward Compatibility

The old global blocking variables (`blockProbeButton`, `blockProbeButtonTimer`) are still updated for backward compatibility with existing code that may reference them.

## Usage Examples

### Example 1: Basic Press Detection

```cpp
// In your service or loop
int buttonPress = probeButton.getButtonPress();

if (buttonPress == 2) {
    // Connect button was pressed
    doConnectAction();
} else if (buttonPress == 1) {
    // Remove button was pressed
    doRemoveAction();
}
```

### Example 2: Hold Detection

```cpp
// Check for long press
if (probeButton.isConnectHeld()) {
    unsigned long duration = probeButton.getConnectHoldDuration();
    
    if (duration > 2000) {
        // Button held for 2+ seconds, do special action
        doSpecialConnectAction();
    }
}
```

### Example 3: Quick Clicks

```cpp
// Quick successive clicks work perfectly (with 100ms minimum spacing)
// Click 1 → Release → (100ms debounce) → Click 2 → Release
// Both clicks register as separate events because
// each release clears the block after minimum time
// 
// Clicks faster than 100ms apart are filtered (debounce protection)
```

### Example 4: Hold to Prevent Re-trigger

```cpp
// Holding button down:
// Press → (block starts) → Continue holding...
// Only the initial press registers
// No re-triggering while held
// Release → (block clears immediately)
```

## Critical Fix: Rapid Exit from probeMode()

**The rapid "connect nodes" spam was caused by probing loop exiting immediately!**

### The Bug Sequence

1. User presses button
2. `ProbeButton::service()` sets `buttonPress=2`, `isBlocked=true`
3. `Probing::handleProbeButtonActions()` calls `getButtonPress()`, gets 2
4. Calls `probeMode(1, firstConnection)`
5. `probeMode()` calls `clearButtonState()` **which cleared `isBlocked`!**
6. Button still physically pressed, but now `isBlocked=false`
7. Next `service()` cycle: reads hardware, button still pressed (0→2 transition)
8. Since not blocked, registers **ANOTHER press**!
9. Inside `probeMode()` while loop, `readProbe()` is called
10. **`readProbe()` calls `checkProbeButton()` which was reading current button STATE**
11. Returns 2 → `readProbe()` returns `-16` → loop breaks immediately
12. Exits `probeMode()`, triggers another `probeMode()` from new press → RAPID FIRE!

### The Two-Part Fix

**Part 1: Don't Clear Block in clearButtonState()**

`clearButtonState()` now **does NOT clear `isBlocked` or `blockStartTime`**:

```cpp
void clearButtonState() { 
    currentButtonState = 0; 
    buttonPress = 0;
    // ... clear hold state ...
    // isBlocked and blockStartTime are NOT cleared - block must stay active!
}
```

This ensures that once a press is registered and blocked, the block remains active for the full duration even if something calls `clearButtonState()`. This prevents re-triggering during button bounce or while `probeMode()` is running.

**Part 2: Check for Press EVENTS, Not Current STATE**

Changed `checkProbeButton()` to use `getButtonPress()` instead of `getButtonState()`:

```cpp
int Probing::checkProbeButton( void ) {
    // Check for button press EVENTS, not current state
    // getButtonPress() consumes the event, returns non-zero only once per press
    int press = probeButton.getButtonPress();
    
    if (press != 0) {
        return press;  // New press event
    }
    
    // Check if we're in blocking period
    if (blockProbeButton > 0 && (millis() - blockProbeButtonTimer < blockProbeButton)) {
        return 0;  // Still blocked
    }
    
    return 0;  // No press event
}
```

Before, it was checking `getButtonState()` which returns the **current hardware state** - so if the button was still pressed after entering `probeMode()`, it would continuously return 2 and break the loop immediately.

Now it checks `getButtonPress()` which is an **event** - it only returns non-zero once per press, then is consumed. This prevents the same press from breaking the loop multiple times.

## Testing Behavior

To verify the new behavior:

1. **Quick Clicks**: Click button rapidly - each click should register
2. **Hold**: Hold button down - should register once, no re-triggering, probing should continue
3. **Hold Flags**: Hold connect button for 800ms+ - `isConnectHeld()` should return true
4. **Hold Duration**: While holding, check duration increases continuously
5. **Release Response**: Release button after minimum time - block should clear for next press
6. **Probing Loop**: Should NOT exit immediately after entering, should wait for actual second press

## Key Design Decisions

### Why Clear Block on Release (After Minimum Time)?

Clearing the block on button release (after minimum time) provides the best user experience:
- Quick successive clicks feel responsive (after 100ms minimum)
- No artificial delay between intentional separate presses
- Still prevents re-triggering during holds (which is the main goal)
- **Debounce protection prevents button bounce from causing rapid re-triggering**

### The Button Bounce Problem

Physical buttons can "bounce" - the electrical contact rapidly makes and breaks for a few milliseconds:
```
Press → Contact → Bounce → Contact → Bounce → Stable Contact
0       2         0        2         0        2 (stable)
```

Without the minimum block time, each bounce would register as a separate press:
1. First press (0→2): Register, block starts
2. Bounce to release (2→0): Block cleared immediately
3. Bounce back to press (0→2): Register again! (block was cleared)
4. Result: Multiple rapid presses from one physical button push

**Solution**: The `minimumBlockMs` (default 100ms) creates a debounce window. The block cannot be cleared by a release until this minimum time has elapsed. This filters out bounce while still allowing intentional quick clicks after 100ms.

### Different Hold Thresholds

Connect and Remove buttons have different hold thresholds:
- **Remove (500ms)**: Shorter threshold for faster response to "I want to disconnect something"
- **Connect (800ms)**: Longer threshold to prevent accidental hold detection during normal connection workflow

These can be adjusted based on user feedback and testing.

### Hold Tracking During Block

Even when blocked, the system continues tracking hold duration. This allows:
- Accurate hold time measurement
- Hold flags to be set even during block period
- Future features that may use hold duration (e.g., hold for 3 seconds to enter special mode)

## Migration Notes

Existing code using `getButtonPress()` will work without changes. The only difference is improved behavior:
- More responsive to quick clicks
- No re-triggering during holds
- Block clears faster (on release rather than waiting for timer)

New code can take advantage of hold detection features for richer interactions.

## Internal State

### Public State Variables
- `currentButtonState`: 0=released, 1=remove pressed, 2=connect pressed
- `lastButtonState`: Previous button state
- `buttonChanged`: True when state changed in last service cycle
- `buttonPress`: Press event (consumed by `getButtonPress()`)
- `connectHeld`: True when connect button held past threshold
- `removeHeld`: True when remove button held past threshold
- `connectHoldTime`: Continuous hold duration for connect button (ms)
- `removeHoldTime`: Continuous hold duration for remove button (ms)

### Private State Variables
- `lastCheckTime`: Last hardware check timestamp (for rate limiting)
- `isBlocked`: True when in blocking period
- `blockStartTime`: When current block period started
- `pressStartTime`: When current press started (for hold tracking)

## Performance

- **Hardware Check Frequency**: Every 12ms (83Hz) by default
- **Service Priority**: CRITICAL - runs with highest priority
- **CPU Impact**: Minimal - rate limited, fast hardware check
- **Response Time**: <12ms typical, <1ms when button released

## Future Enhancements

Potential future additions:
- Double-click detection using hold timers
- Multi-stage holds (e.g., 1s, 2s, 5s thresholds)
- Button combination detection (both pressed simultaneously)
- Configurable hold threshold per operation
- Statistics (press count, average hold time, etc.)

