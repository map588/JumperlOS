// SPDX-License-Identifier: MIT
/*
 * Fake GPIO Implementation
 *
 * Provides software-emulated GPIO pins using the crossbar switching matrix.
 * Supports both INPUT (reading via ADC) and OUTPUT (voltage source switching) modes.
 * 
 * See FakeGpio.h for API documentation.
 */

#include "FakeGpio.h"
#include "JumperlessDefines.h"
#include "States.h"
#include "CH446Q.h"
#include "FileParsing.h"
#include "NetManager.h"
#include "Peripherals.h"
#include "Commands.h"  // For waitCore2()
#include "NetsToChipConnections.h"  // For gndChipAlternator
#include <Arduino.h>
#include <cmath>
#include <cstring>  // For memset
#include <vector>
#include <hardware/sync.h>  // For __dmb()

// External references - most are declared in included headers
// lastChipXY is in CH446Q.h
// gpioState, gpioReading, gpioReadingColors, gpioNet are in Peripherals.h
// numberOfNets is in NetManager.h
// gndChipAlternator is in NetsToChipConnections.h

// Forward declarations for MicroPython API functions (defined in JumperlessMicroPythonAPI.cpp)
extern "C" {
    void jl_dac_set(int channel, float voltage, int save);
    float jl_adc_get(int channel);
    void jl_fake_gpio_set_mode(int node, int mode);  // For MicroPython Pin class
}

// Debug flag
bool debugFakeGpio = false;

// Forward declarations for new path-based functions
static int fakeGpioWriteNew(int node, int state);

// ============================================================================
// NEW Global Storage (path-based switching)
// ============================================================================
FakeGpioOutput fakeGpioOutputs[MAX_FAKE_GP_OUT];
FakeGpioInput fakeGpioInputs[MAX_FAKE_GP_IN];
int fakeGpioCurrentlyConnectedInput = -1;  // Slot of currently connected input (-1 if none)
int fakeGpioInputAdcChannel = -1;          // Which ADC channel (0-3) is used by fake GPIO inputs (-1 if not set)

static bool fakeGpioNewInitialized = false;

// ============================================================================
// LEGACY Global Storage (will be removed after migration)
// ============================================================================
FakeGpioPinConfig fakeGpioPins[MAX_FAKE_GPIO];
int adcCurrentlyConnectedPin = -1;

// Internal state
static bool fakeGpioPinsInitialized = false;
static int tryWithGndChipAlternator = 0;
static bool configurationInProgress = false;  // Prevent update hooks during config

// ============================================================================
// Internal Helper Functions
// ============================================================================

// Initialize fake GPIO pin storage (call once)
static void initFakeGpioPins() {
    if (fakeGpioPinsInitialized)
        return;

    for (int i = 0; i < MAX_FAKE_GPIO; i++) {
        fakeGpioPins[i].active = false;
        fakeGpioPins[i].node = -1;
        fakeGpioPins[i].mode = -1;
        fakeGpioPins[i].chip_k_x = -1;
        fakeGpioPins[i].chip_k_y = -1;
        fakeGpioPins[i].current_state = 0;
        fakeGpioPins[i].hasStoredState = false;
        fakeGpioPins[i].path_length = 0;
        for (int j = 0; j < 4; j++) {
            fakeGpioPins[i].path_chips[j] = -1;
            fakeGpioPins[i].path_x[j] = -1;
            fakeGpioPins[i].path_y[j] = -1;
        }
        // Initialize chipXYState to all zeros
        for (int chip = 0; chip < 12; chip++) {
            for (int y = 0; y < 8; y++) {
                fakeGpioPins[i].chipXYState[chip].connected[y] = 0;
            }
        }
    }
    fakeGpioPinsInitialized = true;
}

// Find config slot for a given node (returns -1 if not found)
static int findFakeGpioPinSlot(int node) {
    for (int i = 0; i < MAX_FAKE_GPIO; i++) {
        if (fakeGpioPins[i].active && fakeGpioPins[i].node == node) {
            return i;
        }
    }
    return -1;
}

// Find free config slot (returns -1 if all full)
static int findFreeFakeGpioPinSlot() {
    for (int i = 0; i < MAX_FAKE_GPIO; i++) {
        if (!fakeGpioPins[i].active) {
            return i;
        }
    }
    return -1;
}

// Check if a DAC is connected to anything (except the node we're configuring)
static bool isDacConnected(int dac_node, int exclude_node = -1) {
    for (int i = 0; i < globalState.connections.numPaths; i++) {
        const pathStruct& path = globalState.connections.paths[i];
        bool has_dac = (path.node1 == dac_node || path.node2 == dac_node);
        bool has_excluded = (path.node1 == exclude_node || path.node2 == exclude_node);
        
        if (has_dac && !has_excluded) {
            return true;
        }
    }
    return false;
}

// Find available DAC for a target voltage
// Returns: 0 for DAC0, 1 for DAC1, -1 if none available
static int findAvailableDAC(float target_voltage, int exclude_node = -1, float tolerance = 0.1) {
    // First priority: Check if a DAC is already set to this voltage
    if (fabs(globalState.power.dac0 - target_voltage) < tolerance) {
        return 0;
    }
    if (fabs(globalState.power.dac1 - target_voltage) < tolerance) {
        return 1;
    }
    
    // Second priority: Find an unconnected DAC
    bool dac0_connected = isDacConnected(DAC0, exclude_node);
    bool dac1_connected = isDacConnected(DAC1, exclude_node);
    
    if (!dac0_connected) {
        return 0;
    }
    if (!dac1_connected) {
        return 1;
    }
    
    return -1;  // No DAC available
}

// Helper: Reroute chip K to connect a specific node (for OUTPUT mode)
// CRITICAL FIX: Always unconditionally disconnect ALL voltage source X positions
// on the target Y line, regardless of what lastChipXY says. This prevents shorts
// when lastChipXY is stale (e.g., when buffer power or other paths were set up
// through code paths that didn't fully sync lastChipXY).
//
// Also updates chip status arrays to keep them in sync with actual hardware state.
static void rerouteChipK(int target_chip_k_x, int chip_k_y, int target_node) {
    // X positions for voltage sources on chip K
    // X4=TOP_RAIL, X5=BOTTOM_RAIL, X6=DAC1, X7=DAC0, X8=ADC0, X15=GND
    int voltage_x_positions_K[] = {4, 5, 6, 7, 15};  // Voltage sources only
    int all_special_x_K[] = {4, 5, 6, 7, 8, 9, 10, 11, 15};  // All special function X positions
    
    if (debugFakeGpio) {
        Serial.print("rerouteChipK: target_x=");
        Serial.print(target_chip_k_x);
        Serial.print(" y=");
        Serial.print(chip_k_y);
        Serial.print(" node=");
        Serial.println(target_node);
    }
    
    // STEP 1: On chip K, UNCONDITIONALLY disconnect ALL special function X positions on target Y
    // This includes voltage sources AND ADC to prevent any shorts
    // CRITICAL: Don't check lastChipXY - it might be stale! Always send disconnect commands.
    for (int i = 0; i < 9; i++) {
        int x = all_special_x_K[i];
        if (x == target_chip_k_x) continue;
        // UNCONDITIONALLY disconnect - don't trust lastChipXY
        sendXYraw(CHIP_K, x, chip_k_y, 0);
        lastChipXY[CHIP_K].connected[chip_k_y] &= ~(1 << x);
        if (debugFakeGpio) {
            Serial.print("  Disconnected K x=");
            Serial.print(x);
            Serial.print(" y=");
            Serial.println(chip_k_y);
        }
    }

    // STEP 2: Connect the new voltage source X to Y on chip K
    if (target_chip_k_x >= 0 && target_chip_k_x < 16 && chip_k_y >= 0 && chip_k_y < 8) {
        sendXYraw(CHIP_K, target_chip_k_x, chip_k_y, 1);
        lastChipXY[CHIP_K].connected[chip_k_y] |= (1 << target_chip_k_x);
        if (debugFakeGpio) {
            Serial.print("  Connected K x=");
            Serial.print(target_chip_k_x);
            Serial.print(" y=");
            Serial.println(chip_k_y);
        }
    }
    
    // STEP 3: Update chip status arrays to reflect the change
    // The xStatus tracks which net is using each X position
    // When we switch voltage sources, we need to update this
    int net = globalState.connections.chipStates[CHIP_K].yStatus[chip_k_y];
    if (net >= 0 && net < MAX_NETS) {
        // Clear old voltage source X assignments for this net on chip K
        for (int i = 0; i < 5; i++) {
            int x = voltage_x_positions_K[i];
            if (x != target_chip_k_x) {
                if (globalState.connections.chipStates[CHIP_K].xStatus[x] == net) {
                    globalState.connections.chipStates[CHIP_K].xStatus[x] = -1;
                }
            }
        }
        // Set the new voltage source X assignment
        globalState.connections.chipStates[CHIP_K].xStatus[target_chip_k_x] = net;
    }
}

// ============================================================================
// NEW Helper Functions (path-based switching)
// ============================================================================

// Initialize new fake GPIO output/input arrays
static void initFakeGpioNew() {
    if (fakeGpioNewInitialized) return;
    
    for (int i = 0; i < MAX_FAKE_GP_OUT; i++) {
        fakeGpioOutputs[i].active = false;
        fakeGpioOutputs[i].userNode = -1;
        fakeGpioOutputs[i].highVoltageNode = -1;
        fakeGpioOutputs[i].lowVoltageNode = -1;
        fakeGpioOutputs[i].currentState = 0;
        fakeGpioOutputs[i].netIndex = -1;
        fakeGpioOutputs[i].thresholdHigh = 2.0f;
        fakeGpioOutputs[i].thresholdLow = 0.8f;
        // Fast path fields
        fakeGpioOutputs[i].chipKY = -1;
        fakeGpioOutputs[i].highVoltageX = -1;
        fakeGpioOutputs[i].lowVoltageX = -1;
        fakeGpioOutputs[i].fastPathReady = false;
    }
    
    for (int i = 0; i < MAX_FAKE_GP_IN; i++) {
        fakeGpioInputs[i].active = false;
        fakeGpioInputs[i].userNode = -1;
        fakeGpioInputs[i].thresholdHigh = 2.0f;
        fakeGpioInputs[i].thresholdLow = 0.8f;
        fakeGpioInputs[i].currentState = -1;
        fakeGpioInputs[i].netIndex = -1;
        // Chip K coordinates for fast switching
        fakeGpioInputs[i].chipKY = -1;
        fakeGpioInputs[i].fastPathReady = false;
        fakeGpioInputs[i].connected = false;
    }
    
    // Reset currently connected input tracker and ADC channel
    fakeGpioCurrentlyConnectedInput = -1;
    fakeGpioInputAdcChannel = -1;  // Will be assigned on first input config
    
    fakeGpioNewInitialized = true;
}

// Find output slot by user node (-1 if not found)
int findFakeGpioOutputSlot(int userNode) {
    for (int i = 0; i < MAX_FAKE_GP_OUT; i++) {
        if (fakeGpioOutputs[i].active && fakeGpioOutputs[i].userNode == userNode) {
            return i;
        }
    }
    return -1;
}

// Find free output slot (-1 if all full)
int findFreeFakeGpioOutputSlot() {
    for (int i = 0; i < MAX_FAKE_GP_OUT; i++) {
        if (!fakeGpioOutputs[i].active) {
            return i;
        }
    }
    return -1;
}

// Find input slot by user node (-1 if not found)
int findFakeGpioInputSlot(int userNode) {
    for (int i = 0; i < MAX_FAKE_GP_IN; i++) {
        if (fakeGpioInputs[i].active && fakeGpioInputs[i].userNode == userNode) {
            return i;
        }
    }
    return -1;
}

