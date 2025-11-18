# OLED Bitmap Converter

This script converts standard image files (PNG, JPG, etc.) to the binary format required by Jumperless OLED displays.

## Quick Start

```bash
# Basic conversion (128x32)
python3 image_to_oled_bitmap.py your_image.png output.bin

# With options
python3 image_to_oled_bitmap.py your_image.png output.bin --width 128 --height 32 --threshold 128 --invert
```

## Fixed Issues

### 1. Incorrect Bit Ordering
**Problem**: Original script used LSB-first ordering, but Adafruit GFX uses MSB-first.
**Solution**: Changed bit ordering to match Adafruit GFX format:
- MSB (bit 7) = leftmost pixel
- LSB (bit 0) = rightmost pixel

### 2. Transparency Handling
**Problem**: Images with alpha channels (RGBA, LA, PA) were converted to all-black pixels.
**Solution**: Automatically composite transparent images onto white background before conversion.

### 3. Color Inversion
**Problem**: No easy way to invert colors for different artistic effects.
**Solution**: Added `--invert` flag to swap black and white colors.

## Output Format

```
Bytes 0-1: Width (16-bit little-endian)
Bytes 2-3: Height (16-bit little-endian)
Bytes 4+:  Bitmap data (Adafruit GFX format)
```

## Usage Examples

```bash
# Standard conversion
python3 image_to_oled_bitmap.py logo.png /logo.bin

# Smaller display
python3 image_to_oled_bitmap.py icon.png /icon.bin --width 64 --height 32

# Light image on dark background
python3 image_to_oled_bitmap.py dark_logo.png /dark_logo.bin --invert

# Fine-tune contrast
python3 image_to_oled_bitmap.py photo.png /photo.bin --threshold 180
```

## Testing Conversion

To verify your bitmap visually:

```python
import struct

with open('your_bitmap.bin', 'rb') as f:
    # Read header
    width, height = struct.unpack('<HH', f.read(4))
    print(f"Size: {width}x{height}")
    
    # Display as ASCII art
    for y in range(height):
        row = ""
        for x in range(0, width, 8):
            byte = ord(f.read(1))
            for bit in range(8):
                if x + bit < width:
                    pixel = (byte >> (7 - bit)) & 1
                    row += "#" if pixel else "."
        print(row)
```

## Requirements

```bash
pip install Pillow
```

## See Also

- Main documentation: `OLED_CUSTOM_STARTUP.md`
- Example images: `examples/oled_bitmaps/` (if available)

