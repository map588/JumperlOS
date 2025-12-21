"""
LED Brightness Control Demo
Touch breadboard pads 1-60 to control LED brightness levels.

Hardware Setup:
1. Connect LED anode to breadboard row 15
2. Connect LED cathode to GND
"""

import jumperless as j
import time

print("LED Brightness Control Demo")
    
j.oled_print("LED Brightness")

print("Hardware Setup:")
print("  Connect LED anode to row 15")
print("  Connect LED cathode to GND")

j.disconnect(j.DAC0, -1)
j.disconnect(15, -1)
j.connect(j.DAC0, 15)

while True:
    pad = j.probe_read(False)

    if pad != j.NO_PAD:

        voltage = (float(pad) / 60.0) * 5.0
        
        j.dac_set(j.DAC0, voltage)
        
        print("\r                      ", end="\r")
        
        print(str(pad) + ": " + str(round(voltage, 1)) + "V", end="")
   
    current_ma = j.get_current(1) * 1000 #current sensor 1 is inline with DAC 0
    
    j.oled_print("Voltage:  " + str(round(voltage, 2)) + " V \n\rCurrent:  " + str(round(current_ma, 2)) + " mA")
            
    time.sleep(0.1)
    
