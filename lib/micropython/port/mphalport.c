/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2023 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "py/mphal.h"

#include "py/runtime.h"
#include "py/lexer.h"
#include "py/builtin.h"
#include "py/mperrno.h"
#include "py/objstr.h"
#include <string.h>

// Forward declaration for filesystem bridge functions
extern char* jl_fs_read_file(const char* path);
extern void* jl_fs_open_file(const char* path, const char* mode);
extern int jl_fs_stat_isdir(const char* path);
extern int jl_fs_exists(const char* path);

// Import the global stream from our main Arduino code
extern void* global_mp_stream_ptr;
extern void arduino_serial_write(const char *str, int len, void *stream);
extern int arduino_serial_read(void *stream);
extern const mp_obj_type_t mp_type_jfs_file;

// Local copy of the JFS file object layout (defined in modules/jumperless/modjumperless.c)
typedef struct _mp_obj_jfs_file_t {
    mp_obj_base_t base;
    void* file_handle;
    uint8_t is_open;
    uint8_t is_binary;
} mp_obj_jfs_file_t;

// Send string of given length to stdout, converting \n to \r\n.
// Note: Implementation moved to Python.cpp for Jumperless-specific functionality
// void mp_hal_stdout_tx_strn_cooked(const char *str, size_t len) {
//     if (global_mp_stream_ptr) {
//         arduino_serial_write(str, len, global_mp_stream_ptr);
//     }
// }

// Send string of given length to stdout (raw).
mp_uint_t mp_hal_stdout_tx_strn(const char *str, size_t len) {
    // CRITICAL: Check for NULL and validate pointer before writing
    // During soft reset, global_mp_stream_ptr might be temporarily invalid
    if (global_mp_stream_ptr && (uintptr_t)global_mp_stream_ptr > 0x20000000) {
        arduino_serial_write(str, len, global_mp_stream_ptr);
    }
    return len;
}

// Send string to stdout (raw).
void mp_hal_stdout_tx_str(const char *str) {
    mp_hal_stdout_tx_strn(str, strlen(str));
}

// Note: mp_hal_set_interrupt_char is implemented in Python_Proper.cpp
// to integrate with our custom interrupt handling system

// Forward declarations for interrupt checking and Arduino timing
extern void mp_hal_check_interrupt(void);
extern void delayMicroseconds(unsigned int us);
extern int getCurrentInterruptChar(void);

// Receive single character from stdin.
// CRITICAL: When no data is available, we must service USB and yield to prevent
// starvation. MicroPython's readline() calls this in a tight for(;;) loop during
// input(). Without USB servicing here, the USB CDC layer desyncs and causes
// hard faults when the script tries to write output after input() returns.
int mp_hal_stdin_rx_chr(void) {
    void *stdin_stream = global_mp_stream_ptr;
    if (stdin_stream) {
        for (;;) {
            int c = arduino_serial_read(stdin_stream);
            if (c != -1) {
                // RACE CONDITION FIX: arduino_serial_read() can consume the
                // interrupt character (Ctrl+Q=0x11 / Ctrl+C=0x03) before
                // check_stream_for_interrupt() gets to peek at it. tud_task()
                // inside mp_hal_check_interrupt() delivers USB data to the
                // buffer, but the throttled check_stream_for_interrupt() may
                // not run before the next loop iteration's read consumes it.
                //
                // Without this check, the interrupt byte passes through to
                // MicroPython's readline as a regular character instead of
                // raising KeyboardInterrupt. This causes input() to silently
                // swallow Ctrl+Q/Ctrl+C, requiring multiple presses to interrupt.
                int int_char = getCurrentInterruptChar();
                if (int_char >= 0 && c == int_char) {
                    // Consume the interrupt char - don't return it to readline.
                    // Schedule + raise KeyboardInterrupt properly.
                    mp_sched_keyboard_interrupt();
                    mp_handle_pending(true);  // nlr_raise -> mp_embed_exec_str catch
                    continue;  // Fallback if mp_handle_pending didn't raise
                }
                // Normalize \n (LF, 0x0A) to \r (CR, 0x0D) for MicroPython.
                // MicroPython's readline only accepts \r as Enter/line-end.
                // Some hosts (e.g., Jumperless App interactive mode) send \n
                // for Enter instead of \r. Without this, input() ignores Enter
                // and the user can never submit a line.
                if (c == '\n') {
                    c = '\r';
                }
                return c;
            }
            // No data available - check for interrupts (which also services USB
            // via tud_task internally) and yield to prevent CPU spin
            mp_hal_check_interrupt();

            // CRITICAL: Process any pending exceptions (e.g., KeyboardInterrupt).
            // Without this, mp_sched_keyboard_interrupt() only SETS a flag but
            // the exception is never RAISED because the VM's backwards-jump check
            // doesn't run while we're blocked in C code. This mirrors what upstream
            // MicroPython ports do via MICROPY_EVENT_POLL_HOOK.
            // mp_handle_pending(true) will nlr_jump if an exception is pending,
            // which unwinds through mp_embed_exec_str()'s nlr_push catch frame.
            mp_handle_pending(true);

            // The active input stream can change at runtime (e.g., friendly REPL
            // to raw REPL transport). Re-sync to current stream pointer.
            if (stdin_stream != global_mp_stream_ptr) {
                stdin_stream = global_mp_stream_ptr;
                if (!stdin_stream) {
                    return -1;
                }
            }

            delayMicroseconds(100); // Small yield to prevent CPU spin
        }
    }
    // For embedded use, we don't support stdin input
    return -1; // No character available
}

