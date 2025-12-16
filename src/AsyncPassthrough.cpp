/**
 * AsyncPassthrough - Bidirectional UART↔USB bridge with command injection
 * 
 * Architecture (complete flow):
 * 
 * 1. UART→USB (Arduino sends commands):
 *    Arduino Serial1 → UART RX (ISR) → ring buffer
 *    → bridge_uart_to_usb() reads ring buffer
 *    → Parses <j>command</j> tags
 *    → Injects command chars to Jerial.injection_buffer
 *    → Raw data (with tags) → USBSer1 (CDC 1) for debugging
 *    → Filtered data (tags removed) → Serial (CDC 0) for display
 * 
 * 2. Jerial.injection_buffer → Command Processing:
 *    InjectionBufferStream wraps injection_buffer (automatically strips tags on read)
 *    → MultiSourceStream multiplexes InjectionBufferStream + Serial
 *    → TermControl reads from MultiSourceStream (line buffering)
 *    → Main loop calls Jerial.service() → TermControl::service()
 *    → Completed lines → SingleCharCommands → executeCommand()
 * 
 * 3. USB→UART (Upload/send to Arduino):
 *    Serial (CDC 0) → bridge_usb_to_uart() reads
 *    → Raw data → USBSer1 (CDC 1) for debugging
 *    → Filtered/forwarded → UART TX → Arduino Serial1
 * 
 * Benefits:
 *   - Non-blocking command injection using composable Stream layers
 *   - No 8-command queue limit (uses 512-byte injection buffer)
 *   - Complete debug visibility via USBSer1 (CDC 1)
 *   - Clean separation: AsyncPassthrough injects, Jerial processes
 */

// Debug flag for command injection tracing
// Set to 1 to see each character being injected from <j> tags
#define DEBUG_INJECTED_COMMANDS 0

#include "AsyncPassthrough.h"
#include "class/cdc/cdc_device.h"
#include "CommandBuffer.h"  // New simplified command buffer system
#if ASYNC_PASSTHROUGH_ENABLED == 1
// pico-sdk
#include "config.h"
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/uart.h>
#include <hardware/regs/uart.h>
#include <hardware/structs/uart.h>
#include <pico/stdlib.h>
// TinyUSB
#include <tusb.h>

// Arduino core
#include <Arduino.h>
#include "Jerial.h" // Unified serial interface
#include "Python_Proper.h" // For executeSinglePythonCommand and ScriptHistory

// External Jerial instance
extern JerialClass Jerial;

// External USBSer1 (declared in ArduinoStuff.cpp) - file scope, before namespace
extern Adafruit_USBD_CDC USBSer1;

bool asyncPassthroughEnabled = jumperlessConfig.serial_1.async_passthrough;
bool asyncPassthroughTagParsingEnabled = true; // Enable tag parsing by default

// Tag parsing timeout support (for Arduino flashing, etc.)
static uint32_t s_tag_parsing_timeout_ms = 0;  // 0 = no timeout
static uint32_t s_tag_parsing_disabled_time = 0;  // When tag parsing was disabled

// Smart re-enable: Track last USB->UART activity to detect end of upload
static uint32_t s_last_usb_to_uart_data_time = 0;  // Last time USB->UART data was sent
static uint32_t s_tag_parsing_inactivity_timeout_ms = 0;  // Re-enable after this many ms of no data (0 = disabled)

// ============================================================================
// Jumperless Command Tag Detection
// ============================================================================

// State machine for detecting command tags
enum TagState {
    TAG_SEARCHING,           // Looking for '<'
    TAG_DETECTING,           // Reading tag name (both opening and closing)
    TAG_IN_COMMAND           // Inside a command tag, accumulating command
};

// Tag parser state structure
struct TagParserState {
    TagState state;
    char tag_buffer[32];           // Buffer for tag name
    uint8_t tag_buffer_idx;
    char current_tag[32];          // Currently open tag name
    bool needs_python_prefix;      // For <p> tags: true if we need to inject '>'
    bool seen_first_char;          // For <p> tags: true after first non-whitespace char
    
    // Command accumulation buffer - collect entire command before injection
    // This eliminates the need for per-character delays that caused blocking
    char command_buffer[256];      // Buffer for accumulating command content
    uint16_t command_buffer_idx;   // Current position in command buffer
};

// Separate state machines for each direction
// Initialize with zeroed command buffers
static TagParserState usb_to_uart_parser = { TAG_SEARCHING, {0}, 0, {0}, false, false, {0}, 0 };
static TagParserState uart_to_usb_parser = { TAG_SEARCHING, {0}, 0, {0}, false, false, {0}, 0 };

// Command injection tracking (no rate limiting - process all commands immediately)
static uint32_t s_last_command_injection_time = 0;
static uint32_t s_injected_commands = 0;

// ============================================================================
// UART Response Queue - Routes command responses back to UART
// ============================================================================

// Queue for responses that need to be sent back to UART
#define UART_RESPONSE_QUEUE_SIZE 8
#define UART_RESPONSE_MAX_LEN 256

struct UARTResponseEntry {
    char data[UART_RESPONSE_MAX_LEN];
    uint16_t length;
    bool pending;
};

static UARTResponseEntry s_uart_response_queue[UART_RESPONSE_QUEUE_SIZE];
static volatile uint8_t s_uart_response_head = 0;
static volatile uint8_t s_uart_response_tail = 0;
static volatile uint8_t s_uart_response_count = 0;

// Track if current command came from UART (for response routing)
static volatile bool s_command_from_uart = false;

// ARCHITECTURAL NOTE: Character-by-character injection instead of line buffering
// ============================================================================
// Previous approach: Accumulate complete commands in 512-byte buffer, inject as lines
// Problem: 8-command queue filled while bridge_uart_to_usb() blocked (900-7000ms)
// 
// Current approach: Inject characters one-by-one as they arrive via Jerial.injectInput()
// Benefits:
//   - Uses larger injection_buffer (no 8-command queue limit)
//   - Commands flow through normal Jerial.read() path
//   - Main loop's line buffering handles command completion naturally
//   - No blocking accumulation - chars available immediately for processing
// ============================================================================

// Supported command tags
static const char* COMMAND_TAGS[] = {
    "jumperlessCommand",
    "j",
    "jumperless",
    "p"  // Python tag - works like 'j' but injects '>' prefix for Python commands
};
static const uint8_t NUM_COMMAND_TAGS = 4;

// ----------------------------------------------------------------------------
// Configuration
// ----------------------------------------------------------------------------

// Interop mode between AsyncPassthrough and MicroPython UART driver
// 0 = integrate (both use shared IRQ handler)
// 1 = preempt (AsyncPassthrough suspends while MicroPython owns UART0)
#ifndef JL_UART0_INTEROP_MODE
#define JL_UART0_INTEROP_MODE 0
#endif

#ifndef ASYNC_PASSTHROUGH_CDC_ITF
// Bridge TinyUSB CDC instance 1 (USBSer1) <-> Serial1
#define ASYNC_PASSTHROUGH_CDC_ITF 1
#endif

#ifndef ASYNC_PASSTHROUGH_UART
#define ASYNC_PASSTHROUGH_UART uart0
#define ASYNC_PASSTHROUGH_UART_IRQ UART0_IRQ

#define ASYNC_PASSTHROUGH_UART_TX_PIN 0
#define ASYNC_PASSTHROUGH_UART_RX_PIN 1
#endif

#ifndef ASYNC_PASSTHROUGH_UART_DEFAULT_BAUD
#define ASYNC_PASSTHROUGH_UART_DEFAULT_BAUD 115200u
#endif

unsigned long usPerByteSerial1 = ( 1000000 / 115200 + 1 ) * ( 8 + 0 + 1 );
unsigned long serial1baud = 115200;

// ----------------------------------------------------------------------------
// Helpers / State
// ----------------------------------------------------------------------------

static inline void set_micros_per_byte( cdc_line_coding_t const* line_coding ) {

    // Compute an approximate microseconds-per-byte based on line coding.
    // Bits per character = 1 start + data_bits + parity(0|1) + stop_bits(1|1.5|2)
    const uint32_t us_per_bit = ( 1000000u + line_coding->bit_rate - 1 ) / line_coding->bit_rate; // ceil

    const uint32_t parity_bits = ( line_coding->parity == 0 ? 0u : 1u );

    // CDC stop_bits: 0=1 stop, 1=1.5 stop, 2=2 stop
    uint32_t us_for_stops = us_per_bit; // default 1 stop
    if ( line_coding->stop_bits == 2 ) {
        us_for_stops = 2u * us_per_bit;
    } else if ( line_coding->stop_bits == 1 ) {
        // 1.5 stop bits -> round up
        us_for_stops = us_per_bit + ( us_per_bit >> 1 );
    }

    const uint32_t us_without_stops = ( 1u + (uint32_t)line_coding->data_bits + parity_bits ) * us_per_bit;
    usPerByteSerial1 = us_without_stops + us_for_stops;
	serial1baud = line_coding->bit_rate;
}

static inline uint16_t make_serial_config_from_line_coding( uint8_t data_bits, uint8_t parity, uint8_t stop_bits ) {
    // Map TinyUSB CDC line coding to Arduino's SERIAL_* bitfield
    unsigned long data = 0x400ul; // default 8 bits
    unsigned long par = 0x3ul;    // default NONE
    unsigned long stop = 0x10ul;  // default 1 stop bit

    switch ( data_bits ) {
    case 5:
        data = 0x100ul;
        break; // SERIAL_DATA_5
    case 6:
        data = 0x200ul;
        break; // SERIAL_DATA_6
    case 7:
        data = 0x300ul;
        break; // SERIAL_DATA_7
    default:
        data = 0x400ul;
        break; // SERIAL_DATA_8
    }

    // Per CDC spec: 0=None,1=Odd,2=Even,3=Mark,4=Space
    switch ( parity ) {
    case 2:
        par = 0x1ul;
        break; // SERIAL_PARITY_EVEN
    case 1:
        par = 0x2ul;
        break; // SERIAL_PARITY_ODD
    case 3:
        par = 0x4ul;
        break; // SERIAL_PARITY_MARK
    case 4:
        par = 0x5ul;
        break; // SERIAL_PARITY_SPACE
    default:
        par = 0x3ul;
        break; // SERIAL_PARITY_NONE
    }

    // Per CDC spec: 0=1 stop, 1=1.5 stop, 2=2 stop
    switch ( stop_bits ) {
    case 2:
        stop = 0x30ul;
        break; // SERIAL_STOP_BIT_2
    case 1:
        stop = 0x20ul;
        break; // SERIAL_STOP_BIT_1_5
    default:
        stop = 0x10ul;
        break; // SERIAL_STOP_BIT_1
    }

    return (uint16_t)( data | par | stop );
}

