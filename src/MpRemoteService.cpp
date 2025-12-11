// SPDX-License-Identifier: MIT
#include "MpRemoteService.h"
#include "ArduinoStuff.h"  // For USBSer2
#include "Python_Proper.h" // For MicroPython execution
#include <cstring>

#ifdef USE_TINYUSB
#include "tusb.h" // For tud_task() function
#endif

extern "C" {
#include "py/gc.h"
#include "py/mpstate.h"
#include "py/runtime.h"
#include <micropython_embed.h>

#if USE_NATIVE_PYEXEC_RAW_REPL
// MicroPython's native raw REPL implementation
#include "shared/runtime/pyexec.h"
#endif

// Forward declaration for soft reboot function
void jl_soft_reboot( void );
}

// MicroPython stdout/stderr routing (defined in Python_Proper.cpp)
extern "C" void* global_mp_stream_ptr;
extern Stream* global_mp_stream;
extern Stream* mp_interrupt_check_stream;
extern bool mp_interrupt_requested;
extern bool mp_soft_reset_requested;

// Singleton instance
MpRemoteService* MpRemoteService::instance = nullptr;

// Global reference
MpRemoteService& mpRemoteService = MpRemoteService::getInstance( );

// Static output buffer pointers for capture callback
static MpRemoteService* s_active_capture = nullptr;

MpRemoteService::MpRemoteService( ) {
    // Allocate buffers
    m_code_buffer = new char[ CODE_BUFFER_SIZE ];
    m_stdout_buffer = new char[ OUTPUT_BUFFER_SIZE ];
    m_stderr_buffer = new char[ OUTPUT_BUFFER_SIZE ];
    m_line_buffer = new char[ LINE_BUFFER_SIZE ];

    if ( m_code_buffer )
        memset( m_code_buffer, 0, CODE_BUFFER_SIZE );
    if ( m_stdout_buffer )
        memset( m_stdout_buffer, 0, OUTPUT_BUFFER_SIZE );
    if ( m_stderr_buffer )
        memset( m_stderr_buffer, 0, OUTPUT_BUFFER_SIZE );
    if ( m_line_buffer )
        memset( m_line_buffer, 0, LINE_BUFFER_SIZE );
}

MpRemoteService& MpRemoteService::getInstance( ) {
    if ( !instance ) {
        instance = new MpRemoteService( );
    }
    return *instance;
}

ServiceStatus MpRemoteService::service( ) {
    if ( !m_enabled ) {
        return ServiceStatus::IDLE;
    }

    // Track DTR state for detecting new connections
    static bool prev_dtr = false;
    static bool repl_initialized = false;
    bool current_dtr = USBSer2; // CDC bool returns true if connected and DTR asserted

    // When DTR goes from low to high (new connection), initialize event REPL
    if ( current_dtr && !prev_dtr ) {
        if ( m_debug ) {
            Serial.println( "[MpRemote] DTR asserted - new connection detected" );
        }
        // Ensure MicroPython is initialized
        ensureMicroPythonInitialized( );

        // CRITICAL: Redirect MicroPython I/O to USBSer2 BEFORE initializing REPL
        // so the banner and prompt go to the correct stream
        global_mp_stream = &USBSer2;
        global_mp_stream_ptr = (void*)&USBSer2;

        // Initialize event-driven REPL (will print banner to USBSer2)
        pyexec_event_repl_init( );
        repl_initialized = true;
        m_in_raw_repl = false;
    }
    prev_dtr = current_dtr;

    // Check if USBSer2 has data available
    if ( !USBSer2 || USBSer2.available( ) == 0 ) {
        return m_in_raw_repl ? ServiceStatus::BUSY : ServiceStatus::IDLE;
    }

    // Process characters one at a time using event-driven REPL
    // This is non-blocking and allows us to service other things
    int processed_count = 0;
    while ( USBSer2.available( ) && processed_count < 1024 ) { // Process max 1024 chars per service call
        int c = USBSer2.read( );
        if ( c < 0 )
            break;

        // Debug output removed for production
        if ( m_debug || printReceivedPython ) {
            Serial.write( c );
        }

        // Feed character to event-driven REPL
        // Returns 0 normally, PYEXEC_FORCED_EXIT if soft reset requested

        int result = pyexec_event_repl_process_char( c );

        // Check current REPL mode
        extern pyexec_mode_kind_t pyexec_mode_kind;
        m_in_raw_repl = ( pyexec_mode_kind == PYEXEC_MODE_RAW_REPL );

        if ( result & PYEXEC_FORCED_EXIT ) {
            if ( m_debug ) {
                Serial.println( "[MpRemote] Soft reset requested via event REPL" );
            }
            mp_soft_reset_requested = true;
        }

        processed_count++;
    }

    // CRITICAL: Handle soft reset requests from the native REPL
    if ( mp_soft_reset_requested ) {
        mp_soft_reset_requested = false;
        mp_interrupt_requested = false;

        if ( m_debug ) {
            Serial.println( "[MpRemote] Executing soft reset via jl_soft_reboot()" );
        }

        // Call our custom soft reset that doesn't corrupt pointers
        jl_soft_reboot( );

        // Send soft reboot message and raw REPL prompt
        if ( m_in_raw_repl ) {
            writeResponse( "soft reboot\r\n" );
            sendRawReplPrompt( );
        }
    }

    return m_in_raw_repl ? ServiceStatus::BUSY : ServiceStatus::IDLE;
}

