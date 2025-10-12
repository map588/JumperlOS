# Python Syntax Highlighting Refactor

## Overview
Consolidated all Python syntax highlighting logic into a centralized `SyntaxHighlighting` class that can be reused across multiple components (Python REPL, Ekilo Editor, and Terminal Control).

## Problem Statement
The Python REPL wasn't properly highlighting Jumperless-specific functions. Syntax highlighting logic was duplicated across multiple files with inconsistent implementations.

## Solution
Created a centralized syntax highlighting system in `SyntaxHighlighting.cpp` with:
- Comprehensive Python keyword arrays
- Jumperless function arrays
- Jumperless constant/type arrays
- JFS (Jumperless Filesystem) function arrays
- Proper classification and color mapping for all identifiers

## Changes Made

### 1. SyntaxHighlighting.h
**Added:**
- `PythonKeywordType` enum for keyword classification
- Public methods:
  - `highlightPythonCode()` - Full Python syntax highlighting with Jumperless support
  - `displayPythonWithHighlighting()` - Stream output version
  - `classifyPythonKeyword()` - Classify Python identifiers
- Private keyword checking methods:
  - `isPythonKeyword()`
  - `isPythonBuiltin()`
  - `isJumperlessFunction()`
  - `isJumperlessType()`
  - `isJFSFunction()`

### 2. SyntaxHighlighting.cpp
**Added:**
- Comprehensive keyword arrays consolidated from EkiloEditor.cpp:
  - `python_keywords[]` - Python language keywords (if, for, def, etc.)
  - `python_builtins[]` - Python built-in functions (len, str, int, etc.)
  - `jumperless_functions[]` - Jumperless hardware functions (connect, dac_set, gpio_get, etc.)
  - `jumperless_types[]` - Jumperless constants (TOP_RAIL, GND, D0-D13, A0-A7, etc.)
  - `jfs_functions[]` - JFS filesystem functions (open, read, write, etc.)

