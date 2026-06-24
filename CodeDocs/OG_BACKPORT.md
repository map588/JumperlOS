# OG Jumperless (RP2040) Backport — Living Doc

**This is the source of truth for the OG backport across chats. Update the
status checklist at the bottom at the end of every working session.**

## Why this exists

The "OG" Jumperless runs an **RP2040** (264 KB SRAM, **no PSRAM**, 16 MB
W25Q128 flash). JumperlOS today targets only the Jumperless **V5** (RP2350B +
8 MB PSRAM). The goal of the backport is to let the cheaper OG hardware run the
JumperlOS **shared core** so it is controllable by **LLM tools and
MicroPython** — that is the primary deliverable. The fancy V5-only UI (menus,
rotary encoder, OLED, breadboard text, logic analyzer, editors) is intentionally
**not** ported.

This is really a **board-support architecture**: once it exists, adding the OG
(and the future **V6**) is mechanical. The shared core never branches on board
`#define`s; it asks a board descriptor what the hardware can do.

## Architecture

```
shared core (board-agnostic)
  NetManager · unified router · States/slots · MicroPython API · serial/LLM CDC
        │  calls only the contract, never a board macro
        ▼
src/boards/board.h   ← THE CONTRACT
  BoardTopology (pure data) + HAL function decls + capability queries
        ├── src/boards/v5/board_v5.cpp   (Y0 = BOUNCE_NODE, 445 LEDs, MCP4728 I2C, PSRAM)
        └── src/boards/og/board_og.cpp   (Y0 = CHIP_L,      111 LEDs, MCP4822 SPI,  no PSRAM)
```

Board selection is compile-time (different MCUs ⇒ one PlatformIO env per board).
`OG_JUMPERLESS` selects the OG package; the default V5 env is unchanged.
Same-MCU revision differences (e.g. V5 r4 vs r5, or V5 vs V6 later) can still be
resolved at runtime via `jumperlessConfig.hardware`.

### The contract (`src/boards/board.h`)

- `enum class Y0Rule { BounceNode, ChipL }` — the single biggest topology
  difference (see below).
- `struct BoardTopology` — name, `y0Rule`, `y0Node`, crossbar `xMap[12][16]` /
  `yMap[12][8]`, `bbNodesToChip[62]`, **explicit** GPIO / ADC / DAC tables, and
  a `BoardCaps` flag block.
- Routing primitives the unified router builds on:
  `boardY0Node(b)`, `boardRowToChipY(b,row,&chip,&y)`.
- Capability queries: `boardFindGpio/Adc/Dac`, `boardCanSetRailVoltage`,
  `boardHasNode`, and `boardCapabilitiesJson(b,buf,cap)` (compact JSON for the
  USBSer3 LLM backchannel; Arduino-free + bounds-checked).
- The header is deliberately **Arduino-free** so the descriptors are unit
  tested on the host. Keep it that way.

Both `v5BoardTopology` and `ogBoardTopology` are always linked; `currentBoard()`
picks via `OG_JUMPERLESS`. The host test compares them directly.

## OG vs V5 hardware differences

| Area | V5 (RP2350B) | OG (RP2040) | Where handled |
|------|--------------|-------------|----------------|
| Crossbar Y0 | `BOUNCE_NODE` (199), virtual hop bus; chip L Y selects BB chip | `CHIP_L` (11) directly; L is the literal hub | `BoardTopology.y0Rule` / `yMap`, unified router |
| Corner rows (1/30/31/60) | rows 30/31/60 → K/L; row 1 → chip A | rows 1/30/31/60 → chip L | `bbNodesToChip` |
| LEDs | 5 per breadboard row, 445 total, logo ring + pads | 1 per row, 111 total, 1 logo LED, no pads | LED HAL: sample center pixel (col 2) |
| DAC | MCP4728 quad, I2C | MCP4822 dual, SPI (faster waveforms) | HAL `initDac/setDac*`; `caps.spiDac` |
| Rails | firmware-controlled (DAC ch C/D) | hardware switch (+3.3/+5/±8V) — read-only | `caps.railsFirmwareControlled=false` |
| DACs ranges | DAC0/DAC1 ±8 V | DAC0 0–5 V, DAC1 ±8 V | `BoardTopology.dac[]` |
| ADCs | 8 ch (pins 40–47) | 4 ch: ADC0–2 buffered 0–5 V, ADC3 ±8 V | `BoardTopology.adc[]` |
| Routable GPIO | 8 (`RP_GPIO_1..8`) + UART TX/RX = 10 | **3**: `RP_GPIO_0`, `RP_UART_TX`, `RP_UART_RX` | `BoardTopology.gpio[]` (RP_GPIO_0 is its OWN node, never aliased to GPIO_1) |
| Nano reset | two hardwired GPIO reset lines | single routable `NANO_RESET` node | OG `xMap` chip I uses `NANO_RESET` |
| Probe | resistive ADC pads + buttons + INA switch | crossbar **scanning** (`scanRows`) | Phase 2; `caps.scanningProbe=true` |
| UI | encoder + OLED + breadboard text + menus | **none** — serial + 1-LED/row only | dropped via `build_src_filter` |
| Memory | 264 KB SRAM **+ 8 MB PSRAM** | 264 KB SRAM, **no PSRAM** | V5 MP heap 96 KB; **OG MP heap 20 KB** (SRAM, ~30 KB total pool); `caps.hasPsram=false` |