// Find free input slot (-1 if all full)
int findFreeFakeGpioInputSlot() {
    for (int i = 0; i < MAX_FAKE_GP_IN; i++) {
        if (!fakeGpioInputs[i].active) {
            return i;
        }
    }
    return -1;
}

// Check if a node is a valid voltage source for fake GPIO
static bool isVoltageSourceNode(int node) {
    return (node == TOP_RAIL || node == BOTTOM_RAIL || 
            node == DAC0 || node == DAC1 || node == GND);
}

// Get chip K X position for a voltage source node
// Returns: X position (4-15), or -1 if not a voltage source
static int8_t voltageNodeToChipKX(int node) {
    switch (node) {
        case TOP_RAIL:    return 4;
        case BOTTOM_RAIL: return 5;
        case DAC1:        return 6;
        case DAC0:        return 7;
        case GND:         return 15;
        default:          return -1;
    }
}

// Fast voltage source switch on chip K (for OUTPUTS)
// Optimized for FakeGPIO: only disconnects the old X, connects new X
// Much faster than rerouteChipK which disconnects all 9 special X positions
// oldX: current X position to disconnect (-1 to skip disconnect)
// newX: new X position to connect
// y: Y position on chip K
static inline void fastSwitchChipK(int8_t oldX, int8_t newX, int8_t y) {
    // Disconnect old voltage source (if different from new)
    if (oldX >= 0 && oldX != newX) {
        sendXYraw(CHIP_K, oldX, y, 0);
        lastChipXY[CHIP_K].connected[y] &= ~(1 << oldX);
    }
    
    // Connect new voltage source
    if (newX >= 0) {
        sendXYraw(CHIP_K, newX, y, 1);
        lastChipXY[CHIP_K].connected[y] |= (1 << newX);
    }
}

// Fast input switch on chip K (for INPUTS)
// All inputs share a single ADC (X position from fakeGpioInputAdcChannel), switching between different Y positions
// Disconnects old input's Y from ADC, connects new input's Y to ADC
// oldY: current Y position to disconnect (-1 to skip disconnect)
// newY: new Y position to connect
// Returns: true if switch was made, false if parameters invalid
static inline bool fastSwitchInputChipK(int8_t oldY, int8_t newY) {
    const int8_t adcX = getFakeGpioInputChipKX();  // Get X position for currently selected ADC
    
    // Disconnect old input (if different from new)
    if (oldY >= 0 && oldY != newY && oldY < 8) {
        sendXYraw(CHIP_K, adcX, oldY, 0);
        lastChipXY[CHIP_K].connected[oldY] &= ~(1 << adcX);
        if (debugFakeGpio) {
            Serial.print("fastSwitchInputChipK: disconnected Y=");
            Serial.print(oldY);
            Serial.print(" from ADC");
            Serial.println(fakeGpioInputAdcChannel);
        }
    }
    
    // Connect new input
    if (newY >= 0 && newY < 8) {
        sendXYraw(CHIP_K, adcX, newY, 1);
        lastChipXY[CHIP_K].connected[newY] |= (1 << adcX);
        if (debugFakeGpio) {
            Serial.print("fastSwitchInputChipK: connected Y=");
            Serial.print(newY);
            Serial.print(" to ADC");
            Serial.println(fakeGpioInputAdcChannel);
        }
        return true;
    }
    
    return false;
}

// Connect a fake GPIO input to the shared ADC
// Disconnects the currently connected input (if any) and connects the new one
// slot: input slot to connect (-1 to disconnect all)
// Returns: true if successful
static bool connectFakeGpioInput(int slot) {
    // Can't connect if no ADC channel is assigned
    if (fakeGpioInputAdcChannel < 0 && slot >= 0) {
        if (debugFakeGpio) {
            Serial.println("connectFakeGpioInput: No ADC channel assigned");
        }
        return false;
    }
    
    int8_t adcX = getFakeGpioInputChipKX();
    
    // Get old input's Y position
    int8_t oldY = -1;
    if (fakeGpioCurrentlyConnectedInput >= 0 && 
        fakeGpioCurrentlyConnectedInput < MAX_FAKE_GP_IN) {
        FakeGpioInput& oldInput = fakeGpioInputs[fakeGpioCurrentlyConnectedInput];
        if (oldInput.active && oldInput.fastPathReady) {
            oldY = oldInput.chipKY;
            oldInput.connected = false;
        }
    }
    
    // If just disconnecting, we're done
    if (slot < 0) {
        if (oldY >= 0 && fakeGpioInputAdcChannel >= 0) {
            sendXYraw(CHIP_K, adcX, oldY, 0);
            lastChipXY[CHIP_K].connected[oldY] &= ~(1 << adcX);
        }
        fakeGpioCurrentlyConnectedInput = -1;
        return true;
    }
    
    // Validate new slot
    if (slot >= MAX_FAKE_GP_IN) return false;
    FakeGpioInput& newInput = fakeGpioInputs[slot];
    if (!newInput.active || !newInput.fastPathReady) return false;
    
    // Skip if already connected
    if (fakeGpioCurrentlyConnectedInput == slot && newInput.connected) {
        return true;
    }
    
    // Switch connections
    int8_t newY = newInput.chipKY;
    if (fastSwitchInputChipK(oldY, newY)) {
        newInput.connected = true;
        fakeGpioCurrentlyConnectedInput = slot;
        return true;
    }
    
    return false;
}

// Check if a node is a valid breadboard node (1-60)
static bool isBreadboardNode(int node) {
    return (node >= 1 && node <= 60);
}

// Get the chip K X position for the currently selected ADC
// ADC0=X8, ADC1=X9, ADC2=X10, ADC3=X11
int getFakeGpioInputChipKX() {
    if (fakeGpioInputAdcChannel < 0 || fakeGpioInputAdcChannel > 3) {
        return 8;  // Default to ADC0 if not set
    }
    return 8 + fakeGpioInputAdcChannel;  // ADC0=8, ADC1=9, ADC2=10, ADC3=11
}

// Check if a specific ADC channel is currently in use by non-FakeGPIO connections
bool isAdcInUseByOtherConnections(int adcChannel) {
    if (adcChannel < 0 || adcChannel > 3) return true;  // Invalid channel
    
    int adcNode = ADC0 + adcChannel;  // ADC0=110, ADC1=111, ADC2=112, ADC3=113
    
    // Check all bridges for connections to this ADC
    for (int i = 0; i < globalState.connections.numBridges; i++) {
        int n1 = globalState.connections.bridges[i][0];
        int n2 = globalState.connections.bridges[i][1];
        
        // Skip if this is a FakeGPIO input bridge (those are expected to use the ADC)
        if (IS_FAKE_GP_IN(n1) || IS_FAKE_GP_IN(n2)) {
            continue;
        }
        
        // If either node is our ADC, it's in use by something else
        if (n1 == adcNode || n2 == adcNode) {
            return true;
        }
    }
    
    return false;  // ADC is free
}

// Find a free ADC channel (0-3) that's not in use by other connections
// Returns: ADC channel number (0-3), or -1 if all are in use
int findFreeAdcForFakeGpioInputs() {
    for (int adc = 0; adc < 4; adc++) {
        if (!isAdcInUseByOtherConnections(adc)) {
            if (debugFakeGpio) {
                Serial.print("findFreeAdcForFakeGpioInputs: ADC");
                Serial.print(adc);
                Serial.println(" is available");
            }
            return adc;
        }
    }
    
    if (debugFakeGpio) {
        Serial.println("findFreeAdcForFakeGpioInputs: No free ADC found (0-3 all in use)");
    }
    return -1;  // All ADCs are in use
}

// Get GPIO array index for output slot (non-inline version)
int fakeGpioOutputToGpioIndex(int slot) {
    return GPIO_INDEX_FAKE_OUT(slot);
}

// Get GPIO array index for input slot (non-inline version)
int fakeGpioInputToGpioIndex(int slot) {
    return GPIO_INDEX_FAKE_IN(slot);
}

// Debug getter/setter
bool getDebugFakeGpio() {
    return debugFakeGpio;
}

void setDebugFakeGpio(bool value) {
    debugFakeGpio = value;
}

// ============================================================================
// Visual Integration Functions
// ============================================================================

// Update fake GPIO visual state (called after each read/write)
static void updateFakeGpioReading(int slot, int reading) {
    if (slot < 0 || slot >= MAX_FAKE_GPIO) return;
    
    int idx = fakeGpioSlotToAnimationIndex(slot);
    gpioReading[idx] = reading;
    
    // Set colors based on state
    if (reading == 1) {
        gpioReadingColors[idx] = 0x230205;  // Reddish for HIGH
    } else if (reading == 0) {
        gpioReadingColors[idx] = 0x052302;  // Greenish for LOW
    } else {
        gpioReadingColors[idx] = 0x040408;  // Gray for floating/unknown
    }
    
    // if (debugFakeGpio) {
    //     Serial.print("updateFakeGpioReading: slot=");
    //     Serial.print(slot);
    //     Serial.print(" idx=");
    //     Serial.print(idx);
    //     Serial.print(" reading=");
    //     Serial.print(reading);
    //     Serial.print(" gpioNet[");
    //     Serial.print(idx);
    //     Serial.print("]=");
    //     Serial.println(gpioNet[idx]);
    // }
}

// Assign fake GPIO to visual net for LED animations
static void assignFakeGpioToVisualSlot(int slot, int node) {
    if (slot < 0 || slot >= MAX_FAKE_GPIO) return;
    
    if (debugFakeGpio) {
        Serial.print("assignFakeGpioToVisualSlot: slot=");
        Serial.print(slot);
        Serial.print(" node=");
        Serial.print(node);
        Serial.print(" numberOfNets=");
        Serial.println(numberOfNets);
    }
    
    // Find which net this node belongs to
    int net = -1;
    for (int n = 0; n < numberOfNets; n++) {
        for (int i = 0; i < MAX_NODES; i++) {
            if (globalState.connections.nets[n].nodes[i] == node) {
                net = n;
                if (debugFakeGpio) {
                    Serial.print("  Found node ");
                    Serial.print(node);
                    Serial.print(" in net ");
                    Serial.println(net);
                }
                break;
            }
            if (globalState.connections.nets[n].nodes[i] == 0) break;
        }
        if (net != -1) break;
    }
    
    if (debugFakeGpio && net == -1) {
        Serial.print("  WARNING: Node ");
        Serial.print(node);
        Serial.println(" not found in any net!");
    }
    
    // Assign to animation system
    int animIdx = fakeGpioSlotToAnimationIndex(slot);
    gpioNet[animIdx] = net;
    
    // Set state based on mode
    FakeGpioPinConfig& pin = fakeGpioPins[slot];
    if (pin.mode == 0) {
        // INPUT mode - use keeper animation for visual feedback
        gpioState[animIdx] = 7;  // Bus keeper mode shows current state
    } else if (pin.mode == 1) {
        // OUTPUT mode - use output state
        gpioState[animIdx] = pin.current_state;  // 0=LOW, 1=HIGH
    }
    
    gpioReading[animIdx] = pin.current_state;
    updateFakeGpioReading(slot, pin.current_state);
    
    if (debugFakeGpio) {
        Serial.print("  Set gpioNet[");
        Serial.print(animIdx);
        Serial.print("]=");
        Serial.print(net);
        Serial.print(" gpioState[");
        Serial.print(animIdx);
        Serial.print("]=");
        Serial.print(gpioState[animIdx]);
        Serial.print(" gpioReading[");
        Serial.print(animIdx);
        Serial.print("]=");
        Serial.println(gpioReading[animIdx]);
    }
}

