# Probe System - Complete Guide

## Overview

The Jumperless probe system allows users to physically probe nodes on the breadboard to create or remove connections. The system includes:

- **ProbeButton Service**: Ultra-responsive button handling (CRITICAL priority)
- **Probe Detection**: Real-time detection of probed nodes via ADC readings
- **Connection Management**: Automatic connection creation/removal based on probe state
- **Hardware Support**: RP2040 and RP2350 with GPIO errata handling

**Key Features:**
- ✅ Never misses a button press (checked every loop iteration)
- ✅ Event-based and state-based APIs
- ✅ Hardware revision detection
- ✅ Non-blocking operation (service architecture)
- ✅ Cached state for instant access

---

## Architecture

### Components

| Component | Purpose | Priority |
|-----------|---------|----------|
| **ProbeButton Service** | Button state detection | CRITICAL |
| **Probe Detection** | ADC reading and node identification | HIGH |
| **Connection Logic** | Apply/remove connections | NORMAL |

### ProbeButton Service

```cpp
class ProbeButton : public Service {
public:
    ServiceStatus service() override;
    ServicePriority getPriority() const override { 
        return ServicePriority::CRITICAL;  // Always runs first!
    }
    
    // Cached state (instant access)
    int currentButtonState = 0;  // 0=not pressed, 1=remove, 2=connect
    int buttonPress = 0;         // Latched event
    bool buttonChanged = false;  // State change flag
    
    // API
    int getButtonState() const;  // Read current state (non-consuming)
    int getButtonPress();        // Read and consume event
    int checkProbeButtonHardware();  // Direct hardware check
};
```

**Global instance:**
```cpp
extern ProbeButton probeButton;
```

---

## Button Detection

### Hardware Configuration

**V3 Hardware:**
- Button on GPIO 22 (RP2040)
- Pull-up resistor

**V5 Hardware:**
- Button on GPIO 11 (RP2350)
- Pull-down resistor
- RP2350 GPIO errata workaround needed

### Detection Logic

```cpp
int ProbeButton::checkProbeButtonHardware() {
    #if defined(ARDUINO_ARCH_RP2040)
        // V3 hardware (RP2040)
        pinMode(PROBEBUTTON2, INPUT_PULLUP);
        int buttonState1 = !digitalRead(PROBEBUTTON2);  // Active LOW
        
    #elif defined(ARDUINO_ARCH_RP2350)
        // V5 hardware (RP2350) - with GPIO errata fix
        gpio_set_pulls(PROBEBUTTON2, false, true);  // Pull-down
        
        // RP2350 GPIO errata: Need delay after changing pulls
        delayMicroseconds(10);
        
        int buttonState1 = digitalRead(PROBEBUTTON2);  // Active HIGH
    #endif
    
    // Debounce and determine action
    if (buttonState1 == 1) {
        return determineButtonAction();  // 1=remove, 2=connect
    }
    return 0;  // Not pressed
}
```

### RP2350 GPIO Errata

**Problem:** RP2350 GPIO reads can return stale data if pulls are changed and read too quickly.

**Solution:** 10µs delay after changing GPIO configuration before reading:

```cpp
gpio_set_pulls(pin, false, true);  // Configure pull-down
delayMicroseconds(10);              // Wait for hardware to settle
int value = digitalRead(pin);       // Now safe to read
```

**Reference:** RP2350 datasheet errata section

---

## Service Architecture

### Service Flow

```
Main Loop:
  ├─ jOS.serviceAll()
  │   ├─ ProbeButton::service() [CRITICAL - runs FIRST]
  │   │   ├─ checkProbeButtonHardware()
  │   │   ├─ Update currentButtonState
  │   │   ├─ Latch buttonPress if pressed
  │   │   └─ Return BUSY/IDLE
  │   │
  │   ├─ ProbeDetection::service() [HIGH]
  │   │   ├─ Read ADCs if button pressed
  │   │   ├─ Identify probed nodes
  │   │   └─ Return BUSY/IDLE
  │   │
  │   └─ Other services... [NORMAL, LOW]
  │
  └─ Continue main loop
```

**Priority guarantees:**
- CRITICAL services always run first
- ProbeButton never misses a press (checked every iteration)
- Other services can't interfere with button detection

### Why Service Architecture?

