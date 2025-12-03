/*
 * Jumperless MicroPython API Wrapper Functions
 * 
 * These functions provide a C-compatible interface for MicroPython
 * to call Jumperless functionality directly without string parsing.
 */

#include "ArduinoStuff.h"
#include "Commands.h"
#include "CH446Q.h"
#include "FileParsing.h"

#include "Graphics.h"
#include "NetsToChipConnections.h"
#include "oled.h"
#include "RotaryEncoder.h"
#include "Peripherals.h"

#include "JumperlessDefines.h"
#include "hardware/gpio.h"
#include "SafeString.h"

#include "LogicAnalyzer.h"
#include "WaveGen.h"
#include "States.h"
#include "externVars.h"  // For fs_mutex filesystem synchronization
#include "FilesystemStuff.h"  // For safe file operations

extern LogicAnalyzer logicAnalyzer; // defined in main.cpp
extern WaveGen wavegen; // defined in main.cpp

// External declarations
extern SafeString nodeFileString;

#include "CH446Q.h"
#include "NetManager.h"
#include "Apps.h"
#include "Probing.h"
#include "Python_Proper.h"
#include "config.h"
#include "FatFS.h"

#include "JulseView.h"




// Forward declarations
int justReadProbe(bool allowDuplicates);

// Include JumperlOS for service management
#include "JumperlOS.h"

/**
 * @brief Run essential services during MicroPython execution
 * 
 * This is called from mp_hal_delay_ms() to keep the system responsive
 * while Python scripts are running. It runs:
 * - Peripherals service (current sense measurements for marching ants)
 * - TinyUSB task (keep USB alive)
 */
extern "C" void jl_service_python(void) {
    jOS.serviceAll();
}

// Python connection context - controls whether Python changes persist or are isolated
#define PYTHON_SLOT_NUMBER 99  // Special slot for Python isolated context

PythonConnectionContext connectionContext = PYTHON_CONTEXT_GLOBAL;  // Default to global mode
static int pythonEntrySlot = -1;  // Track which slot was active when entering Python

