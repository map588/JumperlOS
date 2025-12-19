# Adding New OLED Functions to Documentation Website

## Overview
This document provides all the information needed to add 13 new OLED control functions to the Jumperless documentation website. These functions were successfully implemented and tested in December 2024.

## New Functions Summary

### Text Size Control (3 functions)
1. `oled_set_text_size(size)` - Set default text size
2. `oled_get_text_size()` - Get current default size  
3. Modified `oled_print(text, size=-1)` - Now supports optional size parameter

### Print Redirection (1 function)
4. `oled_copy_print(enable)` - Copy Python print() to OLED

### Font System (3 functions)
5. `oled_get_fonts()` - List available fonts
6. `oled_set_font(name)` - Set font by name
7. `oled_get_current_font()` - Get current font name

### Bitmap Functions (3 functions)
8. `oled_load_bitmap(filepath)` - Load bitmap from file
9. `oled_display_bitmap(x, y, width, height, data=None)` - Display bitmap
10. `oled_show_bitmap_file(filepath, x, y)` - Load and display in one call

### Framebuffer Access (5 functions)
11. `oled_get_framebuffer()` - Get framebuffer as bytes
12. `oled_set_framebuffer(data)` - Set framebuffer from bytes
13. `oled_get_framebuffer_size()` - Get dimensions tuple
14. `oled_set_pixel(x, y, color)` - Set individual pixel
15. `oled_get_pixel(x, y)` - Get pixel value

---

## Documentation Content

### Page 1: Text Size and Print Redirection

**Title:** OLED Text Size and Print Redirection

**Content:**

```markdown
## Text Size Control

The OLED supports three text size modes:
- **Size 0**: Small scrolling text (Andale Mono 5pt) - perfect for terminal output
- **Size 1**: Normal centered text
- **Size 2**: Large centered text (default)

### oled_set_text_size(size)

Set the default text size for all subsequent `oled_print()` calls.

**Parameters:**
- `size` (int): Text size (0=small scrolling, 1=normal, 2=large)

**Returns:** `True` if successful, `False` if invalid size

**Example:**
```python
# Use small scrolling text
oled_set_text_size(0)
for i in range(10):
    oled_print(f"Line {i+1}")  # Each creates a new scrolling line
    
# Switch to large text
oled_set_text_size(2)
oled_print("BIG TEXT")
```

### oled_get_text_size()

Get the current default text size.

**Returns:** Current text size (0, 1, or 2)

**Example:**
```python
current_size = oled_get_text_size()
print(f"Current OLED text size: {current_size}")
```

### oled_print(text, size=-1)

Print text to OLED. If size is not specified, uses the default from `oled_set_text_size()`.

**Parameters:**
- `text`: Text to display (string, number, or any printable type)
- `size` (optional): Override text size for this call only

**Example:**
```python
# Use default size
oled_set_text_size(2)
oled_print("Uses size 2")

# Override size for one call
oled_print("Small text", 0)  # Temporarily use size 0
oled_print("Back to size 2")  # Returns to default
```

---

## Print Redirection

### oled_copy_print(enable)

Enable or disable copying Python `print()` output to the OLED display in real-time.

When enabled, all `print()` statements will appear on both the serial console and the OLED in small scrolling text mode. This is perfect for debugging without a serial connection.

**Parameters:**
- `enable` (bool): `True` to enable, `False` to disable

**Example:**
```python
# Enable print copying
oled_copy_print(True)

# These appear on both serial AND OLED
print("Starting test...")
voltage = adc_get(0)
print(f"Voltage: {voltage:.2f}V")
print("Test complete!")

# Disable print copying
oled_copy_print(False)
print("This only goes to serial")
```

**Practical Use Cases:**
- Debugging scripts when serial console isn't available
- Monitoring sensor readings on the OLED
- Creating portable devices that display status without a computer
```

---

### Page 2: Font System

**Title:** OLED Font System

**Content:**

```markdown
## Available Fonts

The OLED supports 11 font families, each with multiple point sizes automatically selected based on your text size setting:

1. **Eurostile** - Modern geometric sans-serif
2. **Jokerman** - Playful decorative font
3. **Comic Sans** - Casual rounded font
4. **Courier New** - Classic monospace
5. **New Science** - Futuristic tech font
6. **New Science Ext** - Extended version
7. **Andale Mono** - Clean monospace (5pt only)
8. **Free Mono** - Compact monospace (4pt only)
9. **Iosevka Regular** - Programming font with tight spacing
10. **Berkeley Mono** - Professional monospace
11. **Pragmatism** - Clean modern font

### oled_get_fonts()

Get a list of all available font families.

**Returns:** List of font family names (strings)

**Example:**
```python
fonts = oled_get_fonts()
print(f"Available fonts: {fonts}")
# Output: ['Eurostile', 'Jokerman', 'Comic Sans', ...]