// ----------------------------------------------------------------------------
// UART RX IRQ support: push to TinyUSB or fallback ring buffer
// ----------------------------------------------------------------------------

static volatile bool s_uart_flush_pending = false;
// Track suspend state to avoid concurrent access when MicroPython owns UART0
static volatile bool s_uart_suspended_by_mpy = false;
// Exposed ring: uartReceived
uint8_t uartReceived[ 4096 ];
volatile uint16_t uartReceivedHead = 0;
volatile uint16_t uartReceivedTail = 0;
static volatile uint32_t uartReceivedOverflowCount = 0;
static volatile uint32_t s_uart_overrun_count = 0;
static volatile uint32_t s_uart_framing_error_count = 0;
static volatile uint32_t s_uart_framing_error_total = 0;
static volatile uint32_t s_uart_resync_count = 0;

// Garbage detection - non-ASCII/non-printable bytes indicate misalignment
static volatile uint32_t s_uart_garbage_count = 0;        // Consecutive garbage bytes
static volatile uint32_t s_uart_garbage_total = 0;        // Total garbage bytes seen
static volatile uint32_t s_uart_garbage_resyncs = 0;      // Resyncs triggered by garbage

// Framing error threshold - after this many consecutive FE errors, force resync
#define FRAMING_ERROR_RESYNC_THRESHOLD 3

// Garbage detection threshold - after this many consecutive non-ASCII bytes, resync
// Non-ASCII = bytes outside printable range (0x20-0x7E) plus common control chars
#define GARBAGE_RESYNC_THRESHOLD 4

// Check if a byte is "garbage" (likely from misaligned framing)
// Valid bytes: printable ASCII (0x20-0x7E), newline, carriage return, tab
static inline bool is_garbage_byte(uint8_t c) {
    // Printable ASCII range
    if (c >= 0x00 && c <= 0x7E) return false;
    // Common control characters we expect
    if (c == '\n' || c == '\r' || c == '\t') return false;
    // Null can appear in binary protocols
    if (c == 0x00) return false;
    // Everything else is suspicious - especially high-bit-set bytes (0x80-0xFF)
    return true;
}

// ============================================================================
// Idle Line Detection and Timing Validation
// ============================================================================

// Track timing for idle detection and sync validation
static volatile uint32_t s_last_rx_byte_time_us = 0;
static volatile uint32_t s_last_idle_detected_time = 0;
static volatile uint32_t s_bytes_since_last_idle = 0;
static volatile uint32_t s_idle_periods_detected = 0;
static volatile bool s_line_was_idle = true;  // Start assuming idle (fresh start = synced)

// Timing anomaly detection
static volatile uint32_t s_timing_anomaly_count = 0;
static volatile uint32_t s_last_inter_byte_time_us = 0;

// Idle threshold: line is "idle" if no data for this many microseconds
// At 115200 baud, one byte takes ~87µs, so 200µs means ~2+ byte times of silence
#define UART_IDLE_THRESHOLD_US 200

// Timing validation tolerance: bytes should arrive at ≥ usPerByteSerial1
// Allow up to 20% faster than theoretical minimum (accounts for measurement jitter)
#define TIMING_TOLERANCE_PERCENT 80

#define UART_RECEIVED_MASK ( (uint16_t)( sizeof( uartReceived ) - 1 ) )

static inline bool ring_push_byte( uint8_t b ) {
    uint16_t next_head = (uint16_t)( ( uartReceivedHead + 1 ) & UART_RECEIVED_MASK );
    if ( next_head == uartReceivedTail ) {
        uartReceivedOverflowCount++;
        return false;
    }
    uartReceived[ uartReceivedHead ] = b;
    uartReceivedHead = next_head;
    return true;
}

static inline bool ring_pop_byte( uint8_t* out ) {
    if ( uartReceivedTail == uartReceivedHead ) return false;
    *out = uartReceived[ uartReceivedTail ];
    uartReceivedTail = (uint16_t)( ( uartReceivedTail + 1 ) & UART_RECEIVED_MASK );
    return true;
}

static inline uint16_t ring_available( ) {
    return (uint16_t)( ( uartReceivedHead - uartReceivedTail ) & UART_RECEIVED_MASK );
}

static inline uint8_t ring_peek_at( uint16_t offset ) {
    uint16_t idx = (uint16_t)( ( uartReceivedTail + offset ) & UART_RECEIVED_MASK );
    return uartReceived[ idx ];
}

// Force UART receiver resync by disabling/re-enabling RX
// This flushes the FIFO and forces the receiver to wait for a new start bit
// Also clears the ring buffer since it likely contains garbage
static inline void uart_force_receiver_resync( void ) {
    uart_hw_t* hw = uart_get_hw( ASYNC_PASSTHROUGH_UART );
    
    // Disable UART receiver (clears shift register and FIFO)
    hw_clear_bits( &hw->cr, UART_UARTCR_RXE_BITS );
    
    // Drain any remaining bytes from HW FIFO (they're garbage)
    while ( !(hw->fr & UART_UARTFR_RXFE_BITS) ) {
        volatile uint32_t discard = hw->dr;
        (void)discard;
    }
    
    // Clear error flags
    hw->rsr = 0xFFFFFFFFu;
    
    // CRITICAL: Clear the ring buffer - it likely contains garbage from misalignment
    // This prevents garbage from being processed by higher layers
    uartReceivedHead = 0;
    uartReceivedTail = 0;
    
    // Brief delay to ensure the receiver fully stops and line settles
    // At 115200 baud, one bit time is ~8.7us, so 100us is ~11 bit times (>1 frame)
    busy_wait_us( 100 );
    
    // Re-enable receiver - it will now wait for next valid start bit
    hw_set_bits( &hw->cr, UART_UARTCR_RXE_BITS );
    
    s_uart_resync_count++;
    s_uart_framing_error_count = 0;  // Reset consecutive error counters
    s_uart_garbage_count = 0;
}

static void async_uart_irq_handler( void ) {
    uart_hw_t* hw = uart_get_hw( ASYNC_PASSTHROUGH_UART );
    int i = 0;
    bool had_framing_error = false;
    
    // Get current time for timing validation
    uint32_t now_us = time_us_32();
    
    // Check if we were idle before this IRQ (natural sync point)
    uint32_t time_since_last = now_us - s_last_rx_byte_time_us;
    if ( time_since_last > UART_IDLE_THRESHOLD_US && s_last_rx_byte_time_us != 0 ) {
        // Line was idle - this is a natural sync point!
        // UART automatically resynced on the start bit of this new data
        s_line_was_idle = true;
        s_idle_periods_detected++;
        s_last_idle_detected_time = now_us;
        s_bytes_since_last_idle = 0;
        
        // After an idle period, we can trust the framing is good
        // Reset consecutive error counter since we've naturally resynced
        s_uart_framing_error_count = 0;
    } else {
        s_line_was_idle = false;
    }
    
    while ( uart_is_readable( ASYNC_PASSTHROUGH_UART ) ) {
        uint32_t byte_time_us = time_us_32();
        
        // Check for errors BEFORE reading the byte (errors are per-byte in FIFO)
        uint32_t dr = hw->dr;  // Read data register (includes error flags in upper bits)
        uint8_t c = (uint8_t)( dr & 0xFF );
        
        // Timing validation: check if inter-byte time is reasonable
        // Bytes can't arrive faster than one per usPerByteSerial1
        if ( i > 0 && s_last_rx_byte_time_us != 0 ) {
            uint32_t inter_byte_us = byte_time_us - s_last_rx_byte_time_us;
            s_last_inter_byte_time_us = inter_byte_us;
            
            // If bytes are arriving impossibly fast, something is wrong
            uint32_t min_time_us = (usPerByteSerial1 * TIMING_TOLERANCE_PERCENT) / 100;
            if ( inter_byte_us > 0 && inter_byte_us < min_time_us ) {
                // Bytes arriving faster than physically possible - timing anomaly
                // This could indicate we're misaligned and reading partial bytes
                s_timing_anomaly_count++;
            }
        }
        
        s_last_rx_byte_time_us = byte_time_us;
        s_bytes_since_last_idle++;
        
        // Check per-byte error flags from data register
        // DR bits: [11]=OE, [10]=BE, [9]=PE, [8]=FE
        bool has_hw_error = false;
        if ( dr & ( UART_UARTDR_OE_BITS | UART_UARTDR_FE_BITS | UART_UARTDR_PE_BITS | UART_UARTDR_BE_BITS ) ) {
            if ( dr & UART_UARTDR_FE_BITS ) {
                s_uart_framing_error_count++;
                s_uart_framing_error_total++;
                had_framing_error = true;
                has_hw_error = true;
            }
            if ( dr & UART_UARTDR_OE_BITS ) {
                s_uart_overrun_count++;
            }
            // Don't push bytes with framing errors - they're likely garbage
            if ( dr & UART_UARTDR_FE_BITS ) {
                s_uart_garbage_count++;  // Count as garbage too
                s_uart_garbage_total++;
                continue;  // Skip this corrupted byte
            }
        }
        
        // GARBAGE DETECTION: Disabled - was too aggressive and blocked valid data
        // The original intent was to filter framing errors, but it prevented
        // valid high-bit characters and ViperIDE responses from passing through.
        // Framing error detection via hardware flags (above) is sufficient.
        // 
        // if ( asyncPassthroughTagParsingEnabled && is_garbage_byte(c) ) {
        //     s_uart_garbage_count++;
        //     s_uart_garbage_total++;
        //     // Don't push garbage bytes to the ring buffer
        //     continue;
        // }
        
        // Good byte received - reset consecutive error/garbage counters
        if ( !has_hw_error ) {
            s_uart_framing_error_count = 0;
        }
        s_uart_garbage_count = 0;  // Reset garbage streak on valid byte
        
        ring_push_byte( c );
        i++;
        if (i > 1000) {
            // This shouldn't happen in normal operation
            break;
        }
    }
    
    // Check if we've accumulated too many consecutive framing errors
    // This indicates persistent misalignment that needs a resync
    // NOTE: Garbage byte resync disabled - was too aggressive
    bool needs_resync = false;
    if ( s_uart_framing_error_count >= FRAMING_ERROR_RESYNC_THRESHOLD ) {
        needs_resync = true;
    }
    // Garbage resync disabled - was preventing valid high-bit characters
    // if ( asyncPassthroughTagParsingEnabled && s_uart_garbage_count >= GARBAGE_RESYNC_THRESHOLD ) {
    //     s_uart_garbage_resyncs++;
    //     needs_resync = true;
    // }
    if ( needs_resync ) {
        uart_force_receiver_resync();
    }
    
    // Clear any sticky error flags in RSR (Receive Status Register)
    uint32_t rsr = hw->rsr;
    if ( rsr & ( UART_UARTRSR_OE_BITS | UART_UARTRSR_FE_BITS | UART_UARTRSR_PE_BITS | UART_UARTRSR_BE_BITS ) ) {
        hw->rsr = 0xFFFFFFFFu;  // Write to RSR (alias of ECR) clears errors
    }
    
    s_uart_flush_pending = true;
}

