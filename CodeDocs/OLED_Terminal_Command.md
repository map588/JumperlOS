# OLED Terminal Mode (`t` Command)

## Overview

The `t` command provides an interactive terminal mode that lets you type text directly from your serial terminal and see it displayed on the OLED in real-time with automatic scrolling.

## Usage

### Starting OLED Terminal Mode

```
t
```

This will:
1. Check if OLED is connected
2. Clear the OLED display
3. Show a welcome message
4. Enter interactive mode

### Interactive Mode

Once in OLED terminal mode:

- **Type any text** - It appears on the OLED display
- **Press Enter** - Sends the line to OLED and starts a new line
- **Press 'c'** (at start of line) - Clears the OLED display
- **Press ESC or Ctrl+C** - Exits OLED terminal mode
- **Long lines** - Auto-wrap at 21 characters (OLED width limit)

### Example Session

```
> t

╭────────────────────────────────────╮
│     OLED Terminal Mode             │
├────────────────────────────────────┤
│ Type text to display on OLED       │
│ Press ESC or Ctrl+C to exit        │
│ 'c' = clear display                │
╰────────────────────────────────────╯

Ready. Type your text:
Hello World!
Testing OLED
This is line 3
c
[Display cleared]
New text after clear
^C
✓ Exiting OLED terminal mode
```

## Features

### 1. Real-Time Display
Text appears on the OLED as you type and press Enter.

### 2. Automatic Scrolling
When the display fills up (4 lines on 32px display), old lines scroll off the top and new lines appear at the bottom - just like a terminal!

### 3. Word Wrapping
Long lines automatically wrap at word boundaries when possible, breaking at ~21 characters per line.

### 4. Clear Display
Type `c` at the beginning of a line to clear the OLED and start fresh.

### 5. Echo to Serial
Everything you type is echoed back to the serial terminal for confirmation.

## Technical Details

### Display Specifications
- **Font**: Andale Mono 5pt (monospace)
- **Line height**: 8 pixels (compact spacing)
- **Characters per line**: ~21 characters
- **Lines visible**: 4 lines on 32px display, 8 on 64px display

### Text Processing
- Printable characters (ASCII 32-126) are displayed
- Backspace (0x08, 0x7F) removes last character
- Newline (0x0A, 0x0D) sends line to OLED
- ESC (0x1B) or Ctrl+C (0x03) exits mode

### Under the Hood
Uses the `OLEDOut` stream object which provides:
- Automatic line buffering
- Terminal-like scrolling
- ANSI escape sequence filtering
- Efficient framebuffer updates

## Use Cases

### 1. Quick Status Messages
```
t
System Ready
Voltage: 3.3V
All OK
^C
```

### 2. Debug Output Display
```
t
Debug Mode
Counter: 42
Status: OK
^C
```

### 3. User Messages
```
t
Welcome!
Jumperless V5
Ready to use
^C
```

### 4. Multi-Line Instructions
```
t
Step 1: Connect
Step 2: Configure
Step 3: Test
Step 4: Done
^C
```

## Integration with OLEDOut Stream

The `t` command uses the `OLEDOut` global stream object, which means you can also write to the OLED programmatically:

```cpp
// From your code
OLEDOut.clear();
OLEDOut.println("Hello from code!");
OLEDOut.print("Value: ");
OLEDOut.println(42);
```

This makes it easy to:
- Display debug information
- Show system status
- Output sensor readings
- Display error messages

## Command Registration

The command is registered in `SingleCharCommands.cpp`:

```cpp
registerCommand( 't', "OLED terminal mode",
                 "Interactive OLED terminal - type text to display on OLED. Press ESC to exit, 'c' to clear.",
                 cmd_printTextFromTerminal, MENU_ADVANCED, CAT_SETTINGS );
```

**Menu Level**: Advanced (level 2)  
**Category**: Settings  
**Visibility**: Shows in menu when `showExtraMenu >= 2`

## Error Handling

### OLED Not Connected
```
> t
✗ OLED not connected
  Use '.' command to connect OLED first
```

**Solution**: Run the `.` command first to connect the OLED.

### Display Issues
If text doesn't appear correctly:
1. Check OLED connection with `.` command
2. Try clearing with `c` command
3. Exit and re-enter terminal mode
4. Check OLED I2C address in config

## Comparison with Other Display Methods

### Method 1: Direct OLED Functions
```cpp
oled.clearPrintShow("Hello", 2, true, true, true);
```
- ✅ Good for single messages
- ❌ No scrolling
- ❌ Overwrites display

### Method 2: showMultiLineSmallText
```cpp
oled.showMultiLineSmallText("Line 1\nLine 2", true, true);
```
- ✅ Multiple lines
- ✅ Scrolling support
- ❌ Requires newlines in string

### Method 3: OLEDOut Stream (t command uses this)
```cpp
OLEDOut.println("Line 1");
OLEDOut.println("Line 2");
```
- ✅ Terminal-like behavior
- ✅ Automatic scrolling
- ✅ Stream-compatible
- ✅ Interactive mode available

### Method 4: Interactive Terminal Mode (t command)
```
t
[type text interactively]
```
- ✅ Real-time typing
- ✅ Clear command
- ✅ Easy exit
- ✅ Perfect for testing

## Tips & Tricks

### 1. Quick Clear
Type just `c` and press Enter to clear the display without exiting.

### 2. Multi-Line Messages
Press Enter after each line to build up multi-line messages:
```
Line 1 [Enter]
Line 2 [Enter]
Line 3 [Enter]
```

### 3. Testing Display
Use terminal mode to quickly test if your OLED is working:
```
t
Test
^C
```

### 4. Status Dashboard
Create a live status display:
```
t
=== STATUS ===
CPU: OK
MEM: 45KB
TEMP: 25C
^C
```

### 5. Combining with Other Commands
Exit terminal mode and run other commands, then return:
```
t
Initial message
^C
[run other commands]
t
More messages
^C
```

## Future Enhancements

Possible improvements:
- [ ] Cursor position indicator
- [ ] Line editing (arrow keys)
- [ ] Command history (up/down arrows)
- [ ] Text attributes (bold, inverse)
- [ ] Save display contents to file
- [ ] Load text file to display
- [ ] Adjustable font size
- [ ] Color support (for color OLEDs)

## See Also

- `OLED_Small_Text_API.md` - Complete OLED text display API
- `OLED_Terminal_Scrolling_Fix.md` - Technical details on scrolling
- `Jerial.h` - OLEDStream class reference
- `oled.h` - OLED display API

## Conclusion

The `t` command provides a simple, interactive way to write text to the OLED display directly from your terminal. It's perfect for:
- Testing OLED functionality
- Displaying quick status messages
- Creating simple user interfaces
- Debugging display issues

Just type `t`, enter your text, and watch it appear on the OLED with automatic scrolling! 🖥️✨



