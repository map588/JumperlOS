/*
 * EkiloEditor.cpp - Arduino-compatible eKilo text editor for Jumperless
 * Based on the original eKilo editor by Antonio Foti
 * Adapted for Arduino/embedded systems by removing Unix dependencies
 */

#include "EkiloEditor.h"
#include "Graphics.h"
#include "oled.h"
#include "RotaryEncoder.h"
#include "JumperlessDefines.h"
#include "States.h"  // For SlotManager service calls
#include "externVars.h"  // For fs_mutex filesystem synchronization
#include "FilesystemStuff.h"  // For safe file operations
#include "JumperlOS.h"  // For ContextManager
#include <time.h>

// External references
extern class oled oled;

// Global editor state
static EditorConfig E;

// Track currently open file for external monitoring (e.g., SlotManager preview mode)
static char* g_currently_editing_file = nullptr;

// Key definitions
#define KEY_NULL 0
#define CTRL_C 3
#define CTRL_D 4
#define CTRL_F 6
#define CTRL_H 8
#define TAB 9
#define CTRL_L 12
#define ENTER 13
#define CTRL_P 16
#define CTRL_Q 17
#define CTRL_S 19
#define CTRL_U 21
#define ESC 27
#define BACKSPACE 127

// Virtual key codes for special keys
#define ARROW_LEFT 1000
#define ARROW_RIGHT 1001
#define ARROW_UP 1002
#define ARROW_DOWN 1003
#define DEL_KEY 1004
#define HOME_KEY 1005
#define END_KEY 1006
#define PAGE_UP 1007
#define PAGE_DOWN 1008

// Syntax highlighting
#define HL_NORMAL 0
#define HL_NONPRINT 1
#define HL_COMMENT 2
#define HL_MLCOMMENT 3
#define HL_KEYWORD1 4
#define HL_KEYWORD2 5
#define HL_STRING 6
#define HL_NUMBER 7
#define HL_MATCH 8
#define HL_JUMPERLESS_FUNC 9
#define HL_JUMPERLESS_TYPE 10
#define HL_JFS_FUNC 11

#define HL_HIGHLIGHT_STRINGS (1<<0)
#define HL_HIGHLIGHT_NUMBERS (1<<1)

// Python syntax highlighting
const char* python_extensions[] = {".py", ".pyw", ".pyi", nullptr};
const char* python_keywords[] = {
    "and", "as", "assert", "break", "class", "continue", "def", "del",
    "elif", "else", "except", "exec", "finally", "for", "from", "global",
    "if", "import", "in", "is", "lambda", "not", "or", "pass", "print",
    "raise", "return", "try", "while", "with", "yield", "async", "await",
    "nonlocal", "True", "False", "None",
    
    // Python Built-ins (marked with |)
    "abs|", "all|", "any|", "bin|", "bool|", "bytes|", "callable|",
    "chr|", "dict|", "dir|", "enumerate|", "eval|", "filter|", "float|",
    "format|", "getattr|", "globals|", "hasattr|", "hash|", "help|", "hex|",
    "id|", "input|", "int|", "isinstance|", "iter|", "len|", "list|",
    "locals|", "map|", "max|", "min|", "next|", "object|", "oct|", "open|",
    "ord|", "pow|", "print|", "range|", "repr|", "reversed|", "round|",
    "set|", "setattr|", "slice|", "sorted|", "str|", "sum|", "super|",
    "tuple|", "type|", "vars|", "zip|", "self|", "cls|",
    
    // Jumperless Functions (marked with ||)
    "dac_set||", "dac_get||", "set_dac||", "get_dac||", "adc_get||", "get_adc||",
    "ina_get_current||", "ina_get_voltage||", "ina_get_bus_voltage||", "ina_get_power||",
    "get_current||", "get_voltage||", "get_bus_voltage||", "get_power||",
    "gpio_set||", "gpio_get||", "gpio_set_dir||", "gpio_get_dir||", "gpio_set_pull||", "gpio_get_pull||",
    "set_gpio||", "get_gpio||", "set_gpio_dir||", "get_gpio_dir||", "set_gpio_pull||", "get_gpio_pull||",
    "connect||", "disconnect||", "is_connected||", "nodes_clear||", "node||",
    "oled_print||", "oled_clear||", "oled_connect||", "oled_disconnect||",
    "clickwheel_up||", "clickwheel_down||", "clickwheel_press||",
    "print_bridges||", "print_paths||", "print_crossbars||", "print_nets||", "print_chip_status||",
    "probe_read||", "read_probe||", "probe_read_blocking||", "probe_read_nonblocking||",
    "get_button||", "probe_button||", "probe_button_blocking||", "probe_button_nonblocking||",
    "probe_wait||", "wait_probe||", "probe_touch||", "wait_touch||", "button_read||", "read_button||",
    "check_button||", "button_check||", "arduino_reset||", "probe_tap||", "run_app||", "format_output||",
    "help_nodes||", "pause_core2||", "send_raw||", "pwm||", "pwm_set_duty_cycle||", "pwm_set_frequency||", "pwm_stop||", "nodes_save||",
    
    // Wavegen Functions (marked with ||)
    "wavegen_set_output||", "set_wavegen_output||",
    "wavegen_set_freq||", "set_wavegen_freq||",
    "wavegen_set_wave||", "set_wavegen_wave||",
    "wavegen_set_sweep||", "set_wavegen_sweep||",
    "wavegen_set_amplitude||", "set_wavegen_amplitude||",
    "wavegen_set_offset||", "set_wavegen_offset||",
    "wavegen_start||", "start_wavegen||",
    "wavegen_stop||", "stop_wavegen||",
    "wavegen_get_output||", "get_wavegen_output||",
    "wavegen_get_freq||", "get_wavegen_freq||",
    "wavegen_get_wave||", "get_wavegen_wave||",
    "wavegen_get_amplitude||", "get_wavegen_amplitude||",
    "wavegen_get_offset||", "get_wavegen_offset||",
    "wavegen_is_running||",
    
    // Jumperless Types/Constants (marked with |||)
    "TOP_RAIL|||", "BOTTOM_RAIL|||", "GND|||", "DAC0|||", "DAC1|||", "ADC0|||", "ADC1|||", "ADC2|||", "ADC3|||", "ADC4|||",
    "PROBE|||", "ISENSE_PLUS|||", "ISENSE_MINUS|||", "UART_TX|||", "UART_RX|||", "BUFFER_IN|||", "BUFFER_OUT|||",
    "GPIO_1|||", "GPIO_2|||", "GPIO_3|||", "GPIO_4|||", "GPIO_5|||", "GPIO_6|||", "GPIO_7|||", "GPIO_8|||",
    "D0|||", "D1|||", "D2|||", "D3|||", "D4|||", "D5|||", "D6|||", "D7|||", "D8|||", "D9|||", "D10|||", "D11|||", "D12|||", "D13|||",
    "A0|||", "A1|||", "A2|||", "A3|||", "A4|||", "A5|||", "A6|||", "A7|||",
    "D13_PAD|||", "TOP_RAIL_PAD|||", "BOTTOM_RAIL_PAD|||", "LOGO_PAD_TOP|||", "LOGO_PAD_BOTTOM|||",
    "CONNECT_BUTTON|||", "REMOVE_BUTTON|||", "BUTTON_NONE|||", "CONNECT|||", "REMOVE|||", "NONE|||",
    
    // Wavegen constants (marked with |||)
    "SINE|||", "TRIANGLE|||", "SAWTOOTH|||", "SQUARE|||", "RAMP|||", "ARBITRARY|||",
    
    // JFS Functions (marked with ||||)
    "open||||", "read||||", "write||||", "close||||", "seek||||", "tell||||", "size||||", "available||||",
    "exists||||", "listdir||||", "mkdir||||", "rmdir||||", "remove||||", "rename||||", "stat||||", "info||||",
    "SEEK_SET||||", "SEEK_CUR||||", "SEEK_END||||", "nodes_save||||", 
    
    // Basic filesystem functions (marked with ||||)
    "fs_exists||||", "fs_listdir||||", "fs_read||||", "fs_write||||", "fs_cwd||||",
    
    nullptr
};


SyntaxDefinition syntax_db[] = {
    {
        python_extensions,
        python_keywords,
        "#", "", "",  // Single line comment, no multi-line for Python
        HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS
    }
};

// Editor row structure
typedef struct EditorRow {
    int idx;
    int size;
    int rsize;
    char* chars;
    char* render;
    unsigned char* hl;
    int hl_oc;
} EditorRow;

EditorConfig::EditorConfig() {
    cx = cy = 0;
    rowoff = coloff = 0;
    screenrows = DEFAULT_EDITOR_ROWS;  // Use configurable default
    screencols = DEFAULT_EDITOR_COLS;
    numrows = 0;
    row_capacity = 0;  // Will grow exponentially to reduce fragmentation
    row = nullptr;
    dirty = 0;
    filename = nullptr;
    syntax = nullptr;
    should_quit = 0;
    strcpy(statusmsg, "");
    statusmsg_time = 0;
    
    // Initialize OLED horizontal scrolling
    oled_horizontal_offset = 0;
    
    // Initialize OLED batching
    oled_last_input_time = 0;
    oled_update_pending = false;
    
    // Initialize clickwheel character selection
    char_selection_mode = false;
    selected_char_index = 0;
    char_selection_timer = 0;
    
    // Initialize encoder position tracking
    last_encoder_position = 0;
    last_encoder_update = 0;
    
    // Initialize button state tracking
    last_button_state = true; // Button is active low
    button_debounce_time = 0;
    
    // Initialize menu navigation
    in_menu_mode = false;
    menu_selection = 0;
    
    // Initialize REPL mode
    repl_mode = false;
    original_cursor_row = 0;
    original_cursor_col = 0;
    start_row = 0;
    lines_used = 0;
    saved_file_content = "";
    
    // Initialize Ctrl+P functionality
    should_launch_repl = false;
    
    // Initialize screen refresh optimization
    screen_dirty = true; // Start with dirty screen to force initial draw
    
    // Initialize chunked file loading
    is_chunked = false;
    total_file_lines = 0;
    chunk_start = 0;
    chunk_loaded_lines = 0;
    chunked_filename = "";
    chunk_dirty = false;
    
    // Initialize status message history
    status_history_head = 0;
    status_history_count = 0;
    for (int i = 0; i < STATUS_HISTORY_SIZE; i++) {
        status_history[i][0] = '\0';
    }
    
    // Initialize read-only and low memory modes
    read_only = false;
    low_memory_mode = false;
}

EditorConfig::~EditorConfig() {
    // Cleanup will be handled by the editor
}

// Initialize the editor
void ekilo_init() {
    E.cx = 0;
    E.cy = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.numrows = 0;
    E.row_capacity = 0;
    E.row = nullptr;
    E.dirty = 0;
    E.filename = nullptr;
    E.syntax = nullptr;
    E.should_quit = 0;
    strcpy(E.statusmsg, "");
    E.statusmsg_time = 0;
    Jerial.write(0x0E);
    Jerial.flush();
    
    // Clear any pending input that might be CPR garbage from TUI or other sources
    // Do this FIRST before anything else
    delay(50);  // Wait for any in-flight CPR responses to arrive
    while (Jerial.available()) {
        Jerial.read();
    }
    delay(10);  // Brief extra wait
    while (Jerial.available()) {
        Jerial.read();
    }
    
    // Use conservative defaults - terminal size probing disabled due to 
    // CPR response timing issues causing garbage output and crashes
    E.screenrows = DEFAULT_EDITOR_ROWS - 1; // Account for help header
    E.screencols = DEFAULT_EDITOR_COLS;
    
    // Mark screen as dirty for initial draw
    E.screen_dirty = true;
}

// Memory management for dynamic buffer with capacity tracking
// Uses exponential growth to reduce realloc fragmentation
struct Buffer {
    char* b;
    int len;
    int capacity;  // Track capacity to avoid realloc on every append
};

// Dynamic memory allocation - use what's available
#define MIN_FREE_HEAP (1536)      // Keep only 1.5KB reserved for system
#define CRITICAL_FREE_HEAP (512)  // Emergency threshold

bool check_memory_available(size_t needed) {
    size_t freeHeap = rp2040.getFreeHeap();
    // Allow allocation if we have enough for what's needed plus a small buffer
    return (freeHeap > needed + MIN_FREE_HEAP);
}

// More aggressive check for critical operations
bool check_memory_critical(size_t needed) {
    size_t freeHeap = rp2040.getFreeHeap();
    return (freeHeap > needed + CRITICAL_FREE_HEAP);
}

// Optimized buffer append with exponential growth
// This dramatically reduces heap fragmentation during screen refresh
void buffer_append(Buffer* ab, const char* s, int len) {
    if (!ab || !s || len <= 0) return;
    
    // Check if we need to grow
    if (ab->len + len > ab->capacity) {
        // Exponential growth: start at 4KB, then double
        // 4KB is enough for a typical 35x80 screen with escape codes
        int new_capacity = (ab->capacity == 0) ? 4096 : ab->capacity * 2;
        
        // Ensure we have enough for this append
        while (new_capacity < ab->len + len) {
            new_capacity *= 2;
        }
        
        // Cap at reasonable max to avoid over-allocation
        if (new_capacity > 32768) new_capacity = ab->len + len + 1024;
        
        char* new_buf = (char*)realloc(ab->b, new_capacity);
        if (new_buf == nullptr) {
            // Try minimal growth as fallback
            new_capacity = ab->len + len + 64;
            new_buf = (char*)realloc(ab->b, new_capacity);
            if (new_buf == nullptr) {
                return;  // Failed silently
            }
        }
        ab->b = new_buf;
        ab->capacity = new_capacity;
    }
    
    memcpy(ab->b + ab->len, s, len);
    ab->len += len;
}

void buffer_free(Buffer* ab) {
    if (!ab) return;
    free(ab->b);
    ab->b = nullptr;
    ab->len = 0;
    ab->capacity = 0;
}

