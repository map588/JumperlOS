# FontManager Implementation Summary

## Overview

Implemented a lightweight **FontManager** system that provides **granular font scaling** (5pt-15pt) for smooth text sizing on the OLED display. The system intelligently scales text down through all available point sizes to fit on screen, eliminating the jarring 8pt→12pt jumps of the old system.

## Key Features

### 🎯 **Granular Font Scaling**
- **Before**: Only 2 sizes (textSize 1→8pt, textSize 2→12pt)
- **After**: Smooth scaling through multiple sizes:
  - **Pragmatism**: 5, 6, 7, 8, 9, 10, 11, 12pt (8 sizes)
  - **Iosevka Regular**: 9, 11, 12, 13, 14, 15pt (6 sizes)
  - Other fonts: 2-3 sizes each

### 💾 **Memory Efficient**
- All font data stored in **PROGMEM (flash)** - zero RAM overhead
- Font lookup uses simple iteration (35 fonts total) - negligible CPU cost
- No dynamic allocation - fully static structures

### 🔄 **Backwards Compatible**
- Existing `setFontForSize(family, textSize)` still works
- Old textSize 1/2 system automatically converts to point sizes
- All existing code continues to function unchanged

### 🧠 **Intelligent Text Fitting**
When text is too large to fit:
1. Starts at desired point size (e.g., 12pt)
2. Tries each smaller size: 11pt → 10pt → 9pt → 8pt → 7pt → 6pt → 5pt
3. Uses the **largest font that fits**
4. Falls back to wrapping at minimum size (5pt)

## API Changes

### New Methods

#### `oled::setFontPointSize(FontFamily family, uint8_t pointSize)`
Direct point size control for fine-grained font selection:
```cpp
oled.setFontPointSize(FONT_PRAGMATISM, 10);  // Use 10pt Pragmatism
```

#### `FontManager::getFontForPointSize(FontFamily family, uint8_t desiredPointSize)`
Returns font index for the closest available point size:
```cpp
int fontIdx = FontManager::getFontForPointSize(FONT_IOSEVKA_REGULAR, 13);
oled.setFont(fontIdx);
```

#### `FontManager::findBestFitPointSize(FontFamily family, const char* text, int16_t maxWidth)`
Finds the largest point size that fits the text within maxWidth:
```cpp
uint8_t bestPt = FontManager::findBestFitPointSize(
    FONT_PRAGMATISM, 
    "Long text here", 
    displayWidth,
    15,  // max point size
    5    // min point size
);
```

#### `FontManager::textSizeToPointSize(int textSize)`
Converts old textSize system to point sizes (backwards compatibility):
```cpp
uint8_t pt = FontManager::textSizeToPointSize(2);  // Returns 12
```

### Modified Behavior

#### `clearPrintShow()` - Now Scales Smoothly
```cpp
// OLD BEHAVIOR:
// Text too long at 12pt → immediately jumps to 8pt
oled.clearPrintShow("Some long text", 2);

// NEW BEHAVIOR: 
// Text too long at 12pt → tries 11pt, 10pt, 9pt, 8pt...
// Uses the LARGEST size that fits!
oled.clearPrintShow("Some long text", 2);
```

## Font Coverage

### Fonts with Multiple Sizes (Granular Scaling)

**Pragmatism** (8 sizes - best for smooth scaling):
- 5pt, 6pt, 7pt, 8pt, 9pt, 10pt, 11pt, 12pt

**Iosevka Regular** (6 sizes - good for code/terminal):
- 9pt, 11pt, 12pt, 13pt, 14pt, 15pt

### Standard Fonts (2 sizes each)
- Eurostile: 8pt, 12pt
- Jokerman: 8pt, 12pt
- Comic Sans: 8pt, 12pt
- Courier New: 8pt, 12pt
- New Science: 8pt, 12pt
- Berkeley Mono: 8pt, 12pt

### Small Fixed Fonts
- Andale Mono: 5pt
- Free Mono: 4pt, 5pt
- Ubuntu: 5pt
- DotGothic: 4pt
- EnvyCode: 5pt

## Technical Details

### Font Structure
```cpp
struct font {
    const GFXfont* font;
    const char* shortName;
    const char* longName;
    int16_t topRowOffset;
    FontFamily family;
    uint8_t pointSize;     // NEW: Actual point size for granular scaling
};
```

