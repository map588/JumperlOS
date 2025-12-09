"""
Voltage Monitor Demo
Monitor voltage on ADC with real-time OLED display.

Hardware Setup:
1. Connect voltage source to breadboard row 20
2. Voltage range: 0V to 3.3V
"""

import time
import jumperless as j

print("Voltage Monitor Demo")

j.disconnect(12, -1)
j.disconnect(j.ADC0, -1)
j.connect(j.ADC0, 12)
print("ADC0 connected to row 12")
print("Connect voltage source to row 12")

j.oled_print("Voltage Monitor")
time.sleep(1)

while True:
    voltage = j.adc_get(0)
    j.oled_print(str(round(voltage, 3)) + " V")
    print("\r                      ", end="\r")
    print("Voltage: " + str(round(voltage, 3)) + "V", end="")
    time.sleep(0.15)

