# Colors Module

## Overview

The `Colors` module (`Colors.h` / `Colors.cpp`) provides centralized color management for the entire Jumperless project. It consolidates all color handling that was previously scattered across `LEDs.cpp`, `Graphics.cpp`, and `WokwiParser.cpp`.

## Features

### 1. Wokwi Color Support

Complete support for all [Wokwi wire colors](https://docs.wokwi.com/guides/diagram-editor) with proper RGB mappings:

| Wokwi Color | Keyboard | RGB Value | Internal Name | Notes |
|------------|----------|-----------|---------------|-------|
| black      | 0        | 0x000000  | black         | |
| brown      | 1        | 0xA52A2A  | brown         | |
| red        | 2        | 0xFF0000  | red           | |
| orange     | 3        | 0xFFA500  | orange        | |
| gold       | 4        | 0xFFD700  | amber         | Maps to amber internally |
| green      | 5        | 0x00FF00  | green         | Pure green |
| blue       | 6        | 0x0000FF  | blue          | |
| violet     | 7        | 0x8A2BE2  | violet        | Blue-violet |
| gray       | 8        | 0x808080  | grey          | |
| white      | 9        | 0xFFFFFF  | white         | |
| cyan       | C        | 0x00FFFF  | cyan          | |
| limegreen  | L        | 0x32CD32  | chartreuse    | Yellow-green |
| magenta    | M        | 0xFF00FF  | magenta       | |
| purple     | P        | 0x800080  | purple        | Different from violet |
| yellow     | Y        | 0xFFFF00  | yellow        | |

**Key Fixes:**
- **Limegreen** now correctly maps to 0x32CD32 (not cyan!)
- All Wokwi colors map to **unique** internal colors
- Gold properly maps to amber for consistency

### 2. Internal Color Palette

Extended color palette with HSV ranges for intelligent color matching:

| Color      | RGB      | Hue Range  | VT100 | Description |
|-----------|----------|------------|-------|-------------|
| red       | 0xFF0000 | 253-12     | 196   | Wraps around 0° |
| orange    | 0xFFA500 | 13-28      | 208   | |
| amber     | 0xFFBF00 | 29-35      | 214   | Gold/amber |
| yellow    | 0xFFFF00 | 36-60      | 226   | |
| chartreuse| 0x7FFF00 | 61-72      | 154   | Yellow-green |
| green     | 0x00FF00 | 73-94      | 82    | |
| seafoam   | 0x2E8B57 | 95-109     | 84    | |
| cyan      | 0x00FFFF | 110-135    | 86    | |
| blue      | 0x0000FF | 136-164    | 33    | |
| royal blue| 0x4169E1 | 165-175    | 27    | |
| indigo    | 0x8A2BE2 | 176-190    | 21    | |
| violet    | 0x800080 | 191-205    | 57    | |
| purple    | 0x800080 | 206-215    | 12    | |
| pink      | 0xFFC0CB | 216-235    | 164   | |
| magenta   | 0xFF00FF | 236-252    | 198   | |
| brown     | 0xA52A2A | -          | 130   | Special case |
| white     | 0xFFFFFF | -          | 15    | Special case |
| black     | 0x000000 | -          | 0     | Special case |
| grey      | 0x808080 | -          | 8     | Special case |

### 3. Key Functions

#### Wokwi Color Conversion
```cpp
uint32_t wokwiColorToRGB(const String& colorName);
String wokwiColorToInternalName(const String& wokwiColor);
```

#### Color Manipulation
```cpp
uint32_t shiftColorHue(uint32_t baseColor, int shiftAmount);
uint32_t blendColors(uint32_t color1, uint32_t color2, float ratio);
```

#### Color Naming
```cpp
char* colorToName(uint32_t color, int length);
char* colorToName(rgbColor color, int length);
char* colorToName(int hue, int length);
int closestPaletteHueIdx(int hue);
```

#### Terminal Colors
```cpp
int colorToVT100(uint32_t color, int colorDepth);
int colorToAnsi(uint32_t color);
void changeTerminalColor(int termColor, bool flush, Stream *stream);
void cycleTerminalColor(bool reset, float step, bool flush, Stream *stream, int startColorIndex, int bright);
```

## Usage Examples

### Parsing Wokwi Colors
```cpp
#include "Colors.h"

// Convert Wokwi color name to RGB
uint32_t red = wokwiColorToRGB("red");        // 0xFF0000
uint32_t lime = wokwiColorToRGB("limegreen"); // 0x32CD32 (correct!)

// Get internal name for display
String name = wokwiColorToInternalName("limegreen"); // "chartreuse"
```

### Color Manipulation
```cpp
// Shift hue to create variations
uint32_t baseRed = 0xFF0000;
uint32_t shiftedRed = shiftColorHue(baseRed, 20); // Slightly orange-red

// Blend colors
uint32_t blend = blendColors(0xFF0000, 0x00FF00, 0.5); // Yellow
```

### Terminal Output
```cpp
// Change terminal color
changeTerminalColor(colorToVT100(0xFF0000, 256), true, &Serial);
Serial.println("This is red text!");
changeTerminalColor(-1, true, &Serial); // Reset
```

### Color Naming
```cpp
// Get human-readable names
char* name1 = colorToName(0xFF0000, -1);  // "red" (trimmed)
char* name2 = colorToName(0x32CD32, 10);  // "chartreuse" (padded)
```

## Integration

### Files Modified
- **Created:** `Colors.h`, `Colors.cpp`
- **Modified:** `WokwiParser.cpp` - Now uses Colors module
- **Removed duplicates from:** `LEDs.cpp`, `Graphics.cpp` (functions moved to Colors.cpp)

### Include Order
```cpp
#include "Colors.h"  // Include this for all color operations
```

## Benefits

1. **Centralized Management:** All color code in one place
2. **Wokwi Compatibility:** Perfect mapping of all Wokwi colors
3. **No Duplicates:** Single source of truth for color handling
4. **Extensible:** Easy to add new color palettes or mappings
5. **Type Safety:** Clear function signatures
6. **Documentation:** Comprehensive inline docs

## Testing

### Verify Wokwi Color Mappings
```cpp
// All 15 Wokwi colors should map to unique values
for (int i = 0; i < wokwiColorCount; i++) {
    Serial.printf("%s -> 0x%06X (%s)\n", 
        wokwiColors[i].name,
        wokwiColors[i].rgb,
        wokwiColors[i].internalName);
}
```

### Test Color Uniqueness
```cpp
// Verify no RGB collisions
std::set<uint32_t> uniqueColors;
for (int i = 0; i < wokwiColorCount; i++) {
    uniqueColors.insert(wokwiColors[i].rgb);
}
// uniqueColors.size() should equal wokwiColorCount (minus aliases)
```

## Migration Notes

### Before (WokwiParser.cpp)
```cpp
uint32_t wokwiColorToRGB(const String& colorName) {
    // ...duplicate implementation...
}
```

### After
```cpp
#include "Colors.h"
// Use centralized function - no local implementation needed
```

### Breaking Changes
- None! All function signatures remain the same
- Code using old LEDs.cpp functions will work unchanged
- Added new functions for Wokwi support

## Future Enhancements

1. **Color Themes:** Support for light/dark mode color schemes
2. **Custom Palettes:** User-defined color mappings
3. **Color Validation:** Range checking and warnings
4. **Interpolation:** Smooth color gradients
5. **Accessibility:** High-contrast mode support

## References

- [Wokwi Diagram Editor Documentation](https://docs.wokwi.com/guides/diagram-editor)
- [VT100 Terminal Colors](https://en.wikipedia.org/wiki/ANSI_escape_code)
- RGB Color Space: https://en.wikipedia.org/wiki/RGB_color_model