// C-compatible wrapper functions for MicroPython
extern "C" {
#include "py/mpthread.h"
// WaveGen C wrappers (C linkage)
void jl_wavegen_set_output(int channel);
void jl_wavegen_set_freq(float hz);
void jl_wavegen_set_wave(int wave);
void jl_wavegen_set_amplitude(float vpp);
void jl_wavegen_set_offset(float v);
void jl_wavegen_set_sweep(float start_hz, float end_hz, float seconds);
void jl_wavegen_start(int start);
void jl_wavegen_stop(void);

// WaveGen getters
int jl_wavegen_get_output(void);
float jl_wavegen_get_freq(void);
int jl_wavegen_get_wave(void);
float jl_wavegen_get_amplitude(void);
float jl_wavegen_get_offset(void);
int jl_wavegen_is_running(void);
void jl_wavegen_get_sweep(float *start_hz, float *end_hz, float *seconds);





void jl_pause_core2(bool pause) {
    pauseCore2 = pause;
}


void jl_change_terminal_color(int color, bool flush) {
    changeTerminalColor(color, flush);
}

void jl_cycle_term_color(bool reset, float step, bool flush) {
    cycleTermColor(reset, step, flush);
}

void jl_print_terminal_colors(void) {
    printSpectrumOrderedColorCube();
}
// WaveGen implementation
static float s_wg_sweep_start_hz = 0.0f;
static float s_wg_sweep_end_hz = 0.0f;
static float s_wg_sweep_time_s = 0.0f;
static bool s_wg_user_set_output = false;
static bool s_wg_user_set_freq = false;
static bool s_wg_user_set_wave = false;
static bool s_wg_user_set_amp = false;
static bool s_wg_user_set_offset = false;
void jl_wavegen_set_output(int channel) {
    // Map 0..3 to WAVEGEN_DAC0..3; also accept rails via same mapping the module uses
    if (channel < 0) channel = 0;
    if (channel > 3) channel = 3;
    wavegen.setChannel((waveGen_channel_t)channel);
    s_wg_user_set_output = true;
}

void jl_wavegen_set_freq(float hz) {
    if (hz <= 0.0f) hz = 0.0001f;
    wavegen.setFrequency(hz);
    s_wg_user_set_freq = true;
}

void jl_wavegen_set_wave(int wave) {
    if (wave < 0) wave = 0;
    if (wave > 3) wave = 3;
    wavegen.setWaveform((waveGen_waveform_t)wave);
    s_wg_user_set_wave = true;
}

void jl_wavegen_set_amplitude(float vpp) {
    // Public API specifies Vpp. Internally we use amplitude as peak value.
    // So convert Vpp to peak amplitude: A = Vpp / 2
    if (vpp < 0.0f) vpp = 0.0f;
    float peak = vpp * 0.5f;
    wavegen.setAmplitude(peak);
    s_wg_user_set_amp = true;
}

void jl_wavegen_set_offset(float v) {
    wavegen.setOffset(v);
    s_wg_user_set_offset = true;
}

void jl_wavegen_set_sweep(float start_hz, float end_hz, float seconds) {
    if (start_hz <= 0.0f) start_hz = 0.0001f;
    if (end_hz <= 0.0f) end_hz = 0.0001f;
    if (seconds < 0.0f) seconds = 0.0f;
    s_wg_sweep_start_hz = start_hz;
    s_wg_sweep_end_hz = end_hz;
    s_wg_sweep_time_s = seconds;
}

void jl_wavegen_start(int start) {
    if (start) {
        // Ensure initialized and safe mode
        wavegen.begin();
        wavegen.setFallbackMode(true);
        // Apply defaults if user didn't set anything yet
        if (!s_wg_user_set_output) {
            jl_wavegen_set_output(1); // default DAC1
        }
        if (!s_wg_user_set_freq) {
            jl_wavegen_set_freq(100.0f); // default 100 Hz
        }
        if (!s_wg_user_set_wave) {
            jl_wavegen_set_wave(0); // SINE
        }
        if (!s_wg_user_set_amp) {
            jl_wavegen_set_amplitude(3.3f); // Vpp
        }
        if (!s_wg_user_set_offset) {
            jl_wavegen_set_offset(1.65f); // center 0-3.3V
        }
        wavegen.start();
    } else {
        if (wavegen.isRunning()) {
            wavegen.stop();
        }
    }
}

void jl_wavegen_stop(void) {
    wavegen.stop();
}

int jl_wavegen_get_output(void) {
    return (int)wavegen.getChannel();
}

float jl_wavegen_get_freq(void) {
    return wavegen.getFrequency();
}

int jl_wavegen_get_wave(void) {
    return (int)wavegen.getWaveform();
}

float jl_wavegen_get_amplitude(void) {
    // Convert from internal peak to Vpp for external callers
    return wavegen.getAmplitude() * 2.0f;
}

float jl_wavegen_get_offset(void) {
    return wavegen.getOffset();
}

int jl_wavegen_is_running(void) {
    return wavegen.isRunning() ? 1 : 0;
}

void jl_wavegen_get_sweep(float *start_hz, float *end_hz, float *seconds) {
    if (start_hz) *start_hz = s_wg_sweep_start_hz;
    if (end_hz) *end_hz = s_wg_sweep_end_hz;
    if (seconds) *seconds = s_wg_sweep_time_s;
}




// DAC Functions
void jl_dac_set(int channel, float voltage, int save) {
    // if (channel == 0) {
    //     channel = 2;
    // } else if (channel == 1) {
    //     channel = 3;
    // } else if (channel == 2) {
    //     channel = 0;
    // } else if (channel == 3) {
    //     channel = 1;
    // }
    setDacByNumber(channel, voltage, save, 0, false);
}

float jl_dac_get(int channel) {
    float voltage = 0.0f;

    if (channel == 0) {
        voltage = globalState.power.dac0;
    } else if (channel == 1) {
        voltage = globalState.power.dac1;
    } else if (channel == 2) {
        voltage = globalState.power.topRail;
    } else if (channel == 3) {
        voltage = globalState.power.bottomRail;
    }

    return voltage;
}

// ADC Functions  
float jl_adc_get(int channel) {
    return readAdcVoltage(channel, 32);
}

// INA Functions
// NOTE: INA219 uses I2C which may conflict with Core 2 operations (OLED, etc.)
// We temporarily pause Core 2 during I2C operations to prevent bus conflicts
// and potential crashes from concurrent I2C access.

float jl_ina_get_current(int sensor) {
    bool was_paused = pauseCore2;
    pauseCore2 = true;  // Prevent Core 2 I2C conflicts
    delayMicroseconds(50);  // Allow Core 2 to finish any in-progress I2C
    
    float result = 0.0f;
    if (sensor == 0) {
        result = INA0.getCurrent();
    } else if (sensor == 1) {
        result = INA1.getCurrent();
    }
    
    pauseCore2 = was_paused;
    return result;
}

float jl_ina_get_voltage(int sensor) {
    bool was_paused = pauseCore2;
    pauseCore2 = true;
    delayMicroseconds(50);
    
    float result = 0.0f;
    if (sensor == 0) {
        result = INA0.getBusVoltage();
    } else if (sensor == 1) {
        result = INA1.getBusVoltage();
    }
    
    pauseCore2 = was_paused;
    return result;
}

float jl_ina_get_bus_voltage(int sensor) {
    bool was_paused = pauseCore2;
    pauseCore2 = true;
    delayMicroseconds(50);
    
    float result = 0.0f;
    if (sensor == 0) {
        result = INA0.getBusVoltage();
    } else if (sensor == 1) {
        result = INA1.getBusVoltage();
    }
    
    pauseCore2 = was_paused;
    return result;
}

float jl_ina_get_power(int sensor) {
    bool was_paused = pauseCore2;
    pauseCore2 = true;
    delayMicroseconds(50);
    
    float result = 0.0f;
    if (sensor == 0) {
        result = INA0.getPower();
    } else if (sensor == 1) {
        result = INA1.getPower();
    }
    
    pauseCore2 = was_paused;
    return result;
}

// GPIO Functions
void jl_gpio_set(int pin, int value) {
    if (pin >= 1 && pin <= 10) {
        digitalWrite(gpioDef[pin - 1][0], value);
    } else if (pin >= 20 && pin <= 27) {
        digitalWrite(pin, value);
    }
}

int jl_gpio_get(int pin) {
    if (pin >= 1 && pin <= 10) {
        // Serial.print("jl_gpio_get = ");
        // Serial.println(gpioDef[pin - 1][0]);
        // Serial.print("gpio_get = ");
        // Serial.println(gpio_get((uint)gpioDef[pin - 1][0]));
        // Serial.println("digitalRead = ");
        // Serial.println(digitalRead(gpioDef[pin - 1][0]));
        // return gpio_get(gpioDef[pin - 1][0]);
        while (readingGPIO) {
            delayMicroseconds(1);
        }

        int reading = gpioReadWithFloating(gpioDef[pin - 1][0], 50);
        // Serial.print("gpioReadWithFloating = ");
        // Serial.println(reading);
        return reading;
    
    } else if (pin >= 20 && pin <= 27) {
        return gpio_get(pin);
    }
    return 0;
}

int jl_gpio_set_direction(int pin, int direction) {
    if (pin >= 1 && pin <= 10) {
        globalState.config.gpioDirection[pin - 1] = direction;
        pinMode(gpioDef[pin - 1][0], direction ? OUTPUT : INPUT);
    } else if (pin >= 20 && pin <= 27) {
        globalState.config.gpioDirection[pin - 20] = direction;
        pinMode(pin, direction ? OUTPUT : INPUT);
    }
    return 1;
}

int jl_gpio_get_dir(int pin) {
    if (pin >= 1 && pin <= 10) {
        return gpio_get_dir(gpioDef[pin - 1][0]);
    } else if (pin >= 20 && pin <= 27) {
        return gpio_get_dir(pin);
    }
    return 0;
}

void jl_gpio_set_dir(int pin, int direction) {
    if (pin >= 1 && pin <= 10) {
        gpio_set_dir(gpioDef[pin - 1][0], direction);
        globalState.config.gpioDirection[pin - 1] = direction;
    } else if (pin >= 20 && pin <= 27) {
        gpio_set_dir(pin, direction);
        globalState.config.gpioDirection[pin - 20] = direction;
    } 

}

int jl_gpio_get_pull(int pin) {
    
    if (pin >= 1 && pin <= 10) {
        pin = gpioDef[pin - 1][0];
        bool pull_up = gpio_is_pulled_up(pin);
        bool pull_down = gpio_is_pulled_down(pin);
        if (pull_up && pull_down) {
            return 2; // bus keeper
        } else if (pull_up) {
            return 1; // pullup
        } else if (pull_down) {
            return -1; // pulldown
        } else {
            return 0; // no pull
        }
    } else if (pin >= 20 && pin <= 27) {
        bool pull_up = gpio_is_pulled_up(pin);
        bool pull_down = gpio_is_pulled_down(pin);
        if (pull_up && pull_down) {
            return 2; // bus keeper
        } else if (pull_up) {
            return 1; // pullup
        } else if (pull_down) {
            return -1; // pulldown
        } else {
            return 0; // no pull
        }
    }
    return 0;
}

void jl_gpio_set_pull(int pin, int pull) {

    // Serial.print("jl_gpio_set_pull: ");
    // Serial.println(pull);
    
    bool pull_up = false;
    bool pull_down = false;

    int config_pull = 0;
    if (pull == 0) {
        pull_up = false;
        pull_down = false;
        config_pull = 2; // no pull
    } else if (pull == 1) {
        pull_up = true;
        pull_down = false;
        config_pull = 1; // pullup
    } else if (pull == -1) {
        pull_up = false;
        pull_down = true;
        config_pull = 0; // pulldown
    } else if (pull == 2) {
        pull_up = true;
        pull_down = true; // bus keeper mode
        config_pull = 3; // bus keeper
    }


    if (pin >= 1 && pin <= 10) {
        pin = gpioDef[pin - 1][0];
        
        gpio_set_pulls(pin, pull_up, pull_down);
        
        globalState.config.gpioPulls[pin - 1] = config_pull;
    } else if (pin >= 20 && pin <= 27) {
        gpio_set_pulls(pin, pull_up, pull_down);
        globalState.config.gpioPulls[pin - 20] = config_pull;
    }
}

} // temporarily close extern "C" for C++ declarations

