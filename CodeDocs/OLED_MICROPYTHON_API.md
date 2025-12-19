# OLED MicroPython API Documentation

This document describes the new OLED control functions added to the Jumperless MicroPython API.

## Overview

The OLED API provides comprehensive control over the OLED display, including:
- Text size control with scrolling support
- Print redirection (copy Python print() to OLED)
- Font system with 11 font families
- Bitmap loading and display
- Direct framebuffer manipulation
- Pixel-level control

## Text Size Control

### `oled_set_text_size(size: int) -> bool`

Set the default text size for `oled_print()`.

**Parameters:**
- `size`: Text size (0=small multiline scrolling, 1=normal, 2=large centered)

**Returns:** `True` if successful, `False` if invalid size

**Example:**
```python
oled_set_text_size(0)  # Small scrolling text
oled_print("This will scroll")

oled_set_text_size(2)  # Large centered text
oled_print("BIG TEXT")
```

### `oled_get_text_size() -> int`

Get the current default text size.

**Returns:** Current default text size (0-2)

**Example:**
```python
current_size = oled_get_text_size()
print(f"Current size: {current_size}")
```

### `oled_print(text, size=-1)`

Print text to OLED. If size is -1 (default), uses the size set by `oled_set_text_size()`.

**Size Modes:**
- **0 (Small)**: Multiline scrolling text using `showMultiLineSmallText()`. Perfect for terminal-like output.
- **1 (Normal)**: Regular centered text
- **2 (Large)**: Large centered text (default)

**Example:**
```python
# Use default size
oled_print("Hello")

# Override with specific size
oled_print("Small text", 0)
oled_print("Large text", 2)
```

## Print Redirection

### `oled_copy_print(enable: bool)`

Enable/disable copying MicroPython `print()` output to OLED.

When enabled, all `print()` statements will also appear on the OLED in small scrolling text mode. This is perfect for debugging or monitoring script output.

**Parameters:**
- `enable`: `True` to enable copy mode, `False` to disable

**Example:**
```python
# Enable print copying
oled_copy_print(True)

# These will appear on both serial and OLED
print("Starting test...")
print("Value: 42")
print("Done!")

# Disable print copying
oled_copy_print(False)
print("This only goes to serial")
```

## Font System

### `oled_get_fonts() -> list[str]`

Get list of available font families.

**Returns:** List of 11 font family names

**Available Fonts:**
1. Eurostile
2. Jokerman
3. Comic Sans
4. Courier New
5. New Science
6. New Science Ext
7. Andale Mono
8. Free Mono
9. Iosevka Regular
10. Berkeley Mono
11. Pragmatism

**Example:**
```python
fonts = oled_get_fonts()
print(f"Available fonts: {fonts}")
# Output: ['Eurostile', 'Jokerman', 'Comic Sans', ...]
```

### `oled_set_font(font_name: str) -> bool`

Set the current font family.

**Parameters:**
- `font_name`: Name of font family (case-insensitive)

**Returns:** `True` if font was set successfully, `False` if font not found

**Example:**
```python
oled_set_font("Jokerman")
oled_print("Fun font!", 2)

oled_set_font("Courier New")
oled_print("Monospace", 2)
```

### `oled_get_current_font() -> str`

Get the name of the currently active font.

**Returns:** Current font family name

**Example:**
```python
current = oled_get_current_font()
print(f"Using font: {current}")
```

## Bitmap Functions

### `oled_load_bitmap(filepath: str) -> bool`

Load a bitmap file into the internal bitmap buffer.

**Supported Formats:**
- Raw bitmap data (guesses dimensions from file size)
- Custom format with 4-byte header (width, height as 16-bit little-endian)

**Common Sizes:**
- 128x32 = 512 bytes
- 128x64 = 1024 bytes
- 64x32 = 256 bytes
- 128x31 = 496 bytes

**Parameters:**
- `filepath`: Path to bitmap file (e.g., "/jogo32h.bin")

**Returns:** `True` if loaded successfully, `False` on error

**Example:**
```python
if oled_load_bitmap("/logo.bin"):
    print("Bitmap loaded!")
else:
    print("Failed to load bitmap")
```

### `oled_display_bitmap(x: int, y: int, width: int, height: int, data: bytes = None) -> bool`

Display a bitmap on the OLED.

**Two modes:**
1. **Use loaded bitmap**: If `data` is `None`, displays the bitmap loaded by `oled_load_bitmap()`
2. **Direct data**: If `data` is provided, displays that bitmap directly

**Parameters:**
- `x`: X position on display
- `y`: Y position on display
- `width`: Bitmap width in pixels (ignored if using loaded bitmap)
- `height`: Bitmap height in pixels (ignored if using loaded bitmap)
- `data`: Optional bitmap data as bytes (1 bit per pixel, packed)

**Returns:** `True` if displayed successfully, `False` on error

**Example:**
```python
# Display loaded bitmap
oled_load_bitmap("/logo.bin")
oled_display_bitmap(0, 0, 0, 0)  # x, y, width/height ignored

# Display direct data
bitmap_data = bytes([0xFF, 0x00, 0xFF, 0x00] * 128)  # Example pattern
oled_display_bitmap(0, 0, 128, 32, bitmap_data)
```

### `oled_show_bitmap_file(filepath: str, x: int, y: int) -> bool`

Load and display a bitmap file in one call.

Convenience function that combines `oled_load_bitmap()` and `oled_display_bitmap()`.

**Parameters:**
- `filepath`: Path to bitmap file
- `x`: X position on display
- `y`: Y position on display

**Returns:** `True` if successful, `False` on error