// ----------------------------------------------------------------------------
// Runtime command parsing - Parse "tag parsing = on/off" from UART
// ----------------------------------------------------------------------------
#define RUNTIME_CMD_RING_SIZE 32
static char s_runtime_cmd_ring[RUNTIME_CMD_RING_SIZE];
static uint8_t s_runtime_cmd_write_idx = 0;

// Parse runtime control commands from UART (e.g., "tag parsing = on")
// Uses a ring buffer that keeps only the last 32 characters
// Searches for "tag parsing =" anywhere in the buffer, allowing comment prefixes:
//   # tag parsing = off      (Python)
//   // tag parsing = on       (C/C++)
//   ; tag parsing = off       (Assembly/INI)
//   print("tag parsing = on") (anywhere in text)
// Returns: false always (never consumes, just monitors passively)
static inline bool process_runtime_command(uint8_t c) {
    // Add character to ring buffer (always, even if not a command)
    s_runtime_cmd_ring[s_runtime_cmd_write_idx] = c;
    s_runtime_cmd_write_idx = (s_runtime_cmd_write_idx + 1) % RUNTIME_CMD_RING_SIZE;
    
    // Check for command on newline
    if (c == '\n' || c == '\r') {
        // Build a linear string from ring buffer (last RUNTIME_CMD_RING_SIZE chars)
        char temp_buffer[RUNTIME_CMD_RING_SIZE + 1];
        for (int i = 0; i < RUNTIME_CMD_RING_SIZE; i++) {
            temp_buffer[i] = s_runtime_cmd_ring[(s_runtime_cmd_write_idx + i) % RUNTIME_CMD_RING_SIZE];
        }
        temp_buffer[RUNTIME_CMD_RING_SIZE] = '\0';
        
        // Look for "tag parsing" pattern in the buffer (case insensitive)
        // Convert to lowercase for matching
        char lower[RUNTIME_CMD_RING_SIZE + 1];
        for (int i = 0; i < RUNTIME_CMD_RING_SIZE; i++) {
            lower[i] = tolower(temp_buffer[i]);
        }
        lower[RUNTIME_CMD_RING_SIZE] = '\0';
        
        // Search for various forms anywhere in buffer: "tag parsing =", "tag_parsing =", "tagparsing ="
        // This allows comment prefixes like: # tag parsing = off
        char* cmd_pos = nullptr;
        if ((cmd_pos = strstr(lower, "tag parsing =")) != nullptr ||
            (cmd_pos = strstr(lower, "tag_parsing =")) != nullptr ||
            (cmd_pos = strstr(lower, "tagparsing =")) != nullptr) {
            
            // Found command pattern! Extract the value after '='
            char* equals = strchr(cmd_pos, '=');
            if (equals != nullptr && (equals - lower) < RUNTIME_CMD_RING_SIZE - 1) {
                char* value = equals + 1;
                
                // Trim leading whitespace
                while (*value == ' ' || *value == '\t') value++;
                
                // Check value (already lowercase)
                bool new_state = false;
                bool valid_command = false;
                
                if (strncmp(value, "on", 2) == 0 || strncmp(value, "1", 1) == 0 ||
                    strncmp(value, "enable", 6) == 0 || strncmp(value, "true", 4) == 0) {
                    new_state = true;
                    valid_command = true;
                } else if (strncmp(value, "off", 3) == 0 || strncmp(value, "0", 1) == 0 ||
                          strncmp(value, "disable", 7) == 0 || strncmp(value, "false", 5) == 0) {
                    new_state = false;
                    valid_command = true;
                }
                
                if (valid_command) {
                    // Update RUNTIME state only (not config file)
                    asyncPassthroughTagParsingEnabled = new_state;
                    
                    // Send confirmation back over UART
                    char response[64];
                    snprintf(response, sizeof(response), "\r\nTag parsing %s\r\n", 
                            new_state ? "enabled" : "disabled");
                    uart_write_blocking(ASYNC_PASSTHROUGH_UART, (const uint8_t*)response, strlen(response));
                }
            }
        }
    }
    
    // Never consume - always forward all characters
    return false;
}

// ----------------------------------------------------------------------------
// Command prefix forwarding to main Serial
// ----------------------------------------------------------------------------
static const char* s_forward_prefixes[ 8 ];
static size_t s_forward_prefix_count = 0;
static size_t s_forward_max_len = 0;
static const char* s_forward_end_tokens[ 8 ];
static size_t s_forward_end_count = 0;
static bool s_forward_end_on_newline = true;
static bool s_forward_active = false;
static unsigned long s_forward_last_byte_us = 0;

static inline bool ring_starts_with( const char* prefix ) {
    const size_t len = strlen( prefix );
    if ( ring_available( ) < len ) return false;
    for ( size_t i = 0; i < len; ++i ) {
        if ( ring_peek_at( (uint16_t)i ) != (uint8_t)prefix[ i ] ) return false;
    }
    return true;
}

static inline void ring_discard_n( size_t n ) {
    for ( size_t i = 0; i < n; ++i ) {
        uint8_t tmp;
        if ( !ring_pop_byte( &tmp ) ) break;
    }
}

static void process_uart_forward_prefixes( ) {
    if ( s_forward_prefix_count == 0 ) return;
    // Try each prefix; on first match, discard prefix and forward until newline or buffer empty
    for ( size_t i = 0; i < s_forward_prefix_count; ++i ) {
        const char* pfx = s_forward_prefixes[ i ];
        if ( !pfx ) continue;
        if ( ring_starts_with( pfx ) ) {
            const size_t plen = strlen( pfx );
            ring_discard_n( plen );
            s_forward_active = true;
            return; // Only process one prefix per task iteration
        }
    }
}

static inline bool is_end_token_seen( uint8_t last_byte ) {
    // Newline handling
    if ( s_forward_end_on_newline && ( last_byte == '\n' || last_byte == '\r' ) ) return true;
    // Token-based
    if ( s_forward_end_count == 0 ) return false;
    for ( size_t i = 0; i < s_forward_end_count; ++i ) {
        const char* tok = s_forward_end_tokens[ i ];
        if ( !tok ) continue;
        const size_t len = strlen( tok );
        if ( len == 0 ) continue;
        if ( ring_available( ) < len ) continue;
        bool match = true;
        for ( size_t j = 0; j < len; ++j ) {
            if ( ring_peek_at( (uint16_t)( ring_available( ) - len + j ) ) != (uint8_t)tok[ j ] ) {
                match = false; break;
            }
        }
        if ( match ) {
         Serial.println("End token seen!");   
        return true;
        }

    }
    return false;
}

// Check if a tag name matches a command tag
static inline bool is_command_tag( const char* tag ) {
    for ( uint8_t i = 0; i < NUM_COMMAND_TAGS; i++ ) {
        if ( strcmp( tag, COMMAND_TAGS[i] ) == 0 ) {
            return true;
        }
    }
    return false;
}

