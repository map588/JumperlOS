"""
Basic DAC (Digital-to-Analog Converter) operations.
This example shows how to set DAC voltages.

DAC channels: 0=DAC_0, 1=DAC_1, 2=TOP_RAIL, 3=BOTTOM_RAIL
"""

import time

print("DAC Basics Demo")

# Test DAC channels, we're skipping DAC_0 because it's generally reserved for the probe
channels = [1, 2, 3]
channel_names = ["DAC_1", "TOP_RAIL", "BOTTOM_RAIL"]

oled_set_text_size(0) # Set to 0 for scrolling text

for i, channel in enumerate(channels):
    
    oled_print("\nTesting " + channel_names[i] + " (channel " + str(channel) + "):")
    print("\nTesting " + channel_names[i] + " (channel " + str(channel) + "):")
    
    time.sleep(1)
    
    # Set different voltages
    voltages = [0.0, 1.5, 3.3]
    
    for voltage in voltages:
        
        dac_set(channel, voltage)
        
        actual = dac_get(channel) # confirm the setting
        
        oled_print("\nSet " + str(actual) + "V")
        print("   Set " + str(actual) + "V")
        
        time.sleep(1)
    
    # Reset to 0V
    dac_set(channel, 0.0)

print("\nDAC Basics complete!")

