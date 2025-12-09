// SPDX-License-Identifier: MIT
#include "MpRemoteService.h"
#include "ArduinoStuff.h"    // For USBSer2
#include "Python_Proper.h"   // For MicroPython execution
#include <cstring>

#ifdef USE_TINYUSB
#include "tusb.h"  // For tud_task() function
#endif

extern "C" {
#include "py/gc.h"
#include "py/runtime.h"
#include "py/mpstate.h"
#include <micropython_embed.h>

#if USE_NATIVE_PYEXEC_RAW_REPL
// MicroPython's native raw REPL implementation
#include "shared/runtime/pyexec.h"
#endif

// Forward declaration for soft reboot function
void jl_soft_reboot(void);
}

// MicroPython stdout/stderr routing (defined in Python_Proper.cpp)
extern "C" void *global_mp_stream_ptr;
extern Stream *global_mp_stream;
extern Stream *mp_interrupt_check_stream;
extern bool mp_interrupt_requested;
extern bool mp_soft_reset_requested;

// Singleton instance
MpRemoteService* MpRemoteService::instance = nullptr;

// Global reference
MpRemoteService& mpRemoteService = MpRemoteService::getInstance();

// Static output buffer pointers for capture callback
static MpRemoteService* s_active_capture = nullptr;

MpRemoteService::MpRemoteService() {
    // Allocate buffers
    m_code_buffer = new char[CODE_BUFFER_SIZE];
    m_stdout_buffer = new char[OUTPUT_BUFFER_SIZE];
    m_stderr_buffer = new char[OUTPUT_BUFFER_SIZE];
    m_line_buffer = new char[LINE_BUFFER_SIZE];
    
    if (m_code_buffer) memset(m_code_buffer, 0, CODE_BUFFER_SIZE);
    if (m_stdout_buffer) memset(m_stdout_buffer, 0, OUTPUT_BUFFER_SIZE);
    if (m_stderr_buffer) memset(m_stderr_buffer, 0, OUTPUT_BUFFER_SIZE);
    if (m_line_buffer) memset(m_line_buffer, 0, LINE_BUFFER_SIZE);
}

MpRemoteService& MpRemoteService::getInstance() {
    if (!instance) {
        instance = new MpRemoteService();
    }
    return *instance;
}




