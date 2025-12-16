#!/usr/bin/env python3
"""
Test script for FakeGPIO node-based configuration

This demonstrates the new preferred API where you directly specify
voltage source nodes instead of voltages.

Usage:
    import jumperless as j
    
    # Old way (voltage-based, still works):
    pin = j.FakeGpioPin(10, j.OUTPUT, 7.0, -7.0)
    
    # New way (node-based, preferred):
    pin = j.FakeGpioPin(10, j.OUTPUT, j.TOP_RAIL, j.GND)
    
    # Default (TOP_RAIL and GND):
    pin = j.FakeGpioPin(10, j.OUTPUT)
"""

import jumperless as j
import time

# Clear any existing connections
j.nodes_clear()

print("Testing new node-based FakeGPIO API...\n")

# Test 1: Default configuration (TOP_RAIL and GND)
print("Test 1: Default configuration")
print("  Creating pin on node 10 with defaults (TOP_RAIL, GND)")
pin1 = j.FakeGpioPin(10, j.OUTPUT)
pin1.value(1)
print("  Pin 1 HIGH")
time.sleep(0.5)
pin1.value(0)
print("  Pin 1 LOW\n")

# Test 2: Explicit TOP_RAIL and GND
print("Test 2: Explicit TOP_RAIL and GND")
print("  Creating pin on node 19 with j.TOP_RAIL and j.GND")
pin2 = j.FakeGpioPin(19, j.OUTPUT, j.TOP_RAIL, j.GND)
pin2.on()
print("  Pin 2 ON")
time.sleep(0.5)
pin2.off()
print("  Pin 2 OFF\n")

# Test 3: Using BOTTOM_RAIL and GND
print("Test 3: Using BOTTOM_RAIL and GND")
print("  Creating pin on node 15 with j.BOTTOM_RAIL and j.GND")
pin3 = j.FakeGpioPin(15, j.OUTPUT, j.BOTTOM_RAIL, j.GND)
pin3.toggle()
print("  Pin 3 toggled HIGH")
time.sleep(0.5)
pin3.toggle()
print("  Pin 3 toggled LOW\n")

# Test 4: Using DACs (if set to specific voltages)
print("Test 4: Using DACs")
print("  Setting DAC0=3.3V, DAC1=1.5V")
j.dac_set(0, 3.3)
j.dac_set(1, 1.5)
print("  Creating pin on node 20 with j.DAC0 and j.DAC1")
pin4 = j.FakeGpioPin(20, j.OUTPUT, j.DAC0, j.DAC1)
pin4.value(1)
print("  Pin 4 HIGH (3.3V from DAC0)")
time.sleep(0.5)
pin4.value(0)
print("  Pin 4 LOW (1.5V from DAC1)\n")

# Test 5: Fast toggling with pause_core2 (from user's example)
print("Test 5: Fast toggling with pause_core2")
print("  Creating two pins for fast toggling")
fast_pin1 = j.FakeGpioPin(25, j.OUTPUT, j.TOP_RAIL, j.GND)
fast_pin2 = j.FakeGpioPin(30, j.OUTPUT, j.TOP_RAIL, j.GND)
j.pause_core2(True)

print("  Toggling 100 times...")
for i in range(100):
    fast_pin1.value(1)
    fast_pin2.off()
    fast_pin1.value(0)
    fast_pin2.on()

j.pause_core2(False)
print("  Done!\n")

# Test 6: Backward compatibility - voltage-based config still works
print("Test 6: Backward compatibility - voltage-based config")
print("  Creating pin with old API: FakeGpioPin(35, OUTPUT, 5.0, 0.0)")
old_style_pin = j.FakeGpioPin(35, j.OUTPUT, 5.0, 0.0)
old_style_pin.value(1)
print("  Old-style pin HIGH")
time.sleep(0.5)
old_style_pin.value(0)
print("  Old-style pin LOW\n")

print("All tests completed successfully!")
print("\nKey advantages of node-based API:")
print("  1. No DAC allocation magic - you control which sources are used")
print("  2. Voltages automatically follow rail/DAC changes")
print("  3. More explicit and predictable behavior")
print("  4. Can share voltage sources across multiple pins safely")

