/**
 * @file oled_small_text_examples.cpp
 * @brief Practical examples for using the OLED small text display API
 * 
 * This file contains ready-to-use examples demonstrating the new
 * generalized OLED small text display functionality.
 */

#include "oled.h"
#include "Jerial.h"

// ============================================================================
// Example 1: Simple Status Display
// ============================================================================
void example_simple_status() {
    // Display system status with multiple lines
    oled.showMultiLineSmallText(
        "System Status:\n"
        "CPU: RP2350\n"
        "Memory: OK\n"
        "OLED: Ready",
        true,  // clear display first
        true   // show display after
    );
}

// ============================================================================
// Example 2: Dynamic Status with Variables
// ============================================================================
void example_dynamic_status(float temperature, int memoryFree) {
    // Build text buffer with current values
    char statusBuffer[128];
    snprintf(statusBuffer, sizeof(statusBuffer),
        "Temperature: %.1fC\n"
        "Memory: %dKB\n"
        "Status: Running",
        temperature,
        memoryFree / 1024
    );
    
    oled.showMultiLineSmallText(statusBuffer, true, true);
}

// ============================================================================
// Example 3: Menu Display
// ============================================================================
void example_menu_display(int selectedItem) {
    const char* menuItems[] = {
        "1. Start Test",
        "2. View Logs",
        "3. Settings",
        "4. Exit"
    };
    
    // Build menu text with selection indicator
    char menuBuffer[256];
    int bufPos = 0;
    
    for (int i = 0; i < 4; i++) {
        if (i == selectedItem) {
            bufPos += snprintf(menuBuffer + bufPos, sizeof(menuBuffer) - bufPos, "> ");
        } else {
            bufPos += snprintf(menuBuffer + bufPos, sizeof(menuBuffer) - bufPos, "  ");
        }
        bufPos += snprintf(menuBuffer + bufPos, sizeof(menuBuffer) - bufPos, "%s\n", menuItems[i]);
    }
    
    oled.showMultiLineSmallText(menuBuffer, true, true);
}

// ============================================================================
// Example 4: Text Editor Display with Cursor
// ============================================================================
void example_text_editor(const char* fileContent, int cursorLine, int cursorCol) {
    oled::SmallTextDisplayConfig config = {};
    config.text = fileContent;
    config.font = SMALL_FONT_ANDALE_MONO;  // Monospace for code
    config.clear_before = true;
    config.show_after = true;
    config.enable_cursor = true;
    config.cursor_line = cursorLine;
    config.cursor_col = cursorCol;
    config.start_line = 0;
    config.max_lines = 3;  // Show 3 lines
    config.horizontal_offset = 0;
    config.highlight_cursor_line = false;
    config.status_text = "Edit Mode";
    
    oled.showSmallTextBuffer(config);
}

// ============================================================================
// Example 5: Scrollable Log Viewer
// ============================================================================
void example_log_viewer(const char* logLines[], int numLines, int scrollPos, int cursorPos) {
    // Build visible portion of log
    char logBuffer[256];
    int bufPos = 0;
    
    int maxVisibleLines = 4;
    for (int i = scrollPos; i < numLines && i < scrollPos + maxVisibleLines; i++) {
        bufPos += snprintf(logBuffer + bufPos, sizeof(logBuffer) - bufPos,
                          "%s\n", logLines[i]);
    }
    
    oled::SmallTextDisplayConfig config = {};
    config.text = logBuffer;
    config.font = SMALL_FONT_PRAGMATISM_5PT;
    config.clear_before = true;
    config.show_after = true;
    config.enable_cursor = true;
    config.cursor_line = cursorPos - scrollPos;  // Relative position
    config.cursor_col = 0;
    config.start_line = 0;
    config.max_lines = maxVisibleLines;
    config.horizontal_offset = 0;
    config.highlight_cursor_line = true;  // Highlight selected line
    
    // Show line count in status
    char statusBuffer[32];
    snprintf(statusBuffer, sizeof(statusBuffer), "Log %d/%d", cursorPos + 1, numLines);
    config.status_text = statusBuffer;
    
    oled.showSmallTextBuffer(config);
}

