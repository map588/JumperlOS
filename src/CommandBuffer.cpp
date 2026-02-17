// SPDX-License-Identifier: MIT
/**
 * CommandBuffer implementation
 * See CommandBuffer.h for architecture documentation
 */

#include "CommandBuffer.h"
#include "Adafruit_USBD_CDC.h"
#include <cstring>

// Singleton instance
static CommandBuffer* s_instance = nullptr;

CommandBuffer& CommandBuffer::getInstance() {
    if (s_instance == nullptr) {
        s_instance = new CommandBuffer();
    }
    return *s_instance;
}

// Global accessor for convenience
CommandBuffer& cmdBuffer = CommandBuffer::getInstance();

CommandBuffer::CommandBuffer()
    : has_pending_command(false)
    , is_python_command(false)
    , respond_to_uart(false)
    , uart_out_read(0)
    , uart_out_write(0)
    , commands_received(0)
    , commands_processed(0)
    , bytes_queued_to_uart(0)
    , bytes_sent_to_uart(0)
{
    memset(pending_command, 0, sizeof(pending_command));
    memset(uart_out_buffer, 0, sizeof(uart_out_buffer));
}

// =========================================================================
// INCOMING: Pending commands
// =========================================================================

bool CommandBuffer::setPendingJCommand(const char* cmd) {
    if (has_pending_command) {
        // Already have a pending command - can't accept another
        // Caller should wait or handle this
        return false;
    }
    
    if (!cmd || strlen(cmd) == 0) {
        return false;
    }
    
    // Copy command (truncate if too long)
    size_t len = strlen(cmd);
    if (len >= CMD_BUFFER_SIZE) {
        len = CMD_BUFFER_SIZE - 1;
    }
    memcpy(pending_command, cmd, len);
    pending_command[len] = '\0';
    
    is_python_command = false;
    respond_to_uart = true;  // Commands from UART get response to UART
    commands_received++;
    has_pending_command = true;  // Set flag LAST (memory barrier)
    
    return true;
}

bool CommandBuffer::setPendingPCommand(const char* cmd) {
    if (has_pending_command) {
        return false;
    }
    
    if (!cmd) {
        return false;
    }
    
    // Skip leading whitespace
    while (*cmd == ' ' || *cmd == '\t') {
        cmd++;
    }
    
    // Build command with '>' prefix if not already present
    size_t len = strlen(cmd);
    size_t offset = 0;
    
    if (cmd[0] != '>') {
        pending_command[0] = '>';
        offset = 1;
    }
    
    if (len + offset >= CMD_BUFFER_SIZE) {
        len = CMD_BUFFER_SIZE - offset - 1;
    }
    memcpy(pending_command + offset, cmd, len);
    pending_command[offset + len] = '\0';
    
    is_python_command = true;
    respond_to_uart = true;
    commands_received++;
    has_pending_command = true;
    
    return true;
}

String CommandBuffer::consumePendingCommand() {
    if (!has_pending_command) {
        return String();
    }
    
    String result(pending_command);
    
    // Clear command state BUT KEEP respond_to_uart flag!
    // The flag will be cleared by main loop after command execution completes
    has_pending_command = false;
    is_python_command = false;
    pending_command[0] = '\0';
    // Note: respond_to_uart is NOT cleared here - it stays set during execution
    // so that mp_hal_stdout_tx_strn_cooked can capture output for UART
    
    return result;
}

const char* CommandBuffer::consumePendingCommandPtr() {
    if (!has_pending_command) {
        return nullptr;
    }
    
    // Clear command state BUT KEEP respond_to_uart flag and buffer contents!
    // The buffer remains valid until next setPending*Command() call
    has_pending_command = false;
    is_python_command = false;
    // Note: Do NOT clear pending_command - caller needs to read it
    // Note: respond_to_uart is NOT cleared here
    
    return pending_command;
}

void CommandBuffer::clearPendingCommand() {
    has_pending_command = false;
    is_python_command = false;
    respond_to_uart = false;  // Only clear respond_to_uart when explicitly called
    pending_command[0] = '\0';
}

// =========================================================================
// OUTGOING: UART response buffer
// =========================================================================

size_t CommandBuffer::queueForUART(const uint8_t* data, size_t len) {
    if (!data || len == 0) {
        return 0;
    }
    
    size_t queued = 0;
    for (size_t i = 0; i < len; i++) {
        uint16_t next_write = (uart_out_write + 1) % UART_OUT_BUFFER_SIZE;
        
        if (next_write == uart_out_read) {
            // Buffer full
            break;
        }
        
        uart_out_buffer[uart_out_write] = data[i];
        uart_out_write = next_write;
        queued++;
    }
    
    bytes_queued_to_uart += queued;
    return queued;
}

size_t CommandBuffer::queueForUART(const char* str) {
    if (!str) return 0;
    return queueForUART((const uint8_t*)str, strlen(str));
}

size_t CommandBuffer::queueForUART(const String& str) {
    if (str.length() == 0) return 0;
    return queueForUART((const uint8_t*)str.c_str(), str.length());
}

bool CommandBuffer::hasOutgoingData() const {
    return uart_out_read != uart_out_write;
}

size_t CommandBuffer::outgoingAvailable() const {
    if (uart_out_write >= uart_out_read) {
        return uart_out_write - uart_out_read;
    }
    return UART_OUT_BUFFER_SIZE - uart_out_read + uart_out_write;
}

int CommandBuffer::getOutgoingByte() {
    if (uart_out_read == uart_out_write) {
        return -1;  // Empty
    }
    
    uint8_t byte = uart_out_buffer[uart_out_read];
    uart_out_read = (uart_out_read + 1) % UART_OUT_BUFFER_SIZE;
    bytes_sent_to_uart++;
    return byte;
}

int CommandBuffer::peekOutgoingByte() const {
    if (uart_out_read == uart_out_write) {
        return -1;
    }
    return uart_out_buffer[uart_out_read];
}

void CommandBuffer::clearOutgoing() {
    uart_out_read = 0;
    uart_out_write = 0;
}