ServiceStatus MpRemoteService::service() {
    if (!m_enabled) {
        return ServiceStatus::IDLE;
    }
    
#if USE_NATIVE_PYEXEC_RAW_REPL
    // ============================================================================
    // OPTION 1: Use MicroPython's native event-driven REPL implementation
    // ============================================================================
    
    // Track DTR state for detecting new connections
    static bool prev_dtr = false;
    static bool repl_initialized = false;
    bool current_dtr = USBSer2;  // CDC bool returns true if connected and DTR asserted
    
    // When DTR goes from low to high (new connection), initialize event REPL
    if (current_dtr && !prev_dtr) {
        if (m_debug) {
            Serial.println("[MpRemote] DTR asserted - new connection detected");
        }
        // Ensure MicroPython is initialized
        ensureMicroPythonInitialized();
        
        // CRITICAL: Redirect MicroPython I/O to USBSer2 BEFORE initializing REPL
        // so the banner and prompt go to the correct stream
        global_mp_stream = &USBSer2;
        global_mp_stream_ptr = (void*)&USBSer2;
        
        // Initialize event-driven REPL (will print banner to USBSer2)
        pyexec_event_repl_init();
        repl_initialized = true;
        m_in_raw_repl = false;
    }
    prev_dtr = current_dtr;
    
    // Check if USBSer2 has data available
    if (!USBSer2 || USBSer2.available() == 0) {
        return m_in_raw_repl ? ServiceStatus::BUSY : ServiceStatus::IDLE;
    }
    
    // Process characters one at a time using event-driven REPL
    // This is non-blocking and allows us to service other things
    int processed_count = 0;
    while (USBSer2.available() && processed_count < 400) {  // Process max 100 chars per service call
        int c = USBSer2.read();
        if (c < 0) break;
        
        // Debug output removed for production
if (m_debug || printReceivedPython) {
        Serial.write(c);
}
        
        // Feed character to event-driven REPL
        // Returns 0 normally, PYEXEC_FORCED_EXIT if soft reset requested

        int result = pyexec_event_repl_process_char(c);
        
        // Check current REPL mode
        extern pyexec_mode_kind_t pyexec_mode_kind;
        m_in_raw_repl = (pyexec_mode_kind == PYEXEC_MODE_RAW_REPL);
        
        if (result & PYEXEC_FORCED_EXIT) {
            if (m_debug) {
                Serial.println("[MpRemote] Soft reset requested via event REPL");
            }
            mp_soft_reset_requested = true;
        }
        
        processed_count++;
    }
    
    // CRITICAL: Handle soft reset requests from the native REPL
    if (mp_soft_reset_requested) {
        mp_soft_reset_requested = false;
        mp_interrupt_requested = false;
        
        if (m_debug) {
            Serial.println("[MpRemote] Executing soft reset via jl_soft_reboot()");
        }
        
        // Call our custom soft reset that doesn't corrupt pointers
        jl_soft_reboot();
        
        // Send soft reboot message and raw REPL prompt
        if (m_in_raw_repl) {
            writeResponse("soft reboot\r\n");
            sendRawReplPrompt();
        }
    }
    
    return m_in_raw_repl ? ServiceStatus::BUSY : ServiceStatus::IDLE;
    
#else
    // ============================================================================
    // OPTION 2: Use custom MpRemoteService state machine (legacy)
    // ============================================================================
    
    // Track DTR state for sending initial prompt when connection is established
    static bool prev_dtr = false;
    bool current_dtr = USBSer2;  // CDC bool returns true if connected and DTR asserted
    
    // When DTR goes from low to high (new connection), send initial prompt
    if (current_dtr && !prev_dtr) {
        if (m_debug) {
            Serial.println("[MpRemote] DTR asserted - new connection detected");
        }
        // Ensure MicroPython is initialized before sending banner
        ensureMicroPythonInitialized();
        
        // Send the MicroPython banner on initial connection
        // Format must match standard MicroPython banner for tools like ViperIDE to parse:
        // "MicroPython v<version> on <date>; <board> with <mcu>"
        // MICROPY_BANNER_MACHINE is defined as MICROPY_HW_BOARD_NAME " with " MICROPY_HW_MCU_NAME
        // which gives us "jumperless-v5 with rp2350b"
        writeResponse("\r\n");
        writeResponse("MicroPython v1.24.0; jumperless-v5 with rp2350b\r\n");
        writeResponse("Type \"help()\" for more information.\r\n");
        writeResponse(">>> ");
        
        // Reset state
        m_state = ReplState::IDLE;
        m_line_len = 0;
        m_code_len = 0;
        m_in_raw_repl = false;
    }
    prev_dtr = current_dtr;
    
    // Debug: Print periodically to main Serial to verify service is running
    static uint32_t callCount = 0;
    callCount++;
    if (m_debug && (callCount % 50000 == 0)) {
        Serial.printf("[MpRemote] Service alive, calls=%lu, available=%d\r\n", 
            callCount, USBSer2.available());
    }
    
    // Check if USBSer2 has data available
    // NOTE: Don't check "if (!USBSer2)" - the CDC operator bool returns false if DTR not asserted,
    // but we still want to read data. Just check available() directly.
    int avail = USBSer2.available();
    if (avail == 0) {
        return ServiceStatus::IDLE;
    }
    
    if (m_debug) {
        Serial.printf("[MpRemote] Got %d bytes!\r\n", avail);
    }
    
    // Process all available bytes
    while (USBSer2.available()) {
        // CRITICAL: During EXECUTING state, do NOT consume any bytes from USBSer2.
        // The MicroPython VM needs to handle ALL raw REPL protocol bytes itself via mp_hal_check_interrupt():
        // - Ctrl-C (0x03) for KeyboardInterrupt
        // - Ctrl-D (0x04) for soft reset
        // - Flow control bytes in raw-paste mode (0x01, 0x04)
        // If we consume them here, the VM never sees them and interrupts are lost.
        if (m_state == ReplState::EXECUTING) {
            // Just return - let mp_hal_check_interrupt() and mp_hal_stdin_rx_chr() handle everything
            return ServiceStatus::BUSY;
        }

        int c = USBSer2.read();
        if (c < 0) break;
        
        // Debug logging
        if (m_debug) {
            Serial.printf("[MpRemote] State=%d Rx=0x%02X '%c'\r\n", 
                (int)m_state, c, (c >= 0x20 && c < 0x7F) ? c : '.');
        }
        
        switch (m_state) {
            case ReplState::IDLE:
                handleIdleState(c);
                break;
            case ReplState::RAW_REPL:
                handleRawReplState(c);
                break;
            case ReplState::RECEIVING_CODE:
                handleReceivingCodeState(c);
                break;
            case ReplState::RAW_PASTE_CHECK:
                handleRawPasteCheck(c);
                break;
            case ReplState::EXECUTING:
                // Should never reach here due to the check above, but keep for safety
                break;
            default:
                break;
        }
    }
    
    return m_in_raw_repl ? ServiceStatus::BUSY : ServiceStatus::IDLE;
    
#endif // USE_NATIVE_PYEXEC_RAW_REPL
}

