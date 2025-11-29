// SPDX-License-Identifier: MIT
/**
 * @file SharedBuffer.cpp
 * @brief Implementation of pre-allocated shared buffer for context data transfer
 * 
 * This provides an 8KB static buffer that allows Ekilo, Python REPL, and other
 * contexts to pass content directly without saving to flash or dynamic allocation.
 */

#include "SharedBuffer.h"
#include "externVars.h"  // For core_sync_acquire/release

// Static instance pointer (set on first call to getInstance)
SharedBuffer* SharedBuffer::instance = nullptr;

/**
 * @brief Private constructor - initializes all state
 */
SharedBuffer::SharedBuffer()
    : contentLen(0)
    , contentType(SharedBufferContentType::UNKNOWN)
    , isReadyFlag(false)
    , sourceContext(0)
{
    // Initialize the buffer with null terminator
    buffer[0] = '\0';
    filename[0] = '\0';
}

/**
 * @brief Get the singleton instance
 * 
 * Uses a function-local static - allocated in BSS, not heap.
 * This avoids heap fragmentation from the 24KB buffer.
 */
SharedBuffer& SharedBuffer::getInstance() {
    // Function-local static - Meyer's Singleton pattern
    static SharedBuffer localInstance;
    instance = &localInstance;
    return localInstance;
}

// ============================================================================
// Writing to the buffer
// ============================================================================

/**
 * @brief Clear all content and metadata
 */
void SharedBuffer::clear() {
    core_sync_acquire();
    
    contentLen = 0;
    buffer[0] = '\0';
    filename[0] = '\0';
    contentType = SharedBufferContentType::UNKNOWN;
    isReadyFlag = false;
    sourceContext = 0;
    
    core_sync_release();
}

/**
 * @brief Write data to the buffer (overwrites existing content)
 */
bool SharedBuffer::write(const uint8_t* data, size_t len) {
    if (data == nullptr) {
        return false;
    }
    
    // Check if it fits (leave room for null terminator)
    if (len >= SHARED_BUFFER_SIZE) {
        Serial.print("SharedBuffer: Data too large (");
        Serial.print(len);
        Serial.print(" > ");
        Serial.print(SHARED_BUFFER_SIZE - 1);
        Serial.println(")");
        return false;
    }
    
    core_sync_acquire();
    
    memcpy(buffer, data, len);
    contentLen = len;
    buffer[contentLen] = '\0';  // Null terminate
    
    core_sync_release();
    
    return true;
}

bool SharedBuffer::write(const char* str) {
    if (str == nullptr) {
        return false;
    }
    return write((const uint8_t*)str, strlen(str));
}

bool SharedBuffer::write(const char* data, size_t len) {
    return write((const uint8_t*)data, len);
}

/**
 * @brief Append data to existing content
 */
bool SharedBuffer::append(const uint8_t* data, size_t len) {
    if (data == nullptr || len == 0) {
        return true;  // Nothing to append is not an error
    }
    
    // Check if it fits
    if (contentLen + len >= SHARED_BUFFER_SIZE) {
        Serial.print("SharedBuffer: Append would overflow (");
        Serial.print(contentLen);
        Serial.print(" + ");
        Serial.print(len);
        Serial.print(" >= ");
        Serial.print(SHARED_BUFFER_SIZE);
        Serial.println(")");
        return false;
    }
    
    core_sync_acquire();
    
    memcpy(buffer + contentLen, data, len);
    contentLen += len;
    buffer[contentLen] = '\0';  // Null terminate
    
    core_sync_release();
    
    return true;
}

bool SharedBuffer::append(const char* str) {
    if (str == nullptr) {
        return true;
    }
    return append((const uint8_t*)str, strlen(str));
}

bool SharedBuffer::append(const char* data, size_t len) {
    return append((const uint8_t*)data, len);
}

/**
 * @brief Append a line with newline at end
 */
