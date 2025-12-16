# MpRemoteService Script Execution Callbacks

## Overview

Callback mechanisms that trigger when Python scripts begin and complete execution in raw REPL mode (used by ViperIDE/mpremote). This allows custom behavior like timing measurements, logging, cleanup, or state updates.

## Implementation

### Components Added

1. **Virtual Method in MpRemoteService**
   - `void onScriptExecutionComplete()` - Override to add custom behavior
   - Called after each script finishes in raw REPL mode

2. **C Callback Hook**
   - `jl_on_script_complete_callback` - Function pointer for C/C++ bridge
   - Registered in `MpRemoteService` constructor
   - Called from `jl_after_python_exec_hook`

3. **Hook Integration**
   - Modified `jl_after_python_exec_hook` in `micropython_embed.c`
   - Calls callback when in raw REPL mode after GC completes

## Execution Flow

```
User sends script via ViperIDE/mpremote
         ↓
pyexec_event_repl_process_char() receives Ctrl-D
         ↓
parse_compile_execute() runs the script
         ↓
jl_after_python_exec_hook() is called
         ↓
gc_collect() runs (memory cleanup)
         ↓
jl_on_script_complete_callback() is invoked (if in raw REPL)
         ↓
MpRemoteService::onScriptExecutionComplete() executes
         ↓
REPL returns to ">" prompt
```

## When Callback Fires

**✅ Fires for:**
- Complete Python scripts executed in raw REPL mode
- Each script sent by ViperIDE file save operation
- Each command block terminated by Ctrl-D in mpremote

**❌ Does NOT fire for:**
- Interactive (friendly) REPL single-line commands
- Scripts run outside raw REPL mode
- Each line in multiline input (only fires when execution completes)

## Timing Guarantees

The callback is called AFTER:
- ✅ Script execution completes
- ✅ Garbage collection runs
- ✅ File handles are still open (raw REPL preserves them)
- ✅ Standard output/error has been sent

The callback is called BEFORE:
- ✅ Returning to REPL ">" prompt
- ✅ Processing next command from user

## Usage Example

### Method 1: Override in Derived Class

```cpp
class CustomMpRemoteService : public MpRemoteService {
    void onScriptExecutionComplete() override {
        Serial.println("[Custom] Script completed!");
        
        // Your custom logic:
        // - Log execution count
        // - Update display
        // - Trigger state save
        // - Flash indicator LED
    }
};
```

### Method 2: Modify Default Implementation

Simply edit the `onScriptExecutionComplete()` method in `MpRemoteService.cpp`:

```cpp
void MpRemoteService::onScriptExecutionComplete() {
    if ( m_debug ) {
        Serial.println( "[MpRemote] Script execution completed" );
    }
    
    // Add your custom logic here
    updateScriptCounter();
    logExecutionTime();
    flashLED();
}
```

## Use Cases

1. **Script Execution Logging**
   - Track how many scripts have been executed
   - Measure approximate execution times
   - Log script activity for debugging

2. **UI Updates**
   - Update OLED display with execution status
   - Flash LED to indicate script completion
   - Update web dashboard

3. **State Management**
   - Trigger periodic state saves
   - Update metrics and statistics
   - Synchronize external systems

4. **Development Tools**
   - Profiling script execution patterns
   - Detecting script completion for automated testing
   - Triggering post-execution analysis

## Important Warnings

⚠️ **Do NOT close file handles in this callback!**
- Raw REPL mode requires file handles to persist across commands
- ViperIDE opens files in one command and writes in another
- Closing files here will break the file save workflow

⚠️ **Keep callback execution fast!**
- This runs in the critical path of REPL response
- Long-running operations will delay the REPL prompt
- Consider deferring heavy work to a separate task

⚠️ **Thread safety considerations**
- Callback runs on Core 0 (MicroPython core)
- Be careful with shared state accessed from Core 1
- Use appropriate synchronization if needed

## Testing

To test the callback:

1. Connect via ViperIDE or mpremote
2. Run a simple Python script: `print("Hello")`
3. Watch for callback log message: `[MpRemote] Script execution completed`
4. Verify callback fires after each script execution

## Files Modified

- `src/MpRemoteService.h` - Added virtual callback method
- `src/MpRemoteService.cpp` - Implemented callback and registration
- `lib/micropython/port/micropython_embed.c` - Added hook invocation
- `src/MpRemoteService_Example.cpp.example` - Usage examples

## Related Features

- **`jl_in_raw_repl_mode` flag** - Tracks raw REPL state
- **`jl_after_python_exec_hook`** - Low-level execution hook
- **File handle persistence** - Files stay open in raw REPL mode
- **Garbage collection** - Runs before callback fires

## Future Enhancements

Possible improvements:
- [ ] Add execution time measurement (start/end timestamps)
- [ ] Pass execution result (success/exception) to callback
- [ ] Support multiple registered callbacks
- [ ] Add callback for friendly REPL script completion
- [ ] Include script size or line count in callback parameters

