# Custom OLED Startup Display

The Jumperless OLED can now display custom text or bitmap images on startup instead of the default Jogo logo.

## Features

- **Text Display**: Show custom text messages on startup
- **Bitmap Display**: Show custom bitmap images (logos, graphics) on startup
- **Auto-detection**: The system automatically detects whether the `startup_message` is text or a file path

## Configuration

### Setting Custom Text

To display text on startup, set the `startup_message` in your config file:

```ini
[top_oled]
startup_message = Hello World!
```

The text will be automatically centered and displayed in size 2 font.

### Setting Custom Bitmap

To display a bitmap image, set `startup_message` to a file path:

```ini
[top_oled]
startup_message = logo.bin          # Relative path (recommended)
# OR
startup_message = /logo.bin         # Absolute path
```

The system automatically detects if the value is a file path by:
1. Checking for common bitmap extensions (.bin, .bmp, .xbm, .raw)
2. Checking if the file exists on the filesystem

**Supported formats:**
- Binary files with 4-byte header (from our conversion script)
- Raw binary bitmap data (512, 1024, or 256 bytes for common sizes)
- C-style arrays converted to binary (from img2cpp, etc.)

## Creating Bitmap Files

### Using the Conversion Script

A Python script is provided to convert standard image files (PNG, JPG, etc.) to the OLED bitmap format:

```bash
# Basic usage - creates 128x32 bitmap
python scripts/image_to_oled_bitmap.py your_logo.png /logo.bin

# Custom dimensions
python scripts/image_to_oled_bitmap.py your_logo.png /logo.bin --width 64 --height 32

# Adjust brightness threshold (for better contrast)
python scripts/image_to_oled_bitmap.py your_logo.png /logo.bin --threshold 200

# Invert colors (swap black and white)
python scripts/image_to_oled_bitmap.py your_logo.png /logo.bin --invert
```

#### Requirements
The script requires Pillow (PIL):
```bash
pip install Pillow
```

### Using img2cpp or Other Converters

You can also use popular online tools like [img2cpp](https://javl.github.io/image2cpp/) and convert their output:

1. Go to img2cpp and upload your image
2. Set Canvas size to match your display (e.g., 128x32)
3. Generate code and save as .h file
4. Convert to binary:

```bash
python scripts/cpp_array_to_bin.py bitmap.h logo.bin
```

The converter will automatically detect array dimensions and format.

### Bitmap File Format

The bitmap file format is:

```
Bytes 0-1: Width (16-bit little-endian)
Bytes 2-3: Height (16-bit little-endian)
Bytes 4+:  Bitmap data
```

The bitmap data is organized as:
- Each byte represents 8 horizontal pixels
- MSB (bit 7) = leftmost pixel of the 8
- LSB (bit 0) = rightmost pixel of the 8
- Bit value 1 = white pixel, 0 = black pixel
- Pixels are stored row by row, left to right, top to bottom
- This matches the Adafruit GFX drawBitmap() format

**Example:**
For a 128x32 display:
- Header: 4 bytes (80 00 20 00 in hex = 128, 32 in little-endian)
- Data: 512 bytes (128 pixels/row × 32 rows ÷ 8 bits/byte)

### Raw Binary Format (No Header)

For convenience, the system also accepts raw binary bitmap data without a header for common sizes:
- **512 bytes** = assumed to be 128×32
- **1024 bytes** = assumed to be 128×64
- **256 bytes** = assumed to be 64×32

This allows direct use of bitmap data from various sources without conversion.

### Supported Dimensions

- Maximum width: 128 pixels
- Maximum height: 64 pixels
- Recommended for 128x32 displays: 128x32 or smaller

## Usage Examples

### Example 1: Welcome Message
```ini
[top_oled]
startup_message = Welcome!
```

### Example 2: Custom Logo (Absolute Path)
```ini
[top_oled]
startup_message = /custom_logo.bin
```

### Example 3: Custom Logo (Relative Path)
```ini
[top_oled]
startup_message = logo.bin
```
(Relative paths are automatically prefixed with `/` for FatFS)

### Example 4: Small Centered Bitmap
```ini
[top_oled]
startup_message = small_icon.bin
```
(For a 64x32 bitmap, it will be automatically centered on the display)

### Example 5: Raw Binary from img2cpp
```ini
[top_oled]
startup_message = my_bitmap.bin
```
(512-byte raw binary file, no header needed)

## Uploading Bitmap Files

1. Create your bitmap file using the conversion script
2. Copy the file to the Jumperless filesystem (e.g., via USB mass storage mode)
3. Update the config file with the file path
4. Restart or reconnect the OLED

## Troubleshooting

### "Bitmap Error" displayed
- Check that the file path is correct (starts with `/`)
- Verify the file exists on the filesystem
- Ensure the file format is correct (use the conversion script)
- Check file dimensions are within limits (≤128x64)

### Image looks inverted
- Use the `--invert` flag to swap black and white colors
- Alternatively, adjust the `--threshold` parameter:
  - Lower threshold (e.g., 100) = more white pixels
  - Higher threshold (e.g., 200) = more black pixels

### Image is distorted
- Ensure source image aspect ratio matches target dimensions
- For 128x32 displays, use 4:1 aspect ratio images

## Implementation Details

### Text vs. Bitmap Detection

The system uses a simple heuristic:
- If `startup_message` starts with `/`, it's treated as a file path
- Otherwise, it's displayed as text

### Memory Usage

- Bitmap buffer: 1024 bytes (supports up to 128x64)
- Static allocation (no heap usage)
- Loaded once at startup

### Fallback Behavior

If bitmap loading fails (file not found, invalid format, etc.):
- During init: Displays "Bitmap\nError"
- When calling `showJogo32h()`: Falls back to default Jogo logo

## API Usage

### From C++

```cpp
// Set text message
strcpy(jumperlessConfig.top_oled.startup_message, "Hello!");

// Set bitmap path
strcpy(jumperlessConfig.top_oled.startup_message, "/my_logo.bin");

// Clear (use default logo)
strcpy(jumperlessConfig.top_oled.startup_message, "");

// Reload display
oled.showJogo32h();
```

### From Config File

```ini
[top_oled]
# Text message
startup_message = My Text

# Bitmap file
startup_message = /logo.bin

# Default logo (empty)
startup_message = 
```

## Tips for Best Results

1. **Start with simple images**: High contrast, simple shapes work best
2. **Use vector graphics**: Convert from SVG for cleanest results
3. **Test different thresholds**: The default 128 may not be optimal for all images
4. **Consider the display size**: 128x32 is quite small, keep designs simple
5. **Transparency handling**: Transparent areas always become black (pixels off on OLED)
6. **Choose invert based on your image**:
   - **White text on transparent/black** → No `--invert` needed (low power)
   - **Black text on white background** → Use `--invert` (converts to low power)
7. **Power consideration**: Black pixels = off = lower power consumption on OLED

## Examples

See the `examples/oled_bitmaps/` directory for sample bitmap files and source images.

