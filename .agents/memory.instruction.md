---
applyTo: '**'
---
# Coding Preferences

## MicroPython Port Structure
- Jumperless uses an embedded MicroPython port located in `lib/micropython/port/`
- Configuration file: `lib/micropython/port/mpconfigport.h`
- Machine module extensions: `lib/micropython/port/modmachine_jl.inc`
- Peripheral implementations follow the pattern: `machine_*_jl.c`

## Machine Module Peripheral Implementation Pattern
When adding new peripherals to the machine module:
1. Create implementation file: `lib/micropython/port/machine_<peripheral>_jl.c`
2. Enable in `mpconfigport.h` with `MICROPY_PY_MACHINE_<PERIPHERAL> (1)`
3. Add include file path if needed: `MICROPY_PY_MACHINE_<PERIPHERAL>_INCLUDEFILE`
4. Add type to `modmachine_jl.inc` EXTRA_GLOBALS macro
5. Add QSTRs to `jl_qstr_refs()` function

# Project Architecture

## JumperlOS MicroPython Port
The Jumperless V5 firmware includes a full MicroPython port with the following peripherals:
- **GPIO (Pin)**: Digital I/O with pull up/down/open drain
- **Timer**: Software timers with callbacks
- **RTC**: Real-time clock using AON timer
- **WDT**: Watchdog timer for system reliability
- **PWM**: Hardware PWM on all GPIO pins
- **ADC**: 12-bit ADC on GPIO26-29 + temperature sensor
- **I2C**: Hardware I2C0/I2C1 with flexible pin assignment
- **SPI**: Hardware SPI0/SPI1 with DMA support
- **UART**: Serial communication (already implemented)

## Available GPIOs on Jumperless V5
Safe for use: GPIO 0, 1, 6, 7, 9, 10, 20-27
Use with caution: GPIO 18, 19 (NANO_RESET pins)
Reserved: Other GPIOs used by crosspoint switches, display, etc.

# Solutions Repository

## MicroPython Enhancement (December 2024)
Successfully brought Jumperless MicroPython port to feature parity with standard RP2 port.

**Files Created:**
- `machine_timer_jl.c` - Timer with callbacks
- `machine_rtc_jl.c` - Real-time clock
- `machine_wdt_jl.c` - Watchdog timer
- `machine_pwm_jl.c` - PWM output
- `machine_adc_jl.c` - Analog input
- `machine_i2c_jl.c` - I2C communication
- `machine_spi_jl.c` - SPI communication

**Files Modified:**
- `mpconfigport.h` - Enabled all peripheral features
- `modmachine_jl.inc` - Added machine.reset(), unique_id(), reset_cause(), freq(), bootloader()

**Build System:**
No changes needed - peripherals included via INCLUDEFILE macros in mpconfigport.h

**Testing:**
Comprehensive testing examples provided in MICROPYTHON_FEATURES_ADDED.md
Testing should be performed after building firmware with `pio run`

**Benefits:**
- Full API compatibility with Raspberry Pi Pico
- Access to entire MicroPython ecosystem
- Easy to maintain with upstream changes
- Code portability between boards