**Example:**
```python
# One-liner to show a bitmap
oled_show_bitmap_file("/jogo32h.bin", 0, 0)
```

## Framebuffer Access

### `oled_get_framebuffer() -> bytes`

Get the current OLED framebuffer as bytes.

Returns a copy of the framebuffer that can be modified and written back with `oled_set_framebuffer()`.

**Format:** 1 bit per pixel, packed. Pixels are organized in vertical bytes (8 pixels per byte).

**Returns:** Framebuffer data as bytes
- 128x32 display = 512 bytes
- 128x64 display = 1024 bytes

**Example:**
```python
fb = oled_get_framebuffer()
print(f"Framebuffer size: {len(fb)} bytes")

# Save to file
with open("/screen_capture.bin", "wb") as f:
    f.write(fb)
```

### `oled_set_framebuffer(data: bytes | bytearray) -> bool`

Set the entire OLED framebuffer from bytes.

Allows direct manipulation of the display buffer. Data must be the correct size for the display.

**Parameters:**
- `data`: Framebuffer data (1 bit per pixel, packed)
  - Must be 512 bytes for 128x32
  - Must be 1024 bytes for 128x64

**Returns:** `True` if successful, `False` if wrong size

**Example:**
```python
# Load framebuffer from file
with open("/screen_capture.bin", "rb") as f:
    fb_data = f.read()

oled_set_framebuffer(fb_data)
```

### `oled_get_framebuffer_size() -> tuple[int, int, int]`

Get the framebuffer dimensions.

**Returns:** Tuple of `(width, height, buffer_size_in_bytes)`

**Example:**
```python
width, height, size = oled_get_framebuffer_size()
print(f"Display: {width}x{height}, {size} bytes")
# Output: Display: 128x32, 512 bytes
```

### `oled_set_pixel(x: int, y: int, color: int) -> bool`

Set a single pixel on the OLED.

**Note:** Call `oled_show()` to make the change visible.

**Parameters:**
- `x`: X coordinate (0 to width-1)
- `y`: Y coordinate (0 to height-1)
- `color`: Pixel color (0=black/off, 1=white/on)

**Returns:** `True` if successful, `False` if OLED not connected

**Example:**
```python
# Draw a diagonal line
for i in range(32):
    oled_set_pixel(i, i, 1)

oled_show()  # Make changes visible
```

### `oled_get_pixel(x: int, y: int) -> int`

Get the value of a single pixel.

**Parameters:**
- `x`: X coordinate (0 to width-1)
- `y`: Y coordinate (0 to height-1)

**Returns:** Pixel color (0=black/off, 1=white/on, -1=error)

**Example:**
```python
pixel = oled_get_pixel(64, 16)
if pixel == 1:
    print("Pixel is on")
elif pixel == 0:
    print("Pixel is off")
else:
    print("Error reading pixel")
```

## Complete Examples

### Example 1: Debug Output to OLED

```python
# Enable print copying for debugging
oled_copy_print(True)

# Your code with debug output
for i in range(10):
    voltage = adc_get(0)
    print(f"V{i}: {voltage:.2f}V")
    time.sleep(0.5)

oled_copy_print(False)
```

### Example 2: Custom Graphics

```python
# Get display dimensions
width, height, _ = oled_get_framebuffer_size()

# Clear and draw a pattern
oled_clear()

# Draw a box
for x in range(20, 108):
    oled_set_pixel(x, 10, 1)
    oled_set_pixel(x, 22, 1)

for y in range(10, 23):
    oled_set_pixel(20, y, 1)
    oled_set_pixel(107, y, 1)

# Draw text inside
oled_show()
```

### Example 3: Font Showcase

```python
fonts = oled_get_fonts()

for font in fonts:
    oled_set_font(font)
    oled_print(font, 2)
    time.sleep(1.5)
```

### Example 4: Bitmap Animation

```python
# Load multiple frames
frames = ["/frame1.bin", "/frame2.bin", "/frame3.bin"]

# Animate
for _ in range(10):  # 10 loops
    for frame in frames:
        oled_show_bitmap_file(frame, 0, 0)
        time.sleep(0.1)
```

## Implementation Notes

### Architecture

The OLED API follows a three-layer architecture:

1. **C++ Layer** (`JumperlessMicroPythonAPI.cpp`): Core functionality
2. **C Bindings** (`modjumperless.c`): MicroPython C API wrappers
3. **Python Layer** (`jumperless_module.py`): Python exports and type hints

### Print Redirection Hook

Print redirection is implemented by hooking into `mp_hal_stdout_tx_strn_cooked()` in `Python_Proper.cpp`. When enabled, stdout is duplicated to the `OLEDOut` stream, which uses `showMultiLineSmallText()` for scrolling terminal-like display.

### Bitmap Format

Bitmaps use 1 bit per pixel, packed into bytes. The format is compatible with Adafruit GFX library:
- Pixels are organized in vertical bytes (8 pixels per byte)
- Each byte represents a vertical column of 8 pixels
- LSB is the top pixel, MSB is the bottom pixel

### Memory Considerations

- Framebuffer size: 512 bytes (128x32) or 1024 bytes (128x64)
- Bitmap buffer: 1024 bytes maximum
- Font data: Stored in flash, minimal RAM usage

## Testing

Run the comprehensive test suite:

```python
exec(open("/scripts/test_oled_features.py").read())
```

Or run individual tests:

```python
# Test text sizes
oled_set_text_size(0)
oled_print("Small")

# Test print copy
oled_copy_print(True)
print("On OLED too!")

# Test fonts
for font in oled_get_fonts():
    oled_set_font(font)
    oled_print(font, 2)
    time.sleep(1)
```

