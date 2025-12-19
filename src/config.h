#ifndef CONFIG_H
#define CONFIG_H

//Jumperless config
#include "JumperlessDefines.h"


// extern int hwRevision;
// extern int probeRevision;


extern struct config jumperlessConfig;
// Forward declarations of nested structs
struct firmware;
struct hardware;
struct dacs;
struct debug;
struct routing;
struct calibration;
struct logo_pads;
struct display_settings;
struct serial_1;
struct serial_2;
struct top_oled;

struct config {
    struct firmware {
        char last_version[16] = "";  // Last firmware version that was run
        bool files_provisioned = false;  // Whether files have been provisioned for this version
    } firmware;

    struct hardware {
        int generation = 5;
        int revision = 5;
        int probe_revision = 5;
    } hardware;

    struct dacs {
        // Voltage values moved to activeState.power (topRail, bottomRail, dac0, dac1)
        bool set_dacs_on_boot = false;
        bool set_rails_on_boot = true;
        int probe_power_dac = 0;
        float limit_max = 8.00;
        float limit_min = -8.00;
    } dacs;

    struct debug {
        bool file_parsing = false;
        bool net_manager = false;
        bool nets_to_chips = false;
        bool nets_to_chips_alt = false;
        bool leds = false;
        bool probing = false;
        bool oled = false;
        bool logo_pads = false;
        bool logic_analyzer = true; 
        int  arduino = 0;
        bool  usb_mass_storage = false;
    } debug;

    struct routing {
        int stack_paths = 2;
        int stack_rails = 3;
        int stack_dacs = 0;
        int rail_priority = 1;
    } routing;

    struct calibration {
        int top_rail_zero = 1650;
        float top_rail_spread = 21.5;
        int bottom_rail_zero = 1650;
        float bottom_rail_spread = 21.5;
        int dac_0_zero = 1650;
        float dac_0_spread = 21.5;
        int dac_1_zero = 1650;
        float dac_1_spread = 21.5;
        float adc_0_zero = 9.0;
        float adc_0_spread = 18.28;
        float adc_1_zero = 9.0;
        float adc_1_spread = 18.28;
        float adc_2_zero = 9.0;
        float adc_2_spread = 18.28;
        float adc_3_zero = 9.0;
        float adc_3_spread = 18.28;
        float adc_4_zero = 0.0;
        float adc_4_spread = 5.0;
        float adc_7_zero = 9.0;
        float adc_7_spread = 18.28;
        int probe_max = 4055;
        int probe_min = 10;
        // Hysteresis thresholds to prevent oscillation between modes
        // Switch to SELECT mode when current > high threshold
        // Switch to MEASURE mode when current < low threshold
        float probe_switch_threshold_high = 0.65;  // mA - switch to SELECT
        float probe_switch_threshold_low = 0.50;   // mA - switch to MEASURE
        float probe_switch_threshold = 0.40;       // DEPRECATED - kept for backward compatibility
        float measure_mode_output_voltage = 3.30;
        float probe_current_zero = 2.0;
    } calibration;

    struct logo_pads {
        int top_guy = 0; // 0 = uart tx, 1 = uart rx, others as I think of them
        int bottom_guy = 1;
        int building_pad_top = 25;   // default to current sense +
        int building_pad_bottom = 26; // default to current sense -
        int repeat_ms = 100;
    } logo_pads;

    struct display {
        volatile int lines_wires = 1;
        int menu_brightness = -10;
        int led_brightness = 10;
        int rail_brightness = 55;
        int special_net_brightness = 20;
        int net_color_mode = 0;
        int dump_leds = -1;
        int dump_format = 0;
        int terminal_line_buffering = 0;
    } display;

    
        struct serial_1 {
            int function = 1; 
            int baud_rate = 115200;
            int print_passthrough = 0;
            int connect_on_boot = 0;
            int lock_connection = 0;
            int autoconnect_flashing = 1;
            bool async_passthrough = true;
            int tag_parsing = 1; // 0 = disabled, 1 = passthrough + tag parsing, 2 = parsing + strip tags
        } serial_1;

        
        struct serial_2 {
            int function = 3; // 0 = off 3 = USB MSC enabled
            int baud_rate = 115200;
            int print_passthrough = 0;
            int connect_on_boot = 0;
            int lock_connection = 0;
            int autoconnect_flashing = 0;
        } serial_2;

        struct top_oled {
            int enabled = 0;
            int i2c_address = 0x3C;
            const char* display_type = "SSD1306";
           
            int width = 128;
            int height = 32;
            int rotation = 0; // 0 = 0 degrees, 1 = 90 degrees, 2 = 180 degrees, 3 = 270 degrees

            int connection_type = 0; // 0 = GPIO 7/8, 1 = RP6/RP7, 2 = internal I2C0, 3 = custom (use sda_pin and scl_pin to set the pins)
            int sda_pin = 26;//the actual hardware pin number
            int scl_pin = 27;//the actual hardware pin number
            int gpio_sda = RP_GPIO_26; //the define number for the hardware pin
            int gpio_scl = RP_GPIO_27; //the define number for the hardware pin
            int sda_row = NANO_D2; //the row number
            int scl_row = NANO_D3; //the row number
            int connect_on_boot = 0;
            int lock_connection = 0;
            int autoconnect_check_interval = -1;
            int font = 0;
            int show_in_terminal = 0;
            char startup_message[33] = ""; // Startup message for OLED (max 32 chars + null terminator)
        } top_oled;
    
};

#endif // CONFIG_H

/*
 if (EEPROM.read(FIRSTSTARTUPADDRESS) != 0xAA || forceDefaults == 1) {
   EEPROM.write(FIRSTSTARTUPADDRESS, 0xAA);
   
   EEPROM.write(REVISIONADDRESS, REV);

   EEPROM.write(DEBUG_FILEPARSINGADDRESS, 0);
   EEPROM.write(TIME_FILEPARSINGADDRESS, 0);
   EEPROM.write(DEBUG_NETMANAGERADDRESS, 0);
   EEPROM.write(TIME_NETMANAGERADDRESS, 0);
   EEPROM.write(DEBUG_NETTOCHIPCONNECTIONSADDRESS, 0);
   EEPROM.write(DEBUG_NETTOCHIPCONNECTIONSALTADDRESS, 0);
   EEPROM.write(DEBUG_LEDSADDRESS, 0);
   EEPROM.write(LEDBRIGHTNESSADDRESS, DEFAULTBRIGHTNESS);
   EEPROM.write(RAILBRIGHTNESSADDRESS, DEFAULTRAILBRIGHTNESS);
   EEPROM.write(SPECIALBRIGHTNESSADDRESS, DEFAULTSPECIALNETBRIGHTNESS);
   EEPROM.write(PROBESWAPADDRESS, 0);
   EEPROM.write(ROTARYENCODER_MODE_ADDRESS, 0);
   EEPROM.write(DISPLAYMODE_ADDRESS, 1);
   EEPROM.write(NETCOLORMODE_ADDRESS, 0);
   EEPROM.write(MENUBRIGHTNESS_ADDRESS, 100);
   EEPROM.write(PATH_DUPLICATE_ADDRESS, 2);
   EEPROM.write(DAC_DUPLICATE_ADDRESS, 0);
   EEPROM.write(POWER_DUPLICATE_ADDRESS, 2);
   EEPROM.write(DAC_PRIORITY_ADDRESS, 1);
   EEPROM.write(POWER_PRIORITY_ADDRESS, 1);
   EEPROM.write(SHOW_PROBE_CURRENT_ADDRESS, 0);

   saveVoltages(0.0f, 0.0f, 3.33f, 0.0f);

*/