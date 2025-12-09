# MicroPython Examples Automation System

## 🎯 Overview

The Jumperless firmware now includes an automated system for embedding MicroPython example scripts. This system converts Python files into C string literals, making them accessible from the MicroPython REPL without requiring external storage.

## 📁 File Structure

```
JumperlOS/
├── pythonStuff/
│   ├── ex/                          # Source Python examples
│   │   ├── adc_basics.py
│   │   ├── dac_basics.py
│   │   ├── gpio_basics.py
│   │   ├── interaction_demo.py
│   │   ├── led_brightness_control.py
│   │   ├── node_connections.py
│   │   ├── stylophone.py
│   │   ├── uart_basics.py
│   │   ├── uart_loopback.py
│   │   ├── voltage_monitor.py
│   │   └── README.md               # Example documentation
│   ├── jumperless_module.py        # Main module wrapper
│   └── jumperless.pyi              # Type stub for IDEs
├── scripts/
│   ├── generate_micropython_examples.py  # Generator script
│   └── README_GENERATE_EXAMPLES.md       # Script documentation
└── src/
    └── micropythonExamples.h       # Generated C header (DO NOT EDIT!)
```

## 🔄 Workflow

### Quick Start

```bash
# 1. Edit Python examples
vim pythonStuff/ex/my_example.py

# 2. Regenerate header
python3 scripts/generate_micropython_examples.py

# 3. Rebuild firmware
pio run -t upload
```

### Detailed Process

1. **Edit Source Files**
   - Modify existing examples in `pythonStuff/ex/`
   - Add new `.py` files to `pythonStuff/ex/`
   - Update `jumperless_module.py` or `jumperless.pyi` if needed

2. **Run Generator**
   ```bash
   python3 scripts/generate_micropython_examples.py
   ```
   
   Output:
   ```
   ======================================================================
   Generating micropythonExamples.h
   ======================================================================
   Source directory: /path/to/pythonStuff/ex
   Output file: /path/to/src/micropythonExamples.h
   
   Processing adc_basics.py...
   Processing dac_basics.py...
   ...
   Found jumperless module at /path/to/jumperless_module.py
   Found type stub at /path/to/jumperless.pyi
   
   ✓ Generated /path/to/micropythonExamples.h
   ✓ Processed 10 example files
   ```

3. **Rebuild Firmware**
   ```bash
   pio run              # Build only
   pio run -t upload    # Build and upload
   ```

## 📝 Creating New Examples

### Example Template

```python
"""
Example Title
Brief description of what this demonstrates.

Hardware Setup:
1. Connection instructions
2. Required components
3. Expected behavior
"""

import jumperless as j
import time

print("My Example Demo")

# Your code here
while True:
    # Do something
    time.sleep(0.1)
```

### Best Practices

1. **Always include a docstring** at the top describing the example
2. **Import as `j`**: Use `import jumperless as j` for consistency
3. **Print status messages**: Help users understand what's happening
4. **Include hardware setup**: Document required connections
5. **Handle errors gracefully**: Use try/except for robust demos
6. **Keep it simple**: Focus on demonstrating one concept clearly

### Example Categories

- **Hardware Basics**: DAC, ADC, GPIO, Node connections
- **Communication**: UART, I2C, SPI
- **Interactive**: Probe, encoder, buttons
- **Demos**: LED control, voltage monitoring, audio

## ⚙️ Configuration

### Excluding Files

Edit `scripts/generate_micropython_examples.py`:

```python
exclude_files = {
    '__init__.py',
    'fuck.py',           # Old version of interaction_demo
    'batt.py',           # Incomplete battery test
    'lipo_char.py',      # Incomplete battery test
    'listFiles.py',      # ViperIDE utility
    'my_test.py',        # Add your exclusions here
}
```

### Compile-Time Disabling

In `src/micropythonExamples.h`, comment out examples you don't want:

```c
// #define INCLUDE_STYLOPHONE  // Disabled
#define INCLUDE_GPIO_BASICS    // Enabled
```

Or use category macros:

```c
#define DISABLE_DEMO_EXAMPLES      // Disables LED, Voltage, Stylophone
#define DISABLE_HARDWARE_EXAMPLES  // Disables DAC, ADC, GPIO, Node
#define DISABLE_MACHINE_EXAMPLES   // Disables UART examples
#define DISABLE_ALL_EXAMPLES       // Disables everything
```

## 🔍 How It Works

### 1. Python to C Conversion

The generator script converts Python source files into C++11 raw string literals:

```python
# Python source: adc_basics.py
import jumperless as j
print("Hello")
```

Becomes:

```c
// C header: micropythonExamples.h
#ifdef INCLUDE_ADC_BASICS
const char* ADC_BASICS_PY = R"(import jumperless as j
print("Hello")
)";
#endif
```

### 2. Filesystem Integration

The `FilesystemStuff.cpp` module creates virtual files from these strings:

```cpp
ExampleInfo examples[] = {
    { "/python_scripts/examples/adc_basics.py", ADC_BASICS_PY, "adc_basics.py" },
    // ...
};
```

### 3. MicroPython Access

From the MicroPython REPL:

```python
>>> import os
>>> os.listdir('/python_scripts/examples')
['adc_basics.py', 'dac_basics.py', ...]

>>> exec(open('/python_scripts/examples/adc_basics.py').read())
ADC Basics Demo
Reading ADC channels...
```

## 📊 Current Examples

### Hardware Basics (4 examples)
- `adc_basics.py` - Read analog voltages
- `dac_basics.py` - Set DAC output voltages
- `gpio_basics.py` - Digital I/O operations
- `node_connections.py` - Connect/disconnect nodes

### Communication (2 examples)
- `uart_basics.py` - Basic UART communication
- `uart_loopback.py` - UART loopback test

### Interactive Demos (4 examples)
- `interaction_demo.py` - Probe, encoder, and buttons
- `led_brightness_control.py` - Touch-controlled LED
- `voltage_monitor.py` - Real-time voltage display
- `stylophone.py` - Musical instrument

### Total: 10 examples + module wrapper + type stub

## 🐛 Troubleshooting

### Generator Script Issues

**Problem**: "No such file or directory"
```bash
# Solution: Run from project root
cd /path/to/JumperlOS
python3 scripts/generate_micropython_examples.py
```

**Problem**: "No jumperless module found"
```bash
# Solution: Ensure jumperless_module.py exists
ls pythonStuff/jumperless_module.py
```

### Build Issues

**Problem**: Header not found during compilation
```bash
# Solution: Regenerate and clean build
python3 scripts/generate_micropython_examples.py
pio run -t clean
pio run
```

**Problem**: Out of memory during build
```c
// Solution: Disable some examples in src/micropythonExamples.h
#define DISABLE_DEMO_EXAMPLES
```

### Runtime Issues

**Problem**: Examples not appearing in filesystem
```python
# Solution: Check if examples are enabled
# In src/micropythonExamples.h, ensure defines are uncommented
```

**Problem**: Example has syntax errors
```bash
# Solution: Validate Python syntax before generating
python3 -m py_compile pythonStuff/ex/my_example.py
```

## 📈 Statistics

- **Source files**: 10 Python examples
- **Generated header**: ~2,400 lines
- **Compile time**: ~3 seconds (incremental)
- **Flash usage**: ~50KB for all examples
- **Excluded files**: 4 (test/incomplete scripts)

## 🔗 Related Documentation

- [Example Files README](pythonStuff/ex/README.md)
- [Generator Script README](scripts/README_GENERATE_EXAMPLES.md)
- [Jumperless API Reference](pythonStuff/JUMPERLESS_API_QUICKREF.md)
- [MicroPython Module Docs](pythonStuff/MICROPYTHON_MODULE_COMPLETE.md)

## 🎓 Advanced Usage

### Adding New Categories

Edit `scripts/generate_micropython_examples.py`:

```python
categories = {
    "HARDWARE": ["dac_basics", "adc_basics", "gpio_basics", "node_connections"],
    "DEMO": ["led_brightness_control", "voltage_monitor", "stylophone"],
    "UTILITY": ["readme", "test_runner"],
    "MACHINE": ["uart_basics", "uart_loopback"],
    "MYCATEGORY": ["my_example1", "my_example2"],  # Add here
}
```

Then use:
```c
#define DISABLE_MYCATEGORY_EXAMPLES
```

### Custom Source Directory

Modify `scripts/generate_micropython_examples.py`:

```python
# In main()
examples_dir = project_root / 'my_custom_examples'
```

### Pre-processing Examples

Add validation before generation:

```python
# In generate_header_from_directory()
for py_file in py_files:
    with open(py_file, 'r') as f:
        py_content = f.read()
    
    # Validate syntax
    try:
        compile(py_content, py_file.name, 'exec')
    except SyntaxError as e:
        print(f"Warning: Syntax error in {py_file.name}: {e}")
        continue
```

## 🚀 Future Enhancements

Potential improvements:
- [ ] Add Python syntax validation to generator
- [ ] Support for subdirectories (categories)
- [ ] Automatic README generation from docstrings
- [ ] Example dependency tracking
- [ ] Compressed storage for large examples
- [ ] Hot-reload examples without firmware rebuild

## 📝 Change Log

### 2025-12-09 - Initial Implementation
- Created automated generation system
- Converted 10 examples from manual to automated
- Added comprehensive documentation
- Integrated with existing filesystem code

## 🙏 Credits

- Generator script: Kevin Santo
- Example scripts: Jumperless community
- Documentation: AI-assisted (Claude Sonnet 4.5)

---

**Note**: The generated header file (`src/micropythonExamples.h`) should **never** be edited manually. All changes should be made to the source Python files in `pythonStuff/ex/`, then regenerated using the script.

