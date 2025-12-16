/*
 * MicroPython port configuration for Jumperless embedding
 * Based on the embed port with built-in modules enabled
 */

#include <stdint.h>
#include <alloca.h>
#include <stddef.h>

// Basic type definitions
typedef intptr_t mp_int_t;
typedef uintptr_t mp_uint_t;
typedef float mp_float_t;
typedef long mp_off_t;

// Hardware abstraction layer (HAL) types for machine module
typedef uint32_t mp_hal_pin_obj_t;

// Remove conflicting typedefs - let MicroPython define these
// typedef struct _mp_obj_base_t mp_obj_base_t;
// typedef struct _mp_obj_t *mp_obj_t;

// Memory allocation - minimal for microcontroller
#ifndef PATH_MAX
#define PATH_MAX 256
#endif
#define MICROPY_ALLOC_PATH_MAX      (256)
#define MICROPY_ENABLE_GC           (1)



// GC register scanning configuration for ARM Cortex-M33 (RP2350)
// Use setjmp-based fallback for safer, more portable GC register scanning.
// This captures all callee-saved registers reliably via setjmp() instead of
// relying on architecture-specific inline assembly which can be fragile.
#define MICROPY_GCREGS_SETJMP       (1)

#define MICROPY_HELPER_REPL         (1)
#define MICROPY_HELPER_LEXER_UNIX   (0)  
#define MICROPY_MEM_STATS           (1)  
#define MICROPY_KBD_EXCEPTION      (1)

#define MICROPY_CONFIG_ROM_LEVEL  MICROPY_CONFIG_ROM_LEVEL_FULL_FEATURES

// Enable compiler and event-driven REPL for pyexec_event_repl_process_char()
#define MICROPY_ENABLE_COMPILER     (1)
#define MICROPY_REPL_EVENT_DRIVEN   (1)

// CRITICAL: Enable finalizers for proper cleanup of file handles and other resources
// This allows __del__ methods to be called during garbage collection
#define MICROPY_ENABLE_FINALISER    (1)

// REPL configuration - basic only
#define MICROPY_REPL_AUTO_INDENT    (1)  
#define MICROPY_REPL_EMACS_KEYS     (1)  

// Float support - enable single-precision floating point
#define MICROPY_FLOAT_IMPL          (MICROPY_FLOAT_IMPL_FLOAT)
#define MICROPY_PY_BUILTINS_FLOAT   (1)

// Python builtins - minimal set
#define MICROPY_PY_BUILTINS_COMPILE (1)  
#define MICROPY_PY_BUILTINS_EVAL_EXEC (1)
#define MICROPY_PY_BUILTINS_HELP    (1)
#define MICROPY_PY___FILE__         (0)  // Disable to avoid import path issues
#define MICROPY_PY_SYS_PLATFORM     "jumperless-rp2350"
#define MICROPY_PY_SYS_EXIT         (1)
#define MICROPY_PY_SYS_PATH         (1)  
#define MICROPY_PY_SYS_PS1_PS2      (1)  // Enable for REPL
#define MICROPY_PY_SYS_STDIO_BUFFER (1)  
#define MICROPY_PY_SYS_ATTR_DELEGATION (1)  

#define MICROPY_PY_BUILTINS_INPUT   (1)
#define MICROPY_PY_FSTRINGS         (1)


#define MICROPY_STACK_CHECK (1)
#define MICROPY_STACK_CHECK_MARGIN (1024)  // 1KB margin for embedded systems

// Basic modules - minimal set
#define MICROPY_PY_ARRAY            (1)
#define MICROPY_PY_COLLECTIONS      (1)  

#define MICROPY_PY_STRUCT           (1)
#define MICROPY_PY_MATH             (1)
#define MICROPY_PY_GC               (1)
#define MICROPY_PY_BINASCII         (1)  
#define MICROPY_PY_ERRNO            (1)  
#define MICROPY_PY_JSON             (1)
#define MICROPY_PY_RE               (1)
#define MICROPY_PY_HEAPQ            (1)
#define MICROPY_PY_HASHLIB          (1)
#define MICROPY_PY_RANDOM           (1)

// Standard library modules - disable most to save memory
#define MICROPY_PY_TIME             (1)  // Keep disabled to avoid import issues
#define MICROPY_PY_TIME_TIME_TIME_NS (0)
#define MICROPY_PY_TIME_GMTIME_LOCALTIME_MKTIME (0)

// OS module - keep disabled to avoid port-specific requirements
#define MICROPY_PY_OS               (1)  // Enable now that we include extmod
#define MICROPY_PY_OS_DUPTERM       (0)
#define MICROPY_PY_OS_DUPTERM_NOTIFY (0)
#define MICROPY_PY_OS_SYNC          (0)
#define MICROPY_PY_OS_UNAME         (1)  // Enable uname function
#define MICROPY_PY_OS_URANDOM       (0)

// Machine module - enable with rp2 implementations
#define MICROPY_PY_MACHINE                      (1)
#define MICROPY_PY_MACHINE_RESET                (1)  // Enable machine.reset()
#define MICROPY_PY_MACHINE_BARE_METAL_FUNCS     (1)  // Enable unique_id(), freq()
#define MICROPY_PY_MACHINE_BOOTLOADER           (1)  // Enable machine.bootloader()
#define MICROPY_PY_MACHINE_DISABLE_IRQ_ENABLE_IRQ (1)

// Use extmod/modmachine.c glue; provide only features we implement locally

// Peripherals (extmod glue + rp2 backend files)
#define MICROPY_PY_MACHINE_PWM                  (1)  // Enable PWM
#define MICROPY_PY_MACHINE_PWM_INCLUDEFILE      "../../lib/micropython/port/machine_pwm_jl.c"