// Process a single byte through the command tag state machine
// Returns true if byte should be forwarded, false if consumed by tag processing
// NOTE: In mode 1 (passthrough + parsing), this ALWAYS returns true to forward all chars
//       while still processing tags for commands. Mode 2 (future) will consume tag chars.
static inline bool process_command_tag_byte( uint8_t c, TagParserState* parser, const char* direction ) {
    bool should_strip_tags = (jumperlessConfig.serial_1.tag_parsing == 2);  // Mode 2 = strip tags
    
    switch ( parser->state ) {
        case TAG_SEARCHING:
            if ( c == '<' ) {
                parser->state = TAG_DETECTING;
                parser->tag_buffer_idx = 0;
                memset( parser->tag_buffer, 0, sizeof(parser->tag_buffer) );
                return !should_strip_tags; // Mode 1: forward, Mode 2: consume
            }
            return true; // Forward normal byte
            
        case TAG_DETECTING:
            if ( c == '>' ) {
                // End of opening tag
                parser->tag_buffer[parser->tag_buffer_idx] = '\0';
                
                // Check if this is a closing tag (starts with '/')
                if ( parser->tag_buffer[0] == '/' ) {
                    // Closing tag
                    if ( strcmp( &parser->tag_buffer[1], parser->current_tag ) == 0 ) {
                        // Valid closing tag for current command
                        // Safety: ensure buffer index is within bounds
                        if (parser->command_buffer_idx >= sizeof(parser->command_buffer)) {
                            parser->command_buffer_idx = sizeof(parser->command_buffer) - 1;
                        }
                        // Null-terminate the accumulated command
                        parser->command_buffer[parser->command_buffer_idx] = '\0';
                        
                        s_command_from_uart = true;
                        
                        // NEW: Use CommandBuffer for synchronous command handling
                        // This replaces the complex Jerial injection buffer system
                        if (parser->command_buffer_idx > 0) {
                            s_injected_commands++;
                            
                            // Determine if this is a <p> (Python) or <j> (raw) command
                            bool isPythonTag = (strcmp(parser->current_tag, "p") == 0);
                            bool accepted = false;
                            
                            if (isPythonTag) {
                                accepted = CommandBuffer::getInstance().setPendingPCommand(parser->command_buffer);
                            } else {
                                accepted = CommandBuffer::getInstance().setPendingJCommand(parser->command_buffer);
                            }
                            
                            if (!accepted) {
                                // Previous command still pending - can't accept new one
                                // Reset state but don't lose the command
                                // In practice this shouldn't happen if main loop processes fast enough
                                parser->state = TAG_SEARCHING;
                                parser->command_buffer_idx = 0;
                                return !should_strip_tags;
                            }
                        }
                        
                        s_last_command_injection_time = millis();
                        
                        // Reset state - just reset indices, no need to memset large buffers
                        parser->state = TAG_SEARCHING;
                        parser->current_tag[0] = '\0';  // Just null-terminate, don't memset
                        parser->command_buffer_idx = 0;  // Just reset index
                        parser->needs_python_prefix = false;
                        parser->seen_first_char = false;
                    } else {
                        // Mismatched closing tag, treat as normal text
                        parser->state = TAG_SEARCHING;
                        parser->command_buffer_idx = 0;  // Just reset index
                    }
                    return !should_strip_tags; // Mode 1: forward, Mode 2: consume
                    
                } else if ( is_command_tag( parser->tag_buffer ) ) {
                    // Valid opening command tag
                    strcpy( parser->current_tag, parser->tag_buffer );
                    parser->state = TAG_IN_COMMAND;
                    
                    // Reset command buffer for new command
                    parser->command_buffer_idx = 0;
                    
                    // For Python tags, prepare to inject '>' prefix if needed
                    if ( strcmp( parser->current_tag, "p" ) == 0 ) {
                        parser->needs_python_prefix = true;
                        parser->seen_first_char = false;
                    }
                    
                    return !should_strip_tags; // Mode 1: forward, Mode 2: consume
                    
                } else {
                    // Not a command tag, forward the whole thing
                    parser->state = TAG_SEARCHING;
                    return true; // Let this '>' through
                }
                
            } else if ( parser->tag_buffer_idx < sizeof(parser->tag_buffer) - 1 ) {
                parser->tag_buffer[parser->tag_buffer_idx++] = c;
                return !should_strip_tags; // Mode 1: forward, Mode 2: consume tag name
                
            } else {
                // Tag buffer overflow, treat as normal text
                parser->state = TAG_SEARCHING;
                return true;
            }
            break;
            
        case TAG_IN_COMMAND:
            if ( c == '<' ) {
                // Possible closing tag
                parser->state = TAG_DETECTING;
                parser->tag_buffer_idx = 0;
                memset( parser->tag_buffer, 0, sizeof(parser->tag_buffer) );
                return !should_strip_tags; // Mode 1: forward, Mode 2: consume
                
            } else {
                // For <p> tags, add '>' prefix only if not already present
                if ( parser->needs_python_prefix && !parser->seen_first_char ) {
                    // Skip whitespace when checking for existing '>'
                    if ( c != ' ' && c != '\t' && c != '\n' && c != '\r' ) {
                        parser->seen_first_char = true;
                        
                        // Only add '>' prefix if the command doesn't already start with it
                        if ( c != '>' && parser->command_buffer_idx < sizeof(parser->command_buffer) - 1 ) {
                            parser->command_buffer[parser->command_buffer_idx++] = '>';
                        }
                        
                        parser->needs_python_prefix = false;
                    }
                }
                
                // NON-BLOCKING: Accumulate character into command buffer
                // The entire command will be injected at once when closing tag is detected
                // This eliminates the need for blocking delayMicroseconds(350) per character
                if ( parser->command_buffer_idx < sizeof(parser->command_buffer) - 1 ) {
                    parser->command_buffer[parser->command_buffer_idx++] = c;
                }
                // If buffer is full, just drop characters (command too long)
                
                return !should_strip_tags; // Mode 1: forward, Mode 2: consume
            }
            break;
    }
    
    return true; // Default: forward
}

static inline void bridge_usb_to_uart( uint8_t itf ) {
    // Drain CDC RX and forward to UART (pico-sdk, HW FIFO)
    // Process through command tag filter
    uint8_t buf[ 64 ];  // Smaller buffer to process in chunks (less blocking)
    uint8_t forward_buf[ 64 ];
    
    // Only process one small chunk per call to avoid hogging CPU
    if ( tud_cdc_n_available( itf ) ) {
        size_t rd = tud_cdc_n_read( itf, buf, sizeof( buf ) );
        if ( rd > 0 ) {
            // Track USB->UART data activity for inactivity timeout
            // CRITICAL: Only track when tag parsing is DISABLED (during flashing)
            // This lets us detect when the Arduino upload finishes (500ms of no data)
            if ( !asyncPassthroughTagParsingEnabled && s_tag_parsing_inactivity_timeout_ms > 0 ) {
                // Tag parsing disabled - track data to detect upload completion
                s_last_usb_to_uart_data_time = millis();
            }
            
            size_t forward_idx = 0;
            
            // // Copy raw data to USBSer1 (CDC 1) for debugging BEFORE tag processing
            // if ( tud_cdc_n_connected( 1 ) ) {
            //     for ( size_t i = 0; i < rd; i++ ) {
            //         tud_cdc_n_write_char( 1, buf[i] );
            //     }
            //     tud_cdc_n_write_flush( 1 );
            // }
            // Process each byte through tag detector (USB→UART)
            for ( size_t i = 0; i < rd; i++ ) {
                // Monitor for runtime control commands (e.g., "tag parsing = on")
                // This passively monitors last 32 chars for commands, never consumes
                process_runtime_command(buf[i]);
                
                if ( asyncPassthroughTagParsingEnabled ) {
                    // Tag parsing enabled - check for command tags
                    if ( process_command_tag_byte( buf[i], &usb_to_uart_parser, "USB→UART" ) ) {
                        // Byte should be forwarded to UART
                        forward_buf[forward_idx++] = buf[i];
                    }
                } else {
                    // Tag parsing disabled - pass through all bytes directly (binary upload)
                    forward_buf[forward_idx++] = buf[i];
                }
            }
            
            // Write accumulated bytes to UART (non-blocking)
            if ( forward_idx > 0 ) {
                // delayMicroseconds( 350 );
                uart_write_blocking( ASYNC_PASSTHROUGH_UART, forward_buf, forward_idx );
                delayMicroseconds( 350 );
            }
        }
    }
}

static inline void bridge_uart_to_usb( uint8_t itf ) {
    // Flush any ring-buffered bytes from UART IRQ to CDC
    if ( !tud_inited( ) ) return;
    if ( !tud_cdc_n_connected( itf ) ) return;
    uint32_t wrote = 0;
    uint32_t avail = tud_cdc_n_write_available( itf );
    uint8_t c;
    
    // CRITICAL: Limit processing to prevent multi-second blocking
    // Process max 128 bytes per call to keep service responsive
    // With 4096-byte ring buffer, processing everything at once can take 5-7 seconds!
    const uint32_t MAX_BYTES_PER_CALL = 2048;
    uint32_t processed = 0;
    
    // If in forwarding mode, stream all bytes to main Serial until end token or timeout
    if ( s_forward_active ) {
        unsigned long forwardStart = micros();
        bool ended = false;
        
        // CRITICAL: Reduce chunk size during forwarding (Serial.write is VERY slow!)
        // Even 128 bytes can take 800-3400ms when writing to Serial!
        const uint32_t MAX_FORWARD_BYTES = 64;  // Smaller chunk for Serial output
        uint32_t forwardCount = 0;
        
        while ( ring_pop_byte( &c ) && forwardCount < MAX_FORWARD_BYTES ) {
            // Serial.write( c );
            s_forward_last_byte_us = micros();
            wrote++;
            processed++;
            forwardCount++;
            // Check for end on newline or token match
            if ( is_end_token_seen( c ) ) { ended = true; break; }
            // Timeout: if idle > 8*usPerByteSerial1, end session
            if ( micros() - s_forward_last_byte_us > ( 8 * usPerByteSerial1 ) ) { ended = true; break; }
        }
        if ( ended ) {
            // Serial.flush();  // REMOVED: Too slow! Data will flush naturally
            s_forward_active = false;
        }
        
        unsigned long forwardTime = micros() - forwardStart;
        #if DEBUG_INJECTED_COMMANDS
        if (forwardTime > 50000) {
            Serial.printf("⏱️  forwarding took %lu ms (%u bytes)\n", forwardTime / 1000, forwardCount);
        }
        #endif
    }
    
    // Process UART->USB data through tag parser (for Arduino-sent commands)
    // Limit to MAX_BYTES_PER_CALL to prevent blocking
    // CRITICAL: Don't check avail in loop condition - drain ring buffer regardless of USB buffer state!
    // We check per-write if there's space, but must keep draining ring to prevent overflow
    while ( ring_pop_byte( &c ) && processed < MAX_BYTES_PER_CALL ) {
        // Monitor for runtime control commands (e.g., "tag parsing = on")
        // This passively monitors last 32 chars for commands, never consumes
        process_runtime_command(c);
        
        // Passthrough all data to CDC 1 (ViperIDE)
        if ( tud_cdc_n_connected( 1 ) && tud_cdc_n_write_available( 1 ) > 0 ) {
            tud_cdc_n_write_char( 1, c );
            wrote++;
        }
        
        // Process command tags if enabled
        if ( asyncPassthroughTagParsingEnabled ) {
            // Tag parsing enabled - check for command tags
            if ( process_command_tag_byte( c, &uart_to_usb_parser, "UART→USB" ) ) {
                // Byte should be forwarded to CDC 0 (main serial - for ViperIDE, etc.)
                // In mode 1 (passthrough + parsing), this returns true for ALL chars
                // In mode 2 (strip tags), this returns false for tag-related chars
                // Check availability per-write to avoid blocking
                // if ( tud_cdc_n_connected( 1 ) && tud_cdc_n_write_available( 1 ) > 0 ) {
                //     tud_cdc_n_write_char( 1, c );
                // }
            }
        } else {
            // Tag parsing disabled - pass through all bytes directly to CDC 0
            // Check availability per-write to avoid blocking
            // if ( tud_cdc_n_connected( 1 ) && tud_cdc_n_write_available( 1) > 0 ) {
            //     tud_cdc_n_write_char( 1, c );
            // }
        }
        processed++;
    }
    
    // Flush USBSer1 (CDC 1) if we wrote to it
    if ( processed > 0 && tud_cdc_n_connected( 1 ) ) {
        tud_cdc_n_write_flush( 1 );
    }
    
    if ( wrote > 0 || s_uart_flush_pending ) {
        unsigned long flushStart = micros();
        tud_cdc_n_write_flush( itf );
        unsigned long flushTime = micros() - flushStart;
        if (flushTime > 50000) {
            Serial.printf("⏱️  tud_cdc_n_write_flush took %lu ms\n", flushTime / 1000);
        }
        
        unsigned long tudStart = micros();
        tud_task(); // service USB to reduce latency under load
        unsigned long tudTime = micros() - tudStart;
        if (tudTime > 50000) {
            Serial.printf("⏱️  tud_task (in bridge) took %lu ms\n", tudTime / 1000);
        }
    }
    s_uart_flush_pending = ( uartReceivedTail != uartReceivedHead );
}

