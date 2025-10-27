// SPDX-License-Identifier: MIT
#ifndef JERIAL_H
#define JERIAL_H

#include <Arduino.h>
#include "hardware/uart.h"
#include "oled.h"
#include "SyntaxHighlighting.h"

// Forward declarations
class ScriptHistory;
class TermControl;

// Global variable for interactive mode tracking
extern int termInInteractiveMode;

/**
 * Jerial - Unified Serial Communications System
 * 
 * A drop-in replacement for Serial with advanced routing, terminal control,
 * and intelligent endpoint management.
 * 
 * Features:
 * - Route output to multiple streams simultaneously (broadcast mode)
 * - Automatic tag stripping (<j> and </j> tags)
 * - Terminal control with line editing and history (for USB Serial endpoints)
 * - Intelligent OLED filtering (strips consecutive newlines and ANSI codes)
 * - Support for USB CDC, UART, and OLED endpoints
 * - Full Stream interface compatibility
 * 
 * Usage:
 *   Jerial.addOutputStream(&Serial);      // Add Serial as output
 *   Jerial.addOutputStream(&OLEDOut);     // Add OLED as output
 *   Jerial.setInputStream(&Serial);       // Set Serial as input with terminal control
 *   Jerial.println("Hello!");             // Broadcasts to all outputs with appropriate filtering
 */

#define JERIAL_MAX_OUTPUTS 10
#define JERIAL_MAX_LINE_LENGTH 512
#define JERIAL_NEWLINE_OUT "\r\n"

enum class JerialEndpoint {
    NONE = 0,
    USB_SERIAL,      // Main Serial (USB CDC 0) - supports terminal control
    USB_SER1,        // USBSer1 (USB CDC 1) - supports terminal control
    USB_SER2,        // USBSer2 (USB CDC 2) - supports terminal control
    USB_SER3,        // USBSer3 (USB CDC 3) - supports terminal control
    SERIAL1,         // Arduino Serial1 (UART) - raw passthrough
    UART0,           // Direct hardware UART0 - raw passthrough
    UART1,           // Direct hardware UART1 - raw passthrough
    OLED,            // OLED display output - filtered for display
    CUSTOM           // Custom stream pointer
};

class JerialClass : public Stream {
public:
    JerialClass();
    ~JerialClass();

    // ============================================================================
    // Stream Interface - Full compatibility with Serial
    // ============================================================================
    
    virtual int available() override;
    virtual int read() override;
    virtual int peek() override;
    virtual void flush() override;
    virtual size_t write(uint8_t byte) override;
    virtual size_t write(const uint8_t *buffer, size_t size) override;
    virtual int availableForWrite() override;
    
    // ============================================================================
    // Terminal Control - Line Editing and History (for USB Serial endpoints)
    // ============================================================================
    
    /**
     * Process input and handle terminal control
     * Call this in main loop - returns true when a completed line is ready
     * Only works when input stream supports terminal control (USB Serial endpoints)
     */
    bool service();
    
    /**
     * Check if a completed line is ready for processing
     */
    bool hasCompletedLine() const;
    
    /**
     * Get the completed line (consumes it)
     */
    String getCompletedLine();
    
    /**
     * Peek at the completed line without consuming it
     */
    String peekCompletedLine() const;
    
    /**
     * Clear completed line without consuming
     */
    void clearCompletedLine();
    
    /**
     * Inject a completed line directly (for programmatic commands)
     */
    void injectCompletedLine(const char* line);
    
    /**
     * Get current line being edited (read-only)
     */
    const char* getCurrentLineBuffer();
    
    /**
     * Set terminal prompt
     */
    void setPrompt(const char* prompt);
    
    /**
     * Set colored terminal prompt
     */
    void setColoredPrompt(const char* prompt, int color_code = 79);
    
    /**
     * Enable/disable character echoing for terminal control
     */
    void enableEcho(bool enabled);
    
    /**
     * Get access to history manager (may be nullptr)
     */
    ScriptHistory* getHistory();
    
    // ============================================================================
    // Output Routing - Can broadcast to multiple streams
    // ============================================================================
    
