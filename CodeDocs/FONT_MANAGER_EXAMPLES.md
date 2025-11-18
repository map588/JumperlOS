# FontManager Usage Examples

## Quick Start Examples

### Example 1: Simple Auto-Scaling Text
```cpp
// The text will automatically scale down through all available sizes
// until it fits on the display
oled.clearPrintShow("This is a really long message", 2);
// Tries: 12pt → 11pt → 10pt → 9pt → 8pt → 7pt → 6pt → 5pt
// Uses the LARGEST size that fits!
```

### Example 2: Direct Point Size Control
```cpp
// Set specific point size directly
oled.setFontPointSize(FONT_PRAGMATISM, 10);
oled.print("10pt text");

// Change to different size
oled.setFontPointSize(FONT_IOSEVKA_REGULAR, 13);
oled.print("13pt code");
```

### Example 3: Find Best Fit Size
```cpp
// Automatically find the largest font that fits your text
const char* message = "Long status message";
uint8_t optimalSize = FontManager::findBestFitPointSize(
    FONT_PRAGMATISM,
    message,
    oled.displayWidth,  // max width
    12,                 // don't go bigger than 12pt
    6                   // don't go smaller than 6pt
);

oled.setFontPointSize(FONT_PRAGMATISM, optimalSize);
oled.clearPrintShow(message, 2);
```

### Example 4: Multi-Line with Smart Sizing
```cpp
// Multi-line text automatically uses optimal font size
oled.clearPrintShow("Status: OK\nTemp: 25°C\nVoltage: 5.0V", 2);
// Each line rendered at the largest size that fits all lines
```

### Example 5: Menu System with Consistent Sizing
```cpp
void displayMenu(const char* items[], int count) {
    oled.clear();
    oled.setFontPointSize(FONT_PRAGMATISM, 8);  // Consistent 8pt for all items
    
    for (int i = 0; i < count; i++) {
        oled.setCursor(0, 8 + i * 10);
        oled.print(items[i]);
    }
    
    oled.show();
}
```

### Example 6: File Manager with Small Fonts
```cpp
void showFileList(const char* filename, int lineNum) {
    // Use 5pt for compact file listing
    oled.setFontPointSize(FONT_PRAGMATISM, 5);
    oled.setCursor(0, 5 + lineNum * 6);
    oled.print(filename);
}
```

### Example 7: Title + Body with Different Sizes
```cpp
// Large title
oled.setFontPointSize(FONT_PRAGMATISM, 12);
oled.setCursor(0, 12, POS_BASELINE);
oled.print("Settings");

// Smaller body text
oled.setFontPointSize(FONT_PRAGMATISM, 8);
oled.setCursor(0, 24, POS_BASELINE);
oled.print("Brightness: 80%");
```

### Example 8: Dynamic Sizing Based on Content Length
```cpp
void displayStatus(const char* status) {
    int len = strlen(status);
    uint8_t size;
    
    if (len < 10) {
        size = 12;  // Short text - use large font
    } else if (len < 20) {
        size = 9;   // Medium text - use medium font
    } else {
        size = 6;   // Long text - use small font
    }
    
    oled.setFontPointSize(FONT_PRAGMATISM, size);
    oled.clearPrintShow(status, 2);
}
```

## Advanced Examples

### Example 9: Progressive Sizing for Multiple Lines
```cpp
void displayMultiSizeText() {
    oled.clear();
    
    // Title - 12pt
    oled.setFontPointSize(FONT_PRAGMATISM, 12);
    oled.setCursor(0, 12, POS_BASELINE);
    oled.print("JumperlOS");
    
    // Subtitle - 9pt
    oled.setFontPointSize(FONT_PRAGMATISM, 9);
    oled.setCursor(0, 22, POS_BASELINE);
    oled.print("v5.2.1");
    
    // Details - 6pt
    oled.setFontPointSize(FONT_PRAGMATISM, 6);
    oled.setCursor(0, 30, POS_BASELINE);
    oled.print("Ready");
    
    oled.show();
}
```

### Example 10: Font Family Comparison
```cpp
void compareFonts() {
    const char* text = "Sample Text";
    
    oled.clear();
    
    // Pragmatism at 9pt
    oled.setFontPointSize(FONT_PRAGMATISM, 9);
    oled.setCursor(0, 8, POS_BASELINE);
    oled.print(text);
    
    // Iosevka at 9pt
    oled.setFontPointSize(FONT_IOSEVKA_REGULAR, 9);
    oled.setCursor(0, 18, POS_BASELINE);
    oled.print(text);
    
    // Berkeley Mono at 8pt
    oled.setFontPointSize(FONT_BERKELEY_MONO, 8);
    oled.setCursor(0, 28, POS_BASELINE);
    oled.print(text);
    
    oled.show();
}
```

