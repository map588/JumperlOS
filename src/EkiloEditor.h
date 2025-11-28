/*
 * EkiloEditor.h - Arduino-compatible eKilo text editor for Jumperless
 * Based on the original eKilo editor by Antonio Foti
 * Adapted for Arduino/embedded systems
 */

#ifndef EKILO_EDITOR_H
#define EKILO_EDITOR_H

#include <Arduino.h>
#include <FatFS.h>
#include <cstdarg>
#include <cstdlib>
#include <cstring>

// Forward declarations
struct EditorRow;
struct SyntaxDefinition;

// Editor configuration - make screen size configurable
#define DEFAULT_EDITOR_ROWS 35  // Increased from smaller default to 25 rows
#define DEFAULT_EDITOR_COLS 80

// Editor configuration structure
struct EditorConfig {
    int cx, cy;           // Cursor position
    int rx;               // Render cursor position (for tabs)
    int rowoff;           // Row offset for scrolling
    int coloff;           // Column offset for scrolling
    int screenrows;       // Number of rows on screen
    int screencols;       // Number of columns on screen
    int numrows;          // Number of rows in file
    int row_capacity;     // Allocated capacity for row array (reduces realloc fragmentation)
    EditorRow* row;        // Array of editor rows
    int dirty;            // File modified flag
    char* filename;       // Current filename
    char statusmsg[80];   // Status message
    unsigned long statusmsg_time; // Status message timestamp
    SyntaxDefinition* syntax; // Current syntax highlighting
    
    // Search functionality
    char* search_query;
    int search_direction;
    int search_last_match;
    
    // Character insertion mode tracking
    bool char_selection_mode;
    int selected_char_index;
    unsigned long char_selection_timer;
    static const unsigned long CHAR_SELECTION_TIMEOUT = 5000; // 5 seconds timeout for character selection
    
    // OLED update batching - optimized for responsiveness
    unsigned long oled_last_input_time;
    bool oled_update_pending;
    static const unsigned long OLED_UPDATE_DELAY = 350000; // 350 milliseconds for responsive updates
    // Note: OLED updates only happen when screen is clean and serial buffer is empty
    
    // OLED horizontal scrolling for all visible lines
    int oled_horizontal_offset;
    int oled_cursor_position;
    static const int OLED_SCROLL_MARGIN = 4; // Start scrolling when within 4 chars of edge
    
    // Encoder position tracking for smooth movement
    long last_encoder_position;
    unsigned long last_encoder_update;
    
    // Direct button state tracking
    bool last_button_state;
    unsigned long button_debounce_time;
    
    // Special menu navigation (save/exit options)
    bool in_menu_mode;
    int menu_selection; // 0 = save, 1 = exit
    
    // REPL mode and positioning
    bool repl_mode;
    int original_cursor_row;
    int original_cursor_col;
    int start_row;
    int lines_used;
    String saved_file_content; // Content of saved file for REPL return
    
    // Ctrl+P functionality - save and launch MicroPython REPL
    bool should_launch_repl;
    
    // Quit flag
    bool should_quit;
    
    // Screen refresh optimization
    bool screen_dirty;     // Flag to track when screen needs refresh
    
    // Chunked file loading for large files (>8KB)
    bool is_chunked;                    // True if using chunked loading
    size_t total_file_lines;            // Total lines in the file
    size_t chunk_start;                 // First loaded line number
    size_t chunk_loaded_lines;          // Number of lines currently loaded
    static const size_t CHUNK_SIZE = 150;     // Lines loaded at once (~5 pages)
    static const size_t CHUNK_BUFFER = 75;    // Buffer lines above/below viewport (~2 pages)
    static const size_t CHUNK_THRESHOLD = 4096; // File size threshold for chunking (4KB - more aggressive)
    String chunked_filename;            // Filename for chunk reloading
    bool chunk_dirty;                   // True if chunk has been edited
    
    // Status message history for scrolling display
    static const int STATUS_HISTORY_SIZE = 5;
    char status_history[5][80];         // Ring buffer of recent messages
    int status_history_head;            // Next write position
    int status_history_count;           // Number of messages in buffer
    
    // Read-only mode for viewing files without editing
    bool read_only;                     // True if file is read-only (no edits allowed)
    
    // Low memory mode - disables features to save RAM
    bool low_memory_mode;               // True when memory is critically low
    
    EditorConfig();
    ~EditorConfig();
};

// Syntax highlighting definition
struct SyntaxDefinition {
    const char** filematch;           // File extension matches
    const char** keywords;            // Keywords to highlight
    const char* singleline_comment_start; // Single line comment start
    const char* multiline_comment_start;   // Multi-line comment start
    const char* multiline_comment_end;     // Multi-line comment end
    int flags;                        // Highlighting flags
};

// ============================================================================
// Editor Result - Stores outcome of editor session via ContextManager
// ============================================================================

/**
 * @brief Result of an eKilo editor session
 * 
 * Stored in ContextManager transfer data to avoid String allocations.
 * Use ekilo_get_result() after ekilo_run() to retrieve.
 */
struct EkiloResult {
    bool saved;                 // True if file was saved
    bool launch_repl;           // True if Ctrl+P was pressed (save and launch REPL)
    bool cancelled;             // True if user quit without saving (Ctrl+Q on dirty file)
    char saved_path[128];       // Path to saved file (for zero-copy transfer)
    
    EkiloResult() : saved(false), launch_repl(false), cancelled(false) {
        saved_path[0] = '\0';
    }
};

