# Firmware Upgrade: Automatic Python Examples Update

## Overview

When users upgrade their Jumperless firmware to version **5.6.0.0 or above** from any previous version, the system will automatically **force-overwrite all Python example files** with the new automated example system.

This ensures all users benefit from the new curated, properly-formatted examples without manual intervention.

## Implementation

### Version Detection

The firmware upgrade system compares version strings and triggers the update:

```cpp
// In performConfigMigrations()
if (compareVersions(oldVersion, "5.6.0.0") < 0 && 
    compareVersions(newVersion, "5.6.0.0") >= 0) {
    // Force update Python examples
    initializeMicroPythonExamples(true);
}
```

### Files Affected

When upgrading to 5.6.0.0+, the following files are **force-overwritten**:

#### Module Files
- `/python_scripts/lib/jumperless.py` - Main module wrapper (re-exports native functions)
- `/python_scripts/lib/jumperless.pyi` - Type stub for IDE autocomplete

#### Example Files
- `/python_scripts/examples/adc_basics.py`
- `/python_scripts/examples/dac_basics.py`
- `/python_scripts/examples/gpio_basics.py`
- `/python_scripts/examples/interaction_demo.py`
- `/python_scripts/examples/led_brightness_control.py`
- `/python_scripts/examples/node_connections.py`
- `/python_scripts/examples/stylophone.py`
- `/python_scripts/examples/uart_basics.py`
- `/python_scripts/examples/uart_loopback.py`
- `/python_scripts/examples/voltage_monitor.py`

## User Experience

### Upgrade from < 5.6.0.0 to >= 5.6.0.0

```
╔═══════════════════════════════════════╗
║  Firmware Update Detected             ║
╚═══════════════════════════════════════╝
Previous version: 5.5.0.4
Current version:  5.6.0.0

╔═══════════════════════════════════════╗
║  Python Examples Update Required     ║
╚═══════════════════════════════════════╝
  - Updating to new automated example system
  - Force-overwriting all Python examples...
  
[FORCE INIT] Initializing 10 MicroPython examples...
  [WRITE] /python_scripts/lib/jumperless.py (58KB)
  [WRITE] /python_scripts/lib/jumperless.pyi (6KB)
  [WRITE] /python_scripts/examples/adc_basics.py
  [WRITE] /python_scripts/examples/dac_basics.py
  ... (8 more examples)
  
  ✓ Python examples updated successfully

Firmware version updated in config.
```

### Subsequent Boots

After the initial upgrade, the Python examples are **not** overwritten on subsequent boots unless:
1. Files are missing
2. User explicitly requests re-initialization
3. Another firmware upgrade occurs

## Technical Details

### Version Comparison

The `compareVersions()` function handles version strings in the format `X.Y.Z.W`:

```cpp
int compareVersions(const char* v1, const char* v2) {
    int v1_parts[4] = {0, 0, 0, 0};
    int v2_parts[4] = {0, 0, 0, 0};
    
    sscanf(v1, "%d.%d.%d.%d", &v1_parts[0], &v1_parts[1], &v1_parts[2], &v1_parts[3]);
    sscanf(v2, "%d.%d.%d.%d", &v2_parts[0], &v2_parts[1], &v2_parts[2], &v2_parts[3]);
    
    for (int i = 0; i < 4; i++) {
        if (v1_parts[i] < v2_parts[i]) return -1;
        if (v1_parts[i] > v2_parts[i]) return 1;
    }
    return 0;
}
```

Returns:
- `-1` if v1 < v2
- `0` if v1 == v2  
- `1` if v1 > v2

### Force Initialization

When `initializeMicroPythonExamples(true)` is called with `forceInitialization = true`:

1. **Skips existence checks** - overwrites files even if they already exist
2. **Creates directories** if missing
3. **Writes all enabled examples** from embedded firmware constants
4. **Provides user feedback** about what's being updated

### Config Persistence

The firmware version is stored in the config:

```cpp
jumperlessConfig.firmware.last_version = "5.6.0.0";
saveConfig();
```

This ensures the migration only runs **once per version upgrade**, not on every boot.

## Important User Notes

### User-Modified Examples Will Be Overwritten

⚠️ **Warning**: If users have modified the Python example files before upgrading to 5.6.0.0, their changes will be **lost**.