**Enhanced:**
- `highlightPythonCode()` method now:
  - Handles comments (#)
  - Handles strings (single and double quotes)
  - Handles numbers (integers and floats)
  - Properly classifies all identifiers using `classifyPythonKeyword()`
  - Applies correct colors for each keyword type

**Color Mapping:**
- Python keywords (if, for, def) → **Orange** (214)
- Python builtins (len, str, int) → **Green** (79)
- Jumperless functions (connect, dac_set) → **Magenta** (207)
- Jumperless constants (TOP_RAIL, GND, D0) → **Purple** (105)
- JFS functions (open, read, write) → **Cyan-blue** (45)
- Strings → **Cyan** (39)
- Numbers → **Red** (199)
- Comments → **Green** (34)
- Unknown identifiers → **White** (255)

### 3. Python_Proper.cpp
**Added:**
- `#include "SyntaxHighlighting.h"` to imports

**Updated:**
- `displayStringWithSyntaxHighlighting()` now uses centralized `SyntaxHighlighting` class
- REPL automatically benefits from improved highlighting without code changes

### 4. Backward Compatibility
- Global `displayStringWithSyntaxHighlighting()` function maintained for existing code
- Now internally uses `SyntaxHighlighting` class for consistent behavior
- All existing REPL and editor code continues to work without modification

## Benefits

### 1. Consistency
- Single source of truth for Python syntax highlighting
- Same highlighting behavior across REPL, editors, and terminal

### 2. Maintainability
- Easy to add new Jumperless functions - just add to array in one place
- Centralized color scheme management
- Clear separation of concerns

### 3. Extensibility
- Easy to add new keyword categories
- Simple to adjust colors for specific types
- Can be used by any component that needs Python highlighting

### 4. Performance
- Efficient single-pass parsing
- No duplicate parsing or processing
- Lightweight string operations

## Usage Examples

### In Python REPL
```cpp
// Automatically used when typing in REPL - no code changes needed
// Functions like connect(), dac_set(), gpio_get() now highlighted in magenta
// Constants like TOP_RAIL, GND, D0-D13 now highlighted in purple
```

### In Custom Code
```cpp
// Create a highlighter
SyntaxHighlighting highlighter(&Serial);

// Classify a word
PythonKeywordType type = highlighter.classifyPythonKeyword("connect");
// Returns: KW_JUMPERLESS

// Get highlighted string
String highlighted = highlighter.highlightPythonCode("connect(TOP_RAIL, D13)");
// Returns: string with ANSI color codes

// Direct stream output
highlighter.displayPythonWithHighlighting("connect(TOP_RAIL, D13)", &Serial);
// Prints highlighted text to Serial
```

### Global Helper Function
```cpp
// For backward compatibility and convenience
displayStringWithSyntaxHighlighting("connect(TOP_RAIL, D13)", &Serial);
// Uses SyntaxHighlighting class internally
```

## Testing

### Quick Test in REPL
1. Enter Python REPL mode
2. Type: `connect(TOP_RAIL, D13)`
   - `connect` should be **magenta**
   - `TOP_RAIL` and `D13` should be **purple**
3. Type: `dac_set(DAC0, 3.3)`
   - `dac_set` should be **magenta**
   - `DAC0` should be **purple**
   - `3.3` should be **red**
4. Type: `for i in range(10):`
   - `for` and `in` should be **orange**
   - `range` should be **green**
   - `10` should be **red**

### Visual Verification
All Jumperless functions should now be highlighted in magenta:
- Hardware: `connect`, `disconnect`, `dac_set`, `adc_get`, `gpio_set`, etc.
- OLED: `oled_print`, `oled_clear`, `oled_connect`
- Probe: `probe_read`, `probe_button`, `probe_wait`
- PWM/Wavegen: `pwm`, `wavegen_set_freq`, `wavegen_start`

All Jumperless constants should be highlighted in purple:
- Rails: `TOP_RAIL`, `BOTTOM_RAIL`, `GND`
- Arduino pins: `D0`-`D13`, `A0`-`A7`
- DACs: `DAC0`, `DAC1`
- ADCs: `ADC0`-`ADC4`
- Waveforms: `SINE`, `TRIANGLE`, `SAWTOOTH`, `SQUARE`

## Architecture

### Before
```
Python_Proper.cpp → displayStringWithSyntaxHighlighting() [basic, limited]
EkiloEditor.cpp   → editor_highlight() [separate arrays]
```

### After
```
                    ┌─────────────────────────┐
                    │  SyntaxHighlighting     │
                    │  - Keyword Arrays       │
                    │  - Classification       │
                    │  - Color Mapping        │
                    └─────────────────────────┘
                              ▲
                              │
            ┌─────────────────┼─────────────────┐
            │                 │                 │
    ┌───────▼─────┐   ┌──────▼──────┐   ┌─────▼──────┐
    │Python_Proper│   │EkiloEditor  │   │TermControl │
    │    REPL     │   │   Editor    │   │  Terminal  │
    └─────────────┘   └─────────────┘   └────────────┘
```

## Future Enhancements

### Easy Additions
1. **More Jumperless Functions**: Just add to `jumperless_functions[]` array
2. **Custom Color Schemes**: Modify `map_hl_to_color()` function
3. **Additional Keyword Types**: Add new enum to `PythonKeywordType`
4. **Syntax Highlighting for Other Languages**: Add new keyword arrays and classification methods

### Potential Features
- Multi-line string highlighting (""" ... """)
- F-string interpolation highlighting
- Decorator highlighting (@property, @staticmethod)
- Type hint highlighting (-> int, : str)
- Doc-string special highlighting
- Import statement smart highlighting

## Notes

### Color Consistency
All colors are defined centrally in `map_hl_to_color()` function. Changes there affect all highlighting globally.

### Performance
- Single-pass parsing: O(n) complexity
- No regular expressions: Faster and more predictable
- String operations are efficient on Arduino

### Memory Usage
- Static arrays stored in flash memory (PROGMEM)
- Minimal RAM usage during highlighting
- No dynamic allocations for keyword lookups

## Conclusion

The syntax highlighting system is now:
- ✅ Centralized and consistent
- ✅ Comprehensive (covers all Jumperless functions)
- ✅ Maintainable (single source of truth)
- ✅ Extensible (easy to add new features)
- ✅ Backward compatible (no breaking changes)
- ✅ Well-documented (this file!)

Python REPL now properly highlights all Jumperless functions and constants, making it much easier to write and debug Jumperless Python code!