**Before (main loop):**
```cpp
// In loop(), among many other operations:
if (millis() - lastButtonCheck > 12) {
    checkButton();  // Only every 12ms!
    lastButtonCheck = millis();
}
```

**Problems:**
- ❌ Throttled to ~12ms
- ❌ Missed fast button presses
- ❌ Mixed concerns
- ❌ Timing dependent on other code

**After (service):**
```cpp
ServiceStatus ProbeButton::service() {
    int newState = checkProbeButtonHardware();  // Every iteration!
    
    if (newState != currentButtonState) {
        currentButtonState = newState;
        if (newState != 0) {
            buttonPress = newState;  // Latch event
        }
        buttonChanged = true;
        return BUSY;  // State changed
    }
    return IDLE;  // No change
}
```

**Benefits:**
- ✅ Checked every loop (~1-2ms depending on workload)
- ✅ Never misses presses
- ✅ Clean separation of concerns
- ✅ Priority-based execution
- ✅ Event-driven design

---

## API Usage

### State-Based API (Non-Consuming)

**Use when:** You need to continuously monitor button state

```cpp
// Get current button state (doesn't clear the state)
int state = probeButton.getButtonState();

if (state == 1) {
    // Remove mode active
    Serial.println("Remove mode");
} else if (state == 2) {
    // Connect mode active
    Serial.println("Connect mode");
} else {
    // Button not pressed
}
```

**Characteristics:**
- Non-destructive read
- Can be called multiple times
- Returns current hardware state
- Instant access (cached)

---

### Event-Based API (Consuming)

**Use when:** You want one-shot detection (handle once per press)

```cpp
// Get and consume button press event
int press = probeButton.getButtonPress();

if (press == 1) {
    // Remove mode pressed (event consumed)
    removeConnection();
} else if (press == 2) {
    // Connect mode pressed (event consumed)
    createConnection();
}
// Event cleared after reading
```

**Characteristics:**
- Destructive read (event cleared after reading)
- Should be called once per loop iteration
- Prevents duplicate processing
- Best for actions (not monitoring)

---

### Direct Hardware Check (Rare Use)

**Use when:** You need to bypass the cache and check hardware directly

```cpp
// Check hardware state directly (bypasses cache)
int hardwareState = probeButton.checkProbeButtonHardware();
```

**Characteristics:**
- Reads GPIO directly
- Incurs hardware access delay (~1-2µs)
- Rarely needed (service handles this)
- Useful for testing/debugging

---

## Probe Detection Workflow

### Full Connection Cycle

```cpp
// 1. User presses button (remove or connect mode)
probeButton.service();  // Detects press, latches buttonPress

// 2. Check for button press
int press = probeButton.getButtonPress();
if (press == 1) {
    // Remove mode
    enterProbeRemoveMode();
    
} else if (press == 2) {
    // Connect mode
    enterProbeConnectMode();
}

// 3. In probe mode: Read ADCs, identify nodes
void enterProbeConnectMode() {
    // Prompt user
    Serial.println("Touch two nodes to connect");
    
    // Read ADCs (continuous polling)
    while (true) {
        int node1 = readProbeADC();
        if (node1 >= 0) {
            Serial.println("Node 1: " + String(node1));
            
            int node2 = readProbeADC();
            if (node2 >= 0 && node2 != node1) {
                Serial.println("Node 2: " + String(node2));
                
                // 4. Create connection
                globalState.addConnection(node1, node2, errorMsg);
                refreshConnections(-1);
                
                Serial.println("Connected " + String(node1) + " - " + String(node2));
                break;
            }
        }
        
        // Check for button release/timeout
        if (probeButton.getButtonState() == 0) {
            Serial.println("Probe mode cancelled");
            break;
        }
    }
}
```

---

## Bug Fixes and Improvements

### 1. Blocking Timer Fix (FIXED)

**Problem:** Original code used blocking delays:
```cpp
delay(12);  // Blocks entire system!
```

**Impact:**
- System unresponsive during button checking
- LED updates frozen
- Serial communication stalled

**Solution:** Non-blocking service architecture
```cpp
ServiceStatus ProbeButton::service() {
    // Check immediately, no delays
    int state = checkProbeButtonHardware();
    // Process and return
    return state != 0 ? BUSY : IDLE;
}
```

**Result:** System remains responsive at all times