// ============================================================================
// Public Helper Functions
// ============================================================================

int fakeGpioSlotToAnimationIndex(int slot) {
    return 10 + slot;
}

int fakeGpioSlotToNode(int slot) {
    return FAKE_GPIO_1 + slot;
}

bool isFakeGpioVirtualNode(int node) {
    return (node >= FAKE_GPIO_1 && node <= FAKE_GPIO_32);
}

int fakeGpioNodeToSlot(int node) {
    if (!isFakeGpioVirtualNode(node)) return -1;
    return node - FAKE_GPIO_1;
}

// Re-apply all OUTPUT pins' chip K states after a refresh
// This is necessary because refreshLocalConnections() builds paths for BOTH voltage sources
// of each OUTPUT pin, which would short the rails if both chip K connections are active.
// This function ensures only the current state's voltage source is connected.
static void reapplyAllOutputPinChipKStates() {
    if (!fakeGpioPinsInitialized) return;
    
    for (int slot = 0; slot < MAX_FAKE_GPIO; slot++) {
        FakeGpioPinConfig& pin = fakeGpioPins[slot];
        if (!pin.active || pin.mode != 1) continue;  // Only OUTPUT pins
        
        // Skip if chip K coordinates aren't set up
        if (pin.chip_k_x == -1 || pin.chip_k_y == -1) continue;
        
        // Determine target based on current state
        int target_node = (pin.current_state == 1) ? pin.high_voltage_node : pin.low_voltage_node;
        if (target_node <= 0) continue;
        
        int target_x = -1;
        switch (target_node) {
            case TOP_RAIL:    target_x = 4; break;
            case BOTTOM_RAIL: target_x = 5; break;
            case DAC1:        target_x = 6; break;
            case DAC0:        target_x = 7; break;
            case GND:         target_x = 15; break;
        }
        
        if (target_x != -1) {
            rerouteChipK(target_x, pin.chip_k_y, target_node);
            
            if (debugFakeGpio) {
                Serial.print("Reapplied chip K state for OUTPUT slot ");
                Serial.print(slot);
                Serial.print(" (node ");
                Serial.print(pin.node);
                Serial.print("): state=");
                Serial.print(pin.current_state);
                Serial.print(" -> x=");
                Serial.print(target_x);
                Serial.print(", y=");
                Serial.println(pin.chip_k_y);
            }
        }
    }
}

// ============================================================================
// Initialization and State Management
// ============================================================================

void initializeFakeGpioFromLoadedState() {
    initFakeGpioPins();  // Ensure base initialization is done
    
    // Access the global restoration list from States.cpp
    extern std::vector<FakeGpioRestorationInfo> pendingFakeGpioRestorations;
    
    // If we have saved configs, use those; otherwise fall back to inference
    if (!pendingFakeGpioRestorations.empty()) {
        // PREFERRED PATH: Restore from saved configs
        // IMPORTANT: Do NOT call fakeGpioConfigInput/Output here!
        // The bridges already exist from YAML loading. Those functions would try to
        // add/remove bridges and call refreshLocalConnections, causing routing conflicts.
        // Instead, directly populate the pin config from saved data.
        
        for (const auto& info : pendingFakeGpioRestorations) {
            if (info.slot < 0 || info.slot >= MAX_FAKE_GPIO) continue;
            
            // Verify the FAKE_GPIO virtual bridge still exists
            bool bridgeExists = false;
            int fgpVirtualNode = fakeGpioSlotToNode(info.slot);
            
            for (int i = 0; i < globalState.connections.numBridges; i++) {
                int n1 = globalState.connections.bridges[i][0];
                int n2 = globalState.connections.bridges[i][1];
                
                if ((n1 == fgpVirtualNode && n2 == info.node) ||
                    (n2 == fgpVirtualNode && n1 == info.node)) {
                    bridgeExists = true;
                    break;
                }
            }
            
            if (!bridgeExists) {
                if (debugFakeGpio) {
                    Serial.print("Skipping FakeGPIO slot ");
                    Serial.print(info.slot);
                    Serial.println(" - FAKE_GPIO bridge no longer exists");
                }
                continue;
            }
            
            FakeGpioPinConfig& pin = fakeGpioPins[info.slot];
            
            // Directly populate pin config from saved data
            pin.active = true;
            pin.node = info.node;
            pin.mode = info.mode;
            pin.threshold_high = info.threshold_high;
            pin.threshold_low = info.threshold_low;
            
            if (info.mode == 0) {
                // INPUT mode - simple config, no chip K needed for switching
                pin.v_high = 0.0;
                pin.v_low = 0.0;
                pin.high_voltage_node = -1;
                pin.low_voltage_node = -1;
                pin.chip_k_x = -1;
                pin.chip_k_y = -1;
                pin.current_state = 0;
                
                // Capture current chipXY state for input readings (excluding chip K)
                // CRITICAL: Exclude chip K to avoid interfering with OUTPUT pin voltage switching
                captureCurrentChipXYStateExcludeChipK(pin.chipXYState);
                pin.hasStoredState = true;
                
            } else if (info.mode == 1) {
                // OUTPUT mode - need voltage sources and chip K coordinates
                pin.v_high = info.v_high;
                pin.v_low = info.v_low;
                
                // Use saved voltage source nodes if available, otherwise infer from bridges
                if (info.high_voltage_node > 0) {
                    pin.high_voltage_node = info.high_voltage_node;
                } else {
                    // Fallback: infer from bridges (legacy YAML without high_node/low_node)
                    pin.high_voltage_node = -1;
                    for (int i = 0; i < globalState.connections.numBridges; i++) {
                        int n1 = globalState.connections.bridges[i][0];
                        int n2 = globalState.connections.bridges[i][1];
                        if (n1 == info.node || n2 == info.node) {
                            int other = (n1 == info.node) ? n2 : n1;
                            if (other == TOP_RAIL || other == DAC0 || other == DAC1) {
                                pin.high_voltage_node = other;
                                break;
                            }
                        }
                    }
                }
                
                if (info.low_voltage_node > 0) {
                    pin.low_voltage_node = info.low_voltage_node;
                } else {
                    // Fallback: infer from bridges
                    pin.low_voltage_node = -1;
                    for (int i = 0; i < globalState.connections.numBridges; i++) {
                        int n1 = globalState.connections.bridges[i][0];
                        int n2 = globalState.connections.bridges[i][1];
                        if (n1 == info.node || n2 == info.node) {
                            int other = (n1 == info.node) ? n2 : n1;
                            if (other == GND || other == BOTTOM_RAIL || 
                                (other == DAC1 && other != pin.high_voltage_node) ||
                                (other == DAC0 && other != pin.high_voltage_node)) {
                                pin.low_voltage_node = other;
                                break;
                            }
                        }
                    }
                }
                
                // Find chip K coordinates from existing paths
                pin.chip_k_x = -1;
                pin.chip_k_y = -1;
                
                for (int i = 0; i < globalState.connections.numPaths; i++) {
                    const pathStruct& path = globalState.connections.paths[i];
                    if (path.node1 == info.node || path.node2 == info.node) {
                        for (int j = 0; j < 4; j++) {
                            if (path.chip[j] == CHIP_K) {
                                pin.chip_k_x = path.x[j];
                                pin.chip_k_y = path.y[j];
                                break;
                            }
                        }
                        if (pin.chip_k_x != -1) break;
                    }
                }
                
                pin.current_state = 0;
                
                // Capture current chipXY state
                captureCurrentChipXYState(pin.chipXYState);
                pin.hasStoredState = true;
                
                // Apply initial LOW state to chip K hardware
                if (pin.chip_k_x != -1 && pin.chip_k_y != -1 && pin.low_voltage_node > 0) {
                    int target_x = -1;
                    switch (pin.low_voltage_node) {
                        case TOP_RAIL:    target_x = 4; break;
                        case BOTTOM_RAIL: target_x = 5; break;
                        case DAC1:        target_x = 6; break;
                        case DAC0:        target_x = 7; break;
                        case GND:         target_x = 15; break;
                    }
                    
                    if (target_x != -1) {
                        rerouteChipK(target_x, pin.chip_k_y, pin.low_voltage_node);
                        updateFakeGpioReading(info.slot, 0);
                        
                        if (debugFakeGpio) {
                            Serial.print("Applied initial LOW state to chip K for slot ");
                            Serial.print(info.slot);
                            Serial.print(" (x=");
                            Serial.print(target_x);
                            Serial.print(", y=");
                            Serial.print(pin.chip_k_y);
                            Serial.println(")");
                        }
                    }
                } else if (debugFakeGpio && info.mode == 1) {
                    Serial.print("WARNING: Could not find chip K for OUTPUT slot ");
                    Serial.print(info.slot);
                    Serial.print(" (chip_k_x=");
                    Serial.print(pin.chip_k_x);
                    Serial.print(", chip_k_y=");
                    Serial.print(pin.chip_k_y);
                    Serial.print(", low_node=");
                    Serial.print(pin.low_voltage_node);
                    Serial.println(")");
                }
            }
            
            assignFakeGpioToVisualSlot(info.slot, info.node);
            
            if (debugFakeGpio) {
                Serial.print("Restored FakeGPIO slot ");
                Serial.print(info.slot);
                Serial.print(" (node ");
                Serial.print(info.node);
                Serial.print(") as ");
                Serial.print(info.mode == 0 ? "INPUT" : "OUTPUT");
                if (info.mode == 1) {
                    Serial.print(" high=");
                    Serial.print(pin.high_voltage_node);
                    Serial.print(" low=");
                    Serial.print(pin.low_voltage_node);
                }
                Serial.println();
            }
        }
        
        // Clear the restoration list after applying
        pendingFakeGpioRestorations.clear();
        return;
    }
    
    // FALLBACK PATH: Infer from bridges (legacy behavior for old YAML files)
    // SCAN 1: Find all FAKE_GPIO_x nodes in current bridges
    struct FakeGpioBridge {
        int slot;
        int userNode;
        bool found;
    };
    
    FakeGpioBridge foundPins[MAX_FAKE_GPIO];
    for (int i = 0; i < MAX_FAKE_GPIO; i++) {
        foundPins[i].slot = i;
        foundPins[i].userNode = -1;
        foundPins[i].found = false;
    }
    
    // Scan bridges for FAKE_GPIO connections
    for (int i = 0; i < globalState.connections.numBridges; i++) {
        int n1 = globalState.connections.bridges[i][0];
        int n2 = globalState.connections.bridges[i][1];
        
        int fgpNode = -1;
        int userNode = -1;
        
        if (isFakeGpioVirtualNode(n1)) {
            fgpNode = n1;
            userNode = n2;
        } else if (isFakeGpioVirtualNode(n2)) {
            fgpNode = n2;
            userNode = n1;
        }
        
        if (fgpNode != -1 && !isFakeGpioVirtualNode(userNode)) {
            int slot = fakeGpioNodeToSlot(fgpNode);
            if (slot >= 0 && slot < MAX_FAKE_GPIO) {
                foundPins[slot].userNode = userNode;
                foundPins[slot].found = true;
            }
        }
    }
    
    // SCAN 2: For each found FAKE_GPIO, determine mode and voltage sources
    for (int slot = 0; slot < MAX_FAKE_GPIO; slot++) {
        if (!foundPins[slot].found) continue;
        
        int userNode = foundPins[slot].userNode;
        
        // Check what voltage sources are connected
        bool hasTopRail = false, hasBottomRail = false, hasGnd = false;
        bool hasDac0 = false, hasDac1 = false, hasAdc0 = false;
        
        for (int i = 0; i < globalState.connections.numBridges; i++) {
            int n1 = globalState.connections.bridges[i][0];
            int n2 = globalState.connections.bridges[i][1];
            
            if (n1 == userNode || n2 == userNode) {
                int otherNode = (n1 == userNode) ? n2 : n1;
                
                if (otherNode == TOP_RAIL) hasTopRail = true;
                else if (otherNode == BOTTOM_RAIL) hasBottomRail = true;
                else if (otherNode == GND) hasGnd = true;
                else if (otherNode == DAC0) hasDac0 = true;
                else if (otherNode == DAC1) hasDac1 = true;
                else if (otherNode == ADC0) hasAdc0 = true;
            }
        }
        
        bool isOutput = (hasTopRail || hasBottomRail || hasGnd || hasDac0 || hasDac1);
        
        FakeGpioPinConfig& pin = fakeGpioPins[slot];
        pin.active = true;
        pin.node = userNode;
        
        if (isOutput) {
            // OUTPUT MODE
            pin.mode = 1;
            
            // Find chip K in paths
            pin.chip_k_x = -1;
            pin.chip_k_y = -1;
            
            for (int i = 0; i < globalState.connections.numPaths; i++) {
                const pathStruct& path = globalState.connections.paths[i];
                if (path.node1 == userNode || path.node2 == userNode) {
                    for (int j = 0; j < 4; j++) {
                        if (path.chip[j] == CHIP_K) {
                            pin.chip_k_x = path.x[j];
                            pin.chip_k_y = path.y[j];
                            break;
                        }
                    }
                    if (pin.chip_k_x != -1) break;
                }
            }
            
            // Reconstruct voltage source allocations
            if (hasTopRail) {
                pin.high_voltage_node = TOP_RAIL;
                pin.v_high = globalState.power.topRail;
            } else if (hasDac0) {
                pin.high_voltage_node = DAC0;
                pin.v_high = globalState.power.dac0;
            } else if (hasDac1) {
                pin.high_voltage_node = DAC1;
                pin.v_high = globalState.power.dac1;
            } else if (hasBottomRail) {
                pin.high_voltage_node = BOTTOM_RAIL;
                pin.v_high = globalState.power.bottomRail;
            } else if (hasGnd) {
                pin.high_voltage_node = GND;
                pin.v_high = 0.0;
            }
            
            if (hasGnd) {
                pin.low_voltage_node = GND;
                pin.v_low = 0.0;
            } else if (hasBottomRail) {
                pin.low_voltage_node = BOTTOM_RAIL;
                pin.v_low = globalState.power.bottomRail;
            } else if (hasDac1) {
                pin.low_voltage_node = DAC1;
                pin.v_low = globalState.power.dac1;
            } else if (hasDac0) {
                pin.low_voltage_node = DAC0;
                pin.v_low = globalState.power.dac0;
            } else if (hasTopRail) {
                pin.low_voltage_node = TOP_RAIL;
                pin.v_low = globalState.power.topRail;
            }
            
            pin.threshold_high = 2.0;
            pin.threshold_low = 0.8;
            pin.current_state = 0;
            
            captureCurrentChipXYState(pin.chipXYState);
            pin.hasStoredState = true;
            
        } else {
            // INPUT MODE
            pin.mode = 0;
            pin.v_high = 0.0;
            pin.v_low = 0.0;
            pin.threshold_high = 2.0;
            pin.threshold_low = 0.8;
            pin.high_voltage_node = -1;
            pin.low_voltage_node = -1;
            pin.current_state = 0;
            pin.chip_k_x = -1;
            pin.chip_k_y = -1;
            
            // CRITICAL: Exclude chip K to avoid interfering with OUTPUT pin voltage switching
            captureCurrentChipXYStateExcludeChipK(pin.chipXYState);
            pin.hasStoredState = true;
        }
        
        assignFakeGpioToVisualSlot(slot, userNode);
        
        if (debugFakeGpio) {
            Serial.print("Restored fake GPIO slot ");
            Serial.print(slot);
            Serial.print(" (node ");
            Serial.print(userNode);
            Serial.print(") as ");
            Serial.println(pin.mode == 0 ? "INPUT" : "OUTPUT");
        }
    }
}