void MpRemoteService::handleIdleState(int c) {
    // Friendly REPL mode: buffer characters until Enter is pressed
    switch (c) {
        case 0x01: // Ctrl-A: Enter raw REPL
            m_line_len = 0;  // Clear line buffer
            enterRawRepl(false);
            break;
            
        case 0x02: // Ctrl-B: Show banner and prompt
            writeResponse("\r\nMicroPython v1.25.0; jumperless-v5 with rp2350b\r\n");
            writeResponse("Type \"help()\" for more information.\r\n");
            m_line_len = 0;  // Clear line buffer
            writeResponse(">>> ");
            break;
            
        case 0x03: // Ctrl-C: Cancel current line
            m_line_len = 0;  // Clear line buffer
            writeResponse("\r\n>>> ");
            break;
            
        case 0x04: // Ctrl-D: Soft reset
            m_line_len = 0;  // Clear line buffer
            sendSoftReboot();
            break;
            
        case '\r': // Carriage return - execute line
        case '\n': // Newline - execute line
            if (m_line_len > 0) {
                // Execute the buffered line
                executeFriendlyReplLine();
            } else {
                // Empty line - just show prompt
                writeResponse("\r\n>>> ");
            }
            break;
            
        case 0x7F: // Backspace (DEL)
        case 0x08: // Backspace (BS)
            if (m_line_len > 0) {
                m_line_len--;
                // Echo backspace: move back, space, move back
                writeResponse("\b \b");
            }
            break;
            
        default:
            // Buffer printable characters and echo them
            if (c >= 0x20 && c < 0x7F && m_line_buffer && m_line_len < LINE_BUFFER_SIZE - 1) {
                m_line_buffer[m_line_len++] = (char)c;
                // Echo the character
                char echo[2] = {(char)c, '\0'};
                writeResponse(echo);
            }
            break;
    }
}

void MpRemoteService::handleRawReplState(int c) {
    switch (c) {
        case '\r': // Carriage return - ignore in raw REPL
        case '\n': // Newline - ignore in raw REPL waiting state
            break;
            
        case 0x01: // Ctrl-A: Already in raw REPL, just send prompt
            sendRawReplPrompt();
            break;
            
        case 0x02: // Ctrl-B: Exit raw REPL
            exitRawRepl();
            break;
            
        case 0x03: // Ctrl-C: Interrupt (nothing running, just acknowledge)
            m_code_len = 0; // Clear any partial code
            sendRawReplPrompt();
            break;
            
        case 0x04: // Ctrl-D: Soft reset (if no code buffered) or execute (if code buffered)
            if (m_code_len == 0) {
                // Soft reset in raw REPL mode
                // mpremote expects: "soft reboot\r\n" followed by "raw REPL; CTRL-B to exit\r\n"
                // Note: jl_soft_reboot() automatically preserves stream routing
                jl_soft_reboot();
                writeResponse("soft reboot\r\n");
                // Stay in raw REPL and send the prompt again
                sendRawReplPrompt();
            } else {
                // Execute buffered code
                executeCode();
            }
            break;
            
        case 0x05: // Ctrl-E: Possibly entering raw-paste mode
            m_state = ReplState::RAW_PASTE_CHECK;
            m_raw_paste_check_state = 0; // Waiting for 'A'
            break;
            
        default:
            // Start receiving code
            if (m_code_len < CODE_BUFFER_SIZE - 1) {
                m_code_buffer[m_code_len++] = (char)c;
            }
            m_state = ReplState::RECEIVING_CODE;
            break;
    }
}

