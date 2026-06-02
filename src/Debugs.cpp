

#include "Debugs.h"
#include "PersistentStuff.h"

#include "AsyncPassthrough.h"
#include "configManager.h"
#include "config.h"
#include "ArduinoStuff.h"
#include "JulseView.h"
#include "Peripherals.h"
#include "Probing.h"
#include "NetManager.h"
#include "FileParsing.h"
#include "LEDs.h"
#include "Graphics.h"
#include "FakeGpio.h"
#include "MpRemoteService.h"
#include <Wire.h>
#include "oled.h"
#include "CH446Q.h"
#include "NetsToChipConnections.h"
#include "States.h"          // globalState — the single largest static buffer (memory map)
#include "GraphicOverlays.h" // graphicOverlayState — large overlay state buffer (memory map)
#include "SharedBuffer.h"    // borrowed as scratch for the memory-map renderer
#include "FatFS.h"      // FatFS.info() for the flash partition usage map
#include <malloc.h>     // mallinfo() for the SRAM heap usage map
#include <math.h>       // lroundf() for the color ramp

#include "SingleCharCommands.h"

// Cache + Undo + PSRAM debug flags (toggled via the new entries in the menu).
#include "PsramArena.h"   // psram_debug
#include "FileCache.h"    // fc_debug, fc_atomic_debug
#include "Undo.h"         // undo_debug

extern bool printPowerSupplySense;

// SPIFTL per-persist timing logs (defined in lib/FatFS/src/FatFS.cpp).
extern "C" volatile int spiftl_timing_debug;







