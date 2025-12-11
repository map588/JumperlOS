/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Jim Mussared
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

// This is a translation of the cycle counter implementation in ports/stm32/machine_bitstream.c.
// Adapted for Jumperless V5 (RP2350) embedded port.

#include "py/mpconfig.h"
#include "py/mphal.h"
#include "hardware/structs/systick.h"

#if MICROPY_PY_MACHINE_BITSTREAM || 1

#if PICO_RP2350
#define MP_HAL_BITSTREAM_NS_OVERHEAD  (5)
#else
#define MP_HAL_BITSTREAM_NS_OVERHEAD  (9)
#endif

// SysTick is a 24-bit down counter
#define SYSTICK_MAX (0x00FFFFFF)

#if PICO_RISCV

__attribute__((naked)) void mcycle_init(void) {
    __asm volatile (
        "li a0, 4\n"
        "csrw mcountinhibit, a0\n"
        "ret\n"
        );
}

__attribute__((naked)) uint32_t mcycle_get(void) {
    __asm volatile (
        "csrr a0, mcycle\n"
        "ret\n"
        );
}

#endif

// Forward declare debug helper
extern void jl_debug_printf(const char* format, ...);
extern bool debugGpioPinOwnership;

void __time_critical_func(machine_bitstream_high_low)(mp_hal_pin_obj_t pin, uint32_t *timing_ns, const uint8_t *buf, size_t len) {
    uint32_t fcpu_mhz = mp_hal_get_cpu_freq() / 1000000;
    
    if (debugGpioPinOwnership) {
        jl_debug_printf("[BITSTREAM] Starting: pin=%u, len=%u bytes, CPU=%u MHz\n", pin, len, fcpu_mhz);
        jl_debug_printf("[BITSTREAM] Raw timing: [%u, %u, %u, %u] ns\n", 
                       timing_ns[0], timing_ns[1], timing_ns[2], timing_ns[3]);
    }
    
    // Convert ns to clock ticks [high_time_0, period_0, high_time_1, period_1].
    for (size_t i = 0; i < 4; ++i) {
        timing_ns[i] = fcpu_mhz * timing_ns[i] / 1000;
        if (timing_ns[i] > (2 * MP_HAL_BITSTREAM_NS_OVERHEAD)) {
            timing_ns[i] -= MP_HAL_BITSTREAM_NS_OVERHEAD;
        }
        if (i % 2 == 1) {
            // Convert low_time to period (i.e. add high_time).
            timing_ns[i] += timing_ns[i - 1] - MP_HAL_BITSTREAM_NS_OVERHEAD;
        }
    }
    
    if (debugGpioPinOwnership) {
        jl_debug_printf("[BITSTREAM] Converted timing: [%u, %u, %u, %u] ticks\n", 
                       timing_ns[0], timing_ns[1], timing_ns[2], timing_ns[3]);
    }
    
    mp_hal_pin_output(pin);

    if (debugGpioPinOwnership) {
        jl_debug_printf("[BITSTREAM] Pin configured as output, disabling interrupts...\n");
    }

    uint32_t irq_state = mp_hal_quiet_timing_enter();

    if (debugGpioPinOwnership) {
        jl_debug_printf("[BITSTREAM] Interrupts disabled, starting bit loop for %u bytes...\n", len);
    }

    #if PICO_ARM

    // Save original SysTick configuration
    uint32_t systick_rvr_save = systick_hw->rvr;
    uint32_t systick_csr_save = systick_hw->csr;

    // Set systick reset value.
    systick_hw->rvr = 0x00FFFFFF;

    // Enable the systick counter, source CPU clock.
    systick_hw->csr = 5;

    if (debugGpioPinOwnership) {
        jl_debug_printf("[BITSTREAM] Systick initialized, sending data...\n");
        // Show first few bytes
        if (len > 0) {
            jl_debug_printf("[BITSTREAM] First 4 bytes: [%02x, %02x, %02x, %02x]\n",
                           buf[0], len > 1 ? buf[1] : 0, len > 2 ? buf[2] : 0, len > 3 ? buf[3] : 0);
        }
    }

    for (size_t i = 0; i < len; ++i) {
        uint8_t b = buf[i];
        for (size_t j = 0; j < 8; ++j) {
            uint32_t *t = &timing_ns[b >> 6 & 2];
            uint32_t start_ticks = systick_hw->cvr = SYSTICK_MAX;
            mp_hal_pin_high(pin);
            while ((start_ticks - systick_hw->cvr) < t[0]) {
            }
            b <<= 1;
            mp_hal_pin_low(pin);
            while ((start_ticks - systick_hw->cvr) < t[1]) {
            }
        }
    }

    if (debugGpioPinOwnership) {
        jl_debug_printf("[BITSTREAM] Data transmission complete, restoring SysTick...\n");
    }

    // Restore original SysTick configuration (CRITICAL for Arduino timing!)
    systick_hw->rvr = systick_rvr_save;
    systick_hw->csr = systick_csr_save;

    if (debugGpioPinOwnership) {
        jl_debug_printf("[BITSTREAM] SysTick restored, re-enabling interrupts...\n");
    }

    #elif PICO_RISCV

    mcycle_init();

    if (debugGpioPinOwnership) {
        jl_debug_printf("[BITSTREAM] RISC-V mcycle initialized, sending data...\n");
    }

    for (size_t i = 0; i < len; ++i) {
        uint8_t b = buf[i];
        for (size_t j = 0; j < 8; ++j) {
            uint32_t *t = &timing_ns[b >> 6 & 2];
            uint32_t start_ticks = mcycle_get();
            mp_hal_pin_high(pin);
            while ((mcycle_get() - start_ticks) < t[0]) {
            }
            b <<= 1;
            mp_hal_pin_low(pin);
            while ((mcycle_get() - start_ticks) < t[1]) {
            }
        }
    }

    if (debugGpioPinOwnership) {
        jl_debug_printf("[BITSTREAM] RISC-V data transmission complete\n");
    }

    #endif

    mp_hal_quiet_timing_exit(irq_state);
    
    if (debugGpioPinOwnership) {
        jl_debug_printf("[BITSTREAM] Complete! Interrupts restored.\n");
    }
}

#endif // MICROPY_PY_MACHINE_BITSTREAM