void updateFakeGpioAfterConnectionChange(int node1, int node2) {
    if (configurationInProgress) return;  // Don't interfere during config
    
    // =========================================================================
    // Check if the new connection uses our currently selected ADC
    // If so, try to find another free ADC and switch to it
    // =========================================================================
    if (fakeGpioInputAdcChannel >= 0) {
        int currentAdcNode = ADC0 + fakeGpioInputAdcChannel;
        // Check if the new connection involves our ADC (and isn't a FakeGPIO connection)
        bool newConnectionUsesOurAdc = false;
        if (!IS_FAKE_GP_IN(node1) && !IS_FAKE_GP_IN(node2)) {
            if (node1 == currentAdcNode || node2 == currentAdcNode) {
                newConnectionUsesOurAdc = true;
            }
        }
        
        if (newConnectionUsesOurAdc) {
            // Our ADC is now in use by another connection - find a new one
            int newAdc = findFreeAdcForFakeGpioInputs();
            if (newAdc >= 0 && newAdc != fakeGpioInputAdcChannel) {
                if (debugFakeGpio) {
                    Serial.print("ADC");
                    Serial.print(fakeGpioInputAdcChannel);
                    Serial.print(" now in use, switching to ADC");
                    Serial.println(newAdc);
                }
                
                int oldChipKX = getFakeGpioInputChipKX();
                fakeGpioInputAdcChannel = newAdc;
                int newChipKX = getFakeGpioInputChipKX();
                
                // If an input was connected, reconnect to the new ADC
                if (fakeGpioCurrentlyConnectedInput >= 0) {
                    FakeGpioInput& connectedInput = fakeGpioInputs[fakeGpioCurrentlyConnectedInput];
                    if (connectedInput.fastPathReady && connectedInput.chipKY >= 0) {
                        // Disconnect old ADC X position
                        sendXYraw(CHIP_K, oldChipKX, connectedInput.chipKY, 0);
                        lastChipXY[CHIP_K].connected[connectedInput.chipKY] &= ~(1 << oldChipKX);
                        // Connect new ADC X position
                        sendXYraw(CHIP_K, newChipKX, connectedInput.chipKY, 1);
                        lastChipXY[CHIP_K].connected[connectedInput.chipKY] |= (1 << newChipKX);
                    }
                }
            } else if (newAdc < 0) {
                if (debugFakeGpio) {
                    Serial.println("WARNING: No free ADC available for FakeGPIO inputs");
                }
                // All ADCs in use - keep using current one, may have conflicts
            }
        }
    }
    
    // =========================================================================
    // NEW SYSTEM: Update fakeGpioInputs
    // =========================================================================
    for (int slot = 0; slot < MAX_FAKE_GP_IN; slot++) {
        FakeGpioInput& input = fakeGpioInputs[slot];
        if (!input.active) continue;
        
        // Check if this input's bridge still exists
        int virtualNode = FAKE_GP_IN_0 + slot;
        bool bridgeExists = false;
        for (int i = 0; i < globalState.connections.numBridges; i++) {
            int n1 = globalState.connections.bridges[i][0];
            int n2 = globalState.connections.bridges[i][1];
            if ((n1 == virtualNode && n2 == input.userNode) ||
                (n2 == virtualNode && n1 == input.userNode)) {
                bridgeExists = true;
                break;
            }
        }
        
        if (!bridgeExists) {
            // Bridge removed - deactivate this input
            if (fakeGpioCurrentlyConnectedInput == slot) {
                connectFakeGpioInput(-1);  // Disconnect
            }
            input.active = false;
            input.userNode = -1;
            input.chipKY = -1;
            input.fastPathReady = false;
            input.connected = false;
            
            int gpioIdx = GPIO_INDEX_FAKE_IN(slot);
            gpioNet[gpioIdx] = -1;
            gpioState[gpioIdx] = 0xff;
            gpioReading[gpioIdx] = 0xff;
            gpioReadingColors[gpioIdx] = 0x000000;
            
            if (debugFakeGpio) {
                Serial.print("FakeGPIO input slot ");
                Serial.print(slot);
                Serial.println(" deactivated - bridge removed");
            }
            
            // Check if this was the last active input - release ADC
            bool anyActiveInputs = false;
            for (int i = 0; i < MAX_FAKE_GP_IN; i++) {
                if (fakeGpioInputs[i].active) {
                    anyActiveInputs = true;
                    break;
                }
            }
            if (!anyActiveInputs && fakeGpioInputAdcChannel >= 0) {
                if (debugFakeGpio) {
                    Serial.print("Last input removed, releasing ADC");
                    Serial.println(fakeGpioInputAdcChannel);
                }
                fakeGpioInputAdcChannel = -1;
            }
            continue;
        }
        
        // Get the current ADC node for comparison
        int currentAdcNode = (fakeGpioInputAdcChannel >= 0) ? (ADC0 + fakeGpioInputAdcChannel) : ADC0;
        
        // Check if this input is affected by the connection change
        bool affected = (node1 == input.userNode || node2 == input.userNode ||
                        node1 == currentAdcNode || node2 == currentAdcNode ||
                        IS_FAKE_GP_IN(node1) || IS_FAKE_GP_IN(node2));
        
        if (affected) {
            // Update chip K Y from current paths
            int8_t oldChipKY = input.chipKY;
            input.chipKY = -1;
            
            for (int i = 0; i < globalState.connections.numPaths; i++) {
                const pathStruct& path = globalState.connections.paths[i];
                if (path.node1 == input.userNode || path.node2 == input.userNode) {
                    for (int j = 0; j < 4; j++) {
                        if (path.chip[j] == CHIP_K) {
                            input.chipKY = path.y[j];
                            break;
                        }
                    }
                    if (input.chipKY >= 0) break;
                }
            }
            
            input.fastPathReady = (input.chipKY >= 0);
            
            // If this input was connected and its chip K Y changed, need to reconnect
            if (input.connected && oldChipKY != input.chipKY) {
                if (input.fastPathReady) {
                    // Disconnect old Y, connect new Y
                    fastSwitchInputChipK(oldChipKY, input.chipKY);
                } else {
                    // Can't reconnect - lost fast path
                    input.connected = false;
                    if (fakeGpioCurrentlyConnectedInput == slot) {
                        fakeGpioCurrentlyConnectedInput = -1;
                    }
                }
            }
            
            if (debugFakeGpio) {
                Serial.print("Updated FakeGPIO input slot ");
                Serial.print(slot);
                Serial.print(" chipKY: ");
                Serial.print(oldChipKY);
                Serial.print(" -> ");
                Serial.println(input.chipKY);
            }
        }
    }
    
    // =========================================================================
    // NEW SYSTEM: Update fakeGpioOutputs (already handled by existing code for outputs)
    // =========================================================================
    for (int slot = 0; slot < MAX_FAKE_GP_OUT; slot++) {
        FakeGpioOutput& output = fakeGpioOutputs[slot];
        if (!output.active) continue;
        
        // Check if this output's virtual bridge still exists
        int virtualNode = FAKE_GP_OUT_0 + slot;
        bool bridgeExists = false;
        for (int i = 0; i < globalState.connections.numBridges; i++) {
            int n1 = globalState.connections.bridges[i][0];
            int n2 = globalState.connections.bridges[i][1];
            if ((n1 == virtualNode && n2 == output.userNode) ||
                (n2 == virtualNode && n1 == output.userNode)) {
                bridgeExists = true;
                break;
            }
        }
        
        if (!bridgeExists) {
            // Bridge removed - deactivate this output
            output.active = false;
            output.userNode = -1;
            output.chipKY = -1;
            output.fastPathReady = false;
            
            int gpioIdx = GPIO_INDEX_FAKE_OUT(slot);
            gpioNet[gpioIdx] = -1;
            gpioState[gpioIdx] = 0xff;
            gpioReading[gpioIdx] = 0xff;
            gpioReadingColors[gpioIdx] = 0x000000;
            
            if (debugFakeGpio) {
                Serial.print("FakeGPIO output slot ");
                Serial.print(slot);
                Serial.println(" deactivated - bridge removed");
            }
            continue;
        }
        
        // Check if this output is affected by the connection change
        bool affected = (node1 == output.userNode || node2 == output.userNode ||
                        node1 == output.highVoltageNode || node2 == output.highVoltageNode ||
                        node1 == output.lowVoltageNode || node2 == output.lowVoltageNode);
        
        if (affected) {
            // Update chip K Y from current paths
            int8_t oldChipKY = output.chipKY;
            output.chipKY = -1;
            
            for (int i = 0; i < globalState.connections.numPaths; i++) {
                const pathStruct& path = globalState.connections.paths[i];
                if (path.node1 == output.userNode || path.node2 == output.userNode) {
                    for (int j = 0; j < 4; j++) {
                        if (path.chip[j] == CHIP_K) {
                            output.chipKY = path.y[j];
                            break;
                        }
                    }
                    if (output.chipKY >= 0) break;
                }
            }
            
            output.fastPathReady = (output.chipKY >= 0 && 
                                    output.highVoltageX >= 0 && 
                                    output.lowVoltageX >= 0);
            
            if (debugFakeGpio && oldChipKY != output.chipKY) {
                Serial.print("Updated FakeGPIO output slot ");
                Serial.print(slot);
                Serial.print(" chipKY: ");
                Serial.print(oldChipKY);
                Serial.print(" -> ");
                Serial.println(output.chipKY);
            }
        }
    }
    
    // =========================================================================
    // LEGACY SYSTEM: Update fakeGpioPins (for backward compatibility)
    // =========================================================================
    if (!fakeGpioPinsInitialized) return;
    
    // CLEANUP CHECK: If relevant bridges were removed, deactivate the config
    // - For INPUT mode: check FGP_x-to-node bridge
    // - For OUTPUT mode: check node-to-voltage_source bridges
    for (int slot = 0; slot < MAX_FAKE_GPIO; slot++) {
        if (!fakeGpioPins[slot].active) continue;
        
        FakeGpioPinConfig& pin = fakeGpioPins[slot];
        int userNode = pin.node;
        bool bridgeExists = false;
        
        if (pin.mode == 0) {
            // INPUT mode: check for FGP virtual node bridge
            int fgpVirtualNode = fakeGpioSlotToNode(slot);
            for (int i = 0; i < globalState.connections.numBridges; i++) {
                int n1 = globalState.connections.bridges[i][0];
                int n2 = globalState.connections.bridges[i][1];
                
                if ((n1 == fgpVirtualNode && n2 == userNode) ||
                    (n2 == fgpVirtualNode && n1 == userNode)) {
                    bridgeExists = true;
                    break;
                }
            }
        } else if (pin.mode == 1) {
            // OUTPUT mode: check for voltage source bridges (either high or low)
            // The fake GPIO needs at least one voltage source bridge to function
            for (int i = 0; i < globalState.connections.numBridges; i++) {
                int n1 = globalState.connections.bridges[i][0];
                int n2 = globalState.connections.bridges[i][1];
                
                // Check if bridge connects userNode to either voltage source
                if ((n1 == userNode && (n2 == pin.high_voltage_node || n2 == pin.low_voltage_node)) ||
                    (n2 == userNode && (n1 == pin.high_voltage_node || n1 == pin.low_voltage_node))) {
                    bridgeExists = true;
                    break;
                }
            }
        }
        
        if (!bridgeExists) {
            // Bridge was removed - deactivate this FakeGPIO config
            if (debugFakeGpio) {
                Serial.print("FakeGPIO slot ");
                Serial.print(slot);
                Serial.print(" (node ");
                Serial.print(userNode);
                Serial.println(") deactivated - bridge removed");
            }
            
            // Clear the config
            fakeGpioPins[slot].active = false;
            fakeGpioPins[slot].node = -1;
            fakeGpioPins[slot].mode = -1;
            fakeGpioPins[slot].hasStoredState = false;
            
            // Clear visual assignment
            int animIdx = fakeGpioSlotToAnimationIndex(slot);
            gpioNet[animIdx] = -1;
            gpioState[animIdx] = 0;
            gpioReading[animIdx] = 0;
            gpioReadingColors[animIdx] = 0x000000;
        }
    }
    
    // Quick check: affected slots bitmask
    uint32_t affected = 0;
    
    if (isFakeGpioVirtualNode(node1)) {
        int slot = fakeGpioNodeToSlot(node1);
        if (slot >= 0) affected |= (1 << slot);
    }
    if (isFakeGpioVirtualNode(node2)) {
        int slot = fakeGpioNodeToSlot(node2);
        if (slot >= 0) affected |= (1 << slot);
    }
    
    // Check if any active fake GPIO uses these nodes
    for (int slot = 0; slot < MAX_FAKE_GPIO; slot++) {
        if (!fakeGpioPins[slot].active) continue;
        
        FakeGpioPinConfig& pin = fakeGpioPins[slot];
        
        if (node1 == pin.node || node2 == pin.node) {
            affected |= (1 << slot);
            continue;
        }
        
        if (pin.mode == 0) {  // INPUT
            if (node1 == ADC0 || node2 == ADC0) {
                affected |= (1 << slot);
            }
        } else if (pin.mode == 1) {  // OUTPUT
            if (node1 == pin.high_voltage_node || node2 == pin.high_voltage_node ||
                node1 == pin.low_voltage_node || node2 == pin.low_voltage_node) {
                affected |= (1 << slot);
            }
        }
    }
    
    if (affected == 0) return;  // No pins affected
    
    int updateCount = 0;
    
    for (int slot = 0; slot < MAX_FAKE_GPIO; slot++) {
        if (!(affected & (1 << slot))) continue;
        if (!fakeGpioPins[slot].active) continue;
        
        FakeGpioPinConfig& pin = fakeGpioPins[slot];
        
        if (pin.mode == 0) {  // INPUT
            // CRITICAL: Exclude chip K to avoid interfering with OUTPUT pin voltage switching
            captureCurrentChipXYStateExcludeChipK(pin.chipXYState);
            pin.hasStoredState = true;
            updateCount++;
        } else if (pin.mode == 1) {  // OUTPUT
            int old_x = pin.chip_k_x;
            int old_y = pin.chip_k_y;
            
            pin.chip_k_x = -1;
            pin.chip_k_y = -1;
            
            for (int i = 0; i < globalState.connections.numPaths; i++) {
                const pathStruct& path = globalState.connections.paths[i];
                if (path.node1 == pin.node || path.node2 == pin.node) {
                    for (int j = 0; j < 4; j++) {
                        if (path.chip[j] == CHIP_K) {
                            pin.chip_k_x = path.x[j];
                            pin.chip_k_y = path.y[j];
                            break;
                        }
                    }
                    if (pin.chip_k_x != -1) break;
                }
            }
            
            captureCurrentChipXYState(pin.chipXYState);
            pin.hasStoredState = true;
            
            if (old_x != pin.chip_k_x || old_y != pin.chip_k_y) {
                updateCount++;
            }
        }
    }
    
    if (debugFakeGpio && updateCount > 0) {
        Serial.print("Updated ");
        Serial.print(updateCount);
        Serial.println(" fake GPIO pins after connection change");
    }
}

