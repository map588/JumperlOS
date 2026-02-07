# Component Placement Feature

## Goal
Add firmware support for placing components with pin offsets in slot YAML files. When a user says "place SSD1306 at row 20", the firmware auto-creates bridges and assigns net names.

---

## Proposed YAML Format

```yaml
# In slotx.yaml
parts:
  - name: "SSD1306 OLED"
    row: 20  # Pin 1 location
    pins:
      GND: {offset: 0, connect: GND}
      VCC: {offset: 1, connect: TOP_RAIL}
      SCL: {offset: 2, connect: GPIO_1}
      SDA: {offset: 3, connect: GPIO_2}

bridges:
  # Auto-generated from parts section:
  - [20, GND]      # Net name: "OLED_GND"
  - [21, TOP_RAIL] # Net name: "OLED_VCC"
  - [22, GPIO_1]   # Net name: "OLED_SCL"
  - [23, GPIO_2]   # Net name: "OLED_SDA"
```

---

## Implementation Tasks

### 1. YAML Parser Extension
- [ ] Add `parts:` section parsing in `States.cpp` `fromYAML()`
- [ ] Define `PartDefinition` struct with name, row, pins array
- [ ] Store parts in `JumperlessState`

### 2. Part → Bridge Expansion
- [ ] On load, expand parts into bridges: `row = part.row + pin.offset`
- [ ] Create net names: `{PART_NAME}_{PIN_NAME}` (uppercase, underscores)
- [ ] Use existing `DisplayState::setNetName()` for custom names

### 3. Python API
- [ ] `place_part(name, row, pins_json)` - places a part
- [ ] `remove_part(name)` - removes all bridges/names for a part
- [ ] `list_parts()` - returns placed parts

### 4. LED Outline (Optional)
- [ ] Render part outline on LEDs (dim glow for pin locations)
- [ ] Different colors for power/signal/NC pins

---

## Files to Modify

| File | Changes |
|------|---------|
| `States.h` | Add `PartDefinition` struct, `parts[]` array to `JumperlessState` |
| `States.cpp` | Parse `parts:` in YAML, expand to bridges |
| `FileParsing.cpp` | Handle part expansion during slot load |
| `JumperlessMicroPythonAPI.cpp` | Add `jl_place_part()`, `jl_remove_part()` |
| `mod_jumperless.c` | Expose Python functions |

---

## Testing

1. Create `slot1.yaml` with a `parts:` section
2. Load slot 1
3. Verify bridges created with correct net names
4. Check LEDs show part outline (if implemented)

---

## Notes

- Parts are just syntactic sugar - they expand to regular bridges
- Net names persist across saves (stored in DisplayState)
- Parts can overlap if user places them badly (firmware doesn't prevent this)
