#include "externVars.h"
#include <Arduino.h>    // millis()
#include "pico/mutex.h"
#include "tusb.h"
#include "LEDs.h"  // For LogoPalette enum
#include "JumperlOS.h"  // ContextManager / ContextType for systemIdleForFlush
#include "USBfs.h"      // usbMountedByHost
#include "Commands.h"   // refreshInProgress
#include "Probing.h"    // loadingFile

// TinyUSB task function (C linkage)
#ifdef USE_TINYUSB
extern "C" void tud_task(void);
#endif

// Single point of definition (ODR).
volatile bool core1busy = false;
volatile bool core2busy = false;

volatile bool pauseCore2 = false;

// Filesystem activity indicator - set during flash/filesystem operations
// Used by LEDs.cpp to show colored logo during saves
volatile bool filesystemActive = false;
volatile unsigned long filesystemActiveUntil = 0;  // Minimum display time tracking
const unsigned long FILESYSTEM_INDICATOR_MIN_MS = 50;  // 1/4 second minimum display

// Configurable palette for filesystem indicator - change this to any LogoPalette value:
//   PALETTE_RAINBOW, PALETTE_COLD, PALETTE_HOT, PALETTE_PINK, PALETTE_YELLOW,
//   PALETTE_GREEN, PALETTE_ORANGE, PALETTE_TURQUOISE, PALETTE_CHARTREUSE, PALETTE_PURPLE, PALETTE_WHITE
int filesystemIndicatorPalette = PALETTE_YELLOW;

// Measure mode indicator - set by MeasureMode service when actively measuring
volatile bool measureModeActive = false;
int measureModeIndicatorPalette = PALETTE_PURPLE;

// Undo/history activity indicator
volatile unsigned long undoActivityUntil = 0;
int undoIndicatorPalette = PALETTE_YELLOW;

// User-input timestamp - bumped from probe buttons, encoder, serial RX, etc.
// Read by systemIdleForFlush() to gate background flash writes on a real
// quiet window rather than a fixed timer.
volatile uint32_t lastUserInputMs = 0;

void noteUserInput(void) {
    lastUserInputMs = millis();
}

// =============================================================================
// MULTICORE SYNCHRONIZATION IMPLEMENTATION
// =============================================================================

// Mutex instances
mutex_t core_sync_mutex;
mutex_t fs_mutex;


// Track if we've been initialized
static bool sync_initialized = false;

void core_sync_init(void) {
    if (!sync_initialized) {
        mutex_init(&core_sync_mutex);
        mutex_init(&fs_mutex);
        sync_initialized = true;
    }
}

// =============================================================================
// Core Synchronization Functions
// =============================================================================

void core_sync_acquire(void) {
    if (sync_initialized) {
        mutex_enter_blocking(&core_sync_mutex);
    }
}

void core_sync_release(void) {
    if (sync_initialized) {
        mutex_exit(&core_sync_mutex);
    }
}

bool core_sync_try_acquire(void) {
    if (!sync_initialized) return true;  // Not initialized, allow access
    return mutex_try_enter(&core_sync_mutex, nullptr);
}

bool core_sync_acquire_timeout_ms(uint32_t timeout_ms) {
    if (!sync_initialized) return true;
    return mutex_enter_timeout_ms(&core_sync_mutex, timeout_ms);
}



// =============================================================================
// Filesystem Mutex Functions
// =============================================================================

void fs_mutex_acquire(void) {
    if (sync_initialized) {
        mutex_enter_blocking(&fs_mutex);
        filesystemActive = true;  // Visual indicator for logo LEDs
        // Don't set filesystemActiveUntil here - set it on release so the
        // display window starts AFTER the operation (when Core2 is unpaused)
    }
}

void fs_mutex_release(void) {
    if (sync_initialized) {
        filesystemActive = false;  // Clear the active flag
        // Set the display window to start NOW (when operation ends and Core2 may be unpaused)
        filesystemActiveUntil = millis() + FILESYSTEM_INDICATOR_MIN_MS;
        mutex_exit(&fs_mutex);
    }
}

bool fs_mutex_try_acquire(void) {
    if (!sync_initialized) return true;  // Not initialized, allow access
    bool acquired = mutex_try_enter(&fs_mutex, nullptr);
    if (acquired) {
        filesystemActive = true;  // Visual indicator for logo LEDs
    }
    return acquired;
}

bool fs_mutex_acquire_timeout_ms(uint32_t timeout_ms) {
    if (!sync_initialized) return true;
    bool acquired = mutex_enter_timeout_ms(&fs_mutex, timeout_ms);
    if (acquired) {
        filesystemActive = true;  // Visual indicator for logo LEDs
    }
    return acquired;
}

// =============================================================================
// Flash Operation Helpers
// =============================================================================

bool pauseCore2ForFlash(uint32_t timeout_ms) {
    bool was_paused = pauseCore2;
    pauseCore2 = true;
    __dmb();  // Memory barrier to ensure Core2 sees the pause
    
    // Wait for Core2 to actually pause (check core2busy)
    // Also service USB during wait to prevent disconnect
    uint32_t wait_start = millis();
    while (core2busy && (millis() - wait_start < timeout_ms)) {
        #ifdef USE_TINYUSB
        tud_task();
        #endif
        
        delayMicroseconds(100);
    }
    
    return was_paused;
}

void unpauseCore2ForFlash(bool was_paused) {
    pauseCore2 = was_paused;
    __dmb();  // Memory barrier to ensure Core2 sees the unpause
}

// =============================================================================
// Idle gate for deferred file-cache flushes
// =============================================================================
// True iff the system looks quiescent enough that hitting flash won't
// disturb anything the user is actively doing.
//
// Why a context check and not just probeActive: the JumperlOS ContextManager
// already tracks what UI mode we're in. Top of stack == NONE or MAIN_MENU
// covers probe-mode, click-wheel menu, editor, REPL, file browser, help,
// debug, and apps in one check. Each of those pushes its own context on
// entry, so we don't need per-mode flags.
bool systemIdleForFlush(uint32_t quietMs) {
    // Top-of-stack context check. Most interactive modes (editor, REPL,
    // file browser, click-wheel menu, help, debug, apps) push their own
    // context. PROBING is the notable exception - probe mode does NOT
    // push a context today, so we also check probeActive directly below.
    ContextType top = contextManager.currentContext();
    bool inIdleContext = (top == ContextType::NONE || top == ContextType::MAIN_MENU);
    if (!inIdleContext) return false;

    extern volatile int probeActive;
    if (probeActive) return false;            // probe mode is mid-session
    if (refreshInProgress) return false;      // mid-reroute
    if (loadingFile) return false;            // parsing a slot/script
    if (core1busy) return false;              // Core 0 is doing critical work
    if (usbMountedByHost) return false;       // host is the writer right now
    if ((uint32_t)(millis() - lastUserInputMs) <= quietMs) return false;
    return true;
}

// Note: Safe file operations have moved to FilesystemStuff.cpp
