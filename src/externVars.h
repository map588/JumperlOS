#pragma once
#include "TuiGlue.h"

// Pico SDK multicore synchronization primitives
#include "pico/mutex.h"

extern volatile bool core1busy;
extern volatile bool core2busy;

extern volatile bool pauseCore2;
extern TuiGlue tuiGlue;

// =============================================================================
// MULTICORE SYNCHRONIZATION SYSTEM
// =============================================================================
// These primitives ensure thread-safe access to shared resources between cores.
// The Pico SDK mutex provides proper hardware-backed mutual exclusion.
//
// Usage:
//   - Call core_sync_init() once in setup() before Core 2 starts
//   - Use core_sync_acquire() / core_sync_release() around shared resource access
//   - Use core_sync_try_acquire() for non-blocking access attempts
//   - Use fs_mutex_acquire() / fs_mutex_release() for filesystem operations
// =============================================================================

// Core synchronization mutex - protects core1busy, core2busy, shared data structures
extern mutex_t core_sync_mutex;

// Filesystem mutex - protects all FatFS operations
extern mutex_t fs_mutex;

// Initialize synchronization primitives (call once in setup())
void core_sync_init(void);

// Core synchronization functions
// These provide mutual exclusion between cores for accessing shared resources
void core_sync_acquire(void);     // Blocking acquire
void core_sync_release(void);     // Release
bool core_sync_try_acquire(void); // Non-blocking, returns true if acquired

// Filesystem mutex functions
// Use these around ALL filesystem operations to prevent concurrent access
void fs_mutex_acquire(void);      // Blocking acquire
void fs_mutex_release(void);      // Release
bool fs_mutex_try_acquire(void);  // Non-blocking, returns true if acquired

// Timeout versions (returns true if acquired within timeout)
bool core_sync_acquire_timeout_ms(uint32_t timeout_ms);
bool fs_mutex_acquire_timeout_ms(uint32_t timeout_ms);

// =============================================================================
// FLASH OPERATION HELPERS
// =============================================================================
// Use these before/after flash write operations to prevent Core2 crashes.
// Flash writes disable XIP cache, so Core2 must not execute code from flash.

/**
 * Pause Core2 for flash operations and wait for it to actually stop
 * @param timeout_ms Maximum time to wait for Core2 to pause (default 100ms)
 * @return true if Core2 was previously paused (restore with unpauseCore2ForFlash)
 */
bool pauseCore2ForFlash(uint32_t timeout_ms = 100);

/**
 * Unpause Core2 after flash operations
 * @param was_paused Value returned by pauseCore2ForFlash (to restore previous state)
 */
void unpauseCore2ForFlash(bool was_paused);

// Note: Safe file operations (safeFileOpen, safeFileClose, etc.) are now in FilesystemStuff.h
