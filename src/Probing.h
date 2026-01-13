
// SPDX-License-Identifier: MIT
#ifndef PROBING_H
#define PROBING_H
#include "JumperlessDefines.h"
#include "JumperlOS.h"

#define MINIMUM_PROBE_READING 48

// Forward declarations
class EncoderAccelerator;

/**
 * @brief High-frequency probe button service (implemented in Probing.cpp)
 * 
 * Runs with CRITICAL priority to catch all button presses instantly.
 * Other code reads the cached state via probeButton.getButtonState()
 * 
 * New blocking behavior:
 * - When a press is detected, blocks subsequent presses for blockDurationMs (default 1 second)
 * - Block clears immediately if button is released OR timer expires
 * - Quick successive clicks register individually (each release clears block)
 * - Holding button down registers only once (block prevents re-triggering)
 * - Tracks continuous hold time and sets CONNECT_HELD/REMOVE_HELD flags
 */
class ProbeButton : public Service {
public:
    static ProbeButton& getInstance();
    ProbeButton(const ProbeButton&) = delete;
    ProbeButton& operator=(const ProbeButton&) = delete;
    
    ServiceStatus service() override;
    const char* getName() const override { return "ProbeButton"; }
    ServicePriority getPriority() const override { return ServicePriority::CRITICAL; }
    
    //@brief Get the current button state
    //@return 0 = neither pressed, 1 = remove button, 2 = connect button
    int getButtonState() const { return currentButtonState; }
    //@brief Get the current button press
    //@return 0 = no press, 1 = remove button, 2 = connect button
    int getButtonPress(bool consume = true);
    //@brief Check the probe button hardware
    //@return 0 = neither pressed, 1 = remove button, 2 = connect button
    int checkProbeButtonHardware(void);
    
    // Check if either button is held continuously
    bool isConnectHeld() const { return connectHeld; }
    bool isRemoveHeld() const { return removeHeld; }
    
    // Get continuous hold duration in milliseconds
    unsigned long getConnectHoldDuration() const { return connectHoldTime; }
    unsigned long getRemoveHoldDuration() const { return removeHoldTime; }
    
    // Clear the current button state (e.g., when entering probeMode)
    // NOTE: Does NOT clear isBlocked - the block must remain active to prevent re-triggering!
    void clearButtonState() { 
        currentButtonState = 0; 
        buttonPress = 0;
        buttonChanged = false;
        connectHeld = false;
        removeHeld = false;
        connectHoldTime = 0;
        removeHoldTime = 0;
        pressStartTime = 0;
        // isBlocked and blockStartTime are NOT cleared - block must stay active!
    }
    
    // Adjustable timing parameters (milliseconds)
    unsigned long checkIntervalMs = 12;          // Rate limiting between hardware checks
    unsigned long blockDurationMs = 800;        // Block duration after press detected
    unsigned long minimumBlockMs = 50;          // Minimum block time before release can clear (debounce)
    unsigned long connectHoldThresholdMs = 800;  // Threshold to set CONNECT_HELD flag
    unsigned long removeHoldThresholdMs = 500;   // Threshold to set REMOVE_HELD flag
    
    // Public state (for inline access)
    int currentButtonState = 0;   // 0=released, 1=remove, 2=connect
    int lastButtonState = 0;
    bool buttonChanged = false;
    int buttonPress = 0;          // Consumed by getButtonPress()
    
    // Hold state flags
    bool connectHeld = false;     // True when connect button held past threshold
    bool removeHeld = false;      // True when remove button held past threshold
    unsigned long connectHoldTime = 0;  // Continuous hold duration in ms
    unsigned long removeHoldTime = 0;   // Continuous hold duration in ms
    
private:
    ProbeButton();
    ~ProbeButton() = default;
    static ProbeButton* instance;
    
    unsigned long lastCheckTime = 0;
    bool isBlocked = false;
    unsigned long blockStartTime = 0;
    unsigned long pressStartTime = 0;
};

