# OLED Small Text Display API

## Overview

The OLED class now provides generalized functions for displaying small multiline text with optional editing support. This makes it easy to create terminal-like displays, text editors, and other text-based UIs on the OLED display.

## Simple Multi-Line Text Display

### `showMultiLineSmallText()`

Display multi-line text with automatic line breaking at newlines.

```cpp
// Simple usage - clear, display, and show
oled.showMultiLineSmallText("Line 1\nLine 2\nLine 3", true, true);

// Display without clearing first
oled.showMultiLineSmallText("Status: OK\nReady", false, true);

// Build text buffer and display later
oled.showMultiLineSmallText(textBuffer, true, false);
oled.flushFramebuffer(); // Display when ready
```

**Parameters:**
- `text`: Text to display (use `\n` for line breaks)
- `clear`: Clear display before drawing (default: true)
- `show`: Flush framebuffer after drawing (default: true)

**Features:**
- Automatically splits text at newlines
- Uses default small font (SMALL_FONT_PRAGMATISM_5PT)
- Fits ~4 lines on 32px display, ~8 lines on 64px display
- Text that extends past right edge is truncated

## Advanced Text Buffer Display

### `showSmallTextBuffer()`

Display a text buffer with full editing support, cursor display, scrolling, and status text.

```cpp
// Configuration structure
oled::SmallTextDisplayConfig config = {};
config.text = myTextBuffer;           // Text to display
config.font = SMALL_FONT_ANDALE_MONO; // Font choice
config.clear_before = true;           // Clear before drawing
config.show_after = true;             // Show after drawing
config.enable_cursor = true;          // Show cursor
config.cursor_line = 0;               // Cursor at line 0
config.cursor_col = 5;                // Cursor at column 5
config.start_line = 0;                // First line to display
config.max_lines = 3;                 // Show 3 lines
config.horizontal_offset = 0;         // Horizontal scroll offset
config.highlight_cursor_line = false; // Highlight current line
config.status_text = "Editing...";    // Optional status bar

// Display the configured text
oled.showSmallTextBuffer(config);
```

**Configuration Options:**

| Field | Type | Description |
|-------|------|-------------|
| `text` | `const char*` | Text buffer to display (required) |
| `font` | `SmallFont` | Font to use (see SmallFont enum) |
| `clear_before` | `bool` | Clear display before drawing |
| `show_after` | `bool` | Flush framebuffer after drawing |
| `enable_cursor` | `bool` | Show cursor at specified position |
| `cursor_line` | `int` | Line number for cursor (0-based) |
| `cursor_col` | `int` | Column number for cursor (0-based) |
| `start_line` | `int` | First line to display (for scrolling) |
| `max_lines` | `int` | Max lines to show (-1 = auto) |
| `horizontal_offset` | `int` | Horizontal scroll offset |
| `highlight_cursor_line` | `bool` | Highlight entire cursor line |
| `status_text` | `const char*` | Optional status text at bottom |

### Available Small Fonts

```cpp
SMALL_FONT_UBUNTU        // Ubuntu 5pt
SMALL_FONT_DOTGOTHIC     // DotGothic 4pt
SMALL_FONT_JOKERMAN      // Jokerman (small)
SMALL_FONT_ANDALE_MONO   // Andale Mono 5pt (monospace)
SMALL_FONT_IOSEVKA_REGULAR // Iosevka Regular
SMALL_FONT_IOSEVKA_5PT   // Iosevka 5pt Light
SMALL_FONT_PRAGMATISM_5PT // Pragmatism 5pt (default)
SMALL_FONT_FREEMONO_5PT  // Free Mono 5pt
SMALL_FONT_ENVYCODE_5PT  // Envy Code 5pt
```

## Terminal-Like Output with OLEDStream

### Using `OLEDOut` for Terminal Output

The `OLEDOut` object provides a Stream-compatible interface for terminal-like text output with automatic scrolling.

```cpp
// Simple text output
OLEDOut.println("Hello, World!");
OLEDOut.print("Status: ");
OLEDOut.println("OK");

// Formatted output
char buffer[32];
snprintf(buffer, sizeof(buffer), "Value: %d", 42);
OLEDOut.println(buffer);

// Clear and reset
OLEDOut.clear();

// Configure font
OLEDOut.setSmallFont(SMALL_FONT_ANDALE_MONO);

// Control auto-update
OLEDOut.setAutoUpdate(false); // Disable auto-update
OLEDOut.println("Line 1");
OLEDOut.println("Line 2");
OLEDOut.flush(); // Update display manually

// Get display info
int maxLines = OLEDOut.getMaxLines();
int currentLine = OLEDOut.getCurrentLine();
int currentCol = OLEDOut.getCurrentColumn();
```