### FontManager Class
```cpp
class FontManager {
public:
    // Find best font for family and desired point size
    static int getFontForPointSize(FontFamily family, uint8_t desiredPointSize);
    
    // Convert old textSize (1/2) to point size for backwards compatibility
    static uint8_t textSizeToPointSize(int textSize);
    
    // Find largest font that fits given text width
    static uint8_t findBestFitPointSize(FontFamily family, const char* text, 
                                        int16_t maxWidth, uint8_t maxPointSize = 15, 
                                        uint8_t minPointSize = 5);
    
    // Get all available point sizes for a font family
    static void getAvailableSizes(FontFamily family, uint8_t* sizes, int* count);
};
```

### Memory Usage
- **Flash**: Added ~40KB for new font bitmaps (plenty of space - only 8.9% flash used)
- **RAM**: +1 byte per oled instance (`currentPointSize` tracking)
- **No heap allocations** - all data in flash or stack

## Example Usage

### Basic Text Display
```cpp
// Automatic scaling - will try 12pt, 11pt, 10pt... until it fits
oled.clearPrintShow("Hello World!", 2);

// Direct point size control
oled.setFontPointSize(FONT_PRAGMATISM, 9);
oled.print("9pt text");
```

### Advanced - Find Best Fit
```cpp
// Find the largest font size that fits
uint8_t bestSize = FontManager::findBestFitPointSize(
    FONT_PRAGMATISM,
    "This is a long message",
    128,  // display width
    12,   // max 12pt
    5     // min 5pt
);

oled.setFontPointSize(FONT_PRAGMATISM, bestSize);
oled.clearPrintShow("This is a long message", 2);
```

### Multi-Line Text
```cpp
// Multi-line text automatically uses optimal sizing
oled.clearPrintShow("Line 1\nLine 2\nLine 3", 2);
```

## Bug Fixes Included

### Fixed: `std::vector<String>` Compatibility Issue
- **Problem**: displayMultiLineText() used `std::vector<String>` which doesn't work with Arduino String
- **Solution**: Replaced with fixed-size array `String lines[8]` - supports up to 8 lines
- This was a pre-existing bug that caused compilation errors

## Testing Recommendations

1. **Test long text wrapping**: Try text that previously jumped from 12pt to 8pt
2. **Test different fonts**: Verify Pragmatism and Iosevka scale smoothly
3. **Test multi-line**: Ensure line breaks work correctly with new sizing
4. **Test display sizes**: Verify works on different display configurations

## Performance Impact

- **Minimal**: Font lookup is O(n) where n=35 fonts, negligible on modern MCU
- **Flash fetch**: Fonts stored in PROGMEM, fetched only when needed
- **No stack issues**: All allocations are static or small stack variables

## Future Enhancements

### Generate More Font Sizes (Optional)
Use the included `TTF2GFX` tool to generate additional sizes:
```bash
cd src/fonts/TTF2GFX
./ttf2gfx.sh TTFfonts/Pragmatism.ttf 14 > ../Pragmatism14pt7b.h
```

Then add to `oled.h`:
```cpp
#include "fonts/Pragmatism14pt7b.h"
```

And update `fontList[]` in `oled.cpp`:
```cpp
{ &Pragmatism14pt7b, "Pragmtsm", "Pragmatism", 0, FONT_PRAGMATISM, 14 },
```

### Font Families to Consider
Current fonts in `/src/fonts/TTF2GFX/TTFfonts/`:
- ✅ Pragmatism (renamed from Pragmata Pro)
- ✅ Iosevka SS08
- ✅ Berkeley Mono
- Eurostile
- Jokerman
- Comic Sans MS
- Courier New
- New Science
- FiraCode
- SpaceMono Nerd Font

## File Changes

### Modified Files
- `/src/oled.h` - Added FontManager class, updated includes, added `currentPointSize` tracking
- `/src/oled.cpp` - Implemented FontManager, updated font structures, improved text sizing logic

### New Font Includes
Added to `oled.h`:
- Pragmatism: 6pt, 7pt, 9pt, 10pt, 11pt (previously only 5pt, 8pt, 12pt)
- Iosevka: 12pt, 13pt, 14pt, 15pt (previously only 9pt, 11pt)

## Compilation Results

✅ **Build Status**: SUCCESS
- RAM Usage: 78.6% (412,096 / 524,288 bytes)
- Flash Usage: 8.9% (1,125,612 / 12,578,816 bytes)
- No compilation errors or warnings related to font changes

## Summary

The new FontManager system provides **smooth, intelligent font scaling** while maintaining **backwards compatibility** and **minimal memory overhead**. Text now scales gracefully through multiple sizes instead of jumping between extremes, significantly improving readability and user experience on the OLED display.

Key benefit: **Text that previously jumped from 12pt to 8pt now smoothly scales through 11pt, 10pt, 9pt, etc., using the largest size that fits.**

