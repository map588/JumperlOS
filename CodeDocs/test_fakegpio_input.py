"""
Multi-Pin Fake GPIO INPUT Test

Creates 12 INPUT pins on breadboard rows 8-19.
Continuously reads and displays all pin states.

Thresholds:
  > 2.0V = HIGH
  < 0.8V = LOW
"""

import jumperless as j
import time

print("="*70)
print("FAKE GPIO INPUT TEST - 12 PINS")
print("="*70)

# Clear connections
print("\nClearing connections...")
j.nodes_clear()
time.sleep(0.2)

# Create 12 INPUT pins on rows 8-19
print("\nSetting up 12 INPUT pins on rows 8-19...")
pins = []
for row in range(8, 20):
    pin = j.FakeGpioPin(row, j.INPUT, 2.0, 0.8)
    pins.append({'node': row, 'pin': pin})
    print(f"  ✓ Row {row:2d} ready")

print("\n✓ All pins configured!")
print("Thresholds: >2.0V=HIGH, <0.8V=LOW")
print("Reading all pins... (Ctrl+C to stop)\n")
print("="*70)

# Continuous read loop
try:
    count = 0
    while True:
        # Read all pins
        states = []
        for p in pins:
            state = p['pin'].value()
            states.append(state)
        
        # Display in two rows of 6
        print(f"\r[{count:05d}] ", end='')
        
        # First row (pins 8-13)
        for i in range(6):
            node = pins[i]['node']
            state_str = "1" if states[i] else "0"
            print(f"{node:2d}:{state_str} ", end='')
        
        print(" | ", end='')
        
        # Second row (pins 14-19)
        for i in range(6, 12):
            node = pins[i]['node']
            state_str = "1" if states[i] else "0"
            print(f"{node:2d}:{state_str} ", end='')
        
        print("  ", end='')  # Clear any trailing characters
        
        count += 1
        time.sleep_ms(200)  # Read 5 times per second
        
except KeyboardInterrupt:
    print("\n\n✓ Test stopped")
    print("="*70)