# Display each font
for font in fonts:
    oled_set_font(font)
    oled_print(font, 2)
    time.sleep(1.5)
```

### oled_set_font(name)

Set the current font family by name. The font will remain active until changed.

**Parameters:**
- `name` (str): Font family name (case-insensitive)

**Returns:** `True` if successful, `False` if font not found

**Example:**
```python
# Set to Jokerman
if oled_set_font("Jokerman"):
    oled_print("Fun Font!", 2)
else:
    print("Font not found")
    
# Set to Courier for code-like display
oled_set_font("Courier New")
oled_print("Code Style", 2)
```

### oled_get_current_font()

Get the name of the currently active font family.

**Returns:** Current font family name (string)

**Example:**
```python
current = oled_get_current_font()
print(f"Current font: {current}")

# Save and restore font
saved_font = oled_get_current_font()
oled_set_font("Jokerman")
oled_print("Temporary", 2)
oled_set_font(saved_font)  # Restore original
```

**Font Selection Tips:**
- Monospace fonts (Courier, Andale, Berkeley) are best for code and data
- Sans-serif fonts (Eurostile, Pragmatism) are most readable
- Decorative fonts (Jokerman) are fun for titles and emphasis
- Small fonts (Andale 5pt, Free Mono 4pt) work well with size 0 scrolling mode
```

---

### Page 3: Bitmap Display

**Title:** OLED Bitmap Display

**Content:**

```markdown
## Bitmap File Format

The OLED supports two bitmap formats:

1. **Raw Format**: Pure bitmap data (guesses dimensions from file size)
   - 128x32 = 512 bytes
   - 128x64 = 1024 bytes
   - 64x32 = 256 bytes

2. **Custom Format**: 4-byte header + bitmap data
   - Bytes 0-1: Width (16-bit little-endian)
   - Bytes 2-3: Height (16-bit little-endian)
   - Remaining: Bitmap data (1 bit per pixel, packed)

**Bitmap Data Format:**
- 1 bit per pixel: 0=black/off, 1=white/on
- Pixels organized in vertical bytes (8 pixels per byte)
- LSB is top pixel, MSB is bottom pixel
- Compatible with Adafruit GFX format

### oled_load_bitmap(filepath)

Load a bitmap file into the internal bitmap buffer.

**Parameters:**
- `filepath` (str): Path to bitmap file (e.g., "/logo.bin")

**Returns:** `True` if loaded successfully, `False` on error

**Example:**
```python
if oled_load_bitmap("/jogo32h.bin"):
    print("Logo loaded!")
else:
    print("Failed to load logo")
```

### oled_display_bitmap(x, y, width, height, data=None)

Display a bitmap on the OLED.

**Two modes:**
1. **Use loaded bitmap**: If `data` is `None`, displays the bitmap from `oled_load_bitmap()`
2. **Direct data**: If `data` is provided, displays that bitmap immediately

**Parameters:**
- `x` (int): X position on display (0-127)
- `y` (int): Y position on display (0-31)
- `width` (int): Bitmap width in pixels (ignored if using loaded bitmap)
- `height` (int): Bitmap height in pixels (ignored if using loaded bitmap)
- `data` (bytes, optional): Bitmap data to display directly

**Returns:** `True` if successful, `False` on error

**Example:**
```python
# Method 1: Load then display
oled_load_bitmap("/logo.bin")
oled_display_bitmap(0, 0, 0, 0)  # width/height ignored for loaded bitmap

# Method 2: Display direct data
bitmap_data = bytes([0xFF, 0x00, 0xFF, 0x00] * 128)  # Striped pattern
oled_display_bitmap(0, 0, 128, 32, bitmap_data)
```

### oled_show_bitmap_file(filepath, x, y)

Convenience function that loads and displays a bitmap in one call.

**Parameters:**
- `filepath` (str): Path to bitmap file
- `x` (int): X position on display
- `y` (int): Y position on display

**Returns:** `True` if successful, `False` on error

**Example:**
```python
# One-liner to show a logo
oled_show_bitmap_file("/jogo32h.bin", 0, 0)
time.sleep(2)

# Show another image
oled_show_bitmap_file("/badge.bin", 32, 8)
```

**Creating Bitmap Files:**

You can create bitmap files using Python:

```python
# Create a simple pattern
width, height = 128, 32
data = bytearray()

