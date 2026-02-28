# Jumperless Skill for Large Language Models

This document describes how to interact with the Jumperless hardware using
MicroPython commands.  It's written as a **skill specification** for any LLM that
needs to build, test and debug circuits on real hardware via the Jumperless
breadboard.  The goal is to make it easy for a model to generate the correct
Python code, reason about connections, and verify results by interrogating the
board's state.  

> The Jumperless is controlled through a serial REPL running MicroPython.  All
> functions from the `jumperless` module are globally available, so example
> code can call `connect(1, 5)` without importing anything.

---

## Core Concepts

- **Nodes**: Each breadboard hole is a node numbered 1–60; there are also constants for rails, DACs, ADCs, GPIO pins, etc. Nodes may be specified by integer, string name ("d13"), or constant (D13).

  **Breadboard Mapping for DIP Chips**: The Jumperless breadboard is laid out like a standard half-size breadboard. For DIP ICs (like the 555 timer), one side of the chip typically uses nodes 1–4, and the other side (across the center gap) uses nodes 31–34. For example, a 555 timer placed across the gap would have:
    - Pin 1: node 1
    - Pin 2: node 2
    - Pin 3: node 3
    - Pin 4: node 4
    - Pin 5: node 31
    - Pin 6: node 32
    - Pin 7: node 33
    - Pin 8: node 34
  Always use this mapping for DIP chips unless your hardware setup is different.

- **Connections**:  Virtual jumpers are created with `connect(node1, node2)` and
  removed with `disconnect(node1, node2)` (or `disconnect(node1, -1)` to clear
  all).  `is_connected()` returns a truthy `ConnectionState` value.  There are
  fast variants `fast_connect`/`fast_disconnect` that skip LED updates.

- **Context**:  The board has two connection modes – *global* (persisting after
  the Python session ends) and *python* (reverted when the interpreter exits).
  `context_get()` and `context_toggle()` manage this.

- **Slots**:  You can save and load connection sets using `nodes_save()`,
  `switch_slot()`, etc.  Useful when an LLM tries multiple circuit variants.

- **Measurements**:  ADC readings (`adc_get`), DAC control (`dac_set`), INA
  current/power meters (`ina_*`), and GPIO control let the model take
  measurements and stimulate the circuit.

- **Debugging**:  Helper functions such as `print_nets()`, `print_paths()`,
  `get_state()` return structured data for diagnostics.  The model can query
  this state and reason about mismatches between expected and actual wiring.

- **Probe & Clickwheel**:  Not usually needed for automated testing but
  available for manual interaction if the LLM directs a user to touch pads or
  press buttons.

---

## Recommended Workflow for LLMs


1. **Ask clarifying questions**:  When given a high-level task (“build a voltage divider”, “measure the resistance between pins”), first determine which nodes, rails, and measurement functions are appropriate.

2. **Look up example circuits and pin mapping**:  Before generating wiring code, search for a schematic or pinout diagram for the chip you want to use (e.g., 555 timer, op-amp, microcontroller). Figure out which breadboard nodes correspond to each pin on the chip, using the DIP mapping or your hardware setup.

3. **Save current state for quick tests**:  Before making changes, call `get_state()` and save the returned JSON string. This lets you restore the board to its previous configuration after your test.

4. **Plan wiring**:  Use `connect()` commands to lay out the circuit, using the correct node mapping for your chip. Include comments in code describing the intended schematic and pin assignments.

4. **Apply settings**:  Set DAC outputs, pull-ups, GPIO directions, etc., as needed.

5. **Measure & verify**:  Call `adc_get`, `ina_get_current`, or other functions to read values.  Compare results against expected ranges and raise an error or adjust the circuit if they diverge.

6. **Diagnose connections**:  If measurements are wrong, use `is_connected` or inspect `get_state()`/`print_nets()` to ensure the virtual jumpers match the intended schematic.  The LLM should articulate any discrepancies and suggest fixes.

7. **Restore previous state after testing**:  If you only want to test something temporarily, call `set_state(saved_json)` to restore the board to its original configuration. This is faster than manually disconnecting or clearing nodes.

8. **Cleanup**:  Remove any temporary connections (`disconnect` or `nodes_clear()`) to leave the board ready for the next task, or save the configuration to a slot if it's a reusable setup.

**Example for quick test workflow:**
```python
# Save current state
saved_json = get_state()

# Make temporary changes
connect(TOP_RAIL, 8)
connect(8, 7)
# ... (other test commands)

# Restore previous state
set_state(saved_json)
```

---

## Example LLM Prompts and Responses

> **Prompt:** *"Create a 1 kΩ + 2 kΩ voltage divider from TOP_RAIL to GND and
> read the voltage at the midpoint with ADC0."*

**Generated Code:**
```python
# connect top rail to resistor network
connect(TOP_RAIL, 1)          # one end of 1k resistor
connect(1, 2)                 # series connection to 2k resistor
connect(2, GND)               # bottom of divider
connect(1, ADC0)                 # connect midpoint to ADC0 (node 3)
# measure midpoint at node 1 using ADC0
v = adc_get(0)
print("divider voltage:", v)
```