// // Provide import stat for the embed port (needed when MICROPY_VFS is disabled)
// mp_import_stat_t mp_import_stat(const char *path) {
//     if (jl_fs_stat_isdir(path)) {
//         return MP_IMPORT_STAT_DIR;
//     }
//     if (jl_fs_exists(path)) {
//         return MP_IMPORT_STAT_FILE;
//     }
//     return MP_IMPORT_STAT_NO_EXIST;
// }

// // Lexer function for reading Python files through our filesystem bridge
// mp_lexer_t *mp_lexer_new_from_file(qstr filename) {
//     // Convert qstr to C string
//     const char *path = qstr_str(filename);
    
//     // Read file contents using our filesystem bridge
//     char *content = jl_fs_read_file(path);
//     if (content == NULL) {
//         mp_raise_OSError(MP_ENOENT);
//     }
    
//     // Create lexer from the file contents
//     // The lexer will take ownership of the content string
//     mp_lexer_t *lex = mp_lexer_new_from_str_len(filename, content, strlen(content), 0);
    
//     // Note: content should not be freed here as lexer now owns it
//     // MicroPython will handle freeing when the lexer is destroyed
//     return lex;
// }

// // Provide builtin open() using the JFS backend (since MICROPY_VFS is disabled)
// mp_obj_t mp_builtin_open(size_t n_args, const mp_obj_t *args, mp_map_t *kwargs) {
//     (void)kwargs; // encoding/newline not supported in this minimal backend
//     const char *path = mp_obj_str_get_str(args[0]);
//     const char *mode = (n_args > 1) ? mp_obj_str_get_str(args[1]) : "r";

//     uint8_t is_binary = strchr(mode, 'b') != NULL ? 1 : 0;
//     void *fh = jl_fs_open_file(path, mode);
//     if (!fh) {
//         mp_raise_OSError(MP_ENOENT);
//     }

//     mp_obj_jfs_file_t *file_obj = mp_obj_malloc_with_finaliser(mp_obj_jfs_file_t, &mp_type_jfs_file);
//     file_obj->file_handle = fh;
//     file_obj->is_open = true;
//     file_obj->is_binary = is_binary;

//     return MP_OBJ_FROM_PTR(file_obj);
// }
// MP_DEFINE_CONST_FUN_OBJ_KW(mp_builtin_open_obj, 1, mp_builtin_open);

// HAL timing functions - basic implementations for embedded use
// Note: mp_hal_delay_ms and mp_hal_ticks_ms are defined in Python_Proper.cpp
#include <Arduino.h>
#include "hardware/clocks.h"

void mp_hal_delay_us(mp_uint_t us) {
    delayMicroseconds(us);
}

mp_uint_t mp_hal_ticks_us(void) {
    return micros();
}

mp_uint_t mp_hal_ticks_cpu(void) {
    // For RP2040/RP2350, we can use the same as ticks_us
    // In a real implementation, this might use a higher resolution counter
    return micros();
}

// Get CPU frequency in Hz - required by machine.bitstream for precise timing
uint32_t mp_hal_get_cpu_freq(void) {
    return clock_get_hz(clk_sys);
}