void MpRemoteService::handleReceivingCodeState(int c) {
    if (c == 0x04) { // Ctrl-D: End of code, execute
        m_code_buffer[m_code_len] = '\0';
        executeCode();
    } else if (c == 0x03) { // Ctrl-C: Cancel code entry
        m_code_len = 0;
        m_code_buffer[0] = '\0';
        m_state = ReplState::RAW_REPL;
        sendRawReplPrompt();
    } else {
        // Add character to buffer
        if (m_code_len < CODE_BUFFER_SIZE - 1) {
            m_code_buffer[m_code_len++] = (char)c;
        }
    }
}

void MpRemoteService::handleRawPasteCheck(int c) {
    if (m_raw_paste_check_state == 0) {
        // Expecting 'A'
        if (c == 'A') {
            m_raw_paste_check_state = 1;
        } else {
            // Not raw-paste command, treat as regular code
            if (m_code_len < CODE_BUFFER_SIZE - 1) {
                m_code_buffer[m_code_len++] = 0x05; // The original Ctrl-E
                m_code_buffer[m_code_len++] = (char)c;
            }
            m_state = ReplState::RECEIVING_CODE;
        }
    } else if (m_raw_paste_check_state == 1) {
        // Expecting 0x01
        if (c == 0x01) {
            // Raw-paste mode requested
            if (m_raw_paste_supported) {
                // We support raw-paste, respond with R\x01 and window size
                writeByte('R');
                writeByte(0x01);
                // Send window size (2 bytes, little-endian) - use 256 bytes
                writeByte(0x00);  // Low byte
                writeByte(0x01);  // High byte (256)
                
                // Now wait for data in raw-paste mode
                // Raw-paste receives data with flow control
                m_state = ReplState::RECEIVING_CODE;
                m_code_len = 0;
            } else {
                // Not supported
                writeByte('R');
                writeByte(0x00);
                m_state = ReplState::RAW_REPL;
            }
        } else {
            // Invalid sequence, treat as code
            if (m_code_len < CODE_BUFFER_SIZE - 1) {
                m_code_buffer[m_code_len++] = 0x05; // Original Ctrl-E
                m_code_buffer[m_code_len++] = 'A';
                m_code_buffer[m_code_len++] = (char)c;
            }
            m_state = ReplState::RECEIVING_CODE;
        }
    }
}

void MpRemoteService::enterRawRepl(bool with_soft_reset) {
    m_in_raw_repl = true;
    m_code_len = 0;
    m_state = ReplState::RAW_REPL;

    // Route interrupt polling to the mpremote transport and clear pending flags
    mp_interrupt_check_stream = &USBSer2;
    mp_interrupt_requested = false;
    mp_soft_reset_requested = false;
    
    // Ensure MicroPython is initialized (any output goes to main Serial, not USBSer2)
    ensureMicroPythonInitialized();
    // Keep interrupt character at MicroPython default for raw REPL tooling control
    // mp_embed_exec_str("import micropython; micropython.kbd_intr(3)");
    // mp_hal_set_interrupt_char(3);
    
    // Pre-import modules commonly needed by mpremote/ViperIDE
    // This ensures os.listdir(), os.stat(), etc. work for file operations
    // Execute silently with output to main Serial (not USBSer2)
    Stream* saved_stream = global_mp_stream;
    global_mp_stream = &Serial;  // Redirect to main serial to avoid confusing tools
    
    // Import os module for filesystem operations (needed by mpremote fs commands)
    mp_embed_exec_str("import os");
    // Import gc for memory management
    mp_embed_exec_str("import gc");
    // Import jumperless module so all Jumperless functions are available
    mp_embed_exec_str("try:\n    from jumperless import *\nexcept: pass");
    
    global_mp_stream = saved_stream;  // Restore
    
    if (with_soft_reset) {
        // Do soft reset first, then show raw REPL prompt
        // Note: jl_soft_reboot() automatically preserves stream routing
        jl_soft_reboot();
        // Don't send anything extra here - just the reboot message
        writeResponse("soft reboot\r\n");
    }
    
    // Send the raw REPL banner - this is EXACTLY what mpremote and ViperIDE look for
    // Must be exactly: "raw REPL; CTRL-B to exit\r\n>"
    sendRawReplPrompt();
    
    if (m_debug) {
        Serial.println("[MpRemote] Entered raw REPL mode with os, gc, jumperless imported");
    }
}