enum probePressType {
  connectPress = 2,
  disconnectPress = 1,
  connectLongPress = 4,
  disconnectLongPress = 3,
  doubleClickConnect = 5,
  doubleClickDisconnect = 6,
  noPress = 0
};

enum measuredState
{
  floating = 2,
  high = 1,
  low = 0,
  probe = 3,
  unknownState = 4 
};

/**
 * @brief Probing switch service - handles probe switch position (LOW priority)
 * 
 * Checks the 3-position probe switch state.
 * This is not time-critical so can run infrequently.
 */
class ProbeSwitch : public Service {
public:
    static ProbeSwitch& getInstance();
    ProbeSwitch(const ProbeSwitch&) = delete;
    ProbeSwitch& operator=(const ProbeSwitch&) = delete;
    
    ServiceStatus service() override;
    const char* getName() const override { return "ProbeSwitch"; }
    ServicePriority getPriority() const override { return ServicePriority::NORMAL; }
    unsigned long interval_ms = 500; // switchPositionCheckInterval;
    
private:
    ProbeSwitch() = default;
    ~ProbeSwitch() = default;
    static ProbeSwitch* instance;
};

/**
 * @brief Probe pads service - handles expensive ADC pad reading (LOW priority)
 * 
 * Reads probe pads via multiple ADC samples.
 * This is EXPENSIVE (multiple ADC reads) and not time-critical.
 */
class ProbePads : public Service {
public:
    static ProbePads& getInstance();
    ProbePads(const ProbePads&) = delete;
    ProbePads& operator=(const ProbePads&) = delete;
    
    ServiceStatus service() override;
    const char* getName() const override { return "ProbePads"; }
    ServicePriority getPriority() const override { return ServicePriority::LOW; }
    
private:
    ProbePads() = default;
    ~ProbePads() = default;
    static ProbePads* instance;
    unsigned long lastCheckTime = 0;
};

/**
 * @brief Probing system service - handles probe reading and actions
 * 
 * Reads probe position and handles probe button actions.
 * Runs at HIGH priority for responsive probe interaction.
 */
class Probing : public Service {
public:
    // Get singleton instance
    static Probing& getInstance();
    
    // Prevent copying
    Probing(const Probing&) = delete;
    Probing& operator=(const Probing&) = delete;
    
    // Service interface
    ServiceStatus service() override;
    const char* getName() const override { return "Probing"; }
    ServicePriority getPriority() const override { return ServicePriority::HIGH; }
    
    // Member variables (previously globals)
    volatile int sfProbeMenu = 0;
    unsigned long probingTimer = 0;
    
    int probePin = 10;
    int buttonPin = 9;
    
    volatile unsigned long blockProbing = 0;
    volatile unsigned long blockProbingTimer = 0;
    
    volatile unsigned long blockProbeButton = 0;
    volatile unsigned long blockProbeButtonTimer = 0;
    
    volatile int connectOrClearProbe = 0;
    volatile int node1or2 = 0;
    int probeHighlight = 0;
    int logoTopSetting[2] = {0, 0};
    int logoBottomSetting[2] = {0, 0};
    int buildingTopSetting[2] = {0, 0};
    int buildingBottomSetting[2] = {0, 0};
    int showProbeCurrent = 0;
    
    int probePowerDAC = 0;
    int lastProbePowerDAC = 0;
    bool probePowerDACChanged = false;
    
    volatile int removeFade = 0;
    volatile bool bufferPowerConnected = false;
    
    int debugProbing = 0;
    
    volatile int showingProbeLEDs = 0;
int switchPosition = -1; // -1 = unknown, 0 = measure, 1 = select
    int lastSwitchPositions[3] = {0, 0, 0};
    
    int probeRowMapByPad[108];
    int probeRowMap[108];
    
    int lastProbeLEDs = 0;
    int lastProbeButton = 0;
    
    volatile int inPadMenu = 0;
    volatile int checkingButton = 0;
    
    // Track last probe reading for other services
    int getLastProbeReading() const { return lastProbeReading; }
    