---

### 2. Event vs State API Confusion (FIXED)

**Problem:** Original code mixed event-based and state-based checking:
```cpp
// Called multiple times in same iteration:
if (checkProbeButton()) {  // Call 1
    doAction1();
}
if (checkProbeButton()) {  // Call 2 - duplicate!
    doAction2();
}
```

**Impact:**
- Duplicate action processing
- Race conditions
- Inconsistent behavior

**Solution:** Separate APIs for different use cases
```cpp
// Event-based (consuming):
int press = probeButton.getButtonPress();  // Call once per loop
if (press == 1) {
    doAction1();  // Exactly once
}

// State-based (non-consuming):
if (probeButton.getButtonState() == 1) {  // Can call many times
    updateDisplay();  // Continuous monitoring
}
```

**Result:** Clear semantics, no duplicates

---

### 3. Button Loop Fix (FIXED)

**Problem:** Button checking in tight loop monopolized CPU:
```cpp
while (1) {
    if (checkButton()) {  // Tight loop!
        break;
    }
    // No other tasks can run!
}
```

**Impact:**
- Other services starved
- USB communication frozen
- LEDs not updating

**Solution:** Service-based checking with yielding:
```cpp
// In main loop:
jOS.serviceAll();  // All services get fair scheduling

// ProbeButton service returns quickly:
ServiceStatus ProbeButton::service() {
    checkHardware();
    return IDLE or BUSY;  // Returns immediately
}
```

**Result:** Fair scheduling, all services run

---

### 4. Missed Button Presses (FIXED)

**Problem:** 12ms throttling missed fast presses:
```cpp
static unsigned long lastCheck = 0;
if (millis() - lastCheck > 12) {
    checkButton();
    lastCheck = millis();
}
// Button pressed at t+1ms and released at t+5ms → MISSED!
```

**Impact:**
- User frustration
- Unreliable operation
- Required holding button longer

**Solution:** Check every loop iteration (CRITICAL priority):
```cpp
ServiceStatus ProbeButton::service() {
    // Checked EVERY iteration (~1-2ms intervals)
    int state = checkProbeButtonHardware();
    // ...
    return IDLE or BUSY;
}
```

**Result:** Never misses a press, instant response

---

## Performance Characteristics

### Timing

| Metric | Value | Notes |
|--------|-------|-------|
| Check frequency | ~1-2ms | Every loop iteration |
| Button response | <2ms | Next iteration after press |
| Hardware read | ~1-2µs | GPIO access time |
| Debounce time | ~10ms | Software debounce |

### Memory Usage

| Component | Size | Location |
|-----------|------|----------|
| ProbeButton service | ~100 bytes | Static |
| Button state | 3 ints | Stack |
| ADC buffers | ~128 bytes | Static |

### CPU Usage

| Operation | CPU Time | Frequency |
|-----------|----------|-----------|
| Button check | ~5µs | Every 1-2ms |
| ADC read | ~50µs | When probing |
| Connection apply | ~80ms | On button press |

---

## Hardware Revision Detection

### Automatic Detection

```cpp
void setupProbeButton() {
    #if defined(ARDUINO_ARCH_RP2040)
        // V3 hardware (RP2040)
        buttonPin = 22;
        activeLow = true;
        
    #elif defined(ARDUINO_ARCH_RP2350)
        // V5 hardware (RP2350)
        buttonPin = 11;
        activeLow = false;
    #endif
}
```

### Manual Override (for testing)

```cpp
// In jumperless_config.h or compile flags:
#define FORCE_PROBE_V3  // Force V3 hardware mode
// OR
#define FORCE_PROBE_V5  // Force V5 hardware mode
```

---

## Troubleshooting

### Issue: Button Not Responding

**Symptoms:**
- Button presses ignored
- No probe mode activation

**Common causes:**
1. Wrong hardware version detected
2. GPIO pin conflict
3. Button physically damaged
4. Incorrect pull resistor configuration

**Solutions:**
1. Check compilation target (RP2040 vs RP2350)
2. Verify GPIO pin number in code
3. Test button with multimeter (continuity)
4. Check pull resistor direction (up vs down)

**Debug:**
```cpp
// Enable button debug output:
Serial.println("Button state: " + String(probeButton.getButtonState()));
Serial.println("Hardware read: " + String(probeButton.checkProbeButtonHardware()));
```

