#include "externVars.h"
#include "pico/mutex.h"
#include "tusb.h"

// TinyUSB task function (C linkage)
#ifdef USE_TINYUSB
extern "C" void tud_task(void);
#endif

// Single point of definition (ODR).
volatile bool core1busy = false;
volatile bool core2busy = false;

volatile bool pauseCore2 = false;

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
    }
}

void fs_mutex_release(void) {
    if (sync_initialized) {
        mutex_exit(&fs_mutex);
    }
}

bool fs_mutex_try_acquire(void) {
    if (!sync_initialized) return true;  // Not initialized, allow access
    return mutex_try_enter(&fs_mutex, nullptr);
}

bool fs_mutex_acquire_timeout_ms(uint32_t timeout_ms) {
    if (!sync_initialized) return true;
    return mutex_enter_timeout_ms(&fs_mutex, timeout_ms);
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
        // Service USB during wait to prevent disconnect
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

// Note: Safe file operations have moved to FilesystemStuff.cpp
