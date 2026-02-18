// SPDX-License-Identifier: MIT
#ifndef FAKEGPIO_H
#define FAKEGPIO_H

#include "CH446Q.h"  // For chipXYBitfield

// Maximum fake GPIO pins (32 slots for FAKE_GPIO_1 through FAKE_GPIO_32)
#define MAX_FAKE_GPIO 32

// Fake GPIO pin mode
#define FAKE_GPIO_INPUT  0
#define FAKE_GPIO_OUTPUT 1

// Storage structure for fake GPIO pin configuration
struct FakeGpioPinConfig {
    bool active;           // Is this slot in use?
    int node;              // User's node (breadboard pin, etc.)
    float v_high;          // Voltage for HIGH state (OUTPUT mode)
    float v_low;           // Voltage for LOW state (OUTPUT mode)
    float threshold_high;  // Threshold for reading HIGH (INPUT mode, e.g., 2.0V)
    float threshold_low;   // Threshold for reading LOW (INPUT mode, e.g., 0.8V)
    int mode;              // OUTPUT=0, INPUT=1

    // For OUTPUT mode - chip K coordinates for fast switching
    int chip_k_x;          // Which X pin on chip K (0-15)
    int chip_k_y;          // Which Y pin on chip K (0-7)
    int current_state;     // 0=LOW, 1=HIGH
    int high_voltage_node; // Node number for HIGH (TOP_RAIL, DAC0, etc.)
    int low_voltage_node;  // Node number for LOW (GND, DAC1, etc.)

    // ChipXY state storage (used by both modes for different purposes)
    // INPUT: Complete state when ADC is connected to this pin
    // OUTPUT: Could store both HIGH and LOW states for future optimization
    chipXYBitfield chipXYState[12];  // 192 bytes per pin (vs 1536 bytes with bool array)
    bool hasStoredState;  // True if chipXYState is valid

    // Deprecated - remove after migration (kept for compatibility during transition)
    int path_chips[4];     // Chip IDs in the path
    int path_x[4];         // X coordinates for each chip
    int path_y[4];         // Y coordinates for each chip
    int path_length;       // Number of chips in the path (0-4)
};

// Global storage for fake GPIO pins (accessed from Peripherals.cpp for background reading)
extern FakeGpioPinConfig fakeGpioPins[MAX_FAKE_GPIO];

// Global tracking: which INPUT pin is currently connected to ADC0
// All INPUT pins share ADC0 for simplicity
// -1 means no pin is currently connected
extern int adcCurrentlyConnectedPin;

// Debug flag for fake GPIO operations
extern bool debugFakeGpio;

// ============================================================================
// Initialization and State Management
// ============================================================================

// Initialize fake GPIO from loaded state (call after loading YAML/restoring connections)
// This reconstructs FakeGpioPinConfig entries from bridges in globalState
void initializeFakeGpioFromLoadedState();

// Update affected fake GPIO pins after bridge add/remove operations
// Call this after connection changes to keep chipXY snapshots current
void updateFakeGpioAfterConnectionChange(int node1, int node2);

// ============================================================================
// Configuration Functions
// ============================================================================

// Configure a fake GPIO pin in INPUT mode
// Returns 1 on success, 0 on failure
int fakeGpioConfigInput(int node, float threshold_high, float threshold_low);

// Configure a fake GPIO pin in OUTPUT mode (voltage-based)
// Returns 1 on success, 0 on failure
int fakeGpioConfigOutput(int node, float v_high, float v_low, float threshold_high, float threshold_low);

// Configure a fake GPIO pin in OUTPUT mode (node-based - preferred)
// Directly connects to specified voltage source nodes (TOP_RAIL, BOTTOM_RAIL, DAC0, DAC1, GND)
// Returns 1 on success, 0 on failure
int fakeGpioConfigOutputNodes(int node, int high_node, int low_node, float threshold_high, float threshold_low);

// Unified configuration function (auto-detects mode from voltages)
// Returns 1 on success, 0 on failure
int fakeGpioConfig(int node, float v_high, float v_low, float threshold_high, float threshold_low, int mode = -1);

// ============================================================================
// Read/Write Functions
// ============================================================================

// Read fake GPIO pin state (works for both INPUT and OUTPUT modes)
// Returns: 1 for HIGH, 0 for LOW, -1 for error/floating
int fakeGpioRead(int node);

// Write fake GPIO pin state (OUTPUT mode only)
// Returns 1 on success, 0 on failure
int fakeGpioWrite(int node, int state);

// ============================================================================
// Fast Toggle Functions (for temporary disconnects)
// ============================================================================

// Disconnect a path and store its chip/x/y for later reconnection
// Returns 1 on success, 0 on failure
int fakeGpioDisconnect(int node1, int node2);

// Reconnect a previously disconnected path
// Returns 1 on success, 0 on failure
int fakeGpioReconnect(int node1, int node2);

// ============================================================================
// Helper Functions
// ============================================================================

// Get the animation index for a fake GPIO slot (for LED display)
int fakeGpioSlotToAnimationIndex(int slot);

// Get the virtual node number for a fake GPIO slot (FAKE_GPIO_1 + slot)
int fakeGpioSlotToNode(int slot);

// Check if a node is a FAKE_GPIO virtual node (150-181)
bool isFakeGpioVirtualNode(int node);

// Get slot number from FAKE_GPIO virtual node
int fakeGpioNodeToSlot(int node);

bool getDebugFakeGpio() { return debugFakeGpio; }
void setDebugFakeGpio(bool value) { debugFakeGpio = value; }

#endif // FAKEGPIO_H

