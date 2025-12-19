"""
Jumperless Python Module
========================

This module provides a Python-friendly interface to the Jumperless hardware.
It re-exports the native C module with all functions and constants.

To use, upload this file to: /python_scripts/lib/jumperless.py

Then import with:
    from jumperless import *
    
Or import specific items:
    from jumperless import connect, DAC0, HIGH, LOW

All functions from the native module are available here.
"""

# Import native C module - all functions are already global in the native module
import jumperless as _native

# Re-export everything from the native module
# This makes IDE autocomplete work better and keeps the module discoverable

# ============================================================================
# DAC Functions
# ============================================================================
dac_set = _native.dac_set
dac_get = _native.dac_get
set_dac = _native.set_dac
get_dac = _native.get_dac

# ============================================================================
# ADC Functions
# ============================================================================
adc_get = _native.adc_get
get_adc = _native.get_adc

# ============================================================================
# INA Current/Power Monitor Functions
# ============================================================================
ina_get_current = _native.ina_get_current
ina_get_voltage = _native.ina_get_voltage
ina_get_bus_voltage = _native.ina_get_bus_voltage
ina_get_power = _native.ina_get_power

# INA Aliases
get_ina_current = _native.get_ina_current
get_ina_voltage = _native.get_ina_voltage
get_ina_bus_voltage = _native.get_ina_bus_voltage
get_ina_power = _native.get_ina_power
get_current = _native.get_current
get_voltage = _native.get_voltage
get_bus_voltage = _native.get_bus_voltage
get_power = _native.get_power

# ============================================================================
# GPIO Functions
# ============================================================================
gpio_set = _native.gpio_set
gpio_get = _native.gpio_get
gpio_set_dir = _native.gpio_set_dir
gpio_get_dir = _native.gpio_get_dir
gpio_set_pull = _native.gpio_set_pull
gpio_get_pull = _native.gpio_get_pull

# GPIO Aliases
set_gpio = _native.set_gpio
get_gpio = _native.get_gpio
set_gpio_dir = _native.set_gpio_dir
get_gpio_dir = _native.get_gpio_dir
set_gpio_pull = _native.set_gpio_pull
get_gpio_pull = _native.get_gpio_pull

# ============================================================================
# PWM Functions
# ============================================================================
pwm = _native.pwm
pwm_set_duty_cycle = _native.pwm_set_duty_cycle
pwm_set_frequency = _native.pwm_set_frequency
pwm_stop = _native.pwm_stop

# PWM Aliases
set_pwm = _native.set_pwm
set_pwm_duty_cycle = _native.set_pwm_duty_cycle
set_pwm_frequency = _native.set_pwm_frequency
stop_pwm = _native.stop_pwm

# ============================================================================
# Waveform Generator Functions
# ============================================================================
wavegen_set_output = _native.wavegen_set_output
wavegen_set_freq = _native.wavegen_set_freq
wavegen_set_wave = _native.wavegen_set_wave
wavegen_set_sweep = _native.wavegen_set_sweep
wavegen_set_amplitude = _native.wavegen_set_amplitude
wavegen_set_offset = _native.wavegen_set_offset
wavegen_start = _native.wavegen_start
wavegen_stop = _native.wavegen_stop
wavegen_get_output = _native.wavegen_get_output
wavegen_get_freq = _native.wavegen_get_freq
wavegen_get_wave = _native.wavegen_get_wave
wavegen_get_amplitude = _native.wavegen_get_amplitude
wavegen_get_offset = _native.wavegen_get_offset
wavegen_is_running = _native.wavegen_is_running

# Wavegen Aliases
set_wavegen_output = _native.set_wavegen_output
set_wavegen_freq = _native.set_wavegen_freq
set_wavegen_wave = _native.set_wavegen_wave
set_wavegen_sweep = _native.set_wavegen_sweep
set_wavegen_amplitude = _native.set_wavegen_amplitude
set_wavegen_offset = _native.set_wavegen_offset
start_wavegen = _native.start_wavegen
stop_wavegen = _native.stop_wavegen
get_wavegen_output = _native.get_wavegen_output
get_wavegen_freq = _native.get_wavegen_freq
get_wavegen_wave = _native.get_wavegen_wave
get_wavegen_amplitude = _native.get_wavegen_amplitude
get_wavegen_offset = _native.get_wavegen_offset