// ============================================================================
// Configuration Functions - FORWARD DECLARATIONS for path-based versions
// ============================================================================

// Forward declarations - actual implementations are in the "NEW Path-Based" section below
// fakeGpioConfigInput is implemented below in the NEW Path-Based section

// Legacy voltage-based OUTPUT configuration - delegates to new path-based fakeGpioConfigOutputVoltage
// This overload takes floats for voltage values and auto-selects voltage sources
int fakeGpioConfigOutput(int node, float v_high, float v_low, float threshold_high, float threshold_low) {
    if (debugFakeGpio) {
        Serial.print("fakeGpioConfigOutput(float): delegating to fakeGpioConfigOutputVoltage for node ");
        Serial.println(node);
    }
    
    // Call the new path-based voltage implementation
    int slot = fakeGpioConfigOutputVoltage(node, v_high, v_low, threshold_high, threshold_low);
    
    // Return 1 for success (legacy API returns 1/0), 0 for failure
    return (slot >= 0) ? 1 : 0;
}

int fakeGpioConfig(int node, float v_high, float v_low, float threshold_high, float threshold_low, int mode) {
    // Auto-detect mode if not specified
    if (mode == -1) {
        if (v_high == 0.0 && v_low == 0.0) {
            mode = FAKE_GPIO_INPUT;
        } else {
            mode = FAKE_GPIO_OUTPUT;
        }
    }
    
    if (mode == FAKE_GPIO_INPUT) {
        return fakeGpioConfigInput(node, threshold_high, threshold_low);
    } else {
        return fakeGpioConfigOutput(node, v_high, v_low, threshold_high, threshold_low);
    }
}

// ============================================================================
// LEGACY COMPATIBILITY WRAPPERS
// These functions delegate to the new path-based implementation
// ============================================================================

// Node-based OUTPUT configuration - delegates to new path-based fakeGpioConfigOutput
int fakeGpioConfigOutputNodes(int node, int high_node, int low_node, float threshold_high, float threshold_low) {
    if (debugFakeGpio) {
        Serial.print("fakeGpioConfigOutputNodes: delegating to fakeGpioConfigOutput for node ");
        Serial.println(node);
    }
    
    // Call the new path-based implementation
    int slot = fakeGpioConfigOutput(node, high_node, low_node, threshold_high, threshold_low);
    
    // Return 1 for success (legacy API), 0 for failure
    return (slot >= 0) ? 1 : 0;
}

// ============================================================================
// Read/Write Functions (simple ADC-per-input implementation)
// ============================================================================

int fakeGpioRead(int node) {
    // First check new OUTPUT system
    int outSlot = findFakeGpioOutputSlot(node);
    if (outSlot >= 0) {
        // Return current state of output pin
        return fakeGpioOutputs[outSlot].currentState;
    }
    
    // Check new INPUT system
    int inSlot = findFakeGpioInputSlot(node);
    if (inSlot >= 0) {
        FakeGpioInput& input = fakeGpioInputs[inSlot];
        
        // Check if fast path is ready
        if (!input.fastPathReady) {
            if (debugFakeGpio) {
                Serial.print("fakeGpioRead: node ");
                Serial.print(node);
                Serial.println(" - fast path not ready");
            }
            return -1;
        }
        
        // Connect this input to ADC0 (switching from any currently connected input)
        if (!connectFakeGpioInput(inSlot)) {
            if (debugFakeGpio) {
                Serial.print("fakeGpioRead: Failed to connect input slot ");
                Serial.println(inSlot);
            }
            return -1;
        }
        
        // Brief settling time for analog switch
        delayMicroseconds(30);
        
        // Read the shared ADC channel
        float voltage = readAdcVoltage(fakeGpioInputAdcChannel, 2);
        
        if (debugFakeGpio) {
            Serial.print("fakeGpioRead: node=");
            Serial.print(node);
            Serial.print(" slot=");
            Serial.print(inSlot);
            Serial.print(" chipKY=");
            Serial.print(input.chipKY);
            Serial.print(" adc=");
            Serial.print(fakeGpioInputAdcChannel);
            Serial.print(" voltage=");
            Serial.print(voltage);
            Serial.print("V");
        }
        
        // Apply thresholds with hysteresis
        int reading = -1;  // Default: floating/unknown
        if (voltage >= input.thresholdHigh) {
            reading = 1;  // HIGH
        } else if (voltage <= input.thresholdLow) {
            reading = 0;  // LOW
        } else {
            // In hysteresis zone, keep previous state
            reading = (input.currentState >= 0) ? input.currentState : 0;
        }
        
        if (debugFakeGpio) {
            Serial.print(" reading=");
            Serial.println(reading);
        }
        
        input.currentState = reading;
        updateFakeGpioInputDisplay(inSlot);
        
        return reading;
    }
    
    if (debugFakeGpio) {
        Serial.print("fakeGpioRead: No fake GPIO configured for node ");
        Serial.println(node);
    }
    return -1;
}

