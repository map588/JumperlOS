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
};

// Separate state machines for each direction
static TagParserState usb_to_uart_parser = { TAG_SEARCHING, {0}, 0, {0} };
static TagParserState uart_to_usb_parser = { TAG_SEARCHING, {0}, 0, {0} };

// Command injection tracking (no rate limiting - process all commands immediately)
static uint32_t s_last_command_injection_time = 0;
static uint32_t s_injected_commands = 0;

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
    "jumperless"
};
static const uint8_t NUM_COMMAND_TAGS = 3;

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

static void async_uart_irq_handler( void ) {
    while ( uart_is_readable( ASYNC_PASSTHROUGH_UART ) ) {
        uint8_t c = (uint8_t)uart_getc( ASYNC_PASSTHROUGH_UART );
        ring_push_byte( c );
    }
    // Record and clear sticky error flags (e.g., overrun)
    uart_hw_t* hw = uart_get_hw( ASYNC_PASSTHROUGH_UART );
    uint32_t rsr = hw->rsr;
    if ( rsr & ( UART_UARTRSR_OE_BITS | UART_UARTRSR_FE_BITS | UART_UARTRSR_PE_BITS | UART_UARTRSR_BE_BITS ) ) {
        if ( rsr & UART_UARTRSR_OE_BITS ) {
            s_uart_overrun_count++;
        }
        hw->rsr = 0xFFFFFFFFu; // write to RSR (alias of ECR) clears errors
    }
    s_uart_flush_pending = true;
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
static inline bool process_command_tag_byte( uint8_t c, TagParserState* parser, const char* direction ) {
    switch ( parser->state ) {
        case TAG_SEARCHING:
            if ( c == '<' ) {
                parser->state = TAG_DETECTING;
                parser->tag_buffer_idx = 0;
                memset( parser->tag_buffer, 0, sizeof(parser->tag_buffer) );
                return false; // Consume the '<'
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
                        // Inject newline to complete the command AND set flag for InjectedCommandService
                        Jerial.injectInput("\n", false);
                        Jerial.hasInjectedCommand = 1;  // Signal complete command ready
                        s_injected_commands++;
                        s_last_command_injection_time = millis();
                        //delay(1);
                        // Reset state
                        parser->state = TAG_SEARCHING;
                        memset( parser->current_tag, 0, sizeof(parser->current_tag) );
                    } else {
                        // Mismatched closing tag, treat as normal text
                        parser->state = TAG_SEARCHING;
                    }
                    return false; // Consumed
                    
                } else if ( is_command_tag( parser->tag_buffer ) ) {
                    // Valid opening command tag - will inject characters as they arrive
                    strcpy( parser->current_tag, parser->tag_buffer );
                    parser->state = TAG_IN_COMMAND;
                    return false; // Consumed
                    
                } else {
                    // Not a command tag, forward the whole thing
                    parser->state = TAG_SEARCHING;
                    return true; // Let this '>' through
                }
                
            } else if ( parser->tag_buffer_idx < sizeof(parser->tag_buffer) - 1 ) {
                parser->tag_buffer[parser->tag_buffer_idx++] = c;
                return false; // Consuming tag name
                
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
                return false; // Start detecting closing tag
                
            } else {
                // Inject character immediately into input stream (no buffering)
                // This allows Jerial's line buffering to handle command completion
                #if DEBUG_INJECTED_COMMANDS
                Serial.printf("AsyncPassthrough: Injecting '%c' (%d) from <%s> tag\n", 
                             (c >= 32 && c < 127) ? c : '?', (int)c, parser->current_tag);
                Serial.flush();
                #endif
                delayMicroseconds(350);
                Jerial.injectInput(&c, 1, false);
                
                return false; // Consumed and injected
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
                uart_write_blocking( ASYNC_PASSTHROUGH_UART, forward_buf, forward_idx );
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
    const uint32_t MAX_BYTES_PER_CALL = 1024;
    uint32_t processed = 0;
    
    // If in forwarding mode, stream all bytes to main Serial until end token or timeout
    if ( s_forward_active ) {
        unsigned long forwardStart = micros();
        bool ended = false;
        
        // CRITICAL: Reduce chunk size during forwarding (Serial.write is VERY slow!)
        // Even 128 bytes can take 800-3400ms when writing to Serial!
        const uint32_t MAX_FORWARD_BYTES = 256;  // Smaller chunk for Serial output
        uint32_t forwardCount = 0;
        
        while ( ring_pop_byte( &c ) && forwardCount < MAX_FORWARD_BYTES ) {
            Serial.write( c );
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
    while ( avail > 0 && ring_pop_byte( &c ) && processed < MAX_BYTES_PER_CALL ) {
        // ALWAYS passthrough raw data to USBSer1 (CDC 1) for debugging
        // This shows the complete data stream including tags
        if ( tud_cdc_n_connected( 1 ) ) {
            tud_cdc_n_write_char( 1, c );
        }
        wrote++;
        avail--;
        if ( asyncPassthroughTagParsingEnabled ) {
            // Tag parsing enabled - check for command tags
            if ( process_command_tag_byte( c, &uart_to_usb_parser, "UART→USB" ) ) {
                // Byte should be forwarded to USB Serial (CDC 0) for display
                // tud_cdc_n_write_char( itf, c );
                // wrote++;
                // avail--;
            }
            // If consumed by tag parser, don't forward to CDC 0
        } else {
            // Tag parsing disabled - pass through all bytes directly (for upload)
            // tud_cdc_n_write_char( itf, c );
            // wrote++;
            // avail--;
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

    // Enable RX interrupt for immediate forwarding; include RX timeout interrupt
#if JL_UART0_INTEROP_MODE == 0
    // Integrate: use shared handler so MicroPython and passthrough can coexist
    irq_add_shared_handler( ASYNC_PASSTHROUGH_UART_IRQ, async_uart_irq_handler, 0 );
    irq_set_enabled( ASYNC_PASSTHROUGH_UART_IRQ, true );
#else
    // Preempt (default): exclusive handler while not suspended by MicroPython
    irq_set_exclusive_handler( ASYNC_PASSTHROUGH_UART_IRQ, async_uart_irq_handler );
    irq_set_priority( ASYNC_PASSTHROUGH_UART_IRQ, 0 ); // highest priority
    irq_set_enabled( ASYNC_PASSTHROUGH_UART_IRQ, true );
#endif
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

    // Register default forward prefixes
    registerForwardPrefix( "jcommand:" );
    registerForwardPrefix( "\x02" ); // SOH
    registerForwardPrefix( "\x03" ); // DLE
    registerForwardPrefix( "jl:" );
}

void task( ) {
    unsigned long taskStart = micros();
    unsigned long t0, t1;
    
    // CRITICAL: Check DTR state FIRST, before any data processing
    // This ensures DTR pulse detection takes absolute priority over command injection
    // USBSer1 is declared extern at file scope (line 21), outside the namespace
    t0 = micros();
    checkDTRState( USBSer1 );
    t1 = micros();
    #if DEBUG_INJECTED_COMMANDS
    if ((t1 - t0) > 50000) {  // > 50ms
        Serial.printf("⏱️  checkDTRState took %lu ms\n", (t1 - t0) / 1000);
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
    
    // If suspended by MicroPython, avoid touching UART hardware
    if ( s_uart_suspended_by_mpy ) {
        tud_task();
        return;
    }
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


    // USB -> UART when either pending flag set or data available
    t0 = micros();
    if ( s_usb_rx_pending || ( tud_inited( ) && tud_cdc_n_available( ASYNC_PASSTHROUGH_CDC_ITF ) ) ) {
        bridge_usb_to_uart( ASYNC_PASSTHROUGH_CDC_ITF );
        s_usb_rx_pending = false;
    }
    t1 = micros();
    #if DEBUG_INJECTED_COMMANDS
    if ((t1 - t0) > 50000) {
        Serial.printf("⏱️  bridge_usb_to_uart took %lu ms\n", (t1 - t0) / 1000);
    }
    #endif
    
    // UART -> USB
    t0 = micros();
    bridge_uart_to_usb( ASYNC_PASSTHROUGH_CDC_ITF );
    t1 = micros();
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
    
    // Shift the array to the left, keeping only last 3 states
    if (current_dtr != s_dtr_state[2]) {
        s_dtr_state[0] = s_dtr_state[1];
        s_dtr_state[1] = s_dtr_state[2];
        s_dtr_state[2] = current_dtr;
        
        // Detect pulses going either direction (some things invert the DTR line)
        // Windows: just check for any change
        // macOS/Linux: check for full off-on-off or on-off-on pulse
        if (s_dtr_state[1] == 1 && s_dtr_state[2] == 0) {
            // Detected high-to-low pulse
            s_dtr_pulse_detected = true;
            
            // Disable tag parsing during Arduino flash to prevent interference
            // CRITICAL: Only disable if not already disabled to prevent timer reset
            // Increased timeouts: 200ms inactivity (handles upload pauses), 2s max (safety)
            if (asyncPassthroughEnabled && asyncPassthroughTagParsingEnabled) {
                disableTagParsingWithInactivityTimeout(5000, 2000);
                #if DEBUG_INJECTED_COMMANDS
                Serial.println("✓ DTR pulse - tag parsing disabled for flashing");
                #endif
                // No flush - too slow during rapid commands!
            }
        } else if (s_dtr_state[1] == 0 && s_dtr_state[2] == 1) {
            // Detected low-to-high pulse
            s_dtr_pulse_detected = true;
            
            // Disable tag parsing during Arduino flash to prevent interference
            // CRITICAL: Only disable if not already disabled to prevent timer reset
            // Increased timeouts: 200ms inactivity (handles upload pauses), 2s max (safety)
            if (asyncPassthroughEnabled && asyncPassthroughTagParsingEnabled) {
                disableTagParsingWithInactivityTimeout(5000, 2000);
                #if DEBUG_INJECTED_COMMANDS
                Serial.println("✓ DTR pulse - tag parsing disabled for flashing");
                #endif
                // No flush - too slow during rapid commands!
            }
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
    delay(3);
    digitalWrite(resetPin, HIGH);
}

} // namespace AsyncPassthrough

// ----------------------------------------------------------------------------
// Interop hooks for MicroPython ownership of UART0
// ----------------------------------------------------------------------------

// Track suspend state to avoid redundant reconfiguration
// (defined above to gate task() too)

extern "C" void jl_asyncpassthrough_suspend_uart0( void ) {
#if JL_UART0_INTEROP_MODE == 1
    if ( s_uart_suspended_by_mpy ) return;
    // Disable our IRQs and release exclusive handler so shared handlers can be installed safely
    irq_set_enabled( ASYNC_PASSTHROUGH_UART_IRQ, false );
    uart_set_irq_enables( ASYNC_PASSTHROUGH_UART, false, false );
    // Disable RX timeout and overrun interrupts we enabled
    hw_clear_bits( &uart_get_hw( ASYNC_PASSTHROUGH_UART )->imsc, UART_UARTIMSC_RTIM_BITS | UART_UARTIMSC_OEIM_BITS );
    // Release exclusive handler slot (set to null)
    irq_set_exclusive_handler( ASYNC_PASSTHROUGH_UART_IRQ, 0 );
    s_uart_suspended_by_mpy = true;
#else
    (void)s_uart_suspended_by_mpy; // unused
#endif
}

extern "C" void jl_asyncpassthrough_resume_uart0( void ) {
#if JL_UART0_INTEROP_MODE == 1
    if ( !s_uart_suspended_by_mpy ) return;
    // Reconfigure UART hardware and restore our IRQ configuration
    gpio_set_function( ASYNC_PASSTHROUGH_UART_TX_PIN, GPIO_FUNC_UART );
    gpio_set_function( ASYNC_PASSTHROUGH_UART_RX_PIN, GPIO_FUNC_UART );

    uart_init( ASYNC_PASSTHROUGH_UART, serial1baud );
    uart_set_format( ASYNC_PASSTHROUGH_UART, 8, 1, UART_PARITY_NONE );
    uart_set_fifo_enabled( ASYNC_PASSTHROUGH_UART, true );

    irq_set_exclusive_handler( ASYNC_PASSTHROUGH_UART_IRQ, async_uart_irq_handler );
    irq_set_priority( ASYNC_PASSTHROUGH_UART_IRQ, 0 );
    irq_set_enabled( ASYNC_PASSTHROUGH_UART_IRQ, true );
    uart_set_irq_enables( ASYNC_PASSTHROUGH_UART, true, false );
    hw_set_bits( &uart_get_hw( ASYNC_PASSTHROUGH_UART )->imsc, UART_UARTIMSC_RTIM_BITS | UART_UARTIMSC_OEIM_BITS );
    hw_write_masked( &uart_get_hw( ASYNC_PASSTHROUGH_UART )->ifls,
                     ( 0u << UART_UARTIFLS_RXIFLSEL_LSB ) | ( 2u << UART_UARTIFLS_TXIFLSEL_LSB ),
                     UART_UARTIFLS_RXIFLSEL_BITS | UART_UARTIFLS_TXIFLSEL_BITS );
    s_uart_suspended_by_mpy = false;
#endif
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

#endif
