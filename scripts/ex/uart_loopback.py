"""
UART Loopback Demo
"""

import jumperless as j
from machine import UART
import time
    
uart = UART(0, 115200)
uart.init(115200, 8, None, 1)
j.connect(j.UART_TX, j.D0, 0)
j.connect(j.UART_RX, j.D0, 0)

buffer = "UART looped back!"

print("UART Loopback Demo")
print("Open a serial monitor and connect to the Jumperless's second port and set baud rate to 115200")
print("You should see the message looped back from the UART_RX pin to the UART_TX pin")

while True:
    _=uart.write(buffer)
    readback = uart.read(len(buffer))
    print(readback)
    time.sleep(0.5)

