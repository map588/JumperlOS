# Local NeoPixel Library Setup

## Overview
JumperlOS now uses a **local copy** of the Adafruit_NeoPixel library instead of the external dependency. This allows us to make custom optimizations specific to Jumperless hardware.

## Why Local Library?

1. **Custom Optimizations**: Can modify the library for Jumperless-specific performance improvements
2. **Hardware-Specific Tuning**: RP2040 PIO optimizations for our exact LED configuration
3. **Future-Proofing**: No dependency on external library updates that might break optimizations
4. **Dirty Flag Integration**: Can integrate dirty-flag optimization directly into the library

## Library Location

```
lib/Jadafruit_NeoPixel/
├── Adafruit_NeoPixel.h          # Main header file
├── Adafruit_NeoPixel.cpp        # Main implementation
├── Adafruit_Neopixel_RP2.cpp   # RP2040 PIO driver
├── rp2040_pio.h                 # PIO program for WS2812
└── library.json                 # PlatformIO library metadata
```

## Configuration Changes

### platformio.ini
```ini
lib_deps = 
	; adafruit/Adafruit NeoPixel  ; REMOVED - using local copy
	bblanchon/ArduinoJson
	...
```

The local library is automatically detected by PlatformIO's Library Dependency Finder (LDF) because it has proper `library.json` metadata.

## Verification

Build output confirms local library usage:
```
Dependency Graph
|-- Jadafruit_NeoPixel @ 1.15.2 (License: LGPL-3.0, Path: /Users/.../lib/Jadafruit_NeoPixel)
```

Compiled object files:
```
.pio/build/jumperless_v5/lib549/Jadafruit_NeoPixel/Adafruit_NeoPixel.cpp.o
.pio/build/jumperless_v5/lib549/Jadafruit_NeoPixel/Adafruit_Neopixel_RP2.cpp.o
```

## Potential Optimizations

Now that we have local control, here are optimizations we can implement:

### 1. **DMA-Based LED Updates** (Most Impactful)
Currently, `rp2040Show()` uses blocking PIO writes:
```cpp
while(numBytes--)
  pio_sm_put_blocking(pio, pio_sm, ((uint32_t)*pixels++)<< 24);
```

**Optimization**: Use DMA to transfer pixel data to PIO FIFO:
```cpp
// Use DMA channel to blast all pixels at once
dma_channel_transfer_from_buffer_now(dma_chan, pixels, numBytes);
// Returns immediately - LED update happens in background!
```

**Benefits**:
- Non-blocking LED updates
- CPU free during 13ms LED refresh
- Core 2 can do other work while LEDs update
- Potential to reduce effective refresh time to near-zero perceived latency

### 2. **Delta Updates** (Advanced)
Track which LEDs actually changed and only send those pixels:
```cpp
void rp2040ShowDelta(uint8_t *pixels, uint8_t *oldPixels, 
                     uint32_t numBytes, bool *dirtyMap);
```

**Benefits**:
- Only refresh changed LEDs
- Massive speedup when few LEDs change (common case)
- Requires tracking previous pixel state

### 3. **Dual-Buffer with Swap**
Use ping-pong buffers to prepare next frame while current frame displays:
```cpp
uint8_t *frontBuffer = pixels;
uint8_t *backBuffer = pixelsAlt;
// Draw to backBuffer, swap when done
```

**Benefits**:
- Eliminate tearing during updates
- Smoother animations
- Better integration with dirty flags

### 4. **PIO Program Optimization**
Current PIO program is generic WS2812. We can optimize for:
- Exact clock speed we use
- Our specific LED strip lengths
- Reduced instruction count

### 5. **Brightness Optimization**
Currently `setBrightness()` requires full LED refresh. We can:
```cpp
// Apply brightness in PIO program or DMA transfer
// Avoid copying entire pixel buffer
```

## Implementation Priority

### Phase 1: DMA-Based Updates (Immediate Win) ⭐⭐⭐
- **Effort**: Medium (1-2 hours)
- **Benefit**: Huge - non-blocking 13ms LED refresh
- **Risk**: Low - well-documented RP2040 feature
- **Files to modify**:
  - `Adafruit_Neopixel_RP2.cpp` - Add DMA channel setup and transfer
  - `Adafruit_NeoPixel.h` - Add DMA channel members

### Phase 2: Integration with Dirty Flags (Already Done)
- **Status**: ✅ Complete (see LED_DIRTY_FLAG_OPTIMIZATION.md)
- **Benefit**: Skip show() calls when no changes
- **Works perfectly with local library**

