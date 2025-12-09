"""
Jumperless MicroPython Script Template
=======================================

This template shows the recommended way to write Jumperless scripts
with full IDE autocomplete support while maintaining compatibility
with the device's global import environment.

Author: Your Name
Date: YYYY-MM-DD
Description: Brief description of what this script does
"""

# ============================================================================
# IDE Autocomplete Support (Optional - only for development)
# ============================================================================
# This block enables autocomplete in IDEs while never executing on device.
# It's completely optional - remove it if you don't need IDE support.

# type: ignore
#if False:  # Never executes - IDE only
from jumperless import *

# ============================================================================
# Script Configuration
# ============================================================================

# Your configuration constants here
LED_PIN = GPIO_1
BUTTON_PIN = GPIO_2
POLL_INTERVAL = 0.1  # seconds

# ============================================================================
# Helper Functions (Optional)
# ============================================================================

def setup():
    """Initialize hardware connections and GPIO states"""
    # Example: Set up GPIO directions
    gpio_set_dir(LED_PIN, OUTPUT)
    gpio_set_dir(BUTTON_PIN, INPUT)
    gpio_set_pull(BUTTON_PIN, PULLUP)
    
    # Example: Configure power rails
    dac_set(TOP_RAIL, 5.0)
    dac_set(BOTTOM_RAIL, 0.0)
    
    # Example: Make connections
    connect(TOP_RAIL, LED_PIN)
    connect(BUTTON_PIN, ADC0)
    
    oled_print("Setup complete")

def loop():
    """Main program loop"""
    # Your main logic here
    button_state = gpio_get(BUTTON_PIN)
    
    if button_state:  # GPIOState is truthy when HIGH
        gpio_set(LED_PIN, HIGH)
        oled_print("ON")
    else:
        gpio_set(LED_PIN, LOW)
        oled_print("OFF")
    
    time.sleep(POLL_INTERVAL)

def cleanup():
    """Clean up before exit"""
    # Turn off outputs
    gpio_set(LED_PIN, LOW)
    
    # Clear connections if desired
    # nodes_clear()
    
    # Save state if needed
    # nodes_save()
    
    oled_print("Goodbye")

# ============================================================================
# Main Program
# ============================================================================

def main():
    """Main entry point"""
    try:
        setup()
        
        # Run main loop
        while True:
            loop()
            
    except KeyboardInterrupt:
        # Handle Ctrl+C gracefully
        print("\nInterrupted by user")
    except Exception as e:
        # Handle errors
        print(f"Error: {e}")
        oled_print(f"Error: {e}")
    finally:
        # Always run cleanup
        cleanup()

# Auto-run when executed
if __name__ == "__main__":
    main()

# ============================================================================
# Usage Examples (Comment out or remove)
# ============================================================================

# Example 1: Simple LED blink
# for i in range(10):
#     gpio_set(GPIO_1, HIGH)
#     time.sleep(0.5)
#     gpio_set(GPIO_1, LOW)
#     time.sleep(0.5)

# Example 2: Read voltage and display
# voltage = adc_get(0)
# oled_print(f"{voltage:.2f}V")

# Example 3: Connect nodes with probe
# oled_print("Touch 2 pads")
# pad1 = probe_read()
# pad2 = probe_read()
# connect(pad1, pad2)
# oled_print("Connected!")

# Example 4: Generate waveform
# wavegen_set_output(DAC1)
# wavegen_set_wave(SINE)
# wavegen_set_freq(100)  # 100 Hz
# wavegen_set_amplitude(3.3)
# wavegen_start()
# time.sleep(5)
# wavegen_stop()