    /**
     * Add an output stream for broadcasting
     * All write/print operations will be sent to all registered output streams
     */
    bool addOutputStream(Stream* stream);
    
    /**
     * Add an output stream by endpoint type
     */
    bool addOutputStream(JerialEndpoint endpoint);
    
    /**
     * Remove a specific output stream
     */
    bool removeOutputStream(Stream* stream);
    
    /**
     * Remove output stream by endpoint type
     */
    bool removeOutputStream(JerialEndpoint endpoint);
    
    /**
     * Clear all output streams
     */
    void clearOutputStreams();
    
    /**
     * Get number of registered output streams
     */
    int getOutputCount() const { return output_count; }
    
    /**
     * Set single output stream (clears existing and adds one)
     */
    void setOutputStream(Stream* stream);
    void setOutputStream(JerialEndpoint endpoint);
    
    // ============================================================================
    // Input Routing - Read from a specific source
    // ============================================================================
    
    /**
     * Set the input stream for read operations
     * If the stream supports terminal control (USB Serial), TermControl will be enabled
     */
    void setInputStream(Stream* stream);
    
    /**
     * Set input stream by endpoint type
     */
    void setInputStream(JerialEndpoint endpoint);
    
    /**
     * Get current input stream
     */
    Stream* getInputStream() const { return input_stream; }
    
    /**
     * Add an input source to monitor (for multi-source input)
     * When using serviceInputs(), will check all registered input sources
     */
    bool addInputSource(Stream* stream);
    bool addInputSource(JerialEndpoint endpoint);
    
    /**
     * Check all registered input sources and automatically switch to the one with data
     * Returns true if any source has data available
     * Call this in your main loop before checking available()
     */
    bool serviceInputs();
    
    /**
     * Inject data into the input stream (makes it appear as if it was received)
     * Useful for programmatically sending commands that should be processed normally
     * @param strip_tags If true, removes <j> and </j> tags from input
     */
    bool injectInput(const char* data, bool strip_tags = false);
    bool injectInput(const uint8_t* data, size_t size, bool strip_tags = false);
    
    /**
     * Clear the injection buffer
     */
    void clearInjectedInput();
    
    /**
     * Enable/disable automatic tag stripping for all input
     * When enabled, <j> and </j> tags are automatically removed from all input
     */
    void setAutoStripTags(bool enabled) { auto_strip_tags = enabled; }
    bool isAutoStripTags() const { return auto_strip_tags; }
    
    // ============================================================================
    // Utility Functions
    // ============================================================================
    
    /**
     * Check if a specific stream is registered as output
     */
    bool hasOutputStream(Stream* stream) const;
    
    /**
     * Enable/disable broadcast mode
     * When disabled, only writes to first output stream
     */
    void setBroadcastMode(bool enabled) { broadcast_enabled = enabled; }
    bool isBroadcastMode() const { return broadcast_enabled; }
    
    /**
     * Get endpoint type for a stream pointer
     */
    static JerialEndpoint getEndpointType(Stream* stream);
    
    /**
     * Get stream pointer for an endpoint type
     */
    static Stream* getStreamForEndpoint(JerialEndpoint endpoint);
    
    /**
     * Helper to convert endpoint to string name
     */
    static const char* endpointToString(JerialEndpoint endpoint);
    
    /**
     * Check if an endpoint supports terminal control features
     */
    static bool supportsTerminalControl(JerialEndpoint endpoint);
    
    // ============================================================================
    // Direct Print Methods - Override normal routing to print to specific port
    // ============================================================================
    
    /**
     * Print directly to a specific endpoint, bypassing normal output routing
     * Useful for sending data to a port that isn't registered as an output
     * @param endpoint The endpoint to write to
     * @param data The data to write
     * @return Number of bytes written
     */
    size_t printTo(JerialEndpoint endpoint, const char* data);
    size_t printTo(JerialEndpoint endpoint, const String& data);
    size_t printTo(JerialEndpoint endpoint, uint8_t byte);
    size_t printTo(JerialEndpoint endpoint, const uint8_t* buffer, size_t size);
    