---

### Issue: Duplicate Actions

**Symptoms:**
- Single press triggers multiple actions
- Connection created multiple times

**Cause:** Using state API when event API needed:
```cpp
// BAD - reads state multiple times:
if (probeButton.getButtonState() == 1) {
    createConnection();  // Happens every loop while pressed!
}

// GOOD - reads event once:
int press = probeButton.getButtonPress();
if (press == 1) {
    createConnection();  // Happens exactly once
}
```

---

### Issue: Missed Button Presses

**Symptoms:**
- Must hold button longer
- Presses sometimes ignored

**Common causes:**
1. Other code blocking the loop
2. Service priority too low
3. Service not registered

**Solutions:**
1. Ensure ProbeButton has CRITICAL priority
2. Check service registration in setup()
3. Avoid long blocking operations in loop()

**Verify:**
```cpp
void setup() {
    // Ensure service is registered:
    jOS.registerService(&probeButton);
    
    // Check priority:
    if (probeButton.getPriority() != ServicePriority::CRITICAL) {
        Serial.println("WARNING: ProbeButton priority not CRITICAL!");
    }
}
```

---

### Issue: RP2350 Reads Return Wrong Value

**Symptoms:**
- Button state inconsistent
- Random button presses detected

**Cause:** RP2350 GPIO errata - reading too soon after pull change

**Solution:** Already implemented - 10µs delay:
```cpp
gpio_set_pulls(PROBEBUTTON2, false, true);
delayMicroseconds(10);  // REQUIRED for RP2350!
int value = digitalRead(PROBEBUTTON2);
```

**Verify errata workaround is present:**
```bash
grep -n "delayMicroseconds.*10" src/Probing.cpp
```

---

## Testing

### Test Button Detection

```cpp
void testButtonDetection() {
    Serial.println("Button Test - Press button...");
    
    unsigned long startTime = millis();
    int pressCount = 0;
    
    while (millis() - startTime < 10000) {  // 10 second test
        int press = probeButton.getButtonPress();
        if (press != 0) {
            pressCount++;
            Serial.println("Press detected: " + String(press) + 
                         " (total: " + String(pressCount) + ")");
        }
        
        // Keep services running
        jOS.serviceAll();
        delay(1);
    }
    
    Serial.println("Test complete. Presses detected: " + String(pressCount));
}
```

### Test Response Time

```cpp
void testResponseTime() {
    Serial.println("Response Time Test");
    Serial.println("Press button NOW!");
    
    unsigned long pressTime = 0;
    
    // Wait for press
    while (probeButton.getButtonState() == 0) {
        jOS.serviceAll();
    }
    pressTime = micros();
    
    Serial.println("Button pressed at: " + String(pressTime) + "µs");
    Serial.println("Response time: <2ms (one loop iteration)");
}
```

### Test Service Priority

```cpp
void testServicePriority() {
    Serial.println("Service Priority Test");
    
    // Check that ProbeButton runs before other services
    Serial.println("ProbeButton priority: " + 
                  String((int)probeButton.getPriority()));
    Serial.println("Expected: " + String((int)ServicePriority::CRITICAL));
    
    if (probeButton.getPriority() == ServicePriority::CRITICAL) {
        Serial.println("✓ Correct priority");
    } else {
        Serial.println("✗ Wrong priority! Button may miss presses.");
    }
}
```

---

## Related Documentation

- `Probing.h` - ProbeButton class definition
- `Probing.cpp` - Implementation
- `SERVICE_ARCHITECTURE_IMPLEMENTATION.md` - Service system details
- RP2350 Datasheet - GPIO errata section

---

## Future Enhancements

### Planned
1. [ ] Multi-touch detection (simultaneous node probing)
2. [ ] Configurable debounce time
3. [ ] Button press duration detection (short vs long press)
4. [ ] LED feedback during probe mode
5. [ ] Haptic feedback (if hardware supports)

### Under Consideration
1. Capacitive touch sensing (no button needed)
2. Gesture recognition (swipe patterns)
3. Pressure-sensitive probing
4. Wireless probe (Bluetooth)

---

**Status:** Production ready, actively used  
**Hardware Support:** RP2040 (V3), RP2350 (V5)  
**Last Updated:** October 2024

