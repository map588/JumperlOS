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

## Probe Cable / Probe LED / Probe Button Pin Sharing
- `PROBE_LED_PIN` (GPIO 2) carries the WS2812 data line for the single probe LED.
- `BUTTON_PIN` (GPIO 9) is the dedicated software button-read pin.
- On the current TRRS probe cable, GPIO 2 and GPIO 9 are **shorted together** at the
  jack and behave as a single shared net. Anything reading/driving either pin
  affects the other. The probe-button reader currently exploits this: it drives
  the WS2812 line and samples GPIO 9 (or GPIO 2 — they're the same node) to
  discriminate connect / disconnect / floating.
- A **TRRRS** probe cable (5 conductors instead of 4) separates these into two
  independent conductors. In that hardware configuration GPIO 2 is LED-data-only
  and GPIO 9 is button-only; the multiplex constraint disappears and the button
  PIO could sample GPIO 9 continuously without any LED-show coordination.
- Code that touches the probe-LED PIO needs to be tolerant of both topologies.
  Treat the shared-line case as the conservative default and gate any
  TRRRS-only optimizations on a runtime/config check.

## Config Manager - Adding New Config Options

When adding a new config option to JumperlOS, you must update ALL of these sections in `src/configManager.cpp`:

1. **Line ~588** - `loadConfig()` parsing section - parse the new option from file
2. **Line ~926** - `saveConfig()` writing section - write the new option to file
3. **Line ~1098** - `hasConfigChanged()` - compare the new option for change detection
4. **Line ~1459** - `updateConfigInPlace()` - handle in-place config file updates
5. **Line ~2263** - `printConfig()` sections - print the new option to serial
6. **Line ~2289** - `printConfig()` options within sections
7. **Line ~3315** - `updateConfigValue()` GET old value section (for "changed from X to Y" message)
8. **Line ~3450** - `updateConfigValue()` SET new value section (actually applies the change!)

Also update:
- `src/config.h` - Add the option to the appropriate struct with default value
- `resetConfigToDefaults()` in configManager.cpp - Save/restore if it should persist through resets

Look for the `//! this is a place to add new config options` comments to find the right locations.

**IMPORTANT**: `updateConfigValue()` has TWO sections - one for getting the old value (for display) and one for setting the new value. Both must be updated!

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