bool debugFlagsMenu() {
// debugFlagInit();

debugFlags:

  int lastSerial1Passthrough = jumperlessConfig.serial_1.print_passthrough;
  int lastSerial2Passthrough = jumperlessConfig.serial_2.print_passthrough;
  printSerial1Passthrough = 0;
  printSerial2Passthrough = 0;

  // Interactive main debug menu: toggle by number, Enter to apply, 'l' to open LA submenu.
  // Seed from jumperlessConfig (the single source of truth) for the config-backed
  // flags so the displayed state can never drift from what's persisted.
  bool temp_debugFP = jumperlessConfig.debug.file_parsing;
  bool temp_debugNM = jumperlessConfig.debug.net_manager;
  bool temp_debugNTCC = jumperlessConfig.debug.nets_to_chips;
  bool temp_debugNTCC2 = jumperlessConfig.debug.nets_to_chips_alt;
  bool temp_debugLEDs = jumperlessConfig.debug.leds;
  bool temp_debugLA = jumperlessConfig.debug.logic_analyzer;
  bool temp_debugWaitLoopTiming = debugWaitLoopTiming;
  bool temp_debugUSB = debugUSB;
  int temp_showProbeCurrent = jumperlessConfig.debug.show_probe_current;
  int temp_passthrough = jumperlessConfig.serial_1.print_passthrough;
  bool temp_asyncPassthrough = jumperlessConfig.serial_1.async_passthrough;
  bool temp_debugFakeGpio = getDebugFakeGpio();
  int temp_printReceivedPython = mpRemoteService.getPrintReceivedPython();
  bool temp_printPowerSupplySense = printPowerSupplySense;
  // Cache / Undo / PSRAM trace flags (runtime-only, no EEPROM persistence)
  bool temp_psramDebug = (psram_debug != 0);
  bool temp_fcDebug = (fc_debug != 0);
  bool temp_fcAtomicDebug = (fc_atomic_debug != 0);
  bool temp_undoDebug = (undo_debug != 0);
  bool temp_spiftlTiming = (spiftl_timing_debug != 0);
  // Probe button reader: persistent hardware path selector + ephemeral
  // verbose trace. Both toggled from this menu.
  bool temp_usePIOProbeButton = jumperlessConfig.hardware.use_pio_probe_button;
  bool temp_probeButtonTrace  = (probe_button_trace != 0);
  // Track originals for diffing on commit (config-backed flags from config).
  bool orig_debugFP = jumperlessConfig.debug.file_parsing;
  bool orig_debugNM = jumperlessConfig.debug.net_manager;
  bool orig_debugNTCC = jumperlessConfig.debug.nets_to_chips;
  bool orig_debugNTCC2 = jumperlessConfig.debug.nets_to_chips_alt;
  bool orig_debugLEDs = jumperlessConfig.debug.leds;
  bool orig_debugLA = jumperlessConfig.debug.logic_analyzer;
  bool orig_debugWaitLoopTiming = debugWaitLoopTiming;
  bool orig_debugUSB = debugUSB;
  int orig_showProbeCurrent = jumperlessConfig.debug.show_probe_current;
  int orig_passthrough = jumperlessConfig.serial_1.print_passthrough;
  bool orig_asyncPassthrough = jumperlessConfig.serial_1.async_passthrough;
  bool orig_debugFakeGpio = getDebugFakeGpio();
  int orig_printReceivedPython = mpRemoteService.getPrintReceivedPython();
  bool orig_printPowerSupplySense = printPowerSupplySense;
  bool orig_psramDebug = temp_psramDebug;
  bool orig_fcDebug = temp_fcDebug;
  bool orig_fcAtomicDebug = temp_fcAtomicDebug;
  bool orig_undoDebug = temp_undoDebug;
  bool orig_spiftlTiming = temp_spiftlTiming;
  bool orig_usePIOProbeButton = temp_usePIOProbeButton;
  bool orig_probeButtonTrace  = temp_probeButtonTrace;
  
  int lines = 0;
  int last_bulk_cmd = -1; // 0 for all off, 9 for all on; reset to -1 on individual changes

  auto print_main_debug_menu = [&](int &lines_printed) {
    lines_printed = 4;
    cycleTerminalColor(true, 2.5 );
    Serial.print("\n\n\rx.   all off"); lines_printed++;
    cycleTerminalColor();
    Serial.print("\n\rz.   all on"); lines_printed++;
    cycleTerminalColor();
    Serial.print("\n\n\r(press enter to exit)\n\r"); lines_printed+=2; cycleTerminalColor();
    Serial.print("\n\rf. file parsing               =    "); Serial.print(temp_debugFP); lines_printed++; cycleTerminalColor(); 
    Serial.print("\n\rn. net manager                =    "); Serial.print(temp_debugNM); lines_printed++; cycleTerminalColor();
    Serial.print("\n\rc. chip connections           =    "); Serial.print(temp_debugNTCC); lines_printed++; cycleTerminalColor();
    Serial.print("\n\rh. chip conns alt paths       =    "); Serial.print(temp_debugNTCC2); lines_printed++; cycleTerminalColor();
    Serial.print("\n\re. LEDs                       =    "); Serial.print(temp_debugLEDs); lines_printed++; cycleTerminalColor();
    Serial.print("\n\rs. show probe current         =    "); Serial.print(temp_showProbeCurrent); lines_printed++; cycleTerminalColor();
  //  Serial.print("\n\rs. show probe current         =    "); Serial.print(temp_showProbeCurrent); lines_printed++; cycleTerminalColor();
    Serial.print("\n\ru. print serial 1 passthrough =    ");
    if (temp_passthrough == 1) {
      Serial.print("on");
    } else if (temp_passthrough == 2) {
      Serial.print("flashing only");
    } else {
      Serial.print("off");
    }
    lines_printed++; cycleTerminalColor();

    Serial.print("\n\rp. async passthrough          =    "); Serial.print(temp_asyncPassthrough ? 1 : 0); lines_printed++; cycleTerminalColor();

    Serial.print("\n\ra. Arduino debug level        =    "); Serial.print(jumperlessConfig.debug.arduino); lines_printed++; cycleTerminalColor();

    Serial.print("\n\rl. logic analyzer debug       =    "); Serial.print(temp_debugLA); lines_printed++; cycleTerminalColor();
 
    Serial.print("\n\rj. logic analyzer debug menu  >    "); lines_printed++; cycleTerminalColor();

    Serial.print("\n\rw. wait loop timing debug     =    "); Serial.print(temp_debugWaitLoopTiming); lines_printed++; cycleTerminalColor();

    Serial.print("\n\rb. USB mass storage debug     =    "); Serial.print(temp_debugUSB); lines_printed++; cycleTerminalColor();

    Serial.print("\n\rg. fake GPIO debug            =    "); Serial.print(temp_debugFakeGpio); lines_printed++; cycleTerminalColor();

    Serial.print("\n\rm. print received Python      =    "); Serial.print(temp_printReceivedPython); lines_printed++; cycleTerminalColor();

    Serial.print("\n\rv. print power supply sense   =    "); Serial.print(temp_printPowerSupplySense ? 1 : 0); lines_printed++; cycleTerminalColor();

    Serial.print("\n\rr. PSRAM arena trace (master) =    "); Serial.print(temp_psramDebug ? 1 : 0); lines_printed++; cycleTerminalColor();

    Serial.print("\n\rk. file cache trace           =    "); Serial.print(temp_fcDebug ? 1 : 0); lines_printed++; cycleTerminalColor();

    Serial.print("\n\ri. atomic write commit trace  =    "); Serial.print(temp_fcAtomicDebug ? 1 : 0); lines_printed++; cycleTerminalColor();

    Serial.print("\n\rd. undo system trace          =    "); Serial.print(temp_undoDebug ? 1 : 0); lines_printed++; cycleTerminalColor();

    Serial.print("\n\ro. wipe undo history          >    (one-shot)"); lines_printed++; cycleTerminalColor();

    Serial.print("\n\rq. SPIFTL persist timing      =    "); Serial.print(temp_spiftlTiming ? 1 : 0); lines_printed++; cycleTerminalColor();

    // Probe button hardware path. Show counters too so the user can
    // verify whichever path they picked is the one actually firing.
    Serial.print("\n\rt. probe button PIO path      =    ");
    Serial.print(temp_usePIOProbeButton ? "PIO" : "CPU");
    Serial.print("   [PIO ");  Serial.print((unsigned long)probeButtonPIOReadCount);
    Serial.print(" / CPU ");   Serial.print((unsigned long)probeButtonCPUReadCount);
    if (probeButtonPIOTimeoutCount) {
      Serial.print(" / timeout "); Serial.print((unsigned long)probeButtonPIOTimeoutCount);
    }
    Serial.print(" / last ");
    Serial.print((unsigned long)(temp_usePIOProbeButton ? probeButtonPIOLastUs : probeButtonCPULastUs));
    Serial.print("us]");
    lines_printed++; cycleTerminalColor();

    Serial.print("\n\ry. probe button trace         =    "); Serial.print(temp_probeButtonTrace ? 1 : 0); lines_printed++; cycleTerminalColor();

    Serial.print("\n\r\n\r\n\r"); lines_printed += 2;
    Serial.flush();
  };

  print_main_debug_menu(lines);

  while (true) {
    while (Serial.available() == 0) { ; }
    int sel = Serial.read();
    // Enter confirms and applies changes
    if (sel == '\r' || sel == '\n') {
      // Apply bulk commands if selected and no further individual changes
      if (last_bulk_cmd == 0 || last_bulk_cmd == 9) {
        // Re-render once before leaving to clear the menu area
        Serial.printf("\033[%dA", lines);
        for (int i = 0; i < lines; i++) { Serial.print("\033[2K\r\n\r"); }
        Serial.printf("\033[%dA", lines);
        Serial.flush();
        debugFlagSet(last_bulk_cmd);
        // debugFlagSet(0/9) only manages the legacy EEPROM-backed flags.
        // Apply the runtime-only cache/undo/PSRAM trace flags here so
        // "all off" / "all on" actually flushes them too.
        psram_debug = temp_psramDebug ? 1 : 0;
        fc_debug = temp_fcDebug ? 1 : 0;
        fc_atomic_debug = temp_fcAtomicDebug ? 1 : 0;
        undo_debug = temp_undoDebug ? 1 : 0;
        spiftl_timing_debug = temp_spiftlTiming ? 1 : 0;
        // Probe button trace tracks the bulk on/off too; the PIO path
        // selector deliberately doesn't (see note in the bulk-off branch).
        probe_button_trace = temp_probeButtonTrace ? 1 : 0;
        if (temp_usePIOProbeButton != orig_usePIOProbeButton) {
          jumperlessConfig.hardware.use_pio_probe_button = temp_usePIOProbeButton;
        }
      } else {
        // Commit individual diffs using debugFlagSet only for changed items
        if (temp_debugFP != orig_debugFP) debugFlagSet(1);
        if (temp_debugNM != orig_debugNM) debugFlagSet(2);
        if (temp_debugNTCC != orig_debugNTCC) debugFlagSet(3);
        if (temp_debugNTCC2 != orig_debugNTCC2) debugFlagSet(4);
        if (temp_debugLEDs != orig_debugLEDs) debugFlagSet(5);
        if (temp_debugLA != orig_debugLA) debugFlagSet(6);
        if (temp_showProbeCurrent != orig_showProbeCurrent) debugFlagSet(7);
        if (temp_debugWaitLoopTiming != orig_debugWaitLoopTiming) debugFlagSet(14);
        if (temp_debugUSB != orig_debugUSB) debugFlagSet(15);
        if (temp_passthrough != orig_passthrough) {
          int cur = jumperlessConfig.serial_1.print_passthrough;
          int safety = 0;
          while (cur != temp_passthrough && safety < 4) {
            debugFlagSet(8);
            cur = jumperlessConfig.serial_1.print_passthrough;
            safety++;
          }
        }
        if (temp_asyncPassthrough != orig_asyncPassthrough) {
          jumperlessConfig.serial_1.async_passthrough = temp_asyncPassthrough;
          if (temp_asyncPassthrough) {
            AsyncPassthrough::begin();
          }
        }
        if (temp_debugFakeGpio != orig_debugFakeGpio) {
          setDebugFakeGpio(temp_debugFakeGpio);
        }
        if (temp_printReceivedPython != orig_printReceivedPython) {
          mpRemoteService.setPrintReceivedPython(temp_printReceivedPython);
        }
        if (temp_printPowerSupplySense != orig_printPowerSupplySense) {
          printPowerSupplySense = temp_printPowerSupplySense;
        }
        // Cache / Undo / PSRAM trace flags (runtime-only, no persistence).
        if (temp_psramDebug != orig_psramDebug) psram_debug = temp_psramDebug ? 1 : 0;
        if (temp_fcDebug != orig_fcDebug) fc_debug = temp_fcDebug ? 1 : 0;
        if (temp_fcAtomicDebug != orig_fcAtomicDebug) fc_atomic_debug = temp_fcAtomicDebug ? 1 : 0;
        if (temp_undoDebug != orig_undoDebug) undo_debug = temp_undoDebug ? 1 : 0;
        if (temp_spiftlTiming != orig_spiftlTiming) spiftl_timing_debug = temp_spiftlTiming ? 1 : 0;
        // Probe button path (persisted) + trace (runtime-only).
        if (temp_usePIOProbeButton != orig_usePIOProbeButton) {
          jumperlessConfig.hardware.use_pio_probe_button = temp_usePIOProbeButton;
        }
        if (temp_probeButtonTrace != orig_probeButtonTrace) {
          probe_button_trace = temp_probeButtonTrace ? 1 : 0;
        }
        // Persist all config-backed debug flags in ONE write. debugFlagSet()
        // above only updated jumperlessConfig + the runtime globals (no EEPROM,
        // no per-flag save); this is the single source-of-truth commit so the
        // menu and the persisted values can't drift out of sync.
        saveConfig();
      }
      printSerial1Passthrough = lastSerial1Passthrough;
      printSerial2Passthrough = lastSerial2Passthrough;
      break; // leave menu
    }

    // Open JulseView LA submenu
    if (sel == 'j' || sel == 'J') {
      const char* categories[10] = {
        "commands", "buffers", "digital", "analog", "dma",
        "usb", "timing", "data", "errors", "state"
      };
      uint32_t local_mask = julseview_debug_mask;
      auto print_mask_menu = [&](uint32_t mask, int &lines_printed) {
        lines_printed = 1;
        cycleTerminalColor(true, 2.5 );
        Serial.print("\n\rJulseView Debug Categories (toggle by number, Enter to confirm)\n\r"); lines_printed+=2;
        Serial.printf("mask = 0x%08lX\n\r", (unsigned long)mask); lines_printed+=2; cycleTerminalColor();
        for (int i = 0; i < 10; i++) {
          int enabled = (mask & (1u << i)) ? 1 : 0;
          Serial.printf("%d. %-10s = %s\n\r", i, categories[i], enabled ? "on" : "off");
          lines_printed++; cycleTerminalColor();
        }
        Serial.print("\n\r"); lines_printed++; cycleTerminalColor();
        Serial.flush();
      };
      int llines = 0;
      print_mask_menu(local_mask, llines);
      while (true) {
        while (Serial.available() == 0) { ; }
        int ch = Serial.read();
        if (ch == '\r' || ch == '\n') {
          while (Serial.available() > 0) {
            int c2 = Serial.peek();
            if (c2 == '\r' || c2 == '\n') { Serial.read(); } else { break; }
          }
          break;
        }
        if (ch >= '0' && ch <= '9') {
          int idx = ch - '0';
          local_mask ^= (1u << idx);
          julseview_set_debug_mask(local_mask);
          Serial.printf("\033[%dA", llines);
          for (int i = 0; i < llines; i++) { Serial.print("\033[2K\r\n\r"); }
          Serial.printf("\033[%dA", llines);
          Serial.flush();
          print_mask_menu(local_mask, llines);
        }
      }
      // After returning from submenu, re-render the main menu
      Serial.printf("\033[%dA", lines);
      for (int i = 0; i < lines; i++) { Serial.print("\033[2K\r\n\r"); }
      Serial.printf("\033[%dA", lines);
      Serial.flush();
      print_main_debug_menu(lines);
      continue;
    }

    // One-shot action (not a toggle): wipe the persisted + in-RAM undo/redo
    // history. Clear the current menu, run the wipe, show a brief confirmation,
    // then redraw the menu beneath it.
    if (sel == 'o' || sel == 'O') {
      Serial.printf("\033[%dA", lines);
      for (int i = 0; i < lines; i++) { Serial.print("\033[2K\r\n\r"); }
      Serial.printf("\033[%dA", lines);
      Serial.flush();
      undoWipeHistory();
      Serial.print("\n\r\033[1;33mUndo history wiped.\033[0m\n\r");
      Serial.flush();
      delay(700);
      print_main_debug_menu(lines);
      continue;
    }

    // Toggle items and redraw, but do not persist yet
    if (sel == 'x' || sel == 'X') {
      temp_debugFP = false;
      temp_debugNM = false;
      temp_debugNTCC = false;
      temp_debugNTCC2 = false;
      temp_debugLEDs = false;
      temp_debugLA = false;
      temp_debugWaitLoopTiming = false;
      temp_debugUSB = false;
      temp_debugFakeGpio = false;
      temp_showProbeCurrent = 0;
      temp_printReceivedPython = 0;
      temp_printPowerSupplySense = false;
      temp_psramDebug = false;
      temp_fcDebug = false;
      temp_fcAtomicDebug = false;
      temp_undoDebug = false;
      temp_spiftlTiming = false;
      temp_probeButtonTrace = false;
      // NB: temp_usePIOProbeButton is NOT touched by "all off" - that's
      // a hardware-config selector (PIO vs CPU button reader path),
      // not a debug-print toggle, so we don't want the bulk button to
      // silently drop people back to the slower path.
      last_bulk_cmd = 0;
    } else if (sel == 'z' || sel == 'Z') {
      temp_debugFP = true;
      temp_debugNM = true;
      temp_debugNTCC = true;
      temp_debugNTCC2 = true;
      temp_debugLEDs = true;
    //   temp_debugLA = true;
    //   temp_debugWaitLoopTiming = true;
    //   temp_debugUSB = true;
    //   temp_debugFakeGpio = true;
    //   temp_showProbeCurrent = 1;
    //   temp_printReceivedPython = 1;
    //   temp_printPowerSupplySense = true;
      last_bulk_cmd = 9;
    } else if (sel == 'u' || sel == 'U') {
      // Cycle passthrough: 0 -> 2 -> 1 -> 0
      temp_passthrough = (temp_passthrough == 0) ? 2 : (temp_passthrough == 2) ? 1 : 0;
      last_bulk_cmd = -1;
    } else if (sel == 'f' || sel == 'F') { temp_debugFP = !temp_debugFP; last_bulk_cmd = -1; }
    else if (sel == 'n' || sel == 'N') { temp_debugNM = !temp_debugNM; last_bulk_cmd = -1; }
    else if (sel == 'c' || sel == 'C') { temp_debugNTCC = !temp_debugNTCC; last_bulk_cmd = -1; }
    else if (sel == 'h' || sel == 'H') { temp_debugNTCC2 = !temp_debugNTCC2; last_bulk_cmd = -1; }
    else if (sel == 'e' || sel == 'E') { temp_debugLEDs = !temp_debugLEDs; last_bulk_cmd = -1; }
    else if (sel == 'l' || sel == 'L') { temp_debugLA = !temp_debugLA; last_bulk_cmd = -1; }
    else if (sel == 'w' || sel == 'W') { temp_debugWaitLoopTiming = !temp_debugWaitLoopTiming; last_bulk_cmd = -1; }
    else if (sel == 'b' || sel == 'B') { temp_debugUSB = !temp_debugUSB; last_bulk_cmd = -1; }
    else if (sel == 'g' || sel == 'G') { temp_debugFakeGpio = !temp_debugFakeGpio; last_bulk_cmd = -1; }
    else if (sel == 'm' || sel == 'M') { temp_printReceivedPython = temp_printReceivedPython ? 0 : 1; last_bulk_cmd = -1; }
    else if (sel == 'p' || sel == 'P') { temp_asyncPassthrough = !temp_asyncPassthrough; last_bulk_cmd = -1; }
    else if (sel == 's' || sel == 'S') { temp_showProbeCurrent = temp_showProbeCurrent ? 0 : 1; last_bulk_cmd = -1; }
    else if (sel == 'v' || sel == 'V') { temp_printPowerSupplySense = !temp_printPowerSupplySense; last_bulk_cmd = -1; }
    else if (sel == 'r' || sel == 'R') { temp_psramDebug = !temp_psramDebug; last_bulk_cmd = -1; }
    else if (sel == 'k' || sel == 'K') { temp_fcDebug = !temp_fcDebug; last_bulk_cmd = -1; }
    else if (sel == 'i' || sel == 'I') { temp_fcAtomicDebug = !temp_fcAtomicDebug; last_bulk_cmd = -1; }
    else if (sel == 'd' || sel == 'D') { temp_undoDebug = !temp_undoDebug; last_bulk_cmd = -1; }
    else if (sel == 'q' || sel == 'Q') { temp_spiftlTiming = !temp_spiftlTiming; last_bulk_cmd = -1; }
    else if (sel == 't' || sel == 'T') { temp_usePIOProbeButton = !temp_usePIOProbeButton; last_bulk_cmd = -1; }
    else if (sel == 'y' || sel == 'Y') { temp_probeButtonTrace = !temp_probeButtonTrace; last_bulk_cmd = -1; }
    else {
      // not a recognized toggle key; fall through to next handlers
    }

    // If a recognized toggle occurred above, redraw and continue loop
    if (sel == 'x' || sel == 'X' || sel == 'z' || sel == 'Z' || sel == 'u' || sel == 'U' ||
        sel == 'f' || sel == 'F' || sel == 'n' || sel == 'N' || sel == 'c' || sel == 'C' ||
        sel == 'h' || sel == 'H' || sel == 'e' || sel == 'E' || sel == 'l' || sel == 'L' || 
        sel == 'w' || sel == 'W' || sel == 'b' || sel == 'B' || sel == 'g' || sel == 'G' ||
        sel == 'm' || sel == 'M' || sel == 'p' || sel == 'P' || sel == 's' || sel == 'S' ||
        sel == 'v' || sel == 'V' || sel == 'r' || sel == 'R' || sel == 'k' || sel == 'K' ||
        sel == 'i' || sel == 'I' || sel == 'd' || sel == 'D' || sel == 'q' || sel == 'Q' ||
        sel == 't' || sel == 'T' || sel == 'y' || sel == 'Y') {
      Serial.printf("\033[%dA", lines);
      for (int i = 0; i < lines; i++) { Serial.print("\033[2K\r\n\r"); }
      Serial.printf("\033[%dA", lines);
      Serial.flush();
      print_main_debug_menu(lines);
      continue;
    }

    // Adjust Arduino debug level: 'a' increments 0->1->2->0
    if (sel == 'a' || sel == 'A') {
      int level = jumperlessConfig.debug.arduino;
      level = (level >= 2) ? 0 : level + 1;
      jumperlessConfig.debug.arduino = level;
      debugArduino = level;
      // Clear and redraw the menu
      Serial.printf("\033[%dA", lines);
      for (int i = 0; i < lines; i++) { Serial.print("\033[2K\r\n\r"); }
      Serial.printf("\033[%dA", lines);
      Serial.flush();
      print_main_debug_menu(lines);
      continue;
    }

    // Any other key exits without saving
    printSerial1Passthrough = lastSerial1Passthrough;
    printSerial2Passthrough = lastSerial2Passthrough;
    break;
  }

  return true;
}


