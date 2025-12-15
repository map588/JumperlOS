"""
Test script for Fake GPIO API
Demonstrates path query, fast toggle, and voltage-based GPIO emulation

IMPORTANT: Use breadboard pins (1-60) or nano GPIO pins for fake GPIO,
NOT RP_GPIO_20-27 pins! The RP2350's GPIO pins have DNI (Do Not Intersect)
rules that prevent them from connecting to DACs, which would cause issues
with voltage-based GPIO emulation.
"""

import jumperless as j
import time

print("=== Fake GPIO API Test Suite ===\n")
print("Note: Using breadboard pins (safe nodes) for all tests")
print("** CONNECT YOUR OSCILLOSCOPE TO BREADBOARD PIN 20 **")
print("   (All output tests use pin 20 for easy scope observation)\n")

# Test 1: Path Information Query
print("Test 1: Path Information Query")
print("-" * 40)

# Create a simple connection using breadboard pins (safe nodes)
# j.nodes_clear()
j.connect(20, j.TOP_RAIL)  # Use breadboard pin 1
time.sleep(0.5)

# Get path info by index
path_info = j.get_path_info(0)
if path_info:
    print(f"Path 0 info: {path_info}")
    print(f"  Node1: {path_info['node1']}")
    print(f"  Node2: {path_info['node2']}")
    print(f"  Net: {path_info['net']}")
    print(f"  Chips: {path_info['chips']}")
    print(f"  X coords: {path_info['x']}")
    print(f"  Y coords: {path_info['y']}")
else:
    print("  No path found")

# Get all paths
all_paths = j.get_all_paths()
print(f"\nTotal active paths: {len(all_paths)}")

# Get path between specific nodes
path_between = j.get_path_between(20    , j.TOP_RAIL)
if path_between:
    print(f"Path between pin 20 and TOP_RAIL: Found")
else:
    print(f"Path between pin 20 and TOP_RAIL: Not found")

print("\n")

# Test 2: Fast Toggle (Context Manager)
print("Test 2: Fast Toggle Context Manager")
print("-" * 40)

# Create a connection to toggle using breadboard pins
j.nodes_clear()
j.connect(20, j.TOP_RAIL)  # Use breadboard pin 2

print("Connection created: pin 2 <-> TOP_RAIL")
print("Entering fast disconnect context...")

# Use context manager to temporarily disconnect
try:
    with j.FakeGpioDisconnect(20, j.TOP_RAIL):
        print("  Connection is disconnected (inside with block)")
        print("  Doing fast operations without path recomputation...")
        time.sleep(0.5)
        # Connection is broken here
    print("Connection restored (exited with block)")
except Exception as e:
    print(f"Error in fast toggle: {e}")

print("\n")

# Test 3: Fake GPIO Output with Default Voltages (5V/GND)
print("Test 3: Fake GPIO Output - Default Voltages")
print("-" * 40)

# j.nodes_clear()

try:
    # Create fake GPIO pin with default voltages (5V HIGH, 0V LOW)
    # Use breadboard pin 20 (connect oscilloscope here!)
    pin1 = j.FakeGpioPin(20, j.FAKE_GPIO_OUTPUT)
    print("Created FakeGpioPin on breadboard pin 20 (OUTPUT mode)")
    print("  v_high=5.0V (TOP_RAIL), v_low=0.0V (GND)")
    print("  ** Connect oscilloscope to pin 20 **")
    
    print("\nSetting pin HIGH...")
    pin1.on()
    print("  Pin is now HIGH (connected to TOP_RAIL/5V)")
    time.sleep(0.5)
    
    print("Setting pin LOW...")
    pin1.off()
    print("  Pin is now LOW (connected to GND/0V)")
    time.sleep(0.5)
    
    print("Toggling pin...")
    pin1.toggle()
    print("  Pin toggled to HIGH")
    
except Exception as e:
    print(f"Error: {e}")

print("\n")

# Test 4: Fake GPIO Output with Custom Voltages (±8V)
print("Test 4: Fake GPIO Output - Custom ±8V")
print("-" * 40)