int fakeGpioWrite(int node, int state) {
    // Delegate to the new path-based write function
    return fakeGpioWriteNew(node, state);
}

// Configure a fake GPIO pin in INPUT mode
// All inputs share a single ADC on chip K, but only ONE is connected at a time.
// On first config, finds a free ADC (0-3) to use for all fake GPIO inputs.
// After configuration, the input is left DISCONNECTED (chip K X not connected).
// Call fakeGpioRead() to connect and read, or let readFakeGPIO() cycle through.
// Returns slot index (0-31) on success, -1 on failure
int fakeGpioConfigInput(int node, float thresholdHigh, float thresholdLow) {
    if (debugFakeGpio) {
        Serial.print("fakeGpioConfigInput CALLED: node=");
        Serial.print(node);
        Serial.print(" threshHigh=");
        Serial.print(thresholdHigh);
        Serial.print(" threshLow=");
        Serial.println(thresholdLow);
    }
    
    initFakeGpioNew();
    
    // Validate node
    if (!isBreadboardNode(node)) {
        Serial.println("ERROR: fakeGpioConfigInput: node must be 1-60");
        return -1;
    }
    
    // Find or allocate an ADC channel if not already assigned
    if (fakeGpioInputAdcChannel < 0) {
        fakeGpioInputAdcChannel = findFreeAdcForFakeGpioInputs();
        if (fakeGpioInputAdcChannel < 0) {
            Serial.println("ERROR: fakeGpioConfigInput: No free ADC available (0-3 all in use)");
            return -1;
        }
        if (debugFakeGpio) {
            Serial.print("  Assigned ADC");
            Serial.print(fakeGpioInputAdcChannel);
            Serial.println(" for fake GPIO inputs");
        }
    }
    
    // Find or allocate slot
    int slot = findFakeGpioInputSlot(node);
    bool isReconfigure = (slot >= 0);
    if (slot == -1) {
        slot = findFreeFakeGpioInputSlot();
    }
    if (slot == -1) {
        Serial.println("ERROR: fakeGpioConfigInput: no free input slots");
        return -1;
    }
    
    FakeGpioInput& input = fakeGpioInputs[slot];
    
    // Calculate the virtual node for this slot
    int virtualNode = FAKE_GP_IN_0 + slot;


    
    // If reconfiguring existing, disconnect it if connected and remove old bridge
    if (isReconfigure && input.userNode > 0) {
        if (fakeGpioCurrentlyConnectedInput == slot) {
            connectFakeGpioInput(-1);  // Disconnect
        }
        int oldVirtualNode = FAKE_GP_IN_0 + slot;
        removeBridgeFromState(input.userNode, oldVirtualNode, true);
    }
    
    // Initialize slot BEFORE adding bridge (so router can expand correctly)
    input.active = true;
    input.userNode = node;
    input.thresholdHigh = thresholdHigh;
    input.thresholdLow = thresholdLow;
    input.currentState = -1;  // Unknown until first read
    input.netIndex = -1;  // Will be set after routing
    input.chipKY = -1;  // Will be found after routing
    input.fastPathReady = false;
    input.connected = false;  // Start disconnected
    
    if (debugFakeGpio) {
        Serial.print("  Adding bridge: node ");
        Serial.print(node);
        Serial.print(" to FAKE_GP_IN_");
        Serial.print(slot);
        Serial.print(" (virtual node ");
        Serial.print(virtualNode);
        Serial.print(") -> router expands to ADC");
        Serial.println(fakeGpioInputAdcChannel);
    }
    
    // Add bridge with virtual node - router will expand FAKE_GP_IN_x to the selected ADC
    addBridgeToState(node, virtualNode, 0, false);
    
    // Refresh connections - router expands virtual node during routing
    refreshLocalConnections(0, 1, 0);
    
    // Find net index and chip K Y position from the routed path
    for (int i = 0; i < globalState.connections.numPaths; i++) {
        const pathStruct& path = globalState.connections.paths[i];
        if (path.node1 == node || path.node2 == node) {
            input.netIndex = path.net;
            
            // Find chip K Y position from path
            for (int j = 0; j < 4; j++) {
                if (path.chip[j] == CHIP_K) {
                    input.chipKY = path.y[j];
                    break;
                }
            }
            break;
        }
    }
    
    // Enable fast path if we found chip K Y
    int8_t adcX = getFakeGpioInputChipKX();
    if (input.chipKY >= 0) {
        input.fastPathReady = true;
        
        // IMPORTANT: Disconnect the chip K X (ADC) connection after routing
        // This leaves the input in an "unconnected" state until a read is requested
        // The path from breadboard to chip K Y is still in place, just the ADC X is disconnected
        sendXYraw(CHIP_K, adcX, input.chipKY, 0);
        lastChipXY[CHIP_K].connected[input.chipKY] &= ~(1 << adcX);
        
        if (debugFakeGpio) {
            Serial.print("  Fast path enabled: chipKY=");
            Serial.print(input.chipKY);
            Serial.print(" (left disconnected from ADC");
            Serial.print(fakeGpioInputAdcChannel);
            Serial.println(")");
        }
    }
    
    if (debugFakeGpio) {
        Serial.print("fakeGpioConfigInput SUCCESS: slot=");
        Serial.print(slot);
        Serial.print(" node=");
        Serial.print(node);
        Serial.print(" chipKY=");
        Serial.print(input.chipKY);
        Serial.print(" adc=");
        Serial.print(fakeGpioInputAdcChannel);
        Serial.print(" net=");
        Serial.println(input.netIndex);
    }
    
    // Update display arrays
    updateFakeGpioInputDisplay(slot);
    
    // Trigger Core 2 to send paths
    extern volatile int sendAllPathsCore2;
    sendAllPathsCore2 = 3;
    __dmb();
    
    return slot;
}

// Remove a fake GPIO input configuration
int fakeGpioRemoveInput(int node) {
    int slot = findFakeGpioInputSlot(node);
    if (slot == -1) {
        if (debugFakeGpio) {
            Serial.print("fakeGpioRemoveInput: No input configured for node ");
            Serial.println(node);
        }
        return 0;
    }
    
    FakeGpioInput& input = fakeGpioInputs[slot];
    
    if (debugFakeGpio) {
        Serial.print("fakeGpioRemoveInput: removing slot ");
        Serial.print(slot);
        Serial.print(" node ");
        Serial.println(node);
    }
    
    // If this input is currently connected, disconnect it
    if (fakeGpioCurrentlyConnectedInput == slot) {
        connectFakeGpioInput(-1);  // Disconnect
    }
    
    // Remove the bridge
    int virtualNode = FAKE_GP_IN_0 + slot;
    removeBridgeFromState(input.userNode, virtualNode, false);  // Don't auto-refresh yet
    
    // Clear the slot
    input.active = false;
    input.userNode = -1;
    input.currentState = -1;
    input.netIndex = -1;
    input.thresholdHigh = 2.0f;
    input.thresholdLow = 0.8f;
    input.chipKY = -1;
    input.fastPathReady = false;
    input.connected = false;
    
    // Clear display arrays
    int gpioIdx = GPIO_INDEX_FAKE_IN(slot);
    gpioState[gpioIdx] = 0xff;
    gpioReading[gpioIdx] = 0xff;
    gpioNet[gpioIdx] = -1;
    gpioReadingColors[gpioIdx] = 0x040408;
    
    // Check if this was the last active input - if so, release the ADC channel
    bool anyActiveInputs = false;
    for (int i = 0; i < MAX_FAKE_GP_IN; i++) {
        if (fakeGpioInputs[i].active) {
            anyActiveInputs = true;
            break;
        }
    }
    if (!anyActiveInputs) {
        if (debugFakeGpio) {
            Serial.print("fakeGpioRemoveInput: Last input removed, releasing ADC");
            Serial.println(fakeGpioInputAdcChannel);
        }
        fakeGpioInputAdcChannel = -1;  // Release the ADC channel
    }
    
    // Refresh connections
    refreshLocalConnections(0, 1, 0);
    
    return 1;
}

// ============================================================================
// NEW Path-Based Configuration Functions
// ============================================================================

// Update fake GPIO output display arrays
void updateFakeGpioOutputDisplay(int slot) {
    if (slot < 0 || slot >= MAX_FAKE_GP_OUT) return;
    if (!fakeGpioOutputs[slot].active) return;
    
    int gpioIdx = GPIO_INDEX_FAKE_OUT(slot);
    FakeGpioOutput& output = fakeGpioOutputs[slot];
    
    // Update state
    gpioState[gpioIdx] = output.currentState;
    gpioReading[gpioIdx] = output.currentState;
    gpioNet[gpioIdx] = output.netIndex;
    
    // Set colors based on state
    if (output.currentState == 1) {
        gpioReadingColors[gpioIdx] = 0x230205;  // Reddish for HIGH
    } else {
        gpioReadingColors[gpioIdx] = 0x052302;  // Greenish for LOW
    }
    
    if (debugFakeGpio) {
        Serial.print("updateFakeGpioOutputDisplay: slot=");
        Serial.print(slot);
        Serial.print(" gpioIdx=");
        Serial.print(gpioIdx);
        Serial.print(" state=");
        Serial.print(output.currentState);
        Serial.print(" net=");
        Serial.println(output.netIndex);
    }
}

// Update fake GPIO input display arrays
void updateFakeGpioInputDisplay(int slot) {
    if (slot < 0 || slot >= MAX_FAKE_GP_IN) return;
    if (!fakeGpioInputs[slot].active) return;
    
    int gpioIdx = GPIO_INDEX_FAKE_IN(slot);
    FakeGpioInput& input = fakeGpioInputs[slot];
    
    // Update state - use bus keeper mode for inputs
    gpioState[gpioIdx] = 7;  // Bus keeper mode
    gpioReading[gpioIdx] = input.currentState;
    gpioNet[gpioIdx] = input.netIndex;
    
    // Set colors based on last read state
    if (input.currentState == 1) {
        gpioReadingColors[gpioIdx] = 0x230205;  // Reddish for HIGH
    } else if (input.currentState == 0) {
        gpioReadingColors[gpioIdx] = 0x052302;  // Greenish for LOW
    } else {
        gpioReadingColors[gpioIdx] = 0x040408;  // Gray for unknown
    }
}
unsigned long lastReadFakeGPIO = 0;
unsigned long readFakeGPIOInterval = 10000;
// ============================================================================
// Background Reading for Fake GPIO Inputs
// ============================================================================