# Waveform constants
SINE = _native.SINE
TRIANGLE = _native.TRIANGLE
SAWTOOTH = _native.SAWTOOTH
SQUARE = _native.SQUARE
RAMP = _native.RAMP
ARBITRARY = _native.ARBITRARY

# ============================================================================
# Node Connection Functions
# ============================================================================
node = _native.node
connect = _native.connect
disconnect = _native.disconnect
fast_connect = _native.fast_connect
fast_disconnect = _native.fast_disconnect
nodes_clear = _native.nodes_clear
is_connected = _native.is_connected
nodes_save = _native.nodes_save
nodes_has_changes = _native.nodes_has_changes

# ============================================================================
# Net Information Functions
# ============================================================================
get_net_name = _native.get_net_name
set_net_name = _native.set_net_name
get_net_color = _native.get_net_color
get_net_color_name = _native.get_net_color_name
set_net_color = _native.set_net_color
set_net_color_hsv = _native.set_net_color_hsv
get_num_nets = _native.get_num_nets
get_num_bridges = _native.get_num_bridges
get_net_nodes = _native.get_net_nodes
get_bridge = _native.get_bridge
get_net_info = _native.get_net_info

# Net Info Aliases
net_name = _native.net_name
net_color = _native.net_color
net_info = _native.net_info

# ============================================================================
# Path Query Functions
# ============================================================================
get_num_paths = _native.get_num_paths
get_path_info = _native.get_path_info
get_all_paths = _native.get_all_paths
get_path_between = _native.get_path_between

# ============================================================================
# Slot Management
# ============================================================================
switch_slot = _native.switch_slot
CURRENT_SLOT = _native.CURRENT_SLOT

# ============================================================================
# Context Control
# ============================================================================
context_toggle = _native.context_toggle
context_get = _native.context_get

# ============================================================================
# OLED Display Functions
# ============================================================================
oled_print = _native.oled_print
oled_clear = _native.oled_clear
oled_show = _native.oled_show
oled_connect = _native.oled_connect
oled_disconnect = _native.oled_disconnect

# OLED Text Size Control
oled_set_text_size = _native.oled_set_text_size
oled_get_text_size = _native.oled_get_text_size

# OLED Print Redirection
oled_copy_print = _native.oled_copy_print

# OLED Font System
oled_get_fonts = _native.oled_get_fonts
oled_set_font = _native.oled_set_font
oled_get_current_font = _native.oled_get_current_font

# OLED Bitmap Functions
oled_load_bitmap = _native.oled_load_bitmap
oled_display_bitmap = _native.oled_display_bitmap
oled_show_bitmap_file = _native.oled_show_bitmap_file

# OLED Framebuffer Access
oled_get_framebuffer = _native.oled_get_framebuffer
oled_set_framebuffer = _native.oled_set_framebuffer
oled_get_framebuffer_size = _native.oled_get_framebuffer_size
oled_set_pixel = _native.oled_set_pixel
oled_get_pixel = _native.oled_get_pixel

# ============================================================================
# Probe Functions
# ============================================================================
probe_read = _native.probe_read
read_probe = _native.read_probe
probe_read_blocking = _native.probe_read_blocking
probe_read_nonblocking = _native.probe_read_nonblocking
probe_wait = _native.probe_wait
wait_probe = _native.wait_probe
probe_touch = _native.probe_touch
wait_touch = _native.wait_touch
probe_tap = _native.probe_tap

# ============================================================================
# Probe Button Functions
# ============================================================================
get_button = _native.get_button
probe_button = _native.probe_button
probe_button_blocking = _native.probe_button_blocking
probe_button_nonblocking = _native.probe_button_nonblocking
button_read = _native.button_read
read_button = _native.read_button
check_button = _native.check_button
button_check = _native.button_check

# ============================================================================
# Probe Switch Functions
# ============================================================================
get_switch_position = _native.get_switch_position
set_switch_position = _native.set_switch_position
check_switch_position = _native.check_switch_position

# Probe Switch Constants
SWITCH_MEASURE = _native.SWITCH_MEASURE
SWITCH_SELECT = _native.SWITCH_SELECT
SWITCH_UNKNOWN = _native.SWITCH_UNKNOWN

