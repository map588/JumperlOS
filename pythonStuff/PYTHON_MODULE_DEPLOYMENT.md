# Jumperless Python Module Deployment

## Overview

The `jumperless_module.py` file provides a Python-friendly wrapper around the native C `jumperless` module. This makes the module:

1. **Visible in viperIDE** - The `/python_scripts/lib` directory will appear once this file is uploaded
2. **Better IDE support** - Explicit exports enable autocomplete in Python IDEs
3. **Python helpers** - Includes convenience functions like `quick_connect()` and `voltage_divider()`
4. **Documentation** - Inline docs for all functions and constants

## Deployment Instructions

### Option 1: Manual Upload via viperIDE

1. Open viperIDE and connect to your Jumperless
2. Navigate to `/python_scripts/lib/` (create the directory if it doesn't exist)
3. Upload `jumperless_module.py` and rename it to `jumperless.py`

### Option 2: Using mpremote (if available)

```bash
mpremote fs cp jumperless_module.py :/python_scripts/lib/jumperless.py
```

### Option 3: Using JFS in Python

```python
import jfs

# Read the local file
with open('jumperless_module.py', 'r') as f:
    content = f.read()

# Write to device
with jfs.open('/python_scripts/lib/jumperless.py', 'w') as f:
    f.write(content)
```

## Usage After Deployment

Once deployed, users can import the module naturally:

```python
# Import everything
from jumperless import *

# Now use the functions
connect(D13, TOP_RAIL)
dac_set(DAC0, 3.3)
voltage = adc_get(0)

# Use Python helpers
quick_connect(1, 2, 3, 4)  # Chain connections
disconnect_all(D13, A0)     # Disconnect multiple nodes
```

## What Gets Fixed

### Before (without this file):
- `/python_scripts/lib` is invisible in viperIDE (empty directory)
- Must use `import jumperless` and prefix everything
- No IDE autocomplete support
- No Python-level helpers

### After (with this file):
- `/python_scripts/lib/jumperless.py` is visible in viperIDE
- Can use `from jumperless import *` for convenience
- Better IDE support and autocomplete
- Python helper functions available
- Module is easier to discover

## Benefits

1. **Directory Visibility**: Empty directories don't show up in viperIDE - this makes `/python_scripts/lib` visible
2. **Namespace Flexibility**: Users can import specific items or everything
3. **Python Helpers**: Additional convenience functions built in Python
4. **Future-Proof**: Easy to add more Python-level functionality later
5. **Documentation**: Docstrings provide inline help

## Implementation Details

The module works by:
1. Importing the native C module as `_native`
2. Re-exporting all functions and constants
3. Adding Python-level helper functions
4. Defining `__all__` for `from jumperless import *` support

This approach maintains full compatibility with the C module while adding Python conveniences.

