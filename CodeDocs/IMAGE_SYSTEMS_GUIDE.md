
# Jumperless Image Systems Guide

Complete guide for managing images on both the OLED display and breadboard LEDs.

## Table of Contents

1. [OLED Images](#oled-images)
2. [Breadboard LED Images](#breadboard-led-images)
3. [File Organization](#file-organization)
4. [Conversion Tools](#conversion-tools)
5. [Apps](#apps)

---

## OLED Images

### Format

OLED images use monochrome bitmap format (Adafruit GFX compatible):
- **1 bit per pixel**: 1 = white (on), 0 = black (off)
- **MSB-first bit ordering**: bit 7 = leftmost pixel in each byte
- **Row-major order**: left→right, top→bottom

### File Formats Supported

#### 1. Custom Format with Header (Recommended)
```
Bytes 0-1: Width (16-bit little-endian)
Bytes 2-3: Height (16-bit little-endian)
Bytes 4+:  Bitmap data
```

**Example:** 128x32 image = 4 byte header + 512 bytes data = 516 bytes total

#### 2. Raw Binary (Auto-detected)
Just the bitmap data with no header. System auto-detects dimensions:
- **512 bytes** → 128×32
- **1024 bytes** → 128×64
- **256 bytes** → 64×32

### Creating OLED Images

#### From PNG/JPG

```bash
# Basic conversion
python3 scripts/image_to_oled_bitmap.py logo.png /images/logo.bin

# With options
python3 scripts/image_to_oled_bitmap.py logo.png /images/logo.bin \
    --width 128 \
    --height 32 \
    --threshold 128 \
    --invert
```

#### From img2cpp Output

```bash
# 1. Generate C array at https://javl.github.io/image2cpp/
# 2. Save as .h file
# 3. Convert to binary:
python3 scripts/cpp_array_to_bin.py bitmap.h /images/bitmap.bin
```

#### Extract from C++ Source

```bash
# Extract embedded bitmap arrays
python3 scripts/extract_cpp_bitmap.py src/oled.cpp /images/jogo.bin --array-name jogo32h
```

### Using OLED Images

#### As Startup Image

```ini
[top_oled]
startup_message = logo.bin    # Relative path in /images
# OR
startup_message = /images/logo.bin   # Absolute path
```

#### In Images App

1. Place `.bin` files in `/images` folder
2. Run "OLED Images" app from menu
3. Use rotary encoder to browse
4. Button to show info, long press to exit

---

## Breadboard LED Images

### Format

Breadboard LEDs use RGB565 color format:
- **16-bit color**: 5 bits red, 6 bits green, 5 bits blue
- **Little-endian**: LSB first
- **Dimensions**: 30 columns × 14 rows (visible LEDs)
- **Physical layout**: 21 rows total, some skipped

### Pixel Mapping

```
Physical Row  | Status  | Display Row
--------------|---------|-------------
0             | Skip    | -
1             | Display | 0
2             | Display | 1
3             | Skip    | -
4             | Skip    | -
5-9           | Display | 2-6
10            | Skip    | -
11            | Skip    | -
12-16         | Display | 7-11
17            | Skip    | -
18            | Skip    | -
19-20         | Display | 12-13
```

### File Formats

#### Single Image
```
Total size: 840 bytes (30 × 14 × 2 bytes)
Format: Array of RGB565 pixels (little-endian uint16)
```

#### Animation
```
Header: 4 bytes
  - uint16: Number of frames
  - uint16: Frame delay (milliseconds)
Frames: Each frame is 840 bytes
```

### Creating Breadboard LED Images

#### Single Image

```bash
python3 scripts/image_to_breadboard.py logo.png /images/breadboard/logo.bin --preview
```

#### Animation

```bash
# From multiple frames
python3 scripts/image_to_breadboard.py frame*.png /images/breadboard/animation.bin \
    --animation \
    --delay 100
```

### RGB565 Color Encoding

```c
// Convert RGB888 to RGB565
uint16_t rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t r5 = (r >> 3) & 0x1F;  // 5 bits
    uint8_t g6 = (g >> 2) & 0x3F;  // 6 bits
    uint8_t b5 = (b >> 3) & 0x1F;  // 5 bits
    return (r5 << 11) | (g6 << 5) | b5;
}
```

---

## File Organization

### Recommended Structure

```
/images/
├── oled/
│   ├── jogo.bin              # Original logo
│   ├── jumperless_text.bin   # Text logo
│   ├── startup.bin           # Custom startup
│   └── icon1.bin             # Other images
├── breadboard/
│   ├── startup_frame0.bin    # Startup animation frames
│   ├── startup_frame1.bin
│   ├── startup_frame2.bin
│   ├── startup_anim.bin      # Complete animation
│   └── custom_image.bin      # Custom images
└── README.txt                # Usage notes
```

### File Naming Conventions

- **OLED images**: `{name}.bin` (e.g., `logo.bin`)
- **Breadboard single**: `{name}.bin` (e.g., `splash.bin`)
- **Breadboard animation**: `{name}_anim.bin` (e.g., `boot_anim.bin`)
- **Animation frames**: `{name}_frame{N}.bin` (e.g., `boot_frame0.bin`)

---

## Conversion Tools

### Tool Summary

| Tool | Purpose | Input | Output |
|------|---------|-------|--------|
| `image_to_oled_bitmap.py` | Create OLED images | PNG/JPG | .bin (OLED) |
| `cpp_array_to_bin.py` | Convert C arrays | .h/.cpp | .bin (OLED) |
| `extract_cpp_bitmap.py` | Extract from source | .cpp | .bin (OLED) |
| `image_to_breadboard.py` | Create breadboard images | PNG/JPG | .bin (RGB565) |

### Quick Reference

```bash
# OLED: PNG → Binary
python3 scripts/image_to_oled_bitmap.py input.png output.bin

# OLED: C Array → Binary
python3 scripts/cpp_array_to_bin.py input.h output.bin

# OLED: Extract from C++
python3 scripts/extract_cpp_bitmap.py src/oled.cpp output.bin --array-name jogo32h

# Breadboard: PNG → Binary
python3 scripts/image_to_breadboard.py input.png output.bin --preview

# Breadboard: Create Animation
python3 scripts/image_to_breadboard.py frame*.png animation.bin --animation --delay 100
```

---

## Apps

### OLED Images App

**Location:** Menu → Apps → OLED Images

**Features:**
- Browse all `.bin` files in `/images` folder
- Display images on OLED
- Show dimensions and filename
- Navigate with rotary encoder

**Controls:**
- **Rotate**: Browse images
- **Button**: Show info
- **Long Press**: Exit

### Breadboard Image Display (Future)

Custom function to display breadboard images:

```cpp
void displayBreadboardImage(const char* filename);
void playBreadboardAnimation(const char* filename);
```

---

## Technical Details

### OLED Display Specs
- **Resolution**: 128×32 or 128×64 (configurable)
- **Color Depth**: 1-bit (monochrome)
- **Controller**: SSD1306
- **Interface**: I2C

### Breadboard LED Specs
- **Total LEDs**: 420 (30 columns × 14 rows)
- **Color**: RGB (individually addressable)
- **Controller**: WS2812/SK6812
- **Color Format**: RGB565 (16-bit)

### Memory Considerations

**OLED:**
- 128×32 = 512 bytes per image
- Static buffer: 1024 bytes (supports up to 128×64)

**Breadboard:**
- 30×14×2 = 840 bytes per frame
- Animation: 4 bytes header + (840 × frames)

**Space Efficiency:**
- Raw binary is much smaller than C arrays
- 512-byte bitmap vs ~2KB C array source code
- Enables storing multiple images on filesystem

---

## Examples

### Example 1: Custom Startup Logo

```bash
# 1. Create image
python3 scripts/image_to_oled_bitmap.py my_logo.png /images/startup.bin --width 128 --height 32

# 2. Copy to Jumperless filesystem
# (via USB mass storage mode)

# 3. Update config
echo "startup_message = startup.bin" >> /config.txt

# 4. Reboot
```

### Example 2: Breadboard Animation

```bash
# 1. Create frame images (frame0.png, frame1.png, ...)

# 2. Convert to animation
python3 scripts/image_to_breadboard.py frame*.png /images/breadboard/boot.bin --animation --delay 50

# 3. Display in code
displayBreadboardAnimation("/images/breadboard/boot.bin");
```

### Example 3: Extract and Use Built-in Logo

```bash
# 1. Extract jogo logo
cd scripts
python3 extract_cpp_bitmap.py ../src/oled.cpp jogo.bin --array-name jogo32h

# 2. Copy to images folder
cp jogo.bin ../path/to/jumperless/images/

# 3. View in Images app or use as startup
```

---

## Troubleshooting

### OLED Images

**"Bitmap Error" displayed:**
- Check file exists in `/images`
- Verify file format (use conversion scripts)
- Check dimensions ≤128×64

**Image looks wrong:**
- Try `--invert` flag when converting
- Adjust `--threshold` (try 100-200)
- Ensure white-on-black for low power

**Can't find image in app:**
- Ensure filename ends with `.bin`
- Check file is in `/images` folder
- Verify filesystem mounted correctly

### Breadboard Images

**Wrong colors:**
- Verify RGB565 little-endian format
- Check color space conversion
- Test with simple solid colors

**Layout mismatch:**
- Confirm using 30×14 dimensions
- Check skip lines array matches hardware
- Verify pixel mapping

---

## Performance Tips

1. **OLED:** Use raw 512-byte format for fastest loading
2. **Breadboard:** Preload animations into RAM for smooth playback
3. **Storage:** Binary format is 60% smaller than C arrays
4. **Memory:** Delete[] buffers after use to prevent fragmentation

---

## Future Enhancements

- [ ] Breadboard animation player app
- [ ] Image preview in terminal
- [ ] Animated GIF support
- [ ] Dithering options for OLED
- [ ] Color palette optimization for breadboard
- [ ] Image cropping/resizing in-app
- [ ] Slideshow mode

---

## See Also

- `OLED_CUSTOM_STARTUP.md` - OLED startup configuration
- `scripts/README_BITMAP_CONVERTER.md` - Bitmap conversion details
- API documentation for `ImagesApp.cpp`