    /**
     * Print directly to a specific stream, bypassing normal output routing
     */
    size_t printTo(Stream* stream, const char* data);
    size_t printTo(Stream* stream, const String& data);
    size_t printTo(Stream* stream, uint8_t byte);
    size_t printTo(Stream* stream, const uint8_t* buffer, size_t size);
    
    /**
     * Print line directly to a specific endpoint
     */
    size_t printlnTo(JerialEndpoint endpoint, const char* data = "");
    size_t printlnTo(JerialEndpoint endpoint, const String& data);
    
    /**
     * Print line directly to a specific stream
     */
    size_t printlnTo(Stream* stream, const char* data = "");
    size_t printlnTo(Stream* stream, const String& data);
    
private:
    // Output streams (for broadcasting)
    Stream* output_streams[JERIAL_MAX_OUTPUTS];
    int output_count;
    bool broadcast_enabled;
    
    // Input stream (single source)
    Stream* input_stream;
    
    // Input sources for auto-switching
    Stream* input_sources[JERIAL_MAX_OUTPUTS];
    int input_source_count;
    
    // Terminal control instance (created when needed for USB Serial endpoints)
    TermControl* term_control;
    bool term_control_active;
    
    // Injection buffer for programmatic input
    char injection_buffer[512];
    uint16_t injection_read_pos;
    uint16_t injection_write_pos;
    
    // Tag stripping
    bool auto_strip_tags;
    char tag_buffer[4];           // Buffer to detect <j> or </j> tags
    uint8_t tag_buffer_pos;       // Current position in tag buffer
    bool in_tag;                  // Currently inside a tag
    
    // Internal helpers
    size_t writeToOutputs(uint8_t byte);
    size_t writeToOutputs(const uint8_t *buffer, size_t size);
    size_t stripTagsAndInject(const char* data, size_t length);
    int readWithTagFilter();      // Read with tag filtering applied
    void createTermControlIfNeeded(Stream* stream); // Create TermControl for USB Serial endpoints
    void destroyTermControl();    // Clean up TermControl instance
};

// Global Jerial instance - use this like Serial
extern JerialClass Jerial;



/**
 * TermControl - Terminal line editing and history
 * 
 * Internal class used by Jerial for USB Serial endpoints.
 * Provides line buffering, ANSI escape sequence handling, and command history.
 * 
 * NOT intended for direct use - use Jerial instead.
 */
class TermControl : public Stream {
public:
    TermControl(Stream* underlying_stream, bool create_own_history = true);
    ~TermControl();

    // Standard Stream interface
    virtual int available() override;
    virtual int read() override;
    virtual int peek() override;
    virtual void flush() override;
    virtual size_t write(uint8_t byte) override;
    virtual size_t write(const uint8_t *buffer, size_t size) override;

    // Terminal-specific functionality
    bool service();                     // Call this in main loop to handle input
    bool hasCompletedLine() const;      // True when a complete line is ready
    String getCompletedLine();          // Get the completed line (consumes it)
    String peekCompletedLine() const;   // Get the completed line without consuming it
    void clearCompletedLine();          // Clear completed line without consuming
    void injectCompletedLine(const char* line); // Inject a completed line directly
    const char* getCurrentLineBuffer(); // Get current line being edited (read-only)
    void setPrompt(const char* prompt); // Set prompt string
    void enableEcho(bool enabled);      // Enable/disable character echoing
    ScriptHistory* getHistory() { return history; } // Get access to history instance
    void setColoredPrompt(const char* prompt, int color_code = 79); // Set colored prompt
    bool hasInput() const { return hasCompletedLine(); } // Alias for compatibility
    
    // Tag stripping
    void setAutoStripTags(bool enabled) { auto_strip_tags = enabled; }
    bool isAutoStripTags() const { return auto_strip_tags; }

private:
    Stream* stream;                     // Underlying stream (usually Serial)
    ScriptHistory* history;             // History manager
    bool owns_history;                  // True if we created the history instance
    bool history_initialized;           // True if history filesystem has been initialized
    