// ============================================================================
// Input Handler Implementation - Batches keys and provides acceleration
// ============================================================================

InputHandler g_input_handler;

void InputHandler::init() {
    last_key = 0;
    last_category = KEY_CAT_NONE;
    last_key_time = 0;
    repeat_start_time = 0;
    repeat_count = 0;
}

KeyCategory InputHandler::categorize(int key) {
    switch (key) {
        case ARROW_UP:    return KEY_CAT_UP;
        case ARROW_DOWN:  return KEY_CAT_DOWN;
        case ARROW_LEFT:  return KEY_CAT_LEFT;
        case ARROW_RIGHT: return KEY_CAT_RIGHT;
        case PAGE_UP:     return KEY_CAT_PAGEUP;
        case PAGE_DOWN:   return KEY_CAT_PAGEDOWN;
        default:          return KEY_CAT_OTHER;
    }
}

bool InputHandler::shouldAccelerate() {
    if (repeat_count < 3) return false;  // Need at least 3 repeats
    uint32_t now = millis();
    uint32_t held_time = now - repeat_start_time;
    uint32_t since_last = now - last_key_time;
    
    // Reset acceleration if gap too long (key was released and re-pressed)
    if (since_last > ACCEL_TIMEOUT_MS) {
        return false;
    }
    
    return held_time > ACCEL_START_MS;
}

int InputHandler::processInput(Stream* stream, int& moves) {
    moves = 1;  // Default: single move
    uint32_t now = millis();
    
    // Check if we should reset state (key released)
    if (last_category != KEY_CAT_NONE && last_category != KEY_CAT_OTHER) {
        uint32_t gap = now - last_key_time;
        if (gap > REPEAT_THRESHOLD_MS) {
            // Key was released - aggressively clear queue and reset state
            clearRepeats(stream);
            last_category = KEY_CAT_NONE;
            repeat_count = 0;
            repeat_start_time = 0;
        }
    }
    
    if (!stream->available()) {
        return 0;
    }
    
    // Read the first key
    int first_key = ekilo_read_key();
    if (first_key == 0) return 0;
    
    KeyCategory cat = categorize(first_key);
    
    // Check if this is a continuation of same direction within timeout
    if (cat != KEY_CAT_OTHER && cat == last_category) {
        uint32_t since_last = now - last_key_time;
        if (since_last < ACCEL_TIMEOUT_MS) {
            repeat_count++;
        } else {
            // Gap too long - treat as new press, reset acceleration
            repeat_start_time = now;
            repeat_count = 1;
        }
    } else {
        // New direction or non-repeatable key
        repeat_start_time = now;
        repeat_count = 1;
    }
    
    last_key = first_key;
    last_category = cat;
    last_key_time = now;
    
    // For non-directional keys, just return immediately
    if (cat == KEY_CAT_OTHER) {
        return first_key;
    }
    
    // For directional keys, batch only a few pending same-direction keys
    int batch_count = 1;
    while (stream->available() && batch_count < MAX_BATCH_SIZE) {
        int peek_key = ekilo_read_key();
        if (peek_key == 0) break;
        
        KeyCategory peek_cat = categorize(peek_key);
        if (peek_cat == cat) {
            // Same direction - batch it
            batch_count++;
            repeat_count++;
        } else {
            // Different key - stop batching
            break;
        }
    }
    
    // Apply conservative acceleration if key is genuinely held
    if (shouldAccelerate()) {
        moves = batch_count * ACCEL_MULTIPLIER;
        if (moves > 8) moves = 8;  // Strict cap
    } else {
        moves = batch_count;
        if (moves > 3) moves = 3;  // Limit even without acceleration
    }
    
    return first_key;
}

void InputHandler::clearRepeats(Stream* stream) {
    // Aggressively clear ALL pending directional keys from the buffer
    // This prevents "runaway" cursor when key is released
    if (last_category == KEY_CAT_NONE || last_category == KEY_CAT_OTHER) return;
    
    int cleared = 0;
    while (stream->available() && cleared < 100) {  // Clear up to 100 pending keys
        int peek = ekilo_read_key();
        if (peek == 0) break;
        
        KeyCategory peek_cat = categorize(peek);
        if (peek_cat != KEY_CAT_OTHER) {
            // Any directional key - discard it (not just same direction)
            cleared++;
        } else {
            // Non-directional key - stop clearing, this one is important
            break;
        }
    }
}

// Memory monitoring and cleanup functions
void ekilo_show_memory_status() {
    size_t freeHeap = rp2040.getFreeHeap();
    size_t usedMemory = 0;
    
    // Calculate memory used by editor content
    for (int i = 0; i < E.numrows; i++) {
        if (E.row[i].chars) usedMemory += E.row[i].size;
        if (E.row[i].render) usedMemory += E.row[i].rsize;
        if (E.row[i].hl) usedMemory += E.row[i].rsize;
    }
    
    ekilo_set_status_message("Memory: %dKB free, %dKB used by editor", 
                           freeHeap / 1024, usedMemory / 1024);
}

// Emergency cleanup function
void ekilo_emergency_cleanup() {
    // Free all allocated memory to prevent further crashes
    for (int i = 0; i < E.numrows; i++) {
        ekilo_free_row(&E.row[i]);
    }
    free(E.row);
    E.row = nullptr;
    E.numrows = 0;
    E.row_capacity = 0;
    
    free(E.filename);
    E.filename = nullptr;
    
    ekilo_set_status_message("Emergency cleanup completed - editor reset");
}

// External monitoring - get currently editing file
const char* ekilo_get_currently_editing_file() {
    return g_currently_editing_file;
}

// External monitoring - get current buffer content for live preview
String ekilo_get_current_buffer_content() {
    if (E.numrows == 0) {
        return String("");
    }
    
    // Build string from all rows
    String content;
    content.reserve(E.numrows * 80);  // Pre-allocate reasonable size
    
    for (int i = 0; i < E.numrows; i++) {
        if (E.row[i].chars) {
            content += String(E.row[i].chars);
        }
        if (i < E.numrows - 1) {
            content += '\n';  // Add newline between rows
        }
    }
    
    return content;
}

// Track if we recently sent a CPR query (to filter late responses)
uint32_t last_cpr_query_time = 0;  // Non-static so probe function can update it
static const uint32_t CPR_RESPONSE_WINDOW = 1000;  // 1 second window for late responses

// Check if input looks like a stray CPR response remnant (digits;digitsR)
// Only call this when we have reason to believe CPR garbage might be present
static bool consume_stray_cpr() {
    // Only check if we're within the CPR response window
    if (millis() - last_cpr_query_time > CPR_RESPONSE_WINDOW) {
        return false;
    }
    
    // Look for pattern: digit(s) ; digit(s) R
    if (!Jerial.available()) return false;
    
    char first = Jerial.peek();
    if (first < '0' || first > '9') return false;
    
    // Looks like it might be CPR remnant - consume it
    char buf[20];
    int n = 0;
    
    while (Jerial.available() && n < 15) {
        char c = Jerial.read();
        buf[n++] = c;
        if (c == 'R') {
            // Definitely was a CPR - we consumed it
            return true;
        }
        if (c != ';' && (c < '0' || c > '9')) {
            // Not CPR pattern - stop, but we've already consumed some chars
            break;
        }
    }
    
    return n > 0;  // We consumed something
}

// Arduino-compatible key reading
int ekilo_read_key() {
    // First, check for and consume any stray CPR responses
    while (consume_stray_cpr()) {
        // Keep consuming until buffer is clean
    }
    
    if (!Jerial.available()) return 0;
    
    char c = Jerial.read();
    
    // Handle escape sequences (arrow keys, etc.)
    if (c == ESC) {
        // Wait a bit for the rest of the sequence
        unsigned long start = millis();
        // while (millis() - start < 12 && !Jerial.available()) {
        //    // delayMicroseconds(1);
        // }
        
        if (!Jerial.available()) return ESC;
        
        char seq[3];
        seq[0] = Jerial.read();
        if (!Jerial.available()) return ESC;
        seq[1] = Jerial.read();
        
        if (seq[0] == '[') {
            // Check if this is a CPR response (ESC [ digits ; digits R)
            if (seq[1] >= '0' && seq[1] <= '9') {
                // Could be CPR or function key - read more
                char buf[20];
                int n = 0;
                buf[n++] = seq[1];
                
                while (Jerial.available() && n < 15) {
                    char next = Jerial.read();
                    buf[n++] = next;
                    if (next == 'R') {
                        // This is a CPR response - discard it
                        return 0;
                    }
                    if (next == '~') {
                        // This is a function key sequence
                        switch (seq[1]) {
                            case '1': return HOME_KEY;
                            case '3': return DEL_KEY;
                            case '4': return END_KEY;
                            case '5': return PAGE_UP;
                            case '6': return PAGE_DOWN;
                            case '7': return HOME_KEY;
                            case '8': return END_KEY;
                        }
                        return ESC;
                    }
                    if (next != ';' && (next < '0' || next > '9')) {
                        break;  // Unknown sequence
                    }
                }
                return ESC;
            } else {
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }
        return ESC;
    } else {
        return c;
    }
}

// Check if character is a separator for syntax highlighting
int is_separator(int c) {
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != nullptr;
}

// Update syntax highlighting for a row
void ekilo_update_syntax(EditorRow* row) {
    if (!row || row->rsize <= 0) return;
    
    unsigned char* new_hl = (unsigned char*)realloc(row->hl, row->rsize);
    if (!new_hl) {
        // If realloc fails, we can still function without syntax highlighting
        free(row->hl);
        row->hl = nullptr;
        return;
    }
    row->hl = new_hl;
    memset(row->hl, HL_NORMAL, row->rsize);
    
    if (E.syntax == nullptr) return;
    
    const char** keywords = E.syntax->keywords;
    const char* scs = E.syntax->singleline_comment_start;
    const char* mcs = E.syntax->multiline_comment_start;
    const char* mce = E.syntax->multiline_comment_end;
    
    int scs_len = scs ? strlen(scs) : 0;
    int mcs_len = mcs ? strlen(mcs) : 0;
    int mce_len = mce ? strlen(mce) : 0;
    
    int prev_sep = 1;
    int in_string = 0;
    int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_oc);
    
    int i = 0;
    while (i < row->rsize) {
        char c = row->render[i];
        unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;
        
        if (scs_len && !in_string && !in_comment) {
            if (!strncmp(&row->render[i], scs, scs_len)) {
                memset(&row->hl[i], HL_COMMENT, row->rsize - i);
                break;
            }
        }
        
        if (mcs_len && mce_len && !in_string) {
            if (in_comment) {
                row->hl[i] = HL_MLCOMMENT;
                if (!strncmp(&row->render[i], mce, mce_len)) {
                    memset(&row->hl[i], HL_MLCOMMENT, mce_len);
                    i += mce_len;
                    in_comment = 0;
                    prev_sep = 1;
                    continue;
                } else {
                    i++;
                    continue;
                }
            } else if (!strncmp(&row->render[i], mcs, mcs_len)) {
                memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
                i += mcs_len;
                in_comment = 1;
                continue;
            }
        }
        
        if (E.syntax->flags & HL_HIGHLIGHT_STRINGS) {
            if (in_string) {
                row->hl[i] = HL_STRING;
                if (c == '\\' && i + 1 < row->rsize) {
                    row->hl[i + 1] = HL_STRING;
                    i += 2;
                    continue;
                }
                if (c == in_string) in_string = 0;
                i++;
                prev_sep = 1;
                continue;
            } else {
                if (c == '"' || c == '\'') {
                    in_string = c;
                    row->hl[i] = HL_STRING;
                    i++;
                    continue;
                }
            }
        }
        
        if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
            if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) ||
                (c == '.' && prev_hl == HL_NUMBER)) {
                row->hl[i] = HL_NUMBER;
                i++;
                prev_sep = 0;
                continue;
            }
        }
        
        if (prev_sep) {
            int j;
            for (j = 0; keywords[j]; j++) {
                int klen = strlen(keywords[j]);
                int highlight_type = HL_KEYWORD1;
                
                // Check for different keyword types based on suffix
                if (klen >= 4 && !strncmp(&keywords[j][klen - 4], "||||", 4)) {
                    highlight_type = HL_JFS_FUNC;
                    klen -= 4;
                } else if (klen >= 3 && !strncmp(&keywords[j][klen - 3], "|||", 3)) {
                    highlight_type = HL_JUMPERLESS_TYPE;
                    klen -= 3;
                } else if (klen >= 2 && !strncmp(&keywords[j][klen - 2], "||", 2)) {
                    highlight_type = HL_JUMPERLESS_FUNC;
                    klen -= 2;
                } else if (klen >= 1 && keywords[j][klen - 1] == '|') {
                    highlight_type = HL_KEYWORD2;
                    klen--;
                }
                
                if (!strncmp(&row->render[i], keywords[j], klen) &&
                    is_separator(row->render[i + klen])) {
                    memset(&row->hl[i], highlight_type, klen);
                    i += klen;
                    break;
                }
            }
            if (keywords[j] != nullptr) {
                prev_sep = 0;
                continue;
            }
        }
        
        prev_sep = is_separator(c);
        i++;
    }
    
    int changed = (row->hl_oc != in_comment);
    row->hl_oc = in_comment;
    if (changed && row->idx + 1 < E.numrows)
        ekilo_update_syntax(&E.row[row->idx + 1]);
}

// Convert syntax highlighting to ANSI color codes (256-color mode)
int ekilo_syntax_to_color(int hl) {
    switch (hl) {
        case HL_COMMENT: return 34; // Green
        case HL_MLCOMMENT: return 244; // Light gray (much more subtle than bright cyan)
        case HL_KEYWORD1: return 214;  // Orange (vibrant and distinct)
        case HL_KEYWORD2: return 79;   // Forest green (rich green for built-ins)
        case HL_STRING: return 39;    
        case HL_NUMBER: return 199;    
        case HL_MATCH: return 27;      
        case HL_JUMPERLESS_FUNC: return 207;  
        case HL_JUMPERLESS_TYPE: return 105; 
        case HL_JFS_FUNC: return 45;   // Cyan-blue (distinct for filesystem functions)
        default: return 255;           // Bright white (default text)
    }
}

