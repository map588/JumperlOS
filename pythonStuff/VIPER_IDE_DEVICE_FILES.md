# Files to Upload to Device for ViperIDE Support

## What ViperIDE Needs

ViperIDE is a **web-based IDE** that runs in your browser and connects to the MicroPython device. Since it can't access local config files, everything must be on the device.

## Required Files on Device

### 1. Python Module (Required)
```
/python_scripts/lib/jumperless.py
```
**Upload:** `pythonStuff/jumperless_module.py` → `/python_scripts/lib/jumperless.py`

This is the wrapper that re-exports all native C module functions.

### 2. Type Stub (Optional - For Enhanced Autocomplete)
```
/python_scripts/lib/jumperless.pyi
```
**Upload:** `pythonStuff/jumperless.pyi` → `/python_scripts/lib/jumperless.pyi`

ViperIDE *may* use this for better autocomplete if it supports stub files.

### 3. Init Script (Optional - For Auto-Setup)
```
/python_scripts/lib/viper_init.py
```
**Upload:** `pythonStuff/viper_init.py` → `/python_scripts/lib/viper_init.py`

Run manually in ViperIDE console to verify everything is loaded:
```python
exec(open('/python_scripts/lib/viper_init.py').read())
```

## How to Upload

### Method 1: Via Jumperless File Manager

1. Enter MicroPython REPL (press `p` in Jumperless menu)
2. Type `files` to open file manager
3. Navigate to `/python_scripts/lib/`
4. Upload files using file manager

### Method 2: Via JFS in REPL

```python
# In Jumperless REPL:
>>> import jfs
>>> 
>>> # Read local file (you'll need to paste content)
>>> content = """... paste jumperless_module.py content ..."""
>>> 
>>> # Write to device
>>> f = jfs.open("/python_scripts/lib/jumperless.py", "w")
>>> f.write(content)
>>> f.close()
```

### Method 3: Via Serial Upload Script

```python
# upload_to_device.py
import serial
import time

port = "/dev/cu.usbmodem1234"  # Adjust for your device
baud = 115200

with serial.Serial(port, baud, timeout=1) as ser:
    time.sleep(2)
    
    # Enter REPL
    ser.write(b'p')
    time.sleep(1)
    
    # Upload file
    with open('pythonStuff/jumperless_module.py', 'r') as f:
        content = f.read()
    
    ser.write(f'f = jfs.open("/python_scripts/lib/jumperless.py", "w")\r\n'.encode())
    time.sleep(0.5)
    ser.write(f'f.write({repr(content)})\r\n'.encode())
    time.sleep(0.5)
    ser.write(b'f.close()\r\n')
    
    print("✓ Uploaded jumperless.py")
```

## ViperIDE Autocomplete Behavior

### What ViperIDE Can Do

ViperIDE provides autocomplete based on:
1. **REPL inspection** - Runs `dir()` and `help()` on the device
2. **Module docstrings** - Parses help text from modules
3. **Type hints** - May use `.pyi` files if uploaded to device
4. **Code analysis** - Basic Python AST parsing

### What ViperIDE Cannot Do

- Read local config files (it's web-based)
- Access filesystem outside the device
- Use workspace-level `.vscode/settings.json`
- Auto-detect global imports without device query

### Limitations

ViperIDE's autocomplete is **simpler** than desktop IDEs:
- May not show all aliases
- Type hints may be basic
- Function signatures might be incomplete

**Workaround:** Keep API reference open in another browser tab.

## Recommended ViperIDE Workflow

### Step 1: Upload Module to Device

Upload `jumperless_module.py` as `/python_scripts/lib/jumperless.py`

### Step 2: Write Scripts Without Imports

**In ViperIDE, write scripts like this:**

```python
# my_script.py
# No imports - everything is global!

gpio_set_dir(GPIO_1, OUTPUT)
for i in range(10):
    gpio_set(GPIO_1, HIGH)
    time.sleep(0.5)
    gpio_set(GPIO_1, LOW)
    time.sleep(0.5)
```

**Why:**
- Jumperless REPL pre-imports everything
- No linter warnings
- Clean, simple code
- Works immediately

### Step 3: Use REPL for Discovery

When you forget a function name:

```python
# In ViperIDE console:
>>> help()              # Show all functions
>>> help("GPIO")        # GPIO section
>>> nodes_help()        # All node names
>>> [x for x in dir() if 'dac' in x.lower()]  # Find DAC functions
```

### Step 4: Keep Reference Open

Open `JUMPERLESS_API_QUICKREF.md` in another browser tab for quick lookup.

## Testing on Device

### Verify Module is Loaded

```python
>>> import jumperless
>>> len(dir(jumperless))
# Should show 300+ items
```

### Verify Global Import Works

```python
>>> from jumperless import *
>>> connect(1, 5)  # Should work
>>> gpio_set(GPIO_1, HIGH)  # Should work
>>> DAC0  # Should show: DAC0
```

### Check JFS Module

```python
>>> import jfs
>>> jfs.exists("/python_scripts/lib/jumperless.py")
# Should return: True
```

## ViperIDE Alternative Approach

### Use Qualified Names

If autocomplete is limited, use module prefix:

```python
import jumperless as jl

jl.connect(jl.D13, jl.TOP_RAIL)
jl.gpio_set(jl.GPIO_1, jl.HIGH)
```

**Pros:**
- Clear where functions come from
- May get better autocomplete
- Good for learning the API

**Cons:**
- More verbose
- Not the "Jumperless way"

## Summary for ViperIDE Users

**Minimum Required:**
- ✅ Upload `jumperless_module.py` to `/python_scripts/lib/jumperless.py`

**Optional Enhancements:**
- ⚠️ Upload `jumperless.pyi` to `/python_scripts/lib/jumperless.pyi` (may help autocomplete)
- ⚠️ Upload `viper_init.py` (convenience script)

**For Your Scripts:**
- ✅ Don't use `from jumperless import *` - everything is already global!
- ✅ Keep API reference open in another tab
- ✅ Use `help()` liberally in console

**For Linter Errors:**
- ✅ Add `# ruff: noqa: F403, F405` to scripts that have imports
- ✅ Or use the `if False:` pattern
- ✅ Or remove imports entirely (recommended for device scripts)

---

## Files to Deploy

```
Device Filesystem:
/python_scripts/lib/
├── jumperless.py       ← From: pythonStuff/jumperless_module.py (REQUIRED)
├── jumperless.pyi      ← From: pythonStuff/jumperless.pyi (OPTIONAL)
└── viper_init.py       ← From: pythonStuff/viper_init.py (OPTIONAL)
```

**After uploading, write scripts with no imports - they just work!**