# ============================================================================
# Status/Debug Functions
# ============================================================================
print_bridges = _native.print_bridges
print_paths = _native.print_paths
print_crossbars = _native.print_crossbars
print_nets = _native.print_nets
print_chip_status = _native.print_chip_status

# ============================================================================
# Miscellaneous Functions
# ============================================================================
arduino_reset = _native.arduino_reset
pause_core2 = _native.pause_core2
run_app = _native.run_app
send_raw = _native.send_raw
change_terminal_color = _native.change_terminal_color
cycle_term_color = _native.cycle_term_color

# ============================================================================
# Service Management Functions
# ============================================================================
force_service = _native.force_service
force_service_by_index = _native.force_service_by_index
get_service_index = _native.get_service_index

# ============================================================================
# Help Functions
# ============================================================================
help = _native.help
nodes_help = _native.nodes_help

# ============================================================================
# GPIO State Constants
# ============================================================================
HIGH = _native.HIGH
LOW = _native.LOW
FLOATING = _native.FLOATING

# ============================================================================
# GPIO Direction Constants
# ============================================================================
INPUT = _native.INPUT
OUTPUT = _native.OUTPUT

# ============================================================================
# Node Constants - Power Rails
# ============================================================================
TOP_RAIL = _native.TOP_RAIL
T_RAIL = _native.T_RAIL
BOTTOM_RAIL = _native.BOTTOM_RAIL
BOT_RAIL = _native.BOT_RAIL
B_RAIL = _native.B_RAIL
GND = _native.GND

# ============================================================================
# Node Constants - DACs
# ============================================================================
DAC0 = _native.DAC0
DAC_0 = _native.DAC_0
DAC1 = _native.DAC1
DAC_1 = _native.DAC_1

# ============================================================================
# Node Constants - ADCs
# ============================================================================
ADC0 = _native.ADC0
ADC1 = _native.ADC1
ADC2 = _native.ADC2
ADC3 = _native.ADC3
ADC4 = _native.ADC4
ADC7 = _native.ADC7

# ============================================================================
# Node Constants - Current Sense
# ============================================================================
ISENSE_PLUS = _native.ISENSE_PLUS
ISENSE_P = _native.ISENSE_P
I_P = _native.I_P
CURRENT_SENSE_P = _native.CURRENT_SENSE_P
CURRENT_SENSE_PLUS = _native.CURRENT_SENSE_PLUS
ISENSE_MINUS = _native.ISENSE_MINUS
ISENSE_N = _native.ISENSE_N
I_N = _native.I_N
CURRENT_SENSE_N = _native.CURRENT_SENSE_N
CURRENT_SENSE_MINUS = _native.CURRENT_SENSE_MINUS

# ============================================================================
# Node Constants - Buffer
# ============================================================================
BUFFER_IN = _native.BUFFER_IN
BUF_IN = _native.BUF_IN
BUFFER_OUT = _native.BUFFER_OUT
BUF_OUT = _native.BUF_OUT

# ============================================================================
# Node Constants - UART
# ============================================================================
UART_TX = _native.UART_TX
TX = _native.TX
UART_RX = _native.UART_RX
RX = _native.RX

# ============================================================================
# Node Constants - Arduino Nano Digital Pins
# ============================================================================
D0 = _native.D0
D1 = _native.D1
D2 = _native.D2
D3 = _native.D3
D4 = _native.D4
D5 = _native.D5
D6 = _native.D6
D7 = _native.D7
D8 = _native.D8
D9 = _native.D9
D10 = _native.D10
D11 = _native.D11
D12 = _native.D12
D13 = _native.D13

# NANO prefixed digital pins
NANO_D0 = _native.NANO_D0
NANO_D1 = _native.NANO_D1
NANO_D2 = _native.NANO_D2
NANO_D3 = _native.NANO_D3
NANO_D4 = _native.NANO_D4
NANO_D5 = _native.NANO_D5
NANO_D6 = _native.NANO_D6
NANO_D7 = _native.NANO_D7
NANO_D8 = _native.NANO_D8
NANO_D9 = _native.NANO_D9
NANO_D10 = _native.NANO_D10
NANO_D11 = _native.NANO_D11
NANO_D12 = _native.NANO_D12
NANO_D13 = _native.NANO_D13

