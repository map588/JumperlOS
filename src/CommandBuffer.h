// SPDX-License-Identifier: MIT
/**
 * CommandBuffer - Simple synchronous command injection system
 * 
 * This replaces the complex InjectionBufferStream/MultiSourceStream/InjectedCommandService
 * architecture with a simple, synchronous approach:
 * 
 * INCOMING (UART → Main Loop):
 *   1. AsyncPassthrough parses <j>command</j> and <p>command</p> tags
 *   2. Complete command stored in pending_j_command or pending_p_command
 *   3. Main loop checks hasPendingCommand() and processes synchronously
 *   4. No competing services, no race conditions
 * 
 * OUTGOING (Main Loop → UART):
 *   1. Command responses written to outgoing_uart_buffer
 *   2. AsyncPassthrough::task() drains buffer with proper UART pacing
 *   3. Non-blocking, respects 115200 baud timing
 * 
 * Design principles:
 *   - Single writer, single reader for each buffer
 *   - No interrupts access these buffers (IRQ only touches uartReceived ring)
 *   - All command processing on Core 0 main loop
 *   - Simple flag-based signaling (no queues)
 */

#ifndef COMMAND_BUFFER_H
#define COMMAND_BUFFER_H

#include <Arduino.h>

// Buffer sizes
#define CMD_BUFFER_SIZE 512        // Max command length
#define UART_OUT_BUFFER_SIZE 1024  // Outgoing UART buffer

/**
 * CommandBuffer - Singleton managing command injection and response routing
 */
class CommandBuffer {
public:
    static CommandBuffer& getInstance();
    
    // Delete copy/move
    CommandBuffer(const CommandBuffer&) = delete;
    CommandBuffer& operator=(const CommandBuffer&) = delete;
    
    // =========================================================================
    // INCOMING: Pending commands from UART (set by AsyncPassthrough)
    // =========================================================================
    
    /**
     * Set a pending <j> command (raw command, like typing in terminal)
     * Called by AsyncPassthrough when </j> closing tag is detected
     * @param cmd The command string (without tags)
     * @return true if accepted, false if a command is already pending
     */
    bool setPendingJCommand(const char* cmd);
    
    /**
     * Set a pending <p> command (Python command with > prefix)
     * Called by AsyncPassthrough when </p> closing tag is detected
     * @param cmd The command string (without tags, '>' will be added)
     * @return true if accepted, false if a command is already pending
     */
    bool setPendingPCommand(const char* cmd);
    
    /**
     * Check if there's a pending command to process
     */
    bool hasPendingCommand() const { return has_pending_command; }
    
    /**
     * Check if the pending command is a Python (<p>) command
     */
    bool isPythonCommand() const { return is_python_command; }
    
    /**
     * Get the pending command (does NOT consume it)
     */
    const char* getPendingCommand() const { return pending_command; }
    
    /**
     * Consume the pending command (clears it after returning)
     * @return The command string, or empty string if none
     * NOTE: Prefer consumePendingCommandPtr() to avoid heap allocation
     */
    String consumePendingCommand();
    
    /**
     * Consume and return pointer to internal buffer (zero-copy, no heap alloc)
     * The returned pointer is valid until the next setPending*Command() call.
     * @return Pointer to command string, or nullptr if none pending
     */
    const char* consumePendingCommandPtr();
    
    /**
     * Clear pending command without processing
     */
    void clearPendingCommand();
    
    // =========================================================================
    // OUTGOING: Response buffer for UART (drained by AsyncPassthrough)
    // =========================================================================
    
    /**
     * Queue data to be sent to UART
     * Called by command handlers when response should go to Arduino
     * @param data The data to send
     * @param len Length of data
     * @return Number of bytes queued (may be less than len if buffer full)
     */
    size_t queueForUART(const uint8_t* data, size_t len);
    size_t queueForUART(const char* str);
    size_t queueForUART(const String& str);
    
    /**
     * Check if there's data waiting to be sent to UART
     */
    bool hasOutgoingData() const;
    
    /**
     * Get number of bytes waiting to be sent
     */
    size_t outgoingAvailable() const;
    
    /**
     * Get a byte from the outgoing buffer (for AsyncPassthrough to send)
     * @return The byte, or -1 if buffer empty
     */
    int getOutgoingByte();
    
    /**
     * Peek at the next outgoing byte without consuming
     */
    int peekOutgoingByte() const;
    
    /**
     * Clear the outgoing buffer
     */
    void clearOutgoing();
    
    // =========================================================================
    // Response routing
    // =========================================================================
    
    /**
     * Check if current command response should go to UART
     * Set by AsyncPassthrough when command originates from UART
     */
    bool shouldRespondToUART() const { return respond_to_uart; }
    
    /**
     * Set response routing for current command
     */
    void setRespondToUART(bool value) { respond_to_uart = value; }
    
    // =========================================================================
    // Statistics (for debugging)
    // =========================================================================
    
    uint32_t getCommandsReceived() const { return commands_received; }
    uint32_t getCommandsProcessed() const { return commands_processed; }
    uint32_t getBytesQueuedToUART() const { return bytes_queued_to_uart; }
    uint32_t getBytesSentToUART() const { return bytes_sent_to_uart; }
    
    void incrementCommandsProcessed() { commands_processed++; }
    
private:
    CommandBuffer();
    
    // Pending command (incoming)
    char pending_command[CMD_BUFFER_SIZE];
    volatile bool has_pending_command;
    bool is_python_command;
    bool respond_to_uart;
    
    // Outgoing UART buffer (ring buffer)
    uint8_t uart_out_buffer[UART_OUT_BUFFER_SIZE];
    volatile uint16_t uart_out_read;
    volatile uint16_t uart_out_write;
    
    // Statistics
    uint32_t commands_received;
    uint32_t commands_processed;
    uint32_t bytes_queued_to_uart;
    uint32_t bytes_sent_to_uart;
};

// Global accessor
extern CommandBuffer& cmdBuffer;

#endif // COMMAND_BUFFER_H