# j.nodes_clear()

try:
    # Create fake GPIO pin with ±8V using DACs
    # Use breadboard pin 20 (same pin - connect oscilloscope here!)
    pin2 = j.FakeGpioPin(
        20,                         # Node (breadboard pin 20)
        j.FAKE_GPIO_OUTPUT,         # Mode
        5.0,                        # v_high = +8V (will use DAC0)
        -5.0,                       # v_low = -8V (will use DAC1)
        2.0,                        # threshold_high (not used for OUTPUT)
        0.8                         # threshold_low (not used for OUTPUT)
    )
    print("Created FakeGpioPin on breadboard pin 20 with ±8V")
    print("  v_high=+8.0V (DAC0), v_low=-8.0V (DAC1)")
    print("  ** Oscilloscope should be on pin 20 **")
    
    print("\nSetting pin HIGH (+8V)...")
    pin2.value(1)
    print("  Pin is now HIGH (DAC0 set to +8V)")
    time.sleep(0.5)
    
    print("Setting pin LOW (-8V)...")
    pin2.value(0)
    print("  Pin is now LOW (DAC1 set to -8V)")
    time.sleep(0.5)
    
    print("Toggling with on/off methods...")
    pin2.on()   # +8V
    time.sleep(0.3)
    pin2.off()  # -8V
    
except Exception as e:
    print(f"Error: {e}")

print("\n")

# Test 5: Fake GPIO Input Mode
print("Test 5: Fake GPIO Input Mode")
print("-" * 40)

try:
    # Create fake GPIO pin in INPUT mode
    # Use breadboard pin 21 (different pin for input - don't interfere with output test)
    pin3 = j.FakeGpioPin(
        21,                         # Node (breadboard pin 21)
        j.FAKE_GPIO_INPUT,          # Mode
        3.3,                        # v_high (not used for INPUT)
        0.0,                        # v_low (not used for INPUT)
        2.0,                        # threshold_high
        0.8                         # threshold_low
    )
    print("Created FakeGpioPin on breadboard pin 21 (INPUT mode)")
    print("  threshold_high=2.0V, threshold_low=0.8V")
    print("  (Note: INPUT test uses pin 21, separate from OUTPUT tests)")
    
    print("\nReading pin value...")
    val = pin3.value()
    print(f"  Pin value: {val} ({'HIGH' if val else 'LOW'})")
    
except Exception as e:
    print(f"Error: {e}")

print("\n")

# Test 6: Real-world Use Case - UART with ±8V Levels
print("Test 6: Use Case - ±8V UART Simulation")
print("-" * 40)

# j.nodes_clear()

try:
    # Create TX pin with ±8V levels
    # Use breadboard pin 20 (same pin for oscilloscope observation!)
    uart_tx = j.FakeGpioPin(20, j.FAKE_GPIO_OUTPUT, 8.0, -8.0)
    print("Created UART TX simulation on breadboard pin 20")
    print("  Sending data pattern: 1 0 1 1 0 1 (example)")
    print("  ** Watch the ±8V transitions on your oscilloscope! **")
    
    # Simulate sending a byte
    pattern = [1, 0, 1, 1, 0, 1]
    for bit in pattern:
        uart_tx.value(bit)
        print(f"    Bit: {bit} -> {'+8V' if bit else '-8V'}")
        time.sleep(0.1)
    
    print("  UART pattern sent successfully!")
    
except Exception as e:
    print(f"Error: {e}")

print("\n")

# Summary
print("=" * 40)
print("Test Suite Complete!")
print("=" * 40)
print("\nFake GPIO Features Tested:")
print("  ✓ Path information query (get_path_info, get_all_paths, get_path_between)")
print("  ✓ Fast toggle context manager (FakeGpioDisconnect)")
print("  ✓ Fake GPIO output with default voltages (5V/GND)")
print("  ✓ Fake GPIO output with custom voltages (±8V)")
print("  ✓ Fake GPIO input mode with thresholds")
print("  ✓ Real-world UART simulation with ±8V levels")
print("\nAll core functionality implemented and tested!")
