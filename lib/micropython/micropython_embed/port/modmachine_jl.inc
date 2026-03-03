// Extra glue for Jumperless embed port
//
// Jumperless V5 Available GPIOs for machine module:
// GPIO 0, 1 (UART_TX, UART_RX) - also usable as GPIO
// GPIO 6, 7, 9, 10 - dedicated GPIO pins
// GPIO 18, 19 (NANO_RESET_0, NANO_RESET_1) - use with caution
// GPIO 20-27 (GPIO_1..GPIO_8) - primary GPIO pins
// Note: Other GPIOs are used by Jumperless hardware (crosspoint switches, display, etc.)

// Reset cause constants
#define RP2_RESET_PWRON (1)
#define RP2_RESET_WDT (3)

// Forward declarations - implementations will include necessary headers
#ifdef __cplusplus
extern "C" {
#endif
extern void jl_soft_reboot(void);
// Forward declare Pico SDK functions used by machine module
extern void pico_get_unique_board_id(void *id_out);
extern int watchdog_caused_reboot(void);
extern void watchdog_reboot(uint32_t pc, uint32_t sp, uint32_t delay_ms);
// reset_usb_boot is in pico/bootrom.h but may not be linked - make it optional
extern void reset_usb_boot(uint32_t usb_activity_gpio_pin_mask, uint32_t disable_interface_mask) __attribute__((weak));
extern uint32_t mp_hal_get_cpu_freq(void);
#ifdef __cplusplus
}
#endif

// Provide idle hook required by extmod/modmachine.c
static void mp_machine_idle(void) {
    // Allow background processing; nothing required for now
    MICROPY_INTERNAL_EVENT_HOOK;
}

