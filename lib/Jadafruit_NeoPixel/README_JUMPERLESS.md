# Jadafruit_NeoPixel - Local Modified Copy

This is a local copy of [Adafruit_NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel) v1.15.2 maintained within the JumperlOS repository for custom optimizations.

## Why Local Copy?

1. **Performance Optimizations**: Custom RP2040 optimizations for Jumperless hardware
2. **Dirty Flag Integration**: Works with JumperlOS LED dirty flag system
3. **Future DMA Support**: Will add non-blocking DMA-based LED updates
4. **Stable Base**: No unexpected upstream changes breaking optimizations

## Files

- `Adafruit_NeoPixel.h` - Main header (RP2040-specific declarations)
- `Adafruit_NeoPixel.cpp` - Core implementation
- `Adafruit_Neopixel_RP2.cpp` - RP2040 PIO driver
- `rp2040_pio.h` - PIO program for WS2812 bit-banging
- `library.json` - PlatformIO metadata

## Modifications from Upstream

### Current
- None yet - clean copy ready for optimization

### Planned
- [ ] DMA-based `rp2040Show()` for non-blocking LED updates
- [ ] Optional async operation mode
- [ ] Performance metrics and timing hooks

## Integration with JumperlOS

Used by `LEDs.cpp` via the `ledClass` wrapper:

```cpp
Adafruit_NeoPixel bbleds(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel topleds(LED_COUNT_TOP, LED_PIN_TOP, NEO_GRB + NEO_KHZ800);
```

The `ledClass::show()` method implements dirty flag optimization:
```cpp
void ledClass::show(void) {
  if (splitLEDs == 1 && topDirty) {
    topleds.show();  // Uses Adafruit_NeoPixel::show()
    topDirty = false;
  }
  if (bbDirty) {
    bbleds.show();   // Uses Adafruit_NeoPixel::show()
    bbDirty = false;
  }
}
```

## Upstream Source

- **Repository**: https://github.com/adafruit/Adafruit_NeoPixel
- **License**: LGPL-3.0
- **Version**: 1.15.2
- **Commit**: (frozen at migration date)

## License

This code maintains the original LGPL-3.0 license from Adafruit. All modifications will be documented and remain open source.

---

**See also**: `LOCAL_NEOPIXEL_LIBRARY.md` in the project root for detailed optimization plans and implementation guide.