void MpRemoteService::sendRawReplPrompt( ) {
    writeResponse( "raw REPL; CTRL-B to exit\r\n>" );
}

bool MpRemoteService::ensureMicroPythonInitialized( ) {
    if ( !isMicroPythonInitialized( ) ) {
        // Initialize MicroPython with output going to main Serial, not USBSer2
        // This prevents initialization messages from confusing tools like mpremote/ViperIDE
        Stream* saved_stream = global_mp_stream;
        bool result = initMicroPythonProper( &Serial, /*preserve_interrupt_char=*/true );
        global_mp_stream = saved_stream; // Restore
        return result;
    }
    return true;
}

void MpRemoteService::writeResponse( const char* str ) {
    // CRITICAL: Check for NULL or invalid pointer before dereferencing
    // Invalid pointers (like 0x00000000) point to boot ROM and cause garbage output
    if ( !str || (uintptr_t)str < 0x10000000 ) {
        Serial.printf( "[MpRemote] ERROR: Invalid string pointer: %p\r\n", str );
        return;
    }

    if ( m_debug ) {
        Serial.printf( "[MpRemote] Tx: " );
        // Print with escape sequences shown
        for ( const char* p = str; *p; p++ ) {
            if ( *p == '\r' )
                Serial.print( "\\r" );
            else if ( *p == '\n' )
                Serial.print( "\\n" );
            else if ( *p < 0x20 )
                Serial.printf( "\\x%02X", *p );
            else
                Serial.print( *p );
        }
        Serial.println( );
    }
    if ( USBSer2 ) {
        USBSer2.print( str );
        USBSer2.flush( );
    }
}

void MpRemoteService::writeResponse( const char* str, size_t len ) {
    // CRITICAL: Check for NULL or invalid pointer before dereferencing
    if ( !str || (uintptr_t)str < 0x10000000 ) {
        Serial.printf( "[MpRemote] ERROR: Invalid string pointer in writeResponse(len): %p\r\n", str );
        return;
    }

    if ( USBSer2 ) {
        USBSer2.write( (const uint8_t*)str, len );
        USBSer2.flush( );
    }
}

void MpRemoteService::writeByte( uint8_t b ) {
    if ( m_debug ) {
        Serial.printf( "[MpRemote] TxByte: 0x%02X\r\n", b );
    }
    if ( USBSer2 ) {
        USBSer2.write( b );
        USBSer2.flush( );
    }
}