for x in range(width):
    for y_byte in range(height // 8):
        # Create a checkerboard pattern
        byte_val = 0xAA if (x + y_byte) % 2 else 0x55
        data.append(byte_val)

# Save as raw format (no header)
with open("/pattern.bin", "wb") as f:
    f.write(data)

# Display it
oled_show_bitmap_file("/pattern.bin", 0, 0)
```
```

---

### Page 4: Framebuffer and Pixel Manipulation

**Title:** OLED Framebuffer and Pixel Access

**Content:**

```markdown
## Framebuffer Direct Access

The framebuffer functions provide low-level access to the display memory, enabling advanced graphics, animations, and screen capture.

**Framebuffer Format:**
- 1 bit per pixel (0=black/off, 1=white/on)
- Organized in vertical bytes (8 pixels per byte)
- Size: 512 bytes (128x32) or 1024 bytes (128x64)
- Compatible with Adafruit SSD1306 format

### oled_get_framebuffer()

Get a copy of the current OLED framebuffer as a bytes object.

**Returns:** Framebuffer data as `bytes` (512 or 1024 bytes depending on display size)

**Example:**
```python
# Capture current display
fb = oled_get_framebuffer()
print(f"Framebuffer size: {len(fb)} bytes")

# Save to file for later
with open("/screen_capture.bin", "wb") as f:
    f.write(fb)
```

### oled_set_framebuffer(data)

Set the entire OLED framebuffer from bytes or bytearray.

**Parameters:**
- `data` (bytes or bytearray): Framebuffer data (must be correct size for display)

**Returns:** `True` if successful, `False` if wrong size

**Example:**
```python
# Load and display saved screen
with open("/screen_capture.bin", "rb") as f:
    fb_data = f.read()
    
if oled_set_framebuffer(fb_data):
    print("Screen restored!")
else:
    print("Wrong framebuffer size")
```

### oled_get_framebuffer_size()

Get the dimensions and size of the framebuffer.

**Returns:** Tuple of `(width, height, buffer_size_in_bytes)`

**Example:**
```python
width, height, size = oled_get_framebuffer_size()
print(f"Display: {width}x{height}, {size} bytes")
# Output: Display: 128x32, 512 bytes

# Calculate pixels
total_pixels = width * height
print(f"Total pixels: {total_pixels}")
```

### oled_set_pixel(x, y, color)

Set a single pixel on the OLED.

**Note:** Call `oled_show()` after setting pixels to make changes visible.

**Parameters:**
- `x` (int): X coordinate (0 to width-1)
- `y` (int): Y coordinate (0 to height-1)
- `color` (int): Pixel color (0=black/off, 1=white/on)

**Returns:** `True` if successful, `False` if OLED not connected

**Example:**
```python
# Draw a diagonal line
oled_clear()
for i in range(32):
    oled_set_pixel(i, i, 1)
oled_show()

# Draw a box
for x in range(20, 108):
    oled_set_pixel(x, 10, 1)  # Top edge
    oled_set_pixel(x, 22, 1)  # Bottom edge
for y in range(10, 23):
    oled_set_pixel(20, y, 1)  # Left edge
    oled_set_pixel(107, y, 1) # Right edge
oled_show()
```

### oled_get_pixel(x, y)

Get the color value of a single pixel.

**Parameters:**
- `x` (int): X coordinate (0 to width-1)
- `y` (int): Y coordinate (0 to height-1)

**Returns:** Pixel color (0=black/off, 1=white/on, -1=error)

**Example:**
```python
# Check if a pixel is set
pixel = oled_get_pixel(64, 16)
if pixel == 1:
    print("Pixel is white/on")
elif pixel == 0:
    print("Pixel is black/off")
else:
    print("Error reading pixel")
```

---

## Advanced Examples

### Animated Graph
```python
import math

# Clear display
oled_clear()
width, height, _ = oled_get_framebuffer_size()

# Draw sine wave animation
for offset in range(100):
    oled_clear(False)  # Don't show() after clear to avoid flashing
    
    # Draw axes
    for x in range(width):
        oled_set_pixel(x, height//2, 1)  # Center line
    
    # Draw sine wave
    for x in range(width):
        y = int(height//2 + 10 * math.sin((x + offset) / 10))
        if 0 <= y < height:
            oled_set_pixel(x, y, 1)
    
    oled_show()
    time.sleep(0.05)
```

### Custom Bitmap Generation
```python
# Create a circle bitmap
width, height = 128, 32
center_x, center_y = width//2, height//2
radius = 12

oled_clear()
for y in range(height):
    for x in range(width):
        dx = x - center_x
        dy = y - center_y
        if dx*dx + dy*dy <= radius*radius:
            oled_set_pixel(x, y, 1)
oled_show()
```

### Screen Capture and Manipulation
```python
# Capture screen
fb = oled_get_framebuffer()

# Invert all pixels
inverted = bytearray(fb)
for i in range(len(inverted)):
    inverted[i] = ~inverted[i] & 0xFF

# Display inverted
oled_set_framebuffer(inverted)
time.sleep(1)

# Restore original
oled_set_framebuffer(fb)
```
```

---

## Quick Reference Code Snippets

Include this on a "OLED Quick Reference" page:

````markdown
## OLED Quick Reference

### Basic Display
```python
# Simple message
oled_print("Hello!", 2)

# Clear display
oled_clear()
```

### Scrolling Text
```python
# Enable small scrolling mode
oled_set_text_size(0)
for i in range(10):
    oled_print(f"Line {i}")
    time.sleep(0.3)
```

### Print Redirection
```python
# Mirror print() to OLED
oled_copy_print(True)
print("Status: OK")
print(f"Value: {sensor_read()}")
oled_copy_print(False)
```

### Font Showcase
```python
for font in oled_get_fonts():
    oled_set_font(font)
    oled_print(font, 2)
    time.sleep(1)
```

### Draw Graphics
```python
# Draw border
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

### Display Bitmap
```python
# Load and show
oled_show_bitmap_file("/logo.bin", 0, 0)
```
````

---

## Implementation Notes for Documentation Team

### Testing Code
All examples have been tested and work correctly. You can verify by running:
```python
exec(open("/scripts/test_oled_features.py").read())
```

### Related Functions
These new functions work alongside existing OLED functions:
- `oled_clear([show=True])` - Clear display (optionally skip show for animations)
- `oled_show()` - Update display
- `oled_connect()` - Connect to OLED
- `oled_disconnect()` - Disconnect from OLED

### Common Pitfalls to Mention
1. **Pixel changes need `oled_show()`**: After using `oled_set_pixel()`, you must call `oled_show()` to see the changes
2. **Framebuffer size must match**: `oled_set_framebuffer()` requires exact size (512 or 1024 bytes)
3. **Font persistence**: Once set with `oled_set_font()`, the font remains active until changed
4. **Print redirect uses small text**: `oled_copy_print()` automatically uses size 0 (small scrolling) mode

### Type Hints Available
Full type hints are available in `/scripts/jumperless.pyi` for IDE autocomplete support.

---

## Documentation Structure Recommendation

**Suggested page organization:**

1. **OLED Basics** (existing page - update)
   - Add note about new text size modes
   - Add link to new pages

2. **OLED Text and Fonts** (new page)
   - Text size control
   - Print redirection
   - Font system

3. **OLED Graphics** (new page)
   - Bitmap display
   - Framebuffer access
   - Pixel manipulation

4. **OLED Examples** (new page)
   - Complete working examples
   - Common patterns
   - Tips and tricks

5. **OLED API Reference** (new page)
   - All functions with full signatures
   - Quick reference table
   - Return value reference

---

## API Reference Table

Include this table on the API reference page:

| Function | Parameters | Returns | Description |
|----------|------------|---------|-------------|
| `oled_set_text_size(size)` | `size`: int (0-2) | bool | Set default text size |
| `oled_get_text_size()` | None | int | Get current text size |
| `oled_copy_print(enable)` | `enable`: bool | None | Enable print redirect |
| `oled_get_fonts()` | None | list[str] | List available fonts |
| `oled_set_font(name)` | `name`: str | bool | Set font by name |
| `oled_get_current_font()` | None | str | Get current font |
| `oled_load_bitmap(path)` | `path`: str | bool | Load bitmap file |
| `oled_display_bitmap(x, y, w, h, data)` | `x,y,w,h`: int, `data`: bytes? | bool | Display bitmap |
| `oled_show_bitmap_file(path, x, y)` | `path`: str, `x,y`: int | bool | Load & display |
| `oled_get_framebuffer()` | None | bytes | Get framebuffer |
| `oled_set_framebuffer(data)` | `data`: bytes/bytearray | bool | Set framebuffer |
| `oled_get_framebuffer_size()` | None | tuple[int,int,int] | Get dimensions |
| `oled_set_pixel(x, y, color)` | `x,y,color`: int | bool | Set one pixel |
| `oled_get_pixel(x, y)` | `x,y`: int | int | Get pixel value |

---

## Additional Resources

- Full API documentation: `/CodeDocs/OLED_MICROPYTHON_API.md`
- Quick reference: `/CodeDocs/OLED_QUICK_REFERENCE.md`
- Implementation summary: `/CodeDocs/OLED_MICROPYTHON_IMPLEMENTATION_SUMMARY.md`
- Test suite: `/scripts/test_oled_features.py`
- Type hints: `/scripts/jumperless.pyi`

---

## Contact & Questions

If you have questions about these functions while documenting them, refer to:
- Implementation in: `src/JumperlessMicroPythonAPI.cpp`
- C bindings in: `modules/jumperless/modjumperless.c`
- Python exports in: `scripts/jumperless_module.py`

All functions have been tested and verified working as of December 2024.