bool SharedBuffer::appendLine(const char* line, size_t len) {
    // Check if line + newline fits
    if (contentLen + len + 1 >= SHARED_BUFFER_SIZE) {
        Serial.print("SharedBuffer: Line would overflow (");
        Serial.print(contentLen);
        Serial.print(" + ");
        Serial.print(len);
        Serial.println(" + 1)");
        return false;
    }
    
    core_sync_acquire();
    
    // Copy line content
    if (line != nullptr && len > 0) {
        memcpy(buffer + contentLen, line, len);
        contentLen += len;
    }
    
    // Add newline
    buffer[contentLen] = '\n';
    contentLen++;
    buffer[contentLen] = '\0';  // Null terminate
    
    core_sync_release();
    
    return true;
}

bool SharedBuffer::appendLine(const char* line) {
    if (line == nullptr) {
        return appendLine("", 0);  // Just add a newline
    }
    return appendLine(line, strlen(line));
}

/**
 * @brief Append a single character
 */
bool SharedBuffer::appendChar(char c) {
    if (contentLen + 1 >= SHARED_BUFFER_SIZE) {
        return false;
    }
    
    core_sync_acquire();
    
    buffer[contentLen] = c;
    contentLen++;
    buffer[contentLen] = '\0';
    
    core_sync_release();
    
    return true;
}

// ============================================================================
// Metadata
// ============================================================================

/**
 * @brief Set the filename associated with this content
 */
void SharedBuffer::setFilename(const char* name) {
    if (name == nullptr) {
        filename[0] = '\0';
        return;
    }
    
    size_t len = strlen(name);
    if (len >= SHARED_BUFFER_FILENAME_SIZE) {
        len = SHARED_BUFFER_FILENAME_SIZE - 1;
    }
    
    memcpy(filename, name, len);
    filename[len] = '\0';
}

// ============================================================================
// Debug
// ============================================================================

/**
 * @brief Print buffer status to Serial
 */
void SharedBuffer::printStatus() const {
    Serial.println("\n=== Shared Buffer Status ===");
    Serial.print("Content length: ");
    Serial.print(contentLen);
    Serial.print(" / ");
    Serial.print(SHARED_BUFFER_SIZE);
    Serial.print(" (");
    Serial.print((contentLen * 100) / SHARED_BUFFER_SIZE);
    Serial.println("% used)");
    
    Serial.print("Remaining: ");
    Serial.print(remaining());
    Serial.println(" bytes");
    
    if (hasFilename()) {
        Serial.print("Filename: ");
        Serial.println(filename);
    }
    
    Serial.print("Content type: ");
    switch (contentType) {
        case SharedBufferContentType::UNKNOWN: Serial.println("UNKNOWN"); break;
        case SharedBufferContentType::TEXT: Serial.println("TEXT"); break;
        case SharedBufferContentType::PYTHON_SCRIPT: Serial.println("PYTHON_SCRIPT"); break;
        case SharedBufferContentType::NETLIST: Serial.println("NETLIST"); break;
        case SharedBufferContentType::JSON: Serial.println("JSON"); break;
        case SharedBufferContentType::BINARY: Serial.println("BINARY"); break;
        default: Serial.println("(invalid)"); break;
    }
    
    Serial.print("Ready: ");
    Serial.println(isReadyFlag ? "YES" : "NO");
    
    Serial.print("Source context: ");
    Serial.println(sourceContext);
    
    // Show first 100 chars of content if any
    if (contentLen > 0) {
        Serial.print("Content preview: \"");
        size_t previewLen = contentLen < 100 ? contentLen : 100;
        for (size_t i = 0; i < previewLen; i++) {
            char c = buffer[i];
            if (c == '\n') {
                Serial.print("\\n");
            } else if (c == '\r') {
                Serial.print("\\r");
            } else if (c == '\t') {
                Serial.print("\\t");
            } else if (c >= 32 && c < 127) {
                Serial.print(c);
            } else {
                Serial.print('.');
            }
        }
        if (contentLen > 100) {
            Serial.print("...");
        }
        Serial.println("\"");
    }
    
    Serial.println("============================\n");
}