# ============================================================================
# Node Constants - Arduino Nano Analog Pins
# ============================================================================
A0 = _native.A0
A1 = _native.A1
A2 = _native.A2
A3 = _native.A3
A4 = _native.A4
A5 = _native.A5
A6 = _native.A6
A7 = _native.A7

# NANO prefixed analog pins
NANO_A0 = _native.NANO_A0
NANO_A1 = _native.NANO_A1
NANO_A2 = _native.NANO_A2
NANO_A3 = _native.NANO_A3
NANO_A4 = _native.NANO_A4
NANO_A5 = _native.NANO_A5
NANO_A6 = _native.NANO_A6
NANO_A7 = _native.NANO_A7

# ============================================================================
# Node Constants - GPIO Pins (multiple aliases)
# ============================================================================
GPIO_1 = _native.GPIO_1
GPIO_2 = _native.GPIO_2
GPIO_3 = _native.GPIO_3
GPIO_4 = _native.GPIO_4
GPIO_5 = _native.GPIO_5
GPIO_6 = _native.GPIO_6
GPIO_7 = _native.GPIO_7
GPIO_8 = _native.GPIO_8
GP1 = _native.GP1
GP2 = _native.GP2
GP3 = _native.GP3
GP4 = _native.GP4
GP5 = _native.GP5
GP6 = _native.GP6
GP7 = _native.GP7
GP8 = _native.GP8

# GPIO RP2350 physical pin aliases
GPIO_20 = _native.GPIO_20
GPIO_21 = _native.GPIO_21
GPIO_22 = _native.GPIO_22
GPIO_23 = _native.GPIO_23
GPIO_24 = _native.GPIO_24
GPIO_25 = _native.GPIO_25
GPIO_26 = _native.GPIO_26
GPIO_27 = _native.GPIO_27

# ============================================================================
# Probe Button Constants
# ============================================================================
BUTTON_NONE = _native.BUTTON_NONE
BUTTON_CONNECT = _native.BUTTON_CONNECT
BUTTON_REMOVE = _native.BUTTON_REMOVE
CONNECT_BUTTON = _native.CONNECT_BUTTON
REMOVE_BUTTON = _native.REMOVE_BUTTON

# ============================================================================
# Probe Pad Constants
# ============================================================================
NO_PAD = _native.NO_PAD
LOGO_PAD_TOP = _native.LOGO_PAD_TOP
LOGO_PAD_BOTTOM = _native.LOGO_PAD_BOTTOM
GPIO_PAD = _native.GPIO_PAD
DAC_PAD = _native.DAC_PAD
ADC_PAD = _native.ADC_PAD
BUILDING_PAD_TOP = _native.BUILDING_PAD_TOP
BUILDING_PAD_BOTTOM = _native.BUILDING_PAD_BOTTOM

# Nano power/control pad constants
NANO_VIN = _native.NANO_VIN
VIN_PAD = _native.VIN_PAD
NANO_RESET_0 = _native.NANO_RESET_0
RESET_0_PAD = _native.RESET_0_PAD
NANO_RESET_1 = _native.NANO_RESET_1
RESET_1_PAD = _native.RESET_1_PAD
NANO_GND_1 = _native.NANO_GND_1
GND_1_PAD = _native.GND_1_PAD
NANO_GND_0 = _native.NANO_GND_0
GND_0_PAD = _native.GND_0_PAD
NANO_3V3 = _native.NANO_3V3
VIN_3V3_PAD = _native.NANO_3V3
NANO_5V = _native.NANO_5V
VIN_5V_PAD = _native.NANO_5V

# Nano digital pin pad constants
D0_PAD = _native.D0_PAD
D1_PAD = _native.D1_PAD
D2_PAD = _native.D2_PAD
D3_PAD = _native.D3_PAD
D4_PAD = _native.D4_PAD
D5_PAD = _native.D5_PAD
D6_PAD = _native.D6_PAD
D7_PAD = _native.D7_PAD
D8_PAD = _native.D8_PAD
D9_PAD = _native.D9_PAD
D10_PAD = _native.D10_PAD
D11_PAD = _native.D11_PAD
D12_PAD = _native.D12_PAD
D13_PAD = _native.D13_PAD
RESET_PAD = _native.RESET_PAD
AREF_PAD = _native.AREF_PAD

