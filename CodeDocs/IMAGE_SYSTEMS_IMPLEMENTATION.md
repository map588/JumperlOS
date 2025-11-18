# Image Systems Implementation Summary

Complete implementation of image management for both OLED and breadboard LEDs.

## ✅ What Was Created

### 1. **OLED Images App** 
New interactive app to browse and display OLED images from filesystem.

**Files Created:**
- `src/ImagesApp.cpp` - Main app implementation
- `src/ImagesApp.h` - Header file

**Files Modified:**
- `src/Apps.h` - Added ImagesApp include
- `src/Apps.cpp` - Added "OLED Images" app to menu (index 14)

**Features:**
- Browse all `.bin` files in `/images` folder
- Display images on OLED with auto-centering
- Rotary encoder navigation
- Show filename and dimensions
- Support for both header and raw binary formats

**Usage:**
```
Menu → Apps → OLED Images
Rotate: Navigate | Button: Info | Long Press: Exit
```

### 2. **Image Conversion Tools**

#### A. OLED Image Converters

**`scripts/image_to_oled_bitmap.py`** (ENHANCED)
- Fixed transparency handling (composite on black background)
- Fixed bit ordering (MSB-first for Adafruit GFX)
- Added `--invert` flag for color swapping
- Better error messages and validation

**`scripts/cpp_array_to_bin.py`** (NEW)
- Convert img2cpp output to binary
- Auto-detects array dimensions
- Strips existing headers if present
- Validates data integrity

**`scripts/extract_cpp_bitmap.py`** (NEW)
- Extract bitmap arrays from C++ source files
- Finds and converts embedded bitmaps
- Supports various declaration formats
- Auto-adds dimension header

#### B. Breadboard LED Converter

**`scripts/image_to_breadboard.py`** (NEW)
- Convert images to RGB565 format
- Handles 30×14 LED layout with skip lines
- ASCII preview option
- Animation support with multiple frames
- Little-endian uint16 output

**Features:**
- Single image conversion
- Multi-frame animations
- Configurable frame delay
- Preview before saving

### 3. **Documentation**

**`IMAGE_SYSTEMS_GUIDE.md`** (NEW)
Complete guide covering:
- OLED image formats and creation
- Breadboard LED formats and creation
- File organization best practices
- All conversion tools with examples
- Technical specifications
- Troubleshooting guide

**`scripts/IMAGES_README.txt`** (NEW)
Quick reference for `/images` folder:
- Folder structure
- File formats
- Quick usage examples
- Basic tips

### 4. **Example Images**

**`scripts/example_images/oled/`**
- `jogo32h.bin` - Original Jumperless logo (extracted from source)
- `jumperless_text.bin` - "Jumperless" text logo
- `jogotextInv.bin` - Inverted version

## 📁 File Organization

### Recommended Filesystem Structure
```
/images/
├── oled/
│   ├── jogo32h.bin
│   ├── jumperless_text.bin
│   └── *.bin (user images)
├── breadboard/
│   ├── startup_frame0.bin
│   ├── startup_frame1.bin
│   └── *.bin (user images)
└── README.txt
```

## 🔧 How To Use

### OLED Images

#### 1. Create from Photo/PNG
```bash
cd scripts
python3 image_to_oled_bitmap.py my_logo.png ../images/oled/my_logo.bin --width 128 --height 32
```

#### 2. Convert from img2cpp
```bash
# Go to https://javl.github.io/image2cpp/
# Generate code, save as bitmap.h
python3 cpp_array_to_bin.py bitmap.h ../images/oled/bitmap.bin
```

#### 3. Extract from C++ Source
```bash
python3 extract_cpp_bitmap.py ../src/oled.cpp ../images/oled/jogo.bin --array-name jogo32h
```

#### 4. View in App
- Run "OLED Images" from Apps menu
- Use rotary encoder to browse
- Images auto-centered and displayed

#### 5. Set as Startup
```ini
[top_oled]
startup_message = logo.bin
```

### Breadboard LED Images

#### 1. Create Single Image
```bash
python3 image_to_breadboard.py logo.png ../images/breadboard/logo.bin --preview
```

#### 2. Create Animation
```bash
python3 image_to_breadboard.py frame*.png ../images/breadboard/anim.bin --animation --delay 100
```

#### 3. Display in Code
```cpp
// Single image (to be implemented)
displayBreadboardImage("/images/breadboard/logo.bin");

// Animation (to be implemented)
playBreadboardAnimation("/images/breadboard/anim.bin");
```

## 🎨 Format Details

### OLED Format