// Note: setCustomNetName() and hasCustomNetName() are declared in States.h with C++ linkage

// Forward declarations for color parsing (C++ functions that return String)
uint32_t parseColorValue(const String& colorStr, bool& success);
String colorValueToName(uint32_t color);

// Helper to get color name into a C buffer (wraps C++ function)
static void getColorNameIntoBuffer(uint32_t color, char* buffer, size_t bufSize) {
    String name = colorValueToName(color);
    strncpy(buffer, name.c_str(), bufSize - 1);
    buffer[bufSize - 1] = '\0';
}

extern "C" { // reopen extern "C"

// ============================================================================
// Net Information API
// ============================================================================

// Get the name of a specific net
// Returns the net name string, or nullptr if net doesn't exist
const char* jl_get_net_name(int netNum) {
    if (netNum < 0 || netNum >= MAX_NETS) return nullptr;
    
    // Check DisplayState for custom name first
    const char* customName = globalState.display.getNetName(netNum);
    if (customName != nullptr) {
        return customName;
    }
    
    // Fall back to default name from net struct
    return globalState.connections.nets[netNum].name;
}

// Set a custom name for a net
// Pass empty string or nullptr to reset to default name
void jl_set_net_name(int netNum, const char* name) {
    if (netNum < 0 || netNum >= MAX_NETS) return;
    
    setCustomNetName(netNum, name);
    globalState.markDirty();
}

// Get the color of a net as a 32-bit RGB value (0xRRGGBB)
uint32_t jl_get_net_color(int netNum) {
    if (netNum < 0 || netNum >= MAX_NETS) return 0;
    
    // Check for custom color first
    rgbColor color;
    uint32_t rawColor;
    char colorName[32];
    
    if (globalState.display.getNetColor(netNum, color, rawColor, colorName)) {
        return rawColor;
    }
    
    // Return the computed color from the net struct
    rgbColor netColor = globalState.connections.nets[netNum].color;
    return (netColor.r << 16) | (netColor.g << 8) | netColor.b;
}

// Get the color name of a net (returns static buffer)
const char* jl_get_net_color_name(int netNum) {
    static char colorNameBuffer[32];
    
    if (netNum < 0 || netNum >= MAX_NETS) {
        strcpy(colorNameBuffer, "unknown");
        return colorNameBuffer;
    }
    
    // Check for custom color first
    rgbColor color;
    uint32_t rawColor;
    
    if (globalState.display.getNetColor(netNum, color, rawColor, colorNameBuffer)) {
        return colorNameBuffer;
    }
    
    // Generate color name from computed color
    rgbColor netColor = globalState.connections.nets[netNum].color;
    uint32_t packed = (netColor.r << 16) | (netColor.g << 8) | netColor.b;
    getColorNameIntoBuffer(packed, colorNameBuffer, sizeof(colorNameBuffer));
    return colorNameBuffer;
}

// Set the color of a net by name (e.g., "red", "blue", "pink") or hex string (e.g., "#FF0000")
int jl_set_net_color(int netNum, const char* colorStr) {
    if (netNum < 0 || netNum >= MAX_NETS || !colorStr) return 0;
    
    String colorString(colorStr);
    bool parseSuccess;
    uint32_t rawColor = parseColorValue(colorString, parseSuccess);
    
    if (!parseSuccess) {
        return 0;  // Invalid color
    }
    
    rgbColor color;
    color.r = (rawColor >> 16) & 0xFF;
    color.g = (rawColor >> 8) & 0xFF;
    color.b = rawColor & 0xFF;
    
    // Store as custom color
    globalState.display.setNetColor(netNum, color, rawColor, colorStr);
    globalState.markDirty();
    
    return 1;  // Success
}

// Set the color of a net by RGB values
int jl_set_net_color_rgb(int netNum, int r, int g, int b) {
    if (netNum < 0 || netNum >= MAX_NETS) return 0;
    
    rgbColor color;
    color.r = r & 0xFF;
    color.g = g & 0xFF;
    color.b = b & 0xFF;
    
    uint32_t rawColor = (color.r << 16) | (color.g << 8) | color.b;
    char colorNameBuf[32];
    getColorNameIntoBuffer(rawColor, colorNameBuf, sizeof(colorNameBuf));
    
    globalState.display.setNetColor(netNum, color, rawColor, colorNameBuf);
    globalState.markDirty();
    
    return 1;
}

// Get the number of active nets
int jl_get_num_nets(void) {
    return numberOfNets;
}

// Get the number of bridges
int jl_get_num_bridges(void) {
    return globalState.connections.numBridges;
}

// Get nodes in a net as a comma-separated string (returns static buffer)
const char* jl_get_net_nodes(int netNum) {
    static char nodesBuffer[256];
    nodesBuffer[0] = '\0';
    
    if (netNum < 0 || netNum >= MAX_NETS) return nodesBuffer;
    
    int pos = 0;
    bool first = true;
    
    for (int j = 0; j < MAX_NODES && globalState.connections.nets[netNum].nodes[j] != 0; j++) {
        if (!first && pos < 250) {
            nodesBuffer[pos++] = ',';
        }
        first = false;
        
        // Get short name for node
        const char* nodeName = definesToChar(globalState.connections.nets[netNum].nodes[j], 0);
        if (nodeName && strlen(nodeName) > 0 && pos < 250) {
            int len = strlen(nodeName);
            if (pos + len < 255) {
                strcpy(&nodesBuffer[pos], nodeName);
                pos += len;
            }
        }
    }
    nodesBuffer[pos] = '\0';
    
    return nodesBuffer;
}

// Get bridge info: node1, node2, duplicates for a specific bridge index
int jl_get_bridge(int bridgeIdx, int* node1, int* node2, int* duplicates) {
    if (bridgeIdx < 0 || bridgeIdx >= globalState.connections.numBridges) return 0;
    
    if (node1) *node1 = globalState.connections.bridges[bridgeIdx][0];
    if (node2) *node2 = globalState.connections.bridges[bridgeIdx][1];
    if (duplicates) *duplicates = globalState.connections.bridges[bridgeIdx][2];
    
    return 1;
}




// Node Functions
int jl_nodes_connect(int node1, int node2, int save) {
    
    // Add to RAM state
    addBridgeToState(node1, node2, -1, true);
    
    // Update shown readings to detect current sense connections
    // This enables the marching ants animation when ISENSE_PLUS/MINUS are connected
    chooseShownReadings();

    return 1;
}

int jl_nodes_disconnect(int node1, int node2) {
    // Remove from RAM state
    removeBridgeFromState(node1, node2, true);

    
    // Update shown readings to detect current sense disconnections
    chooseShownReadings();

    return 1;
}

int jl_nodes_clear(void) {
    // Pause Core 2 BEFORE modifying state to prevent race conditions
    // Core 2 handles LEDs and may be reading state while we modify it
    bool was_paused = pauseCore2;
    pauseCore2 = true;
    delayMicroseconds(50);  // Allow Core 2 to finish any in-progress operations
    
    // Clear the entire state (safe now that Core 2 is paused)
    globalState.clearAllConnections();
    // Save the cleared state
    //saveStateToSlot();
    
    // Unpause Core 2 BEFORE refreshConnections since it internally calls waitCore2
    // and needs Core 2 to be running to process sendAllPathsCore2/showLEDsCore2
    pauseCore2 = was_paused;
    
    refreshConnections(-1, 1, 1);
    // waitCore2 is called internally by refreshConnections
    
    return 1;
}

int jl_nodes_is_connected(int node1, int node2) {
    // Check in globalState instead of file
    bool connected = globalState.hasConnection(node1, node2);
    return connected ? 1 : 0;
}

int jl_nodes_save(int slot) {
    int target_slot = (slot == -1) ? netSlot : slot;  // Use current slot if -1
    
    // Pause Core 2 while saving to prevent race conditions
    bool was_paused = pauseCore2;
    pauseCore2 = true;
    delayMicroseconds(50);
    
    // Save globalState to YAML
    saveStateToSlot(target_slot);
    
    // Unpause Core 2 BEFORE refreshConnections since it internally calls waitCore2
    pauseCore2 = was_paused;
    
    // Refresh connections to make sure everything is in sync
    refreshConnections();
    
    return target_slot;  // Return the slot that was saved to
}

void jl_init_micropython_local_copy(void) {
    // Store which slot was active when entering Python
    pythonEntrySlot = netSlot;
    
    if (connectionContext == PYTHON_CONTEXT_ISOLATED) {
        // ISOLATED MODE: Save current state to backup and switch to Python slot
        storeStateBackup();
        
        // Load Python slot (or create empty if doesn't exist)
        SlotManager& mgr = SlotManager::getInstance();
        String errorMsg;
        
        // Try to load existing Python slot
        if (!mgr.slotExists(PYTHON_SLOT_NUMBER)) {
            // Create empty Python slot
            mgr.getActiveState().clear();
            mgr.saveSlot(PYTHON_SLOT_NUMBER, errorMsg);
        }
        
        // Load Python slot into active state
        if (mgr.loadSlot(PYTHON_SLOT_NUMBER, errorMsg)) {
            netSlot = PYTHON_SLOT_NUMBER;
            mgr.setActiveSlot(PYTHON_SLOT_NUMBER);
        } else {
            Serial.println("Warning: Failed to load Python slot: " + errorMsg);
        }
    } else {
        // GLOBAL MODE: Just store a backup for potential restore
        // but continue working with the current slot
        storeStateBackup();
    }
}

void jl_exit_micropython_restore_entry_state(void) {
    // Pause Core 2 during state modifications to prevent race conditions
    bool was_paused = pauseCore2;
    pauseCore2 = true;
    delayMicroseconds(50);
    
    if (connectionContext == PYTHON_CONTEXT_ISOLATED) {
        // ISOLATED MODE: Save Python slot and restore entry state
        SlotManager& mgr = SlotManager::getInstance();
        String errorMsg;
        
        // Save current Python state to Python slot
        mgr.saveSlot(PYTHON_SLOT_NUMBER, errorMsg);
        
        // Restore the entry state (discards Python changes from global state)
        restoreAndSaveStateBackup();
        
        // Restore the original slot number
        if (pythonEntrySlot >= 0 && pythonEntrySlot < NUM_SLOTS) {
            netSlot = pythonEntrySlot;
            mgr.setActiveSlot(pythonEntrySlot);
        }
    } else {
        // GLOBAL MODE: Changes persist, just clear the backup
        clearStateBackup();
    }
    
    // Unpause Core 2 BEFORE refreshConnections since it internally calls waitCore2
    pauseCore2 = was_paused;
    
    // Refresh connections to match the current state
    refreshConnections(-1, 1, 1);
}

void jl_restore_micropython_entry_state(void) {
    // Use the new state-based backup system to restore entry state
    restoreAndSaveStateBackup();
    
    // Refresh connections to match the restored state
    refreshLocalConnections();
}

int jl_has_unsaved_changes(void) {
    // Use the new state-based backup system to check for changes
    return hasStateChanges() ? 1 : 0;
}

void jl_toggle_connection_context(void) {
    // Toggle between global and isolated modes
    if (connectionContext == PYTHON_CONTEXT_GLOBAL) {
        connectionContext = PYTHON_CONTEXT_ISOLATED;
    } else {
        connectionContext = PYTHON_CONTEXT_GLOBAL;
    }
}

const char* jl_get_connection_context_name(void) {
    return (connectionContext == PYTHON_CONTEXT_GLOBAL) ? "global" : "python";
}

// Helper function to convert chip identifier to chip number
int parseChipIdentifier(const char* chip_str) {
    if (strlen(chip_str) == 1) {
        char c = chip_str[0];
        if (c >= 'A' && c <= 'L') {
            return c - 'A';  // A=0, B=1, ..., L=11
        } else if (c >= 'a' && c <= 'l') {
            return c - 'a';  // a=0, b=1, ..., l=11
        }
    }
    // If not a letter, try to parse as number
    int chip_num = atoi(chip_str);
    if (chip_num >= 0 && chip_num <= 11) {
        return chip_num;
    }
    return -1; // Invalid chip identifier
}

void jl_send_raw(int chip, int x, int y, int setOrClear) {
    // Validate chip number (0-11)
    if (chip < 0 || chip > 11) {
        Serial.print("jl_send_raw: Invalid chip number: ");
        Serial.println(chip);
        return; // Invalid chip number
    }
    
    // Validate x,y coordinates (assuming 0-15 range based on typical crossbar chips)
    if (x < 0 || x > 15 || y < 0 || y > 15) {
        Serial.print("jl_send_raw: Invalid coordinates: ");
        Serial.print(x);
        Serial.print(",");
        Serial.println(y);
        return; // Invalid coordinates
    }
    
    // Call the existing sendXYraw function with setOrClear=1 (set path)
    lastChipXY[chip].connected[x][y] = setOrClear;
    sendXYraw(chip, x, y, setOrClear);
}

void jl_send_raw_str(const char* chip_str, int x, int y, int setOrClear) {
    int chip = parseChipIdentifier(chip_str);
    if (chip >= 0) {
        // Serial.print("jl_send_raw_str: chip = ");
        // Serial.println(chip);
        // Serial.print("jl_send_raw_str: x = ");
        // Serial.println(x);
        // Serial.print("jl_send_raw_str: y = ");
        // Serial.println(y);
        // Serial.print("jl_send_raw_str: setOrClear = ");
        jl_send_raw(chip, x, y, setOrClear);
    }
}

int jl_switch_slot(int slot) {
    // Validate slot number
    if (slot < 0 || slot >= NUM_SLOTS) {
        return -1; // Invalid slot number
    }
    
    // Save current slot if different
    if (netSlot != slot) {
        // Pause Core 2 briefly while changing slot number
        bool was_paused = pauseCore2;
        pauseCore2 = true;
        delayMicroseconds(50);
        
        int old_slot = netSlot;
        netSlot = slot;

        // Unpause Core 2 BEFORE refreshConnections since it internally calls waitCore2
        pauseCore2 = was_paused;
        
        // Refresh connections for the new slot
        refreshConnections(-1);
        
        return old_slot; // Return the previous slot number
    }
    
    return slot; // Already in this slot
}



// // Logic Analyzer Functions


void jl_control_set_analog(int channel, float value) {
    // if (channel >= 0 && channel < 4) {
    //     control_A[channel] = value;
    // }
}

void jl_control_set_digital(int channel, bool value) {
    // if (channel >= 0 && channel < 4) {
    //     control_D[channel] = value;
    // }
}

// Enhanced Logic Analyzer Functions
bool jl_la_set_trigger(int trigger_type, int channel, float value) {
    // Triggers not implemented in LogicAnalyzer yet; accept and noop
    (void)trigger_type; (void)channel; (void)value;
    return true;
}

bool jl_la_capture_single_sample(void) {
    if (logicAnalyzer.getIsRunning()) return false;
    logicAnalyzer.num_samples = 1;
    logicAnalyzer.sample_rate_hz = 1000;
    logicAnalyzer.arm();
    logicAnalyzer.run();
    while (logicAnalyzer.getIsRunning()) { delayMicroseconds(100); }
    return true;
}

bool jl_la_start_continuous_capture(void) {
    if (logicAnalyzer.getIsRunning()) return false;
    logicAnalyzer.num_samples = 0; // 0 => continuous not yet supported; use large value
    logicAnalyzer.num_samples = 0x7FFFFFFF;
    logicAnalyzer.sample_rate_hz = 1000000;
    logicAnalyzer.arm();
    logicAnalyzer.run();
    return true;
}

bool jl_la_stop_capture(void) {
    if (!logicAnalyzer.getIsRunning()) return false;
    logicAnalyzer.reset();
    return true;
}

bool jl_la_is_capturing(void) {
    return logicAnalyzer.getIsRunning();
}

void jl_la_set_sample_rate(uint32_t sample_rate) {
    logicAnalyzer.sample_rate_hz = sample_rate;
}

void jl_la_set_num_samples(uint32_t num_samples) {
    logicAnalyzer.num_samples = num_samples;
}

void jl_la_enable_channel(int channel_type, int channel, bool enable) {
    if (channel_type == 0) { // Digital
        if (channel >= 0 && channel < 8) {
            if (enable) logicAnalyzer.d_mask |= (1u << channel);
            else logicAnalyzer.d_mask &= ~(1u << channel);
        }
    } else if (channel_type == 1) { // Analog
        if (channel >= 0 && channel < 8) {
            if (enable) logicAnalyzer.a_mask |= (1u << channel);
            else logicAnalyzer.a_mask &= ~(1u << channel);
        }
    }
}

void jl_la_set_control_analog(int channel, float value) {
    jl_control_set_analog(channel, value);
}

void jl_la_set_control_digital(int channel, bool value) {
    jl_control_set_digital(channel, value);
}

float jl_la_get_control_analog(int channel) {
   // return (channel >= 0 && channel < 4) ? control_A[channel] : 0.0f;
   return 0.0f;
}

bool jl_la_get_control_digital(int channel) {
   // return (channel >= 0 && channel < 4) ? control_D[channel] : false;
   return false;
}

// OLED Functions
int jl_oled_print(const char* text, int size) {
   // mp_hal_check_interrupt();
    if (oled.isConnected()) {
        oled.clearPrintShow(text, 2, true, true, true);
        return 1;
    } else {
        return 0;
    }
}

int jl_oled_clear(void) {
    if (oled.isConnected()) {
    oled.clear(1000);
   oled.show(1000);
   return 1;
    } else {
        return 0;
    }
}

int jl_oled_show(void) {
    if (oled.isConnected()) {
   oled.show(1000);
   return 1;
    } else {
        return 0;
    }
}

int jl_oled_connect(void) {
    return oled.init();
}

int jl_oled_disconnect(void) {
    oled.disconnect();
    return 1;
}

// Arduino Functions
void jl_arduino_reset(void) {
    resetArduino();
}

// Status Functions
int jl_nodes_print_bridges(void) {
    printPathsCompact();
    return 1;
}

int jl_nodes_print_paths(void) {
    printPathsCompact();
    return 1;
}

int jl_nodes_print_crossbars(void) {
    printChipStateArray();
    return 1;
}

int jl_nodes_print_nets(void) {
    listNets(0);
    return 1;
}

int jl_nodes_print_chip_status(void) {
    printChipStatus();
    return 1;
}

int jl_run_app(char* appName) {
    runApp(-1,appName);
    return 1;
}

// Probe Functions
void jl_probe_tap(int node) {
    // TODO: Implement probe simulation
    // This would simulate tapping the probe on a specific node
}

int jl_probe_read_blocking(void) {
    int pad = -1;
    static int call_count = 0;
    call_count++;
    
    while (pad == -1) {
        mp_hal_check_interrupt();
        
        // Check if interrupt was requested and return special value
        if (mp_interrupt_requested) {
            mp_interrupt_requested = false; // Clear the flag
            Serial.print("DEBUG: Interrupt detected in jl_probe_read_blocking, call #");
            Serial.println(call_count);
            return -999; // Special return value indicating interrupt
        }
        
        pad = justReadProbe(false, 1);
        delay(1); // Small delay to prevent busy waiting
    }
    return pad;
}

int jl_probe_read_nonblocking(void) {
    return justReadProbe(true, 1);
}

// Clickwheel Functions
void jl_clickwheel_up(int clicks) {
    encoderOverride = 10;
    lastDirectionState = NONE;
    encoderDirectionState = UP;
}

void jl_clickwheel_down(int clicks) {
    encoderOverride = 10;
    lastDirectionState = NONE;
    encoderDirectionState = DOWN;
}

void jl_clickwheel_press(void) {
    encoderOverride = 10;
    lastButtonEncoderState = PRESSED;
    encoderButtonState = RELEASED;
}

// PWM Functions
extern "C" int jl_pwm_setup(int gpio_pin, float frequency, float duty_cycle) {
    return setupPWM(gpio_pin, frequency, duty_cycle);
}

extern "C" int jl_pwm_set_duty_cycle(int gpio_pin, float duty_cycle) {
    return setPWMDutyCycle(gpio_pin, duty_cycle);
}

extern "C" int jl_pwm_set_frequency(int gpio_pin, float frequency) {
    return setPWMFrequency(gpio_pin, frequency);
}

extern "C" int jl_pwm_stop(int gpio_pin) {
    return stopPWM(gpio_pin);
}

// Filesystem Functions - all require mutex for thread safety
int jl_fs_exists(const char* path) {
    if (!path) return 0;
    
    fs_mutex_acquire();  // THREAD SAFETY: Lock filesystem
    int result = FatFS.exists(path) ? 1 : 0;
    fs_mutex_release();  // THREAD SAFETY: Unlock filesystem
    
    return result;
}

char* jl_fs_listdir(const char* path) {
    if (!path) return nullptr;
    
    fs_mutex_acquire();  // THREAD SAFETY: Lock filesystem
    
    // Use static buffer to avoid memory management issues
    static char listBuffer[2048];
    listBuffer[0] = '\0';
    
    Dir dir = FatFS.openDir(path);
    
    bool first = true;
    while (dir.next()) {
        if (!first) {
            strcat(listBuffer, ",");
        }
        strcat(listBuffer, dir.fileName().c_str());
        if (dir.isDirectory()) {
            strcat(listBuffer, "/");
        }
        first = false;
        
        // Prevent buffer overflow
        if (strlen(listBuffer) > 1900) {
            strcat(listBuffer, "...");
            break;
        }
    }
    
    fs_mutex_release();  // THREAD SAFETY: Unlock filesystem
    return listBuffer;
}

char* jl_fs_read_file(const char* path) {
    if (!path) return nullptr;
    
    // Use static buffer for file contents
    static char fileBuffer[4096];
    size_t bytesRead = 0;
    
    if (!safeFileReadAll(path, fileBuffer, sizeof(fileBuffer), &bytesRead, 2000)) {
        return nullptr;
    }
    
    return fileBuffer;
}

int jl_fs_write_file(const char* path, const char* content) {
    if (!path || !content) return 0;
    
    return safeFileWriteAll(path, content, 0, 2000) ? 1 : 0;
}

char* jl_fs_get_current_dir(void) {
    static char currentDir[] = "/";
    return currentDir;
}

// ============================================================
// JFS File Handle Tracking
// Keeps track of all open JFS file handles so they can be 
// closed when exiting MicroPython (prevents file conflicts)
// ============================================================
#define MAX_JFS_OPEN_FILES 4
static void* jfs_open_files[MAX_JFS_OPEN_FILES] = {nullptr};
int debug_fs = 0;

static void jfs_track_file(void* handle) {
    for (int i = 0; i < MAX_JFS_OPEN_FILES; i++) {
        if (jfs_open_files[i] == nullptr) {
            jfs_open_files[i] = handle;
            if (debug_fs) {
                Serial.println("DEBUG: jfs_track_file: File is tracked");
                Serial.flush();
            }
            return;
        }
    }
    // No space - file won't be tracked (will still work but won't auto-close)
    if (debug_fs) {
        Serial.println("DEBUG: jfs_track_file: No space - file won't be tracked (will still work but won't auto-close)");
        Serial.flush();
    }
}

static void jfs_untrack_file(void* handle) {
    for (int i = 0; i < MAX_JFS_OPEN_FILES; i++) {
        if (jfs_open_files[i] == handle) {
            jfs_open_files[i] = nullptr;
            if (debug_fs) {
                Serial.println("DEBUG: jfs_untrack_file: File is untracked");
                Serial.flush();
            }
            return;
        }
    }
    if (debug_fs) {
        Serial.println("DEBUG: jfs_untrack_file: File is not found in tracked files");
        Serial.flush();
    }
}

// Check if a file handle is still tracked (i.e., not already closed by jl_close_all_jfs_files)
// This is used to detect use-after-free scenarios where a Python file object
// holds a stale pointer to an already-deleted File* object
static bool jfs_is_tracked(void* handle) {
    if (!handle) return false;
    for (int i = 0; i < MAX_JFS_OPEN_FILES; i++) {
        if (jfs_open_files[i] == handle) {
            if (debug_fs) {
                Serial.println("DEBUG: jfs_is_tracked: File is tracked");
                Serial.flush();
            }
            return true;
        }
    }
    if (debug_fs) {
        Serial.println("DEBUG: jfs_is_tracked: File is not tracked");
        Serial.flush();
    }
    return false;
}

// Close all open JFS files - called when exiting MicroPython
// CRITICAL: Must flush before close to ensure all buffered data is written to disk
// This prevents data loss and potential filesystem corruption on script exit
// THREAD SAFETY: Acquires fs_mutex to prevent concurrent filesystem access
void jl_close_all_jfs_files(void) {
    if (debug_fs) {
        Serial.println("DEBUG: jl_close_all_jfs_files: Closing all open JFS files");
        Serial.flush();
    }
    // CRITICAL: Pause Core2 during flash operations (flush writes to flash)
    bool was_paused = pauseCore2ForFlash(100);
    
    fs_mutex_acquire();  // THREAD SAFETY: Lock filesystem
    
    for (int i = 0; i < MAX_JFS_OPEN_FILES; i++) {
        if (jfs_open_files[i] != nullptr) {
            File* file = (File*)jfs_open_files[i];
            if (*file) {
                // CRITICAL: Flush before close to ensure buffered writes are committed
                // Without this, data written but not flushed could be lost on close
                file->flush();
                file->close();
            }
            delete file;
            jfs_open_files[i] = nullptr;
        }
    }
    
    fs_mutex_release();  // THREAD SAFETY: Unlock filesystem
    unpauseCore2ForFlash(was_paused);
    
    if (debug_fs) {
        Serial.println("DEBUG: jl_close_all_jfs_files: All open JFS files closed");
        Serial.flush();
    }
}

// File operations
// THREAD SAFETY: All file operations acquire fs_mutex to prevent concurrent access
void* jl_fs_open_file(const char* path, const char* mode) {
    if (!path || !mode) return nullptr;
    if (debug_fs) {
        Serial.print("DEBUG: jl_fs_open_file: Opening ");
        Serial.print(path);
        Serial.println("...");
        Serial.flush();
    }
    
    // NOTE: File open is primarily flash READS (directory lookup, FAT scan)
    // Flash reads don't disable XIP, so Core2 pause may not be needed here.
    // Only flash WRITES disable XIP and require Core2 synchronization.
    // Testing: removed Core2 pause from open - only keep mutex for thread safety
    
    fs_mutex_acquire();  // THREAD SAFETY: Lock filesystem
    
    File* file = new File(FatFS.open(path, mode));
    if (debug_fs) {
        Serial.print("DEBUG: jl_fs_open_file: File opened: ");
        Serial.print(path);
        Serial.print(" in mode: ");
        Serial.println(mode);
        Serial.print("DEBUG: jl_fs_open_file: File handle: ");
        Serial.println((String)file->name());
        Serial.flush();
    }
    if (!*file) {
        if (debug_fs) {
            Serial.println("DEBUG: jl_fs_open_file: File not open");
            Serial.flush();
        }
        delete file;
        fs_mutex_release();  // THREAD SAFETY: Unlock before returning
        return nullptr;
    }
    
    // Track the file handle for cleanup on exit
    jfs_track_file(file);
    
    fs_mutex_release();  // THREAD SAFETY: Unlock filesystem
    
    if (debug_fs) {
        Serial.println("DEBUG: jl_fs_open_file: File opened");
        Serial.flush();
    }
    return file;
}

void jl_fs_close_file(void* file_handle) {
    if (file_handle) {
        // CRITICAL FIX: Use non-blocking mutex acquire to prevent GC finaliser deadlock
        // 
        // Problem: Pico SDK mutexes are NOT recursive. If this function is called from
        // a GC finaliser while another JFS operation (like jl_fs_write_bytes) holds the
        // mutex, we would deadlock trying to acquire it again on the same core.
        //
        // Solution: Try non-blocking acquire. If we can't get the mutex, we're likely
        // in a GC finaliser during another JFS operation. In that case, just return
        // without closing - the file handle stays tracked and will be properly cleaned
        // up by jl_close_all_jfs_files() when exiting Python.
        if (!fs_mutex_try_acquire()) {
            // Can't get mutex - likely called from GC finaliser during JFS operation
            // Leave file tracked - jl_close_all_jfs_files() will clean it up on exit
            if (debug_fs) {
                Serial.println("DEBUG: jl_fs_close_file: Can't get mutex - likely called from GC finaliser during JFS operation");
                Serial.flush();
            }
            Serial.flush();
            return;
        }
        
        // CRITICAL: Check if file is still tracked before closing
        // This prevents use-after-free crashes when:
        // 1. jl_close_all_jfs_files() already closed and deleted the File*
        // 2. A GC finalizer later tries to close the same (now invalid) handle
        // If not tracked, the file was already closed - skip to avoid crash
        if (!jfs_is_tracked(file_handle)) {
            if (debug_fs) {
                Serial.println("DEBUG: jl_fs_close_file: File not tracked - already closed by jl_close_all_jfs_files()");
                Serial.flush();
            }
            Serial.flush();
            fs_mutex_release();
            return;  // Already closed by jl_close_all_jfs_files()
        }
        
        // Untrack the file handle
        jfs_untrack_file(file_handle);
        
        File* file = (File*)file_handle;
        // Only close if the file is actually open (prevents double-close crashes)
        if (*file) {
            // CRITICAL: Pause Core2 during flash operations (flush writes to flash)
            bool was_paused = pauseCore2ForFlash(100);
            
            // CRITICAL: Flush before close to ensure all buffered data is written
            // This is essential for GC finalizers where files may have pending writes
            // Without flush, close might lose unflushed buffer data
            file->flush();
            file->close();
            
            unpauseCore2ForFlash(was_paused);
        }
        delete file;
        
        fs_mutex_release();  // THREAD SAFETY: Unlock filesystem
    }
}

int jl_fs_read_bytes(void* file_handle, char* buffer, int size) {
    if (!file_handle || !buffer || size <= 0) return -1;
    
    fs_mutex_acquire();  // THREAD SAFETY: Lock filesystem
    
    File* file = (File*)file_handle;
    if (!*file) {
        fs_mutex_release();
        return -1;  // File not open
    }
    int result = file->readBytes(buffer, size);
    
    fs_mutex_release();  // THREAD SAFETY: Unlock filesystem
    if (debug_fs) {
        Serial.println("DEBUG: jl_fs_read_bytes: Read bytes");
        Serial.flush();
    }
    return result;
}

int jl_fs_write_bytes(void* file_handle, const char* data, int size) {
    if (!file_handle || !data || size <= 0) return -1;
    
    // CRITICAL: Pause Core2 during flash write operations
    bool was_paused = pauseCore2ForFlash(100);
    
    fs_mutex_acquire();  // THREAD SAFETY: Lock filesystem
    
    File* file = (File*)file_handle;
    if (!*file) {
        fs_mutex_release();
        unpauseCore2ForFlash(was_paused);
        return -1;  // File not open
    }
    int result = file->write((const uint8_t*)data, size);
    
    fs_mutex_release();  // THREAD SAFETY: Unlock filesystem
    unpauseCore2ForFlash(was_paused);
    
    if (debug_fs) {
        Serial.println("DEBUG: jl_fs_write_bytes: Written bytes");
        Serial.flush();
    }
    
    return result;
}

int jl_fs_seek(void* file_handle, int position, int mode) {
    if (!file_handle) return 0;
    
    fs_mutex_acquire();  // THREAD SAFETY: Lock filesystem
    
    File* file = (File*)file_handle;
    if (!*file) {
        fs_mutex_release();
        return 0;  // File not open
    }
    SeekMode seekMode = SeekSet;
    if (mode == 1) seekMode = SeekCur;
    else if (mode == 2) seekMode = SeekEnd;
    int result = file->seek(position, seekMode) ? 1 : 0;
    
    fs_mutex_release();  // THREAD SAFETY: Unlock filesystem
    if (debug_fs) {
        Serial.println("DEBUG: jl_fs_seek: Sought to position");
        Serial.flush();
    }
    return result;
}

int jl_fs_position(void* file_handle) {
    if (!file_handle) return -1;
    
    fs_mutex_acquire();  // THREAD SAFETY: Lock filesystem
    
    File* file = (File*)file_handle;
    if (!*file) {
        fs_mutex_release();
        return -1;  // File not open
    }
    int result = file->position();
    
    fs_mutex_release();  // THREAD SAFETY: Unlock filesystem
    if (debug_fs) {
        Serial.println("DEBUG: jl_fs_position: Position");
        Serial.flush();
    }
    return result;
}

int jl_fs_size(void* file_handle) {
    if (!file_handle) return -1;
    
    fs_mutex_acquire();  // THREAD SAFETY: Lock filesystem
    
    File* file = (File*)file_handle;
    if (!*file) {
        fs_mutex_release();
        return -1;  // File not open
    }
    int result = file->size();
    
    fs_mutex_release();  // THREAD SAFETY: Unlock filesystem
    return result;
}

int jl_fs_available(void* file_handle) {
    if (!file_handle) return 0;
    
    fs_mutex_acquire();  // THREAD SAFETY: Lock filesystem
    
    File* file = (File*)file_handle;
    if (!*file) {
        fs_mutex_release();
        return 0;  // File not open
    }
    int result = file->available();
    
    fs_mutex_release();  // THREAD SAFETY: Unlock filesystem
    return result;
}

// Flush file buffer to disk - CRITICAL for read-after-write operations
// THREAD SAFETY: Acquires fs_mutex to prevent concurrent filesystem access
void jl_fs_flush(void* file_handle) {
    if (file_handle) {
        // CRITICAL: Pause Core2 during flash write operations
        bool was_paused = pauseCore2ForFlash(100);
        
        fs_mutex_acquire();  // THREAD SAFETY: Lock filesystem
        
        File* file = (File*)file_handle;
        if (*file) {  // Only flush if file is actually open
            file->flush();
        }

        if (debug_fs) {
            Serial.println("DEBUG: jl_fs_flush: Flushed file");
            Serial.flush();
        }
        fs_mutex_release();  // THREAD SAFETY: Unlock filesystem
        unpauseCore2ForFlash(was_paused);
    }
}

char* jl_fs_name(void* file_handle) {
    if (!file_handle) return nullptr;
    
    fs_mutex_acquire();  // THREAD SAFETY: Lock filesystem
    
    File* file = (File*)file_handle;
    static char nameBuffer[256];
    strncpy(nameBuffer, file->name(), sizeof(nameBuffer) - 1);
    nameBuffer[sizeof(nameBuffer) - 1] = '\0';
    
    fs_mutex_release();  // THREAD SAFETY: Unlock filesystem
    return nameBuffer;
}

// Directory operations - all require mutex for thread safety
int jl_fs_mkdir(const char* path) {
    if (!path) return 0;
    
    fs_mutex_acquire();  // THREAD SAFETY: Lock filesystem
    int result = FatFS.mkdir(path) ? 1 : 0;
    fs_mutex_release();  // THREAD SAFETY: Unlock filesystem
    
    return result;
}

int jl_fs_rmdir(const char* path) {
    if (!path) return 0;
    
    fs_mutex_acquire();  // THREAD SAFETY: Lock filesystem
    int result = FatFS.rmdir(path) ? 1 : 0;
    fs_mutex_release();  // THREAD SAFETY: Unlock filesystem
    
    return result;
}

int jl_fs_remove(const char* path) {
    if (!path) return 0;
    
    fs_mutex_acquire();  // THREAD SAFETY: Lock filesystem
    int result = FatFS.remove(path) ? 1 : 0;
    fs_mutex_release();  // THREAD SAFETY: Unlock filesystem
    
    return result;
}

int jl_fs_rename(const char* pathFrom, const char* pathTo) {
    if (!pathFrom || !pathTo) return 0;
    
    fs_mutex_acquire();  // THREAD SAFETY: Lock filesystem
    int result = FatFS.rename(pathFrom, pathTo) ? 1 : 0;
    fs_mutex_release();  // THREAD SAFETY: Unlock filesystem
    
    return result;
}

// Get filesystem info - requires mutex for consistent reads
int jl_fs_total_bytes(void) {
    fs_mutex_acquire();  // THREAD SAFETY: Lock filesystem
    
    FSInfo info;
    int result = -1;
    if (FatFS.info(info)) {
        result = (int)(info.totalBytes & 0xFFFFFFFF); // Return lower 32 bits
    }
    
    fs_mutex_release();  // THREAD SAFETY: Unlock filesystem
    return result;
}

int jl_fs_used_bytes(void) {
    fs_mutex_acquire();  // THREAD SAFETY: Lock filesystem
    
    FSInfo info;
    int result = -1;
    if (FatFS.info(info)) {
        result = (int)(info.usedBytes & 0xFFFFFFFF); // Return lower 32 bits  
    }
    
    fs_mutex_release();  // THREAD SAFETY: Unlock filesystem
    return result;
}

} // extern "C" 