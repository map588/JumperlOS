/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2021 Damien P. George
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

// Adapted for Jumperless embed port - simplified version without external ADC support
// This file is never compiled standalone, it's included directly from
// extmod/machine_adc.c via MICROPY_PY_MACHINE_ADC_INCLUDEFILE.

// When included from extmod/machine_adc.c, these are already included.
// Add guards to prevent duplicate includes.
#ifndef MICROPY_INCLUDED_PY_RUNTIME_H
#include "py/runtime.h"
#endif
#ifndef MICROPY_INCLUDED_PY_MPHAL_H  
#include "py/mphal.h"
#endif
#include "hardware/adc.h"

// Forward declare the type - it's defined in extmod/machine_adc.c
extern const mp_obj_type_t machine_adc_type;

// ADC pins on RP2: GPIO26=ADC0, GPIO27=ADC1, GPIO28=ADC2, GPIO29=ADC3
// ADC4 is the internal temperature sensor
#ifndef ADC_BASE_PIN
#define ADC_BASE_PIN (26)
#endif

#ifndef NUM_ADC_CHANNELS
#define NUM_ADC_CHANNELS (5)  // 4 GPIO channels + 1 temperature sensor
#endif

#ifndef ADC_TEMPERATURE_CHANNEL_NUM
#define ADC_TEMPERATURE_CHANNEL_NUM (4)
#endif

#define ADC_IS_VALID_GPIO(gpio) ((gpio) >= ADC_BASE_PIN && (gpio) < (ADC_BASE_PIN + NUM_ADC_CHANNELS - 1))
#define ADC_CHANNEL_FROM_GPIO(gpio) ((gpio) - ADC_BASE_PIN)

static uint16_t adc_config_and_read_u16(uint32_t channel) {
    adc_select_input(channel);
    uint32_t raw = adc_read();
    const uint32_t bits = 12;
    // Scale raw reading to 16 bit value using a Taylor expansion (for 8 <= bits <= 16)
    return raw << (16 - bits) | raw >> (2 * bits - 16);
}

/******************************************************************************/
// MicroPython bindings for machine.ADC

#define MICROPY_PY_MACHINE_ADC_CLASS_CONSTANTS \
    { MP_ROM_QSTR(MP_QSTR_CORE_TEMP), MP_ROM_INT(ADC_TEMPERATURE_CHANNEL_NUM) }, \

typedef struct _machine_adc_obj_t {
    mp_obj_base_t base;
    uint32_t channel;
} machine_adc_obj_t;

static void mp_machine_adc_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    machine_adc_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "<ADC channel=%u>", self->channel);
}

// Helper to get pin object (simplified for embed port)
static uint32_t get_pin_id(mp_obj_t pin_obj) {
    // If it's already an integer, return it
    if (mp_obj_is_int(pin_obj)) {
        return mp_obj_get_int(pin_obj);
    }
    // Otherwise try to get it via mp_hal_get_pin_obj
    return mp_hal_get_pin_obj(pin_obj);
}

// ADC(id)
static mp_obj_t mp_machine_adc_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    // Check number of arguments
    mp_arg_check_num(n_args, n_kw, 1, 1, false);

    mp_obj_t source = all_args[0];

    uint32_t channel = -1;
    uint32_t gpio = -1;

    if (mp_obj_is_int(source)) {
        // Try to interpret as channel number first
        channel = mp_obj_get_int(source);
        if (!(channel >= 0 && channel < NUM_ADC_CHANNELS)) {
            // Not a valid ADC channel, try as GPIO pin
            gpio = channel;
            channel = -1;
        }
    } else {
        // Get GPIO pin number
        gpio = get_pin_id(source);
    }

    if (channel == -1 && gpio != (uint32_t)-1) {
        // Check if GPIO has ADC capabilities
        if (!ADC_IS_VALID_GPIO(gpio)) {
            mp_raise_ValueError(MP_ERROR_TEXT("Pin doesn't have ADC capabilities"));
        }
        channel = ADC_CHANNEL_FROM_GPIO(gpio);
    }

    if (channel == (uint32_t)-1) {
        mp_raise_ValueError(MP_ERROR_TEXT("Invalid ADC pin or channel"));
    }

    // Initialise the ADC peripheral if it's not already running.
    if (!(adc_hw->cs & ADC_CS_EN_BITS)) {
        adc_init();
    }

    if (gpio != (uint32_t)-1 && gpio != channel) {
        // Configure the GPIO pin in ADC mode.
        adc_gpio_init(gpio);
    } else if (channel == ADC_TEMPERATURE_CHANNEL_NUM) {
        // Enable temperature sensor.
        adc_set_temp_sensor_enabled(1);
    }

    // Create ADC object.
    machine_adc_obj_t *o = mp_obj_malloc(machine_adc_obj_t, &machine_adc_type);
    o->channel = channel;

    return MP_OBJ_FROM_PTR(o);
}

// read_u16()
static mp_int_t mp_machine_adc_read_u16(machine_adc_obj_t *self) {
    return adc_config_and_read_u16(self->channel);
}