    // Current line being edited
    char current_line[JERIAL_MAX_LINE_LENGTH];      // Clean input buffer (no ANSI codes)
    char display_buffer[JERIAL_MAX_LINE_LENGTH * 4]; // Display buffer with ANSI codes
    int display_length;
    int line_length;
    int cursor_position;
    
    // Completed line ready for external parsing
    String completed_line;
    bool line_ready;
    
    // Settings
    String prompt_text;
    bool echo_enabled;
    SyntaxHighlighting syntax_highlighter;
    
    // ANSI escape sequence state
    enum AnsiState {
        ANSI_NORMAL,
        ANSI_ESCAPE,
        ANSI_BRACKET,
        ANSI_MAIN_SERIAL_ENQ
    } ansi_state;
    
    // Tag stripping
    bool auto_strip_tags;
    char tag_buffer[4];           // Buffer to detect <j> or </j> tags
    uint8_t tag_buffer_pos;       // Current position in tag buffer
    
    // Internal methods
    void handleNormalChar(char c);
    void handleBackspace();
    void handleEnter();
    void handleArrowUp();
    void handleArrowDown();
    void handleArrowLeft();
    void handleArrowRight();
    void handleCtrlU();
    void handleTab();
    void handleMainSerialENQ();
    
    void renderCurrentLine();
    void clearCurrentLine();
    void moveCursorTo(int position);
    void moveCursorToPosition(int position);
    void insertCharAtCursor(char c);
    void deleteCharAtCursor();
    int calculateVisualLength(const String& text);
    
    // Tag filtering
    bool shouldSkipChar(char c);
};



/**
 * OLEDStream - A Stream wrapper for the OLED display
 * 
 * Allows printing to the OLED using standard print/println/printf functions.
 * Automatically filters consecutive newlines and ANSI control sequences.
 * 
 * Features:
 * - Automatic line wrapping and scrolling
 * - Small text mode (5pt fonts) for better readability
 * - Filters consecutive newlines for compact display
 * - Strips ANSI color codes automatically
 * - Full Stream interface compatibility
 * 
 * Usage:
 *   OLEDStream oledOut;
 *   oledOut.println("Hello OLED!");
 *   oledOut.printf("Value: %d\n", 42);
 */

// OLEDSTREAM_MAX_LINES is now calculated dynamically based on display height
#define OLEDSTREAM_MAX_POSSIBLE_LINES 16  // Max for largest supported display (128x128)
#define OLEDSTREAM_LINE_LENGTH 32     // Max chars per line (conservative)
#define OLEDSTREAM_BUFFER_SIZE 512    // Total buffer size

class OLEDStream : public Stream {
public:
    OLEDStream();
    ~OLEDStream();
    
    // ============================================================================
    // Stream Interface
    // ============================================================================
    
    virtual int available() override;
    virtual int read() override;
    virtual int peek() override;
    virtual void flush() override;
    virtual size_t write(uint8_t byte) override;
    virtual size_t write(const uint8_t *buffer, size_t size) override;
    
    // ============================================================================
    // OLED-specific Functions
    // ============================================================================
    
    /**
     * Set the small font to use for display
     */
    void setSmallFont(SmallFont font);
    
    /**
     * Get current small font
     */
    SmallFont getSmallFont() const { return current_font; }
    
    /**
     * Get maximum number of lines for current display height
     */
    int getMaxLines() const { return max_lines; }
    
    /**
     * Enable/disable automatic display updates
     * When disabled, call flush() manually to update display
     */
    void setAutoUpdate(bool enabled) { auto_update = enabled; }
    bool isAutoUpdate() const { return auto_update; }
    
    /**
     * Clear the display and reset cursor
     */
    void clear();
    
    /**
     * Get current line number (0-based)
     */
    int getCurrentLine() const { return current_line; }
    
    /**
     * Get current column position (0-based)
     */
    int getCurrentColumn() const { return current_col; }
    
    /**
     * Enable/disable scrolling (when disabled, wraps to line 0)
     */
    void setScrollEnabled(bool enabled) { scroll_enabled = enabled; }
    bool isScrollEnabled() const { return scroll_enabled; }
    
