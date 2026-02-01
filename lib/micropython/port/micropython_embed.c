/*
 * MicroPython embed API bridge for Jumperless
 * This provides a bridge between Arduino C++ code and the MicroPython runtime
 */

#include <stdio.h>
#include <string.h>
// CRITICAL: Include MicroPython core headers FIRST to define all types
#include "py/compile.h"     // Defines mp_parse_input_kind_t and related types
#include "py/runtime.h"
#include "py/repl.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "py/cstack.h"  // Use newer cstack API instead of deprecated stackctrl
#include "py/nlr.h"
#include "py/builtin.h"
#include "py/mphal.h"
#include "shared/runtime/gchelper.h"
// Include Jumperless headers AFTER MicroPython headers
#include "JumperlessDefines.h"
#include "micropython_embed.h"

// =============================================================================
// PSRAM Support for Extended MicroPython Heap
// =============================================================================
// When PSRAM is available, we add it as an additional GC heap region using
// MICROPY_GC_SPLIT_HEAP. This gives MicroPython access to ~8MB additional memory.
//
// The Arduino-Pico framework provides PSRAM detection and initialization.
// PSRAM is memory-mapped at address 0x11000000 (PSRAM_BASE on RP2350).
// Detection is done at runtime - the same firmware works with or without PSRAM.
//
// The wrapper function jl_get_psram_size() is implemented in the Arduino C++ code
// and calls rp2040.getPSRAMSize() to detect PSRAM.
#define PSRAM_BASE 0x11000000
// C wrapper for Arduino rp2040.getPSRAMSize() - implemented in Python_Proper.cpp
extern size_t jl_get_psram_size(void);
// Track PSRAM state for reporting
static size_t psram_heap_size = 0;

// Forward declaration of Arduino delay function
void delay(unsigned long ms);

// Forward declarations for Jumperless functions
extern bool jl_in_raw_repl_mode;
void jl_close_all_jfs_files(void);

// Callback function pointers for script execution notifications
// Set by MpRemoteService to receive notifications when scripts start/finish executing
typedef void (*script_callback_t)(void);
extern script_callback_t jl_on_script_begin_callback;
extern script_callback_t jl_on_script_complete_callback;