**Features:**
- Automatic line wrapping at display edge
- Automatic scrolling when reaching bottom
- ANSI escape sequence filtering
- Consecutive newline suppression
- Stream-compatible interface (works with Serial.print style)
- Configurable fonts and scrolling behavior

## Real-World Examples

### Example 1: Simple Status Display

```cpp
void showStatus() {
    oled.showMultiLineSmallText(
        "System: OK\n"
        "Memory: 45KB\n"
        "Temp: 25C\n"
        "Ready",
        true, true
    );
}
```

### Example 2: Text Editor Display (like Ekilo Editor)

```cpp
void updateEditorDisplay(const char* fileContent, int cursorLine, int cursorCol, int scrollOffset) {
    oled::SmallTextDisplayConfig config = {};
    config.text = fileContent;
    config.font = SMALL_FONT_ANDALE_MONO;
    config.clear_before = true;
    config.show_after = true;
    config.enable_cursor = true;
    config.cursor_line = cursorLine;
    config.cursor_col = cursorCol;
    config.start_line = 0;
    config.max_lines = 3;
    config.horizontal_offset = scrollOffset;
    config.highlight_cursor_line = false;
    config.status_text = nullptr;
    
    oled.showSmallTextBuffer(config);
}
```

### Example 3: Terminal Output

```cpp
void runDiagnostics() {
    OLEDOut.clear();
    OLEDOut.setSmallFont(SMALL_FONT_PRAGMATISM_5PT);
    
    OLEDOut.println("Running tests...");
    delay(500);
    
    OLEDOut.println("Test 1: PASS");
    delay(500);
    
    OLEDOut.println("Test 2: PASS");
    delay(500);
    
    OLEDOut.println("Test 3: PASS");
    delay(500);
    
    OLEDOut.println("All tests OK!");
}
```

### Example 4: Scrollable Log Viewer

```cpp
void showLog(const char* logLines[], int numLines, int scrollPosition) {
    // Build text buffer from log lines
    char textBuffer[256];
    int bufPos = 0;
    
    for (int i = scrollPosition; i < numLines && i < scrollPosition + 4; i++) {
        int len = strlen(logLines[i]);
        if (bufPos + len + 1 < sizeof(textBuffer)) {
            strcpy(textBuffer + bufPos, logLines[i]);
            bufPos += len;
            textBuffer[bufPos++] = '\n';
        }
    }
    textBuffer[bufPos] = '\0';
    
    // Configure display
    oled::SmallTextDisplayConfig config = {};
    config.text = textBuffer;
    config.font = SMALL_FONT_PRAGMATISM_5PT;
    config.clear_before = true;
    config.show_after = true;
    config.enable_cursor = false;
    config.start_line = 0;
    config.max_lines = 4;
    config.status_text = "Logs [↑↓]";
    
    oled.showSmallTextBuffer(config);
}
```

## Performance Considerations

### Memory Usage
- `showMultiLineSmallText()`: Uses minimal stack memory (64 byte line buffer)
- `showSmallTextBuffer()`: Uses minimal stack memory (64 byte line buffer)
- `OLEDStream`: Uses ~512 bytes for line buffering

### Best Practices
1. **Use stack buffers** instead of String objects to avoid heap fragmentation
2. **Disable auto-update** when making multiple OLEDOut calls, then flush manually
3. **Check memory** before OLED updates in memory-constrained scenarios
4. **Reuse config structures** to minimize stack usage
5. **Use framebuffer mode** for flicker-free updates

### Framebuffer vs Direct Display
```cpp
// Framebuffer mode (flicker-free, recommended)
oled.clearFramebuffer();
oled.setSmallFont(SMALL_FONT_ANDALE_MONO);
oled.drawText(0, 8, "Line 1");
oled.drawText(0, 16, "Line 2");
oled.flushFramebuffer();

// Direct mode (may flicker)
oled.clear();
oled.setSmallFont(SMALL_FONT_ANDALE_MONO);
oled.setCursor(0, 8);
oled.print("Line 1");
oled.show();
```

## Migration Guide

### Old Code
```cpp
// Old way - custom implementation
oled.clearFramebuffer();
oled.setSmallFont(SMALL_FONT_ANDALE_MONO);
for (int i = 0; i < numLines; i++) {
    oled.drawText(0, (i * 8) + 8, lines[i]);
}
oled.flushFramebuffer();
```

### New Code
```cpp
// New way - generalized function
char textBuffer[256];
// ... build textBuffer with newlines between lines ...
oled.showMultiLineSmallText(textBuffer, true, true);
```

## See Also

- `oled.h` - Full OLED API documentation
- `Jerial.h` - OLEDStream class reference
- `EkiloEditor.cpp` - Advanced text editor example using `showSmallTextBuffer()`
- `Apps.cpp` - Simple usage examples with `showMultiLineSmallText()`





