# Font Characteristics System

## Overview

The font characteristics system allows per-family customization of **line spacing** and **clipping tolerance** for intelligent multi-line text scaling. This solves the problem where fonts with tight metrics (like Iosevka) were scaling down too aggressively.

## The Problem

**Before:**
- Iosevka has very tight glyph metrics (compact design)
- Standard line spacing calculation: `lineCount × lineHeight`
- Result: 2-line text with Iosevka at 12pt = ~28px height
- On 32px display: Too tall! → Scale down to 9pt → Text appears tiny ❌

**After:**
- Iosevka uses 0.85× line spacing multiplier (15% tighter)
- Allows 4px vertical clipping tolerance (for descenders)
- Result: 2-line text with Iosevka at 12pt = ~24px height + 4px tolerance = fits!
- Text stays at 12pt → Properly sized! ✅

## Font Characteristics Structure

```cpp
struct FontFamilyCharacteristics {
    FontFamily family;
    float lineSpacingMultiplier;     // Multiply lineHeight by this
    int8_t verticalClipTolerance;    // Pixels that can clip vertically
    int8_t horizontalClipTolerance;  // Pixels that can clip horizontally
};
```

### Parameters Explained

#### `lineSpacingMultiplier`
- **1.0** = Normal spacing (use font's native lineHeight)
- **0.85** = 15% tighter (for compact fonts like Iosevka)
- **0.95** = 5% tighter (for slightly compact fonts)
- **1.2** = 20% looser (for fonts that need extra breathing room)

Applied to: Line spacing in multi-line text

#### `verticalClipTolerance`
Allows the top/bottom of glyphs to extend beyond display edges without triggering scaling.

**Use cases:**
- Descenders (p, g, q, y) extending below baseline
- Tall ascenders (b, d, h, k, l) extending above
- Accent marks (é, ñ, ü) on capital letters

**Typical values:**
- **2px** - Standard tolerance for most fonts
- **4px** - Generous tolerance for tight fonts (Iosevka)
- **1px** - Minimal tolerance for tiny fonts (4-5pt)

#### `horizontalClipTolerance`
Allows left/right edges of glyphs to extend beyond display edges.

**Use cases:**
- Italic fonts with rightward slant
- Wide characters (W, M) in narrow displays
- Kerning pairs that overlap slightly

**Typical values:**
- **2px** - Standard tolerance
- **3px** - Generous tolerance for wide fonts
- **1px** - Minimal tolerance for tiny fonts

## Current Font Settings

```cpp
FontFamilyCharacteristics fontCharacteristics[] = {
    // {Family,                           Spacing, Vert, Horiz}
    { FONT_EUROSTILE,                    1.0,     2,    2 },  // Standard
    { FONT_JOKERMAN,                     1.0,     2,    2 },  // Standard
    { FONT_COMIC_SANS,                   1.0,     2,    2 },  // Standard
    { FONT_COURIER_NEW,                  1.0,     2,    2 },  // Standard
    { FONT_NEW_SCIENCE_MEDIUM,           1.0,     2,    2 },  // Standard
    { FONT_NEW_SCIENCE_MEDIUM_EXTENDED,  1.0,     2,    2 },  // Standard
    { FONT_ANDALE_MONO,                  1.0,     1,    1 },  // Tiny font
    { FONT_FREE_MONO,                    1.0,     1,    1 },  // Tiny font
    { FONT_IOSEVKA_REGULAR,              0.85,    4,    3 },  // ⭐ TIGHT METRICS
    { FONT_BERKELEY_MONO,                0.95,    3,    2 },  // Slightly tight
    { FONT_PRAGMATISM,                   1.0,     2,    2 },  // Standard
};
```

### Highlighted Fonts

**Iosevka Regular** - Most aggressive optimization:
- **0.85× spacing** - Lines pack 15% tighter
- **4px vertical tolerance** - Allows significant descender clipping
- **3px horizontal tolerance** - Allows wider glyphs
- Result: Can fit 12pt text where other fonts need 9pt

**Berkeley Mono** - Moderate optimization:
- **0.95× spacing** - Lines pack 5% tighter
- **3px vertical tolerance** - Slightly generous
- **2px horizontal tolerance** - Standard

## How It Works

### During Font Scaling

```cpp
// Check if multi-line text fits
FontMetrics metrics = getFontMetrics();
FontFamilyCharacteristics chars = getFontCharacteristics(currentFontFamily);

// Calculate adjusted line spacing
int adjustedLineHeight = (int)(metrics.lineHeight * chars.lineSpacingMultiplier);
int totalHeight = lineCount * adjustedLineHeight;

// Check fit WITH clipping tolerance
bool fits = (maxLineWidth <= (displayWidth + chars.horizontalClipTolerance) && 
             totalHeight <= (displayHeight + chars.verticalClipTolerance));
```

### During Rendering

```cpp
// Use adjusted line spacing when positioning lines
int16_t lineSpacing = (int16_t)(metrics.lineHeight * chars.lineSpacingMultiplier);

for (int i = 0; i < lineCount; i++) {
    int16_t lineY = firstLineBaseline + (i * lineSpacing);
    // ...
}
```

## Examples

### Example 1: Standard Font (Eurostile)

```
Text: "Top Rail\n1.50 V"
Font: Eurostile 12pt
Display: 128x32px

Calculations:
- lineHeight = 16px
- adjustedLineHeight = 16 × 1.0 = 16px
- totalHeight = 2 lines × 16px = 32px
- verticalTolerance = 2px
- fits? 32px <= (32px + 2px) = YES ✅

Result: Uses 12pt Eurostile
```

### Example 2: Tight Font (Iosevka)

```
Text: "Top Rail\n1.50 V"
Font: Iosevka 12pt
Display: 128x32px

Calculations:
- lineHeight = 16px
- adjustedLineHeight = 16 × 0.85 = 13.6px → 13px
- totalHeight = 2 lines × 13px = 26px
- verticalTolerance = 4px
- fits? 26px <= (32px + 4px) = YES ✅

Result: Uses 12pt Iosevka (without tolerance would have scaled to 9pt!)
```

### Example 3: Three Lines

```
Text: "GPIO 5\nInput\nHigh"
Font: Iosevka 12pt
Display: 128x32px

Calculations:
- lineHeight = 16px
- adjustedLineHeight = 16 × 0.85 = 13.6px → 13px
- totalHeight = 3 lines × 13px = 39px
- verticalTolerance = 4px
- fits? 39px <= (32px + 4px) = NO ❌

Try 11pt:
- lineHeight = 14px
- adjustedLineHeight = 14 × 0.85 = 11.9px → 11px
- totalHeight = 3 lines × 11px = 33px
- fits? 33px <= (32px + 4px) = YES ✅

Result: Uses 11pt Iosevka (scales down only 1 step)
```

## Tuning Guide

### If Text is Too Small
1. **Increase `lineSpacingMultiplier`** - Makes lines pack tighter
   - From 1.0 → 0.95 (5% tighter) or 0.9 (10% tighter)
2. **Increase `verticalClipTolerance`** - Allow more clipping
   - From 2px → 3px or 4px
3. **Check font metrics** - Font might have excessive built-in spacing

### If Text is Too Large (Clipped)
1. **Decrease `lineSpacingMultiplier`** - Give more space between lines
   - From 0.85 → 0.9 or 0.95
2. **Decrease `verticalClipTolerance`** - Scale down sooner
   - From 4px → 3px or 2px
3. **Reduce point size** - Use 11pt instead of 12pt

### If Text is Off-Center
1. **Check `topRowOffset`** in font structure
2. **Verify multi-line centering algorithm**
3. **Adjust `lineSpacingMultiplier`** for better visual balance

## Adding New Fonts

When adding a new font family:

1. **Test at 12pt with 2-line text** on your target display
2. **Measure if it fits** - does it clip or look cramped?
3. **Choose characteristics:**
   ```cpp
   // Compact font (like Iosevka):
   { FONT_MYFONT, 0.85, 4, 3 }
   
   // Standard font:
   { FONT_MYFONT, 1.0, 2, 2 }
   
   // Spacious font:
   { FONT_MYFONT, 1.05, 3, 3 }
   ```

4. **Add to `fontCharacteristics[]` array** in `oled.cpp`
5. **Test with highlighting** - Check voltage displays, DAC values, etc.

## Technical Notes

### Why Iosevka Needs Special Treatment

Iosevka is designed as a **narrow, space-efficient monospace font** for programming:
- Glyphs are compact horizontally
- Line height is minimal (tight vertical metrics)
- Descenders are shallow to save space
- Perfect for terminals, less ideal for small displays

Without characteristics system:
- Iosevka 12pt on 32px display → doesn't fit
- Scales down to 9pt → looks tiny and loses readability

With characteristics system:
- 15% tighter line spacing
- 4px clipping tolerance for descenders
- Result: Iosevka 12pt fits perfectly!

### Performance Impact

- **Minimal** - Lookup is O(1) array access (11 entries)
- **Multiplication** - One float multiply per multi-line text
- **Memory** - 132 bytes (11 × 12 bytes per entry)
- **Flash** - Stored in PROGMEM, zero RAM overhead

### Future Improvements

Consider adding:
1. **Dynamic adjustment** based on display size
2. **Per-size characteristics** (different settings for 9pt vs 15pt)
3. **User-configurable tolerances** in config system
4. **Automatic metrics analysis** from font files

## Summary

The font characteristics system allows **per-family fine-tuning** of multi-line text scaling:

- **Iosevka**: 0.85× spacing, 4px tolerance → Stays large
- **Standard fonts**: 1.0× spacing, 2px tolerance → Normal behavior  
- **Future fonts**: Easily customizable without code changes

Result: **Better text sizing** across all fonts, especially compact designs like Iosevka! 🎉

