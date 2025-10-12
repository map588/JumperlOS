# Probe Button Loop Fix - Event vs State-Based APIs

## Problem

When selecting special functions (GPIO, DAC, ADC) as the first connection in probe mode, the user could choose options (like which GPIO or DAC) but then couldn't continue to select the second node to connect to.

### Root Cause

The new `ProbeButton` service uses an event-based system where button presses are "consumed" by `getButtonPress()`. The functions `longShortPress()` and `delayWithButton()` were using `checkProbeButton()`, which consumed button press events in their internal loops. This meant:

1. User presses button to exit a menu (e.g., after selecting GPIO)
2. The menu's loop consumes the button press event via `checkProbeButton()`
3. The outer `probeMode()` logic never sees the button press
4. User can't continue to select the second node

## Solution

Implemented a dual API approach:

### 1. Event-Based API (For One-Shot Detection)
- **Function**: `checkProbeButton()`
- **Behavior**: Returns button press events and **consumes** them
- **Use case**: When you want to detect a button press once and have it consumed
- **Example**: Main probe mode logic detecting initial press

### 2. State-Based API (For Continuous Monitoring)
- **Function**: `checkProbeButtonState()`  
- **Behavior**: Returns **current hardware state** without consuming events
- **Use case**: Loops where you continuously check if button is pressed
- **Example**: Menu selection loops, voltage selection loops

## Changes Made

### Core Functions Rewritten

#### 1. `delayWithButton()` - Probing.cpp:1894
**Before**: Used event-based `checkProbeButton()` which consumed events
```cpp
int reading = checkProbeButton();
if (reading == 1) return 1;
```

**After**: Uses state-based `probeButton.getButtonState()` without consuming events
```cpp
int currentState = probeButton.getButtonState();
if (currentState != 0 && lastSeenState == 0) {
    return currentState; // Detect press transition
}
```

#### 2. `longShortPress()` - Probing.cpp:3924
**Before**: Called `checkProbeButton()` in loops, consuming events
```cpp
int buttonState = checkProbeButton();
if (buttonState > 0) {
    while (millis() - clickTimer < pressLength) {
        if (checkProbeButton() == 0) {
            return buttonState;
        }
    }
}
```

**After**: Uses state-based API throughout
```cpp
int initialState = probeButton.getButtonState();
if (initialState == 0) return -1;

while (millis() - clickTimer < pressLength) {
    int currentState = probeButton.getButtonState();
    if (currentState == 0) {
        return whichButton; // Short press
    }
}
return longPressCode; // Long press (3 or 4)
```

#### 3. `checkProbeButtonState()` - New Function
Added alongside existing `checkProbeButton()` to provide non-consuming state access:
```cpp
int Probing::checkProbeButtonState(void) {
    // Simply return the current hardware state without consuming events
    return probeButton.getButtonState();
}
```

### Loop Updates

All loops that continuously check button state were updated to use `checkProbeButtonState()`:

1. **Probing.cpp**:
   - `measureMode()` - lines 1416, 1426
   - `chooseGPIOinputOutput()` - line 2126
   - `chooseGPIO()` - line 2236
   - `voltageSelect()` - lines 2500, 2618

2. **Apps.cpp**:
   - Probe display app - line 431

3. **Menus.cpp**:
   - Menu selection loop - line 411

4. **NetManager.cpp**:
   - Live update loop - line 1412

## API Documentation

### When to Use Each API

| Situation | Use Function | Why |
|-----------|--------------|-----|
| Main probe mode detecting initial press | `checkProbeButton()` | Want to consume event so it doesn't retrigger |
| Loop checking if button pressed to exit | `checkProbeButtonState()` | Don't consume event, allow outer logic to see it |
| Menu selection with button to cancel | `checkProbeButtonState()` | Don't consume event in menu loop |
| Voltage selection with button to confirm | `checkProbeButtonState()` | Don't consume event in selection loop |
| One-shot "did user press button?" check | `checkProbeButton()` | Consume event to prevent duplicate handling |

### Return Values

Both functions return:
- `0` = No button pressed
- `1` = Remove button pressed (rear button)
- `2` = Connect button pressed (front button)

`longShortPress()` additionally returns:
- `-1` = No button pressed
- `1` = Short remove press
- `2` = Short connect press  
- `3` = Long remove press
- `4` = Long connect press

## Testing

After these changes:
1. Select special function (GPIO/DAC/ADC) as first connection
2. Choose specific option (which GPIO, which DAC, etc.)
3. Button press should now properly exit the submenu
4. User can continue to select second node for connection
5. Connection completes successfully

## Design Philosophy

The key insight is recognizing two different use patterns:

**Event-Based**: "Tell me when something happens, then forget about it"
- Like interrupts or callbacks
- Used for triggering state transitions
- Prevents duplicate handling

**State-Based**: "What's the current situation?"
- Like polling
- Used for monitoring conditions
- Doesn't interfere with event handling

By separating these concerns, we allow submenus and loops to monitor button state without interfering with the main probe mode's event-driven architecture.

## Future Considerations

- Consider making `checkProbeButton()` use state-based by default
- Rename functions to make distinction more obvious (e.g., `waitForButtonPress()` vs `isButtonPressed()`)
- Add timeout parameters to state-based checking functions
- Consider adding button hold detection using `probeButton.isConnectHeld()` / `isRemoveHeld()`