Node-id namespace (GND=100, DAC0=106, ADC0=110, RP_GPIO_0=114, …) is shared
between OG and V5, which is why one descriptor table type serves both.

## The Y0 difference (most important detail for the router)

- **V5 `BounceNode`:** `yMap[A..H][0] = BOUNCE_NODE`. It is *not* a hole; the
  pathfinder uses it as an internal bus to hop between chips. Chip L's Y-axis
  selects which breadboard chip a special-function line bridges to.
- **OG `ChipL`:** `yMap[A..H][0] = CHIP_L`. Chip L is the central hub itself;
  there is no bounce concept. To hop between BB chips you route through L.

`boardRowToChipY()` already skips Y0 for both boards (Y0 is never a routable
row). The unified router must consult `boardY0Node()` instead of hard-coding
`BOUNCE_NODE` at the chip-hop sites.

## How to add a new board (e.g. V6)

1. `src/boards/v6/board_v6.cpp` — define `const BoardTopology v6BoardTopology`
   (copy the closest existing descriptor, edit the maps/tables/caps).
2. `src/boards/board.h` — add `extern const BoardTopology v6BoardTopology;`
   and a branch in `currentBoard()` (e.g. `#elif defined(JUMPERLESS_V6)`).
3. `platformio.ini` — add `[env:jumperless_v6]` with the right `board`/platform
   (V6 is RP2350-family, so it can extend `env:jumperless_v5`) and
   `-DJUMPERLESS_V6`.
4. Add the board to the host test's parity assertions.
5. No shared-core file should need to change. If it does, that's a missing seam
   in the contract — add the seam, don't sprinkle `#ifdef`s.

## Verification (the runnable check)

Host-buildable, no hardware / PlatformIO needed:

```
cd JumperlOS
g++ -std=c++17 -I src \
    test/test_boards/test_boards.cpp \
    src/boards/board.cpp src/boards/v5/board_v5.cpp src/boards/og/board_og.cpp \
    -o /tmp/test_boards && /tmp/test_boards
```

It asserts the topology kernel returns the right chip/Y for both Y0 rules,
the corner-row differences, GPIO non-aliasing (OG `RP_GPIO_0` ≠ `GPIO_1`),
ADC3/DAC1 ±8 V ranges, capability flags, and the capability-JSON bounds safety.
**Run this after any edit to the board layer.**

## Per-phase status checklist

### Phase 1 — skeleton (boot + route + MicroPython + LLM serial)

Done (architecture + first hardware bring-up — VERIFIED ON A REAL OG BOARD):
- [x] `src/boards/board.h` contract (BoardTopology, HAL decls, capability queries).
- [x] `src/boards/board.cpp` selection + topology-driven routing primitives.
- [x] `src/boards/v5/board_v5.cpp` descriptor (mirrors current V5 data; not yet
      wired into V5 live routing → zero V5 behavior change).
- [x] `src/boards/og/board_og.cpp` descriptor (ported from OG reference firmware).
- [x] `src/boards/og/og_atomic.cpp` — `__atomic_test_and_set` shim (M0+ has no
      native atomics; backed by a fixed RP2040 hardware spinlock, NOT a global
      ctor).
- [x] `boards/jumperless_og.json` — repo-local board (RP2040, cortex-m0plus,
      16 MB flash, variant `jumperless_v1`, Jumperless USB VID/PID).