**Recommendation**: Users should:
1. Back up any modified examples before upgrading
2. Save custom scripts with different filenames
3. Use the `/python_scripts/` directory for personal scripts (not `/python_scripts/examples/`)

### Disabling Examples at Compile Time

Users who want fewer examples can disable them in `src/micropythonExamples.h`:

```c
// Disable specific examples
// #define INCLUDE_STYLOPHONE  // Commented out = disabled

// Or disable entire categories
#define DISABLE_DEMO_EXAMPLES  // Disables LED, Voltage, Stylophone
```

## Migration for Future Versions

To add migrations for future firmware versions, edit `performConfigMigrations()`:

```cpp
void performConfigMigrations(const char* oldVersion, const char* newVersion) {
    // Existing 5.6.0.0 migration
    if (compareVersions(oldVersion, "5.6.0.0") < 0 && 
        compareVersions(newVersion, "5.6.0.0") >= 0) {
        initializeMicroPythonExamples(true);
    }
    
    // Add new migrations here
    if (compareVersions(oldVersion, "5.7.0.0") < 0 && 
        compareVersions(newVersion, "5.7.0.0") >= 0) {
        // Future migration for 5.7.0.0
        // Example: update config structure, migrate settings, etc.
    }
}
```

## Testing

### Test Scenarios

1. **Fresh Install (No previous version)**
   - Should provision all files
   - Version set to 5.6.0.0

2. **Upgrade from 5.5.x to 5.6.0.0**
   - Should trigger Python examples update
   - All example files overwritten
   - Version updated to 5.6.0.0

3. **Already on 5.6.0.0**
   - Should NOT update Python examples
   - Silent boot (unless files are missing)

4. **Upgrade from 5.6.0.0 to 5.6.0.1**
   - Should NOT update Python examples (minor version)
   - Normal provisioning for other files

### Manual Testing

To test the migration without waiting for a real upgrade:

```cpp
// Temporarily modify checkAndHandleFirmwareUpdate() for testing
// In src/configManager.cpp:

bool checkAndHandleFirmwareUpdate(void) {
    // Force a fake upgrade for testing
    const char* fakeOldVersion = "5.5.0.4";  // Simulate old version
    performConfigMigrations(fakeOldVersion, firmwareVersion);
    // ... rest of function
}
```

Then flash and observe the Python examples being force-updated.

## Related Files

| File | Purpose |
|------|---------|
| `src/configManager.cpp` | Version comparison, migration logic |
| `src/configManager.h` | Function declarations |
| `src/FilesystemStuff.cpp` | Python example initialization |
| `src/micropythonExamples.h` | Embedded Python examples (auto-generated) |
| `scripts/generate_micropython_examples.py` | Generator script |
| `scripts/ex/*.py` | Source Python examples |

## Benefits

1. **Automatic Updates** - Users get new examples without manual intervention
2. **Consistent Quality** - All users have the same curated examples
3. **Version Tracking** - System knows when updates are needed
4. **Minimal User Impact** - Happens once during upgrade, silent thereafter
5. **Extensible** - Easy to add migrations for future versions

## Potential Issues

### Issue: User Scripts Overwritten

**Problem**: User had custom examples in `/python_scripts/examples/`

**Solution**: 
- Document that custom scripts should go in `/python_scripts/` (not `/examples/`)
- Add warning in release notes about backing up modifications
- Consider adding a `/python_scripts/user/` directory for custom scripts

### Issue: Disk Space

**Problem**: Writing 10+ examples during upgrade takes time/space

**Solution**:
- Current total size: ~70KB (very small)
- Operation takes < 1 second on most systems
- Can disable examples at compile time if needed

### Issue: Version Format Changes

**Problem**: Future versions might use different format (e.g., "6.0-beta")

**Solution**:
- Current `compareVersions()` handles X.Y.Z.W format
- Can extend to handle semantic versioning if needed
- Graceful fallback to string comparison

## Change Log

### 2025-12-09 - Initial Implementation
- Added `compareVersions()` function for version comparison
- Added automatic Python examples update when upgrading to 5.6.0.0+
- Updated `performConfigMigrations()` with version-specific logic
- Tested compilation and basic functionality

---

**Note**: This feature ensures all users benefit from the new automated Python examples system introduced in firmware 5.6.0.0, while maintaining compatibility with previous versions and allowing for future migrations.