// ============================================================================
// Example 6: Terminal Output with OLEDOut
// ============================================================================
void example_terminal_output() {
    // Configure terminal
    OLEDOut.clear();
    OLEDOut.setSmallFont(SMALL_FONT_ANDALE_MONO);
    
    // Output lines like a terminal
    OLEDOut.println("Jumperless OS v5");
    OLEDOut.println("Initializing...");
    delay(500);
    
    OLEDOut.println("USB: OK");
    delay(300);
    
    OLEDOut.println("OLED: OK");
    delay(300);
    
    OLEDOut.println("Ready.");
}

// ============================================================================
// Example 7: Progress Display
// ============================================================================
void example_progress_display(int progress, const char* operation) {
    // Build progress bar
    char progressBuffer[256];
    int bufPos = 0;
    
    bufPos += snprintf(progressBuffer + bufPos, sizeof(progressBuffer) - bufPos,
                      "%s\n", operation);
    
    // Progress bar (20 characters wide)
    bufPos += snprintf(progressBuffer + bufPos, sizeof(progressBuffer) - bufPos, "[");
    int filled = (progress * 20) / 100;
    for (int i = 0; i < 20; i++) {
        progressBuffer[bufPos++] = (i < filled) ? '#' : '-';
    }
    bufPos += snprintf(progressBuffer + bufPos, sizeof(progressBuffer) - bufPos, "]\n");
    
    bufPos += snprintf(progressBuffer + bufPos, sizeof(progressBuffer) - bufPos,
                      "%d%%", progress);
    
    oled.showMultiLineSmallText(progressBuffer, true, true);
}

// ============================================================================
// Example 8: File Browser
// ============================================================================
void example_file_browser(const char* files[], int numFiles, int selectedFile, int scrollPos) {
    char browserBuffer[256];
    int bufPos = 0;
    
    // Current directory
    bufPos += snprintf(browserBuffer + bufPos, sizeof(browserBuffer) - bufPos,
                      "/home/files\n");
    
    // Show files
    int maxVisible = 3;
    for (int i = scrollPos; i < numFiles && i < scrollPos + maxVisible; i++) {
        if (i == selectedFile) {
            bufPos += snprintf(browserBuffer + bufPos, sizeof(browserBuffer) - bufPos, "> ");
        } else {
            bufPos += snprintf(browserBuffer + bufPos, sizeof(browserBuffer) - bufPos, "  ");
        }
        bufPos += snprintf(browserBuffer + bufPos, sizeof(browserBuffer) - bufPos,
                          "%s\n", files[i]);
    }
    
    oled::SmallTextDisplayConfig config = {};
    config.text = browserBuffer;
    config.font = SMALL_FONT_PRAGMATISM_5PT;
    config.clear_before = true;
    config.show_after = true;
    config.enable_cursor = false;
    config.start_line = 0;
    config.max_lines = 4;
    
    char statusBuffer[32];
    snprintf(statusBuffer, sizeof(statusBuffer), "%d files", numFiles);
    config.status_text = statusBuffer;
    
    oled.showSmallTextBuffer(config);
}

// ============================================================================
// Example 9: Split View with Multiple Sections
// ============================================================================
void example_split_view(const char* leftText, const char* rightText) {
    // For split view, we need to manually control drawing
    oled.clearFramebuffer();
    oled.setSmallFont(SMALL_FONT_PRAGMATISM_5PT);
    
    // Left column (0-63 pixels)
    const char* line = leftText;
    int y = 8;
    int lineHeight = 8;
    while (line && *line && y < oled.displayHeight) {
        const char* nextLine = strchr(line, '\n');
        int len = nextLine ? (nextLine - line) : strlen(line);
        
        char lineBuffer[32];
        memcpy(lineBuffer, line, min(len, 10));  // Truncate to fit column
        lineBuffer[min(len, 10)] = '\0';
        
        oled.drawText(0, y, lineBuffer);
        
        y += lineHeight;
        line = nextLine ? nextLine + 1 : nullptr;
    }
    
    // Draw vertical divider
    oled.drawLine(64, 0, 64, oled.displayHeight - 1, SSD1306_WHITE);
    
    // Right column (65-127 pixels)
    line = rightText;
    y = 8;
    while (line && *line && y < oled.displayHeight) {
        const char* nextLine = strchr(line, '\n');
        int len = nextLine ? (nextLine - line) : strlen(line);
        
        char lineBuffer[32];
        memcpy(lineBuffer, line, min(len, 10));
        lineBuffer[min(len, 10)] = '\0';
        
        oled.drawText(66, y, lineBuffer);
        
        y += lineHeight;
        line = nextLine ? nextLine + 1 : nullptr;
    }
    
    oled.flushFramebuffer();
}