# Nano analog pin pad constants
A0_PAD = _native.A0_PAD
A1_PAD = _native.A1_PAD
A2_PAD = _native.A2_PAD
A3_PAD = _native.A3_PAD
A4_PAD = _native.A4_PAD
A5_PAD = _native.A5_PAD
A6_PAD = _native.A6_PAD
A7_PAD = _native.A7_PAD

# Rail pad constants
TOP_RAIL_PAD = _native.TOP_RAIL_PAD
BOTTOM_RAIL_PAD = _native.BOTTOM_RAIL_PAD
BOT_RAIL_PAD = _native.BOT_RAIL_PAD
TOP_RAIL_GND = _native.TOP_RAIL_GND
TOP_GND_PAD = _native.TOP_GND_PAD
BOTTOM_RAIL_GND = _native.BOTTOM_RAIL_GND
BOT_RAIL_GND = _native.BOT_RAIL_GND
BOTTOM_GND_PAD = _native.BOTTOM_GND_PAD
BOT_GND_PAD = _native.BOT_GND_PAD

# ============================================================================
# Clickwheel Functions
# ============================================================================
clickwheel_up = _native.clickwheel_up
clickwheel_down = _native.clickwheel_down
clickwheel_press = _native.clickwheel_press
clickwheel_get_position = _native.clickwheel_get_position
clickwheel_reset_position = _native.clickwheel_reset_position
clickwheel_get_direction = _native.clickwheel_get_direction
clickwheel_get_button = _native.clickwheel_get_button
clickwheel_is_initialized = _native.clickwheel_is_initialized

# Clickwheel Constants
CLICKWHEEL_NONE = _native.CLICKWHEEL_NONE
CLICKWHEEL_UP = _native.CLICKWHEEL_UP
CLICKWHEEL_DOWN = _native.CLICKWHEEL_DOWN
CLICKWHEEL_IDLE = _native.CLICKWHEEL_IDLE
CLICKWHEEL_PRESSED = _native.CLICKWHEEL_PRESSED
CLICKWHEEL_HELD = _native.CLICKWHEEL_HELD
CLICKWHEEL_RELEASED = _native.CLICKWHEEL_RELEASED
CLICKWHEEL_DOUBLECLICKED = _native.CLICKWHEEL_DOUBLECLICKED

# ============================================================================
# Filesystem Functions
# ============================================================================
fs_exists = _native.fs_exists
fs_listdir = _native.fs_listdir
fs_read = _native.fs_read
fs_write = _native.fs_write
fs_cwd = _native.fs_cwd

# ============================================================================
# JFS Filesystem Module
# ============================================================================
jfs = _native.jfs