#ifdef __cplusplus
extern "C" {
#endif

#if MICROPY_PY_MACHINE && MICROPY_PY_MACHINE_UART
// Ensure the UART type from the port backend is retained by the linker
extern const mp_obj_type_t machine_uart_type;
const mp_obj_type_t *jl_retain_machine_uart_type = &machine_uart_type;
#endif

// Minimal stubs for modmachine low-level hooks to satisfy linker
void mp_machine_idle(void) {}
void mp_machine_set_freq(size_t n_args, const mp_obj_t *args) { (void)n_args; (void)args; }
mp_obj_t mp_machine_get_freq(void) { return MP_OBJ_NEW_SMALL_INT(150000000); }
mp_obj_t mp_machine_unique_id(void) { return mp_const_none; }
void mp_machine_lightsleep(size_t n_args, const mp_obj_t *args) { (void)n_args; (void)args; }
NORETURN void mp_machine_deepsleep(size_t n_args, const mp_obj_t *args) { (void)n_args; (void)args; for (;;) {} }
NORETURN void mp_machine_reset(void) { for (;;) {} }
mp_int_t mp_machine_reset_cause(void) { return 0; }

// NOTE: Heap is allocated in Python_Proper.cpp (mp_heap) and passed to mp_embed_init()
// This file does NOT allocate its own heap - that would waste 128KB of RAM!
// The heap variable below is REMOVED - caller provides heap buffer

// Note: HAL functions (arduino_serial_write, arduino_serial_read) are implemented
// in the Arduino C++ code (Python_Proper.cpp) as extern "C" functions

// Initialize MicroPython runtime with optional PSRAM support
int mp_embed_init(void *heap, size_t heap_size, void *stack_top) {
    // Use the newer cstack API with proper stack limit initialization
    // Define a reasonable stack size for embedded systems (8KB)
    #if OG_JUMPERLESS == 1
    const size_t stack_size = 16 * 1024;  // 16KB stack size
    #else
    const size_t stack_size = 32 * 1024;  // 32KB stack size
    #endif
    mp_cstack_init_with_top(stack_top, stack_size);
    
    // Initialize primary GC heap (SRAM)
    gc_init(heap, (char*)heap + heap_size);
    
    // =============================================================================
    // PSRAM Heap Extension (Runtime Detection)
    // =============================================================================
    // Check if PSRAM is present and add it as additional GC heap.
    // This uses MICROPY_GC_SPLIT_HEAP feature to maintain multiple heap regions.
    // The PSRAM is memory-mapped by the Arduino-Pico framework at PSRAM_BASE (0x11000000).
    // Detection is done at runtime - same firmware works with or without PSRAM.
    #if MICROPY_GC_SPLIT_HEAP
    // jl_get_psram_size() wraps rp2040.getPSRAMSize() from Arduino-Pico
    size_t detected_psram_size = jl_get_psram_size();
    if (detected_psram_size > 0) {
        // Add PSRAM region to GC heap
        // Note: gc_add() handles the region as a separate heap segment
        void *psram_start = (void *)PSRAM_BASE;
        void *psram_end = (void *)(PSRAM_BASE + detected_psram_size);
        gc_add(psram_start, psram_end);
        psram_heap_size = detected_psram_size;
    }
    #endif
    
    mp_init();
    return 0;
}

// Get PSRAM heap size (returns 0 if PSRAM not present)
size_t mp_embed_get_psram_size(void) {
    return psram_heap_size;
}

// Deinitialize MicroPython runtime
void mp_embed_deinit(void) {
    mp_deinit();
}

// Execute a string of Python code
int mp_embed_exec_str(const char *str) {
    if (!str) return -1;
    
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        // Compile and execute the string
        qstr source_name = qstr_from_str("<stdin>");
        mp_parse_input_kind_t input_kind = MP_PARSE_FILE_INPUT;
        mp_lexer_t *lex = mp_lexer_new_from_str_len(source_name, str, strlen(str), 0);
        if (lex) {
            mp_parse_tree_t parse_tree = mp_parse(lex, input_kind);
            // Compile as non-REPL file input so expression statements do NOT echo results
            mp_obj_t module_fun = mp_compile(&parse_tree, source_name, false);
            if (module_fun != MP_OBJ_NULL) {
                // Execute code with interrupt handling active
                // Ensure interrupt character is set (usually 3 for Ctrl+C)
                // Note: mp_hal_set_interrupt_char handled externally or by default
                
                mp_call_function_0(module_fun);
                
                // Handle any pending exceptions or callbacks immediately after execution
                mp_handle_pending(true); 
            }
        }
        nlr_pop();
        return 0;
    } else {
        // Handle exception
        mp_obj_print_exception(&mp_plat_print, MP_OBJ_FROM_PTR(nlr.ret_val));
        return -1;
    }
}

// REPL functionality - basic implementation
void mp_embed_repl(void) {
    // This is a simple REPL implementation
    // In practice, the Jumperless firmware handles REPL state management
    // in Python_Proper.cpp, so this is mainly a stub for API completeness
    printf(">>> ");
    
    // The actual REPL loop is handled by the Arduino code
    // This function exists primarily for API compatibility
}

// CRITICAL: Hook called before Python script execution begins
// This function is called by MicroPython's parse_compile_execute() before each
// script starts executing. It's defined as MICROPY_BOARD_BEFORE_PYTHON_EXEC in mpconfigport.h
//
// Purpose:
// 1. Notify MpRemoteService that a script is about to execute
// 2. Allow pre-execution setup or logging
// 3. Track script execution timing
//
// Parameters:
//   parse_input_kind - Type of input (MP_PARSE_FILE_INPUT=1, MP_PARSE_SINGLE_INPUT=2, etc.)
//   exec_flags - Execution flags (EXEC_FLAG_PRINT_EOF, etc.)
//
// Note: Using int/unsigned int in declaration (mpconfigport.h) to avoid header ordering issues
void jl_before_python_exec_hook(int parse_input_kind, unsigned int exec_flags) {
    (void)exec_flags;  // Unused for now
    
    // Only notify for file input (complete scripts), not for REPL single-line inputs
    // MP_PARSE_FILE_INPUT = 1, MP_PARSE_SINGLE_INPUT = 2
    if (parse_input_kind == 1 /* MP_PARSE_FILE_INPUT */) {
        // Notify MpRemoteService that script execution is beginning (if in raw REPL)
        if (jl_in_raw_repl_mode && jl_on_script_begin_callback) {
            jl_on_script_begin_callback();
        }
    }
}