// Select syntax highlighting based on filename
void ekilo_select_syntax_highlight(const char* filename) {
    E.syntax = nullptr;
    if (filename == nullptr) return;
    
    const char* ext = strrchr(filename, '.');
    if (ext == nullptr) return;
    
    for (unsigned int j = 0; j < sizeof(syntax_db) / sizeof(syntax_db[0]); j++) {
        SyntaxDefinition* s = &syntax_db[j];
        for (int i = 0; s->filematch[i]; i++) {
            int is_ext = (s->filematch[i][0] == '.');
            if ((is_ext && !strcmp(ext, s->filematch[i])) ||
                (!is_ext && strstr(filename, s->filematch[i]))) {
                E.syntax = s;
                
                // Re-highlight all rows
                for (int filerow = 0; filerow < E.numrows; filerow++) {
                    ekilo_update_syntax(&E.row[filerow]);
                }
                return;
            }
        }
    }
}

// Convert tabs to spaces and calculate render string
void ekilo_update_row(EditorRow* row) {
    if (!row) return;
    
    // In low memory mode, skip render buffer entirely - use chars directly
    if (E.low_memory_mode) {
        free(row->render);
        row->render = nullptr;
        row->rsize = row->size;  // Display will use chars directly
        free(row->hl);
        row->hl = nullptr;
        return;
    }
    
    int tabs = 0;
    for (int j = 0; j < row->size; j++) {
        if (row->chars[j] == '\t') tabs++;
    }
    
    size_t needed_size = row->size + tabs * 7 + 1;
    
    free(row->render);
    row->render = (char*)malloc(needed_size);
    if (!row->render) {
        // Silently fail - we can still edit, just no fancy rendering
        row->rsize = 0;
        return;
    }
    
    int idx = 0;
    for (int j = 0; j < row->size; j++) {
        if (row->chars[j] == '\t') {
            row->render[idx++] = ' ';
            while (idx % 8 != 0) row->render[idx++] = ' ';
        } else {
            row->render[idx++] = row->chars[j];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;
    
    ekilo_update_syntax(row);
}

// Insert a row at the specified position
void ekilo_insert_row(int at, const char* s, size_t len) {
    if (at < 0 || at > E.numrows) return;
    if (!s) return;
    
    // Just check if we have critical memory available - be aggressive about using what we have
    size_t freeHeap = rp2040.getFreeHeap();
    size_t needed = sizeof(EditorRow) + len + 64;  // Row struct + content + small buffer
    
    if (freeHeap < needed + CRITICAL_FREE_HEAP) {
        ekilo_set_status_message("ERROR: Low memory (%dKB free, need %d bytes)", freeHeap / 1024, needed);
        return;
    }
    
    // Use exponential growth strategy to reduce realloc fragmentation
    // Only realloc when we exceed capacity, doubling each time
    if (E.numrows >= E.row_capacity) {
        int new_capacity = (E.row_capacity == 0) ? 32 : E.row_capacity * 2;
        
        // Cap at reasonable maximum to avoid over-allocation
        if (new_capacity > 10000) new_capacity = E.numrows + 100;
        
        EditorRow* new_rows = (EditorRow*)realloc(E.row, sizeof(EditorRow) * new_capacity);
        if (!new_rows) {
            // Try smaller growth if doubling fails
            new_capacity = E.numrows + 16;
            new_rows = (EditorRow*)realloc(E.row, sizeof(EditorRow) * new_capacity);
            if (!new_rows) {
                ekilo_set_status_message("ERROR: Memory allocation failed for row array");
                return;
            }
        }
        E.row = new_rows;
        E.row_capacity = new_capacity;
    }
    
    memmove(&E.row[at + 1], &E.row[at], sizeof(EditorRow) * (E.numrows - at));
    for (int j = at + 1; j <= E.numrows; j++) E.row[j].idx++;
    
    E.row[at].idx = at;
    E.row[at].size = len;
    E.row[at].chars = (char*)malloc(len + 1);
    if (!E.row[at].chars) {
        ekilo_set_status_message("ERROR: Memory allocation failed for row content");
        // Revert the row array changes
        memmove(&E.row[at], &E.row[at + 1], sizeof(EditorRow) * (E.numrows - at));
        for (int j = at; j < E.numrows; j++) E.row[j].idx--;
        return;
    }
    
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    
    E.row[at].rsize = 0;
    E.row[at].render = nullptr;
    E.row[at].hl = nullptr;
    E.row[at].hl_oc = 0;
    ekilo_update_row(&E.row[at]);
    
    E.numrows++;
    E.dirty++;
}

// Free a row's memory
void ekilo_free_row(EditorRow* row) {
    free(row->render);
    free(row->chars);
    free(row->hl);
}

// Delete a row
void ekilo_del_row(int at) {
    if (at < 0 || at >= E.numrows) return;
    
    ekilo_free_row(&E.row[at]);
    memmove(&E.row[at], &E.row[at + 1], sizeof(EditorRow) * (E.numrows - at - 1));
    for (int j = at; j < E.numrows - 1; j++) E.row[j].idx--;
    E.numrows--;
    E.dirty++;
}

// Insert character in a row
void ekilo_row_insert_char(EditorRow* row, int at, int c) {
    if (!row || at < 0 || at > row->size) {
        if (row && (at < 0 || at > row->size)) at = row->size;
        else return;
    }
    
    char* new_chars = (char*)realloc(row->chars, row->size + 2);
    if (!new_chars) {
        // Show error but don't spam
        static uint32_t last_error_time = 0;
        if (millis() - last_error_time > 2000) {
            ekilo_set_status_message("ERROR: Low memory - can't insert");
            last_error_time = millis();
        }
        return;
    }
    row->chars = new_chars;
    
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    row->chars[at] = c;
    ekilo_update_row(row);
    E.dirty++;
}

// Delete character from a row
void ekilo_row_del_char(EditorRow* row, int at) {
    if (at < 0 || at >= row->size) return;
    
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    ekilo_update_row(row);
    E.dirty++;
}

// Append string to a row
void ekilo_row_append_string(EditorRow* row, char* s, size_t len) {
    if (!row || !s || len == 0) return;
    
    char* new_chars = (char*)realloc(row->chars, row->size + len + 1);
    if (!new_chars) {
        ekilo_set_status_message("ERROR: Low memory - can't append");
        return;
    }
    row->chars = new_chars;
    
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    ekilo_update_row(row);
    E.dirty++;
}

// Insert character at cursor
void ekilo_insert_char(int c) {
    if (E.cy == E.numrows) {
        ekilo_insert_row(E.numrows, "", 0);
    }
    ekilo_row_insert_char(&E.row[E.cy], E.cx, c);
    E.cx++;
    
    // Schedule OLED update after character insertion
    ekilo_schedule_oled_update();
    
    // Mark screen as dirty for refresh
    E.screen_dirty = true;
}

// Insert newline
void ekilo_insert_newline() {
    if (E.cx == 0) {
        ekilo_insert_row(E.cy, "", 0);
    } else {
        EditorRow* row = &E.row[E.cy];
        ekilo_insert_row(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
        row = &E.row[E.cy];
        row->size = E.cx;
        row->chars[row->size] = '\0';
        ekilo_update_row(row);
        
        // When splitting a line, also update the new row to ensure proper syntax highlighting
        if (E.cy + 1 < E.numrows) {
            ekilo_update_row(&E.row[E.cy + 1]);
            
            // Update syntax highlighting for subsequent rows since line split can affect
            // multiline comments and other context-dependent highlighting
            for (int i = E.cy + 2; i < E.numrows; i++) {
                ekilo_update_row(&E.row[i]);
            }
        }
    }
    E.cy++;
    E.cx = 0;
    
    // Reset horizontal scrolling when moving to new line
    E.oled_horizontal_offset = 0;
    
    // Schedule OLED update after newline insertion
    ekilo_schedule_oled_update();
    
    // Mark screen as dirty for refresh
    E.screen_dirty = true;
}

// Delete character
void ekilo_del_char() {
    if (E.cy == E.numrows) return;
    if (E.cx == 0 && E.cy == 0) return;
    
    EditorRow* row = &E.row[E.cy];
    if (E.cx > 0) {
        ekilo_row_del_char(row, E.cx - 1);
        E.cx--;
    } else {
        // Joining lines - need to update syntax highlighting for subsequent lines
        E.cx = E.row[E.cy - 1].size;
        ekilo_row_append_string(&E.row[E.cy - 1], row->chars, row->size);
        ekilo_del_row(E.cy);
        E.cy--;
        // Reset horizontal scrolling when moving to previous line
        E.oled_horizontal_offset = 0;
        
        // When joining lines, update syntax highlighting for subsequent rows since 
        // line joining can affect multiline comments and other context-dependent highlighting
        for (int i = E.cy; i < E.numrows; i++) {
            ekilo_update_row(&E.row[i]);
        }
    }
    
    // Schedule OLED update after character deletion
    ekilo_schedule_oled_update();
    
    // Mark screen as dirty for refresh
    E.screen_dirty = true;
}

// Set status message and add to history
void ekilo_set_status_message(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = millis();
    
    // Add to status history ring buffer (skip duplicates of last message)
    int prev_idx = (E.status_history_head - 1 + EditorConfig::STATUS_HISTORY_SIZE) % EditorConfig::STATUS_HISTORY_SIZE;
    if (E.status_history_count == 0 || strcmp(E.status_history[prev_idx], E.statusmsg) != 0) {
        strncpy(E.status_history[E.status_history_head], E.statusmsg, sizeof(E.status_history[0]) - 1);
        E.status_history[E.status_history_head][sizeof(E.status_history[0]) - 1] = '\0';
        E.status_history_head = (E.status_history_head + 1) % EditorConfig::STATUS_HISTORY_SIZE;
        if (E.status_history_count < EditorConfig::STATUS_HISTORY_SIZE) {
            E.status_history_count++;
        }
    }
    
    // Make error messages persist much longer (30 seconds vs 5 seconds)
    if (strstr(E.statusmsg, "ERROR:") || strstr(E.statusmsg, "WARNING:")) {
        E.statusmsg_time = millis() - 25000; // Show for 30 seconds instead of 5
    }
    
    // Mark screen as dirty for refresh
    E.screen_dirty = true;
}

// Move cursor based on key
void ekilo_move_cursor(int key) {
    EditorRow* row = (E.cy >= E.numrows) ? nullptr : &E.row[E.cy];
    
    switch (key) {
        case ARROW_LEFT:
            if (E.cx != 0) {
                E.cx--;
            } else if (E.cy > 0) {
                E.cy--;
                E.cx = E.row[E.cy].size;
            }
            break;
        case ARROW_RIGHT:
            if (row && E.cx < row->size) {
                E.cx++;
            } else if (row && E.cx == row->size) {
                E.cy++;
                E.cx = 0;
            }
            break;
        case ARROW_UP:
            if (E.cy != 0) {
                E.cy--;
                // Reset horizontal scrolling when changing lines
                E.oled_horizontal_offset = 0;
                
                // Auto-scroll if cursor moves above visible area
                if (E.cy < E.rowoff) {
                    E.rowoff = E.cy;
                }
            }
            break;
        case ARROW_DOWN:
            if (E.cy < E.numrows) {
                E.cy++;
                // Reset horizontal scrolling when changing lines
                E.oled_horizontal_offset = 0;
                
                // Auto-scroll if cursor moves below visible area
                int available_rows = E.repl_mode ? E.screenrows - 3 : E.screenrows - 4;
                if (E.cy >= E.rowoff + available_rows) {
                    E.rowoff = E.cy - available_rows + 1;
                }
            }
            break;
    }
    
    row = (E.cy >= E.numrows) ? nullptr : &E.row[E.cy];
    int rowlen = row ? row->size : 0;
    if (E.cx > rowlen) {
        E.cx = rowlen;
    }
    
    // Schedule OLED update when cursor moves
    ekilo_schedule_oled_update();
    
    
    // Mark screen as dirty for refresh
    E.screen_dirty = true;
}

// Open file
int ekilo_open(const char* filename) {
    if (!filename) return -1;

    // Brief delay to ensure any recent file operations have completed
    // This helps when opening files just created by MicroPython
    delay(2);
    yield();
    
    // Get file size first using safe function (handles mutex internally)
    int32_t file_size = safeFileSize(filename, 2000);
    if (file_size < 0) {
        // Retry after a brief delay (file might still be syncing)
        delay(50);
        file_size = safeFileSize(filename, 2000);
        if (file_size < 0) {
            ekilo_set_status_message("ERROR: Cannot open file '%s'", filename);
            return -1;
        }
    }
    
    // Validate file size to catch corruption
    if (file_size > 100000) {  // 100KB sanity limit
        ekilo_set_status_message("ERROR: File too large (%d KB)", file_size / 1024);
        return -1;
    }
    
    // For large files, use chunked loading
    if ((size_t)file_size > EditorConfig::CHUNK_THRESHOLD) {
        return ekilo_open_chunked(filename) ? 0 : -1;
    }
    
    // Check if we have enough memory - if not, fall back to chunked loading
    size_t freeHeap = rp2040.getFreeHeap();
    
    // More aggressive memory threshold - need file size + 4KB overhead for screen buffer
    // Also require at least 8KB total free to avoid fragmentation issues
    if (freeHeap < (size_t)file_size + 4096 || freeHeap < 8192) {
        ekilo_set_status_message("Low memory, using chunked mode...");
        return ekilo_open_chunked(filename) ? 0 : -1;
    }
    
    // Test for heap fragmentation - try to allocate what we'll need
    size_t estimated_need = file_size + 4096;  // File + screen buffer
    void* frag_test = malloc(estimated_need);
    if (frag_test) {
        free(frag_test);
    } else {
        // Heap is fragmented, use chunked loading
        ekilo_set_status_message("Fragmented heap, using chunked mode...");
        return ekilo_open_chunked(filename) ? 0 : -1;
    }
    
    // Mark as non-chunked file
    E.is_chunked = false;
    E.chunk_dirty = false;
    
    // Check if this file should be read-only
    // history.txt and other system files are read-only by default
    // Use C string operations instead of String to avoid heap allocation
    const char* ext = strrchr(filename, '.');
    const char* lastSlash = strrchr(filename, '/');
    const char* basename = lastSlash ? lastSlash + 1 : filename;
    
    E.read_only = (ext && strcmp(ext, ".log") == 0) ||
                  (strcmp(basename, "history.txt") == 0) ||
                  (strstr(filename, "/python_scripts/history") != nullptr);
    
    // Enter low memory mode if memory is tight or already set (saves RAM by skipping syntax highlighting)
    if (!E.low_memory_mode) {
        E.low_memory_mode = (freeHeap < (size_t)file_size * 3);  // Less than 3x file size available
    }
    
    // Free existing filename and allocate new one
    free(E.filename);
    E.filename = strdup(filename);
    if (!E.filename) {
        ekilo_set_status_message("ERROR: Memory allocation failed for filename");
        return -1;
    }
    
    // Update global tracking for external monitoring
    free(g_currently_editing_file);
    g_currently_editing_file = strdup(filename);
    
    // Skip syntax highlighting in low memory mode to save RAM
    if (!E.low_memory_mode) {
        ekilo_select_syntax_highlight(filename);
    } else {
        E.syntax = nullptr;  // No syntax highlighting
    }
    
    // Open file for reading using safe function (holds mutex until close)
    File file = safeFileOpen(filename, "r", 2000);
    if (!file) {
        ekilo_set_status_message("ERROR: Cannot open file for reading");
        free(E.filename);
        E.filename = nullptr;
        return -1;
    }
    
    // Track memory usage while loading
    size_t total_loaded = 0;
    size_t line_count = 0;
    const size_t MAX_LINE_LENGTH = 512; // Reduced line length limit for safety
    const size_t MAX_LINES = 2000;      // Safety limit on lines
    
    // Use stack buffer instead of String to avoid heap fragmentation
    // String allocations in a loop cause severe fragmentation on RP2040
    char line_buf[MAX_LINE_LENGTH + 1];
    
    while (file.available() && total_loaded < (size_t)file_size && line_count < MAX_LINES) {
        // Check memory before each line read
        size_t currentFree = rp2040.getFreeHeap();
        if (currentFree < 1024) {
            safeFileClose(file, false);  // Read-only, no flush needed
            ekilo_set_status_message("Loaded %d lines (stopped: low memory)", line_count);
            E.dirty = 0;  // Not dirty since we're stopping early
            return 0;     // Return success with partial load
        }
        
        // Read line character-by-character into stack buffer
        // This avoids heap fragmentation from String allocations
        size_t len = 0;
        while (file.available() && len < MAX_LINE_LENGTH) {
            char c = file.read();
            if (c == '\n') break;
            if (c != '\r') {  // Skip carriage returns
                line_buf[len++] = c;
            }
        }
        line_buf[len] = '\0';
        
        ekilo_insert_row(E.numrows, line_buf, len);
        total_loaded += len + 1; // +1 for newline
        line_count++;
        
        // Yield more frequently to prevent watchdog issues
        if (line_count % 25 == 0) {
            yield();
        }
        
        // Check memory every 10 lines and show progress for large files
        if (line_count % 10 == 0) {
            if (!check_memory_available(1024)) { // Keep 1KB free
                safeFileClose(file, false);  // Read-only, no flush needed
                size_t freeHeap = rp2040.getFreeHeap();
                ekilo_set_status_message("ERROR: Out of memory at line %d (%dKB free)", line_count, freeHeap / 1024);
                return -1;
            }
            
            // Show loading progress for files with many lines
            if (line_count > 100 && line_count % 50 == 0) {
                ekilo_set_status_message("Loading... %d lines (%d bytes)", line_count, total_loaded);
                // Force a quick screen update to show progress
                if (E.screen_dirty) {
                    ekilo_refresh_screen();
                    E.screen_dirty = false;
                }
            }
        }
        
        // Prevent infinite loops on corrupted files
        if (line_count > 10000) { // Reasonable file size limit in lines
            safeFileClose(file, false);  // Read-only, no flush needed
            ekilo_set_status_message("ERROR: File too many lines (max 10000)");
            return -1;
        }
    }
    
    safeFileClose(file, false);  // Read-only, no flush needed
    E.dirty = 0;
    
    // Mark screen as dirty for refresh after file load
    E.screen_dirty = true;
    
    ekilo_set_status_message("Loaded %d lines (%d bytes)", line_count, total_loaded);
    return 0;
}

// Save file
int ekilo_save() {
    if (E.filename == nullptr) {
        // TODO: Implement save-as functionality
        ekilo_set_status_message("Save aborted");
        return 0;
    }
    
    int len = 0;
    for (int j = 0; j < E.numrows; j++)
        len += E.row[j].size + 1;
    
    // Try buffered save first, fall back to streaming if low memory
    char* buf = (char*)malloc(len);
    
    // CRITICAL: Pause Core2 during flash write operations
    // On RP2040, flash writes disable XIP cache and Core2 will crash
    // if it tries to execute code from flash during the write.
    // Keep Core2 paused for entire save operation for safety
    bool was_paused = pauseCore2ForFlash(100);
    
    // Open file for writing (safeFileOpen handles mutex, we handle Core2 pause)
    File file = safeFileOpen(E.filename, "w", 2000);
    if (!file) {
        unpauseCore2ForFlash(was_paused);
        if (buf) free(buf);
        ekilo_set_status_message("ERROR: Could not open file for writing");
        return 0;
    }
    
    if (buf) {
        // Buffered write (faster)
        char* p = buf;
        for (int j = 0; j < E.numrows; j++) {
            if (E.row[j].chars && E.row[j].size > 0) {
                memcpy(p, E.row[j].chars, E.row[j].size);
                p += E.row[j].size;
            }
            *p = '\n';
            p++;
        }
        file.write((uint8_t*)buf, len);
    } else {
        // Streaming write (low memory fallback)
        ekilo_set_status_message("Saving... (streaming mode)");
        for (int j = 0; j < E.numrows; j++) {
            if (E.row[j].chars && E.row[j].size > 0) {
                file.write((uint8_t*)E.row[j].chars, E.row[j].size);
            }
            file.write('\n');
        }
    }
    
    // safeFileClose handles flush and mutex release
    // Pass true for write mode to ensure proper flush
    file.flush();  // Explicit flush while Core2 still paused
    safeFileClose(file, true);
    
    // Restore Core2 state
    unpauseCore2ForFlash(was_paused);
    
    // Set transfer path for zero-copy communication with parent context
    // The parent (e.g., Python REPL) can read directly from this file path
    // instead of passing content through String objects
    ContextManager::getInstance().setTransferPath(E.filename);
    
    // If in REPL mode, handle save behavior
    if (E.repl_mode) {
        // ZERO-COPY: Instead of storing content in E.saved_file_content,
        // we set the transfer path and the parent context reads from file
        // Still store content for backward compatibility, but callers should
        // prefer using ContextManager::getTransferPath() for efficiency
        if (buf && len < 8192) {
            E.saved_file_content = String(buf, len);
        } else {
            E.saved_file_content = "";  // Clear - use file path instead
        }
        E.should_quit = 1;
        ekilo_set_status_message("File saved: %s (%d bytes)", E.filename, len);
    }
    
    E.dirty = 0;
    if (!E.repl_mode) {
        ekilo_set_status_message("%d bytes written to flash", len);
    }
    E.screen_dirty = true;
    if (buf) free(buf);
    return len;
}

// Write buffer in chunks for Windows compatibility
void ekilo_write_buffer_chunked(Buffer* ab) {
    if (!ab) return;
    
    if (!ab->b || ab->len == 0) {
        buffer_free(ab);
        return;
    }
    
    // Optimized for speed: larger chunks, minimal delay
    const int CHUNK_SIZE = 256; // Larger chunks for faster output
    const int DELAY_MICROS = 2;  // Minimal delay - just enough for USB buffering
    
    int bytes_written = 0;
    while (bytes_written < ab->len) {
        int chunk_size = min(CHUNK_SIZE, ab->len - bytes_written);
        
        // Validate chunk boundaries
        if (chunk_size <= 0 || bytes_written + chunk_size > ab->len) {
            break; // Prevent buffer overrun
        }
        
        // Write this chunk
        Jerial.write((uint8_t*)(ab->b + bytes_written), chunk_size);
        
        bytes_written += chunk_size;
        
        // Only flush and delay every few chunks for better throughput
        if (bytes_written % 1024 == 0 && bytes_written < ab->len) {
            Jerial.flush();
            delayMicroseconds(DELAY_MICROS);
        }
    }
    
    Jerial.flush(); // Final flush
    buffer_free(ab);
}

// Quick cursor position update without full redraw
static void ekilo_update_cursor_only() {
    char buf[32];
    if (E.repl_mode) {
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.cx - E.coloff) + 1);
    } else {
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 2, (E.cx - E.coloff) + 1);
    }
    Jerial.print(buf);
    Jerial.flush();
}

