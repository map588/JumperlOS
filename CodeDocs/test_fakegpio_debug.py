"""Simple test to see if FAKE_GPIO bridges are being created"""
import serial
import time

# Find the Jumperless
port = "/dev/cu.usbmodemJLV5port1"
ser = serial.Serial(port, 115200, timeout=1)
time.sleep(0.5)

# Clear and enable debug
print("Clearing connections...")
ser.write(b"nc\n")
time.sleep(0.2)

print("\nConfiguring first FAKE_GPIO pin...")
ser.write(b"mp\n")
time.sleep(0.1)
ser.write(b"import jumperless as j\n")
time.sleep(0.1)
ser.write(b"j.nodes_clear()\n")
time.sleep(0.2)

# Configure just one pin
ser.write(b"pin = j.FakeGpioPin(8, j.INPUT, 2.0, 0.8)\n")
time.sleep(0.5)

# Check paths
ser.write(b"\x03")  # Ctrl+C to exit Python
time.sleep(0.2)
ser.write(b"p\n")  # Print paths
time.sleep(0.5)

# Read all output
output = ser.read_all().decode('utf-8', errors='ignore')
print(output)

ser.close()