### Example 11: Terminal-Style Output with Fixed Width
```cpp
void terminalPrint(const char* text) {
    // Use monospace font at consistent size
    static int line = 0;
    
    oled.setFontPointSize(FONT_IOSEVKA_REGULAR, 9);
    oled.setCursor(0, 8 + line * 9, POS_BASELINE);
    oled.print(text);
    
    line = (line + 1) % 4;  // Wrap after 4 lines
    if (line == 0) {
        oled.clear();
    }
    
    oled.show();
}
```

### Example 12: Query Available Sizes for a Font
```cpp
void printAvailableSizes(FontFamily family) {
    uint8_t sizes[16];
    int count;
    
    FontManager::getAvailableSizes(family, sizes, &count);
    
    Serial.print("Available sizes for font ");
    Serial.print((int)family);
    Serial.print(": ");
    
    for (int i = 0; i < count; i++) {
        Serial.print(sizes[i]);
        Serial.print("pt ");
    }
    Serial.println();
}

// Usage:
printAvailableSizes(FONT_PRAGMATISM);
// Output: "Available sizes for font 10: 5pt 6pt 7pt 8pt 9pt 10pt 11pt 12pt"
```

### Example 13: Backwards Compatibility Test
```cpp
// Old code continues to work unchanged
void oldStyleUsage() {
    // textSize 1 (small) → automatically maps to 9pt
    oled.setFontForSize(FONT_PRAGMATISM, 1);
    oled.print("Small text");
    
    // textSize 2 (large) → automatically maps to 12pt
    oled.setFontForSize(FONT_PRAGMATISM, 2);
    oled.print("Large text");
}
```

### Example 14: Intelligent Wrapping
```cpp
void smartWrap(const char* longText) {
    // Start at desired size
    uint8_t currentPt = 10;
    
    // Try progressively smaller sizes until it fits
    while (!oled.textFits(longText) && currentPt > 5) {
        currentPt--;
        oled.setFontPointSize(FONT_PRAGMATISM, currentPt);
    }
    
    // If still doesn't fit, enable wrapping
    if (!oled.textFits(longText)) {
        // Text will wrap at minimum size
        oled.clearPrintShow(longText, 1);  // Use size 1 which enables wrap
    } else {
        oled.clearPrintShow(longText, 2, true, true, true);
    }
}
```

## Font Recommendations

### Best for Code/Terminal
- **Iosevka Regular**: Excellent monospace, 6 sizes (9-15pt)
- **Berkeley Mono**: Clean monospace, 2 sizes (8, 12pt)

### Best for UI/Menus
- **Pragmatism**: Very smooth scaling, 8 sizes (5-12pt)
- **Eurostile**: Clean sans-serif, 2 sizes (8, 12pt)

### Best for Compact Displays
- **Pragmatism 5pt**: Most readable at tiny size
- **FreeMono 5pt**: Very compact for dense information

### Best for Readability
- **Pragmatism 9-12pt**: Excellent balance
- **Comic Sans 8-12pt**: Friendly, very readable

## Common Patterns

### Pattern: Menu Headers
```cpp
oled.setFontPointSize(FONT_PRAGMATISM, 12);  // Large header
```

### Pattern: Menu Items
```cpp
oled.setFontPointSize(FONT_PRAGMATISM, 8);   // Medium items
```

### Pattern: Status Bar
```cpp
oled.setFontPointSize(FONT_PRAGMATISM, 6);   // Compact status
```

### Pattern: Error Messages
```cpp
oled.setFontPointSize(FONT_PRAGMATISM, 10);  // Readable but compact
```

### Pattern: Success Messages
```cpp
oled.setFontPointSize(FONT_PRAGMATISM, 12);  // Large and clear
```

## Performance Tips

1. **Cache font indices** if repeatedly using the same font:
   ```cpp
   int myFontIdx = FontManager::getFontForPointSize(FONT_PRAGMATISM, 10);
   oled.setFont(myFontIdx);  // Faster than setFontPointSize
   ```

2. **Pre-calculate sizes** for static content:
   ```cpp
   uint8_t titleSize = 12;  // Calculate once at startup
   uint8_t bodySize = 8;
   ```

3. **Use textSize 1/2** for backwards compatibility and simplicity:
   ```cpp
   oled.clearPrintShow("Text", 2);  // Simpler than setFontPointSize
   ```

## Troubleshooting

### Text Still Too Large
Try smaller minimum size:
```cpp
uint8_t size = FontManager::findBestFitPointSize(
    FONT_PRAGMATISM, text, width, 12, 4  // min=4pt instead of 5pt
);
```

### Font Looks Wrong
Check if font family has that point size:
```cpp
uint8_t sizes[16];
int count;
FontManager::getAvailableSizes(FONT_PRAGMATISM, sizes, &count);
// Use one of the available sizes
```

### Inconsistent Spacing
Use `POS_BASELINE` mode for multi-line:
```cpp
oled.setCursor(x, y, POS_BASELINE);
```