// Track previous scroll state for optimization
static int prev_rowoff = -1;
static int prev_coloff = -1;

// Refresh screen - Buffered approach with chunked output for Windows compatibility
void ekilo_refresh_screen() {
    // Safety checks
    if (E.screenrows <= 0) E.screenrows = 24;
    if (E.screencols <= 0) E.screencols = 80;
    if (E.cy < 0) E.cy = 0;
    if (E.cx < 0) E.cx = 0;
    if (E.numrows > 0 && E.cy >= E.numrows) E.cy = E.numrows - 1;
    
    // For chunked files, check if we need to load a new chunk
    if (E.is_chunked && ekilo_needs_chunk_reload()) {
        // Calculate file-relative cursor position
        size_t file_row = E.chunk_start + E.cy;
        // Load new chunk centered on current position
        if (ekilo_load_chunk(file_row)) {
            // Adjust cursor position for new chunk
            E.cy = file_row - E.chunk_start;
            if (E.cy < 0) E.cy = 0;
            if (E.cy >= E.numrows) E.cy = E.numrows - 1;
        }
    }
    
    // Track if scroll changed (requires full redraw)
    bool scroll_changed = false;
    
    // Scroll handling
    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
        scroll_changed = true;
    }
    if (E.cy >= E.rowoff + E.screenrows) {
        E.rowoff = E.cy - E.screenrows + 1;
        scroll_changed = true;
    }
    if (E.cx < E.coloff) {
        E.coloff = E.cx;
        scroll_changed = true;
    }
    if (E.cx >= E.coloff + E.screencols) {
        E.coloff = E.cx - E.screencols + 1;
        scroll_changed = true;
    }
    
    // Check if scroll position changed
    if (E.rowoff != prev_rowoff || E.coloff != prev_coloff) {
        scroll_changed = true;
        prev_rowoff = E.rowoff;
        prev_coloff = E.coloff;
    }
    
    // If screen isn't dirty and scroll didn't change, just update cursor
    if (!E.screen_dirty && !scroll_changed) {
        ekilo_update_cursor_only();
        return;
    }
    
    Buffer ab = {nullptr, 0, 0};  // {buffer, length, capacity} - will grow exponentially
    
    // Clear screen and position cursor - use absolute positioning for both modes
    if (E.repl_mode) {
        // In REPL mode, clear and position at top-left
        // No header in REPL mode for cleaner interface
        buffer_append(&ab, "\x1b[2J\x1b[H", 7);
    } else {
        buffer_append(&ab, "\x1b[2J\x1b[H", 7);
        
        // Add persistent help header that stays visible (only in non-REPL mode)
        buffer_append(&ab, "\x1b[48;5;199m\x1b[38;5;236m", 27); // Blue background, white text
        char help_header[128];
        snprintf(help_header, sizeof(help_header), 
                     "                    Jumperless Kilo Text Editor                      ");
        int help_len = strlen(help_header);
        if (help_len > E.screencols) help_len = E.screencols;
        buffer_append(&ab, help_header, help_len);
        
        // Pad help header to full width
        while (help_len < E.screencols) {
            buffer_append(&ab, " ", 1);
            help_len++;
        }
        buffer_append(&ab, "\x1b[0m\r\n", 6); // Reset formatting and newline
    }
    
    // Draw rows (adjust available rows for header and help lines)
    int available_rows = E.repl_mode ? E.screenrows - 3 : E.screenrows - 4; // -3 for status+message+help, -4 for header+status+message+help
    for (int y = 0; y < available_rows; y++) {
        int filerow = E.rowoff + y;
        
        if (filerow >= E.numrows) {
            if (E.numrows == 0 && y == available_rows / 3) {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome),
                    "eKilo editor -- version 1.0.0");
                if (welcomelen > E.screencols) welcomelen = E.screencols;
                int padding = (E.screencols - welcomelen) / 2;
                if (padding) {
                    buffer_append(&ab, "~", 1);
                    padding--;
                }
                while (padding--) buffer_append(&ab, " ", 1);
                buffer_append(&ab, welcome, welcomelen);
            } else {
                buffer_append(&ab, "~", 1);
            }
        } else {
            // In low memory mode, render may be null - use chars directly
            char* display_str = E.row[filerow].render ? E.row[filerow].render : E.row[filerow].chars;
            int display_size = E.row[filerow].render ? E.row[filerow].rsize : E.row[filerow].size;
            
            int len = display_size - E.coloff;
            if (len < 0) len = 0;
            if (len > E.screencols) len = E.screencols;
            
            // If we have syntax highlighting, use it
            if (display_str && E.row[filerow].hl && !E.low_memory_mode) {
                char* c = &display_str[E.coloff];
                unsigned char* hl = &E.row[filerow].hl[E.coloff];
                int current_color = -1;
                for (int j = 0; j < len; j++) {
                    if (iscntrl(c[j])) {
                        // Show control chars as inverted symbols
                        char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                        buffer_append(&ab, "\x1b[7m", 4);
                        buffer_append(&ab, &sym, 1);
                        buffer_append(&ab, "\x1b[0m", 4);
                        if (current_color != -1) {
                            char buf[16];
                            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
                            buffer_append(&ab, buf, clen);
                        }
                    } else if (hl[j] == HL_NORMAL) {
                        if (current_color != -1) {
                            buffer_append(&ab, "\x1b[0m", 4);
                            current_color = -1;
                        }
                        buffer_append(&ab, &c[j], 1);
                    } else {
                        int color = ekilo_syntax_to_color(hl[j]);
                        if (color != current_color) {
                            current_color = color;
                            char buf[16];
                            int clen = snprintf(buf, sizeof(buf), "\x1b[38;5;%dm", color);
                            buffer_append(&ab, buf, clen);
                        }
                        buffer_append(&ab, &c[j], 1);
                    }
                }
                if (current_color != -1) {
                    buffer_append(&ab, "\x1b[0m", 4);
                }
            } else if (display_str) {
                // No syntax highlighting - just output the text directly
                buffer_append(&ab, &display_str[E.coloff], len);
            }
        }
        
        // Clear to end of line and add newline
        buffer_append(&ab, "\x1b[K\r\n", 6);
    }
    
    // Status bar with reverse video
    buffer_append(&ab, "\x1b[7m", 4);
    char status[120], rstatus[80];
    
    // Calculate available space for filename
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", E.cy + 1, E.numrows);
    const char* suffix = E.read_only ? " [READ-ONLY]" : (E.dirty ? " (modified)" : "");
    const char* menu_suffix = E.in_menu_mode ? " [MENU MODE]" : (E.low_memory_mode ? " [LOW MEM]" : "");
    
    // Calculate space needed for " - XX lines" + suffix + menu + right status + padding
    int fixed_space = strlen(" - ") + 10 + strlen(" lines") + strlen(suffix) + strlen(menu_suffix) + rlen + 2;
    int available_for_filename = E.screencols - fixed_space;
    
    // Ensure we have at least some space for filename
    if (available_for_filename < 10) {
        available_for_filename = 10;
    }
    
    const char* display_filename = E.filename ? E.filename : "[No Name]";
    
    // Build status string with dynamic filename length
    int len = snprintf(status, sizeof(status), "%.*s - %d lines%s%s",
        available_for_filename, display_filename, E.numrows, suffix, menu_suffix);
    
    if (len > E.screencols) len = E.screencols;
    buffer_append(&ab, status, len);
    while (len < E.screencols) {
        if (E.screencols - len == rlen) {
            buffer_append(&ab, rstatus, rlen);
            break;
        } else {
            buffer_append(&ab, " ", 1);
            len++;
        }
    }
    buffer_append(&ab, "\x1b[0m", 4); // Reset formatting
    buffer_append(&ab, "\r\n", 2);
    
    // Message bar - show status history (most recent messages, scrolling)
    // Show up to 2 lines of status messages
    for (int line = 0; line < 2; line++) {
        int msg_idx = (E.status_history_head - 1 - line + EditorConfig::STATUS_HISTORY_SIZE) % EditorConfig::STATUS_HISTORY_SIZE;
        
        const char* msg = "";
        if (line == 0) {
            // First line: current message (with timeout)
            uint32_t timeout = (strstr(E.statusmsg, "ERROR:") || strstr(E.statusmsg, "WARNING:")) ? 30000 : 5000;
            if (strlen(E.statusmsg) > 0 && millis() - E.statusmsg_time < timeout) {
                msg = E.statusmsg;
            }
        } else if (line < E.status_history_count && E.status_history_count > 1) {
            // Second line: previous message from history (dimmed)
            msg = E.status_history[msg_idx];
            buffer_append(&ab, "\x1b[90m", 5); // Dim/gray color
        }
        
        int msglen = strlen(msg);
        if (msglen > E.screencols) msglen = E.screencols;
        buffer_append(&ab, msg, msglen);
        
        // Pad to full width
        while (msglen < E.screencols) {
            buffer_append(&ab, " ", 1);
            msglen++;
        }
        
        if (line == 1 && E.status_history_count > 1) {
            buffer_append(&ab, "\x1b[0m", 4); // Reset color
        }
        buffer_append(&ab, "\r\n", 2);
    }
    
    // Help bar (second line - comprehensive commands in magenta)
    buffer_append(&ab, "\x1b[35m", 5); // Magenta color
    char help_line1[120];
    char help_line2[120];
    snprintf(help_line1, sizeof(help_line1), 
             "Ctrl-S = Save │ Ctrl-Q = Quit │ ↑/↓ = Navigate | CTRL-P = Save and load in MicroPython");
    snprintf(help_line2, sizeof(help_line2),
             "Tab = Indent │ Backspace = Delete │ Wheel=Move/Type │ Ctrl-U = Memory Status");
    
    int help1_len = strlen(help_line1);
    if (help1_len > E.screencols) help1_len = E.screencols;
    buffer_append(&ab, help_line1, help1_len);
    // Pad first help line to full width
    while (help1_len < E.screencols) {
        buffer_append(&ab, " ", 1);
        help1_len++;
    }
    buffer_append(&ab, "\r\n", 2);
    
    int help2_len = strlen(help_line2);
    if (help2_len > E.screencols) help2_len = E.screencols;
    buffer_append(&ab, help_line2, help2_len);
    // Pad second help line to full width
    while (help2_len < E.screencols) {
        buffer_append(&ab, " ", 1);
        help2_len++;
    }
    buffer_append(&ab, "\x1b[0m", 4); // Reset color
    
    // Position cursor - use absolute positioning for both modes
    char buf[32];
    if (E.repl_mode) {
        // In REPL mode with XTerm alternate screen, use absolute positioning
        // No header offset since we skip the header in REPL mode
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, // +1 for content area (no header)
                                                    (E.cx - E.coloff) + 1);
        buffer_append(&ab, buf, strlen(buf));
        
        // Track total lines for XTerm cleanup (though XTerm handles this automatically)
        E.lines_used = E.screenrows; // Use full screen in XTerm alternate buffer
    } else {
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 2, // +2 for help header
                                                    (E.cx - E.coloff) + 1);
        buffer_append(&ab, buf, strlen(buf));
    }
    
    // Write buffer in chunks for Windows compatibility
    ekilo_write_buffer_chunked(&ab);
    
    // Clear dirty flag after full redraw
    E.screen_dirty = false;
    
    // Schedule OLED update with context around cursor
    ekilo_schedule_oled_update();
    
    // Note: Don't set screen_dirty here - we only call this when screen is already dirty
    // OLED update will be processed later when screen is clean and serial buffer is empty
}