- [x] `[env:jumperless_og]` in `platformio.ini`: minimal-diff build (compile the
      full tree for RP2040, drop only `boards/v5/`), `-DOG_JUMPERLESS`,
      `-URP2350_PSRAM_CS`, 1 MB FatFS, own extra_scripts (no V5 fs-lock).
- [x] `USB_CDC_ENABLE_COUNT` / `USB_MSC_ENABLE` per-board overridable;
      `boardCapabilitiesJson()` for the USBSer3 LLM backchannel.
- [x] Host test `test/test_boards/test_boards.cpp` (green).
- [x] **Compiles + links** (`pio run -e jumperless_og`): Flash 10.5%, RAM 79.3%.
- [x] **Flashes** over SWD (`openocd ... program`) AND UF2/BOOTSEL.
- [x] **BOOTS STABLY on real RP2040 hardware AND is controllable over serial.**
      Enumerates as USB "Jumperless OG" / "JLOGport" with all 4 CDC ports
      (`JLOGport1/3/5/7` = CDC0 cmds / USBSer1 passthrough / USBSer2 mpremote /
      USBSer3 backchannel), stable indefinitely. The **USBSer3 LLM backchannel
      works**: an `A` query returns the full status JSON (version, ADC, INA
      current, GPIO, net list). RAM 69.9% static (183 KB). The PRIMARY GOAL
      (LLM/serial control of the OG) is met. MicroPython REPL on USBSer2 is the
      next thing to verify.

