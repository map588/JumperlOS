#ifndef USB_INTERFACE_CONFIG_H
#define USB_INTERFACE_CONFIG_H
#include <stdint.h>
#include <stdbool.h>  // For bool type in C
// Note: Don't include config.h here - it causes preprocessor issues
// We'll handle dynamic naming in the USB descriptor callback

// =============================================================================
// SIMPLE STATIC USB Interface Configuration
// =============================================================================
// Fixed configuration: 5 CDC + 1 MSC for logic analyzer on port 3

// CDC Serial Interfaces (Communication Device Class)
// Overridable per board via -DUSB_CDC_ENABLE_COUNT=N (see platformio.ini).
// V5 exposes 4: CDC0 commands, USBSer1 Arduino passthrough, USBSer2 mpremote,
// USBSer3 LLM JSON backchannel. OG keeps the same layout; if the RP2040
// full-speed endpoint budget is tight, drop USBSer1 first (set this to 3).
#ifndef USB_CDC_ENABLE_COUNT
#define USB_CDC_ENABLE_COUNT 4
#endif

// MSC (Mass Storage Class). Overridable per board (OG may disable mass storage
// if USBfs is filtered out of the build).
#ifndef USB_MSC_ENABLE
#define USB_MSC_ENABLE 1
#endif

// All other interfaces disabled for simplicity
#define USB_MIDI_ENABLE 0
#define USB_HID_ENABLE 0
#define USB_HID_ENABLE_COUNT 0
#define USB_VENDOR_ENABLE 0

// =============================================================================
// TinyUSB Configuration Mapping
// =============================================================================

#define CFG_TUD_CDC USB_CDC_ENABLE_COUNT
#define CFG_TUD_MSC USB_MSC_ENABLE
#define CFG_TUD_MIDI USB_MIDI_ENABLE
#define CFG_TUD_HID USB_HID_ENABLE

// =============================================================================
// Simple Static Interface Names
// =============================================================================

static const char* USB_CDC_NAMES[] = {
    "Jumperless Main",       // CDC 0 - Main serial
    "JL UART Passthrough",   // CDC 1 - Arduino/Serial1
    "JL Micropython REPL",     // CDC 2 - Micropython REPL
    "JL TUI",                // CDC 3 - TUI
    "JL Serial 3"            // CDC 4 - Serial 3
};

#define USB_MSC_NAME     "JL Mass Storage"

// =============================================================================
// Interface Number Definitions (for debugging and verification)
// =============================================================================

// Calculate interface numbers based on enabled interfaces
enum {
  // CDC interfaces (each CDC uses 2 interface numbers)
#if USB_CDC_ENABLE_COUNT >= 1
  ITF_NUM_CDC_0 = 0,
  ITF_NUM_CDC_0_DATA,
#endif
#if USB_CDC_ENABLE_COUNT >= 2
  ITF_NUM_CDC_1,
  ITF_NUM_CDC_1_DATA,
#endif
#if USB_CDC_ENABLE_COUNT >= 3
  ITF_NUM_CDC_2,
  ITF_NUM_CDC_2_DATA,
#endif
#if USB_CDC_ENABLE_COUNT >= 4
  ITF_NUM_CDC_3,
  ITF_NUM_CDC_3_DATA,
#endif
#if USB_CDC_ENABLE_COUNT >= 5
  ITF_NUM_CDC_4,
  ITF_NUM_CDC_4_DATA,
#endif

  // MSC interface
#if USB_MSC_ENABLE
  ITF_NUM_MSC,
#endif

  ITF_NUM_TOTAL
};

// =============================================================================
// USB CDC Flow Control Configuration
// =============================================================================
// These functions allow ignoring DTR/RTS flow control signals from the host.
// This is useful for hosts that don't set DTR (industrial software, custom apps).

#ifdef __cplusplus
extern "C" {
#endif

// Global flag indicating whether to ignore DTR for connection status
extern volatile bool g_usb_ignore_dtr;

// Apply CDC configuration from jumperlessConfig - call after config is loaded
void usb_cdc_apply_config(void);

// Get current DTR ignore state
bool usb_cdc_get_ignore_dtr(void);

// Set DTR ignore mode at runtime (also updates config)
void usb_cdc_set_ignore_dtr(bool ignore);

#ifdef __cplusplus
}
#endif

#endif // USB_INTERFACE_CONFIG_H 