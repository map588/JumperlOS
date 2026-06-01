

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

        // Enable raw input mode indicator
        Serial.write(0x0E);  // Interactive mode on
        delay(10);
            
    
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
    
    // Clean up
    Serial.write(0x0F);  // Interactive mode off
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

