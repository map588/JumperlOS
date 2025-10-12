# USB Mass Storage Mount/Unmount Improvements

## Problem
The USB mass storage mode was experiencing crashes when exiting, caused by improper filesystem synchronization between the device and host PC during mount/unmount operations.

## Root Causes
Based on [arduino-pico FatFSUSB documentation](https://arduino-pico.readthedocs.io/en/latest/fatfsusb.html) and best practices:

1. **Missing callbacks**: No `driveReady`, `onPlug`, or `onUnplug` callbacks were registered
2. **Concurrent filesystem access**: Device was accessing filesystem while host was mounted (multiple writers)
3. **No write blocking**: Auto-save continued running even when host had control
4. **Race conditions**: Multiple `service()` calls from different locations

## Solution: Read-Only Live Monitoring

The implementation now uses a **hybrid approach**:
- **When USB NOT mounted**: Normal operation (device can read + write)
- **When USB IS mounted**: Device switches to **READ-ONLY** mode
  - ✅ Device CAN read files (for live monitoring of host changes)
  - ❌ Device CANNOT write files (host has exclusive write access)

This enables the **live file monitoring feature** where changes made by the host are reflected in real-time on the device, while preventing filesystem corruption from multiple concurrent writers.

## Implementation Details

### 1. State Tracking Flags

```cpp
extern bool mscModeEnabled;        // USB mass storage mode is active
extern bool usbMountedByHost;      // Host has mounted - device in READ-ONLY mode
extern bool usbFilesystemBusy;     // Device is doing filesystem ops - host must wait
```

### 2. Callback Functions

#### `usbDriveReadyCallback()`
Called when host attempts to mount. Returns:
- `false` if device is busy with filesystem operations (makes host wait)
- `true` when device is ready for host to mount

#### `usbPlugCallback()`
Called when host mounts the drive:
1. Closes any open file handles
2. Sets `usbMountedByHost = true` (switches to READ-ONLY mode)
3. Keeps FatFS mounted so device can still READ files
4. Enables live file monitoring

#### `usbUnplugCallback()`
Called when host unmounts/ejects:
1. Clears `usbMountedByHost` flag (resumes READ+WRITE access)
2. Syncs filesystem to see all host changes
3. Reloads active slot to pick up any changes made by host

### 3. SlotManager Service Updates

The `SlotManager::service()` method now:

**Always does:**
- Sets `usbFilesystemBusy = true` at start (prevents host from mounting mid-operation)
- Monitors file changes by reading file mod times
- Detects when files change and reloads them
- Clears `usbFilesystemBusy = false` at end

**When USB is mounted (READ-ONLY mode):**
- ✅ Continues monitoring files for changes (READ operations)
- ❌ Blocks auto-save operations (WRITE operations)
- Queues dirty state for saving when USB is unmounted

**When USB is NOT mounted:**
- ✅ Normal operation (both READ and WRITE)

### 4. Removed Race Conditions

- `usbPeriodic()` no longer calls `SlotManager::service()` directly
- Service is called from main loop via `jOS.serviceAll()` or explicit calls in USB mode loop
- Single point of service execution prevents conflicts

## Benefits

1. **No more crashes on exit**: Proper cleanup with callbacks
2. **Live file monitoring**: Changes made by host are reflected in real-time
3. **Corruption prevention**: Only host can write while USB is mounted
4. **Graceful degradation**: Auto-save is blocked but queued during USB mode
5. **Better synchronization**: Memory barriers and explicit filesystem syncs

## Usage Example

When USB mass storage is enabled:

```
User: Press 'U' to enable USB mode
Device: 
  ✓ Callbacks registered
  ✓ USB Mass Storage ready
  
Host mounts drive:
  ◆ USB MOUNTED BY HOST - Switching to READ-ONLY mode
  ◆ Live file monitoring ACTIVE
  
[Host edits slot0.yaml]
  ✓ Slot 0 reloaded from disk (file change detected)
  [Hardware updates automatically reflect the changes]
  
Host unmounts drive:
  ◆ USB UNMOUNTED BY HOST - Resuming full READ+WRITE
  ✓ Slot reloaded successfully
  [Any pending dirty state is saved]

User: Press 'u' to disable USB mode
Device:
  ✓ Pending changes saved
  ✓ USB Mass Storage fully disabled
```

## Testing Recommendations

1. **Basic mount/unmount**: Verify no crashes when enabling/disabling USB mode
2. **Live file editing**: Edit a slot file on host while device is running, verify real-time updates
3. **Dirty state handling**: Make device changes, mount USB, verify auto-save is blocked, unmount, verify save completes
4. **Multiple mount/unmount cycles**: Verify stability over repeated connect/disconnect cycles
5. **Host eject**: Use OS "eject" feature to verify proper cleanup

## Safety Notes

⚠️ **CRITICAL**: The device performs READ operations while USB is mounted. According to strict FAT filesystem rules, even reads by one party while another is writing can cause issues (e.g., access time updates). However, this is a deliberate trade-off to enable live monitoring.

If stability issues occur, the implementation can be changed to fully block ALL filesystem access when USB is mounted by:
1. Uncommenting `FatFS.end()` in `usbPlugCallback()`
2. Adding early return in `SlotManager::service()` when `usbMountedByHost == true`

## References

- [arduino-pico FatFSUSB Documentation](https://arduino-pico.readthedocs.io/en/latest/fatfsusb.html)
- [FatFSUSB Example: Listfiles-USB](https://github.com/earlephilhower/arduino-pico/blob/master/libraries/FatFSUSB/examples/Listfiles-USB/Listfiles-USB.ino)

## Crash Prevention Measures

After initial testing revealed crashes on the second file save, the root cause was identified through comprehensive debug logging:

### **Root Cause: Redundant disk_ioctl(CTRL_SYNC) Calls**

The crash was caused by calling `fatfs::disk_ioctl(0, CTRL_SYNC, nullptr)` **twice in rapid succession**:

1. First call: Before `f_stat()` to check if file changed ✅ Works
2. Second call: Before `loadSlot()` to reload the file ❌ **CRASHES**

The FatFS library on RP2350 cannot handle multiple SYNC commands in quick succession, especially during USB mass storage mode when the host may also be accessing the filesystem.

**Solution:** Remove the redundant second SYNC call. We only need to sync once before checking file modification time, not again before reading the file.

### Additional Safety Measures

Beyond fixing the root cause, additional safety measures were added:

### 1. **Reload Debouncing** (500ms cooldown)
Prevents rapid successive reloads that can cause memory corruption or race conditions.

### 2. **Concurrent Reload Prevention**
A `reloadInProgress` flag ensures only one reload operation happens at a time:
- If reload is in progress, subsequent service() calls return immediately
- Includes 5-second timeout to recover from hung reloads
- Critical for preventing cascading failures

### 3. **Host Write Completion Delays**
- 50ms delay before opening files
- 100ms delay before reloading after change detected
- Allows host OS to complete write operations before device reads

### 4. **Error Handling for Locked Files**
- Checks `f_stat()` return code explicitly
- Skips reload if file is locked by host OS
- Resets state on error to allow retry on next cycle

### 5. **Memory Safety**
- Zero-initializes `FILINFO` structure with `memset()`
- Prevents reading uninitialized memory
- Adds memory barriers (`__sync_synchronize()`) around critical state changes

### 6. **Lazy Filesystem Synchronization with Failure Tolerance** ⭐ **Critical for Stability**

**Problem:** Initially, the code called `disk_ioctl(CTRL_SYNC)` every single second (60 times per minute), even when no files changed. After many sync operations, the FatFS library enters a bad state and crashes. Even with reduced frequency, occasional crashes still occurred.

**Solution - Three Layers of Defense:**

**Layer 1 - Lazy Sync:** Only sync when a file change is **actually detected**
1. Check file mod time WITHOUT syncing first (quick, safe check)
2. If mod time changed → attempt sync and re-check to confirm
3. If confirmed → proceed with reload

**Layer 2 - Error Handling:** Catch sync failures and continue
- Checks return value from `disk_ioctl(CTRL_SYNC)`
- If sync fails, proceeds with reload anyway (file might still be readable)
- Tracks consecutive sync failures

**Layer 3 - Failure Backoff:** Stop syncing after repeated failures
- After 3 consecutive sync failures, **stops attempting sync entirely**
- File monitoring continues, but sync is skipped
- Reloads still happen based on mod time changes
- Sync is re-enabled after a successful operation

**Impact:** 
- Reduces SYNC calls from ~60/minute to ~1 per file change (98% reduction)
- Gracefully handles sync failures without crashing
- Can operate indefinitely even if sync becomes completely unreliable
- Live file monitoring continues regardless of sync state

**Additional Safety:**
- **Does NOT** call sync again before `loadSlot()` - redundant and causes crashes
- Only ONE sync attempt per file change, not per check cycle
- File reloads proceed even if sync is unavailable

## Files Modified

- `src/USBfs.cpp`: Added callbacks, improved mount/unmount logic
- `src/USBfs.h`: Exported new state flags
- `src/States.cpp`: Updated `SlotManager::service()` with crash prevention and USB mount state handling