### Phase 3: Delta Updates (Future)
- **Effort**: High (requires pixel change tracking)
- **Benefit**: Medium (diminishing returns after DMA)
- **Risk**: Medium (more complex logic)

### Phase 4: Dual Buffering (Polish)
- **Effort**: Medium
- **Benefit**: Visual quality improvement
- **Risk**: Low

## Example: Adding DMA Support

### Step 1: Add DMA Members (Adafruit_NeoPixel.h)
```cpp
#if defined(ARDUINO_ARCH_RP2040)
  PIO pio;
  int pio_sm;
  int pio_program_offset;
  int dma_channel;        // ADD THIS
  bool use_dma;           // ADD THIS
#endif
```

### Step 2: Initialize DMA (Adafruit_NeoPixel.cpp::begin())
```cpp
#if defined(ARDUINO_ARCH_RP2040)
  dma_channel = dma_claim_unused_channel(true);
  use_dma = (dma_channel >= 0);
  
  if (use_dma) {
    dma_channel_config c = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_dreq(&c, pio_get_dreq(pio, pio_sm, true));
    dma_channel_configure(dma_channel, &c, 
                         &pio->txf[pio_sm], // dest
                         NULL,              // src (set in show())
                         0,                 // count (set in show())
                         false);            // don't start yet
  }
#endif
```

### Step 3: Use DMA in show() (Adafruit_Neopixel_RP2.cpp)
```cpp
void Adafruit_NeoPixel::rp2040Show(uint8_t *pixels, uint32_t numBytes) {
  if (!pio || (pio_sm < 0)) return;
  
  if (use_dma && dma_channel >= 0) {
    // DMA path - non-blocking!
    dma_channel_transfer_from_buffer_now(dma_channel, pixels, numBytes);
    // Could return immediately here for async operation
    // For now, wait for completion to maintain compatibility
    dma_channel_wait_for_finish_blocking(dma_channel);
  } else {
    // Fallback to blocking PIO
    while(numBytes--)
      pio_sm_put_blocking(pio, pio_sm, ((uint32_t)*pixels++)<< 24);
  }
}
```

## Testing

After modifications:
1. **Compile**: `pio run -e jumperless_v5`
2. **Upload**: `pio run -e jumperless_v5 -t upload`
3. **Verify**: Check Core 2 timing debug output
4. **Benchmark**: Run 50-toggle test to measure improvement

## License Note

Adafruit_NeoPixel is licensed under LGPL-3.0. Our modifications:
- Must remain open source (already satisfied)
- Must document changes (this file + git history)
- Can be used in commercial projects (Jumperless is open hardware)

## Maintenance

To update from upstream Adafruit library:
```bash
cd lib/Jadafruit_NeoPixel
# Compare with upstream
diff -u Adafruit_NeoPixel.cpp ~/.platformio/lib/Adafruit_NeoPixel_ID28/Adafruit_NeoPixel.cpp

# Manually merge changes if needed
# Keep our optimizations!
```

## Files Modified from Original

### Current Differences
- `library.json` - Added for PlatformIO integration
- No other changes yet - ready for optimization!

### Planned Modifications
- [ ] DMA support in `Adafruit_Neopixel_RP2.cpp`
- [ ] DMA members in `Adafruit_NeoPixel.h`
- [ ] Non-blocking show() option

## Related Documentation

- **LED_DIRTY_FLAG_OPTIMIZATION.md** - Dirty flag system (uses local library)
- **CORE2_LOOP_OPTIMIZATION.md** - Core 2 performance optimizations
- **PERFORMANCE_OPTIMIZATIONS_ROUND2.md** - Previous optimization work

## Success Metrics

With local library + DMA optimization, we should achieve:
- **Current**: 13ms blocking LED refresh
- **Target**: <100μs perceived latency (DMA transfer setup time)
- **Core 2 Loop**: Drop from 15ms to ~2ms when LEDs active
- **Connect Speed**: >500 Hz (currently ~304 Hz)

## Next Steps

1. ✅ **Migrate to local library** (COMPLETE)
2. ✅ **Implement dirty flags** (COMPLETE - see LED_DIRTY_FLAG_OPTIMIZATION.md)
3. ⏭️ **Add DMA support** (NEXT - see example above)
4. ⏭️ **Benchmark improvements**
5. ⏭️ **Optimize PIO program if needed**

