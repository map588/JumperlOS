# OLED MicroPython Quick Reference

Quick reference for the new OLED functions in JumperlOS MicroPython.

## Text Size & Display

```python
# Set default text size
oled_set_text_size(0)  # Small scrolling
oled_set_text_size(1)  # Normal centered
oled_set_text_size(2)  # Large centered

# Print with default or override size
oled_print("Hello")           # Uses default
oled_print("Big", 2)          # Override to large
oled_print("Scrolling", 0)    # Override to small

# Get current size
size = oled_get_text_size()   # Returns 0, 1, or 2
```

## Print Redirection

```python
# Mirror print() to OLED
oled_copy_print(True)
print("Appears on OLED too!")
oled_copy_print(False)
```

## Fonts

```python
# List available fonts
fonts = oled_get_fonts()
# ['Eurostile', 'Jokerman', 'Comic Sans', 'Courier New',
#  'New Science', 'New Science Ext', 'Andale Mono', 'Free Mono',
#  'Iosevka Regular', 'Berkeley Mono', 'Pragmatism']

# Set font
oled_set_font("Jokerman")
oled_print("Fun!", 2)

# Get current font
font = oled_get_current_font()
```

## Bitmaps

```python
# Load from file
oled_load_bitmap("/logo.bin")
oled_display_bitmap(0, 0, 0, 0)

# One-liner
oled_show_bitmap_file("/logo.bin", 0, 0)

# Display raw data
data = bytes([0xFF, 0x00] * 256)
oled_display_bitmap(0, 0, 128, 32, data)
```

## Framebuffer

```python
# Get dimensions
w, h, size = oled_get_framebuffer_size()  # (128, 32, 512)

# Get/set framebuffer
fb = oled_get_framebuffer()      # Returns bytes
oled_set_framebuffer(fb)         # Set from bytes

# Pixel operations
oled_set_pixel(64, 16, 1)        # Set pixel white
color = oled_get_pixel(64, 16)   # Get pixel (0 or 1)
oled_show()                      # Update display
```

## Common Patterns

### Debug Output
```python
oled_copy_print(True)
for i in range(10):
    voltage = adc_get(0)
    print(f"V: {voltage:.2f}")
    time.sleep(0.5)
oled_copy_print(False)
```

### Terminal-like Output
```python
oled_set_text_size(0)  # Small scrolling
oled_clear()
for i in range(20):
    oled_print(f"Line {i}")
    time.sleep(0.2)
```

### Draw Border
```python
w, h, _ = oled_get_framebuffer_size()
oled_clear()
for x in range(w):
    oled_set_pixel(x, 0, 1)
    oled_set_pixel(x, h-1, 1)
for y in range(h):
    oled_set_pixel(0, y, 1)
    oled_set_pixel(w-1, y, 1)
oled_show()
```

### Font Showcase
```python
for font in oled_get_fonts():
    oled_set_font(font)
    oled_print(font, 2)
    time.sleep(1.5)
```

### Screen Capture
```python
# Save
fb = oled_get_framebuffer()
with open("/capture.bin", "wb") as f:
    f.write(fb)

# Restore
with open("/capture.bin", "rb") as f:
    fb = f.read()
oled_set_framebuffer(fb)
```

## Tips

- **Size 0** (small) is best for scrolling text and terminal output
- **Size 2** (large) is best for status messages and titles
- Call `oled_show()` after `oled_set_pixel()` to see changes
- Print redirection uses small scrolling mode automatically
- Framebuffer format: 1 bit/pixel, vertical bytes (Adafruit GFX format)
- Bitmap files: 512 bytes (128x32) or 1024 bytes (128x64)

## Full Documentation

See `CodeDocs/OLED_MICROPYTHON_API.md` for complete API reference.

## Test Suite

Run comprehensive tests:
```python
exec(open("/scripts/test_oled_features.py").read())
```

