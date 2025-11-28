/*
 * MicroPython embed API bridge for Jumperless
 * This provides a bridge between Arduino C++ code and the MicroPython runtime
 */

#include <stdio.h>
#include <string.h>
#include "micropython_embed.h"
#include "py/compile.h"
#include "py/runtime.h"
#include "py/repl.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "py/cstack.h"  // Use newer cstack API instead of deprecated stackctrl
#include "py/nlr.h"
#include "py/builtin.h"
#include "py/mphal.h"
#include "shared/runtime/gchelper.h"
#include "JumperlessDefines.h"

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

// Initialize MicroPython runtime
int mp_embed_init(void *heap, size_t heap_size, void *stack_top) {
    // Use the newer cstack API with proper stack limit initialization
    // Define a reasonable stack size for embedded systems (8KB)
    #if OG_JUMPERLESS == 1
    const size_t stack_size = 16 * 1024;  // 16KB stack size
    #else
    const size_t stack_size = 32 * 1024;  // 32KB stack size
    #endif
    mp_cstack_init_with_top(stack_top, stack_size);
    
    gc_init(heap, (char*)heap + heap_size);
    mp_init();
    return 0;
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
            mp_obj_t module_fun = mp_compile(&parse_tree, source_name, true);
            if (module_fun != MP_OBJ_NULL) {
                mp_call_function_0(module_fun);
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
    // For embedded use, we can't do much here - just reset
    mp_hal_stdout_tx_strn_cooked("FATAL: uncaught exception\n", 26);
    // CRITICAL: This function is marked NORETURN - must not return!
    // Without this infinite loop, stack corruption WILL occur
    // while(1) {
    //     // Infinite loop - this should never be reached in normal operation
    // }
}

#ifdef __cplusplus
}
#endif 