    /**
     * Check if OLED is connected
     */
    bool isConnected() const;
    
private:
    // Line buffer for managing display content
    char line_buffer[OLEDSTREAM_MAX_POSSIBLE_LINES][OLEDSTREAM_LINE_LENGTH];
    
    // Current position
    int current_line;
    int current_col;
    
    // Display dimensions
    int max_lines; // Dynamically calculated based on display height (displayHeight / 8)
    
    // Settings
    SmallFont current_font;
    bool auto_update;
    bool scroll_enabled;
    
    // ANSI escape filtering
    bool in_ansi_escape;
    
    // Newline filtering (prevent consecutive newlines)
    bool last_was_newline;
    
    // Internal helpers
    void newline();
    void scrollUp();
    void updateDisplay();
    void printChar(char c);
    void recalculateMaxLines(); // Recalculate max_lines from oled.displayHeight
    bool isANSIEscape(char c); // Check if character is part of ANSI sequence
};

// Global instance
extern OLEDStream OLEDOut;


#include <Arduino.h>
#include "hardware/uart.h"
#include "hardware/gpio.h"

/**
 * UARTStream - A Stream wrapper for Pico hardware UART
 * 
 * Provides a Stream-compatible interface for direct UART access.
 * Useful when you want to bypass Arduino Serial1 and work directly
 * with the hardware UART peripheral.
 * 
 * Features:
 * - Full Stream interface compatibility
 * - Direct hardware UART access (uart0 or uart1)
 * - Configurable baud rate, parity, stop bits
 * - Hardware FIFO support
 * - Works alongside Serial1 or independently
 * 
 * Usage:
 *   UARTStream uart0Stream(uart0, 0, 1);  // UART0, TX=GP0, RX=GP1
 *   uart0Stream.begin(115200);
 *   uart0Stream.println("Hello UART!");
 */

class UARTStream : public Stream {
public:
    /**
     * Constructor
     * @param uart_inst UART instance (uart0 or uart1)
     * @param tx_pin GPIO pin for TX
     * @param rx_pin GPIO pin for RX
     */
    UARTStream(uart_inst_t* uart_inst, uint tx_pin, uint rx_pin);
    ~UARTStream();
    
    // ============================================================================
    // Initialization
    // ============================================================================
    
    /**
     * Initialize UART with specified baud rate
     */
    void begin(uint32_t baud = 115200, 
               uint data_bits = 8, 
               uint stop_bits = 1,
               uart_parity_t parity = UART_PARITY_NONE);
    
    /**
     * Deinitialize UART
     */
    void end();
    
    /**
     * Check if UART is initialized
     */
    bool isInitialized() const { return initialized; }
    
    /**
     * Change baud rate
     */
    void setBaudRate(uint32_t baud);
    
    /**
     * Get current baud rate
     */
    uint32_t getBaudRate() const { return current_baud; }
    
    // ============================================================================
    // Stream Interface
    // ============================================================================
    
    virtual int available() override;
    virtual int read() override;
    virtual int peek() override;
    virtual void flush() override;
    virtual size_t write(uint8_t byte) override;
    virtual size_t write(const uint8_t *buffer, size_t size) override;
    virtual int availableForWrite() override;
    
    // ============================================================================
    // UART-specific Functions
    // ============================================================================
    
    /**
     * Check if UART is writable (TX FIFO not full)
     */
    bool isWritable() const;
    
    /**
     * Check if UART is readable (RX FIFO has data)
     */
    bool isReadable() const;
    
    /**
     * Get UART instance
     */
    uart_inst_t* getUARTInstance() const { return uart; }
    
    /**
     * Enable/disable hardware flow control (RTS/CTS)
     */
    void setFlowControl(bool enable, uint cts_pin = 0, uint rts_pin = 0);
    
private:
    uart_inst_t* uart;
    uint tx_pin;
    uint rx_pin;
    uint32_t current_baud;
    bool initialized;
    
    // Peek buffer for peek() implementation
    bool has_peeked;
    uint8_t peek_byte;
};

#endif // JERIAL_H

