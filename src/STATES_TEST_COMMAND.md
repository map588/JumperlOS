# States System Test Command (J)

## Overview

The `J` command has been integrated into `main.cpp` to test and demonstrate the new States system. It provides a complete test harness for creating states, adding connections, and verifying the JSON serialization and slot management.

## Usage

### Basic Syntax

```
J [connections]
```

Where `[connections]` is one or more node connections in the format `N1-N2`, separated by commas.

### Examples

```
J 1-2                    # Single connection
J 1-5,10-20             # Multiple connections
J 1-5,10-20,15-30       # Multiple connections
J 1-5,1-5,1-5           # Duplicate connections (increments count)
```

### Help

Simply type `J` with no arguments to see help:

```
J
```

Output:
```
╭────────────────────────────────────╮
│   States System Test (J command)  │
╰────────────────────────────────────╯

States System Test Command

Usage:
  J 1-2              - Add connection 1-2
  J 1-5,10-20        - Add multiple connections
  J 1-5,1-5,1-5      - Add duplicates (increments count)

Features:
  • Validates connections
  • Tracks duplicate counts
  • JSON serialization
  • Save/load from slots
  • Undo/redo history

Example:
  J 1-5              - Creates connection 1-5
  J TOP_RAIL-10      - Connects top rail to row 10
  J GND-32           - Connects ground to row 32
```

## What It Tests

The `J` command performs a comprehensive test of the States system:

### 1. Connection Parsing
- Parses comma-separated connection strings
- Validates format (must be `N1-N2`)
- Extracts node numbers

### 2. Connection Validation
- Validates node numbers using `isNodeValid()`
- Checks if connections are allowed
- Reports specific error messages on failure

### 3. Duplicate Tracking
- Automatically increments duplicate count for repeated connections
- Shows duplicate count in connection list

### 4. State Display
Shows current state including:
- Total number of connections
- Active slot number
- List of all connections with duplicate counts

### 5. JSON Serialization
- Converts state to JSON format
- Displays first 500 characters of JSON output
- Validates JSON structure

### 6. Slot Save/Load
- Saves state to slot 7 (test slot)
- Creates `/slots/slot7.json` file
- Loads the slot back to verify
- Compares connection counts

### 7. Memory Usage
- Estimates total RAM usage
- Shows active state size
- Displays state object size

## Sample Output

```
J 1-5,10-20,1-5

╭────────────────────────────────────╮
│   States System Test (J command)  │
╰────────────────────────────────────╯

Parsing connections: 1-5,10-20,1-5
  Adding connection: 1-5... ✓ Success
  Adding connection: 10-20... ✓ Success
  Adding connection: 1-5... ✓ Success

─── Current State ───
Connections: 2
Active Slot: -1

Connections in state:
  1. 1-5 (x2 duplicates)
  2. 10-20

─── Testing JSON Serialization ───
JSON output (first 500 chars):
{
  "version": 1,
  "power": {
    "topRail": 0.00,
    "bottomRail": 0.00,
    "dac0": 3.33,
    "dac1": 0.00
  },
  "config": {
    "stackPaths": 2,
    "stackRails": 3,
    ...
  },
  "connections": {
    "numBridges": 2,
    "bridges": [
      {"n1": 1, "n2": 5, "dup": 2},
      {"n1": 10, "n2": 20, "dup": 1}
    ]
  }
}...

─── Testing Slot Save ───
Saving to slot 7... ✓ Success
  File: /slots/slot7.json
Loading from slot 7... ✓ Success
  Loaded 2 connections

─── Memory Usage ───
Active state RAM: ~250000 bytes
State object size: ~180000 bytes

─── Test Complete ───
```

## Error Examples

### Invalid Node

```
J 999-1000

  Adding connection: 999-1000... ✗ Failed
    Error: Invalid node 1: 999
```

### Power/Ground Conflict

```
J 100-101

  Adding connection: 100-101... ✗ Failed
    Error: Connection not allowed between 100 and 101 (likely power/ground conflict)
```

### Invalid Format

```
J 1-5-10

  Invalid format: 1-5-10 (should be N1-N2)
```

## Implementation Details

### Location
- **File:** `src/main.cpp`
- **Line:** ~1206
- **Case:** `case 'J':`

### Key Features