// ISR-safe flags updated by TinyUSB callbacks, processed in main context
static volatile bool s_usb_rx_pending = false;
static volatile bool s_apply_line_coding_pending = false;
volatile bool s_line_coding_override = false;

static cdc_line_coding_t s_line_coding = { .bit_rate = ASYNC_PASSTHROUGH_UART_DEFAULT_BAUD, .stop_bits = 1, .parity = 0, .data_bits = 8 };

// ----------------------------------------------------------------------------
// TinyUSB CDC Callbacks (C linkage)
// ----------------------------------------------------------------------------

extern "C" {

void tud_cdc_rx_cb( uint8_t itf ) {
    // Mark pending work; do actual bridging in main context to avoid ISR reentrancy
    if ( itf == ASYNC_PASSTHROUGH_CDC_ITF ) {
        s_usb_rx_pending = true;
    }
}

// Cache new line coding and apply it in main context
void tud_cdc_line_coding_cb( uint8_t itf, cdc_line_coding_t const* p ) {
    if ( itf != ASYNC_PASSTHROUGH_CDC_ITF )
        return;
    s_line_coding = *p;
    s_apply_line_coding_pending = true;
}
}

// ----------------------------------------------------------------------------
// External functions from ArduinoStuff.cpp
// ----------------------------------------------------------------------------
extern void SetArduinoResetLine(bool state, int topBottomBoth);

// ----------------------------------------------------------------------------
// Foreground task runner
// ----------------------------------------------------------------------------