> **Prompt:** *"Verify that D13 is connected to the top rail and, if not,
> report the unexpected net information."*

**Generated Code:**
```python
if not is_connected(D13, TOP_RAIL):
    print("Error: D13 not tied to TOP_RAIL")
    print_nets()
```

> **Prompt:** *"Automate connecting a 555 timer astable circuit and measure its output using Jumperless via USB serial."*

**Generated Code:**
```python
# 555 timer astable circuit wiring (node mapping assumed)
connect(TOP_RAIL, 8)      # VCC to pin 8
connect(TOP_RAIL, 4)      # VCC to pin 4 (RESET)
connect(GND, 1)           # GND to pin 1
connect(8, 7)             # R1: VCC to DISCH
connect(7, 6)             # R2: DISCH to THRS
connect(6, 1)             # C1: THRS to GND
connect(2, 6)             # TRIG to THRS
connect(5, 1)             # C2: CTRL to GND (optional)
connect(3, ADC0)          # OUT to ADC0 for measurement

# Measure output voltage at OUT (pin 3)
v = adc_get(0)
print(f"555 output voltage: {v}")
```

**Port discovery helper:**
Before running any automation you may wish to let the LLM determine which
physical serial port corresponds to which Jumperless function.  A helper
script is provided in this repository to perform that enumeration:

```shell
python jumperless_skill/port_identify.py
```

The script prints a JSON mapping of every available port, flags the ones that
look like Jumperless devices, and (when the OS supports it) reads the USB
interface descriptor so you know which port is “main”, “python_repl”,
“passthrough”, etc.  The same mapping is written to `jumperless_ports.json`
for the remainder of the session; later code can simply open that file and use
its contents when selecting a port.

```python
# example helper that uses the generated JSON
import json
ports = json.load(open('jumperless_ports.json'))
print('Jumperless devices:', [p for p,v in ports.items() if v['is_jumperless']])
```

**Automation Tip:**
To automate this from a host computer, use a Python script with `pyserial` to send these commands to the Jumperless REPL over USB. Example:
```python
import serial

# Ask the user which serial port has the Jumperless REPL
port = input('Enter the path of the Jumperless serial port (e.g. /dev/cu.usbmodemXYZ): ')
with serial.Serial(port, 115200, timeout=1) as ser:
    commands = '''
connect(TOP_RAIL, 8)
connect(TOP_RAIL, 4)
connect(GND, 1)
connect(8, 7)
connect(7, 6)
connect(6, 1)
connect(2, 6)
connect(5, 1)
connect(3, ADC0)
v = adc_get(0)
print("555 output voltage:", v)
'''
    for line in commands.strip().splitlines():
        ser.write((line + '\r\n').encode())
        # read and display any response from the board
        resp = ser.readline().decode(errors='ignore').strip()
        if resp:
            print(resp)
```

---

## Debugging Strategies

- Use `get_state()` to capture a JSON snapshot and compare against an expected
  state stored in a regression test.  This is especially helpful when multiple
  connections should exist.

- When measuring with `adc_get`, first ensure the node is physically reachable
  by connecting it to a known voltage source via the DAC or rails.  Floating
  nodes yield unpredictable readings.

- For timing‑sensitive GPIO toggling, claim pins with `gpio_claim_pin()`
  before driving them and `gpio_release_pin()` afterward.

- Check bus voltages and currents with `ina_get_*` to detect short circuits or
  unpowered sections.

---

## Best Practices

- **Always convert high‑level descriptions to precise node numbers or
  constants.** Ambiguity leads to incorrect wiring.
- **Prefer named constants** (`DAC0`, `GPIO_1`, `TOP_RAIL`, etc.) for clarity
  and autocompletion support.
- **Keep sessions idempotent.**  A good LLM response includes code that leaves
  the board in a known state or documents its assumptions explicitly.
 **Handle errors gracefully.**  If a measurement call returns `None` or a
 value outside an expected range, the code should print a helpful message and
 optionally adjust the circuit.

 **Always connect ADC nodes to a voltage source or relevant signal before reading.**
 Attempting to read from an unconnected (floating) ADC node will yield unpredictable or invalid results. For example, connect the ADC to a rail, DAC, or output node before calling `adc_get()`.

- **Use context mode appropriately.**  By default, run in python context (the
  device resets on exit) unless persistence is desired.

---

## Skill Integration Notes

- This markdown file is consumed by the Jumperless agent at runtime.  When an
  LLM is asked to perform a circuit-building task, it should reference this
  document for API details and examples.
- The agent may append more examples over time or generate online help via the
  existing `help()` function from the MicroPython environment.

---

With this skill in place, an LLM should be capable of expressing test
procedures in natural language and translating them into concrete commands that
exercise the Jumperless hardware, then interpreting results to diagnose and
iterate on the design.