    // Check if we need to trigger a goto (for backward compatibility during transition)
    bool needsProbeConnect() const { return triggerProbeConnect; }
    bool needsProbeClear() const { return triggerProbeClear; }
    void clearTriggers() { triggerProbeConnect = false; triggerProbeClear = false; }
    char getPendingInput() const { return pendingInput; }
    void clearPendingInput() { pendingInput = '\0'; }
    
    // Public methods (existing function interfaces)
    int probeMode(int setOrClear = 1, int firstConnection = -1);
    float measureMode(int updateSpeed = 150);
    void checkPads(void);
    int delayWithButton(int delayTime = 1000);
    
    int chooseGPIO(int skipInputOutput = 0);
    int chooseGPIOinputOutput(int gpioChosen);
    int chooseADC(void);
    int chooseDAC(int justPickOne = 0);
    int chooseIsense(void);
    int attachPadsToSettings(int pad);
    
    float voltageSelect(int fiveOrEight = 8);
    int longShortPress(int pressLength = 500);
    
    int selectFromLastFound(void);
    int checkLastFound(int);
    void clearLastFound(void);
    
    int checkProbeButton(void);  // Event-based: consumes button press event (use for one-shot detection)
    int checkProbeButtonState(void);  // State-based: reads current hardware state (use in loops)
    int checkProbeDoubleClick(unsigned long timeout, int waitForRelease = 0);
    int readFloatingOrState(int pin = 0, int row = 0);
    
    int checkSwitchPosition(void);
    float checkProbeCurrent(void);
    float checkProbeCurrentZero(void);
    
    void routableBufferPower(int offOn, int flash = 0, int force = 0);
    
    void startProbe(long probeSpeed = 25000);
    void stopProbe();
    
    int selectSFprobeMenu(int function = 0);
    
    int getNothingTouched(int samples = 8);
    int scanRows(int pin = 0);
    
    int readRails(int pin = 0);
    int justReadProbe(bool allowDuplicates = false, int rawPad = 0);
    int readProbe(void);
    
    int readProbeRaw(int readNothingTouched = 0, bool allowDuplicates = false); 
    int calibrateProbe(void);
    void calibrateDac0(float target = 3.3);
    
    void probeLEDhandler(void);
    int probeToggle(int buttonState = -1);  // Note: Currently unused, global function used instead
    
private:
    Probing();
    ~Probing() = default;
    
    static Probing* instance;
    int lastProbeReading = 0;
    unsigned long lastButtonCheckTime = 0;
    unsigned long waitTimer = 0;
    
    // Flags for triggering probe actions (used during transition from goto-based code)
    bool triggerProbeConnect = false;
    bool triggerProbeClear = false;
    char pendingInput = '\0';
    
    // Handle probe button actions and toggle logic
    void handleProbeButtonActions();
    
    // Handle encoder-based node selection in probe mode
    void handleEncoderCursorNavigation(
        int setOrClear,
        int node1or2,
        const int* nodesToConnect,
        int connectOrClearProbe,
        unsigned long probeModeStartTime,
        long& lastEncoderPosition,
        float& encoderAccumulator,
        int& encoderCursorNode,
        int& cursorZone,
        int& subIndex,
        int& lastEncoderCursorNode,
        int& lastCursorZone,
        int& lastSubIndex,
        unsigned long& lastEncoderMovement,
        bool& encoderCursorVisible,
        int& persistentEncoderCursorNode,
        int& persistentCursorZone,
        int& persistentSubIndex,
        int* row,
        int* connectedRows,
        int& connectedRowsIndex,
        EncoderAccelerator& encoderAccel,
        unsigned long encoderHideTimeout
    );
};

// Global references for clean syntax
extern ProbeButton& probeButton;
extern ProbeSwitch& probeSwitch;
extern ProbePads& probePads;

// Backward compatibility - global references to singleton members
extern volatile int& sfProbeMenu;
extern unsigned long& probingTimer;
extern int& probePin;
extern int& buttonPin;
extern volatile unsigned long& blockProbeButton;
extern volatile unsigned long& blockProbeButtonTimer;
extern volatile int& connectOrClearProbe;
extern volatile int& node1or2;
extern int& probeHighlight;
extern volatile int& removeFade;
extern volatile bool& bufferPowerConnected;
extern int& debugProbing;
extern volatile int& showingProbeLEDs;
extern int& switchPosition;