namespace AsyncPassthrough {

void begin( unsigned long baud ) {
    // Configure UART pins and UART with HW FIFO enabled
    gpio_set_function( ASYNC_PASSTHROUGH_UART_TX_PIN, GPIO_FUNC_UART );
    gpio_set_function( ASYNC_PASSTHROUGH_UART_RX_PIN, GPIO_FUNC_UART );

    uart_init( ASYNC_PASSTHROUGH_UART, baud );
    uart_set_format( ASYNC_PASSTHROUGH_UART, 8, 1, UART_PARITY_NONE );
    uart_set_fifo_enabled( ASYNC_PASSTHROUGH_UART, true );
    
    // =========================================================================
    // AGGRESSIVE STARTUP RESYNC SEQUENCE
    // =========================================================================
    // On boot, the Arduino might already be transmitting, or there could be
    // noise on the line. This can cause the UART to start sampling mid-byte,
    // leading to persistent framing errors and crashes.
    // 
    // This aggressive sequence does MULTIPLE resync attempts to ensure both
    // sides start clean, even in worst-case scenarios.
    // =========================================================================
    
    uart_hw_t* hw = uart_get_hw( ASYNC_PASSTHROUGH_UART );
    
    // Do multiple resync cycles to handle worst-case scenarios
    // Each cycle: drain FIFO, disable RX, send break, wait, drain again
    for (int resync_attempt = 0; resync_attempt < 3; resync_attempt++) {
        
        // Serial.println("Resync attempt " + String(resync_attempt));
        // Serial.flush();
        // Step 1: Drain any garbage from RX FIFO (don't process, just discard)
        int drained = 0;
        while ( !(hw->fr & UART_UARTFR_RXFE_BITS) && drained < 1000 ) {
            volatile uint32_t discard = hw->dr;  // Read and discard
            (void)discard;
            drained++;
        }
        
        // Step 2: Clear any error flags that accumulated
        hw->rsr = 0xFFFFFFFFu;
        
        // Step 3: Disable receiver to clear shift register state
        hw_clear_bits( &hw->cr, UART_UARTCR_RXE_BITS );
        busy_wait_us( 100 );  // Give it time to fully stop
        
        // Step 4: Send break condition to force Arduino's UART to resync
        // Wait for TX FIFO to be empty first
        int tx_wait = 0;
        while ( !(hw->fr & UART_UARTFR_TXFE_BITS) && tx_wait < 10000 ) {
            busy_wait_us(1);
            tx_wait++;
        }
        hw_set_bits( &hw->lcr_h, UART_UARTLCR_H_BRK_BITS );  // Start break (TX LOW)
        busy_wait_us( 500 );  // Hold break for ~50 bit times at 115200 (longer!)
        hw_clear_bits( &hw->lcr_h, UART_UARTLCR_H_BRK_BITS );  // End break
        
        // Step 5: Wait for line to stabilize and any echo/response to clear
        busy_wait_us( 1000 );  // ~100 bit times - longer wait
        
        // Step 6: Drain any bytes that arrived during break/stabilization
        drained = 0;
        while ( !(hw->fr & UART_UARTFR_RXFE_BITS) && drained < 1000 ) {
            volatile uint32_t discard = hw->dr;
            (void)discard;
            drained++;
        }
        hw->rsr = 0xFFFFFFFFu;  // Clear errors again
        
        // Step 7: Re-enable receiver
        hw_set_bits( &hw->cr, UART_UARTCR_RXE_BITS );
        
        // Brief pause before next attempt (if any)
        if (resync_attempt < 2) {
            busy_wait_us( 500 );
        }
    }
    
    // Final wait to ensure line is truly idle
    busy_wait_us( 500 );  // 2ms final stabilization
    
    // One more drain in case anything came in during final wait
    while ( !(hw->fr & UART_UARTFR_RXFE_BITS) ) {
        volatile uint32_t discard = hw->dr;
        (void)discard;
    }
    hw->rsr = 0xFFFFFFFFu;
    
    // Reset all timing/error stats for clean start
    s_uart_framing_error_count = 0;
    s_uart_framing_error_total = 0;
    s_uart_overrun_count = 0;
    s_uart_resync_count = 0;
    s_uart_garbage_count = 0;
    s_uart_garbage_total = 0;
    s_uart_garbage_resyncs = 0;
    s_timing_anomaly_count = 0;
    s_idle_periods_detected = 0;
    s_bytes_since_last_idle = 0;
    s_last_rx_byte_time_us = 0;
    s_line_was_idle = true;  // Fresh start = synced
    
    // Clear ring buffer too (should already be empty but be sure)
    // Use global scope :: because these are file-scope variables, not namespace members
    ::uartReceivedHead = 0;
    ::uartReceivedTail = 0;
    
    // =========================================================================
    // END STARTUP RESYNC SEQUENCE
    // =========================================================================

    // Enable RX interrupt for immediate forwarding; include RX timeout interrupt
//#if JL_UART0_INTEROP_MODE == 0
    // Integrate: use shared handler so MicroPython and passthrough can coexist
    irq_set_exclusive_handler( ASYNC_PASSTHROUGH_UART_IRQ, async_uart_irq_handler );
    irq_set_priority( ASYNC_PASSTHROUGH_UART_IRQ, 1 );
    irq_set_enabled( ASYNC_PASSTHROUGH_UART_IRQ, true );
// #else
//     // Preempt (default): exclusive handler while not suspended by MicroPython
//     irq_set_exclusive_handler( ASYNC_PASSTHROUGH_UART_IRQ, async_uart_irq_handler );
//     // Priority 64 (was 0) - allow main loop to get CPU time during fast command streams
//     irq_set_priority( ASYNC_PASSTHROUGH_UART_IRQ, 64 );
//     irq_set_enabled( ASYNC_PASSTHROUGH_UART_IRQ, true );
// #endif
    uart_set_irq_enables( ASYNC_PASSTHROUGH_UART, true, false );
    // Ensure RX timeout and overrun interrupts are enabled
    hw_set_bits( &uart_get_hw( ASYNC_PASSTHROUGH_UART )->imsc, UART_UARTIMSC_RTIM_BITS | UART_UARTIMSC_OEIM_BITS );
    // Set RX IRQ trigger at 1/8 FIFO, TX at 1/2 FIFO
    hw_write_masked( &uart_get_hw( ASYNC_PASSTHROUGH_UART )->ifls,
                     ( 0u << UART_UARTIFLS_RXIFLSEL_LSB ) | ( 2u << UART_UARTIFLS_TXIFLSEL_LSB ),
                     UART_UARTIFLS_RXIFLSEL_BITS | UART_UARTIFLS_TXIFLSEL_BITS );

    // Track initial micros per byte based on default config
    s_line_coding.bit_rate = (uint32_t)baud;
    s_line_coding.data_bits = 8;
    s_line_coding.parity = 0;
    s_line_coding.stop_bits = 0; // 1 stop
    set_micros_per_byte( &s_line_coding );
    
    // Initialize tag parsing state from config
    // 0 = disabled, 1 = enabled, 2 = strip tags (future)
    asyncPassthroughTagParsingEnabled = (jumperlessConfig.serial_1.tag_parsing > 0);

    // // Register default forward prefixes
    // registerForwardPrefix( "jcommand:" );
    // registerForwardPrefix( "\x02" ); // SOH
    // registerForwardPrefix( "\x03" ); // DLE
    // registerForwardPrefix( "jl:" );
}

void task( ) {
    // DEBUG DISABLED: All checkpoint output removed to minimize USB pressure
    // The A12345678 markers were contributing to freeze by adding USB load
    static uint32_t taskCount = 0;
    taskCount++;
    
    // Checkpoints disabled for production
    bool printCheckpoints = false;  // Was: (taskCount % 1000 == 0)
    
    unsigned long taskStart = micros();
    unsigned long t0, t1;
    
    // CRITICAL: Check DTR state FIRST, before any data processing
    // This ensures DTR pulse detection takes absolute priority over command injection
    // USBSer1 is declared extern at file scope (line 21), outside the namespace
    t0 = micros();
    
    if (printCheckpoints) { Serial.write('1'); tud_task(); }
    
    checkDTRState( USBSer1 );
    
    if (printCheckpoints) { Serial.write('2'); tud_task(); }
    t1 = micros();
    #if DEBUG_INJECTED_COMMANDS
    if ((t1 - t0) > 50000) {  // > 50ms
        Serial.printf("⏱️  checkDTRState took %lu ms\n", (t1 - t0) / 1000);
        Serial.flush();
    }
    #endif
    
    // Check if tag parsing should be re-enabled
    if ( !asyncPassthroughTagParsingEnabled ) {
        bool should_reenable = false;
        const char* reenable_reason = nullptr;
        
        // Check absolute timeout (e.g., 10 seconds after disable) - safety fallback
        if ( s_tag_parsing_timeout_ms > 0 ) {
            uint32_t elapsed = millis() - s_tag_parsing_disabled_time;
            if ( elapsed >= s_tag_parsing_timeout_ms ) {
                should_reenable = true;
                reenable_reason = "absolute timeout (10s safety)";
            }
        }
        
        // Check inactivity timeout (e.g., 2000ms after last USB->UART data)
        // This detects when Arduino upload has finished
        // Increased from 500ms to 2000ms to handle normal pauses during uploads
        if ( !should_reenable && s_tag_parsing_inactivity_timeout_ms > 0 && s_last_usb_to_uart_data_time > 0 ) {
            uint32_t inactivity = millis() - s_last_usb_to_uart_data_time;
            if ( inactivity >= s_tag_parsing_inactivity_timeout_ms ) {
                should_reenable = true;
                reenable_reason = "upload complete (2s idle)";
            }
        }
        
        if ( should_reenable ) {
            asyncPassthroughTagParsingEnabled = true;
            s_tag_parsing_timeout_ms = 0;  // Clear timeouts
            s_tag_parsing_inactivity_timeout_ms = 0;
            s_last_usb_to_uart_data_time = 0;
            
            // Single clean message when re-enabling (no flush - too slow!)
            #if DEBUG_INJECTED_COMMANDS
            Serial.printf("✓ Tag parsing re-enabled: %s\n", reenable_reason);
            #endif
        }
    }
    
    if (printCheckpoints) { Serial.write('3'); tud_task(); }
    
    // If suspended by MicroPython, avoid touching UART hardware
    if ( s_uart_suspended_by_mpy ) {
        tud_task();
        checkDTRState( USBSer1 );
        return;
    }
    
    if (printCheckpoints) { Serial.write('4'); tud_task(); }
    
    // Apply pending line coding from host
    t0 = micros();
    if ( s_apply_line_coding_pending && s_line_coding_override == false ) {
        // Apply to pico-sdk UART
        uint data_bits = 8;
        switch ( s_line_coding.data_bits ) {
        case 5: data_bits = 5; break;
        case 6: data_bits = 6; break;
        case 7: data_bits = 7; break;
        default: data_bits = 8; break;
        }

        uart_parity_t parity = UART_PARITY_NONE;
        if ( s_line_coding.parity == 1 ) {
            parity = UART_PARITY_ODD;
        } else if ( s_line_coding.parity == 2 ) {
            parity = UART_PARITY_EVEN;
        } else {
            parity = UART_PARITY_NONE;
        }

        uint stop_bits = 1;
        if ( s_line_coding.stop_bits == 2 ) {
            stop_bits = 2;
        } else if ( s_line_coding.stop_bits == 1 ) {
            // 1.5 not supported; approximate with 2
            stop_bits = 2;
        } else {
            stop_bits = 1;
        }
       
        uart_set_baudrate( ASYNC_PASSTHROUGH_UART, s_line_coding.bit_rate );
        uart_set_format( ASYNC_PASSTHROUGH_UART, data_bits, stop_bits, parity );
        
        serial1baud = s_line_coding.bit_rate;
        set_micros_per_byte( &s_line_coding );
        s_apply_line_coding_pending = false;
        
        t1 = micros();
        #if DEBUG_INJECTED_COMMANDS
        if ((t1 - t0) > 50000) {
            Serial.printf("⏱️  line_coding took %lu ms\n", (t1 - t0) / 1000);
        }
        #endif
    }


    if (printCheckpoints) { Serial.write('5'); tud_task(); }
    
    // USB -> UART when either pending flag set or data available
    t0 = micros();
    if ( s_usb_rx_pending || ( tud_inited( ) && tud_cdc_n_available( ASYNC_PASSTHROUGH_CDC_ITF ) ) ) {
        bridge_usb_to_uart( ASYNC_PASSTHROUGH_CDC_ITF );
        s_usb_rx_pending = false;
    }
    
    if (printCheckpoints) { Serial.write('6'); tud_task(); }
    t1 = micros();
    #if DEBUG_INJECTED_COMMANDS
    if ((t1 - t0) > 50000) {
        Serial.printf("⏱️  bridge_usb_to_uart took %lu ms\n", (t1 - t0) / 1000);
    }
    #endif
    
    // UART -> USB (where tag parsing and command injection happen)
    t0 = micros();
    bridge_uart_to_usb( ASYNC_PASSTHROUGH_CDC_ITF );
    t1 = micros();
    
    if (printCheckpoints) { Serial.write('7'); tud_task(); }  // After UART->USB bridge
    
    #if DEBUG_INJECTED_COMMANDS
    if ((t1 - t0) > 50000) {
        Serial.printf("⏱️  bridge_uart_to_usb took %lu ms\n", (t1 - t0) / 1000);
    }
    #endif

    // Check if uartReceived starts with any forward prefix and route to main Serial
    t0 = micros();
    process_uart_forward_prefixes();
    t1 = micros();
    #if DEBUG_INJECTED_COMMANDS
    if ((t1 - t0) > 50000) {
        Serial.printf("⏱️  process_uart_forward_prefixes took %lu ms\n", (t1 - t0) / 1000);
    }
    #endif

    // Service USB stack regardless to minimize latency and prevent CDC TX stalling
    t0 = micros();
    tud_task();
    t1 = micros();
    #if DEBUG_INJECTED_COMMANDS
    if ((t1 - t0) > 50000) {
        Serial.printf("⏱️  tud_task took %lu ms\n", (t1 - t0) / 1000);
    }
    #endif
    
    // Send any pending UART responses (responses to commands from Arduino)
    sendPendingUARTResponses();
    
    if (printCheckpoints) { Serial.write('8'); Serial.write('\n'); }  // Task completed
    
    unsigned long taskEnd = micros();
    #if DEBUG_INJECTED_COMMANDS
    if ((taskEnd - taskStart) > 100000) {  // > 100ms total
        Serial.printf("⏱️  AsyncPassthrough::task() TOTAL: %lu ms\n", (taskEnd - taskStart) / 1000);
    }
    #endif
}

bool registerForwardPrefix( const char* prefix ) {
    if ( !prefix || !*prefix ) return false;
    if ( s_forward_prefix_count >= ( sizeof( s_forward_prefixes ) / sizeof( s_forward_prefixes[ 0 ] ) ) ) return false;
    s_forward_prefixes[ s_forward_prefix_count++ ] = prefix;
    const size_t len = strlen( prefix );
    if ( len > s_forward_max_len ) s_forward_max_len = len;
    return true;
}

bool unregisterForwardPrefix( const char* prefix ) {
    if ( !prefix ) return false;
    for ( size_t i = 0; i < s_forward_prefix_count; ++i ) {
        if ( s_forward_prefixes[ i ] && strcmp( s_forward_prefixes[ i ], prefix ) == 0 ) {
            // Compact array
            for ( size_t j = i + 1; j < s_forward_prefix_count; ++j ) {
                s_forward_prefixes[ j - 1 ] = s_forward_prefixes[ j ];
            }
            s_forward_prefix_count--;
            s_forward_prefixes[ s_forward_prefix_count ] = nullptr;
            // Recompute max len
            s_forward_max_len = 0;
            for ( size_t k = 0; k < s_forward_prefix_count; ++k ) {
                size_t l = strlen( s_forward_prefixes[ k ] );
                if ( l > s_forward_max_len ) s_forward_max_len = l;
            }
            return true;
        }
    }
    return false;
}

size_t listForwardPrefixes( const char** out, size_t max ) {
    size_t n = ( s_forward_prefix_count < max ) ? s_forward_prefix_count : max;
    for ( size_t i = 0; i < n; ++i ) out[ i ] = s_forward_prefixes[ i ];
    return s_forward_prefix_count;
}

bool registerForwardEnd( const char* token ) {
    if ( !token ) return false;
    if ( s_forward_end_count >= ( sizeof( s_forward_end_tokens ) / sizeof( s_forward_end_tokens[ 0 ] ) ) ) return false;
    s_forward_end_tokens[ s_forward_end_count++ ] = token;
    return true;
}

bool unregisterForwardEnd( const char* token ) {
    if ( !token ) return false;
    for ( size_t i = 0; i < s_forward_end_count; ++i ) {
        if ( s_forward_end_tokens[ i ] && strcmp( s_forward_end_tokens[ i ], token ) == 0 ) {
            for ( size_t j = i + 1; j < s_forward_end_count; ++j ) {
                s_forward_end_tokens[ j - 1 ] = s_forward_end_tokens[ j ];
            }
            s_forward_end_count--;
            s_forward_end_tokens[ s_forward_end_count ] = nullptr;
            return true;
        }
    }
    return false;
}

size_t listForwardEnds( const char** out, size_t max ) {
    size_t n = ( s_forward_end_count < max ) ? s_forward_end_count : max;
    for ( size_t i = 0; i < n; ++i ) out[ i ] = s_forward_end_tokens[ i ];
    return s_forward_end_count;
}

void setForwardEndOnNewline( bool enable ) {
    s_forward_end_on_newline = enable;
}

void setTagParsingEnabled( bool enable ) {
    if ( !enable && asyncPassthroughTagParsingEnabled ) {
        // Tag parsing is being disabled - record the time
        s_tag_parsing_disabled_time = millis();
    }
    
    asyncPassthroughTagParsingEnabled = enable;
    
    // If re-enabling, clear any pending timeouts
    if ( enable ) {
        s_tag_parsing_timeout_ms = 0;
        s_tag_parsing_inactivity_timeout_ms = 0;
        s_last_usb_to_uart_data_time = 0;
    }
}

void disableTagParsingWithTimeout( uint32_t timeout_ms ) {
    asyncPassthroughTagParsingEnabled = false;
    s_tag_parsing_disabled_time = millis();
    s_tag_parsing_timeout_ms = timeout_ms;
    s_tag_parsing_inactivity_timeout_ms = 0;  // Clear inactivity timeout when using absolute timeout
    s_last_usb_to_uart_data_time = 0;
}

void disableTagParsingWithInactivityTimeout( uint32_t absolute_timeout_ms, uint32_t inactivity_timeout_ms ) {
    // If already disabled with an inactivity timeout active, don't reset the state
    // This prevents multiple DTR pulses or calls from breaking the inactivity tracking
    if ( !asyncPassthroughTagParsingEnabled && s_tag_parsing_inactivity_timeout_ms > 0 ) {
        // Already disabled and tracking - preserve existing state
        // Don't print anything here to avoid spam during flashing
        return;
    }
    
    asyncPassthroughTagParsingEnabled = false;
    s_tag_parsing_disabled_time = millis();
    s_tag_parsing_timeout_ms = absolute_timeout_ms;
    s_tag_parsing_inactivity_timeout_ms = inactivity_timeout_ms;
    s_last_usb_to_uart_data_time = 0;  // Will be set when first data arrives
}

bool getTagParsingEnabled() {
    return asyncPassthroughTagParsingEnabled;
}

// ============================================================================
// DTR Pulse Detection and Arduino Reset
// ============================================================================

// DTR state tracking
static bool s_dtr_state[3] = { false, false, false };
static bool s_dtr_pulse_detected = false;

void checkDTRState(Adafruit_USBD_CDC& cdc) {
    bool current_dtr = cdc.dtr();
    
    // Shift the array to track state changes
    if (current_dtr != s_dtr_state[2]) {
        s_dtr_state[0] = s_dtr_state[1];
        s_dtr_state[1] = s_dtr_state[2];
        s_dtr_state[2] = current_dtr;
        
        // Detect pulses going either direction (some things invert the DTR line)
        bool pulse_detected = false;
        if (s_dtr_state[1] == 1 && s_dtr_state[2] == 0) {
            pulse_detected = true;  // high-to-low pulse
        } else if (s_dtr_state[1] == 0 && s_dtr_state[2] == 1) {
            pulse_detected = true;  // low-to-high pulse
        }
        
        if (pulse_detected && millis() > 3000) {  // Ignore pulses during boot
            s_dtr_pulse_detected = true;
            
            // CRITICAL: Disable tag parsing IMMEDIATELY
            if (asyncPassthroughEnabled && asyncPassthroughTagParsingEnabled) {
                disableTagParsingWithInactivityTimeout(5000, 2000);
            }
            
            // CRITICAL: Reset Arduino IMMEDIATELY - don't wait for main loop!
            // This matches standard Arduino auto-reset timing
            SetArduinoResetLine(LOW, 2);   // Assert reset (both top and bottom)
            delayMicroseconds(5000);       // Hold reset for 5ms
            SetArduinoResetLine(HIGH, 2);  // Release reset
            
            // Small delay to let bootloader start before data arrives
           // delayMicroseconds(50000);  // 50ms for bootloader to initialize
        }
    }
}

bool wasDTRPulseDetected() {
    return s_dtr_pulse_detected;
}

void clearDTRPulse() {
    s_dtr_pulse_detected = false;
}

void resetArduino(int resetPin) {
    // Pull reset line low for 3ms, then high
    pinMode(resetPin, OUTPUT);
    digitalWrite(resetPin, LOW);
    delay(5);
    digitalWrite(resetPin, HIGH);
    pinMode(resetPin, INPUT);
}

// ============================================================================
// UART Response Functions - Send command responses back to Arduino
// ============================================================================

bool queueUARTResponse(const char* data, size_t len) {
    if (!data || len == 0) return false;
    if (len > UART_RESPONSE_MAX_LEN - 1) len = UART_RESPONSE_MAX_LEN - 1;
    
    if (s_uart_response_count >= UART_RESPONSE_QUEUE_SIZE) {
        // Queue full - drop oldest
        s_uart_response_tail = (s_uart_response_tail + 1) % UART_RESPONSE_QUEUE_SIZE;
        s_uart_response_count--;
    }
    
    UARTResponseEntry* entry = &s_uart_response_queue[s_uart_response_head];
    memcpy(entry->data, data, len);
    entry->data[len] = '\0';
    entry->length = len;
    entry->pending = true;
    
    s_uart_response_head = (s_uart_response_head + 1) % UART_RESPONSE_QUEUE_SIZE;
    s_uart_response_count++;
    
    return true;
}

bool queueUARTResponse(const String& data) {
    return queueUARTResponse(data.c_str(), data.length());
}

void sendPendingUARTResponses() {
    // CRITICAL: Non-blocking UART transmission
    // At 115200 baud, each byte takes ~87us. Limit bytes per call to prevent blocking.
    const uint32_t MAX_BYTES_PER_CALL = 64;  // ~5.5ms max per call
    uint32_t bytesSent = 0;
    
    // First, drain the legacy queue (for backwards compatibility)
    while (s_uart_response_count > 0 && bytesSent < MAX_BYTES_PER_CALL) {
        UARTResponseEntry* entry = &s_uart_response_queue[s_uart_response_tail];
        if (entry->pending && entry->length > 0) {
            // Write bytes one at a time, checking FIFO space
            while (entry->length > 0 && bytesSent < MAX_BYTES_PER_CALL) {
                if (uart_is_writable(ASYNC_PASSTHROUGH_UART)) {
                    uart_putc_raw(ASYNC_PASSTHROUGH_UART, entry->data[0]);
                    // Shift data (inefficient but simple for legacy queue)
                    memmove(entry->data, entry->data + 1, entry->length - 1);
                    entry->length--;
                    bytesSent++;
                } else {
                    // UART FIFO full, try again next call
                    return;
                }
            }
            if (entry->length == 0) {
                entry->pending = false;
            }
        }
        if (!entry->pending) {
            s_uart_response_tail = (s_uart_response_tail + 1) % UART_RESPONSE_QUEUE_SIZE;
            s_uart_response_count--;
        }
    }
    
    // Second, drain the CommandBuffer outgoing buffer (new system)
    CommandBuffer& cb = CommandBuffer::getInstance();
    while (cb.hasOutgoingData() && bytesSent < MAX_BYTES_PER_CALL) {
        if (uart_is_writable(ASYNC_PASSTHROUGH_UART)) {
            int byte = cb.getOutgoingByte();
            if (byte >= 0) {
                uart_putc_raw(ASYNC_PASSTHROUGH_UART, (uint8_t)byte);
                bytesSent++;
            }
        } else {
            // UART FIFO full, try again next call
            return;
        }
    }
}

bool wasCommandFromUART() {
    return s_command_from_uart;
}

void clearCommandFromUARTFlag() {
    s_command_from_uart = false;
}

// ============================================================================
// UART Framing Error Detection and Resync - Public API
// ============================================================================

void getUARTErrorStats(uint32_t* framing_errors, uint32_t* overruns, uint32_t* resyncs) {
    if (framing_errors) *framing_errors = s_uart_framing_error_total;
    if (overruns) *overruns = s_uart_overrun_count;
    if (resyncs) *resyncs = s_uart_resync_count;
}

void resetUARTErrorStats() {
    s_uart_framing_error_total = 0;
    s_uart_framing_error_count = 0;
    s_uart_overrun_count = 0;
    s_uart_resync_count = 0;
    s_uart_garbage_count = 0;
    s_uart_garbage_total = 0;
    s_uart_garbage_resyncs = 0;
}

void forceUARTResync() {
    // Call the internal resync function
    uart_force_receiver_resync();
}

void printUARTErrorStats() {
    Serial.printf("UART Error Stats:\n");
    Serial.printf("  Framing errors: %lu (consecutive: %lu)\n", 
                  s_uart_framing_error_total, s_uart_framing_error_count);
    Serial.printf("  Overrun errors: %lu\n", s_uart_overrun_count);
    Serial.printf("  Receiver resyncs: %lu\n", s_uart_resync_count);
    Serial.printf("  Ring buffer overflow: %lu\n", uartReceivedOverflowCount);
}

void sendBreakToRemote(uint32_t break_duration_us) {
    uart_hw_t* hw = uart_get_hw(ASYNC_PASSTHROUGH_UART);
    
    // Wait for TX FIFO to drain before sending break
    // This ensures we don't corrupt any pending data
    while (!(hw->fr & UART_UARTFR_TXFE_BITS)) {
        // TX FIFO not empty, wait
        tight_loop_contents();
    }
    
    // Set BRK bit in Line Control Register to force TX LOW
    // This holds the TX line in the "space" (LOW) state
    hw_set_bits(&hw->lcr_h, UART_UARTLCR_H_BRK_BITS);
    
    // Hold break for specified duration
    // At 115200 baud, one bit time is ~8.7µs
    // A frame is ~10-11 bits, so ~87-96µs minimum for a valid break
    // We default to 100µs which gives a bit of margin
    busy_wait_us(break_duration_us);
    
    // Clear BRK bit to return TX to normal (idle HIGH)
    hw_clear_bits(&hw->lcr_h, UART_UARTLCR_H_BRK_BITS);
    
    // Small delay to let the remote receiver stabilize
    busy_wait_us(10);
}

void fullBidirectionalResync() {
    // Step 1: Resync our local receiver
    forceUARTResync();
    
    // Step 2: Send break to force remote receiver to resync
    // Use longer break (200µs) for better reliability
    sendBreakToRemote(200);
    
    // Step 3: Brief pause to let both sides stabilize
    busy_wait_us(50);
    
    Serial.println("Full bidirectional UART resync completed");
}

// ============================================================================
// Idle Line Detection and Timing Validation - Public API
// ============================================================================

bool isLineIdle() {
    if (s_last_rx_byte_time_us == 0) return true;  // Never received = idle
    uint32_t now = time_us_32();
    uint32_t elapsed = now - s_last_rx_byte_time_us;
    return elapsed > UART_IDLE_THRESHOLD_US;
}

uint32_t getTimeSinceLastRxUs() {
    if (s_last_rx_byte_time_us == 0) return UINT32_MAX;
    return time_us_32() - s_last_rx_byte_time_us;
}

void getTimingStats(uint32_t* idle_periods, uint32_t* bytes_since_idle, 
                    uint32_t* timing_anomalies, uint32_t* last_inter_byte_us) {
    if (idle_periods) *idle_periods = s_idle_periods_detected;
    if (bytes_since_idle) *bytes_since_idle = s_bytes_since_last_idle;
    if (timing_anomalies) *timing_anomalies = s_timing_anomaly_count;
    if (last_inter_byte_us) *last_inter_byte_us = s_last_inter_byte_time_us;
}

void resetTimingStats() {
    s_idle_periods_detected = 0;
    s_bytes_since_last_idle = 0;
    s_timing_anomaly_count = 0;
    s_last_inter_byte_time_us = 0;
    s_last_rx_byte_time_us = 0;
    s_line_was_idle = true;
}

void printFullDiagnostics() {
    Serial.println("=== UART Full Diagnostics ===");
    
    // Error stats
    Serial.println("Error Statistics:");
    Serial.printf("  Framing errors: %lu (consecutive: %lu)\n", 
                  s_uart_framing_error_total, s_uart_framing_error_count);
    Serial.printf("  Garbage bytes: %lu (consecutive: %lu)\n",
                  s_uart_garbage_total, s_uart_garbage_count);
    Serial.printf("  Overrun errors: %lu\n", s_uart_overrun_count);
    Serial.printf("  Receiver resyncs: %lu (garbage-triggered: %lu)\n", 
                  s_uart_resync_count, s_uart_garbage_resyncs);
    Serial.printf("  Ring buffer overflow: %lu\n", uartReceivedOverflowCount);
    
    // Timing stats
    Serial.println("Timing Statistics:");
    Serial.printf("  Idle periods detected: %lu\n", s_idle_periods_detected);
    Serial.printf("  Bytes since last idle: %lu\n", s_bytes_since_last_idle);
    Serial.printf("  Timing anomalies: %lu\n", s_timing_anomaly_count);
    Serial.printf("  Last inter-byte time: %lu us\n", s_last_inter_byte_time_us);
    Serial.printf("  Expected byte time: %lu us\n", usPerByteSerial1);
    
    // Current state
    Serial.println("Current State:");
    Serial.printf("  Line idle: %s\n", isLineIdle() ? "YES" : "NO");
    Serial.printf("  Time since last RX: %lu us\n", getTimeSinceLastRxUs());
    Serial.printf("  Baud rate: %lu\n", serial1baud);
    Serial.printf("  Ring buffer: %u bytes pending\n", ring_available());
    
    Serial.println("=============================");
}

bool waitForLineIdle(uint32_t timeout_ms) {
    uint32_t start = millis();
    while (millis() - start < timeout_ms) {
        if (isLineIdle()) {
            return true;
        }
        delayMicroseconds(10);
    }
    return false;
}

} // namespace AsyncPassthrough

