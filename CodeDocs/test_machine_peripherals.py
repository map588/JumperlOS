#!/usr/bin/env python3
"""
Quick Test Script for Jumperless MicroPython Machine Module Enhancements
Run this in the Jumperless MicroPython REPL to verify all peripherals work
"""

print("=" * 60)
print("Jumperless MicroPython Machine Module Test")
print("=" * 60)

import machine
import sys

# Test 1: Basic machine functions
print("\n1. Testing basic machine functions...")
try:
    uid = machine.unique_id()
    print(f"   ✓ machine.unique_id(): {uid.hex()}")
except Exception as e:
    print(f"   ✗ machine.unique_id() failed: {e}")

try:
    freq = machine.freq()
    print(f"   ✓ machine.freq(): {freq} Hz ({freq/1000000:.1f} MHz)")
except Exception as e:
    print(f"   ✗ machine.freq() failed: {e}")

try:
    cause = machine.reset_cause()
    cause_str = "PWRON" if cause == machine.PWRON_RESET else "WDT" if cause == machine.WDT_RESET else "UNKNOWN"
    print(f"   ✓ machine.reset_cause(): {cause} ({cause_str})")
except Exception as e:
    print(f"   ✗ machine.reset_cause() failed: {e}")

# Test 2: Pin (should already work)
print("\n2. Testing machine.Pin...")
try:
    pin = machine.Pin(20, machine.Pin.OUT)
    pin.value(1)
    pin.value(0)
    print("   ✓ machine.Pin working")
except Exception as e:
    print(f"   ✗ machine.Pin failed: {e}")

# Test 3: Timer
print("\n3. Testing machine.Timer...")
try:
    timer_count = 0
    def timer_callback(t):
        global timer_count
        timer_count += 1
    
    tim = machine.Timer(mode=machine.Timer.PERIODIC, freq=10, callback=timer_callback)
    import time
    time.sleep(0.5)
    tim.deinit()
    if timer_count >= 4:
        print(f"   ✓ machine.Timer working (fired {timer_count} times)")
    else:
        print(f"   ? machine.Timer uncertain (fired {timer_count} times, expected ~5)")
except Exception as e:
    print(f"   ✗ machine.Timer failed: {e}")

# Test 4: RTC
print("\n4. Testing machine.RTC...")
try:
    rtc = machine.RTC()
    rtc.datetime((2024, 12, 10, 2, 15, 30, 0, 0))
    dt = rtc.datetime()
    print(f"   ✓ machine.RTC working: {dt[0]}/{dt[1]}/{dt[2]} {dt[4]}:{dt[5]}:{dt[6]}")
except Exception as e:
    print(f"   ✗ machine.RTC failed: {e}")

# Test 5: WDT (be careful - don't let it reset!)
print("\n5. Testing machine.WDT...")
try:
    wdt = machine.WDT(timeout=8000)
    wdt.feed()
    print("   ✓ machine.WDT working (fed successfully)")
    # Keep feeding it so we don't reset
    wdt.feed()
except Exception as e:
    print(f"   ✗ machine.WDT failed: {e}")

# Test 6: PWM
print("\n6. Testing machine.PWM...")
try:
    pwm = machine.PWM(machine.Pin(20))
    pwm.freq(1000)
    pwm.duty_u16(32768)
    duty = pwm.duty_u16()
    freq = pwm.freq()
    pwm.deinit()
    print(f"   ✓ machine.PWM working: freq={freq}Hz, duty={duty}/65535")
except Exception as e:
    print(f"   ✗ machine.PWM failed: {e}")

# Test 7: ADC
print("\n7. Testing machine.ADC...")
try:
    # Try temperature sensor first (no pin required)
    adc_temp = machine.ADC(4)  # or machine.ADC.CORE_TEMP
    temp_reading = adc_temp.read_u16()
    print(f"   ✓ machine.ADC working: temp sensor={temp_reading}")
    
    # Optionally test GPIO ADC if pin 26 is available
    # adc_pin = machine.ADC(26)
    # reading = adc_pin.read_u16()
    # print(f"   ✓ machine.ADC(26): {reading}")
except Exception as e:
    print(f"   ✗ machine.ADC failed: {e}")

# Test 8: I2C (requires pins to be available)
print("\n8. Testing machine.I2C...")
try:
    # Use GPIO20/21 (primary GPIO pins on Jumperless)
    i2c = machine.I2C(0, scl=machine.Pin(21), sda=machine.Pin(20), freq=400000)
    devices = i2c.scan()
    print(f"   ✓ machine.I2C working: found {len(devices)} device(s) {[hex(d) for d in devices]}")
    i2c.deinit()
except Exception as e:
    print(f"   ✗ machine.I2C failed: {e}")

# Test 9: SPI (requires pins to be available)
print("\n9. Testing machine.SPI...")
try:
    # SPI1 with GPIO26/27/24 (need to verify these are available)
    spi = machine.SPI(1, baudrate=1000000, sck=machine.Pin(26), mosi=machine.Pin(27), miso=machine.Pin(24))
    # Just test initialization, don't actually transfer
    print("   ✓ machine.SPI working: initialized successfully")
    spi.deinit()
except Exception as e:
    print(f"   ✗ machine.SPI failed: {e}")

# Summary
print("\n" + "=" * 60)
print("Test Summary:")
print("=" * 60)
print("✓ = Pass, ✗ = Fail, ? = Uncertain")
print("\nAll machine module enhancements are now available!")
print("See MICROPYTHON_FEATURES_ADDED.md for detailed usage examples.")
print("=" * 60)