void MpRemoteService::exitRawRepl() {
    m_in_raw_repl = false;
    m_code_len = 0;
    m_state = ReplState::IDLE;
    
    // Restore interrupt polling to main Serial when leaving raw REPL
    mp_interrupt_check_stream = &Serial;
    mp_interrupt_requested = false;
    mp_soft_reset_requested = false;
    
    // Send exit message
    writeResponse("\r\n");
    
    // Return to friendly REPL prompt
    writeResponse(">>> ");
}

void MpRemoteService::sendSoftReboot() {
    // Perform soft reset of MicroPython state
    // Note: jl_soft_reboot() automatically preserves stream routing
    jl_soft_reboot();
    
    // Send the standard MicroPython soft reboot message
    // This is used when soft reset happens in friendly REPL mode
    writeResponse("MPY: soft reboot\r\n");
    writeResponse("MicroPython v1.25.0; jumperless-v5 with rp2350b\r\n");
    writeResponse("Type \"help()\" for more information.\r\n");
    writeResponse(">>> ");
}

void MpRemoteService::sendRawReplPrompt() {
    writeResponse("raw REPL; CTRL-B to exit\r\n>");
}

void MpRemoteService::executeCode() {
    m_state = ReplState::EXECUTING;
    mp_interrupt_requested = false;
    mp_soft_reset_requested = false;
    
    // Null-terminate the code
    m_code_buffer[m_code_len] = '\0';

    // Some hosts (e.g., ViperIDE) may prepend a leading NUL. Strip leading NULs
    // so MicroPython sees the intended source text.
    size_t start_idx = 0;
    while (start_idx < m_code_len && m_code_buffer[start_idx] == '\0') {
        start_idx++;
    }
    if (start_idx > 0) {
        m_code_len -= start_idx;
        memmove(m_code_buffer, m_code_buffer + start_idx, m_code_len + 1); // +1 to include '\0'
    }
    
    // Clear output buffers
    m_stdout_len = 0;
    m_stderr_len = 0;
    if (m_stdout_buffer) m_stdout_buffer[0] = '\0';
    if (m_stderr_buffer) m_stderr_buffer[0] = '\0';
    
    if (m_debug) {
        Serial.printf("[MpRemote] Executing %d bytes of code:\r\n", m_code_len);
        // Print first 200 chars of code for debugging
        int showLen = (m_code_len > 20000) ? 20000 : m_code_len;
        for (int i = 0; i < showLen; i++) {
            char c = m_code_buffer[i];
            if (c == '\n') Serial.print("\\n");
            else if (c == '\r') Serial.print("\\r");
            else if (c >= 0x20 && c < 0x7F) Serial.print(c);
            else Serial.printf("\\x%02X", (uint8_t)c);
        }
        if (m_code_len > 20000) Serial.print("...");
        Serial.println();
    }
    
    // Save current stream - we'll redirect MicroPython output to USBSer2
    m_previous_stream = global_mp_stream;
    void *prev_stream_ptr = global_mp_stream_ptr;
    
    // Send OK to indicate we're executing (BEFORE redirecting stream)
    writeResponse("OK");
    
    // Now redirect MicroPython output to USBSer2
    // NOTE: We use a wrapper approach to avoid mp_hal_check_interrupt reading from USBSer2
    // which would consume data that mpremote is trying to send us
    global_mp_stream = &USBSer2;
    global_mp_stream_ptr = (void *)&USBSer2;
    
    // Enable Python output debugging if service debug is enabled
    extern bool mpremote_debug_python_output;
    if (m_debug) {
        mpremote_debug_python_output = true;
    }
    
    // Execute the code using mp_embed_exec_str which handles exceptions internally
    // The output will go to USBSer2 via global_mp_stream
    // NOTE: mp_embed_exec_str returns void, not int
    mp_embed_exec_str(m_code_buffer);
    
    // Disable Python output debugging
    mpremote_debug_python_output = false;
    
    // Service USB to ensure all buffered output is transmitted
    tud_task();
    if (USBSer2) {
        USBSer2.flush();
    }
    tud_task();

    // Restore stream BEFORE doing anything else
    global_mp_stream = m_previous_stream;
    global_mp_stream_ptr = prev_stream_ptr;

    // If a soft reset was requested during execution, reboot MicroPython and re-emit the raw prompt
    if (mp_soft_reset_requested) {
        mp_soft_reset_requested = false;
        mp_interrupt_requested = false;
        m_code_len = 0;

        // Note: jl_soft_reboot() automatically preserves stream routing
        jl_soft_reboot();
        writeResponse("soft reboot\r\n");
        sendRawReplPrompt();
        m_state = ReplState::RAW_REPL;
        return;
    }

    // Send raw-REPL EOF markers per protocol: output then errors. We always emit
    // both markers even if empty to keep the host parser aligned.
    writeByte(0x04); // stdout end
    writeByte(0x04); // stderr end
    // Insert CRLF so text decoders and IDEs don't see a bare 0x04 0x04 at buffer end
   // writeResponse("\r\n");
    
    // Clear code buffer
    m_code_len = 0;
    // m_code_buffer[0] = '\0';
    mp_interrupt_requested = false;
    
    // Run garbage collection
    gc_collect();
    
    // Return to RAW_REPL state, waiting for prompt
    m_state = ReplState::RAW_REPL;
    
    // Send the prompt character for next command
    writeByte('>');
    
    if (m_debug) {
        Serial.println("[MpRemote] Execution complete");
    }
}