// ----------------------------------------------------------------------------
// Interop hooks for MicroPython ownership of UART0
// ----------------------------------------------------------------------------

// Track suspend state to avoid redundant reconfiguration
// (defined above to gate task() too)

extern "C" void jl_asyncpassthrough_suspend_uart0( void ) {
// #if JL_UART0_INTEROP_MODE == 1
//     if ( s_uart_suspended_by_mpy ) return;
//     // Disable our IRQs and release exclusive handler so shared handlers can be installed safely
//     irq_set_enabled( ASYNC_PASSTHROUGH_UART_IRQ, false );
//     uart_set_irq_enables( ASYNC_PASSTHROUGH_UART, false, false );
//     // Disable RX timeout and overrun interrupts we enabled
//     hw_clear_bits( &uart_get_hw( ASYNC_PASSTHROUGH_UART )->imsc, UART_UARTIMSC_RTIM_BITS | UART_UARTIMSC_OEIM_BITS );
//     // Release exclusive handler slot (set to null)
//     irq_set_exclusive_handler( ASYNC_PASSTHROUGH_UART_IRQ, 0 );
//     s_uart_suspended_by_mpy = true;
// #else
//     (void)s_uart_suspended_by_mpy; // unused
// #endif
}

extern "C" void jl_asyncpassthrough_resume_uart0( void ) {
// #if JL_UART0_INTEROP_MODE == 1
//     if ( !s_uart_suspended_by_mpy ) return;
//     // Reconfigure UART hardware and restore our IRQ configuration
//     gpio_set_function( ASYNC_PASSTHROUGH_UART_TX_PIN, GPIO_FUNC_UART );
//     gpio_set_function( ASYNC_PASSTHROUGH_UART_RX_PIN, GPIO_FUNC_UART );

//     uart_init( ASYNC_PASSTHROUGH_UART, serial1baud );
//     uart_set_format( ASYNC_PASSTHROUGH_UART, 8, 1, UART_PARITY_NONE );
//     uart_set_fifo_enabled( ASYNC_PASSTHROUGH_UART, true );

//     irq_set_exclusive_handler( ASYNC_PASSTHROUGH_UART_IRQ, async_uart_irq_handler );
//     // Priority 64 (was 0) - allow main loop to get CPU time during fast command streams
//     irq_set_priority( ASYNC_PASSTHROUGH_UART_IRQ, 64 );
//     irq_set_enabled( ASYNC_PASSTHROUGH_UART_IRQ, true );
//     uart_set_irq_enables( ASYNC_PASSTHROUGH_UART, true, false );
//     hw_set_bits( &uart_get_hw( ASYNC_PASSTHROUGH_UART )->imsc, UART_UARTIMSC_RTIM_BITS | UART_UARTIMSC_OEIM_BITS );
//     hw_write_masked( &uart_get_hw( ASYNC_PASSTHROUGH_UART )->ifls,
//                      ( 0u << UART_UARTIFLS_RXIFLSEL_LSB ) | ( 2u << UART_UARTIFLS_TXIFLSEL_LSB ),
//                      UART_UARTIFLS_RXIFLSEL_BITS | UART_UARTIFLS_TXIFLSEL_BITS );
//     s_uart_suspended_by_mpy = false;
// #endif
}