// CRITICAL: Hook called after every Python script execution
// This function is called by MicroPython's parse_compile_execute() after each
// script completes (successfully or with exception). It's defined as
// MICROPY_BOARD_AFTER_PYTHON_EXEC in mpconfigport.h
//
// Purpose:
// 1. Trigger garbage collection to free memory for next script execution
// 2. Close any leaked file handles from scripts that didn't clean up properly
// 3. Provide stable memory state for ViperIDE and other tools
//
// Parameters:
//   parse_input_kind - Type of input (MP_PARSE_FILE_INPUT=1, MP_PARSE_SINGLE_INPUT=2, etc.)
//   exec_flags - Execution flags (EXEC_FLAG_PRINT_EOF, etc.)
//   exception - Pointer to exception object if one occurred (nlr.ret_val), NULL if success
//   result - Pointer to result code from execution
//
// Note: Using int/unsigned int in declaration (mpconfigport.h) to avoid header ordering issues,
// but treating them as mp_parse_input_kind_t and mp_uint_t internally
void jl_after_python_exec_hook(int parse_input_kind, unsigned int exec_flags, void *exception, int *result) {
    (void)exec_flags;        // Unused for now
    (void)exception;         // Unused for now
    (void)result;            // Unused for now
    
    // CRITICAL: Always run garbage collection after script execution
    // This is especially important for raw REPL mode (ViperIDE/mpremote) where
    // multiple scripts run sequentially. Without GC, memory fragments and causes
    // MemoryError on subsequent runs even when total memory is sufficient.
    //
    // Only run GC for file input (complete scripts), not for REPL single-line inputs
    // to avoid performance issues during interactive use
    // MP_PARSE_FILE_INPUT = 1, MP_PARSE_SINGLE_INPUT = 2
    if (parse_input_kind == 1 /* MP_PARSE_FILE_INPUT */) {
        #if MICROPY_ENABLE_GC
        gc_collect();
        #endif
        
        // CRITICAL: Do NOT close files when in raw REPL mode!
        // Raw REPL (used by ViperIDE/mpremote) sends multiple commands that need
        // file handles to persist across commands. For example:
        //   Command 1: f=open('file.txt','w')
        //   Command 2: f.write('data')
        //   Command 3: f.close()
        // If we close files after Command 1, Command 2 will fail with EIO.
        //
        // Only close leaked files after:
        // - Interactive REPL script execution (handled in Python_Proper.cpp)
        // - Soft reset (handled in jl_soft_reboot)
        // - Python exit (handled by jl_exit_python)
        if (!jl_in_raw_repl_mode) {
            jl_close_all_jfs_files();
        }
        
        // Notify MpRemoteService that script execution completed (if in raw REPL)
        // This allows the service to perform post-execution tasks like logging or cleanup
        if (jl_in_raw_repl_mode && jl_on_script_complete_callback) {
            jl_on_script_complete_callback();
        }
    }
}

// HAL implementations that are required by MicroPython

// Import stat function - removed because VFS provides inline implementation

// Garbage collection function
// CRITICAL: Must properly scan CPU registers AND the entire call stack
// to find all live Python objects. The previous implementation only scanned
// mp_state_ctx which caused live objects to be freed, resulting in crashes
// after running scripts multiple times.
void gc_collect(void) {
    gc_collect_start();
    // gc_helper_collect_regs_and_stack() captures callee-saved registers
    // and scans from current stack position to stack_top for GC roots
    gc_helper_collect_regs_and_stack();
    gc_collect_end();
}

// Non-local return (exception handling) failure function  
void nlr_jump_fail(void *val) {
    (void)val;
    // For embedded use, we can't do much here - just halt
    mp_hal_stdout_tx_strn_cooked("\nFATAL: nlr_jump_fail - uncaught exception\n", 43);
    mp_hal_stdout_tx_strn_cooked("MicroPython has encountered a fatal error.\n", 44);
    mp_hal_stdout_tx_strn_cooked("Please reset the device.\n", 26);
    
    // CRITICAL: This function is marked NORETURN - must NOT return!
    // An infinite loop prevents stack corruption
    // Note: We cannot safely recover from this state - trying to reinit MicroPython
    // from within an exception handler is dangerous and could cause more corruption
    // while(1) {
    mp_deinit();
    delay(1000);
    mp_init();
    delay(1000);
    // Could potentially trigger a watchdog reset here if desired
    // }
}

#ifdef __cplusplus
}
#endif 