// Background reading for fake GPIO inputs - cycles through active inputs ONE AT A TIME
// All inputs share ADC0 via chip K switching. Only the currently connected input is read.
// Call periodically from main loop to update input states and LED colors.
void readFakeGPIO(void) {
    if (micros() - lastReadFakeGPIO < readFakeGPIOInterval) {
        return;
    }
    lastReadFakeGPIO = micros();
    // Find next active input to read (round-robin starting from last + 1)
    // return;
    // Serial.println("readFakeGPIO");
    //return;
    static int lastReadSlot = -1;
    int startSlot = (lastReadSlot + 1) % MAX_FAKE_GP_IN;
    int nextSlot = -1;
    
    // Search for next active input with fast path ready
    for (int i = 0; i < MAX_FAKE_GP_IN; i++) {
        int slot = (startSlot + i) % MAX_FAKE_GP_IN;
        FakeGpioInput& input = fakeGpioInputs[slot];
        
        if (input.active && input.fastPathReady) {
            nextSlot = slot;
            break;
        }
    }
    
    // No active inputs, nothing to do
    if (nextSlot < 0) {
        return;
    }
    
    FakeGpioInput& input = fakeGpioInputs[nextSlot];
    
    // Connect this input to ADC0 (switching from any currently connected input)
    if (!connectFakeGpioInput(nextSlot)) {
        // Connection failed, try again next time
        return;
    }
    
    // Brief settling time for analog switch
    delayMicroseconds(30);
    
    // Read the shared ADC channel
    float voltage = readAdcVoltage(fakeGpioInputAdcChannel, 4);
    
    // Apply thresholds with hysteresis
    int newState = input.currentState;  // Default: keep previous
    if (voltage >= input.thresholdHigh) {
        newState = 1;  // HIGH
    } else if (voltage <= input.thresholdLow) {
        newState = 0;  // LOW
    }
    // else: in hysteresis zone - keep previous state
    
    // Update state if changed (or first read)
    if (newState != input.currentState || input.currentState < 0) {
        input.currentState = newState;
    }
    
    // Update visual display
    updateFakeGpioInputDisplay(nextSlot);
    
    // Remember which slot we just read for round-robin
    lastReadSlot = nextSlot;
}

// Initialize fake GPIO subsystem (call at startup)
void initFakeGpio() {
    initFakeGpioNew();
   //initFakeGpioPins();  // Also init legacy system for compatibility
}

// Configure a fake GPIO pin in OUTPUT mode (node-based)
// Uses virtual node expansion: adds bridge userNode-FAKE_GP_OUT_x
// Router expands FAKE_GP_OUT_x to actual voltage source based on currentState
// highNode/lowNode: voltage source nodes (TOP_RAIL, BOTTOM_RAIL, DAC0, DAC1, GND)
// Returns slot index (0-7) on success, -1 on failure
int fakeGpioConfigOutput(int node, int highNode, int lowNode, 
                         float thresholdHigh, float thresholdLow) {
    initFakeGpioNew();
    
    if (debugFakeGpio) {
        Serial.print("fakeGpioConfigOutput: node=");
        Serial.print(node);
        Serial.print(" highNode=");
        Serial.print(highNode);
        Serial.print(" lowNode=");
        Serial.println(lowNode);
    }
    
    // Validate inputs
    if (!isBreadboardNode(node)) {
        Serial.println("ERROR: fakeGpioConfigOutput: node must be 1-60");
        return -1;
    }
    
    if (!isVoltageSourceNode(highNode)) {
        Serial.print("ERROR: fakeGpioConfigOutput: invalid highNode ");
        Serial.println(highNode);
        return -1;
    }
    
    if (!isVoltageSourceNode(lowNode)) {
        Serial.print("ERROR: fakeGpioConfigOutput: invalid lowNode ");
        Serial.println(lowNode);
        return -1;
    }
    
    if (highNode == lowNode) {
        Serial.println("ERROR: fakeGpioConfigOutput: highNode and lowNode must be different");
        return -1;
    }
    
    // Find or allocate slot
    int slot = findFakeGpioOutputSlot(node);
    if (slot == -1) {
        slot = findFreeFakeGpioOutputSlot();
    }
    if (slot == -1) {
        Serial.println("ERROR: fakeGpioConfigOutput: no free output slots");
        return -1;
    }
    
    FakeGpioOutput& output = fakeGpioOutputs[slot];
    
    // Calculate the virtual node for this slot
    int virtualNode = FAKE_GP_OUT_0 + slot;
    
    // If reconfiguring existing, remove old virtual node bridge
    if (output.active && output.userNode > 0) {
        int oldVirtualNode = FAKE_GP_OUT_0 + slot;
        removeBridgeFromState(output.userNode, oldVirtualNode, false);
    }
    
    // Clean up any pre-existing voltage source bridges for this node
    // (might exist from state file load or manual bridge setup)
    removeBridgeFromState(node, TOP_RAIL, false);
    removeBridgeFromState(node, BOTTOM_RAIL, false);
    removeBridgeFromState(node, DAC0, false);
    removeBridgeFromState(node, DAC1, false);
    removeBridgeFromState(node, GND, false);
    
    // Initialize the output slot BEFORE adding bridge
    // (so router can expand the virtual node correctly)
    output.active = true;
    output.userNode = node;
    output.highVoltageNode = highNode;
    output.lowVoltageNode = lowNode;
    output.thresholdHigh = thresholdHigh;
    output.thresholdLow = thresholdLow;
    output.currentState = 0;  // Start LOW
    output.netIndex = -1;  // Will be set after routing
    // Pre-calculate chip K X positions for fast switching
    output.highVoltageX = voltageNodeToChipKX(highNode);
    output.lowVoltageX = voltageNodeToChipKX(lowNode);
    output.chipKY = -1;  // Will be found after routing
    output.fastPathReady = false;
    
    if (debugFakeGpio) {
        Serial.print("  Adding bridge: node ");
        Serial.print(node);
        Serial.print(" to FAKE_GP_OUT_");
        Serial.print(slot);
        Serial.print(" (virtual node ");
        Serial.print(virtualNode);
        Serial.println(") -> router expands to voltage source");
    }
    
    // Add bridge with virtual node - router will expand FAKE_GP_OUT_x to actual voltage source
    addBridgeToState(node, virtualNode, 0, false);
    
    // Refresh connections - router expands virtual node during routing
    refreshLocalConnections(0, 1, 0);
    
    // Find net index and chip K Y position from the routed path
    int currentVoltage = lowNode;  // currentState is 0 (LOW)
    for (int i = 0; i < globalState.connections.numPaths; i++) {
        const pathStruct& path = globalState.connections.paths[i];
        // Look for path with our node - the router expanded the virtual node
        if ((path.node1 == node && path.node2 == currentVoltage) ||
            (path.node1 == currentVoltage && path.node2 == node) ||
            (path.node1 == node) || (path.node2 == node)) {
            output.netIndex = path.net;
            
            // Find chip K Y position from path
            for (int j = 0; j < 4; j++) {
                if (path.chip[j] == CHIP_K) {
                    output.chipKY = path.y[j];
                    break;
                }
            }
            break;
        }
    }
    
    // Enable fast path if we found all coordinates
    if (output.chipKY >= 0 && output.highVoltageX >= 0 && output.lowVoltageX >= 0) {
        output.fastPathReady = true;
        if (debugFakeGpio) {
            Serial.print("  Fast path enabled: chipKY=");
            Serial.print(output.chipKY);
            Serial.print(" highX=");
            Serial.print(output.highVoltageX);
            Serial.print(" lowX=");
            Serial.println(output.lowVoltageX);
        }
    }
    
    if (debugFakeGpio) {
        Serial.print("fakeGpioConfigOutput SUCCESS: slot=");
        Serial.print(slot);
        Serial.print(" node=");
        Serial.print(node);
        Serial.print(" virtualNode=");
        Serial.print(virtualNode);
        Serial.print(" net=");
        Serial.println(output.netIndex);
    }
    
    // Update display arrays
    updateFakeGpioOutputDisplay(slot);
    
    // Trigger Core 2 to send paths
    extern volatile int sendAllPathsCore2;
    sendAllPathsCore2 = 3;
    __dmb();
    
    return slot;
}

// Configure a fake GPIO pin in OUTPUT mode (voltage-based)
// Automatically selects appropriate voltage sources
// Returns slot index (0-7) on success, -1 on failure
int fakeGpioConfigOutputVoltage(int node, float vHigh, float vLow,
                                float thresholdHigh, float thresholdLow) {
    initFakeGpioNew();
    
    if (debugFakeGpio) {
        Serial.print("fakeGpioConfigOutputVoltage: node=");
        Serial.print(node);
        Serial.print(" vHigh=");
        Serial.print(vHigh);
        Serial.print(" vLow=");
        Serial.println(vLow);
    }
    
    // Map voltages to nodes
    int highNode = -1;
    int lowNode = -1;
    
    // Select HIGH voltage source
    if (vHigh >= 4.5f) {
        highNode = TOP_RAIL;  // ~5V
    } else if (vHigh >= 3.0f && vHigh <= 3.5f) {
        highNode = BOTTOM_RAIL;  // 3.3V
    } else if (vHigh >= 0.0f && vHigh < 5.0f) {
        // Use DAC - find available one
        int dac = findAvailableDAC(vHigh, node);
        if (dac == 0) {
            highNode = DAC0;
            jl_dac_set(0, vHigh, 1);  // Set DAC0 to target voltage
        } else if (dac == 1) {
            highNode = DAC1;
            jl_dac_set(1, vHigh, 1);  // Set DAC1 to target voltage
        } else {
            Serial.println("ERROR: No DAC available for HIGH voltage");
            return -1;
        }
    } else {
        Serial.println("ERROR: Invalid HIGH voltage");
        return -1;
    }
    
    // Select LOW voltage source
    if (vLow <= 0.1f) {
        lowNode = GND;
    } else if (vLow >= 3.0f && vLow <= 3.5f) {
        lowNode = BOTTOM_RAIL;
    } else if (vLow >= 0.0f && vLow < 5.0f) {
        // Use DAC - find available one (exclude highNode if it's a DAC)
        int dac = findAvailableDAC(vLow, node);
        if (dac == 0 && highNode != DAC0) {
            lowNode = DAC0;
            jl_dac_set(0, vLow, 1);
        } else if (dac == 1 && highNode != DAC1) {
            lowNode = DAC1;
            jl_dac_set(1, vLow, 1);
        } else {
            Serial.println("ERROR: No DAC available for LOW voltage");
            return -1;
        }
    } else {
        Serial.println("ERROR: Invalid LOW voltage");
        return -1;
    }
    
    if (highNode == lowNode) {
        Serial.println("ERROR: HIGH and LOW resolved to same node");
        return -1;
    }
    
    return fakeGpioConfigOutput(node, highNode, lowNode, thresholdHigh, thresholdLow);
}