**Monochrome Bitmap (Adafruit GFX Compatible)**
- 1 bit per pixel (1=white/on, 0=black/off)
- MSB-first: bit 7 = leftmost pixel
- Row-major order
- Optional 4-byte header (width, height as uint16 LE)

**Common Sizes:**
- 128×32 = 512 bytes
- 128×64 = 1024 bytes
- 64×32 = 256 bytes

### Breadboard LED Format

**RGB565 Color**
- 16-bit per pixel (5R, 6G, 5B)
- Little-endian uint16
- 30 columns × 14 rows = 840 bytes
- Animation: 4-byte header + frames

**Layout:**
- Physical: 21 rows (some skipped)
- Display: 14 rows visible
- Mapping handled by skip lines array

## 💾 Space Efficiency

**Before (C Arrays):**
```cpp
const unsigned char bitmap[] PROGMEM = {
    0x00, 0x00, 0x00, ... // ~2KB source code
};
```

**After (Binary Files):**
```
bitmap.bin: 512 bytes on filesystem
```

**Savings:** ~75% reduction in storage!
- Multiple images can be stored
- No code recompilation needed
- Easy to update via filesystem

## 🚀 Integration

### Add to Your Code

```cpp
#include "ImagesApp.h"

// Use in menu or standalone
imagesApp(true);  // With wait for enter
imagesApp(false); // Immediate start
```

### Access from Config

```ini
[top_oled]
startup_message = mylogo.bin  # Auto-detected as file
# OR
startup_message = /images/oled/mylogo.bin  # Explicit path
```

## 🔍 Technical Implementation

### OLED Image Loading

1. **File Detection:** Checks for `.bin` extension and file existence
2. **Format Detection:** Tries header format, falls back to raw
3. **Size Validation:** Ensures dimensions ≤128×64
4. **Memory:** Allocates buffer, loads, displays, frees
5. **Display:** Auto-centers using `drawBitmap()`

### Breadboard LED Encoding

1. **Resize:** Scale to 30×14 pixels
2. **Convert:** RGB888 → RGB565 per pixel
3. **Layout:** Apply skip lines mapping
4. **Output:** Little-endian uint16 array

### Animation Format

```
Header (4 bytes):
  uint16 frame_count (LE)
  uint16 delay_ms (LE)

Frames:
  frame_0: 840 bytes (30×14×2)
  frame_1: 840 bytes
  ...
```

## 🎯 Future Enhancements

### Planned Features
- [ ] Breadboard image display functions
- [ ] Animation player app
- [ ] Slideshow mode for OLED
- [ ] Terminal preview for images
- [ ] In-app image cropping/editing
- [ ] GIF to animation converter
- [ ] Dithering options

### Performance Optimizations
- [ ] Preload animations to RAM
- [ ] Async loading for large animations
- [ ] Image caching system
- [ ] Compressed image format

## 📋 Testing Checklist

### OLED Images
- [x] Create from PNG/JPG
- [x] Convert from img2cpp
- [x] Extract from C++ source
- [x] Display in app
- [x] Transparency handling
- [x] Color inversion
- [x] Auto-centering
- [x] Multiple formats supported

### Breadboard LEDs
- [x] Single image conversion
- [x] Animation creation
- [x] RGB565 encoding
- [x] Skip lines mapping
- [ ] Display function (to be implemented)
- [ ] Animation playback (to be implemented)

### Tools
- [x] All scripts executable
- [x] Error handling
- [x] Help text
- [x] Examples in docs

## 📝 Summary

This implementation provides:

✅ **Complete OLED image system**
- Browse, display, and manage OLED images from filesystem
- Multiple input formats (PNG, C arrays, extracted from source)
- Flexible output (with/without headers)
- Interactive viewer app

✅ **Breadboard LED image system**
- Convert images to RGB565 format
- Animation support with configurable delays
- Proper layout mapping for physical LEDs
- Ready for integration with display functions

✅ **Comprehensive tooling**
- 4 Python conversion scripts
- Extensive documentation
- Example images included
- Integration with existing systems

✅ **Space efficient**
- Binary storage vs C arrays
- Multiple images on filesystem
- No code recompilation needed
- Easy updates and management

## 🎓 Learning Resources

- **Main Guide:** `IMAGE_SYSTEMS_GUIDE.md`
- **OLED Startup:** `OLED_CUSTOM_STARTUP.md`
- **Bitmap Conversion:** `scripts/README_BITMAP_CONVERTER.md`
- **Quick Reference:** `scripts/IMAGES_README.txt`

## 🙏 Credits

- Adafruit GFX library for bitmap format
- PIL/Pillow for image processing
- img2cpp tool compatibility