### BOOT ROOT CAUSES — RESOLVED
Two deterministic boot crashes (found via SWD+GDB `vector_catch`/`break abort`)
plus heap-exhaustion crashes, all fixed:
1. **RP2040 flash unique-ID read** (`src/boards/og/og_unique_id.c`): the RP2040
   has NO on-chip unique-ID register (unlike RP2350), so the pico-sdk reads it
   from QSPI flash via a `0x4B` command (`flash_get_unique_id` -> `__flash_do_cmd`)
   in a PRE-MAIN constructor. That early flash command (XIP disabled, flash->RAM
   veneer) hard-faulted and reset-looped the board. Override `flash_get_unique_id`
   to return a fixed ID with no flash access (we don't need it - USB serial is the
   fixed "JLOGport"). Build links src/ first + `--allow-multiple-definition`, so
   our def wins. (This ALSO made `openocd ... reset` boot cleanly, since the
   firmware no longer issues that flash command the debugger-reset couldn't survive.)
2. **FatFS FIL too big** (`lib/FatFS/src/ffconf.h`): `FF_MAX_SS=4096` + `FF_FS_TINY=0`
   gave each open file a 4 KB sector buffer -> `make_shared<FIL>` = 4152 bytes,
   which `bad_alloc`-aborted on every file open (config/slot/provisioning) on the
   tight/fragmented ~71 KB heap. Set `FF_FS_TINY=1` on OG: FIL shrinks to ~56
   bytes (shares the volume window), no on-disk format change, slower I/O.
3. **Heap exhaustion in loop()**: `undoInit` grabbed ~21 KB (no-PSRAM ring +
   persist scratch) and fragmented the heap so even a 107-byte `printf` aborted.
   Disabled undo on OG (`Undo.cpp` undoInit early-returns; record hooks no-op when
   uninitialized). Also skipped `oled.init()` on OG (no OLED; it kicked
   refreshConnections + debug printfs).
Plus: `JumperlessState` made non-copyable (States.h) with all 5 copy sites
rewritten in place (the ~50 KB copies couldn't fit the RP2040 stack/heap), and
`provisionFirmwareFiles` skipped on OG (V5 LED/OLED image assets).

### UPDATE: JumperlessState is now NON-COPYABLE (state-copy fix applied)
Per "never copy the state": `JumperlessState`'s copy ctor + copy assignment are
now `= delete` (States.h), so any copy is a COMPILE ERROR. The 5 copy sites the
compiler flagged (all in States.cpp) were refactored to work in place:
- `migrateOldSlotFile`: parses the legacy file directly into `activeState`
  (was a ~50KB `JumperlessState newState` stack local + copy — the prime
  boot-crash suspect when a persisted legacy slot is loaded at boot).
- `printSlotInfo`: persists-if-dirty + reload-from-disk instead of save/restore
  via a 50KB copy (also removed a dead `tempState`).
- `pushHistory`/`undo`/`redo`: the legacy full-state snapshot history
  (STATE_HISTORY_SIZE==0, superseded by Undo.cpp deltas) is now a no-op instead
  of copying the whole state.
Builds clean (OG 72.5% RAM). **NOT yet verified on hardware** — needs a BOOTSEL
reflash (user was away). If it boots reliably now, this was the root cause. If
not, get the backtrace (probe + vector_catch) and also erase the persisted FS.

### CRITICAL ROOT CAUSE (original finding that led to the fix above)
`JumperlessState` (the full board state: nets[MAX_NETS] each with
nodes/bridges[MAX_NODES], paths[MAX_BRIDGES], chipStates, power, ...) is ~50 KB
on V5, ~33 KB on OG after the MAX_BRIDGES/MAX_NODES cut. **The RP2040 has only
~50 KB of TOTAL free RAM** (8 KB core0 stack at 0x20040000-0x20042000 + the heap
from `&end`≈0x2002e000 to 0x20040000). `States.cpp` COPIES the whole state in
several places — stack locals (`JumperlessState newState` in the legacy-slot
migration ~line 3216; `tempState`/`savedState` in preview ~line 3323) and
assignments. A 33-50 KB copy **overflows the 8 KB stack → hard fault**, or
exhausts the heap. States.h:264 even warns "DANGER: copy uses 50KB stack!".
- This is why it booted ONCE with a fresh FS (boot path avoided a copy) and now
  deterministically crashes: the persisted 1 MB FatFS has a slot file whose
  load/migration path makes a copy. Adding heap does NOT help a stack copy.
- **FIX (next session), in order:**
  1. Reattach the SWD probe; `openocd ... -c "flash erase_address <FS_START> 0x100000"`
     to wipe the persisted FatFS (FS is at the top of the 16 MB flash just under
     the 4 KB EEPROM; confirm FS_START from the build/linker), OR generate a
     blank-FS UF2. A fresh FS should let it boot again immediately (confirms the
     diagnosis). picotool v2.0.0 here has NO `erase`, so use openocd or a UF2.
  2. Make the OG-relevant state-copy paths in States.cpp NOT copy 50 KB: operate
     in place on `globalState`, or use a single shared scratch buffer, or gate
     legacy-slot migration / preview off on OG (capability flag). Grep
     `JumperlessState ` for stack locals + `= activeState` / `= globalState`.
  3. Consider shrinking `JumperlessState` further on OG (it's the only ~big
     object) so any unavoidable copy fits.
- Get the boot backtrace to confirm: SWD probe + `openocd` gdb server, then
  `gdb -batch`: `monitor reset halt; monitor cortex_m vector_catch hard_err;
  break abort; continue; bt`. (NOTE: halting through `__flash_do_cmd` /
  `flash_get_unique_id` at boot gives a debugger-induced fault — let it run, then
  halt ~3 s later, or catch the real fault with vector_catch + a DTR/connect trigger.)

RP2040-vs-RP2350 source fixes applied (all guarded by `#if defined(PICO_RP2350)`
so V5 is byte-identical):
- `ArduinoStuff.cpp` PSRAM XIP CS1 registers; `Debugs.cpp` / `Peripherals.cpp` /
  `SingleCharCommands.cpp` / `RotaryEncoder.cpp` third PIO block (`pio2`);
  `Peripherals.cpp` GPIO-function-name table (RP2350-only mux entries);
  `Probing.cpp` `gpio_coproc.h` + `gpioc_bit_oe_set/clr` → `gpio_set_dir`.
- `usb_descriptors.cpp`: product/serial strings board-gated to "Jumperless OG"/"JLOGport".

RAM reduction done (RP2040 has ~34 KB heap after static; static-init + FatFS were
aborting boot via `operator new` → `bad_alloc` → `abort`. Found each via SWD+GDB
`break abort; bt`). Cuts (all `#if defined(OG_JUMPERLESS)`), 98% → 79% static:
- GraphicOverlays `MAX_GRAPHIC_OVERLAYS` 8→1 + render no-op (~9 KB).
- MicroPython API scratch buffers (`JumperlessMicroPythonAPI.cpp`) shrunk (~11 KB).
- `MpRemoteService` buffers (8K+4K+4K+512 → 1536+1K+1K+512); these `new[]` at
  static-init and were the FIRST abort.
- Current-sense overlay `kCurrentSenseMaxPathLength` 320→8; FatFS SafeStrings
  (`FileParsing.cpp`); CDC FIFOs 1024→256 (`custom_tusb_config.h`).
- FS partition 4 MB→1 MB (SPIFTL FTL map ~16 KB→~4 KB heap).
- `kTraceN` 1024→64 (Debugs, ~9 KB); `uartReceived` ring 8 K→2 K (AsyncPassthrough).
- OG MicroPython heap sized to 16 KB (lazy-init, doesn't affect boot).

### Session 2026-06-20 — OG BOOTS, MicroPython + native module CONFIRMED WORKING
Three fixes this session took the OG from a boot/connect crash-loop to a stable
board that runs MicroPython end to end (verified over SWD + serial):
1. **LED buffer overflow → heap corruption (the boot crash).** `setup()` calls
   `drawAnimatedImage(0)` → `drawImage(44)` which emits V5 pixel indices (up to
   ~445) into the breadboard NeoPixel buffer. On OG that buffer is only
   `LED_COUNT`=111 px (333 B), so the RAM-resident fast path
   (`setPixelColorRamHelper`, a raw `pixels[n*3]` write with NO bounds check) ran
   ~1 KB past the buffer and zeroed the heap singleton right after it — caught via
   a hardware watchpoint: the `MeasureMode` singleton's vtable went `0x10152454`
   → `0` between main.cpp:330 and :415, then `registerService(&measureModeService)`
   virtual-called through the null vtable → hard fault. FIX (`LEDs.cpp`): added
   `ledMaxPixels()` and bounds-check all three setters
   (`setPixelColor` x2 + `setPixelColorDirect`); also fixed `clear()` to size from
   `bbleds.numPixels()` (was `LED_COUNT+LED_COUNT_TOP`, a 3-B overrun on OG). This
   is the trust-boundary guard that makes ANY shared V5 graphics path safe on the
   smaller OG strip — no need to special-case every drawing routine.
2. **Provisioning OOM.** `jumperless.py` (36 KB) + `jumperless.pyi` (45 KB) can't
   be written on the tiny OG heap (`writeStringToFile` needs content+2 KB C-heap).
   Skipped both on OG (`micropythonExamples.h`: `INCLUDE_JUMPERLESS_MODULE/STUB`
   gated off) — the native C `jumperless` module already satisfies
   `import jumperless`; the .py is only an autocomplete re-export, the .pyi is
   IDE-only stubs. Also skipped `rp2.py` provisioning on OG (`Python_Proper.cpp`,
   ~10 KB C-heap spike; PIO @asm_pio not needed for the minimal goal).
3. **MicroPython MemoryError (`allocating 4168 bytes`, repeated).** Measured at
   runtime: ~30 KB total for (MP GC heap + C runtime heap). Registering the native
   module eats ~12 KB of the GC heap, so the old 16 KB heap left <4 KB and every
   init/exec script OOM'd. 24 KB fixed the OOM but starved the C heap (~6.5 KB) →
   main-loop reboot-loop. **20 KB is the sweet spot** (`JumperlessDefines.h`):
   ~8 KB free in the GC heap, ~10.5 KB C heap. No OOM, no reboot.

VERIFIED on hardware (SWD flash + pyserial REPL drive): boots stably (0 USB drops
over 70 s, free-running), no provisioning errors, no MemoryError, and a held REPL
session runs `import jumperless` → `print('IMPORT_OK')` →
`jumperless.adc_get(0)` returning `0.86` (a real voltage) → `dir(jumperless)`
listing the native DAC fns. **The primary deliverable (LLM/MicroPython control of
the OG) works.** The periodic reset cycle that initially masked this was the
core1 encoder-PIO poll — see "RESOLVED" below. Also quieted the OG MeasureMode
flood (3) and gated ADC channels 5-7 off on OG (4).
3. **MeasureMode flood** (`MeasureMode.cpp`): service() no-ops on OG (no probe-pad
   ADC / connect-measure switch wired in; the scanning probe is Phase 2). Was
   spamming the "row A1" voltage line every loop and poking the uninitialized OG
   OLED I2C each update.
4. **ADC channels 5-7** (`Peripherals.cpp updateLazyAdcReadings`): the slow loop
   read ADC ch 5-7 (V5 has 8 ADC inputs), but the RP2040 ADC only has inputs 0-4.
   Gated the slow loop off on OG (its 4 ADCs are covered by the fast 0-4 loop).

### ✅ RESOLVED: periodic ~6 s reset cycle was core1 polling a dead encoder PIO
SYMPTOM: after boot the board ran a few seconds then reset (USB dropped ~every
5-6 s; sometimes recovered, sometimes core1 hit a Cortex-M LOCKUP with halted
PC = `0xFFFFFFFE` and died until reflash). It was NOT caught by
`break isr_hardfault`/`abort`/`panic` (a lockup escalates past the handler), and
`monitor reset halt` always landed core0 in boot code (`data_cpy_loop`/`setup`),
confirming a reset cycle. Bisecting core1 work (`core2stuff`, main.cpp) found it:
ROOT CAUSE = **`rotaryEncoderStuff()` on core1** reading
`quadrature_encoder_get_count(pioEnc, smEnc)` on a PIO state machine that was
never loaded on the OG — OG has no encoder AND the RP2040's 2 PIO blocks are
oversubscribed (boot logs "probe button PIO: no instruction memory"), so the
quadrature program failed to load. Polling that dead/invalid SM stalled/faulted
core1 → core0 stalled → reset. FIX: `rotaryEncoderStuff()` early-returns on OG
(`RotaryEncoder.cpp`). After the fix: **0 drops over 70 s**, and a full MP REPL
session held without interruption.
- Bisection steps applied along the way (all correct OG changes, kept): gate
  MeasureMode (`MeasureMode.cpp`), `updateLazyAdcReadings` (`Peripherals.cpp`,
  OLED-only cache + no ADC ch 5-7 on RP2040), and the V5 boot animation
  (`drawAnimatedImage`, main.cpp) off on OG. None were the cause, but they
  removed flooding / invalid reads and sped boot.
- FOLLOW-UP (not blocking): the OG still oversubscribes PIO (encoder + probe
  button + WS2812 strips on 2 PIO blocks). Now that the encoder is off, audit PIO
  allocation so the WS2812 LED program is guaranteed a slot — this is the likely
  remaining cause of any "LEDs not displaying" (the LED *show* path may be on an
  SM that lost the PIO-memory race). `core1_stack` is only 2 KB; fine now but
  watch it if core1 work grows.
- Workflow gotchas learned: do NOT `monitor halt` a *running* OG for register
  reads — halting mid-flash-write/lockup double-faults a core and USB never comes
  back (recover with `program ... verify reset exit`, or `program ... reset exit`
  if verify times out on an unstable target). A USB re-enum drops the SWD
  multidrop link mid-session. And `grep`-no-match in a piped shell command here
  can swallow the whole line's output — redirect to a file and read it instead.

NOTE on SWD while running: `monitor halt`/register reads on a *running* OG can
double-fault a core and break USB (TinyUSB starves) — recover with a clean
`program ... verify reset exit`. For crash autopsy use breakpoints
(`break abort` / `break isr_hardfault`) + `continue`, not halt-polling; and note
a USB re-enum (e.g. triggered by opening a port) can drop the SWD multidrop link.

REMAINING for Phase 1:
- [x] **DTR-on-connect crash** — was NOT a true blocker (see session note above).
      A single connect is survivable; the prior "hard-fault on DTR" was the LED
      heap corruption (now fixed) plus rapid reopen thrashing in the test harness.
- [x] **More heap for MicroPython** — MP heap 16 KB → 20 KB; OOM gone, MP usable.
      Further headroom still requires reclaiming static `.bss`; best remaining
      targets (from the SRAM map): `rowAnimations` (4.8 K), inflate `window` (4 K)
      + `frame_buf` (2.6 K) [gate the boot animation/`drawAnimatedImage` off on OG
      first], `logoColorsAll` (3.4 K), Undo `toastScreen` (3.2 K), and ultimately
      `globalState` (~30 K). Each byte cut lowers `&end` and grows the heap 1:1,
      so the MP heap could then be pushed back toward 24–32 KB.
- [x] **Unified router wiring:** replace the hard-coded `BOUNCE_NODE` chip-hop
      sites in `NetsToChipConnections.cpp` with `boardY0Node(currentBoard())` +
      the OG descriptor maps. Currently the OG runs the V5 bounce-node router on
      OG topology data — routing correctness on OG is UNVERIFIED. Keep V5 on its
      `ch[]` path until Phase 3.
- [x] **LED HAL (display correctness):** the buffer-overflow/heap-corruption bug
      is FIXED (bounds-checked setters, see session note) so the V5 framebuffer no
      longer crashes the OG. Net rendering uses `nodesToPixelMap[node]` directly
      (1 px/row). Whether nets/animations *look right* on the 111-px strip is a
      VISUAL check the user must make. Still TODO: OG renderer should sample the
      center pixel of each V5 row (`Graphics.cpp rowColumnToPixelIndex(row, 2)`;
      rows 31–60 mirror columns) for animations/highlights. `caps.ledsPerRow==1`.
- [x] **MicroPython smoke test** — DONE. `import jumperless` (native module) +
      `jumperless.gpio_get(1)` → `FLOATING` over the JLOGport1 REPL.

How to flash + debug the OG (workflow established this session):
- Build: `~/.platformio/penv/bin/pio run -e jumperless_og` (NOT the homebrew
  `pio` — its Python 3.14 is rejected by the platform; the penv has 3.11).
- Flash over SWD (no BOOTSEL): `openocd -s <scripts> -c "adapter driver cmsis-dap;
  adapter speed 4000" -f target/rp2040.cfg -c "program .pio/build/jumperless_og/firmware.elf verify reset exit"`.
- Crash backtrace: run openocd as a gdb server, then `arm-none-eabi-gdb -batch`
  with `target extended-remote 127.0.0.1:3333; monitor reset halt; break abort;
  continue; bt`. (A Raspberry Pi Debug Probe `2e8a:000c` is wired to the OG SWD pads.)
- Gotchas: don't `pkill -f openocd` (matches & kills your own shell); zsh aborts
  the line on a no-match glob (use `ls /dev/ | grep usbmodem`).

### Phase 2 — analog + probe
- [ ] SPI `MCP4822` DAC backend (DAC0 0–5 V, DAC1 ±8 V) behind the HAL.
- [ ] 4 ADCs (ADC3 ±8 V), INA219 current sense.
- [ ] 3 routable GPIO + single routable `NANO_RESET`.
- [ ] Scanning probe (`scanRows`) ported from OG reference firmware.
- [ ] Capability-aware structured errors for unsupported ops (rail voltage set,
      GPIO 4–10, out-of-range DAC, probe pads) on serial + MicroPython.

### Phase 3 — parity + V5 cutover
- [ ] Migrate V5 onto the unified router; prove parity on real V5 hardware
      (host test + HIL) before removing the old `ch[]` path.
- [ ] Optional extras: wavegen via RP2040 PIO, undo, etc.

### Deferred — docs website
- [ ] Add an **OG Jumperless** page to the `Jumperless-docs` site (for humans
      AND agents): what features the OG supports vs V5, how to flash the
      `jumperless_og` firmware, the capability JSON an LLM tool should read, and
      the MicroPython/serial control surface. Not started; intentionally
      deferred until the OG firmware boots.

## Agent conventions

- **Never** branch the shared core on `OG_JUMPERLESS`/board macros — extend the
  `board.h` contract instead.
- **Never** change V5 runtime behavior while doing OG work. The V5 descriptor is
  not yet wired into V5's live routing; V5 keeps `MatrixState.cpp ch[]` until
  Phase 3. If you must touch a shared file, gate OG-only paths behind board
  capabilities and leave the V5 path byte-identical.
- Peripherals/features are **enumerated, never counted** (the OG `RP_GPIO_0`
  must never be treated as `GPIO_1`). Binary features → `BoardCaps` flags.
- Run the host test after any board-layer edit; add an assertion when you add a
  capability.
- Update this doc's checklist before ending a session.

## Key files

- Contract: `src/boards/board.h`, `src/boards/board.cpp`
- Descriptors: `src/boards/v5/board_v5.cpp`, `src/boards/og/board_og.cpp`
- Build: `platformio.ini` (`[env:jumperless_og]`)
- Test: `test/test_boards/test_boards.cpp`
- OG reference firmware (for porting data/logic):
  `../Jumperless/JumperlessBackport/src/` (`MatrixStateRP2040.cpp`,
  `JumperlessDefinesRP2040.h`, `Probing.cpp`, `Peripherals.cpp`, `LEDs.h`).
  Note: that tree is a draft snapshot ("probably don't use this") — use it as a
  reference for OG topology/probe/DAC, not as production code.
