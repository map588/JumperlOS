# ViperIDE Quick Reference for Jumperless

## 🎯 Essential Commands (Copy/Paste These)

### Start Every Session With This:
```python
from jumperless import *
print("✓ Ready")
```

### View All Available Functions:
```python
help()
```

### View Node Names:
```python
nodes_help()
```

### See What's Imported:
```python
[name for name in dir() if not name.startswith('_')]
```

---

## 📝 Writing Scripts in ViperIDE

### ✅ Do This (No Imports):
```python
# script.py - runs perfectly on device
connect(D13, TOP_RAIL)
dac_set(DAC0, 3.3)
gpio_set(GPIO_1, HIGH)
```

### ⚠️ Or This (With Session Init):
```python
# In console first:
from jumperless import *

# Then in your script:
connect(D13, TOP_RAIL)
dac_set(DAC0, 3.3)
```

### ❌ Don't Do This:
```python
# This won't help autocomplete in ViperIDE
from jumperless import *

# ViperIDE autocomplete is runtime-based
# imports in scripts don't affect editor autocomplete
```

---

## 🔧 Common Tasks

### Upload a Script:
1. File Manager → Upload
2. Navigate to `/python_scripts/`
3. Select file

### Run a Script:
```python
exec(open('/python_scripts/my_script.py').read())
```

### Run Init Script:
```python
exec(open('/python_scripts/lib/viper_init.py').read())
```

### Clear Connections:
```python
clear_all()
```

### Save Netlist:
```python
write_netlist()
```

---

## 🐛 Troubleshooting

### "No autocomplete for jumperless functions"
**Solution:** Run in console:
```python
from jumperless import *
```

### "NameError: name 'connect' not defined"
**In REPL:** Run `from jumperless import *` first
**In Script:** Everything is already global, should work

### "Can't find my script"
**Check path:**
```python
import os
os.listdir('/python_scripts')
```

### "Function signature not showing"
**Get help:**
```python
help(connect)
help(dac_set)
```

---

## 📚 Quick API Reference

### Connections:
```python
connect(node1, node2)            # Create connection
disconnect(node1, node2)         # Remove connection  
clear_all()                       # Remove all connections
```

### DAC (0-4.096V):
```python
dac_set(DAC0, 3.3)               # Set voltage
dac_sweep(DAC0, 0, 3.3, 1.0)     # Sweep voltage
```

### GPIO (Digital I/O):
```python
gpio_set_dir(GPIO_1, OUTPUT)     # Set direction
gpio_set(GPIO_1, HIGH)           # Write high
value = gpio_get(GPIO_1)         # Read value
```

### ADC (Read Voltages):
```python
voltage = adc_read(ADC0)         # Read voltage
raw = adc_read_raw(ADC0)         # Read raw value
```

### Nodes:
```python
# Digital: D0-D53
# Analog: A0-A5
# Power: TOP_RAIL, BOTTOM_RAIL, GND
# DAC: DAC0, DAC1
# GPIO: GPIO_0 through GPIO_7
# ADC: ADC0, ADC1, ADC2, ADC3
# Special: CURRENT_SENSE_PLUS, CURRENT_SENSE_MINUS
```

---

## ⌨️ Keyboard Shortcuts (ViperIDE)

- `Ctrl+Enter` / `Cmd+Enter` - Run selection or current line
- `Ctrl+S` / `Cmd+S` - Save file
- `Ctrl+/` / `Cmd+/` - Toggle comment
- `Tab` - Autocomplete (after running `from jumperless import *`)

---

## 🔗 More Resources

- Full Documentation: See `VIPERIDEAUTOCOMPLETE_REALITY.md`
- API Reference: See `JUMPERLESS_API_QUICKREF.md`  
- Examples: `/python_scripts/examples/` on device
- Help System: `help()` in console

---

## 💡 Pro Tips

1. **Keep a help() window open** - split screen with ViperIDE
2. **Save frequently used commands** - in a snippet file
3. **Test in REPL first** - then move to script
4. **Use exec() to reload** - faster than re-uploading
5. **Remember the init** - `from jumperless import *` every session

---

## 🎓 ViperIDE Philosophy

**ViperIDE is for:**
- ✅ Quick testing on device
- ✅ Interactive REPL exploration
- ✅ Running/debugging on hardware
- ✅ File management

**ViperIDE is NOT for:**
- ❌ Full IDE autocomplete (use VS Code for that)
- ❌ Static type checking
- ❌ Advanced refactoring

**Use the right tool for the job:**
- **Develop** in VS Code (full autocomplete, type checking)
- **Deploy & Test** in ViperIDE (direct hardware access)

---

**Last Updated:** December 2025

