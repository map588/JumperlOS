"""
Basic UART operations.
This example shows how to use UART.
"""

import jumperless as j

from machine import UART
    
uart = UART(0, 115200)
uart.init(115200, 8, None, 1)

j.connect(UART_TX, D0)
j.connect(UART_RX, D1)

print("UART Basics Demo")
print("This example will send a message to the Arduino Nano over UART")

time.sleep(1)

buffer = "Sup Arduino"
while True:
    j.uart.write(buffer)
    j.time.sleep(0.5)