// Process keypress with batching and acceleration
void ekilo_process_keypress() {
    static int quit_times = 3;
    static bool handler_initialized = false;
    
    // Initialize input handler once
    if (!handler_initialized) {
        g_input_handler.init();
        handler_initialized = true;
    }
    
    // Use input handler for batched/accelerated input
    int moves = 1;
    int c = g_input_handler.processInput(&Jerial, moves);
    if (c == 0) return; // No key available
    
    switch (c) {
        case '\n':
        
        case ENTER:
            if (E.read_only) {
                ekilo_set_status_message("READ-ONLY - press Ctrl+Q to exit");
                break;
            }
            ekilo_insert_newline();
            break;
            
        case ESC:
            // ESC exits menu mode if in menu, otherwise quits editor without saving
            if (E.in_menu_mode) {
                // Exit menu mode with ESC
                E.in_menu_mode = false;
                if (E.numrows > 0) {
                    E.cy = E.numrows - 1; // Go to last line
                    E.cx = E.row[E.cy].size; // Go to end of line
                }
                ekilo_set_status_message("Menu mode cancelled");
                ekilo_schedule_oled_update();
                E.screen_dirty = true;
            } else {
                // Not in menu mode - quit editor immediately without saving
                E.should_quit = 1;
            }
            break;
            
        case CTRL_Q:
            if (E.dirty && quit_times > 0) {
                ekilo_set_status_message("WARNING!!! File has unsaved changes. "
                    "Press Ctrl-Q %d more times to quit.", quit_times);
                quit_times--;
                return;
            }
            E.should_quit = 1;
            break;
            
        case CTRL_S:
            if (E.read_only) {
                ekilo_set_status_message("READ-ONLY file - cannot save");
            } else {
                ekilo_save();
            }
            break;
            
        case CTRL_P:
            // Save and launch MicroPython REPL
            if (!E.read_only) {
                ekilo_save();
            }
            E.should_launch_repl = true;
            E.should_quit = 1;
            break;
            
        case CTRL_U:
            // Show detailed memory status including logic analyzer and MicroPython usage
            {
                size_t freeHeap = rp2040.getFreeHeap();
                size_t usedByEditor = 0;
                
                // Calculate memory used by editor content
                for (int i = 0; i < E.numrows; i++) {
                    if (E.row[i].chars) usedByEditor += E.row[i].size;
                    if (E.row[i].render) usedByEditor += E.row[i].rsize;
                    if (E.row[i].hl) usedByEditor += E.row[i].rsize;
                }
                
                ekilo_set_status_message("Memory: %dKB free, Editor:%dKB, MP:64KB, LA:~24KB, Need:%dKB reserve", 
                                       freeHeap / 1024, usedByEditor / 1024, MIN_FREE_HEAP / 1024);
            }
            E.screen_dirty = true;
            break;
            
        case HOME_KEY:
            E.cx = 0;
            E.oled_horizontal_offset = 0; // Reset scrolling when going to start of line
            ekilo_schedule_oled_update();
            E.screen_dirty = true;
            break;
            
        case END_KEY:
            if (E.cy < E.numrows)
                E.cx = E.row[E.cy].size;
            // Don't reset horizontal scrolling for END - let it scroll to show end of line
            ekilo_schedule_oled_update();
            E.screen_dirty = true;
            break;
            
        case BACKSPACE:
        case CTRL_H:
        case DEL_KEY:
            if (E.read_only) {
                ekilo_set_status_message("READ-ONLY - press Ctrl+Q to exit");
                break;
            }
            if (c == DEL_KEY) ekilo_move_cursor(ARROW_RIGHT);
            ekilo_del_char();
            break;
            
        case PAGE_UP:
        case PAGE_DOWN: {
            // Apply batched page movement (moves pages at once if held)
            for (int m = 0; m < moves; m++) {
                if (c == PAGE_UP) {
                    E.cy = E.rowoff;
                } else if (c == PAGE_DOWN) {
                    E.cy = E.rowoff + E.screenrows - 1;
                    if (E.cy > E.numrows) E.cy = E.numrows;
                }
                
                int times = E.screenrows;
                while (times--)
                    ekilo_move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            E.screen_dirty = true;
            break;
        }
        case TAB:
            if (E.read_only) {
                ekilo_set_status_message("READ-ONLY - press Ctrl+Q to exit");
                break;
            }
            ekilo_insert_char(' ');
            ekilo_insert_char(' ');
            ekilo_insert_char(' '); 
            ekilo_insert_char(' ');
            break;
        
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            // Apply batched/accelerated movement - always allowed
            for (int i = 0; i < moves; i++) {
                ekilo_move_cursor(c);
            }
            break;
            
        case CTRL_L:
            // Refresh screen
            E.screen_dirty = true;
            break;
            
        default:
            // Block character insertion in read-only mode
            if (E.read_only) {
                // Only show message occasionally to avoid spam
                static uint32_t last_ro_msg = 0;
                if (millis() - last_ro_msg > 1000) {
                    ekilo_set_status_message("READ-ONLY file - Ctrl+Q to exit");
                    last_ro_msg = millis();
                }
                break;
            }
            ekilo_insert_char(c);
            break;
    }
    
    quit_times = 1;
}

// OLED update batching functions
void ekilo_schedule_oled_update() {
    unsigned long currentTime = millis();
    
    // Always mark as pending - we'll process it when conditions are right
    E.oled_last_input_time = currentTime;
    E.oled_update_pending = true;
}

void ekilo_process_oled_update() {
    // Only update OLED if:
    // 1. There's a pending update
    // 2. Screen is clean (not dirty) for better responsiveness
    // 3. No serial input pending (to avoid interrupting input processing)
    // 4. Enough time has passed since last input
    if (E.oled_update_pending && 
        !E.screen_dirty && 
        !Jerial.available() && 
        (millis() - E.oled_last_input_time) >= 10) { // 50ms delay
        
        ekilo_update_oled_context();
        E.oled_update_pending = false;
    }
}

// Calculate horizontal scrolling for OLED display of current line
void ekilo_calculate_oled_scrolling() {
    if (!oled.isConnected() || E.numrows == 0) return;
    
    // Get current line text
    String currentLineText = "";
    if (E.cy < E.numrows && E.row[E.cy].chars) {
        currentLineText = String(E.row[E.cy].chars);
    }
    
    // Cursor position within the current line
    int cursorPos = E.cx;
    
    // Calculate characters that fit based on actual font metrics
    int charWidth = oled.getCharacterWidth();
    const int maxVisibleChars = charWidth > 0 ? (128 / charWidth) : 21;
    
    // Calculate visible cursor position relative to current offset
    int visibleCursorPos = cursorPos - E.oled_horizontal_offset;
    
    // Ensure cursor has 2 characters of padding from right edge for adding characters
    const int CURSOR_RIGHT_PADDING = 2;
    
    // Scroll right if cursor is too close to right edge (accounting for padding)
    if (visibleCursorPos >= maxVisibleChars - CURSOR_RIGHT_PADDING) {
        E.oled_horizontal_offset = cursorPos - maxVisibleChars + CURSOR_RIGHT_PADDING + 1;
    }
    // Scroll left if cursor is too close to left edge  
    else if (visibleCursorPos < E.OLED_SCROLL_MARGIN) {
        E.oled_horizontal_offset = cursorPos - E.OLED_SCROLL_MARGIN;
    }
    
    // Keep offset within bounds
    if (E.oled_horizontal_offset < 0) {
        E.oled_horizontal_offset = 0;
    }
    
    // Allow scrolling past end of line to provide space for adding characters
    // Don't restrict offset based on line length - let cursor have padding space
}

// Update OLED with 3 lines around cursor
// Optimized to avoid String allocations that fragment heap
void ekilo_update_oled_context() {
    if (!oled.isConnected()) {
        return;
    }
    
    // Check memory before OLED update - skip if critically low
    size_t freeHeap = rp2040.getFreeHeap();
    if (freeHeap < 2048) {
        return;  // Skip OLED update when memory is critically low
    }
    
    // Clear framebuffer first to prevent text overlapping (like FileManager does)
    oled.clearFramebuffer();
    
    // Use small font mode like FileManager for consistent text rendering
    oled.setSmallFont(SMALL_FONT_ANDALE_MONO);
    
    // Calculate horizontal scrolling for current line
    if (E.numrows > 0) {
        ekilo_calculate_oled_scrolling();
    }
    
    // Calculate current file row position
    int currentRow = E.cy;
    
    // Determine which 3 lines to show (1 before, 2 after, adjusting for edges)
    int startRow = currentRow - 1;
    int endRow = currentRow + 1;
    
    // Adjust for edges
    if (startRow < 0) {
        startRow = 0;
        endRow = min(2, E.numrows - 1);
    } else if (endRow >= E.numrows) {
        endRow = E.numrows - 1;
        startRow = max(0, endRow - 2);
    }
    
    // Use stack buffers instead of String to avoid heap fragmentation
    // Max visible chars on OLED is ~21 at small font, so 32 is plenty
    char lineBufs[3][32];
    int lineCount = 0;
    int cursorLineInDisplay = -1;
    
    // Define constants for drawing
    const int lineY = 8;        // Start Y position with proper baseline
    const int lineHeight = 12;  // Line spacing for small font
    
    // Handle empty file case
    if (E.numrows == 0) {
        strncpy(lineBufs[0], "[Empty File]", 31);
        lineBufs[0][31] = '\0';
        lineCount = 1;
        cursorLineInDisplay = 0;
    } else {
        int charWidth = oled.getCharacterWidth();
        const int maxVisibleChars = charWidth > 0 ? min(128 / charWidth, 31) : 21;
        
        for (int i = startRow; i <= endRow && i < E.numrows && lineCount < 3; i++) {
            // Track which line is current
            if (i == currentRow) {
                cursorLineInDisplay = lineCount;
            }
            
            // Copy line content with horizontal scrolling - directly to stack buffer
            if (E.row[i].chars && E.row[i].size > 0) {
                int startPos = E.oled_horizontal_offset;
                int availableLen = E.row[i].size - startPos;
                
                if (availableLen > 0) {
                    int copyLen = min(availableLen, maxVisibleChars);
                    memcpy(lineBufs[lineCount], E.row[i].chars + startPos, copyLen);
                    lineBufs[lineCount][copyLen] = '\0';
                } else {
                    lineBufs[lineCount][0] = '\0';  // Past end of line
                }
            } else {
                lineBufs[lineCount][0] = '\0';  // Empty line
            }
            
            lineCount++;
        }
    }
    
    // Draw each line (using stack buffers - no heap allocation)
    for (int i = 0; i < lineCount; i++) {
        oled.drawText(0, lineY + (i * lineHeight), lineBufs[i]);
    }
    
    // Add cursor position indicator if current line is visible
    if (cursorLineInDisplay >= 0 && E.numrows > 0) {
        // Calculate cursor position within the visible text of current line
        int visibleCursorPos = E.cx - E.oled_horizontal_offset;
        
        // Show cursor if it's visible on screen (including beyond end of line)
        int charWidth = oled.getCharacterWidth();
        const int maxVisibleChars = charWidth > 0 ? (128 / charWidth) : 21;
        
        if (visibleCursorPos >= 0 && visibleCursorPos < maxVisibleChars) {
            // Get the character at cursor position
            char cursorChar = ' '; // Default to space
            
            int lineLen = strlen(lineBufs[cursorLineInDisplay]);
            if (visibleCursorPos < lineLen) {
                cursorChar = lineBufs[cursorLineInDisplay][visibleCursorPos];
                if (cursorChar == '\0' || cursorChar == '\n') {
                    cursorChar = ' '; // Show space for end of line
                }
            }
            // If cursor is beyond the line, cursorChar remains space
            
            int cursorX = visibleCursorPos * charWidth;
            int cursorY = lineY + (cursorLineInDisplay * lineHeight);
            
            // Draw highlighted character instead of underline
            oled.drawHighlightedChar(cursorX, cursorY, cursorChar);
        }
    }
    
    // Show character selection mode if active
    if (E.char_selection_mode) {
        // Draw character selection indicator in top-right corner with larger text
        char selectionText[5];
        char selected = ekilo_get_character_from_index(E.selected_char_index);
        if (selected == '\n') {
            strcpy(selectionText, "\\n");
        } else if (selected == '\b') {
            strcpy(selectionText, "BS");
        } else if (selected == '\t') {
            strcpy(selectionText, "TAB");
        } else {
            snprintf(selectionText, sizeof(selectionText), "%c", selected);
        }
        
        // Use larger font for character indicator
        oled.setFontForSize(FONT_ANDALE_MONO, 2);
        int largeCharWidth = oled.getCharacterWidth();
        
        // Position in top-right corner
        int textWidth = strlen(selectionText) * largeCharWidth;
        int x = 128 - textWidth - 2; // 2 pixel margin
        int y = 12; // Adjusted for larger font baseline
        
        // Draw background box
        oled.fillRect(x - 1, y - 11, textWidth + 2, 14, SSD1306_WHITE);
        
        // Draw text in black on white background
        oled.setTextColor(SSD1306_BLACK);
        oled.drawText(x, y, selectionText);
        oled.setTextColor(SSD1306_WHITE); // Restore default
        
        // Restore small font for rest of display
        oled.setSmallFont(SMALL_FONT_ANDALE_MONO);
    }
    
    // Draw save/exit menu 2 lines below file content
    int menuY = lineY + (lineCount * lineHeight) + 6; // 6 pixel gap between file and menu
    
    // Save and cancel options with proper UI styling
    // Use const char* instead of String to avoid heap allocation
    const char* saveText = "Save";
    const char* cancelText = "Cancel";
    const int saveLen = 4;   // strlen("Save")
    const int cancelLen = 6; // strlen("Cancel")
    
    // Always show menu buttons, but only highlight when in menu mode
    if (E.in_menu_mode) {
        // Clear display and just show menu options
        oled.clearFramebuffer();
        
        // Calculate positions for side-by-side layout in center of screen
        int charWidth = oled.getCharacterWidth();
        int saveWidth = saveLen * charWidth;
        int cancelWidth = cancelLen * charWidth;
        int buttonHeight = 12;
        int buttonPadding = 4;
        int gapBetweenButtons = 8;
        
        // Total width needed: saveButton + gap + cancelButton
        int totalWidth = (saveWidth + buttonPadding * 2) + gapBetweenButtons + (cancelWidth + buttonPadding * 2);
        int startX = (128 - totalWidth) / 2; // Center horizontally
        int buttonY = 10; // Center vertically on 32px tall screen
        
        int saveButtonX = startX;
        int cancelButtonX = startX + (saveWidth + buttonPadding * 2) + gapBetweenButtons;
        
        // Draw Save button
        if (E.menu_selection == 0) { // Save selected
            oled.fillRect(saveButtonX, buttonY, saveWidth + buttonPadding * 2, buttonHeight, SSD1306_WHITE);
            oled.setTextColor(SSD1306_BLACK);
            oled.drawText(saveButtonX + buttonPadding, buttonY + 8, saveText);
            oled.setTextColor(SSD1306_WHITE);
        } else {
            oled.drawText(saveButtonX + buttonPadding, buttonY + 8, saveText);
        }
        
        // Draw Cancel button
        if (E.menu_selection == 1) { // Cancel selected
            oled.fillRect(cancelButtonX, buttonY, cancelWidth + buttonPadding * 2, buttonHeight, SSD1306_WHITE);
            oled.setTextColor(SSD1306_BLACK);
            oled.drawText(cancelButtonX + buttonPadding, buttonY + 8, cancelText);
            oled.setTextColor(SSD1306_WHITE);
        } else {
            oled.drawText(cancelButtonX + buttonPadding, buttonY + 8, cancelText);
        }
    } else {
        // Show dimmed menu options when not in menu mode
        oled.drawText(2, menuY, saveText);
        oled.drawText(2, menuY + 12, cancelText);
    }
    
    // Flush to display
    oled.flushFramebuffer();
}

// Character selection array for clickwheel input
const char* character_set = " abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()[]{}|\\:;\"'<>,.?/+-=_~`\t\n\b";

// Get character from index in character set
char ekilo_get_character_from_index(int index) {
    int len = strlen(character_set);
    if (index < 0) index = len - 1;
    if (index >= len) index = 0;
    return character_set[index];
}

// Enter character selection mode
void ekilo_enter_char_selection() {
    E.char_selection_mode = true;
    E.selected_char_index = 1; // Start with 'A' instead of space
    E.char_selection_timer = millis();
    ekilo_set_status_message("CHAR SELECT: %c (Wheel=cycle, Click=confirm, Wait=exit)", 
                             ekilo_get_character_from_index(E.selected_char_index));
    
    // Schedule OLED update when entering character selection mode
    ekilo_schedule_oled_update();
    
    // Mark screen as dirty for refresh
    E.screen_dirty = true;
}

// Exit character selection mode
void ekilo_exit_char_selection() {
    E.char_selection_mode = false;
    ekilo_set_status_message("Character selection cancelled");
    
    // Schedule OLED update when exiting character selection mode
    ekilo_schedule_oled_update();
    
    // Mark screen as dirty for refresh
    E.screen_dirty = true;
}

// Cycle through characters
void ekilo_cycle_character(int direction) {
    if (!E.char_selection_mode) return;
    
    E.selected_char_index -= direction;
    int len = strlen(character_set);
    
    // Wrap around
    if (E.selected_char_index < 0) E.selected_char_index = len - 1;
    if (E.selected_char_index >= len) E.selected_char_index = 0;
    
    E.char_selection_timer = millis(); // Reset timeout
    
    char selected = ekilo_get_character_from_index(E.selected_char_index);
    if (selected == '\n') {
        ekilo_set_status_message("CHAR SELECT: \\n (Wheel=cycle, Click=confirm, Wait=exit)");
    } else if (selected == '\b') {
        ekilo_set_status_message("CHAR SELECT: BKSP (Wheel=cycle, Click=confirm, Wait=exit)");
    } else {
        ekilo_set_status_message("CHAR SELECT: %c (Wheel=cycle, Click=confirm, Wait=exit)", selected);
    }
    
    // Schedule OLED update in character selection mode for responsiveness
    ekilo_schedule_oled_update();
    
    // Mark screen as dirty for refresh
    E.screen_dirty = true;
}

// Confirm character selection and insert it
void ekilo_confirm_character() {
    if (!E.char_selection_mode) return;
    
    char selected = ekilo_get_character_from_index(E.selected_char_index);
    E.char_selection_mode = false;
    
    // If cursor is beyond the end of line, move to end first
    if (E.cy < E.numrows && E.cx > E.row[E.cy].size) {
        E.cx = E.row[E.cy].size;
    }
    
    if (selected == '\t') {
        ekilo_insert_char(' ');
        ekilo_insert_char(' ');
        ekilo_insert_char(' ');
        ekilo_insert_char(' ');
        ekilo_set_status_message("Inserted tab");
    } else if (selected == '\n') {
        ekilo_insert_newline();
        ekilo_set_status_message("Inserted newline");
    } else if (selected == '\b') {
        ekilo_del_char();
        ekilo_set_status_message("Backspace");
    } else {
        ekilo_insert_char(selected);
        ekilo_set_status_message("Inserted '%c'", selected);
    }
    
    // Schedule OLED update after character insertion
    ekilo_schedule_oled_update();
    
    // Mark screen as dirty for refresh
    E.screen_dirty = true;
}

// Process encoder input for cursor movement and character selection
void ekilo_process_encoder_input() {
    // Check for character selection timeout
    if (E.char_selection_mode && (millis() - E.char_selection_timer > E.CHAR_SELECTION_TIMEOUT)) {
        ekilo_exit_char_selection();
        return;
    }
    
    unsigned long currentTime = millis();
    
    // Read current encoder position
    long currentPosition = encoderPosition;
    
    // Calculate position delta
    long deltaPosition = (currentPosition - E.last_encoder_position)/4;
    deltaPosition = -deltaPosition;
    
    // Only process if there's a significant change and enough time has passed
    if (deltaPosition != 0 && (currentTime - E.last_encoder_update >= 20)) { // 20ms minimum between updates for responsiveness
        E.last_encoder_position = currentPosition;
        E.last_encoder_update = currentTime;
        
        if (E.char_selection_mode) {
            // In character selection mode: cycle through characters
            // Use smaller delta for precise character selection
            int steps = max(1, min(abs(deltaPosition), 3)); // 1-3 characters per rotation for faster cycling
            int direction = (deltaPosition > 0) ? -1 : 1;
            
            for (int i = 0; i < steps; i++) {
                ekilo_cycle_character(direction);
            }
        } else {
            // Normal mode: move cursor or navigate menu
            if (E.in_menu_mode) {
                // In menu mode: navigate between save/exit options or loop to file
                if (deltaPosition != 0) {
                    int direction = (deltaPosition > 0) ? 1 : -1;  // Match cursor movement direction
                    
                    if (direction > 0) {  // Right scroll in menu (clockwise)
                        // Scrolling right in menu: Save -> Cancel -> loop to beginning of file
                        E.menu_selection++;
                        if (E.menu_selection > 1) {
                            // Past exit option - loop to beginning of file
                            E.in_menu_mode = false;
                            E.cy = 0;
                            E.cx = 0;
                            ekilo_set_status_message("Looped to beginning of file");
                        } else {
                            // Update menu selection
                            if (E.menu_selection == 0) {
                                ekilo_set_status_message("Save selected - click to save file");
                            } else {
                                ekilo_set_status_message("Cancel selected - click to cancel without saving");
                            }
                        }
                    } else {  // Left scroll in menu (counter-clockwise)
                        // Scrolling left in menu: Cancel -> Save -> back to end of file
                        E.menu_selection--;
                        if (E.menu_selection < 0) {
                            // Before save option - exit menu to end of file
                            E.in_menu_mode = false;
                            if (E.numrows > 0) {
                                E.cy = E.numrows - 1;
                                E.cx = E.row[E.cy].size;
                            } else {
                                E.cy = 0;
                                E.cx = 0;
                            }
                            ekilo_set_status_message("Back to end of file");
                        } else {
                            // Update menu selection
                            if (E.menu_selection == 0) {
                                ekilo_set_status_message("Save selected - click to save file");
                            } else {
                                ekilo_set_status_message("Cancel selected - click to cancel without saving");
                            }
                        }
                    }
                    ekilo_schedule_oled_update();
                    E.screen_dirty = true;
                }
            } else {
                // Use direct delta for responsive cursor movement (horizontal only)
                int steps = max(1, min(abs(deltaPosition), 4)); // 1-4 characters per rotation
                int direction = (deltaPosition > 0) ? 1 : -1;  // Fixed: positive delta = right, negative = left
                
                for (int i = 0; i < steps; i++) {
                    if (direction > 0) {  // Right movement (clockwise scroll)
                        // Moving right - check if we should enter menu mode
                        if (E.cy >= E.numrows || (E.cy == E.numrows - 1 && E.cx >= E.row[E.cy].size)) {
                            // At or past end of file - enter menu mode
                            if (!E.in_menu_mode) {
                                E.in_menu_mode = true;
                                E.menu_selection = 0; // Start with save option
                                ekilo_set_status_message("Menu mode: Use wheel to select Save/Exit, click to confirm");
                                break; // Stop processing more steps
                            }
                        } else {
                            ekilo_move_cursor(ARROW_RIGHT);
                        }
                    } else {
                        // Moving left - check if we should loop to menu
                        if (E.cy == 0 && E.cx == 0) {
                            // At beginning of file - loop to menu
                            E.in_menu_mode = true;
                            E.menu_selection = 1; // Start with cancel (last option)
                            ekilo_set_status_message("Looped to menu from beginning");
                            break;
                        } else {
                            ekilo_move_cursor(ARROW_LEFT);
                        }
                    }
                }
                ekilo_schedule_oled_update();
                E.screen_dirty = true;
            }
        }
    }
    
    // Handle button press with direct digitalRead and debouncing
    bool current_button_state = digitalRead(BUTTON_ENC);
    
    // Check for button press (HIGH to LOW transition) with debouncing
    if (!current_button_state && E.last_button_state && (currentTime - E.button_debounce_time > 50)) {
        E.button_debounce_time = currentTime;
        
        if (E.char_selection_mode) {
            // In character selection mode: confirm character
            ekilo_confirm_character();
        } else if (E.in_menu_mode) {
            // In menu mode: handle menu selection
            if (E.menu_selection == 0) { // Save
                ekilo_save();
                E.in_menu_mode = false; // Exit menu after save
                ekilo_update_oled_context();
            } else if (E.menu_selection == 1) { // Cancel
                E.should_quit = 1;
            }
        } else {
            // Normal mode: enter character selection
            ekilo_enter_char_selection();
            // Initialize encoder position tracking when entering char selection
            E.last_encoder_position = encoderPosition;
        }
    }
    
    E.last_button_state = current_button_state;
}

// REPL mode functions
void ekilo_init_repl_mode() {
    E.repl_mode = true;
    // No need to store cursor position - XTerm alternate screen handles this
    E.screenrows = DEFAULT_EDITOR_ROWS; // Use configurable screen size in alternate buffer
    Jerial.write(0x0E);
    Jerial.flush();    
    // Clear the alternate screen and position at top-left
    Jerial.print("\x1b[2J\x1b[H");

    
    // Print a simple header once when entering REPL mode
    Jerial.println("eKilo Editor | Ctrl-S/Ctrl-P=save & load | Ctrl-Q=quit | Wheel=navigate");
    Jerial.flush();
}

void ekilo_store_cursor_position() {
    // Save current cursor position using terminal escape sequence
    Jerial.print("\033[s"); // Save cursor position
    Jerial.flush();
    E.lines_used = 0;
}

void ekilo_restore_cursor_position() {
    if (E.repl_mode) {
        // Restore saved cursor position
        Jerial.print("\033[u"); // Restore cursor position
        Jerial.flush();
    }
}

void ekilo_cleanup_repl_mode() {
    if (!E.repl_mode) return;
    
    // XTerm alternate screen buffer handles all cleanup automatically
    // when we call restoreScreenState() in the calling function
    // No manual cleanup needed here
    
    E.repl_mode = false;
}

// ============================================================================
// UNIFIED EDITOR ENTRY POINT
// ============================================================================

// Static storage for result (accessed via ContextManager transfer data)
static EkiloResult g_ekilo_result;

/**
 * @brief Get the result of the last ekilo_run() call
 */
const EkiloResult* ekilo_get_result() {
    size_t len = 0;
    const void* data = ContextManager::getInstance().getTransferData(&len);
    if (data && len == sizeof(EkiloResult)) {
        return static_cast<const EkiloResult*>(data);
    }
    return nullptr;
}

/**
 * @brief Clear the stored ekilo result
 */
void ekilo_clear_result() {
    ContextManager::getInstance().clearTransferData();
}

/**
 * @brief Unified eKilo entry point - consolidates ekilo_main and ekilo_main_repl
 * 
 * This function:
 * 1. Initializes the editor
 * 2. Opens the file (or creates new)
 * 3. Runs the editor loop
 * 4. Stores result in ContextManager for caller
 * 5. Cleans up resources
 */
bool ekilo_run(const char* filename, EkiloMode mode) {
    // Initialize result
    g_ekilo_result = EkiloResult();
    
    // Safety check: ensure we have minimum heap before starting
    size_t freeHeap = rp2040.getFreeHeap();
    if (freeHeap < 4096) {
        Jerial.println("ERROR: Not enough memory to start editor");
        return false;
    }
    
    // Fragmentation detection: try to allocate a 4KB contiguous block
    // If this fails, the heap is fragmented and we should use low-memory mode
    bool heap_fragmented = false;
    void* test_alloc = malloc(4096);
    if (test_alloc) {
        free(test_alloc);
    } else {
        // Heap is fragmented - we have free memory but not contiguous
        heap_fragmented = true;
        Jerial.println("NOTE: Heap fragmented, using low-memory mode");
    }
    
    // Initialize editor
    ekilo_init();
    
    // Force low-memory mode if heap is fragmented
    if (heap_fragmented) {
        E.low_memory_mode = true;
    }
    
    // REPL mode setup
    if (mode == EKILO_MODE_REPL) {
        ekilo_init_repl_mode();
    }
    
    // Extra safety: verify init succeeded
    if (E.screenrows <= 0 || E.screencols <= 0) {
        E.screenrows = 24;
        E.screencols = 80;
    }
    
    // Open file or set up for new file
    if (filename != nullptr) {
        int open_result = ekilo_open(filename);
        if (open_result != 0) {
            // File doesn't exist or error - set up for new file
            free(E.filename);
            E.filename = strdup(filename);
            if (!E.filename) {
                ekilo_set_status_message("ERROR: Memory allocation failed for filename");
                if (mode == EKILO_MODE_REPL) {
                    ekilo_cleanup_repl_mode();
                }
                return false;
            }
            
            // Ensure directory exists for new files using safe functions
            String file_path = String(filename);
            int last_slash = file_path.lastIndexOf('/');
            if (last_slash > 0) {
                String dir_path = file_path.substring(0, last_slash);
                if (!safeFileExists(dir_path.c_str(), 1000)) {
                    safeMkdir(dir_path.c_str(), 2000);
                }
            }
            
            // Set syntax highlighting for new files
            ekilo_select_syntax_highlight(filename);
            ekilo_set_status_message("Creating new file: %s", filename);
            E.screen_dirty = true;
            
            // Reset state for empty file
            for (int i = 0; i < E.numrows; i++) {
                ekilo_free_row(&E.row[i]);
            }
            free(E.row);
            E.row = nullptr;
            E.numrows = 0;
            E.row_capacity = 0;
            E.cy = 0;
            E.cx = 0;
        }
    }
    
    // Set appropriate status message
    if (mode == EKILO_MODE_REPL) {
        if (E.filename) {
            ekilo_set_status_message("Editing: %s | Ctrl-S: Save & Load | Ctrl-P: Save & REPL | Ctrl-Q: Exit", E.filename);
        } else {
            ekilo_set_status_message("REPL Mode | Ctrl-S: Save & Load | Ctrl-P: Save & REPL | Ctrl-Q: Exit");
        }
    } else {
        ekilo_set_status_message("HELP: Ctrl-S = save | Ctrl-P = save & REPL | Ctrl-Q = quit | Clickwheel = move/type");
    }
    
    // Initialize encoder position tracking
    E.last_encoder_position = encoderPosition;
    E.last_encoder_update = millis();
    
    // Initialize button state
    E.last_button_state = digitalRead(BUTTON_ENC);
    E.button_debounce_time = millis();
    
    // Initial OLED update
    ekilo_schedule_oled_update();
    
    // Timing for service calls and memory checks
    unsigned long last_service_call = millis();
    unsigned long last_memory_check = millis();
    const unsigned long SERVICE_CALL_INTERVAL = 100;
    const unsigned long MEMORY_CHECK_INTERVAL = 5000;
    
    // ========== MAIN EDITOR LOOP ==========
    while (!E.should_quit) {
        yield();  // Feed watchdog
        
        unsigned long current_time = millis();
        
        // Service calls (SlotManager for file change detection)
        if (current_time - last_service_call > SERVICE_CALL_INTERVAL) {
            SlotManager::getInstance().service();
            last_service_call = current_time;
        }
        
        // Memory monitoring
        if (current_time - last_memory_check > MEMORY_CHECK_INTERVAL) {
            size_t currentFree = rp2040.getFreeHeap();
            if (currentFree < 2048) {
                ekilo_set_status_message("WARNING: Low memory (%dKB) - save your work!", currentFree / 1024);
                E.screen_dirty = true;
            }
            last_memory_check = current_time;
        }
        
        // Process keyboard input
        ekilo_process_keypress();
        
        // Process encoder input (FIXED: was missing in REPL mode)
        ekilo_process_encoder_input();
        
        // Process OLED updates
        ekilo_process_oled_update();
        
        // Refresh screen if needed
        if (E.screen_dirty) {
            ekilo_refresh_screen();
            E.screen_dirty = false;
        }
    }
    
    // ========== STORE RESULT ==========
    g_ekilo_result.launch_repl = E.should_launch_repl;
    g_ekilo_result.saved = (E.dirty == 0 && E.filename != nullptr);
    g_ekilo_result.cancelled = (E.dirty != 0);  // Quit with unsaved changes
    
    // Store saved file path for zero-copy transfer
    if (E.filename) {
        strncpy(g_ekilo_result.saved_path, E.filename, sizeof(g_ekilo_result.saved_path) - 1);
        g_ekilo_result.saved_path[sizeof(g_ekilo_result.saved_path) - 1] = '\0';
        
        // Also set transfer path in ContextManager
        ContextManager::getInstance().setTransferPath(E.filename);
    }
    
    // Store result in ContextManager transfer data
    ContextManager::getInstance().setTransferData(&g_ekilo_result, sizeof(g_ekilo_result));
    
    // ========== CLEANUP ==========
    oled.restoreNormalFont();
    
    for (int i = 0; i < E.numrows; i++) {
        ekilo_free_row(&E.row[i]);
    }
    free(E.row);
    free(E.filename);
    E.row = nullptr;
    E.numrows = 0;
    E.row_capacity = 0;
    E.filename = nullptr;
    
    // Clear global tracking
    free(g_currently_editing_file);
    g_currently_editing_file = nullptr;
    
    if (mode == EKILO_MODE_REPL) {
        ekilo_cleanup_repl_mode();
    }
    
    return true;
}

// ============================================================================
// LEGACY WRAPPERS (for backward compatibility)
// ============================================================================

// Legacy ekilo_main - wraps ekilo_run
int ekilo_main(const char* filename) {
    if (!ekilo_run(filename, EKILO_MODE_NORMAL)) {
        return -1;
    }
    
    const EkiloResult* result = ekilo_get_result();
    if (result && result->launch_repl) {
        return 2;  // Signal to launch REPL
    }
    return 0;  // Normal exit
}

// Legacy ekilo_main_repl - wraps ekilo_run
String ekilo_main_repl(const char* filename) {
    if (!ekilo_run(filename, EKILO_MODE_REPL)) {
        return "";
    }
    
    const EkiloResult* result = ekilo_get_result();
    if (!result) {
        return "";
    }
    
    // Build return string for backward compatibility
    // NOTE: This creates a copy - new code should use ekilo_get_result() instead
    String savedContent = "";
    if (result->saved && result->saved_path[0] != '\0') {
        // Read content from saved file for backward compatibility using safe function
        File f = safeFileOpen(result->saved_path, "r", 2000);
        if (f) {
            savedContent = f.readString();
            safeFileClose(f, false);  // Read-only, no flush
        }
    }
    
    // Add REPL launch prefix if needed
    if (result->launch_repl && savedContent.length() > 0) {
        savedContent = "[LAUNCH_REPL]" + savedContent;
    }
    
    return savedContent;
}

// ============================================================================
// REMOVED: ekilo_inline_edit (dead code - was never called)
// See git history if you need to recover this ~600 line function.
// ============================================================================


// ============================================================================
// Terminal Size Detection
// ============================================================================

// Probe terminal size using CSI 6n (Cursor Position Report)
// Returns true if successful, false if terminal doesn't respond
bool ekilo_probe_terminal_size(uint16_t& rows, uint16_t& cols) {
    // Mark that we're sending a CPR query (for stray response filtering)
    extern uint32_t last_cpr_query_time;
    last_cpr_query_time = millis();
    
    // Clear any pending input thoroughly
    delay(10);  // Brief delay for any in-flight data
    while (Jerial.available()) Jerial.read();
    
    // Save cursor position
    Jerial.print("\x1b[s");
    // Move to far bottom-right corner
    Jerial.print("\x1b[9999;9999H");
    // Query cursor position (CSI 6n)
    Jerial.print("\x1b[6n");
    Jerial.flush();
    
    // Wait a bit for response to arrive
    delay(50);
    
    // Read response with timeout: ESC [ rows ; cols R
    char buf[32];
    size_t n = 0;
    uint32_t start = millis();
    const uint32_t timeout = 300;  // 300ms timeout (response should be quick)
    
    while ((millis() - start) < timeout && n < sizeof(buf) - 1) {
        if (Jerial.available()) {
            char c = Jerial.read();
            // Skip until we see ESC
            if (n == 0 && c != '\x1b') continue;
            buf[n++] = c;
            // Response ends with 'R'
            if (c == 'R') break;
        }
        delayMicroseconds(500);
    }
    buf[n] = '\0';
    
    // Restore cursor position
    Jerial.print("\x1b[u");
    Jerial.flush();
    
    // Clear any remaining garbage from buffer (late responses, echoes)
    delay(20);
    while (Jerial.available()) Jerial.read();
    
    // Parse response: ESC [ rows ; cols R
    int r = 0, c = 0;
    if (sscanf(buf, "\x1b[%d;%dR", &r, &c) == 2) {
        rows = (uint16_t)r;
        cols = (uint16_t)c;
        return true;
    }
    
    return false;
}

// Re-probe terminal size and resize editor
void ekilo_resize_to_terminal() {
    uint16_t term_rows = 0, term_cols = 0;
    if (ekilo_probe_terminal_size(term_rows, term_cols)) {
        E.screenrows = constrain(term_rows - 4, 16, 60);
        E.screencols = constrain(term_cols, 40, 200);
        E.screen_dirty = true;
        ekilo_set_status_message("Resized to %dx%d", E.screencols, E.screenrows);
    } else {
        ekilo_set_status_message("Terminal size detection failed");
    }
}

// ============================================================================
// Chunked File Loading for Large Files
// ============================================================================

// Open a large file using chunked loading
bool ekilo_open_chunked(const char* filename) {
    if (!filename) return false;
    
    // First pass: count total lines in file using safe file functions
    File file = safeFileOpen(filename, "r", 2000);
    if (!file) {
        ekilo_set_status_message("ERROR: Cannot open file '%s'", filename);
        return false;
    }
    
    // Count lines WITHOUT allocating String objects - just count newlines
    // This avoids heap fragmentation from repeated String allocations
    size_t total_lines = 0;
    while (file.available()) {
        char c = file.read();
        if (c == '\n') {
            total_lines++;
        }
        // Safety limit
        if (total_lines > 50000) {
            safeFileClose(file, false);  // Read-only, no flush
            ekilo_set_status_message("ERROR: File too large (>50000 lines)");
            return false;
        }
        // Yield every 1000 chars to prevent watchdog timeout
        if ((total_lines & 0x3FF) == 0) {
            yield();
        }
    }
    // Account for last line if file doesn't end with newline
    if (file.position() > 0) {
        total_lines++;  // Count the line we're on even without trailing newline
    }
    safeFileClose(file, false);  // Read-only, no flush
    
    // Setup chunked state
    E.is_chunked = true;
    E.total_file_lines = total_lines;
    E.chunk_start = 0;
    E.chunk_loaded_lines = 0;
    E.chunked_filename = String(filename);
    E.chunk_dirty = false;
    E.low_memory_mode = true;  // Chunked files always use low memory mode
    
    // Check if this file should be read-only
    String fname = String(filename);
    E.read_only = fname.endsWith("history.txt") || 
                  fname.endsWith(".log") ||
                  fname.startsWith("/python_scripts/history");
    
    // Free existing filename and allocate new one
    free(E.filename);
    E.filename = strdup(filename);
    
    // Update global tracking
    free(g_currently_editing_file);
    g_currently_editing_file = strdup(filename);
    
    // Skip syntax highlighting in chunked mode to save memory
    E.syntax = nullptr;
    
    // Load initial chunk around line 0
    if (!ekilo_load_chunk(0)) {
        E.is_chunked = false;
        return false;
    }
    
    E.dirty = 0;
    E.screen_dirty = true;
    
    ekilo_set_status_message("Chunked load: %d lines total, showing %d-%d", 
                            total_lines, E.chunk_start, E.chunk_start + E.chunk_loaded_lines);
    return true;
}

// Load a chunk of lines centered around center_line
bool ekilo_load_chunk(size_t center_line) {
    if (!E.is_chunked) return true;  // Not chunked, nothing to do
    
    // Calculate chunk bounds FIRST (before any allocations)
    size_t half_chunk = EditorConfig::CHUNK_SIZE / 2;
    size_t start = (center_line > half_chunk) ? center_line - half_chunk : 0;
    size_t end = start + EditorConfig::CHUNK_SIZE;
    if (end > E.total_file_lines) {
        end = E.total_file_lines;
        if (end > EditorConfig::CHUNK_SIZE) {
            start = end - EditorConfig::CHUNK_SIZE;
        } else {
            start = 0;
        }
    }
    
    // If chunk hasn't moved much, don't reload
    if (E.chunk_loaded_lines > 0 && 
        start >= E.chunk_start && 
        end <= E.chunk_start + E.chunk_loaded_lines) {
        return true;  // Already have this chunk loaded
    }
    
    // Pre-allocate new row array BEFORE freeing old one to check memory
    size_t needed_rows = end - start;
    EditorRow* new_rows = (EditorRow*)malloc(sizeof(EditorRow) * needed_rows);
    if (!new_rows) {
        ekilo_set_status_message("ERROR: Cannot allocate chunk (%d rows)", needed_rows);
        return false;
    }
    memset(new_rows, 0, sizeof(EditorRow) * needed_rows);
    
    // Save dirty chunk if needed (before any file operations)
    if (E.chunk_dirty) {
        ekilo_save_chunk_to_temp();
    }
    
    // NOW free existing rows (after we know we have memory for new ones)
    for (int i = 0; i < E.numrows; i++) {
        ekilo_free_row(&E.row[i]);
    }
    free(E.row);
    E.row = new_rows;
    E.numrows = 0;
    E.row_capacity = needed_rows;  // Pre-allocated for chunk size
    
    // Open file and seek to start line using safe file function
    File file = safeFileOpen(E.chunked_filename.c_str(), "r", 2000);
    if (!file) {
        ekilo_set_status_message("ERROR: Cannot reopen chunked file");
        return false;
    }
    
    // Skip to start line - use a char buffer instead of String to save memory
    size_t current_line = 0;
    while (current_line < start && file.available()) {
        while (file.available()) {
            char c = file.read();
            if (c == '\n') break;
        }
        current_line++;
    }
    
    // Load chunk - read directly into pre-allocated rows
    E.chunk_start = start;
    char line_buf[256];  // Stack buffer for reading lines
    
    while (current_line < end && file.available() && E.numrows < (int)needed_rows) {
        // Read line into stack buffer
        int len = 0;
        while (file.available() && len < 255) {
            char c = file.read();
            if (c == '\n') break;
            if (c != '\r') {
                line_buf[len++] = c;
            }
        }
        line_buf[len] = '\0';
        
        // Allocate and copy into row
        int row_idx = E.numrows;
        E.row[row_idx].idx = row_idx;
        E.row[row_idx].size = len;
        E.row[row_idx].chars = (char*)malloc(len + 1);
        if (E.row[row_idx].chars) {
            memcpy(E.row[row_idx].chars, line_buf, len + 1);
        } else {
            E.row[row_idx].size = 0;
        }
        
        // In low memory/chunked mode, skip render and highlight buffers
        E.row[row_idx].render = nullptr;
        E.row[row_idx].rsize = len;
        E.row[row_idx].hl = nullptr;
        E.row[row_idx].hl_oc = 0;
        
        E.numrows++;
        current_line++;
    }
    E.chunk_loaded_lines = E.numrows;
    
    safeFileClose(file, false);  // Read-only, no flush needed
    
    E.chunk_dirty = false;
    E.screen_dirty = true;
    E.low_memory_mode = true;  // Chunked files always use low memory mode
    
    return true;
}

// Check if scroll position requires loading a new chunk
bool ekilo_needs_chunk_reload() {
    if (!E.is_chunked) return false;
    
    // Current cursor position in file coordinates
    size_t file_row = E.chunk_start + E.cy;
    
    // Check if we're getting close to chunk boundaries
    size_t buffer = EditorConfig::CHUNK_BUFFER;
    
    // Need reload if cursor is within buffer of chunk edges
    if (E.cy < (int)buffer && E.chunk_start > 0) {
        return true;  // Near top of chunk, need to load earlier lines
    }
    if (E.cy >= (int)(E.chunk_loaded_lines - buffer) && 
        E.chunk_start + E.chunk_loaded_lines < E.total_file_lines) {
        return true;  // Near bottom of chunk, need to load later lines
    }
    
    return false;
}

// Save current chunk edits back to the original file
// Uses atomic write pattern: write to temp, then rename
void ekilo_save_chunk_to_temp() {
    if (!E.is_chunked || !E.chunk_dirty) return;
    if (E.chunked_filename.length() == 0) return;
    
    // Mark as dirty in case we fail
    E.dirty = 1;
    
    // Create temp file path
    String tempPath = E.chunked_filename + ".tmp";
    
    // CRITICAL: Pause Core2 during flash write operations
    bool was_paused = pauseCore2ForFlash(100);
    
    // Open original file for reading
    File origFile = safeFileOpen(E.chunked_filename.c_str(), "r", 2000);
    if (!origFile) {
        unpauseCore2ForFlash(was_paused);
        ekilo_set_status_message("ERROR: Cannot open original file");
        return;
    }
    
    // Open temp file for writing
    File tempFile = safeFileOpen(tempPath.c_str(), "w", 2000);
    if (!tempFile) {
        safeFileClose(origFile, false);
        unpauseCore2ForFlash(was_paused);
        ekilo_set_status_message("ERROR: Cannot create temp file");
        return;
    }
    
    // Phase 1: Copy lines BEFORE the chunk from original file
    size_t line_num = 0;
    char line_buf[512];
    while (line_num < E.chunk_start && origFile.available()) {
        // Read line from original
        int len = 0;
        while (origFile.available() && len < 510) {
            char c = origFile.read();
            if (c == '\n') {
                line_buf[len++] = '\n';
                break;
            }
            if (c != '\r') {
                line_buf[len++] = c;
            }
        }
        // Write to temp file
        if (len > 0) {
            tempFile.write((uint8_t*)line_buf, len);
        }
        line_num++;
    }
    
    // Phase 2: Write edited chunk from memory
    for (int i = 0; i < E.numrows; i++) {
        if (E.row[i].chars && E.row[i].size > 0) {
            tempFile.write((uint8_t*)E.row[i].chars, E.row[i].size);
        }
        tempFile.write('\n');
    }
    
    // Phase 3: Skip original chunk lines, then copy rest
    // Skip the lines that were in the chunk
    size_t chunk_end = E.chunk_start + E.chunk_loaded_lines;
    while (line_num < chunk_end && origFile.available()) {
        // Just skip these lines
        while (origFile.available()) {
            char c = origFile.read();
            if (c == '\n') break;
        }
        line_num++;
    }
    
    // Copy remaining lines from original
    while (origFile.available()) {
        int len = 0;
        while (origFile.available() && len < 510) {
            char c = origFile.read();
            if (c == '\n') {
                line_buf[len++] = '\n';
                break;
            }
            if (c != '\r') {
                line_buf[len++] = c;
            }
        }
        if (len > 0) {
            tempFile.write((uint8_t*)line_buf, len);
        }
    }
    
    // Flush and close files
    tempFile.flush();
    safeFileClose(tempFile, true);
    safeFileClose(origFile, false);
    
    // Atomic rename: remove original, rename temp to original
    fs_mutex_acquire();
    if (FatFS.exists(E.chunked_filename.c_str())) {
        FatFS.remove(E.chunked_filename.c_str());
    }
    bool renamed = FatFS.rename(tempPath.c_str(), E.chunked_filename.c_str());
    fs_mutex_release();
    
    unpauseCore2ForFlash(was_paused);
    
    if (renamed) {
        E.chunk_dirty = false;
        E.dirty = 0;
        
        // Recalculate total lines (chunk size may have changed)
        // We need to count lines in the new file
        size_t new_total = 0;
        File countFile = safeFileOpen(E.chunked_filename.c_str(), "r", 1000);
        if (countFile) {
            while (countFile.available()) {
                char c = countFile.read();
                if (c == '\n') new_total++;
            }
            safeFileClose(countFile, false);
            E.total_file_lines = new_total > 0 ? new_total : 1;
        }
        
        ekilo_set_status_message("Chunk saved: %d lines merged", E.numrows);
    } else {
        ekilo_set_status_message("ERROR: Failed to save chunk");
    }
} 