// ═══════════════════════════════════════════════════════════════════════════════
// Interactive Status & Diagnostics Menu with Arrow Key Navigation
// ═══════════════════════════════════════════════════════════════════════════════

// Menu item structure
struct StatusMenuItem {
    const char* label;
    const char* description;
    void (*action)();
};

// Forward declarations for menu actions
void action_resourceStatus();
void action_psramTest();
void action_gpioState();
void action_netlist();
void action_bridgeArray();
void action_crossbar();
void action_pioStatus();
void action_memoryUsage();
void action_memoryMap();
void action_i2cScan();
void action_speedTest();
void action_colorSpectrum();

// Menu items array
const StatusMenuItem statusMenuItems[] = {
    { "Resource Status",    "Show memory, PIO, GPIO overview",        action_resourceStatus },
    { "PSRAM Test",         "Run PSRAM integrity & speed tests",      action_psramTest },
    { "GPIO State",         "Show GPIO pin nets and states",          action_gpioState },
    { "Show Netlist",       "Display current net connections",        action_netlist },
    { "Bridge Array",       "Show paths and chip status",             action_bridgeArray },
    { "Crossbar View",      "Compact crossbar chip visualization",    action_crossbar },
    { "PIO Status",         "Detailed PIO state machine status",      action_pioStatus },
    { "Memory Usage",       "Detailed heap and stack analysis",       action_memoryUsage },
    { "Memory Map",         "Granular RAM/PSRAM/flash block map",     action_memoryMap },
    { "I2C Scan",           "Scan I2C bus for devices",               action_i2cScan },
    { "Speed Test",         "Raw crossbar switch speed test",         action_speedTest },
    { "Color Spectrum",     "Display terminal color palette",         action_colorSpectrum },
};
const int STATUS_MENU_COUNT = sizeof(statusMenuItems) / sizeof(statusMenuItems[0]);

// Draw the menu with current selection highlighted
void drawStatusMenu(int selected, int topVisible, int visibleCount) {
    // Clear and redraw
    Serial.print("\033[2J\033[H");  // Clear screen, cursor home
    Serial.flush();
    
    // Rainbow header using color cycling
    cycleTerminalColor(true, 3.0, false, &Serial, 0, 1);  // Reset to start of rainbow
    Serial.print("\n\r");
    cycleTerminalColor(false, 3.0, false, &Serial, 0, 1);
    Serial.print("╭──────────────────────────────────────────────────────────────────╮\n\r");
    cycleTerminalColor(false, 3.0, false, &Serial, 0, 1);
    Serial.print("│              ");
    Serial.print("\033[1m");  // Bold for title
    cycleTerminalColor(false, 2.0, false, &Serial, 0, 1);
    Serial.print("S");
    cycleTerminalColor(false, 2.0, false, &Serial, 0, 1);
    Serial.print("T");
    cycleTerminalColor(false, 2.0, false, &Serial, 0, 1);
    Serial.print("A");
    cycleTerminalColor(false, 2.0, false, &Serial, 0, 1);
    Serial.print("T");
    cycleTerminalColor(false, 2.0, false, &Serial, 0, 1);
    Serial.print("U");
    cycleTerminalColor(false, 2.0, false, &Serial, 0, 1);
    Serial.print("S");
    cycleTerminalColor(false, 2.0, false, &Serial, 0, 1);
    Serial.print(" ");
    cycleTerminalColor(false, 2.0, false, &Serial, 0, 1);
    Serial.print("&");
    cycleTerminalColor(false, 2.0, false, &Serial, 0, 1);
    Serial.print(" ");
    cycleTerminalColor(false, 2.0, false, &Serial, 0, 1);
    Serial.print("D");
    cycleTerminalColor(false, 2.0, false, &Serial, 0, 1);
    Serial.print("I");
    cycleTerminalColor(false, 2.0, false, &Serial, 0, 1);
    Serial.print("A");
    cycleTerminalColor(false, 2.0, false, &Serial, 0, 1);
    Serial.print("G");
    cycleTerminalColor(false, 2.0, false, &Serial, 0, 1);
    Serial.print("N");
    cycleTerminalColor(false, 2.0, false, &Serial, 0, 1);
    Serial.print("O");
    cycleTerminalColor(false, 2.0, false, &Serial, 0, 1);
    Serial.print("S");
    cycleTerminalColor(false, 2.0, false, &Serial, 0, 1);
    Serial.print("T");
    cycleTerminalColor(false, 2.0, false, &Serial, 0, 1);
    Serial.print("I");
    cycleTerminalColor(false, 2.0, false, &Serial, 0, 1);
    Serial.print("C");
    cycleTerminalColor(false, 2.0, false, &Serial, 0, 1);
    Serial.print("S");
    cycleTerminalColor(false, 2.0, false, &Serial, 0, 1);
    Serial.print(" ");
    cycleTerminalColor(false, 2.0, false, &Serial, 0, 1);
    Serial.print("M");
    cycleTerminalColor(false, 2.0, false, &Serial, 0, 1);
    Serial.print("E");
    cycleTerminalColor(false, 2.0, false, &Serial, 0, 1);
    Serial.print("N");
    cycleTerminalColor(false, 2.0, false, &Serial, 0, 1);
    Serial.print("U");
    Serial.print("\033[0m");
    cycleTerminalColor(false, 3.0, false, &Serial, 0, 1);
    Serial.print("                           │\n\r");
    cycleTerminalColor(false, 3.0, false, &Serial, 0, 1);
    Serial.print("├──────────────────────────────────────────────────────────────────┤\n\r");
    cycleTerminalColor(false, 3.0, false, &Serial, 0, 1);
    Serial.print("│\033[0m    Use \033[1;32m↑/↓\033[0m or \033[1;32mj/k\033[0m to navigate, \033[1;32mEnter\033[0m to run, \033[1;32mq\033[0m to exit     ");
    cycleTerminalColor(false, 3.0, false, &Serial, 0, 1);
    Serial.print("      │\n\r");
    cycleTerminalColor(false, 3.0, false, &Serial, 0, 1);
    Serial.print("╰──────────────────────────────────────────────────────────────────╯\033[0m\n\r\n\r");
    
    // Menu items with rainbow colored names
    for (int i = 0; i < visibleCount && (topVisible + i) < STATUS_MENU_COUNT; i++) {
        int idx = topVisible + i;
        const StatusMenuItem& item = statusMenuItems[idx];
        
        if (idx == selected) {
            // Highlighted item - inverse with current rainbow color
            cycleTerminalColor(false, 8.0, false, &Serial, 0, 1);
            Serial.print("\033[1;7m");  // Bold, inverse
            Serial.printf("  ▶ %-22s \033[0m\033[1;7m│\033[0m\033[1;7m %-36s  \033[0m\n\r", item.label, item.description);
        } else {
            // Regular item with rainbow colored name
            Serial.print("    ");
            cycleTerminalColor(false, 8.0, false, &Serial, 0, 1);
            Serial.printf("%-22s", item.label);
            Serial.print("\033[0m \033[90m│\033[0m \033[90m");
            Serial.printf("%-36s\033[0m\n\r", item.description);
        }
    }
    
    Serial.println();
    Serial.flush();
}

