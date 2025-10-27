# Serial Input Tag Stripping Feature

## Overview

The TermControl class now automatically strips `<j>` and `</j>` tags from incoming USB serial data to prevent weird behavior when commands are wrapped in XML-like tags. This feature is enabled by default in the system initialization.

## What Was Implemented

### 1. Automatic Tag Filtering During Input Processing

When `auto_strip_tags` is enabled on TermControl, tags are transparently removed as characters are received from the serial port. This happens in real-time during the `service()` method without requiring any changes to existing code.

**Example:**
- Input received:  `<j>+ 1-2</j>`
- Your code reads:  `+ 1-2`

### 2. Enabled by Default

Tag stripping is automatically enabled during system initialization in `main.cpp`:

```cpp
termSerial.setAutoStripTags(true);
```

### 3. Works in Both Line-Buffered and Character Modes

The tag stripping works regardless of whether terminal line buffering is enabled or disabled, as it filters characters at the input stage before any other processing.

## API Reference

### Enable/Disable Auto-Stripping

```cpp
// Enable automatic tag stripping (default in main.cpp)
termSerial.setAutoStripTags(true);

// Disable automatic tag stripping
termSerial.setAutoStripTags(false);

// Check if enabled
if (termSerial.isAutoStripTags()) {
    // Tag stripping is active
}
```

## Implementation Details

### Tag Detection State Machine

TermControl maintains a 4-byte buffer to detect tags in real-time:

1. **Buffer incoming characters** when '<' is detected
2. **Check for complete tags**:
   - `<j>` (3 bytes) - opening tag
   - `</j>` (4 bytes) - closing tag
3. **Discard tags** when detected
4. **Return non-tag characters** to caller
5. **Handle malformed tags** by returning buffered characters

### Edge Cases Handled

✅ **Incomplete tags** - Not treated as tags:
```
Input: "<j test"    → Output: "<j test"
Input: "< j>test"   → Output: "< j>test"
```

✅ **Multiple consecutive tags**:
```
Input: "<j>a</j><j>b</j>" → Output: "ab"
```

✅ **Nested/malformed tags**:
```
Input: "<<j>test</j>" → Output: "<test"
```

✅ **Only `<j>` and `</j>` are stripped** - other tags preserved:
```
Input: "<j>test</j><other>tag</other>" → Output: "test<other>tag</other>"
```

✅ **Buffered character management** - When a potential tag turns out not to be a tag, buffered characters are returned in the correct order

## Memory Usage

- **Tag buffer**: 4 bytes per TermControl instance
- **State variables**: 1 byte (tag_buffer_pos)
- **Total overhead**: ~5 bytes

## Performance

- **Zero-copy for non-tag characters** - Characters not part of tags are returned immediately
- **Minimal buffering** - Only up to 4 characters buffered when detecting potential tags
- **No heap allocation** - All buffers are fixed-size on the stack

## Files Modified

### Header File: `src/TermControl.h`
- Added `auto_strip_tags` flag
- Added `tag_buffer[4]` for tag detection
- Added `tag_buffer_pos` state variable
- Added `setAutoStripTags()` and `isAutoStripTags()` public methods
- Added `shouldSkipChar()` private method for tag detection

### Implementation: `src/TermControl.cpp`
- Initialized tag stripping state in constructor
- Implemented `shouldSkipChar()` with state machine logic
- Updated `service()` to use tag filter when enabled
- Tag filtering happens before ANSI escape sequence processing

### System Initialization: `src/main.cpp`
- Added `termSerial.setAutoStripTags(true)` during startup
- Placed after Serial.begin() and jSerial configuration
- Applies to all input (both line-buffered and character modes)

## Testing

To test tag stripping:

```cpp
// Send these over USB serial:
<j>+ 1-2</j>   // Should connect nodes 1-2
<j>- 1-2</j>   // Should disconnect nodes 1-2
<j>c</j>       // Should clear connections

// The system will receive them without tags:
+ 1-2
- 1-2
c
```

## Example Code

See `CodeDocs/examples/jSerial_tag_stripping.cpp` for a complete example demonstrating tag stripping functionality.

## Disabling Tag Stripping

If you need to receive `<j>` tags as-is (for debugging or other purposes):

```cpp
// In main.cpp setup() or your initialization code
termSerial.setAutoStripTags(false);
```

## Future Enhancements

Potential improvements if needed:
- Support for additional tag types (configurable)
- Statistics on tags stripped
- Optional logging of stripped tags for debugging
- Escape sequences to send literal `<j>` tags

## Reasoning Behind Implementation

The tag filtering uses a character-by-character state machine approach because:

1. **Memory efficient** - Only buffers up to 4 characters at a time
2. **Real-time processing** - No need to buffer entire lines
3. **Transparent** - Existing code works unchanged
4. **Handles edge cases** - Properly deals with incomplete tags, malformed tags, and overlapping tags
5. **Zero performance impact when disabled** - Single boolean check in service loop
6. **Integrated early** - Filters before ANSI escape processing and line buffering

The implementation prioritizes correctness and minimal memory usage over raw speed, which is appropriate for serial communication where baud rate is the bottleneck.

## Why TermControl Instead of jSerial?

Initially, tag stripping was implemented in jSerial, but it was discovered that the system reads input primarily through:
1. **TermControl (termSerial)** - For line-buffered input (most common case)
2. **Serial directly** - For single-character input when line buffering is disabled

Since jSerial is used primarily for **output** (broadcasting to multiple streams), the tag stripping needed to be implemented in TermControl where the actual input processing happens. This ensures tags are stripped regardless of whether line buffering is enabled or disabled.