// ============================================================================
// Example 10: Real-time Sensor Dashboard
// ============================================================================
void example_sensor_dashboard(float temp, float voltage, int freq) {
    // Build dashboard with formatted sensor values
    char dashboardBuffer[256];
    snprintf(dashboardBuffer, sizeof(dashboardBuffer),
        "SENSORS\n"
        "Temp: %.1fC\n"
        "Volt: %.2fV\n"
        "Freq: %dHz",
        temp, voltage, freq
    );
    
    oled.showMultiLineSmallText(dashboardBuffer, true, true);
}

// ============================================================================
// Example 11: Batch Updates with Manual Flush
// ============================================================================
void example_batch_updates() {
    // Disable auto-update for efficiency
    OLEDOut.setAutoUpdate(false);
    
    // Make multiple updates
    OLEDOut.println("Update 1");
    OLEDOut.println("Update 2");
    OLEDOut.println("Update 3");
    OLEDOut.println("Update 4");
    
    // Flush all at once (more efficient)
    OLEDOut.flush();
}

// ============================================================================
// Example 12: Error Message Display
// ============================================================================
void example_error_display(const char* errorMessage, int errorCode) {
    char errorBuffer[128];
    snprintf(errorBuffer, sizeof(errorBuffer),
        "ERROR\n"
        "Code: %d\n"
        "%s",
        errorCode,
        errorMessage
    );
    
    // Use inverted display for error
    oled.invertDisplay(true);
    oled.showMultiLineSmallText(errorBuffer, true, true);
    delay(2000);
    oled.invertDisplay(false);
}

// ============================================================================
// Complete Usage Example
// ============================================================================
void complete_usage_example() {
    // Initialize display
    if (!oled.isConnected()) {
        return;
    }
    
    // Example 1: Welcome screen
    oled.showMultiLineSmallText(
        "JumperlOS v5\n"
        "Initializing...",
        true, true
    );
    delay(1000);
    
    // Example 2: System check with terminal output
    OLEDOut.clear();
    OLEDOut.println("System Check:");
    OLEDOut.println("CPU: OK");
    delay(300);
    OLEDOut.println("RAM: OK");
    delay(300);
    OLEDOut.println("Storage: OK");
    delay(1000);
    
    // Example 3: Show main menu
    char menuBuffer[128];
    snprintf(menuBuffer, sizeof(menuBuffer),
        "> Start\n"
        "  Settings\n"
        "  About\n"
        "  Exit"
    );
    oled.showMultiLineSmallText(menuBuffer, true, true);
    delay(2000);
    
    // Example 4: Show status dashboard
    char statusBuffer[128];
    snprintf(statusBuffer, sizeof(statusBuffer),
        "SYSTEM STATUS\n"
        "Temp: 25.3C\n"
        "Mem: 45KB free\n"
        "Ready"
    );
    oled.showMultiLineSmallText(statusBuffer, true, true);
}

// ============================================================================
// Helper: Convert lines array to newline-separated buffer
// ============================================================================
int lines_to_buffer(const char* lines[], int numLines, char* buffer, int bufferSize) {
    int bufPos = 0;
    for (int i = 0; i < numLines && bufPos < bufferSize - 2; i++) {
        int len = strlen(lines[i]);
        int copyLen = min(len, bufferSize - bufPos - 2);
        memcpy(buffer + bufPos, lines[i], copyLen);
        bufPos += copyLen;
        buffer[bufPos++] = '\n';
    }
    buffer[bufPos] = '\0';
    return bufPos;
}