// Main interactive status menu function
bool statusDiagnosticsMenu() {
    int selected = 0;
    int topVisible = 0;
    const int visibleCount = 14;  // Show all items (we have 11)
    bool exitMenu = false;
    
    // Clear screen first
    Serial.print("\033[2J\033[H");
    Serial.flush();

        // Enable raw input mode for the status menu (navigation keys).
        bool wasInteractive = ( termInInteractiveMode == 1 );
        setTerminalLineBuffering(true);
        delay(10);

    // The "press enter to start" prompt/wait only exists to break the app's cooked
    // line-input loop. If the app is already interactive, start the menu immediately.
    if (!wasInteractive) {
    // Prompt user to press Enter to start interactive mode
    cycleTerminalColor(true, 3.0, false, &Serial, 0, 1);
    Serial.println("\n\r");
    cycleTerminalColor(false, 3.0, false, &Serial, 0, 1);
    Serial.print("╭───────────────────────────────────────────────────╮\n\r");
    cycleTerminalColor(false, 3.0, false, &Serial, 0, 1);
    Serial.print("│  ");
    cycleTerminalColor(false, 3.0, false, &Serial, 0, 1);
    Serial.print("Press ");
    Serial.print("\033[1;32mEnter\033[0m");
    cycleTerminalColor(false, 3.0, false, &Serial, 0, 1);
    Serial.print(" to start status & diagnostics menu");
    cycleTerminalColor(false, 3.0, false, &Serial, 0, 1);
    Serial.print("   │\n\r");
    cycleTerminalColor(false, 3.0, false, &Serial, 0, 1);
    Serial.print("╰───────────────────────────────────────────────────╯\033[0m\n\r");
    Serial.flush();
    
    // Wait for Enter key
    while (true) {
        if (Serial.available() > 0) {
            int ch = Serial.read();
            if (ch == '\r' || ch == '\n' || ch == ' ') {
                // Consume any additional newline characters
                delay(10);
                while (Serial.available() > 0) {
                    int peek = Serial.peek();
                    if (peek == '\r' || peek == '\n') {
                        Serial.read();
                    } else {
                        break;
                    }
                }
                break;
            }
            // 'q' to cancel before even starting
            if (ch == 'q' || ch == 'Q') {
                Serial.print("\033[0m");  // Reset colors
                return true;
            }
        }
        delay(10);
    }
    }
    

    drawStatusMenu(selected, topVisible, visibleCount);
    
    while (!exitMenu) {
        if (Serial.available() > 0) {
            int ch = Serial.read();
            
            // Handle escape sequences (arrow keys)
            if (ch == 27) {  // ESC
                delay(5);
                if (Serial.available() > 0) {
                    int ch2 = Serial.read();
                    if (ch2 == '[') {
                        delay(5);
                        if (Serial.available() > 0) {
                            int ch3 = Serial.read();
                            switch (ch3) {
                                case 'A':  // Up arrow
                                    ch = 'k';
                                    break;
                                case 'B':  // Down arrow
                                    ch = 'j';
                                    break;
                                case 'C':  // Right arrow (can be used as enter)
                                    ch = '\r';
                                    break;
                                case 'D':  // Left arrow (can be used as back/quit)
                                    ch = 'q';
                                    break;
                            }
                        }
                    }
                } else {
                    // Just ESC pressed, exit
                    ch = 'q';
                }
            }
            
            // Process the key
            switch (ch) {
                case 'k':
                case 'K':
                case 'w':
                case 'W':
                    // Move up
                    if (selected > 0) {
                        selected--;
                        if (selected < topVisible) {
                            topVisible = selected;
                        }
                        drawStatusMenu(selected, topVisible, visibleCount);
                    }
                    break;
                    
                case 'j':
                case 'J':
                case 's':
                case 'S':
                    // Move down
                    if (selected < STATUS_MENU_COUNT - 1) {
                        selected++;
                        if (selected >= topVisible + visibleCount) {
                            topVisible = selected - visibleCount + 1;
                        }
                        drawStatusMenu(selected, topVisible, visibleCount);
                    }
                    break;
                    
                case '\r':
                case '\n':
                case ' ':
                    // Execute selected item
                    Serial.print("\033[2J\033[H");  // Clear screen
                    Serial.flush();
                    Serial.println("\n\r");
                    
                    if (statusMenuItems[selected].action != nullptr) {
                        statusMenuItems[selected].action();
                    }
                    
                    // Wait for keypress to return to menu
                    Serial.println("\n\r\033[1;33m─── Press any key to return to menu ───\033[0m");
                    Serial.flush();
                    while (Serial.available() == 0) { delay(10); }
                    Serial.read();  // Consume the key
                    
                    drawStatusMenu(selected, topVisible, visibleCount);
                    break;
                    
                case 'q':
                case 'Q':
                    exitMenu = true;
                    break;
                    
                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                case 'a': case 'A':  // 'a' = 10
                    // Quick access by number
                    {
                        int quickIdx;
                        if (ch == 'a' || ch == 'A') {
                            quickIdx = 10;
                        } else {
                            quickIdx = ch - '0';
                        }
                        
                        if (quickIdx < STATUS_MENU_COUNT) {
                            selected = quickIdx;
                            if (selected < topVisible) {
                                topVisible = selected;
                            } else if (selected >= topVisible + visibleCount) {
                                topVisible = selected - visibleCount + 1;
                            }
                            
                            // Execute immediately
                            Serial.print("\033[2J\033[H");
                            Serial.flush();
                            Serial.println("\n\r");
                            
                            if (statusMenuItems[selected].action != nullptr) {
                                statusMenuItems[selected].action();
                            }
                            
                            Serial.println("\n\r\033[1;38;5;213m─── Press any key to return to menu ───\033[0m");
                            Serial.flush();
                            while (Serial.available() == 0) { delay(10); }
                            Serial.read();
                            
                            drawStatusMenu(selected, topVisible, visibleCount);
                        }
                    }
                    break;
            }
        }
        delay(10);
    }
    
    // Clean up: resync the app to the user's line-buffering config.
    pushLineBufferingToApp();
    delay(10);
    Serial.print("\033[2J\033[H");  // Clear screen
    Serial.flush();
    
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Menu Action Implementations
// ═══════════════════════════════════════════════════════════════════════════════

// External function declarations - these are now included via headers
extern void sendXYraw(int, int, int, int);
extern volatile bool pauseCore2;

void action_resourceStatus() {
  cmd_resourceStatus( 'j', "" );
    // Serial.println("\n\r╭────────────────────────────────────────────────────────────────────────────╮");
    // Serial.println("│                         SYSTEM RESOURCE STATUS                             │");
    // Serial.println("╰────────────────────────────────────────────────────────────────────────────╯\n\r");
    
    // // Memory info
    // size_t sramTotal = rp2040.getTotalHeap();
    // size_t sramFree = rp2040.getFreeHeap();
    // size_t sramUsed = sramTotal - sramFree;
    // int sramPercent = (sramUsed * 100) / sramTotal;
    
    // Serial.println("┌─────────────────────────────────────┐");
    // Serial.println("│           SRAM MEMORY               │");
    // Serial.println("├─────────────────────────────────────┤");
    // Serial.printf("│ Total:  %6lu KB (%lu bytes)       │\n\r", sramTotal / 1024, sramTotal);
    // Serial.printf("│ Free:   %6lu KB                   │\n\r", sramFree / 1024);
    // Serial.printf("│ Used:   %6lu KB (%d%%)             │\n\r", sramUsed / 1024, sramPercent);
    // Serial.println("└─────────────────────────────────────┘");
    
    // // PSRAM quick check
    // size_t psramSize = rp2040.getPSRAMSize();
    // if (psramSize > 0) {
    //     Serial.println("\n\r┌─────────────────────────────────────┐");
    //     Serial.println("│           PSRAM MEMORY              │");
    //     Serial.println("├─────────────────────────────────────┤");
    //     Serial.printf("│ Chip Size: %lu MB                    │\n\r", psramSize / 1024 / 1024);
    //     Serial.printf("│ Heap Free: %lu KB                    │\n\r", rp2040.getFreePSRAMHeap() / 1024);
    //     Serial.println("└─────────────────────────────────────┘");
    // } else {
    //     Serial.println("\n\rPSRAM: Not detected");
    // }
    
    // Serial.flush();
}

void action_psramTest() {
  Serial.println( "\n=== PSRAM Test Suite ===" );
  Serial.flush();
  Serial.println( "Config psram_installed: " + String( jumperlessConfig.hardware.psram_installed ) );
  Serial.flush();
  
  // Show regular SRAM info first (this is always safe)
  Serial.println( "\n--- SRAM Info ---" );
  Serial.println( "SRAM Total Heap: " + String( rp2040.getTotalHeap() / 1024 ) + " KB" );
  Serial.println( "SRAM Free Heap: " + String( rp2040.getFreeHeap() / 1024 ) + " KB" );
  Serial.flush();
  
  // Try to get PSRAM size - this may crash if no PSRAM is present
  Serial.println( "\n--- PSRAM Detection ---" );
  Serial.println( "Checking PSRAM size..." );
  Serial.flush();
  
  size_t psramSize = rp2040.getPSRAMSize();
  Serial.println( "PSRAM Chip Size: " + String( psramSize / 1024 / 1024 ) + " MB (" + String( psramSize ) + " bytes)" );
  Serial.flush();
  
  if ( psramSize == 0 ) {
      Serial.println( "\nNo PSRAM detected!" );
      Serial.println( "If you have installed the PSRAM mod, check:" );
      Serial.println( "  - PSRAM chip is properly soldered" );
      Serial.println( "  - CS pin (GPIO 19) connection" );
      Serial.println( "  - Power and ground connections" );
      Serial.flush();
      return;
    
  }
  
  // PSRAM detected - get heap info
  Serial.println( "Getting PSRAM heap info..." );
  Serial.flush();
  
  size_t psramTotal = rp2040.getTotalPSRAMHeap();
  size_t psramUsed = rp2040.getUsedPSRAMHeap();
  size_t psramFree = rp2040.getFreePSRAMHeap();
  
  Serial.println( "\n--- PSRAM Info ---" );
  Serial.println( "PSRAM Heap Total: " + String( psramTotal / 1024 ) + " KB" );
  Serial.println( "PSRAM Heap Used: " + String( psramUsed / 1024 ) + " KB" );
  Serial.println( "PSRAM Heap Free: " + String( psramFree / 1024 ) + " KB" );
  Serial.flush();
  
  // Memory integrity test - start small
  Serial.println( "\n--- Memory Integrity Test ---" );
  Serial.flush();
  
  // Try a small allocation first
  Serial.println( "Testing small allocation (256 bytes)..." );
  Serial.flush();
  
  uint32_t* testSmall = (uint32_t*)pmalloc( 256 );
  if ( testSmall == nullptr ) {
      Serial.println( "ERROR: Small pmalloc() failed!" );
      Serial.flush();
      
  }
  
  // Quick write/read test
  testSmall[0] = 0xDEADBEEF;
  testSmall[1] = 0xCAFEBABE;
  Serial.flush();
  
  if ( testSmall[0] != 0xDEADBEEF || testSmall[1] != 0xCAFEBABE ) {
      Serial.println( "ERROR: Basic read/write test FAILED!" );
      Serial.println( "  Wrote: 0xDEADBEEF, Read: 0x" + String( testSmall[0], HEX ) );
      Serial.println( "  Wrote: 0xCAFEBABE, Read: 0x" + String( testSmall[1], HEX ) );
      free( testSmall );
      Serial.flush(); 
      
  }
  Serial.println( "Small allocation test: PASS" );
  free( testSmall );
  Serial.flush();
  
  // Now try larger test
  const size_t testSize = 64 * 1024; // 64KB test block
  Serial.println( "Allocating " + String( testSize / 1024 ) + " KB test block..." );
  Serial.flush();
  
  uint32_t* psramBlock = (uint32_t*)pmalloc( testSize );
  if ( psramBlock == nullptr ) {
      Serial.println( "ERROR: Failed to allocate PSRAM test block!" );
      Serial.flush();
      
  }
  Serial.println( "Allocation successful at address: 0x" + String( (uint32_t)psramBlock, HEX ) );
  Serial.flush();
  
  size_t numWords = testSize / sizeof(uint32_t);
  int errors = 0;
  
  // Test 1: Sequential pattern
  Serial.print( "Test 1: Sequential pattern... " );
  Serial.flush();
  for ( size_t i = 0; i < numWords; i++ ) {
      psramBlock[i] = i;
  }
  for ( size_t i = 0; i < numWords; i++ ) {
      if ( psramBlock[i] != i ) {
          errors++;
          if ( errors <= 5 ) {
              Serial.println( "Error at " + String(i) + ": expected " + String(i) + ", got " + String(psramBlock[i]) );
          }
      }
  }
  Serial.println( errors == 0 ? "PASS" : "FAIL (" + String(errors) + " errors)" );
  Serial.flush();
  
  // Test 2: Alternating bits pattern (0x55555555 / 0xAAAAAAAA)
  errors = 0;
  Serial.print( "Test 2: Alternating bits (0x55/0xAA)... " );
  Serial.flush();
  for ( size_t i = 0; i < numWords; i++ ) {
      psramBlock[i] = ( i & 1 ) ? 0xAAAAAAAA : 0x55555555;
  }
  for ( size_t i = 0; i < numWords; i++ ) {
      uint32_t expected = ( i & 1 ) ? 0xAAAAAAAA : 0x55555555;
      if ( psramBlock[i] != expected ) {
          errors++;
      }
  }
  Serial.println( errors == 0 ? "PASS" : "FAIL (" + String(errors) + " errors)" );
  Serial.flush();
  
  // Test 3: Walking ones
  errors = 0;
  Serial.print( "Test 3: Walking ones pattern... " );
  Serial.flush();
  for ( size_t i = 0; i < numWords; i++ ) {
      psramBlock[i] = 1 << ( i % 32 );
  }
  for ( size_t i = 0; i < numWords; i++ ) {
      uint32_t expected = 1 << ( i % 32 );
      if ( psramBlock[i] != expected ) {
          errors++;
      }
  }
  Serial.println( errors == 0 ? "PASS" : "FAIL (" + String(errors) + " errors)" );
  Serial.flush();
  
  // Test 4: All zeros and all ones
  errors = 0;
  Serial.print( "Test 4: All zeros/ones... " );
  Serial.flush();
  for ( size_t i = 0; i < numWords; i++ ) {
      psramBlock[i] = 0x00000000;
  }
  for ( size_t i = 0; i < numWords; i++ ) {
      if ( psramBlock[i] != 0x00000000 ) {
          errors++;
      }
  }
  for ( size_t i = 0; i < numWords; i++ ) {
      psramBlock[i] = 0xFFFFFFFF;
  }
  for ( size_t i = 0; i < numWords; i++ ) {
      if ( psramBlock[i] != 0xFFFFFFFF ) {
          errors++;
      }
  }
  Serial.println( errors == 0 ? "PASS" : "FAIL (" + String(errors) + " errors)" );
  Serial.flush();
  
  // Speed test
  Serial.println( "\n--- Speed Comparison Test ---" );
  Serial.flush();
  
  const size_t speedTestSize = 16 * 1024; // 32KB for speed test
  size_t speedWords = speedTestSize / sizeof(uint32_t);
  
  // Allocate SRAM block for comparison
  uint32_t* sramBlock = (uint32_t*)malloc( speedTestSize );
  if ( sramBlock == nullptr ) {
      Serial.println( "Warning: Could not allocate SRAM comparison block" );
      free( psramBlock );
      Serial.flush();
     
  }
  
  unsigned long startTime, endTime;
  
  // PSRAM sequential write speed
  startTime = micros();
  for ( size_t i = 0; i < speedWords; i++ ) {
      psramBlock[i] = i;
  }
  endTime = micros();
  unsigned long psramWriteTime = endTime - startTime;
  float psramWriteSpeed = ( speedTestSize / 1024.0 ) / ( psramWriteTime / 1000000.0 ); // KB/s
  
  // PSRAM sequential read speed
  volatile uint32_t dummy = 0;
  startTime = micros();
  for ( size_t i = 0; i < speedWords; i++ ) {
      dummy += psramBlock[i];
  }
  endTime = micros();
  unsigned long psramReadTime = endTime - startTime;
  float psramReadSpeed = ( speedTestSize / 1024.0 ) / ( psramReadTime / 1000000.0 ); // KB/s
  
  // SRAM sequential write speed
  startTime = micros();
  for ( size_t i = 0; i < speedWords; i++ ) {
      sramBlock[i] = i;
  }
  endTime = micros();
  unsigned long sramWriteTime = endTime - startTime;
  float sramWriteSpeed = ( speedTestSize / 1024.0 ) / ( sramWriteTime / 1000000.0 ); // KB/s
  
  // SRAM sequential read speed  
  startTime = micros();
  for ( size_t i = 0; i < speedWords; i++ ) {
      dummy += sramBlock[i];
  }
  endTime = micros();
  unsigned long sramReadTime = endTime - startTime;
  float sramReadSpeed = ( speedTestSize / 1024.0 ) / ( sramReadTime / 1000000.0 ); // KB/s
  
  Serial.println( "Test block size: " + String( speedTestSize / 1024 ) + " KB" );
  Serial.println( "" );
  Serial.println( "PSRAM Write: " + String( psramWriteTime ) + " us (" + String( psramWriteSpeed / 1024, 2 ) + " MB/s)" );
  Serial.println( "PSRAM Read:  " + String( psramReadTime ) + " us (" + String( psramReadSpeed / 1024, 2 ) + " MB/s)" );
  Serial.println( "SRAM Write:  " + String( sramWriteTime ) + " us (" + String( sramWriteSpeed / 1024, 2 ) + " MB/s)" );
  Serial.println( "SRAM Read:   " + String( sramReadTime ) + " us (" + String( sramReadSpeed / 1024, 2 ) + " MB/s)" );
  Serial.println( "" );
  Serial.println( "Speed ratio (SRAM/PSRAM):" );
  Serial.println( "  Write: " + String( sramWriteSpeed / psramWriteSpeed, 2 ) + "x" );
  Serial.println( "  Read:  " + String( sramReadSpeed / psramReadSpeed, 2 ) + "x" );
  Serial.flush();
  
  // Cleanup
  free( psramBlock );
  free( sramBlock );
  
  Serial.println( "\n=== PSRAM Test Complete ===" );
  Serial.flush();
  
}

void action_gpioState() {
    printGPIOState();
}

void action_netlist() {
    couldntFindPath(1);
    Serial.println("\n\rnetlist");
    listNets(anythingInteractiveConnected(-1));
    Serial.flush();
}

void action_bridgeArray() {
    couldntFindPath(1);
    Serial.println("\n\rBridge Array");
    printBridgeArray();
    Serial.println("\n\n\rPaths");
    printPathsCompact(1);
    Serial.println("\n\rChip Status");
    printChipStatus();
    Serial.flush();
}

void action_crossbar() {
    printChipStateArrayColor();
}

void action_pioStatus() {
    printPIOStateMachines();
}

void action_memoryUsage() {
    Serial.println("\n\r╭────────────────────────────────────╮");
    Serial.println("│        DETAILED MEMORY USAGE       │");
    Serial.println("╰────────────────────────────────────╯\n\r");
    
    // SRAM
    Serial.println("=== SRAM Heap ===");
    Serial.println("Total Heap:      " + String(rp2040.getTotalHeap()) + " bytes");
    Serial.println("Free Heap:       " + String(rp2040.getFreeHeap()) + " bytes");
    Serial.println("Used Heap:       " + String(rp2040.getTotalHeap() - rp2040.getFreeHeap()) + " bytes");
    
    // PSRAM if available
    size_t psramSize = rp2040.getPSRAMSize();
    if (psramSize > 0) {
        Serial.println("\n\r=== PSRAM ===");
        Serial.println("Chip Size:       " + String(psramSize) + " bytes (" + String(psramSize / 1024 / 1024) + " MB)");
        Serial.println("Heap Total:      " + String(rp2040.getTotalPSRAMHeap()) + " bytes");
        Serial.println("Heap Used:       " + String(rp2040.getUsedPSRAMHeap()) + " bytes");
        Serial.println("Heap Free:       " + String(rp2040.getFreePSRAMHeap()) + " bytes");
    }
    
    Serial.flush();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Granular Memory Map Viewer
// ═══════════════════════════════════════════════════════════════════════════════
//
// Renders SRAM, PSRAM and flash as colored grids of box-drawing blocks. Each
// cell covers a fixed power-of-two number of bytes and is colored on a
// green→yellow→red "used" ramp for dynamically-managed regions, or with a fixed
// hue for fixed-purpose segments (firmware, static globals, stacks, EEPROM,
// MicroPython GC heap, reserved metadata, vacant space).
//
// The PSRAM app arena is walked block-by-block (true per-allocation detail);
// the SRAM heap is summarized via mallinfo(); the FatFS partition via
// FatFS.info(); fixed segment boundaries come from linker symbols.

// Linker symbols describing the on-chip layout (see the arduino-pico
// memmap_default.ld). Taking their *address* gives the boundary; the declared
// type is irrelevant since we never dereference them.
extern "C" {
    extern char __data_start__;        // start of .data (static globals) in SRAM
    extern char __end__;               // start of the SRAM heap (after .bss)
    extern char __StackLimit;          // top of the SRAM heap / 0x20080000
    extern char __flash_binary_start;  // 0x10000000
    extern char __flash_binary_end;    // end of the firmware image in flash
    extern uint8_t _FS_start;          // start of the FatFS partition
    extern uint8_t _FS_end;            // end of the FatFS partition (EEPROM follows)
}

// Two big static buffers we want to mark on the SRAM map but that lack a usable
// public declaration: both are defined at global scope in their .cpp but the
// header decl is either commented out (newBridges) or namespaced (uartReceived).
extern int     newBridges[MAX_NETS][MAX_DUPLICATE][2];  // NetsToChipConnections.cpp
extern uint8_t uartReceived[8192];                      // AsyncPassthrough.cpp (DMA RX ring)

namespace {

// Segment categories. MC_GRADIENT cells are colored by their per-cell used
// fraction; every other category is a solid fixed hue.
enum MemCat : uint8_t {
    MC_GRADIENT = 0,
    MC_CODE,
    MC_STATIC,
    MC_STACK,
    MC_EEPROM,
    MC_MPHEAP,
    MC_RESERVED,
    MC_VACANT,
    MC_NAMED,     // big named static buffer; color comes from the seg, not kCat
    MC_COUNT
};

struct MemSeg {
    uint64_t start;   // byte offset from the region base
    uint64_t size;    // length in bytes
    uint8_t  cat;     // MemCat
    float    frac;    // MC_GRADIENT: used fraction. MC_NAMED: xterm color (cast to int).
};

// xterm-256 color cube helper: r,g,b each 0..5.
inline uint8_t cube256(int r, int g, int b) {
    return (uint8_t)(16 + 36 * r + 6 * g + b);
}

// Fixed colors + legend names for the non-gradient categories.
struct CatStyle { uint8_t color; const char* name; };
const CatStyle kCat[MC_COUNT] = {
    { 0,              "used"     },  // MC_GRADIENT (color computed per cell)
    { cube256(1,2,5), "firmware" },  // blue
    { cube256(0,4,5), "static"   },  // cyan
    { cube256(5,0,4), "stack"    },  // magenta
    { cube256(5,4,0), "eeprom"   },  // gold
    { cube256(3,1,5), "mpython"  },  // violet
    { cube256(2,2,2), "reserved" },  // grey
    { 236,            "vacant"   },  // near-black grey
    { cube256(5,5,5), "marked"   },  // MC_NAMED fallback (per-seg color normally)
};

// Map a 0..1 used fraction onto green → yellow → red.
uint8_t usedRamp256(float f) {
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    int r, g;
    if (f <= 0.5f) { r = (int)lroundf(f * 2.0f * 5.0f); g = 5; }   // green→yellow
    else           { r = 5; g = (int)lroundf((1.0f - f) * 2.0f * 5.0f); } // yellow→red
    return cube256(r, g, 0);
}

uint64_t nextPow2_u64(uint64_t v) {
    uint64_t p = 1;
    while (p < v && p < (1ULL << 62)) p <<= 1;
    return p;
}

const char* fmtBytes(uint64_t b, char* buf, size_t n) {
    if (b >= 1024ULL * 1024 * 1024) snprintf(buf, n, "%.2f GB", b / (1024.0 * 1024 * 1024));
    else if (b >= 1024ULL * 1024)   snprintf(buf, n, "%.2f MB", b / (1024.0 * 1024));
    else if (b >= 1024ULL)          snprintf(buf, n, "%.1f KB", b / 1024.0);
    else                            snprintf(buf, n, "%lu B", (unsigned long)b);
    return buf;
}

// Context used while walking a block allocator (PSRAM arena or SRAM heap)
// into a segment list. Adjacent same-state blocks are merged so a heavily
// fragmented heap still fits the cap; the used↔free boundaries (the granular
// signal we care about) are preserved.
struct BlockRunCtx {
    MemSeg*  segs;
    int      n;
    int      cap;
    uintptr_t base;
    int      lastUsed;   // -1 = no run seg started yet
    uint64_t usedSum;
};

void blockRunCb(void* vctx, uintptr_t addr, size_t size, int used) {
    BlockRunCtx* c = (BlockRunCtx*)vctx;
    if (used) c->usedSum += size;
    if (c->n > 0 && c->lastUsed == used && c->segs[c->n - 1].cat == MC_GRADIENT) {
        c->segs[c->n - 1].size += size;
        return;
    }
    if (c->n < c->cap) {
        c->segs[c->n].start = (uint64_t)(addr - c->base);
        c->segs[c->n].size  = size;
        c->segs[c->n].cat   = MC_GRADIENT;
        c->segs[c->n].frac  = used ? 1.0f : 0.0f;
        c->n++;
        c->lastUsed = used;
    } else if (c->n > 0) {
        c->segs[c->n - 1].size += size;  // cap hit: absorb the tail
    }
}

typedef void (*block_visitor)(void* ctx, uintptr_t addr, size_t size, int used);

// ── SRAM heap walk (newlib full dlmalloc) ────────────────────────────────────
//
// arduino-pico links Newlib's full malloc (mallocr.c, a Doug Lea v2.6.x
// derivative). The heap is one contiguous sbrk arena starting at
// __malloc_sbrk_base; the "top"/wilderness free chunk is __malloc_av_[2]
// (= bin_at(0)->fd). Each chunk header is { prev_size, size }; chunksize masks
// off the low flag bits, and chunk p is in use iff the *next* chunk's size has
// PREV_INUSE(0x1) set. We mirror that model exactly, walking address-ordered
// blocks from the first chunk to top, then emit top itself as free.
extern "C" {
    extern char* __malloc_sbrk_base;   // base of the sbrk heap arena (char*)
    extern void* __malloc_av_[];       // dlmalloc bin array; [2] == top chunk
}

struct DlChunk { size_t prev_size; size_t size; DlChunk* fd; DlChunk* bk; };

// Returns the number of blocks visited (0 if the heap is uninitialized or the
// structures look inconsistent, so the caller can fall back to a coarse view).
size_t sram_heap_walk(block_visitor cb, void* ctx, uintptr_t* outTopEnd) {
    char* sb = __malloc_sbrk_base;
    if (sb == nullptr || sb == (char*)~(uintptr_t)0) return 0;

    DlChunk* top = (DlChunk*)__malloc_av_[2];
    uintptr_t first = ((uintptr_t)sb + 7) & ~(uintptr_t)7;
    uintptr_t topAddr = (uintptr_t)top;

    // Sanity: top must live in main SRAM, at or above the first chunk.
    if (topAddr < first || topAddr < 0x20000000u || topAddr >= 0x20080000u) return 0;

    size_t count = 0;
    DlChunk* p = (DlChunk*)first;
    int guard = 0;
    while ((uintptr_t)p < topAddr && guard++ < 8000) {
        size_t sz = p->size & ~(size_t)0x3;            // chunksize: strip flag bits
        if (sz < sizeof(DlChunk) || (sz & 0x7) != 0) return count;   // desync guard
        uintptr_t pa = (uintptr_t)p;
        if (pa + sz > topAddr) return count;           // would overrun top: bail
        DlChunk* nxt = (DlChunk*)(pa + sz);
        int used = (int)(nxt->size & 0x1);             // PREV_INUSE of next == inuse(p)
        if (cb) cb(ctx, pa, sz, used);
        count++;
        p = nxt;
    }
    // The top/wilderness chunk is always free.
    size_t topSz = top->size & ~(size_t)0x3;
    if (topSz >= sizeof(DlChunk)) {
        if (cb) cb(ctx, topAddr, topSz, 0);
        count++;
        if (outTopEnd) *outTopEnd = topAddr + topSz;
    } else if (outTopEnd) {
        *outTopEnd = topAddr;
    }
    return count;
}

// Max segments in the scratch list. SRAM / PSRAM / flash are rendered one at a
// time, so a single buffer is reused for each region (run-merging keeps the
// count well under the cap even on a heavily fragmented heap). Rather than park
// ~12 KB in .bss, the renderer borrows the existing 24 KB shared transfer buffer
// while the map is on screen (see action_memoryMap).
constexpr int kMaxSegs = 512;
static_assert(sizeof(MemSeg) * kMaxSegs <= SHARED_BUFFER_SIZE,
              "memory-map segment scratch must fit within the shared buffer");

// Draw one labeled, colored grid for a memory region.
void drawMemRegion(const char* title, const char* info,
                   uintptr_t base, uint64_t total,
                   const MemSeg* segs, int nSegs) {
    if (total == 0) return;

    const int cols = 64;
    const int targetRows = 24;

    uint64_t bpc = (total + (uint64_t)cols * targetRows - 1) / ((uint64_t)cols * targetRows);
    if (bpc < 1) bpc = 1;
    bpc = nextPow2_u64(bpc);
    uint64_t rowBytes = bpc * (uint64_t)cols;
    int rows = (int)((total + rowBytes - 1) / rowBytes);

    char cbuf[24], rbuf[24];
    Serial.println();
    Serial.printf("\033[1;97m  %s\033[0m  \033[90m[0x%08lX]\033[0m\n\r",
                  title, (unsigned long)base);
    if (info && *info) Serial.printf("  %s\n\r", info);
    Serial.printf("  \033[90mscale: each \033[0m\033[38;5;%dm█\033[0m\033[90m = %s    row = %s\033[0m\n\r",
                  usedRamp256(1.0f), fmtBytes(bpc, cbuf, sizeof(cbuf)),
                  fmtBytes(rowBytes, rbuf, sizeof(rbuf)));

    for (int r = 0; r < rows; r++) {
        uint64_t rowStart = (uint64_t)r * rowBytes;
        Serial.printf("  \033[90m0x%08lX\033[0m ", (unsigned long)(base + rowStart));
        int curColor = -1;
        for (int c = 0; c < cols; c++) {
            uint64_t cs = rowStart + (uint64_t)c * bpc;
            if (cs >= total) {
                if (curColor != -1) { Serial.print("\033[0m"); curColor = -1; }
                Serial.print(" ");
                continue;
            }
            uint64_t ce = cs + bpc;
            if (ce > total) ce = total;

            uint64_t gradOv = 0;
            double   gradUsed = 0.0;
            uint64_t bestSolidOv = 0;
            int      bestSolidCat = -1;
            uint64_t namedOv = 0;          // marked buffers override the cell entirely
            uint8_t  namedColor = 0;
            for (int s = 0; s < nSegs; s++) {
                uint64_t a = segs[s].start;
                uint64_t b = a + segs[s].size;
                uint64_t lo = cs > a ? cs : a;
                uint64_t hi = ce < b ? ce : b;
                if (lo >= hi) continue;
                uint64_t ov = hi - lo;
                if (segs[s].cat == MC_GRADIENT) {
                    gradOv += ov;
                    gradUsed += (double)segs[s].frac * (double)ov;
                } else if (segs[s].cat == MC_NAMED) {
                    // MC_NAMED carries its xterm color in 'frac' (see MemSeg).
                    if (ov > namedOv) { namedOv = ov; namedColor = (uint8_t)segs[s].frac; }
                } else if (ov > bestSolidOv) {
                    bestSolidOv = ov;
                    bestSolidCat = segs[s].cat;
                }
            }

            int color;
            if (namedOv > 0) {
                color = namedColor;        // a marked buffer touches this cell → highlight it
            } else if (gradOv > 0 && gradOv >= bestSolidOv) {
                color = usedRamp256((float)(gradUsed / (double)gradOv));
            } else if (bestSolidCat >= 0) {
                color = kCat[bestSolidCat].color;
            } else {
                color = kCat[MC_VACANT].color;
            }

            if (color != curColor) { Serial.printf("\033[38;5;%dm", color); curColor = color; }
            Serial.print("█");
        }
        if (curColor != -1) Serial.print("\033[0m");
        Serial.print("\n\r");
        if ((r & 3) == 3) Serial.flush();
    }
    Serial.flush();
}

void printMemLegend() {
    Serial.print("  \033[90mfree \033[0m");
    for (int i = 0; i <= 5; i++) Serial.printf("\033[38;5;%dm█", usedRamp256(i / 5.0f));
    Serial.print("\033[0m\033[90m used    \033[0m");
    Serial.printf("\033[38;5;%dm█\033[0m firmware  ", kCat[MC_CODE].color);
    Serial.printf("\033[38;5;%dm█\033[0m static  ",   kCat[MC_STATIC].color);
    Serial.printf("\033[38;5;%dm█\033[0m stack\n\r",  kCat[MC_STACK].color);
    Serial.printf("  \033[38;5;%dm█\033[0m eeprom    ",   kCat[MC_EEPROM].color);
    Serial.printf("\033[38;5;%dm█\033[0m \u00b5python  ", kCat[MC_MPHEAP].color);
    Serial.printf("\033[38;5;%dm█\033[0m reserved  ", kCat[MC_RESERVED].color);
    Serial.printf("\033[38;5;%dm█\033[0m vacant\n\r", kCat[MC_VACANT].color);
    Serial.flush();
}

// ── Big named static buffers ─────────────────────────────────────────────────
//
// The static region (.data + .bss) is ~240 KB and otherwise renders as one flat
// block. These are the largest individual buffers the firmware reserves there;
// addresses + sizes come straight from the real symbols so they always track the
// current build. Each is painted in a distinct hue on top of the static block so
// you can see where the big memory hogs actually live.
struct NamedAlloc { const char* label; uintptr_t addr; uint64_t size; uint8_t color; };

// Distinct hues kept clear of the category colors so a marked buffer pops out of
// the cyan static background.
const uint8_t kNamedPalette[] = {
    cube256(5,2,0),  // orange
    cube256(5,0,2),  // rose
    cube256(4,0,5),  // purple
    cube256(2,3,5),  // sky blue
    cube256(5,5,5),  // white
    cube256(5,3,5),  // pink
    cube256(3,5,0),  // lime
    cube256(4,3,2),  // tan
    cube256(0,5,3),  // aqua-green
    cube256(5,5,2),  // pale yellow
};
constexpr int kNamedPaletteN = (int)(sizeof(kNamedPalette) / sizeof(kNamedPalette[0]));

// Fill 'out' with the tracked big static buffers, sorted largest-first, each
// assigned a palette color. Returns the count written.
int collectNamedStatics(NamedAlloc* out, int cap) {
    NamedAlloc all[] = {
        { "globalState",      (uintptr_t)&globalState,         sizeof(globalState),         0 },
        { "graphicOverlay",   (uintptr_t)&graphicOverlayState, sizeof(graphicOverlayState), 0 },
        { "uart RX ring",     (uintptr_t)&uartReceived,        sizeof(uartReceived),        0 },
        { "rowAnimations",    (uintptr_t)&rowAnimations,       sizeof(rowAnimations),       0 },
        { "newBridges",       (uintptr_t)&newBridges,          sizeof(newBridges),          0 },
        { "logoColors",       (uintptr_t)&logoColorsAll,       sizeof(logoColorsAll),       0 },
        { "newBridge",        (uintptr_t)&newBridge,           sizeof(newBridge),           0 },
        { "changedNetColors", (uintptr_t)&changedNetColors,    sizeof(changedNetColors),    0 },
        { "customBitmap",     (uintptr_t)&customBitmapBuffer,  sizeof(customBitmapBuffer),  0 },
    };
    int total = (int)(sizeof(all) / sizeof(all[0]));
    // Selection sort by size, largest first (tiny list — simplicity over speed).
    for (int i = 0; i < total; i++)
        for (int j = i + 1; j < total; j++)
            if (all[j].size > all[i].size) { NamedAlloc t = all[i]; all[i] = all[j]; all[j] = t; }
    int n = 0;
    for (int i = 0; i < total && n < cap; i++) {
        all[i].color = kNamedPalette[n % kNamedPaletteN];
        out[n++] = all[i];
    }
    return n;
}

}  // namespace

void action_memoryMap() {
    char b1[24], b2[24], b3[24], b4[24], info[200];

    // Borrow the existing 24 KB shared transfer buffer for the segment scratch
    // instead of reserving a dedicated one. On PSRAM units it lives in PSRAM (so
    // the SRAM map stays clean); otherwise it's already on the SRAM heap. We
    // clear() it on the way out since we scribble binary into its body.
    SharedBuffer& shared = SharedBuffer::getInstance();
    MemSeg* segScratch = (MemSeg*)shared.rawBuffer();
    if (!segScratch) {
        Serial.println("\n\r\033[91mmemory map: shared buffer unavailable\033[0m");
        Serial.flush();
        return;
    }

    Serial.println("\n\r\033[1m╭──────────────────────────────────────────────────────────────────╮\033[0m");
    Serial.println(    "\033[1m│                       GRANULAR MEMORY MAP                        │\033[0m");
    Serial.println(    "\033[1m╰──────────────────────────────────────────────────────────────────╯\033[0m\n\r");
    printMemLegend();

    // ── SRAM ────────────────────────────────────────────────────────────────
    {
        uintptr_t base       = 0x20000000u;
        uintptr_t heapStart  = (uintptr_t)&__end__;
        uintptr_t stackLimit = (uintptr_t)&__StackLimit;   // top of heap = 0x20080000
        uintptr_t sramEnd    = 0x20082000u;                // + 8 KB scratch (core stacks)
        uint64_t  total      = sramEnd - base;

        struct mallinfo mi = mallinfo();
        uint64_t heapTotal = (heapStart < stackLimit) ? (stackLimit - heapStart) : 0;

        MemSeg* segs = segScratch;
        int n = 0;
        // Everything below the heap is static / startup data (.data + .bss + vectors).
        segs[n++] = { 0, (uint64_t)(heapStart - base), MC_STATIC, 0.0f };

        // Walk newlib's heap block-by-block for a true per-allocation map.
        // Pause core 1 so the chunk chain can't shift mid-walk.
        extern volatile bool pauseCore2;
        bool wasPaused = pauseCore2;
        pauseCore2 = true;
        delay(2);
        uintptr_t topEnd = 0;
        BlockRunCtx ctx{ segs, n, kMaxSegs, base, -1, 0 };
        size_t blocks = sram_heap_walk(blockRunCb, &ctx, &topEnd);
        pauseCore2 = wasPaused;
        n = ctx.n;

        uint64_t heapUsed, heapFree;
        if (blocks > 0 && topEnd > base) {
            // True walk succeeded: used = sum of in-use chunks; the gap from the
            // sbrk top up to __StackLimit has never been touched.
            heapUsed = ctx.usedSum;
            heapFree = (heapTotal > heapUsed) ? (heapTotal - heapUsed) : 0;
            if (topEnd < stackLimit)
                segs[n++] = { (uint64_t)(topEnd - base), (uint64_t)(stackLimit - topEnd),
                              MC_VACANT, 0.0f };
        } else {
            // Fallback: no per-block detail — show the sbrk arena as one heatmap.
            uint64_t arena = (uint64_t)(unsigned)mi.arena;
            if (arena > heapTotal) arena = heapTotal;
            heapUsed = (uint64_t)(unsigned)mi.uordblks;
            if (heapUsed > arena) heapUsed = arena;
            heapFree = (heapTotal > heapUsed) ? (heapTotal - heapUsed) : 0;
            float arenaFrac = arena > 0 ? (float)((double)heapUsed / (double)arena) : 0.0f;
            n = 1;  // keep the static seg only
            if (arena > 0)
                segs[n++] = { (uint64_t)(heapStart - base), arena, MC_GRADIENT, arenaFrac };
            if (heapTotal > arena)
                segs[n++] = { (uint64_t)(heapStart - base) + arena, heapTotal - arena,
                              MC_VACANT, 0.0f };
        }
        // 2x 4 KB scratch banks at the top hold the per-core stacks.
        segs[n++] = { (uint64_t)(stackLimit - base), 8192, MC_STACK, 0.0f };

        // Mark the biggest named static buffers on top of the generic static
        // block so the memory hogs are visible by location.
        NamedAlloc named[16];
        int nNamed = collectNamedStatics(named, 16);
        // The borrowed shared buffer (24 KB). On no-PSRAM units it sits on the
        // SRAM heap, so mark it here; on PSRAM units its address is out of the
        // SRAM range and the guard below simply skips it.
        if (nNamed < 16)
            named[nNamed++] = { "sharedBuffer", (uintptr_t)segScratch,
                                (uint64_t)SHARED_BUFFER_SIZE, (uint8_t)cube256(1,1,5) };
        for (int i = 0; i < nNamed && n < kMaxSegs; i++) {
            if (named[i].addr < base) continue;
            uint64_t off = (uint64_t)(named[i].addr - base);
            if (off + named[i].size > total) continue;  // must fall within the rendered SRAM range
            segs[n++] = { off, named[i].size, MC_NAMED, (float)named[i].color };  // color in frac
        }

        snprintf(info, sizeof(info),
                 "\033[97mheap\033[0m used %s / %s (%d%%)   \033[90mfree\033[0m %s   \033[90mstatic\033[0m %s   \033[90mblocks\033[0m %u",
                 fmtBytes(heapUsed, b1, sizeof(b1)),
                 fmtBytes(heapTotal, b2, sizeof(b2)),
                 heapTotal ? (int)((heapUsed * 100) / heapTotal) : 0,
                 fmtBytes(heapFree, b3, sizeof(b3)),
                 fmtBytes((uint64_t)(heapStart - base), b4, sizeof(b4)),
                 (unsigned)blocks);
        drawMemRegion("SRAM  (520 KB)", info, base, total, segs, n);

        // Key for the marked buffers (matches the colors painted above).
        if (nNamed > 0) {
            Serial.println("  \033[90mmarked buffers (biggest static + shared buffer):\033[0m");
            for (int i = 0; i < nNamed; i++) {
                char sb[24];
                Serial.printf("  \033[38;5;%dm█\033[0m %-16s %8s  \033[90m0x%08lX\033[0m\n\r",
                              named[i].color, named[i].label,
                              fmtBytes(named[i].size, sb, sizeof(sb)),
                              (unsigned long)named[i].addr);
            }
            Serial.flush();
        }
    }

    // ── PSRAM ─────────────────────────────────────────────────────────────────
    if (psram_available()) {
        uintptr_t base     = 0x11000000u;
        uintptr_t poolBase = psram_pool_base();
        uint64_t  poolSize = psram_pool_size();
        uint64_t  arenaTot = psram_app_total();
        uintptr_t mpBase   = psram_mp_base();
        uint64_t  mpSize   = psram_mp_size();
        uint64_t  total    = arenaTot + mpSize;
        uint64_t  headerSz = (poolBase > base) ? (poolBase - base) : 0;

        MemSeg* psegs = segScratch;
        int n = 0;
        // ArenaState + reserved journal/undo header.
        if (headerSz > 0) psegs[n++] = { 0, headerSz, MC_RESERVED, 0.0f };

        // Walk the pool block-by-block for a true per-allocation map.
        BlockRunCtx ctx{ psegs, n, kMaxSegs, base, -1, 0 };
        psram_arena_walk(blockRunCb, &ctx);
        n = ctx.n;

        // MicroPython GC heap occupies the remainder of the chip.
        if (mpSize > 0 && n < kMaxSegs)
            psegs[n++] = { (uint64_t)(mpBase - base), mpSize, MC_MPHEAP, 0.0f };

        uint64_t poolFree = psram_app_free();
        uint64_t poolUsed = (poolSize > poolFree) ? (poolSize - poolFree) : 0;
        snprintf(info, sizeof(info),
                 "\033[97mapp arena\033[0m used %s / %s (%d%%)   \033[90mreserved\033[0m %s   \033[90m\u00b5python GC\033[0m %s",
                 fmtBytes(poolUsed, b1, sizeof(b1)),
                 fmtBytes(poolSize, b2, sizeof(b2)),
                 poolSize ? (int)((poolUsed * 100) / poolSize) : 0,
                 fmtBytes(headerSz, b3, sizeof(b3)),
                 fmtBytes(mpSize, b4, sizeof(b4)));
        drawMemRegion("PSRAM  (8 MB)", info, base, total, psegs, n);
    } else {
        Serial.println("\n\r\033[1;97m  PSRAM\033[0m  \033[90m[0x11000000]\033[0m");
        Serial.printf("  \033[90mnot present / app arena disabled (psram_installed=%d)\033[0m\n\r",
                      jumperlessConfig.hardware.psram_installed);
        Serial.flush();
    }

    // ── FLASH ───────────────────────────────────────────────────────────────
    {
        uintptr_t base    = (uintptr_t)&__flash_binary_start;  // 0x10000000
        uintptr_t fwEnd   = (uintptr_t)&__flash_binary_end;
        uintptr_t fsStart = (uintptr_t)&_FS_start;
        uintptr_t fsEnd   = (uintptr_t)&_FS_end;
        uintptr_t eeEnd   = fsEnd + 4096;                      // 4 KB EEPROM above FatFS
        uint64_t  total   = eeEnd - base;

        uint64_t fsTotal = 0, fsUsed = 0;
        FSInfo fsinfo;
        if (FatFS.info(fsinfo)) { fsTotal = fsinfo.totalBytes; fsUsed = fsinfo.usedBytes; }
        float fsFrac = fsTotal > 0 ? (float)((double)fsUsed / (double)fsTotal) : 0.0f;

        MemSeg segs[6];
        int n = 0;
        segs[n++] = { 0, (uint64_t)(fwEnd - base), MC_CODE, 0.0f };
        if (fsStart > fwEnd)
            segs[n++] = { (uint64_t)(fwEnd - base), (uint64_t)(fsStart - fwEnd), MC_VACANT, 0.0f };
        segs[n++] = { (uint64_t)(fsStart - base), (uint64_t)(fsEnd - fsStart), MC_GRADIENT, fsFrac };
        segs[n++] = { (uint64_t)(fsEnd - base), 4096, MC_EEPROM, 0.0f };

        snprintf(info, sizeof(info),
                 "\033[97mfirmware\033[0m %s   \033[90mfree sketch\033[0m %s   \033[97mFatFS\033[0m %s / %s (%d%%)   \033[90meeprom\033[0m 4.0 KB",
                 fmtBytes((uint64_t)(fwEnd - base), b1, sizeof(b1)),
                 fmtBytes((uint64_t)(fsStart - fwEnd), b2, sizeof(b2)),
                 fmtBytes(fsUsed, b3, sizeof(b3)),
                 fmtBytes(fsTotal, b4, sizeof(b4)),
                 fsTotal ? (int)((fsUsed * 100) / fsTotal) : 0);
        drawMemRegion("FLASH  (16 MB)", info, base, total, segs, n);
    }

    shared.clear();   // leave the borrowed buffer empty (we wrote binary into it)
    Serial.println();
    Serial.flush();
}

    void action_i2cScan() {
    Serial.println("\n\rScanning I2C bus...\n\r");
    
    int deviceCount = 0;
    for (int addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        int error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.printf("Device found at 0x%02X", addr);
            
            // Identify common devices
            if (addr == 0x3C || addr == 0x3D) Serial.print(" (OLED display)");
            else if (addr == 0x60 || addr == 0x61) Serial.print(" (MCP4725 DAC)");
            else if (addr == 0x48) Serial.print(" (ADS1115 ADC)");
            else if (addr == 0x27 || addr == 0x3F) Serial.print(" (PCF8574)");
            else if (addr >= 0x50 && addr <= 0x57) Serial.print(" (EEPROM)");
            
            Serial.println();
            deviceCount++;
        }
    }
    
    if (deviceCount == 0) {
        Serial.println("No I2C devices found!");
    } else {
        Serial.println("\n\rFound " + String(deviceCount) + " device(s)");
    }
    Serial.flush();
}

void action_speedTest() {
    Serial.println("\n\rRaw Crossbar Speed Test...\n\r");
    
    pauseCore2 = true;
    delay(100);
    unsigned long cycles = 100000;
    unsigned long start = micros();
    
    sendXYraw(10, 0, 4, 1);
    for (unsigned long i = 0; i < cycles; i++) {
        sendXYraw(10, 0, 0, 1);
        sendXYraw(10, 0, 0, 0);
    }
    unsigned long end = micros();
    
    Serial.print("Time for ");
    Serial.print(cycles);
    Serial.print(" on/off cycles: ");
    Serial.print(end - start);
    Serial.println(" microseconds");
    
    Serial.print("Time per cycle: ");
    Serial.print((end - start) / cycles);
    Serial.println(" microseconds");
    
    Serial.print("Frequency: ");
    Serial.print(((float)cycles / (float)(end - start)) * 1000);
    Serial.println(" kHz");
    
    pauseCore2 = false;
    Serial.flush();
}

void action_colorSpectrum() {
  cmd_printColorSpectrum( 'j', "" );
  
    // Serial.println("\n\rTerminal Color Spectrum:\n\r");
    
    // // Standard colors
    // Serial.println("Standard colors:");
    // for (int i = 0; i < 8; i++) {
    //     Serial.printf("\033[%dm %d \033[0m", 30 + i, i);
    // }
    // Serial.println();
    
    // // Bright colors
    // Serial.println("\n\rBright colors:");
    // for (int i = 0; i < 8; i++) {
    //     Serial.printf("\033[%dm %d \033[0m", 90 + i, i);
    // }
    // Serial.println();
    
    // // 256 color spectrum (first 16 colors)
    Serial.println("\n\r256-color palette (0-15):");
    for (int i = 0; i < 16; i++) {
        Serial.printf("\033[48;5;%dm  \033[0m", i);
    }
    Serial.println();
    
    // Color cube sample
    Serial.println("\n\rColor cube:");
    for (int r = 0; r < 6; r++) {
        for (int g = 0; g < 6; g++) {
            for (int b = 0; b < 6; b += 1) {
                int color = 16 + (r * 36) + (g * 6) + b;
                Serial.printf("\033[48;5;%dm  \033[0m", color);
            }
        }
        Serial.println();
    }
    
    Serial.println();
    Serial.flush();
}

// ============================================================================
// FakeGPIO Live Debug Display
// ============================================================================

CommandResult cmd_fakeGpioDebug(char c, const String& line) {
    extern TimeDomainMultiplexer tdmInputs;

    Serial.println("\n\r\033[1m=== FakeGPIO Live Debug ===\033[0m");
    Serial.println("  (send any key to exit)\n\r");

    int lineCount = 0;

    do {
        lineCount = 0;

        // --- OUTPUTS ---
        int numOuts = 0;
        for (int i = 0; i < MAX_FAKE_GP_OUT; i++) {
            if (fakeGpioOutputs[i].active) numOuts++;
        }
        if (numOuts > 0) {
            Serial.print("\033[1mOutputs\033[0m  ");
            Serial.printf("(%d active)\n\r", numOuts);
            lineCount++;
            Serial.println("  Slot  Node  State  HighSrc  LowSrc  ChipKY  FastPath  Net");
            lineCount++;
            for (int i = 0; i < MAX_FAKE_GP_OUT; i++) {
                if (!fakeGpioOutputs[i].active) continue;
                FakeGpioOutput& out = fakeGpioOutputs[i];
                Serial.printf("  %-4d  %-4d  %-5s  ", i, out.userNode,
                              out.currentState ? "HIGH" : "LOW");
                printNodeOrName(out.highVoltageNode);
                Serial.print("\t ");
                printNodeOrName(out.lowVoltageNode);
                Serial.printf("\t %-6d  %-8s  %d\n\r",
                              out.chipKY,
                              out.fastPathReady ? "yes" : "no",
                              out.netIndex);
                lineCount++;
            }
        }

        // --- INPUTS ---
        int numIns = 0;
        for (int i = 0; i < MAX_FAKE_GP_IN; i++) {
            if (fakeGpioInputs[i].active) numIns++;
        }
        if (numIns > 0) {
            Serial.printf("\n\r\033[1mInputs\033[0m   (%d active, ADC%d, TDM channels: %d)\n\r",
                          numIns, tdmInputs.adcChannel, tdmInputs.activeCount);
            lineCount += 2;
            Serial.println("  Slot  Node  State  Voltage   ThreshH  ThreshL  TDM  ChipKY  Net");
            lineCount++;
            for (int i = 0; i < MAX_FAKE_GP_IN; i++) {
                if (!fakeGpioInputs[i].active) continue;
                FakeGpioInput& in = fakeGpioInputs[i];

                float voltage = 0.0f;
                int8_t chipKY = -1;
                if (in.tdmSlot >= 0 && in.tdmSlot < TDM_MAX_CHANNELS) {
                    voltage = tdmInputs.channels[in.tdmSlot].lastVoltage;
                    chipKY = tdmInputs.channels[in.tdmSlot].chipKY;
                }

                const char* stateStr = (in.currentState == 1) ? "HIGH" :
                                        (in.currentState == 0) ? "LOW " : "??? ";

                Serial.printf("  %-4d  %-4d  %s  %+7.3fV  %5.2f    %5.2f     %-3d  %-6d  %d\n\r",
                              i, in.userNode, stateStr,
                              voltage, in.thresholdHigh, in.thresholdLow,
                              in.tdmSlot, chipKY, in.netIndex);
                lineCount++;
            }
        }

        if (numOuts == 0 && numIns == 0) {
            Serial.println("  No FakeGPIO pins configured.");
            lineCount++;
        }

        Serial.flush();

        // Wait for change or keypress
        unsigned long startTime = millis();
        while (Serial.available() == 0) {
            if (millis() - startTime > 150) break;  // Refresh ~6-7 Hz
        }

        if (Serial.available() > 0) {
            while (Serial.available() > 0) Serial.read();
            break;
        }

        // Move cursor up to overwrite (ANSI escape)
        Serial.printf("\033[%dA\033[J", lineCount);
        Serial.flush();

    } while (true);

    Serial.println("\n\r\033[0m");
    Serial.flush();
    return CMD_DONT_SHOW_MENU;
}

