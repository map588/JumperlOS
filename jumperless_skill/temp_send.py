import serial
import time

PORT = '/dev/cu.usbmodemJLV5port3'
BAUD = 115200

commands = r'''
# 555 astable on the middle of the breadboard
connect(TOP_RAIL, 48)      # VCC -> pin 8
connect(TOP_RAIL, 18)      # VCC -> RESET (pin 4)
connect(GND, 15)           # GND -> pin 1
connect(48, 47)
connect(47, 46)
connect(46, 15)
connect(16, 46)
connect(45, 15)
connect(17, ADC0)
v = adc_get(0)
print("555 output (pin 3) voltage:", v)
'''

print("This script would normally open", PORT)
for line in commands.strip().splitlines():
    print("CMD>", line)
    time.sleep(0.05)

try:
    with serial.Serial(PORT, BAUD, timeout=1) as ser:
        print("Opened port", PORT)
except Exception as e:
    print("Could not open port (expected in this environment):", e)