bool MpRemoteService::ensureMicroPythonInitialized() {
    if (!isMicroPythonInitialized()) {
        // Initialize MicroPython with output going to main Serial, not USBSer2
        // This prevents initialization messages from confusing tools like mpremote/ViperIDE
        Stream* saved_stream = global_mp_stream;
        bool result = initMicroPythonProper(&Serial, /*preserve_interrupt_char=*/true);
        global_mp_stream = saved_stream;  // Restore
        return result;
    }
    return true;
}

void MpRemoteService::executeFriendlyReplLine() {
    // Null-terminate the line buffer
    if (m_line_buffer) {
        m_line_buffer[m_line_len] = '\0';
    }
    
    if (m_debug) {
        Serial.printf("[MpRemote] FriendlyREPL executing: %s\r\n", m_line_buffer);
    }
    
    // Echo newline after command
    writeResponse("\r\n");
    
    // Redirect output to USBSer2 for the execution
    m_previous_stream = global_mp_stream;
    void *prev_stream_ptr = global_mp_stream_ptr;
    global_mp_stream = &USBSer2;
    global_mp_stream_ptr = (void *)&USBSer2;
    
    // Execute the line
    // Note: mp_embed_exec_str handles exceptions and prints them
    mp_embed_exec_str(m_line_buffer);
    
    // Service USB to ensure output is transmitted
    #ifdef USE_TINYUSB
    tud_task();
    if (USBSer2) {
        USBSer2.flush();
    }
    tud_task();
    #endif
    
    // Restore stream
    global_mp_stream = m_previous_stream;
    global_mp_stream_ptr = prev_stream_ptr;
    
    // Clear line buffer
    m_line_len = 0;
    
    // Run garbage collection
    gc_collect();
    
    // Show prompt for next command
    writeResponse(">>> ");
}

void MpRemoteService::writeResponse(const char* str) {
    // CRITICAL: Check for NULL or invalid pointer before dereferencing
    // Invalid pointers (like 0x00000000) point to boot ROM and cause garbage output
    if (!str || (uintptr_t)str < 0x10000000) {
        Serial.printf("[MpRemote] ERROR: Invalid string pointer: %p\r\n", str);
        return;
    }
    
    if (m_debug) {
        Serial.printf("[MpRemote] Tx: ");
        // Print with escape sequences shown
        for (const char* p = str; *p; p++) {
            if (*p == '\r') Serial.print("\\r");
            else if (*p == '\n') Serial.print("\\n");
            else if (*p < 0x20) Serial.printf("\\x%02X", *p);
            else Serial.print(*p);
        }
        Serial.println();
    }
    if (USBSer2) {
        USBSer2.print(str);
        USBSer2.flush();
    }
}

void MpRemoteService::writeResponse(const char* str, size_t len) {
    // CRITICAL: Check for NULL or invalid pointer before dereferencing
    if (!str || (uintptr_t)str < 0x10000000) {
        Serial.printf("[MpRemote] ERROR: Invalid string pointer in writeResponse(len): %p\r\n", str);
        return;
    }
    
    if (USBSer2) {
        USBSer2.write((const uint8_t*)str, len);
        USBSer2.flush();
    }
}

void MpRemoteService::writeByte(uint8_t b) {
    if (m_debug) {
        Serial.printf("[MpRemote] TxByte: 0x%02X\r\n", b);
    }
    if (USBSer2) {
        USBSer2.write(b);
        USBSer2.flush();
    }
}

