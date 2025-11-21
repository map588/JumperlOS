#ifndef AsyncPassthrough_h
#define AsyncPassthrough_h

#include "Arduino.h"

#define ASYNC_PASSTHROUGH_ENABLED 1

extern bool asyncPassthroughEnabled;
extern bool asyncPassthroughTagParsingEnabled;
extern unsigned long microsPerByteSerial1;
extern unsigned long serial1baud;
extern volatile bool s_line_coding_override;

#if ASYNC_PASSTHROUGH_ENABLED == 1
namespace AsyncPassthrough {
    // Initialize the CDC1 <-> UART passthrough (pico-sdk uart with HW FIFO)
    void begin(unsigned long baud = 115200);

    // Call frequently from loop() to move data in both directions and
    // apply any pending line-coding changes safely outside ISRs
    void task();

    // Expose UART received ring for other modules
    extern uint8_t uartReceived[4096];
    extern volatile uint16_t uartReceivedHead;
    extern volatile uint16_t uartReceivedTail;

    // Register command prefixes to forward UART payload to main Serial
    // Returns true on success, false if registry full
    bool registerForwardPrefix(const char* prefix);

    // Remove a previously-registered prefix (exact string match)
    bool unregisterForwardPrefix(const char* prefix);

    // List current prefixes; returns number written into out (may be truncated)
    size_t listForwardPrefixes(const char** out, size_t max);

    // Register/remove/list end tokens that terminate forwarding sessions
    bool registerForwardEnd(const char* token);
    bool unregisterForwardEnd(const char* token);
    size_t listForwardEnds(const char** out, size_t max);

    // Control whether newline (\n or \r) also ends forwarding (default true)
    void setForwardEndOnNewline(bool enable);
    
    // Control whether tag parsing is enabled (default true)
    // When disabled, all data passes through without checking for command tags
    void setTagParsingEnabled(bool enable);
    
    // Disable tag parsing for a specific duration, then auto-re-enable
    // Useful for Arduino flashing where you want to temporarily disable tag parsing
    // Example: disableTagParsingWithTimeout(5000) disables for 5 seconds
    void disableTagParsingWithTimeout(uint32_t timeout_ms);
    
    // Disable tag parsing with smart re-enable based on upload completion detection
    // Re-enables when EITHER condition is met:
    //   1. absolute_timeout_ms elapsed since disable (safety fallback)
    //   2. inactivity_timeout_ms elapsed since last USB->UART data (upload finished)
    // Example: disableTagParsingWithInactivityTimeout(5000, 500)
    //   - Re-enables after 500ms of no data (upload done) OR 5 seconds max (safety)
    void disableTagParsingWithInactivityTimeout(uint32_t absolute_timeout_ms, uint32_t inactivity_timeout_ms);
    
    bool getTagParsingEnabled();
    
    // Apply a new UART line coding immediately (baud/data/parity/stop)
    // Keeps passthrough active while updating hardware and timing
    void applyLineCodingOverride(uint32_t baud, uint8_t data_bits, uint8_t parity, uint8_t stop_bits);
    
    // DTR pulse detection and Arduino reset handling
    // Call this frequently to monitor DTR state changes on CDC interface
    void checkDTRState(Adafruit_USBD_CDC& cdc);
    
    // Returns true if a DTR pulse was detected since last check
    bool wasDTRPulseDetected();
    
    // Clear the DTR pulse flag
    void clearDTRPulse();
    
    // Trigger Arduino reset via GPIO pin
    void resetArduino(int resetPin);
}

#endif

// Interop C hooks to coordinate UART0 ownership with MicroPython
// Only effective when JL_UART0_INTEROP_MODE == 1
#ifdef __cplusplus
extern "C" {
#endif
void jl_asyncpassthrough_suspend_uart0( void );
void jl_asyncpassthrough_resume_uart0( void );

// Allow MicroPython to update UART0 line coding while integrating with passthrough
void jl_asyncpassthrough_override_line_coding( uint32_t baud, uint8_t data_bits, uint8_t parity, uint8_t stop_bits );
#ifdef __cplusplus
}
#endif

#endif