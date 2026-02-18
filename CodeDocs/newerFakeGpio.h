// SPDX-License-Identifier: MIT
#ifndef FAKEGPIO_H
#define FAKEGPIO_H

#include "JumperlessDefines.h"  // For FAKE_GP_OUT_*, FAKE_GP_IN_*
#include "CH446Q.h"  // For chipXYBitfield (legacy FakeGpioPinConfig)

// ============================================================================
// Fake GPIO Output Structure
// ============================================================================
// Uses virtual node expansion: bridge stays as userNode-FAKE_GP_OUT_x
// Router expands FAKE_GP_OUT_x to actual voltage source based on currentState

struct FakeGpioOutput {
    bool active;                // Is this slot in use?
    int userNode;               // Breadboard node (e.g., 10)
    int highVoltageNode;        // TOP_RAIL, DAC0, DAC1, BOTTOM_RAIL, or GND
    int lowVoltageNode;         // TOP_RAIL, DAC0, DAC1, BOTTOM_RAIL, or GND
    int currentState;           // 0=LOW, 1=HIGH
    int netIndex;               // Net index for LED display
    float thresholdHigh;        // Threshold for reading back (API compatibility)
    float thresholdLow;
    // Cached chip K coordinates for fast switching (set after initial route)
    int8_t chipKY;              // Chip K Y position (breadboard node's row on chip K)
    int8_t highVoltageX;        // X position for HIGH voltage source on chip K
    int8_t lowVoltageX;         // X position for LOW voltage source on chip K
    bool fastPathReady;         // True when chip K coords are cached and ready for fast switching
};

// ============================================================================
// Fake GPIO Input Structure
// ============================================================================
// Uses virtual node expansion: bridge stays as userNode-FAKE_GP_IN_x
// All inputs share ADC0 (chip K X=8), but only ONE is connected at a time.
// Each input has its own chip K Y position (from its intermediate chip A-H).
// On read: disconnect current input's Y from ADC0, connect new input's Y, read.

struct FakeGpioInput {
    bool active;                // Is this slot in use?
    int userNode;               // Breadboard node
    float thresholdHigh;        // Voltage threshold for HIGH (e.g., 2.0V)
    float thresholdLow;         // Voltage threshold for LOW (e.g., 0.8V)
    int currentState;           // Last read state (0, 1, or -1 for unknown)
    int netIndex;               // Net index for LED display
    // Chip K coordinates for fast switching (all inputs use ADC0 = X position 8)
    int8_t chipKY;              // Chip K Y position (determined by intermediate chip A-H)
    bool fastPathReady;         // True when chip K Y is cached and ready for fast switching
    bool connected;             // True if this input is currently connected to ADC0
};

// ============================================================================
// Global Storage
// ============================================================================

extern FakeGpioOutput fakeGpioOutputs[MAX_FAKE_GP_OUT];
extern FakeGpioInput fakeGpioInputs[MAX_FAKE_GP_IN];
extern int fakeGpioCurrentlyConnectedInput;  // Slot of currently connected input (-1 if none)
extern int fakeGpioInputAdcChannel;          // Which ADC channel (0-3) is used by fake GPIO inputs (-1 if not set)
extern bool debugFakeGpio;     // Debug flag for fake GPIO operations

// Get the chip K X position for the currently selected ADC
// ADC0=X8, ADC1=X9, ADC2=X10, ADC3=X11
int getFakeGpioInputChipKX();

// Find a free ADC channel (0-3) that's not in use by other connections
// Returns: ADC channel number (0-3), or -1 if all are in use
int findFreeAdcForFakeGpioInputs();

// Check if a specific ADC channel is currently in use by non-FakeGPIO connections
bool isAdcInUseByOtherConnections(int adcChannel);

// ============================================================================
// Initialization and State Management
// ============================================================================

// Initialize fake GPIO subsystem (call at startup)
void initFakeGpio();

// Initialize fake GPIO from loaded state (call after loading YAML/restoring connections)
// Scans bridges for FAKE_GP_OUT_x and FAKE_GP_IN_x nodes and restores configuration
void initializeFakeGpioFromLoadedState();

// Background reading for fake GPIO inputs - cycles through inputs
// Call periodically to update input states and LED colors
void readFakeGPIO(void);

// ============================================================================
// Configuration Functions
// ============================================================================

// Configure a fake GPIO pin in OUTPUT mode (node-based)
// highNode/lowNode: voltage source nodes (TOP_RAIL, BOTTOM_RAIL, DAC0, DAC1, GND)
// Returns slot index (0-7) on success, -1 on failure
int fakeGpioConfigOutput(int node, int highNode, int lowNode, 
                         float thresholdHigh = 2.0f, float thresholdLow = 0.8f);

