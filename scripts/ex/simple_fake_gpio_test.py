"""
Simple FakeGPIO Node-Based API Test

Quick test to verify the new node-based FakeGPIO API works correctly.
Uses TOP_RAIL and BOTTOM_RAIL for differential output.
"""

import jumperless as j
import time

print("\n" + "="*70)
print("FAKE GPIO NODE-BASED API TEST")
print("="*70)

# Clear connections
print("\n→ Clearing connections...")
j.nodes_clear()
time.sleep(0.2)

# Set power rail voltages (j.dac_set accepts node constants)
# TOP_RAIL (101) → DAC channel 2
# BOTTOM_RAIL (102) → DAC channel 3
print("\n→ Setting power rail voltages...")
j.dac_set(j.TOP_RAIL, 8.0)
j.dac_set(j.BOTTOM_RAIL, -8.0)
print("  TOP_RAIL = 8.0V (via DAC channel 2)")
print("  BOTTOM_RAIL = -8.0V (via DAC channel 3)")

# Test 1: Create two differential output pins
print("\n→ Test 1: Creating differential output pins")
print("  Pin A (node 10): HIGH=TOP_RAIL, LOW=BOTTOM_RAIL")
print("  Pin B (node 11): HIGH=BOTTOM_RAIL, LOW=TOP_RAIL (inverted)")

pin_a = j.FakeGpioPin(10, j.OUTPUT, j.TOP_RAIL, j.BOTTOM_RAIL)
pin_b = j.FakeGpioPin(11, j.OUTPUT, j.BOTTOM_RAIL, j.TOP_RAIL)

print("  ✓ Pins created successfully")

# Test 2: Toggle pins
print("\n→ Test 2: Toggling pins")
for i in range(5):
    print(f"  Cycle {i+1}: A=HIGH, B=HIGH (both at their HIGH voltage)")
    pin_a.on()
    pin_b.on()
    time.sleep(0.2)
    
    print(f"  Cycle {i+1}: A=LOW, B=LOW (both at their LOW voltage)")
    pin_a.off()
    pin_b.off()
    time.sleep(0.2)

print("  ✓ Toggle test complete")

# Test 3: Fast toggling with pause_core2
print("\n→ Test 3: Fast toggling (100 iterations with pause_core2)")
j.pause_core2(True)
for i in range(100):
    pin_a.value(1)
    pin_b.value(0)
    pin_a.value(0)
    pin_b.value(1)
j.pause_core2(False)
print("  ✓ Fast toggle complete")

# Test 4: Create INPUT pin
print("\n→ Test 4: Creating INPUT pin with loopback")
print("  Creating loopback: node 10 → node 15 (TX → RX)")
j.nodes_connect(10, 15)

rx_pin = j.FakeGpioPin(15, j.INPUT, 2.0, 0.8)
print("  ✓ RX pin created")

# Test reading with loopback
print("\n→ Test 5: Reading loopback values")
for i in range(5):
    pin_a.on()
    time.sleep_us(1000)
    rx_val = rx_pin.value()
    print(f"  TX=HIGH → RX reads: {rx_val} (expected: 1)")
    
    pin_a.off()
    time.sleep_us(1000)
    rx_val = rx_pin.value()
    print(f"  TX=LOW  → RX reads: {rx_val} (expected: 0)")

print("\n" + "="*70)
print("ALL TESTS COMPLETE")
print("="*70)
print("\nIf all tests passed, the node-based FakeGPIO API is working correctly!")
print("You can now run the RS-485 terminal: fake_gpio.py")
print("="*70 + "\n")