// Encoder cursor override for LED display functions
extern volatile int globalEncoderCursorNode;      // -1 = hidden, else breadboard node
extern volatile int globalEncoderCursorInHeader;  // 1 if in nano header
extern volatile uint32_t globalEncoderCursorColor; // Cursor color
extern int& probePowerDAC;
extern int& lastProbePowerDAC;
extern bool& probePowerDACChanged;
extern int& showProbeCurrent;


// Legacy wrapper functions for backward compatibility
// These call the corresponding methods on the singleton instance
inline int probeMode(int setOrClear = 1, int firstConnection = -1) {
    return Probing::getInstance().probeMode(setOrClear, firstConnection);
}

inline float measureMode(int updateSpeed = 150) {
    return Probing::getInstance().measureMode(updateSpeed);
}

inline void checkPads(void) {
    Probing::getInstance().checkPads();
}

inline int checkProbeButton(void) {
    return Probing::getInstance().checkProbeButton();
}

inline int checkProbeButtonState(void) {
    return Probing::getInstance().checkProbeButtonState();
}

inline int checkSwitchPosition(void) {
    return Probing::getInstance().checkSwitchPosition();
}

inline int justReadProbe(bool allowDuplicates = false, int rawPad = 0) {
    return Probing::getInstance().justReadProbe(allowDuplicates, rawPad);
}

inline void routableBufferPower(int offOn, int flash = 0, int force = 0) {
    Probing::getInstance().routableBufferPower(offOn, flash, force);
}

inline int getNothingTouched(int samples = 8) {
    return Probing::getInstance().getNothingTouched(samples);
}

inline float checkProbeCurrentZero(void) {
    return Probing::getInstance().checkProbeCurrentZero();
}

inline void probeLEDhandler(void) {
    Probing::getInstance().probeLEDhandler();
}

inline int readProbeRaw(int readNothingTouched = 0, bool allowDuplicates = false) {
    return Probing::getInstance().readProbeRaw(readNothingTouched, allowDuplicates);
}

inline float checkProbeCurrent(void) {
    return Probing::getInstance().checkProbeCurrent();
}

inline int delayWithButton(int delayTime = 1000) {
    return Probing::getInstance().delayWithButton(delayTime);
}

inline int chooseADC(void) {
    return Probing::getInstance().chooseADC();
}

inline int chooseDAC(int justPickOne = 0) {
    return Probing::getInstance().chooseDAC(justPickOne);
}

inline int chooseIsense(void) {
    return Probing::getInstance().chooseIsense();
}

inline int chooseGPIO(int skipInputOutput = 0) {
    return Probing::getInstance().chooseGPIO(skipInputOutput);
}

inline int longShortPress(int pressLength = 500) {
    return Probing::getInstance().longShortPress(pressLength);
}

inline float voltageSelect(int fiveOrEight = 8) {
    return Probing::getInstance().voltageSelect(fiveOrEight);
}

inline int readProbe(void) {
    return Probing::getInstance().readProbe();
}

inline void startProbe(long probeSpeed = 25000) {
    Probing::getInstance().startProbe(probeSpeed);
}

// Additional references for backward compatibility
extern volatile int& inPadMenu;
extern volatile int& checkingButton;
extern int& lastProbeLEDs;

// Export probe maps for Apps.cpp
extern int (&probeRowMap)[108];
extern int (&probeRowMapByPad)[108];

// Export pad settings
extern int (&logoTopSetting)[2];
extern int (&logoBottomSetting)[2];
extern int (&buildingTopSetting)[2];
extern int (&buildingBottomSetting)[2];

// Global state flags from main.cpp
extern volatile int probeActive;

extern volatile bool core1busy;
extern volatile bool core2busy;
extern volatile int loadingFile;

// Timestamp for last INA219 probe current check (for measure mode timing coordination)
extern volatile unsigned long lastProbeCurrentCheckTime;
  

#endif