/**
 * @brief Editor mode flags
 */
enum EkiloMode : uint8_t {
    EKILO_MODE_NORMAL = 0,      // Normal full-screen editor mode
    EKILO_MODE_REPL = 1,        // REPL mode - uses alternate screen, auto-quits on save
};

// ============================================================================
// Unified Editor Entry Point
// ============================================================================

/**
 * @brief Run the eKilo editor (unified entry point)
 * 
 * This is the single entry point for all eKilo operations.
 * Results are stored in ContextManager transfer data.
 * 
 * @param filename File to open (nullptr for new file)
 * @param mode EKILO_MODE_NORMAL or EKILO_MODE_REPL
 * @return true if editor ran successfully, false on error
 */
bool ekilo_run(const char* filename, EkiloMode mode = EKILO_MODE_NORMAL);

/**
 * @brief Get the result of the last ekilo_run() call
 * 
 * Retrieves result from ContextManager transfer data.
 * @return Pointer to result, or nullptr if no result available
 */
const EkiloResult* ekilo_get_result();

/**
 * @brief Clear the stored ekilo result
 */
void ekilo_clear_result();

// ============================================================================
// Legacy Entry Points (for backward compatibility)
// ============================================================================

// Main editor functions
void ekilo_init();
int ekilo_main(const char* filename); // DEPRECATED: Use ekilo_run() instead
String ekilo_main_repl(const char* filename); // DEPRECATED: Use ekilo_run(filename, EKILO_MODE_REPL)
int ekilo_open(const char* filename);
int ekilo_save();
void ekilo_refresh_screen();
void ekilo_process_keypress();
void ekilo_set_status_message(const char* fmt, ...);

// REPL mode functions
void ekilo_init_repl_mode();
void ekilo_store_cursor_position();
void ekilo_restore_cursor_position();
void ekilo_cleanup_repl_mode();

// Text manipulation functions
void ekilo_insert_char(int c);
void ekilo_insert_newline();
void ekilo_del_char();
void ekilo_move_cursor(int key);

// Row management functions
void ekilo_insert_row(int at, const char* s, size_t len);
void ekilo_del_row(int at);
void ekilo_free_row(EditorRow* row);

// Input handling
int ekilo_read_key();

// Syntax highlighting
void ekilo_select_syntax_highlight(const char* filename);
void ekilo_update_syntax(EditorRow* row);
int ekilo_syntax_to_color(int hl);

// OLED display functions
void ekilo_update_oled_context();
void ekilo_calculate_oled_scrolling();
void ekilo_schedule_oled_update();
void ekilo_process_oled_update();

// Clickwheel input functions
void ekilo_process_encoder_input();
void ekilo_enter_char_selection();
void ekilo_exit_char_selection();
void ekilo_cycle_character(int direction);
void ekilo_confirm_character();
char ekilo_get_character_from_index(int index);

// Memory monitoring and safety functions
bool check_memory_available(size_t needed);
void ekilo_show_memory_status();
void ekilo_emergency_cleanup();

// External monitoring functions
const char* ekilo_get_currently_editing_file();  // Get currently open file (nullptr if none)
String ekilo_get_current_buffer_content();       // Get current editor buffer as String (for live preview)

// Terminal size detection
bool ekilo_probe_terminal_size(uint16_t& rows, uint16_t& cols);
void ekilo_resize_to_terminal();

// Chunked file loading for large files
bool ekilo_open_chunked(const char* filename);
bool ekilo_load_chunk(size_t center_line);
bool ekilo_needs_chunk_reload();
void ekilo_save_chunk_to_temp();

// ============================================================================
// Input Handler - Batches repeated keys and provides acceleration
// ============================================================================

// Key categories for batching
enum KeyCategory {
    KEY_CAT_NONE = 0,
    KEY_CAT_UP,
    KEY_CAT_DOWN,
    KEY_CAT_LEFT,
    KEY_CAT_RIGHT,
    KEY_CAT_PAGEUP,
    KEY_CAT_PAGEDOWN,
    KEY_CAT_OTHER  // Non-repeatable keys
};

struct InputHandler {
    // State tracking
    int last_key;                    // Last key processed
    KeyCategory last_category;       // Category of last key
    uint32_t last_key_time;          // Time of last key
    uint32_t repeat_start_time;      // When repeat started
    int repeat_count;                // Number of repeated keys
    
    // Configuration - aggressive timeout to prevent runaway cursor
    static const uint32_t REPEAT_THRESHOLD_MS = 25;   // Quick timeout - 25ms gap = key released
    static const uint32_t ACCEL_START_MS = 150;       // Start acceleration after 150ms
    static const uint32_t ACCEL_TIMEOUT_MS = 40;      // Reset acceleration after 40ms gap
    static const int ACCEL_MULTIPLIER = 3;            // Conservative acceleration (2x)
    static const int MAX_BATCH_SIZE = 5;              // Limit batch size to stay responsive
    
    // Initialize
    void init();
    
    // Get key category for batching
    KeyCategory categorize(int key);
    
    // Process available input and return batched movement
    // Returns: key code (or 0 if no input), moves = number of times to apply
    int processInput(Stream* stream, int& moves);
    
    // Clear pending repeated keys (call after processing)
    void clearRepeats(Stream* stream);
    
    // Check if we should apply acceleration
    bool shouldAccelerate();
};

// Global input handler instance
extern InputHandler g_input_handler;

#endif // EKILO_EDITOR_H 