1. **Non-destructive Testing**
   - Uses singleton pattern (doesn't create multiple managers)
   - Tests save to slot 7 (dedicated test slot)
   - Doesn't interfere with active connections

2. **Comprehensive Validation**
   - Validates each connection before adding
   - Reports specific errors
   - Counts successes and failures

3. **Duplicate Handling**
   - Calling `J 1-5` multiple times increments the duplicate count
   - Shows duplicate count in output: `(x3 duplicates)`

4. **JSON Verification**
   - Serializes state to JSON
   - Shows formatted output
   - Tests round-trip (save → load)

5. **Memory Reporting**
   - Shows total RAM usage of state manager
   - Estimates state object size
   - Includes history buffer size

## Integration with Existing Code

### Includes
The command requires `States.h` to be included:

```cpp
#include "States.h"  // New state management system
```

### No Conflicts
- Uses `case 'J':` (capital J) which was unused
- lowercase `j` still shows terminal colors
- Follows existing command pattern
- Uses `goto dontshowmenu;` to skip menu display

### Command Line Parsing
Uses the existing `currentCommandLine` global:

```cpp
String commandLine = currentCommandLine;
if (commandLine.length() > 1) {
    commandLine = commandLine.substring(1);  // Remove 'J'
    commandLine.trim();
    // ... parse connections
}
```

## Testing Workflow

### Basic Test
```
1. Type: J 1-5
2. Verify connection is added
3. Check JSON output
4. Verify slot save/load works
```

### Duplicate Test
```
1. Type: J 1-5
2. Type: J 1-5
3. Type: J 1-5
4. Check duplicate count shows (x3 duplicates)
```

### Error Handling Test
```
1. Type: J 999-1000     # Invalid nodes
2. Type: J 100-101      # Power/ground conflict
3. Type: J 1-5-10       # Invalid format
4. Verify error messages are clear
```

### Multiple Connections Test
```
1. Type: J 1-5,10-20,15-30
2. Verify all three connections are added
3. Check JSON includes all three
4. Verify save/load preserves all
```

## Advanced Usage

### Named Node Support (Future)

The system is designed to support named nodes like `TOP_RAIL`, `GND`, but currently only accepts numeric values. To add support:

1. Parse node names in the command handler
2. Use `replaceSFNamesWithDefinedInts()` from FileParsing
3. Convert to numeric values before calling `addConnection()`

Example:
```cpp
// Future enhancement
specialFunctionsString = conn;
replaceSFNamesWithDefinedInts();
int node1 = specialFunctionsString.substring(0, dashIdx).toInt();
```

### Integration with Hardware

The `J` command currently only tests the state management layer. To actually connect the hardware:

1. After adding connections, call existing functions:
   ```cpp
   bridgesToPaths(1, 1);
   sendAllPaths();
   ```

2. Or use the existing integration path:
   ```cpp
   refreshConnections(-1);
   ```

## Compilation

- **Status:** ✅ Compiles successfully
- **RAM Usage:** 92.4% (484,488 / 524,288 bytes)
- **Flash Usage:** 8.7% (1,099,036 / 12,578,816 bytes)
- **Build Time:** ~4 seconds

## Future Enhancements

1. **Visual Feedback**
   - Light up LEDs for added connections
   - Show connection paths on display

2. **Hardware Integration**
   - Option to actually route connections: `J -r 1-5`
   - Preview mode: `J -p 1-5` (show without connecting)

3. **Named Nodes**
   - Support: `J TOP_RAIL-10`
   - Support: `J GND-32`
   - Support: `J NANO_D13-15`

4. **Undo/Redo**
   - `J -u` to undo last connection
   - `J -r` to redo

5. **Slot Management**
   - `J -s 3` to save to specific slot
   - `J -l 5` to load from specific slot
   - `J -c` to clear current state

6. **Batch Operations**
   - `J -f connections.txt` to load from file
   - `J -e slot7.json` to export

## Troubleshooting

### Command Not Recognized
- Make sure you're using capital `J`, not lowercase `j`
- Check that States.h is included in main.cpp

### Compilation Errors
- Verify States.cpp and States.h are in src/
- Check that ArduinoJson v7 is installed
- Ensure CH446Q.h defines struct justXY

### Save/Load Fails
- Check that FatFS is initialized
- Verify /slots directory can be created
- Check available flash space

### Duplicate Count Wrong
- This is expected - each call to `J 1-5` increments
- Use different nodes for separate connections
- Or manually set: `state.setConnectionDuplicates(1, 5, 1, err)`

## Quick Reference

| Command | Description |
|---------|-------------|
| `J` | Show help |
| `J 1-2` | Add single connection |
| `J 1-5,10-20` | Add multiple connections |
| `J 1-5,1-5,1-5` | Add duplicates (3x) |

## See Also

- [STATES_INTEGRATION.md](STATES_INTEGRATION.md) - Complete integration guide
- [STATES_DUPLICATES.md](STATES_DUPLICATES.md) - Duplicate connections feature
- [States.h](States.h) - Header file
- [States.cpp](States.cpp) - Implementation