// Expose an override for MicroPython to update UART0 line coding
extern "C" void jl_asyncpassthrough_override_line_coding( uint32_t baud, uint8_t data_bits, uint8_t parity, uint8_t stop_bits ) {
    // Map to pico-sdk settings and apply immediately

    // Serial.println("jl_asyncpassthrough_override_line_coding");
    // Serial.println(baud);
    // Serial.println(data_bits);
    // Serial.println(parity);
    // Serial.println(stop_bits);

    uint d = ( data_bits < 5 ? 5 : ( data_bits > 8 ? 8 : data_bits ) );
    uart_parity_t p = UART_PARITY_NONE;
    if ( parity == 1 ) p = UART_PARITY_ODD; else if ( parity == 2 ) p = UART_PARITY_EVEN;
    uint s = ( stop_bits >= 2 ? 2 : 1 );

    if ( baud > 0 ) {
        uart_set_baudrate( ASYNC_PASSTHROUGH_UART, baud );
    }
    uart_set_format( ASYNC_PASSTHROUGH_UART, d, s, p );

    // Keep tracking vars in sync (CDC semantics: stop_bits 0=1,1=1.5,2=2)
    if ( baud > 0 ) {
        s_line_coding.bit_rate = baud;
        serial1baud = baud;
    }
    s_line_coding.data_bits = d;
    s_line_coding.parity = ( parity == 1 ? 1 : ( parity == 2 ? 2 : 0 ) );
    s_line_coding.stop_bits = ( s == 1 ? 0 : 2 );
    set_micros_per_byte( &s_line_coding );
    s_line_coding_override = true;
}

#endif // ASYNC_PASSTHROUGH_ENABLED