#define MICROPY_PY_MACHINE_SPI                  (1)  // Enable hardware SPI
#define MICROPY_PY_MACHINE_SPI_MSB              (0)  // MSB first (standard)
#define MICROPY_PY_MACHINE_SPI_LSB              (1)  // LSB first
#define MICROPY_PY_MACHINE_SOFTSPI              (1)  // Enable software SPI

#define MICROPY_PY_MACHINE_I2C                  (1)  // Enable hardware I2C
#define MICROPY_PY_MACHINE_SOFTI2C              (1)  // Enable software I2C

#define MICROPY_PY_MACHINE_I2S                  (0)  // I2S not implemented yet

#define MICROPY_PY_MACHINE_UART                 (1)  // Already enabled

#define MICROPY_PY_MACHINE_ADC                  (1)  // Enable ADC
#define MICROPY_PY_MACHINE_ADC_INCLUDEFILE      "../../lib/micropython/port/machine_adc_jl.c"

#define MICROPY_PY_MACHINE_WDT                  (1)  // Enable watchdog timer
#define MICROPY_PY_MACHINE_WDT_INCLUDEFILE      "../../lib/micropython/port/machine_wdt_jl.c"

#define MICROPY_PY_MACHINE_BITSTREAM            (1)  // Already enabled
#define MICROPY_PY_MACHINE_PULSE                (0)

// Disable extras to avoid unresolved symbols
#define MICROPY_PY_MACHINE_MEMX                 (0)
#define MICROPY_PY_MACHINE_SIGNAL               (0)
#define MICROPY_PY_MACHINE_PIN_BASE             (0)

// Allow port to extend machine module (e.g., expose Pin, Timer, RTC, etc.)
#define MICROPY_PY_MACHINE_INCLUDEFILE          "../../lib/micropython/port/modmachine_jl.inc"

// Additional useful modules - disable to save memory
#define MICROPY_PY_ONEWIRE          (1)

// Optimize for size but keep features
#define MICROPY_OPT_COMPUTED_GOTO   (1)
#define MICROPY_MODULE_WEAK_LINKS   (1)

// Enable error reporting features
#define MICROPY_ERROR_REPORTING     (MICROPY_ERROR_REPORTING_DETAILED)
#define MICROPY_ENABLE_SOURCE_LINE  (1)

#define MICROPY_ENABLE_EXTERNAL_IMPORT (1)  // Disable to avoid sys.path dependency
#define MICROPY_MALLOC_USES_ALLOCATED_SIZE (1)

// Additional features for embedded use
#define MICROPY_PY_MICROPYTHON_MEM_INFO (1)
#define MICROPY_ENABLE_SCHEDULER    (1)
#define MICROPY_SCHEDULER_DEPTH     (8)

// Enable standard MicroPython VFS so tools (e.g. mpremote/ViperIDE) behave normally
#define MICROPY_VFS                 (1)
#define MICROPY_VFS_FAT             (0)  // Use custom JFS VFS driver, not FatFs blockdev
#define MICROPY_VFS_LFS2            (0)
#define MICROPY_VFS_POSIX           (0)
// Allow the lexer/reader to pull files via VFS (required for imports/open)
#define MICROPY_READER_VFS          (1)
// Disable legacy hand-written os bridge now that VFS+standard os are available
#define MICROPY_JL_CUSTOM_OS_BRIDGE (0)

#define MICROPY_ENABLE_FINALIZER    (1)

// IO needs to be on for standard file objects when using VFS
#define MICROPY_PY_IO_FILEIO        (1)
#define MICROPY_PY_IO               (1)

// Time module configuration
#define MICROPY_PY_TIME_INCLUDEFILE "shared/timeutils/timeutils.h"

// Platform module for os.uname()
#define MICROPY_PY_PLATFORM         (1)

// User C modules (Jumperless module will be added here)
#define MODULE_JUMPERLESS_ENABLED   (1)

// Force all print output through our HAL functions instead of sys.stdout
#define MICROPY_PY_SYS_STDFILES     (0)

// Board name for sys.platform
#define MICROPY_HW_BOARD_NAME "jumperless-v5"
#define MICROPY_HW_MCU_NAME   "rp2350b"

// Built-in modules - minimal set (most modules disabled to save memory)
// Only the jumperless module will be available via MP_REGISTER_MODULE

// Add built-in modules to the list - empty for maximum compatibility
#define MICROPY_PORT_BUILTIN_MODULES \

// Module weak links for compatibility - empty
#define MICROPY_PORT_BUILTIN_MODULE_WEAK_LINKS \

#define MP_STATE_PORT MP_STATE_VM

// Ensure the VM periodically polls for host-driven interrupts (Ctrl-C/Ctrl-D)
#ifdef __cplusplus
extern "C" {
#endif
void mp_hal_check_interrupt(void);
// Forward declare the hook functions - actual declarations are in py/compile.h context
// Using int instead of mp_parse_input_kind_t to avoid header ordering issues
void jl_before_python_exec_hook(int parse_input_kind, unsigned int exec_flags);
void jl_after_python_exec_hook(int parse_input_kind, unsigned int exec_flags, void *exception, int *result);
#ifdef __cplusplus
}
#endif
#define MP_HAL_CHECK_INTERRUPT_DECLARED 1
#define MICROPY_VM_HOOK_LOOP  mp_hal_check_interrupt();

// Hook called before script execution begins
// This allows notification when a script is about to execute
#define MICROPY_BOARD_BEFORE_PYTHON_EXEC jl_before_python_exec_hook

// CRITICAL: Hook called after every script execution to perform cleanup
// This is where we trigger garbage collection to free memory for the next script
#define MICROPY_BOARD_AFTER_PYTHON_EXEC  jl_after_python_exec_hook