// Write fake GPIO output state
// FAST PATH: If chip K coordinates are cached, directly switch voltage source on chip K
// SLOW PATH: Full routing refresh (only used if fast path not ready)
// state: 0=LOW, 1=HIGH
// Returns 1 on success, 0 on failure
int fakeGpioWriteNew(int node, int state) {
    int slot = findFakeGpioOutputSlot(node);
    if (slot == -1) {
        if (debugFakeGpio) {
            Serial.print("fakeGpioWriteNew: No output configured for node ");
            Serial.println(node);
        }
        return 0;
    }
    
    FakeGpioOutput& output = fakeGpioOutputs[slot];
    
    // Normalize state to 0 or 1
    state = (state != 0) ? 1 : 0;
    
    // Skip if already in desired state
    if (output.currentState == state) {
        return 1;
    }
    
    // =========================================================================
    // FAST PATH: Direct chip K switching (no routing, ~50us vs ~11ms)
    // Only disconnects old voltage source and connects new one (2 SPI transactions)
    // =========================================================================
    if (output.fastPathReady) {
        int8_t oldX = (output.currentState == 1) ? output.highVoltageX : output.lowVoltageX;
        int8_t newX = (state == 1) ? output.highVoltageX : output.lowVoltageX;
        
        if (debugFakeGpio) {
            Serial.print("fakeGpioWriteNew FAST: node=");
            Serial.print(node);
            Serial.print(" state ");
            Serial.print(output.currentState);
            Serial.print(" -> ");
            Serial.print(state);
            Serial.print(" chipK y=");
            Serial.print(output.chipKY);
            Serial.print(" x: ");
            Serial.print(oldX);
            Serial.print(" -> ");
            Serial.println(newX);
        }
        
        // Fast switch: disconnect old, connect new (only 2 SPI transactions)
        fastSwitchChipK(oldX, newX, output.chipKY);
        
        // Update state and display
        output.currentState = state;
        updateFakeGpioOutputDisplay(slot);
        
        return 1;
    }
    
    // =========================================================================
    // SLOW PATH: Full routing refresh (first time or if fast path not ready)
    // =========================================================================
    int newVoltageNode = (state == 1) ? output.highVoltageNode : output.lowVoltageNode;
    
    if (debugFakeGpio) {
        Serial.print("fakeGpioWriteNew SLOW: node=");
        Serial.print(node);
        Serial.print(" state ");
        Serial.print(output.currentState);
        Serial.print(" -> ");
        Serial.print(state);
        Serial.print(" (router will expand to ");
        Serial.print(newVoltageNode);
        Serial.println(")");
    }
    
    // Update state BEFORE refreshing - router reads this to expand virtual node
    output.currentState = state;
    
    // Refresh connections - router will expand FAKE_GP_OUT_x to new voltage source
    refreshLocalConnections(0, 1, 0);
    
    // Try to enable fast path after routing (in case it wasn't ready before)
    if (!output.fastPathReady && output.highVoltageX >= 0 && output.lowVoltageX >= 0) {
        // Find chip K Y position from newly routed paths
        for (int i = 0; i < globalState.connections.numPaths; i++) {
            const pathStruct& path = globalState.connections.paths[i];
            if (path.node1 == node || path.node2 == node) {
                for (int j = 0; j < 4; j++) {
                    if (path.chip[j] == CHIP_K) {
                        output.chipKY = path.y[j];
                        output.fastPathReady = true;
                        if (debugFakeGpio) {
                            Serial.print("  Fast path now enabled: chipKY=");
                            Serial.println(output.chipKY);
                        }
                        break;
                    }
                }
                if (output.fastPathReady) break;
            }
        }
    }
    
    // Update display
    updateFakeGpioOutputDisplay(slot);
    
    // Trigger Core 2 to send paths
    extern volatile int sendAllPathsCore2;
    extern volatile bool pauseCore2;
    
    // Only trigger Core 2 if it's not paused
    if (!pauseCore2) {
        sendAllPathsCore2 = 3;  // 3 = full rebuild from bridges
        __dmb();
        waitCore2();
    }
    
    if (debugFakeGpio) {
        Serial.println("fakeGpioWriteNew SLOW: SUCCESS");
    }
    
    return 1;
}

// Write multiple fake GPIO outputs at once (batch write)
// Useful for differential pairs or multi-pin protocols
// nodes: array of node numbers
// states: array of states (0=LOW, 1=HIGH)
// count: number of pins to write
// Returns: number of successful writes
int fakeGpioWriteBatch(const int* nodes, const int* states, int count) {
    int success = 0;
    bool anySlowPath = false;
    
    // First pass: do all fast path writes
    for (int i = 0; i < count; i++) {
        int slot = findFakeGpioOutputSlot(nodes[i]);
        if (slot == -1) continue;
        
        FakeGpioOutput& output = fakeGpioOutputs[slot];
        int state = (states[i] != 0) ? 1 : 0;
        
        // Skip if already in desired state
        if (output.currentState == state) {
            success++;
            continue;
        }
        
        // Fast path
        if (output.fastPathReady) {
            int8_t oldX = (output.currentState == 1) ? output.highVoltageX : output.lowVoltageX;
            int8_t newX = (state == 1) ? output.highVoltageX : output.lowVoltageX;
            
            fastSwitchChipK(oldX, newX, output.chipKY);
            output.currentState = state;
            updateFakeGpioOutputDisplay(slot);
            success++;
        } else {
            anySlowPath = true;
        }
    }
    
    // Second pass: if any needed slow path, do ONE refresh for all
    if (anySlowPath) {
        // Update all states first
        for (int i = 0; i < count; i++) {
            int slot = findFakeGpioOutputSlot(nodes[i]);
            if (slot == -1) continue;
            
            FakeGpioOutput& output = fakeGpioOutputs[slot];
            if (output.fastPathReady) continue;  // Already done
            
            int state = (states[i] != 0) ? 1 : 0;
            if (output.currentState != state) {
                output.currentState = state;
            }
        }
        
        // Single refresh for all slow path writes
        refreshLocalConnections(0, 1, 0);
        
        // Update displays and enable fast paths
        for (int i = 0; i < count; i++) {
            int slot = findFakeGpioOutputSlot(nodes[i]);
            if (slot == -1) continue;
            
            FakeGpioOutput& output = fakeGpioOutputs[slot];
            
            // Try to enable fast path
            if (!output.fastPathReady && output.highVoltageX >= 0 && output.lowVoltageX >= 0) {
                for (int p = 0; p < globalState.connections.numPaths; p++) {
                    const pathStruct& path = globalState.connections.paths[p];
                    if (path.node1 == nodes[i] || path.node2 == nodes[i]) {
                        for (int j = 0; j < 4; j++) {
                            if (path.chip[j] == CHIP_K) {
                                output.chipKY = path.y[j];
                                output.fastPathReady = true;
                                break;
                            }
                        }
                        if (output.fastPathReady) break;
                    }
                }
            }
            
            updateFakeGpioOutputDisplay(slot);
            success++;
        }
    }
    
    return success;
}

// Remove a fake GPIO output configuration
// Returns 1 on success, 0 on failure
int fakeGpioRemoveOutput(int node) {
    int slot = findFakeGpioOutputSlot(node);
    if (slot == -1) {
        if (debugFakeGpio) {
            Serial.print("fakeGpioRemoveOutput: No output configured for node ");
            Serial.println(node);
        }
        return 0;
    }
    
    FakeGpioOutput& output = fakeGpioOutputs[slot];
    
    if (debugFakeGpio) {
        Serial.print("fakeGpioRemoveOutput: removing slot ");
        Serial.print(slot);
        Serial.print(" node ");
        Serial.println(node);
    }
    
    // Remove the virtual node bridge from state
    int virtualNode = FAKE_GP_OUT_0 + slot;
    removeBridgeFromState(output.userNode, virtualNode, true);  // Auto-refresh
    
    // Clear the slot
    output.active = false;
    output.userNode = -1;
    output.highVoltageNode = -1;
    output.lowVoltageNode = -1;
    output.currentState = 0;
    output.netIndex = -1;
    output.thresholdHigh = 2.0f;
    output.thresholdLow = 0.8f;
    // Clear fast path fields
    output.chipKY = -1;
    output.highVoltageX = -1;
    output.lowVoltageX = -1;
    output.fastPathReady = false;
    
    // Clear display arrays
    int gpioIdx = GPIO_INDEX_FAKE_OUT(slot);
    gpioState[gpioIdx] = 0xff;
    gpioReading[gpioIdx] = 0xff;
    gpioNet[gpioIdx] = -1;
    gpioReadingColors[gpioIdx] = 0x040408;
    
    return 1;
}

// Read fake GPIO output current state
int fakeGpioReadOutput(int node) {
    int slot = findFakeGpioOutputSlot(node);
    if (slot == -1) return -1;
    return fakeGpioOutputs[slot].currentState;
}

// ============================================================================
// Fast Toggle Functions
// ============================================================================

struct FakeGpioToggle {
    bool active;
    int node1, node2;
    int chips[4];
    int x[6], y[6];
    int num_connections;
};

static FakeGpioToggle fakeGpioToggles[16];
static bool fakeGpioTogglesInitialized = false;

static void initFakeGpioToggles() {
    if (fakeGpioTogglesInitialized) return;
    
    for (int i = 0; i < 16; i++) {
        fakeGpioToggles[i].active = false;
        fakeGpioToggles[i].node1 = -1;
        fakeGpioToggles[i].node2 = -1;
        fakeGpioToggles[i].num_connections = 0;
    }
    fakeGpioTogglesInitialized = true;
}

static int findToggleSlot(int node1, int node2) {
    for (int i = 0; i < 16; i++) {
        if (fakeGpioToggles[i].active &&
            ((fakeGpioToggles[i].node1 == node1 && fakeGpioToggles[i].node2 == node2) ||
             (fakeGpioToggles[i].node1 == node2 && fakeGpioToggles[i].node2 == node1))) {
            return i;
        }
    }
    return -1;
}

static int findFreeToggleSlot() {
    for (int i = 0; i < 16; i++) {
        if (!fakeGpioToggles[i].active) {
            return i;
        }
    }
    return -1;
}

int fakeGpioDisconnect(int node1, int node2) {
    initFakeGpioToggles();
    
    if (findToggleSlot(node1, node2) != -1) {
        return 0;  // Already disconnected
    }
    
    extern void bridgesToPaths(int fillUnused, int allowStacking, int startIndex);
    bridgesToPaths(1, 0, 0);  // fillUnused=1, allowStacking=0, startIndex=0 (full refresh)
    
    // Find the path
    int pathIdx = -1;
    for (int i = 0; i < globalState.connections.numPaths; i++) {
        const pathStruct& path = globalState.connections.paths[i];
        if ((path.node1 == node1 && path.node2 == node2) ||
            (path.node1 == node2 && path.node2 == node1)) {
            pathIdx = i;
            break;
        }
    }
    
    if (pathIdx == -1) return 0;
    
    int slot = findFreeToggleSlot();
    if (slot == -1) return 0;
    
    const pathStruct& path = globalState.connections.paths[pathIdx];
    FakeGpioToggle& toggle = fakeGpioToggles[slot];
    
    toggle.active = true;
    toggle.node1 = node1;
    toggle.node2 = node2;
    toggle.num_connections = 0;
    
    for (int i = 0; i < 4; i++) {
        toggle.chips[i] = path.chip[i];
        if (path.chip[i] >= 0) {
            toggle.num_connections++;
        }
    }
    
    for (int i = 0; i < 6; i++) {
        toggle.x[i] = path.x[i];
        toggle.y[i] = path.y[i];
    }
    
    // Disconnect
    for (int i = 0; i < 4 && i < toggle.num_connections; i++) {
        if (toggle.chips[i] >= 0 && toggle.x[i] >= 0 && toggle.y[i] >= 0) {
            sendXYraw(toggle.chips[i], toggle.x[i], toggle.y[i], 0);
        }
    }
    
    return 1;
}

int fakeGpioReconnect(int node1, int node2) {
    initFakeGpioToggles();
    
    int slot = findToggleSlot(node1, node2);
    if (slot == -1) return 0;
    
    FakeGpioToggle& toggle = fakeGpioToggles[slot];
    
    // Reconnect
    for (int i = 0; i < 4 && i < toggle.num_connections; i++) {
        if (toggle.chips[i] >= 0 && toggle.x[i] >= 0 && toggle.y[i] >= 0) {
            sendXYraw(toggle.chips[i], toggle.x[i], toggle.y[i], 1);
        }
    }
    
    toggle.active = false;
    toggle.node1 = -1;
    toggle.node2 = -1;
    toggle.num_connections = 0;
    
    return 1;
}