// Configure a fake GPIO pin in OUTPUT mode (voltage-based)
// Automatically selects appropriate voltage sources
// Returns slot index (0-7) on success, -1 on failure
int fakeGpioConfigOutputVoltage(int node, float vHigh, float vLow,
                                float thresholdHigh = 2.0f, float thresholdLow = 0.8f);

// Configure a fake GPIO pin in INPUT mode
// Returns slot index (0-31) on success, -1 on failure
int fakeGpioConfigInput(int node, float thresholdHigh = 2.0f, float thresholdLow = 0.8f);

// Remove a fake GPIO output configuration
// Returns 1 on success, 0 on failure
int fakeGpioRemoveOutput(int node);

// Remove a fake GPIO input configuration
// Returns 1 on success, 0 on failure
int fakeGpioRemoveInput(int node);

// ============================================================================
// Read/Write Functions
// ============================================================================

// Write fake GPIO output state
// state: 0=LOW, 1=HIGH
// Returns 1 on success, 0 on failure
int fakeGpioWrite(int node, int state);

// Write multiple fake GPIO outputs at once (batch write)
// Useful for differential pairs or multi-pin protocols
// nodes: array of node numbers
// states: array of states (0=LOW, 1=HIGH)
// count: number of pins to write
// Returns: number of successful writes
int fakeGpioWriteBatch(const int* nodes, const int* states, int count);

// Read fake GPIO input state
// Returns: 1 for HIGH, 0 for LOW, -1 for error/floating
int fakeGpioRead(int node);

// Read fake GPIO output current state (returns stored state, no ADC read)
int fakeGpioReadOutput(int node);

// ============================================================================
// Helper Functions
// ============================================================================

// Find output slot by user node (-1 if not found)
int findFakeGpioOutputSlot(int userNode);

// Find input slot by user node (-1 if not found)
int findFakeGpioInputSlot(int userNode);

// Find free output slot (-1 if all full)
int findFreeFakeGpioOutputSlot();

// Find free input slot (-1 if all full)
int findFreeFakeGpioInputSlot();

// Get GPIO array index for output slot
int fakeGpioOutputToGpioIndex(int slot);

// Get GPIO array index for input slot
int fakeGpioInputToGpioIndex(int slot);

// Check if a node is a fake GPIO output virtual node
inline bool isFakeGpioOutputNode(int node) {
    return IS_FAKE_GP_OUT(node);
}

// Check if a node is a fake GPIO input virtual node
inline bool isFakeGpioInputNode(int node) {
    return IS_FAKE_GP_IN(node);
}

// Update GPIO display arrays for an output
void updateFakeGpioOutputDisplay(int slot);

// Update GPIO display arrays for an input
void updateFakeGpioInputDisplay(int slot);

// ============================================================================
// Legacy Compatibility (will be removed after full migration)
// ============================================================================

// Legacy constants
#define MAX_FAKE_GPIO 32
#define FAKE_GPIO_INPUT 0
#define FAKE_GPIO_OUTPUT 1

// Legacy FakeGpioPinConfig struct
struct FakeGpioPinConfig {
    bool active;
    int node;
    int mode;  // 0=OUTPUT, 1=INPUT
    float v_high;
    float v_low;
    float threshold_high;
    float threshold_low;
    int high_voltage_node;
    int low_voltage_node;
    int chip_k_x;
    int chip_k_y;
    int current_state;
    bool hasStoredState;
    chipXYBitfield chipXYState[12];  // Uses chipXYBitfield from CH446Q.h
    int path_length;
    int path_chips[4];
    int path_x[4];
    int path_y[4];
};

// Legacy global storage
extern FakeGpioPinConfig fakeGpioPins[MAX_FAKE_GPIO];
extern int adcCurrentlyConnectedPin;

// Legacy helper functions
int fakeGpioSlotToAnimationIndex(int slot);
int fakeGpioSlotToNode(int slot);
bool isFakeGpioVirtualNode(int node);
int fakeGpioNodeToSlot(int node);

// Debug getter/setter
bool getDebugFakeGpio();
void setDebugFakeGpio(bool value);

// Connection change hook (called from FileParsing)
void updateFakeGpioAfterConnectionChange(int node1, int node2);

// Fast toggle functions (for quick connect/disconnect without full routing)
int fakeGpioDisconnect(int node1, int node2);
int fakeGpioReconnect(int node1, int node2);

// Old unified config function - now routes to new functions
int fakeGpioConfig(int node, float v_high, float v_low, 
                   float threshold_high, float threshold_low, int mode = -1);

// Old node-based output config
int fakeGpioConfigOutputNodes(int node, int high_node, int low_node, 
                              float threshold_high, float threshold_low);

#endif // FAKEGPIO_H
