# Font Metrics Tuning Guide

## Understanding GFXfont Structure

The Adafruit GFX font format consists of three components:

### 1. GFXglyph Structure (Per Character)
```cpp
typedef struct {
    uint16_t bitmapOffset;  // Pointer into GFXfont->bitmap
    uint8_t  width;         // Bitmap dimensions in pixels
    uint8_t  height;        // Bitmap dimensions in pixels
    uint8_t  xAdvance;      // Distance to advance cursor (X axis)
    int8_t   xOffset;       // X offset from cursor position
    int8_t   yOffset;       // Y offset from cursor position  
} GFXglyph;
```

### 2. GFXfont Structure
```cpp
typedef struct {
    uint8_t  *bitmap;       // Pointer to bitmap data
    GFXglyph *glyph;        // Pointer to glyph array
    uint8_t   first;        // First ASCII character (usually 0x20)
    uint8_t   last;         // Last ASCII character (usually 0x7E)
    uint8_t   yAdvance;     // Line spacing (newline distance)
} GFXfont;
```

## Example: IosevkaSS08_Regular11pt7b.h

```cpp
// Line 134: Space character
{     0,   1,   1,  11,    0,    0 },   // 0x20 ' '
//    ^    ^    ^    ^     ^     ^
//    |    |    |    |     |     yOffset
//    |    |    |    |     xOffset
//    |    |    |    xAdvance = 11 pixels
//    |    |    height = 1
//    |    width = 1
//    bitmapOffset = 0

// Line 135: Exclamation mark
{     1,   3,  17,  11,    4,  -16 },   // 0x21 '!'
//    |    |    |    |     |     yOffset = -16 (16px above baseline)
//    |    |    |    |     xOffset = 4 (start 4px right of cursor)
//    |    |    |    xAdvance = 11 pixels (move cursor 11px right)
//    |    |    height = 17 pixels
//    |    width = 3 pixels
//    bitmapOffset = 1

// At end of file: Font structure
const GFXfont IosevkaSS08_Regular11pt7b PROGMEM = {
  (uint8_t  *)IosevkaSS08_Regular11pt7bBitmaps,
  (GFXglyph *)IosevkaSS08_Regular11pt7bGlyphs,
  0x20,    // first = space (ASCII 32)
  0x7E,    // last = ~ (ASCII 126)
  21       // yAdvance = 21 pixels (line height)
};
```

## Current Iosevka Issues

### Issue 1: Inconsistent xAdvance
Iosevka is **monospace** but some characters may have inconsistent xAdvance values.

**Expected:** All characters should have **same xAdvance** (e.g., 11 pixels for 11pt)

**Check:**
```bash
grep "xAdvance" IosevkaSS08_Regular11pt7b.h | sort | uniq -c
```

If you see multiple values like 10, 11, 12 → Inconsistent!

### Issue 2: Excessive yAdvance (Line Spacing)
Looking at the structure:
```cpp
const GFXfont IosevkaSS08_Regular11pt7b = { ..., 21 };  // yAdvance
```

**For 11pt font:**
- yAdvance = 21 pixels seems large for tight metrics
- Could be reduced to 18-19 pixels for tighter line spacing
- This affects multi-line text height calculations

## How to Fix Iosevka Fonts

### Method 1: Manual Editing (Precise)

1. **Open font file** in text editor
2. **Find the GFXfont structure** at the end
3. **Adjust yAdvance:**
   ```cpp
   // BEFORE (too spacious):
   const GFXfont IosevkaSS08_Regular11pt7b = {
     ...,
     21  // yAdvance - OLD VALUE
   };
   
   // AFTER (tighter):
   const GFXfont IosevkaSS08_Regular11pt7b = {
     ...,
     18  // yAdvance - NEW VALUE (15% tighter)
   };
   ```

4. **Verify xAdvance consistency:**
   - All glyphs should have same xAdvance for monospace
   - For 11pt Iosevka, should be 11 pixels
   - If you find any with 10 or 12, change to 11

### Method 2: Regenerate with TTF2GFX (Recommended)

The included `TTF2GFX` tool can regenerate fonts with custom metrics:

```bash
cd src/fonts/TTF2GFX

# Regenerate with tighter line spacing
./ttf2gfx.sh TTFfonts/IosevkaSS08_Regular.ttf 11 \
  --line-height 18 \
  --monospace 11 \
  > ../IosevkaSS08_Regular11pt7b.h
```

**Parameters:**
- `11` - Point size
- `--line-height 18` - Custom yAdvance (instead of default 21)
- `--monospace 11` - Force all xAdvance to 11 pixels

Repeat for all sizes: 9pt, 11pt, 12pt, 13pt, 14pt, 15pt

### Method 3: Batch Script (For All Sizes)

Create `regenerate_iosevka.sh`:

```bash
#!/bin/bash
cd src/fonts/TTF2GFX

FONT="TTFfonts/IosevkaSS08_Regular.ttf"

# Calculate line height as 1.5× point size (tighter than default 1.9×)
generate_font() {
  PT=$1
  LINE_HEIGHT=$(echo "$PT * 1.5" | bc | cut -d. -f1)
  
  ./ttf2gfx.sh "$FONT" $PT \
    --line-height $LINE_HEIGHT \
    --monospace $PT \
    > ../IosevkaSS08_Regular${PT}pt7b.h
    
  echo "Generated ${PT}pt with line height ${LINE_HEIGHT}"
}

generate_font 9
generate_font 11
generate_font 12
generate_font 13
generate_font 14
generate_font 15

echo "All Iosevka fonts regenerated!"
```

Run:
```bash
chmod +x regenerate_iosevka.sh
./regenerate_iosevka.sh
```

## Recommended yAdvance Values

### Standard Formula
`yAdvance = pointSize × 1.5` (50% more than font size)

### Iosevka Tight Formula
`yAdvance = pointSize × 1.3` (30% more - very tight)

| Point Size | Standard yAdvance | Tight yAdvance (Iosevka) | Current | Recommendation |
|------------|-------------------|--------------------------|---------|----------------|
| 9pt        | 14px              | 12px                     | 15px?   | **12px** ✅    |
| 11pt       | 17px              | 14px                     | 21px    | **14px** ✅    |
| 12pt       | 18px              | 16px                     | 23px?   | **16px** ✅    |
| 13pt       | 20px              | 17px                     | 25px?   | **17px** ✅    |
| 14pt       | 21px              | 18px                     | 27px?   | **18px** ✅    |
| 15pt       | 23px              | 20px                     | 29px?   | **20px** ✅    |

## Verifying Changes

### Check Line Height
```cpp
// In oled.cpp, add debug output:
FontMetrics metrics = getFontMetrics();
Serial.print("Font line height: ");
Serial.println(metrics.lineHeight);
```

### Check xAdvance Consistency
```cpp
// Print character advances:
for (char c = 'A'; c <= 'Z'; c++) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(&c, 0, 0, &x1, &y1, &w, &h);
  Serial.print(c);
  Serial.print(": ");
  Serial.println(w);
}
```

All characters should print same width (e.g., 11) for monospace.

## Alternative: Adjust Font Characteristics Instead

Instead of regenerating fonts, you can **increase the lineSpacingMultiplier**:

```cpp
// In oled.cpp:
{ FONT_IOSEVKA_REGULAR, 0.65, 6, 3 },  // Even tighter: 35% reduction
```

**Pros:**
- No font file changes needed
- Can be adjusted per display size
- Easier to experiment

**Cons:**
- Font files still have excessive yAdvance
- Takes up more flash space than necessary
- Affects all point sizes equally

## Testing Procedure

After making changes:

1. **Compile firmware**
2. **Test single-line text:**
   ```cpp
   oled.clearPrintShow("Test Text", 2);
   ```
   Should look same as before.

3. **Test multi-line text:**
   ```cpp
   oled.clearPrintShow("Line 1\nLine 2", 2);
   ```
   Lines should be **closer together** with new metrics.

4. **Test voltage displays:**
   ```cpp
   oled.clearPrintShow("DAC 0\n1.50 V", 2);
   ```
   Should fit at **larger point size** than before.

5. **Test 3-line text:**
   ```cpp
   oled.clearPrintShow("L1\nL2\nL3", 2);
   ```
   Should fit better on 32px displays.

## Recommended Action Plan

### Phase 1: Quick Fix (Current System)
✅ Already done - Using `fontCharacteristics[]` with:
- `lineSpacingMultiplier = 0.85`
- `verticalClipTolerance = 4px`

Result: Iosevka displays at reasonable sizes.

### Phase 2: Font File Optimization (Future)
Regenerate Iosevka fonts with:
- Tighter yAdvance values (1.3× instead of 1.9×)
- Consistent xAdvance for true monospace
- Saves ~10-15% flash space
- Allows even tighter packing

Commands:
```bash
cd src/fonts/TTF2GFX
./regenerate_iosevka.sh
```

Then update `fontCharacteristics`:
```cpp
{ FONT_IOSEVKA_REGULAR, 0.95, 3, 2 },  // Less aggressive now
```

### Phase 3: All Fonts Review (Optional)
Apply same optimization to:
- Berkeley Mono
- Pragmatism
- Other custom fonts

## Summary

**Current Status:**
- ✅ Iosevka works via `fontCharacteristics` system
- ⚠️ Font files have excessive yAdvance values
- ✅ Text displays at proper sizes

**Recommended Next Steps:**
1. Test current implementation thoroughly
2. If satisfied, leave as-is (works well enough)
3. If want perfection, regenerate fonts with tighter metrics
4. Consider regenerating other fonts for consistency

**Result:**
Iosevka will display at **optimal sizes** with **proper line spacing** for small displays! 🎉