// Custom soft_reset implementation for Jumperless
static mp_obj_t machine_soft_reset_jl(void) {
    jl_soft_reboot();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(machine_soft_reset_jl_obj, machine_soft_reset_jl);

// machine.unique_id() - Get unique chip ID
static mp_obj_t mp_machine_unique_id(void) {
    uint8_t id[8];  // Pico unique ID is 8 bytes
    pico_get_unique_board_id(id);
    return mp_obj_new_bytes(id, 8);
}

// machine.reset() - Hard reset the chip
NORETURN static void mp_machine_reset(void) {
    watchdog_reboot(0, 0, 0);
    while (1) {
        // Wait for watchdog to reset the system
    }
}

// machine.reset_cause() - Get the reset cause
static mp_int_t mp_machine_reset_cause(void) {
    int reset_cause;
    if (watchdog_caused_reboot()) {
        reset_cause = RP2_RESET_WDT;
    } else {
        reset_cause = RP2_RESET_PWRON;
    }
    return reset_cause;
}

// machine.freq() - Get CPU frequency
static mp_obj_t mp_machine_get_freq(void) {
    return MP_OBJ_NEW_SMALL_INT(mp_hal_get_cpu_freq());
}

// machine.freq(freq) - Set CPU frequency (not implemented, just accept the call)
static void mp_machine_set_freq(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    (void)args;
    // CPU frequency setting not implemented for embed port
    // Silently ignore to maintain API compatibility
}

// machine.lightsleep() - Light sleep mode
static void mp_machine_lightsleep(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    (void)args;
    // Light sleep not fully implemented, just idle
    mp_machine_idle();
}

// machine.deepsleep() - Deep sleep mode (just do a soft reset for now)
NORETURN static void mp_machine_deepsleep(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    (void)args;
    // Deep sleep not implemented, do soft reset instead
    jl_soft_reboot();
    while (1) {
        // Should never reach here
    }
}

// machine.bootloader() - Enter USB bootloader mode
NORETURN void mp_machine_bootloader(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    (void)args;
    // Check if reset_usb_boot is available (weak symbol)
    if (reset_usb_boot) {
        reset_usb_boot(0, 0);
    }
    // Fallback: just do a hard reset
    watchdog_reboot(0, 0, 0);
    while (1) {
        // Wait - system will reset
    }
}

// Expose additional globals in machine module
// Note: bitstream, PWM, ADC, WDT, I2C, SPI are automatically added by extmod/modmachine.c
// when their respective MICROPY_PY_MACHINE_* flags are enabled
#define MICROPY_PY_MACHINE_EXTRA_GLOBALS \
    { MP_ROM_QSTR(MP_QSTR_Pin), MP_ROM_PTR(&machine_pin_type) }, \
    { MP_ROM_QSTR(MP_QSTR_Timer), MP_ROM_PTR(&machine_timer_type) }, \
    { MP_ROM_QSTR(MP_QSTR_RTC), MP_ROM_PTR(&machine_rtc_type) }, \
    { MP_ROM_QSTR(MP_QSTR_soft_reset), MP_ROM_PTR(&machine_soft_reset_jl_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_PWRON_RESET), MP_ROM_INT(RP2_RESET_PWRON) }, \
    { MP_ROM_QSTR(MP_QSTR_WDT_RESET), MP_ROM_INT(RP2_RESET_WDT) },

// Force QSTRs used by Pin and other peripherals into the embed QSTR scan
static void jl_qstr_refs(void) {
    // Pin QSTRs
    (void)MP_QSTR_Pin; (void)MP_QSTR_low; (void)MP_QSTR_high; (void)MP_QSTR_toggle; (void)MP_QSTR_ALT;
    (void)MP_QSTR_on;  (void)MP_QSTR_off; (void)MP_QSTR_pull; (void)MP_QSTR_alt;
    
    // Peripheral type QSTRs
    (void)MP_QSTR_Timer; (void)MP_QSTR_RTC; (void)MP_QSTR_WDT; (void)MP_QSTR_PWM; (void)MP_QSTR_ADC;
    (void)MP_QSTR_I2C; (void)MP_QSTR_SPI; (void)MP_QSTR_UART;
    
    // ADC QSTRs
    (void)MP_QSTR_CORE_TEMP; (void)MP_QSTR_read_u16;
    
    // PWM QSTRs  
    (void)MP_QSTR_freq; (void)MP_QSTR_duty_u16; (void)MP_QSTR_duty_ns; (void)MP_QSTR_invert;
    
    // Timer/RTC QSTRs
    (void)MP_QSTR_mode; (void)MP_QSTR_callback; (void)MP_QSTR_period; (void)MP_QSTR_tick_hz;
    (void)MP_QSTR_ONE_SHOT; (void)MP_QSTR_PERIODIC; (void)MP_QSTR_datetime;
    
    // WDT QSTRs
    (void)MP_QSTR_timeout; (void)MP_QSTR_feed;
    
    // I2C/SPI QSTRs
    (void)MP_QSTR_scl; (void)MP_QSTR_sda; (void)MP_QSTR_baudrate;
    (void)MP_QSTR_polarity; (void)MP_QSTR_phase; (void)MP_QSTR_bits; (void)MP_QSTR_firstbit;
    (void)MP_QSTR_sck; (void)MP_QSTR_mosi; (void)MP_QSTR_miso;
    (void)MP_QSTR_scan; (void)MP_QSTR_writeto; (void)MP_QSTR_readfrom; (void)MP_QSTR_readfrom_into;
    (void)MP_QSTR_writeto_mem; (void)MP_QSTR_readfrom_mem; (void)MP_QSTR_readfrom_mem_into;
    
    // Common peripheral QSTRs
    (void)MP_QSTR_init; (void)MP_QSTR_deinit; (void)MP_QSTR_value;
    (void)MP_QSTR_bitstream;
    
    // Additional machine module QSTRs
    (void)MP_QSTR_MSB; (void)MP_QSTR_LSB;
    (void)MP_QSTR_unique_id; (void)MP_QSTR_reset; (void)MP_QSTR_reset_cause;
    (void)MP_QSTR_lightsleep; (void)MP_QSTR_deepsleep;
}