# Export all functions and constants for "from jumperless import *"
__all__ = [
    # DAC Functions
    'dac_set', 'dac_get', 'set_dac', 'get_dac',
    
    # ADC Functions
    'adc_get', 'get_adc',
    
    # INA Functions
    'ina_get_current', 'ina_get_voltage', 'ina_get_bus_voltage', 'ina_get_power',
    'get_ina_current', 'get_ina_voltage', 'get_ina_bus_voltage', 'get_ina_power',
    'get_current', 'get_voltage', 'get_bus_voltage', 'get_power',
    
    # GPIO Functions
    'gpio_set', 'gpio_get', 'gpio_set_dir', 'gpio_get_dir', 'gpio_set_pull', 'gpio_get_pull',
    'set_gpio', 'get_gpio', 'set_gpio_dir', 'get_gpio_dir', 'set_gpio_pull', 'get_gpio_pull',
    
    # PWM Functions
    'pwm', 'pwm_set_duty_cycle', 'pwm_set_frequency', 'pwm_stop',
    'set_pwm', 'set_pwm_duty_cycle', 'set_pwm_frequency', 'stop_pwm',
    
    # Wavegen Functions
    'wavegen_set_output', 'wavegen_set_freq', 'wavegen_set_wave', 'wavegen_set_sweep',
    'wavegen_set_amplitude', 'wavegen_set_offset', 'wavegen_start', 'wavegen_stop',
    'wavegen_get_output', 'wavegen_get_freq', 'wavegen_get_wave',
    'wavegen_get_amplitude', 'wavegen_get_offset', 'wavegen_is_running',
    'set_wavegen_output', 'set_wavegen_freq', 'set_wavegen_wave', 'set_wavegen_sweep',
    'set_wavegen_amplitude', 'set_wavegen_offset', 'start_wavegen', 'stop_wavegen',
    'get_wavegen_output', 'get_wavegen_freq', 'get_wavegen_wave',
    'get_wavegen_amplitude', 'get_wavegen_offset',
    
    # Node Connection Functions
    'node', 'connect', 'disconnect', 'fast_connect', 'fast_disconnect', 'nodes_clear', 'is_connected',
    'nodes_save', 'nodes_discard', 'nodes_has_changes',
    
    # Net Information Functions
    'get_net_name', 'set_net_name', 'get_net_color', 'get_net_color_name', 'set_net_color', 'set_net_color_hsv',
    'get_num_nets', 'get_num_bridges', 'get_net_nodes', 'get_bridge', 'get_net_info',
    'net_name', 'net_color', 'net_info',
    
    # Path Query Functions
    'get_num_paths', 'get_path_info', 'get_all_paths', 'get_path_between',
    
    # Slot Management
    'switch_slot', 'CURRENT_SLOT',
    
    # Context Control
    'context_toggle', 'context_get',
    
    # OLED Functions
    'oled_print', 'oled_clear', 'oled_show', 'oled_connect', 'oled_disconnect',
    
    # Probe Functions
    'probe_read', 'read_probe', 'probe_read_blocking', 'probe_read_nonblocking',
    'probe_wait', 'wait_probe', 'probe_touch', 'wait_touch', 'probe_tap',
    
    # Probe Button Functions
    'get_button', 'probe_button', 'probe_button_blocking', 'probe_button_nonblocking',
    'button_read', 'read_button', 'check_button', 'button_check',
    
    # Probe Switch Functions
    'get_switch_position', 'set_switch_position', 'check_switch_position',
    
    # Status Functions
    'print_bridges', 'print_paths', 'print_crossbars', 'print_nets', 'print_chip_status',
    
    # Clickwheel Functions
    'clickwheel_up', 'clickwheel_down', 'clickwheel_press',
    'clickwheel_get_position', 'clickwheel_reset_position', 'clickwheel_get_direction',
    'clickwheel_get_button', 'clickwheel_is_initialized',
    
    # Logic Analyzer Functions
    'la_set_trigger', 'la_capture_single_sample', 'la_start_continuous_capture',
    'la_stop_capture', 'la_is_capturing', 'la_set_sample_rate', 'la_set_num_samples',
    'la_enable_channel', 'la_set_control_analog', 'la_set_control_digital',
    'la_get_control_analog', 'la_get_control_digital',
    
    # Filesystem Functions
    'fs_exists', 'fs_listdir', 'fs_read', 'fs_write', 'fs_cwd',
    
    # Misc Functions
    'arduino_reset', 'pause_core2', 'run_app', 'send_raw',
    'change_terminal_color', 'cycle_term_color',
    
    # Service Management Functions
    'force_service', 'force_service_by_index', 'get_service_index',
    
    # Help Functions
    'help', 'nodes_help',
    
    # GPIO State Constants
    'HIGH', 'LOW', 'FLOATING',
    
    # GPIO Direction Constants
    'INPUT', 'OUTPUT',
    
    # Power Rail Constants
    'TOP_RAIL', 'T_RAIL', 'BOTTOM_RAIL', 'BOT_RAIL', 'B_RAIL', 'GND',
    
    # DAC Constants
    'DAC0', 'DAC_0', 'DAC1', 'DAC_1',
    
    # ADC Constants
    'ADC0', 'ADC1', 'ADC2', 'ADC3', 'ADC4', 'ADC7',
    
    # Current Sense Constants
    'ISENSE_PLUS', 'ISENSE_P', 'I_P', 'CURRENT_SENSE_P', 'CURRENT_SENSE_PLUS',
    'ISENSE_MINUS', 'ISENSE_N', 'I_N', 'CURRENT_SENSE_N', 'CURRENT_SENSE_MINUS',
    
    # Buffer Constants
    'BUFFER_IN', 'BUF_IN', 'BUFFER_OUT', 'BUF_OUT',
    
    # UART Constants
    'UART_TX', 'TX', 'UART_RX', 'RX',
    
    # Arduino Digital Pins
    'D0', 'D1', 'D2', 'D3', 'D4', 'D5', 'D6', 'D7', 'D8', 'D9', 'D10', 'D11', 'D12', 'D13',
    'NANO_D0', 'NANO_D1', 'NANO_D2', 'NANO_D3', 'NANO_D4', 'NANO_D5', 'NANO_D6', 'NANO_D7',
    'NANO_D8', 'NANO_D9', 'NANO_D10', 'NANO_D11', 'NANO_D12', 'NANO_D13',
    
    # Arduino Analog Pins
    'A0', 'A1', 'A2', 'A3', 'A4', 'A5', 'A6', 'A7',
    'NANO_A0', 'NANO_A1', 'NANO_A2', 'NANO_A3', 'NANO_A4', 'NANO_A5', 'NANO_A6', 'NANO_A7',
    
    # GPIO Pin Aliases
    'GPIO_1', 'GPIO_2', 'GPIO_3', 'GPIO_4', 'GPIO_5', 'GPIO_6', 'GPIO_7', 'GPIO_8',
    'GP1', 'GP2', 'GP3', 'GP4', 'GP5', 'GP6', 'GP7', 'GP8',
    'GPIO_20', 'GPIO_21', 'GPIO_22', 'GPIO_23', 'GPIO_24', 'GPIO_25', 'GPIO_26', 'GPIO_27',
    
    # Waveform Constants
    'SINE', 'TRIANGLE', 'SAWTOOTH', 'SQUARE', 'RAMP', 'ARBITRARY',
    
    # Probe Button Constants
    'BUTTON_NONE', 'BUTTON_CONNECT', 'BUTTON_REMOVE', 'CONNECT_BUTTON', 'REMOVE_BUTTON',
    
    # Probe Switch Constants
    'SWITCH_MEASURE', 'SWITCH_SELECT', 'SWITCH_UNKNOWN',
    
    # Clickwheel Constants
    'CLICKWHEEL_NONE', 'CLICKWHEEL_UP', 'CLICKWHEEL_DOWN', 
    'CLICKWHEEL_IDLE', 'CLICKWHEEL_PRESSED', 'CLICKWHEEL_HELD', 
    'CLICKWHEEL_RELEASED', 'CLICKWHEEL_DOUBLECLICKED',
    
    # Probe Pad Constants
    'NO_PAD', 'LOGO_PAD_TOP', 'LOGO_PAD_BOTTOM', 'GPIO_PAD', 'DAC_PAD', 'ADC_PAD',
    'BUILDING_PAD_TOP', 'BUILDING_PAD_BOTTOM',
    'NANO_VIN', 'VIN_PAD', 'NANO_RESET_0', 'RESET_0_PAD', 'NANO_RESET_1', 'RESET_1_PAD',
    'NANO_GND_1', 'GND_1_PAD', 'NANO_GND_0', 'GND_0_PAD', 'NANO_3V3', 'VIN_3V3_PAD', 'NANO_5V', 'VIN_5V_PAD',
    'D0_PAD', 'D1_PAD', 'D2_PAD', 'D3_PAD', 'D4_PAD', 'D5_PAD', 'D6_PAD', 'D7_PAD',
    'D8_PAD', 'D9_PAD', 'D10_PAD', 'D11_PAD', 'D12_PAD', 'D13_PAD', 'RESET_PAD', 'AREF_PAD',
    'A0_PAD', 'A1_PAD', 'A2_PAD', 'A3_PAD', 'A4_PAD', 'A5_PAD', 'A6_PAD', 'A7_PAD',
    'TOP_RAIL_PAD', 'BOTTOM_RAIL_PAD', 'BOT_RAIL_PAD',
    'TOP_RAIL_GND', 'TOP_GND_PAD', 'BOTTOM_RAIL_GND', 'BOT_RAIL_GND', 'BOTTOM_GND_PAD', 'BOT_GND_PAD',
    
    # JFS Module
    'jfs',
]

