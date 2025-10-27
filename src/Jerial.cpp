// SPDX-License-Identifier: MIT

#include "Jerial.h"
#include "Adafruit_TinyUSB.h"
#include "ArduinoStuff.h"
#include "Python_Proper.h" // For ScriptHistory

// External USB CDC instances (defined in ArduinoStuff.cpp)
extern Adafruit_USBD_CDC USBSer1;
extern Adafruit_USBD_CDC USBSer2;
extern Adafruit_USBD_CDC USBSer3;

// External OLED instance
extern class oled oled;

// Serial1 is a global Arduino object, available by default

// Global variable for interactive mode tracking
int termInInteractiveMode = 0;

// ============================================================================
// TermControl Implementation (Internal Class for USB Serial Endpoints)
// ============================================================================

TermControl::TermControl( Stream* underlying_stream, bool create_own_history )
    : stream( underlying_stream ),
      history( nullptr ),
      owns_history( create_own_history ),
      history_initialized( false ),
      line_length( 0 ),
      cursor_position( 0 ),
      line_ready( false ),
      echo_enabled( true ),
      syntax_highlighter( underlying_stream ),
      ansi_state( ANSI_NORMAL ),
      auto_strip_tags( false ),
      tag_buffer_pos( 0 ) {

    // Create our own history instance if requested
    if ( create_own_history ) {
        history = new ScriptHistory( );
        if ( history ) {
            history->initFilesystem( );
        }
    }

    // Initialize current line buffer and display buffer
    memset( current_line, 0, sizeof( current_line ) );
    memset( display_buffer, 0, sizeof( display_buffer ) );
    memset( tag_buffer, 0, sizeof( tag_buffer ) );
    display_length = 0;
}

TermControl::~TermControl( ) {
    // Clean up history if we own it
    if ( owns_history && history ) {
        delete history;
        history = nullptr;
    }
}

// Stream Interface Implementation
int TermControl::available( ) {
    if ( !stream )
        return 0;
    return stream->available( );
}

int TermControl::read( ) {
    if ( !stream )
        return -1;
    return stream->read( );
}

int TermControl::peek( ) {
    if ( !stream )
        return -1;
    return stream->peek( );
}

void TermControl::flush( ) {
    if ( stream ) {
        stream->flush( );
    }
}

size_t TermControl::write( uint8_t byte ) {
    if ( !stream )
        return 0;
    return stream->write( byte );
}

size_t TermControl::write( const uint8_t* buffer, size_t size ) {
    if ( !stream )
        return 0;
    return stream->write( buffer, size );
}

// Terminal-specific functionality
bool TermControl::service( ) {
    if ( !stream )
        return false;

    bool line_was_ready_before = line_ready;

    while ( stream->available( ) > 0 ) {
        char c = (char)stream->read( );
        
        // Apply tag filtering if enabled
        if ( auto_strip_tags && shouldSkipChar( c ) ) {
            // If we're in the middle of parsing a tag, wait briefly for more data
            if ( tag_buffer_pos > 0 && stream->available() == 0 ) {
                uint32_t wait_start = millis();
                while (stream->available() == 0 && (millis() - wait_start) < 5) {
                    delayMicroseconds(100);
                }
            }
            continue; // Skip this character, it's part of a tag
        }

        // Handle ANSI escape sequences
        switch ( ansi_state ) {
        case ANSI_NORMAL:
            if ( c == '\x1b' ) { // ESC
                ansi_state = ANSI_ESCAPE;
            } else {
                handleNormalChar( c );
            }
            break;

        case ANSI_ESCAPE:
            if ( c == '[' ) {
                ansi_state = ANSI_BRACKET;
            } else {
                ansi_state = ANSI_NORMAL;
                // Handle other escape sequences if needed
            }
            break;

        case ANSI_BRACKET:
            ansi_state = ANSI_NORMAL;
            switch ( c ) {
            case 'A':
                handleArrowUp( );
                break;
            case 'B':
                handleArrowDown( );
                break;
            case 'C':
                handleArrowRight( );
                break;
            case 'D':
                handleArrowLeft( );
                break;
                // Ignore other bracket sequences
            }
            break;
        
        case ANSI_MAIN_SERIAL_ENQ:
            // Handle ENQ state if needed
            ansi_state = ANSI_NORMAL;
            break;
        }

        // Return immediately if a line became ready
        if ( !line_was_ready_before && line_ready ) {
            return true;
        }
    }

    return false; // No line ready
}

bool TermControl::hasCompletedLine( ) const {
    return line_ready;
}

String TermControl::getCompletedLine( ) {
    if ( !line_ready )
        return "";

    String result = completed_line;
    completed_line = "";
    line_ready = false;
    return result;
}

String TermControl::peekCompletedLine( ) const {
    if ( !line_ready )
        return "";
    return completed_line;
}

void TermControl::clearCompletedLine( ) {
    completed_line = "";
    line_ready = false;
}

void TermControl::injectCompletedLine( const char* line ) {
    if ( !line ) {
        return;
    }
    
    // Set the completed line directly
    completed_line = String( line );
    line_ready = true;
    
    // Optionally add to history
    if ( history && strlen(line) > 0 ) {
        history->addToHistory( String( line ) );
    }
}

const char* TermControl::getCurrentLineBuffer( ) {
    return current_line;
}

void TermControl::setPrompt( const char* prompt ) {
    if ( prompt ) {
        prompt_text = String( prompt );
    } else {
        prompt_text = "";
    }
}

void TermControl::enableEcho( bool enabled ) {
    echo_enabled = enabled;
}

void TermControl::setColoredPrompt( const char* prompt, int color_code ) {
    if ( prompt ) {
        char colored_prompt[ 128 ];
        snprintf( colored_prompt, sizeof( colored_prompt ), "\x1b[38;5;%dm%s\x1b[0m", color_code, prompt );
        prompt_text = String( colored_prompt );
    } else {
        prompt_text = "";
    }
}

// Character and Line Handling
void TermControl::handleNormalChar( char c ) {
    switch ( c ) {
    case '\r':
    case '\n':
        handleEnter( );
        break;

    case '\b': // Backspace
    case 0x7F: // DEL
        handleBackspace( );
        break;

    case 0x15: // Ctrl+U - clear line
        handleCtrlU( );
        break;

    case '\t': // Tab
        handleTab( );
        break;

    case '?':
        if (stream) {
            stream->print( "Jumperless firmware version: " );
            stream->println( "5.4.9.9" ); //TODO: fix this
            stream->flush( );
        }
        break;

    default:
        if ( c >= 32 && c <= 126 ) { // Printable characters
            insertCharAtCursor( c );
            if ( echo_enabled ) {
                renderCurrentLine( );
            }
        }
        break;
    }
}

void TermControl::handleBackspace( ) {
    if ( cursor_position > 0 ) {
        cursor_position--;
        if ( cursor_position < line_length ) {
            memmove( &current_line[ cursor_position ],
                     &current_line[ cursor_position + 1 ],
                     line_length - cursor_position - 1 );
        }
        line_length--;
        display_length--;
        current_line[ line_length ] = '\0';

        if ( echo_enabled ) {
            renderCurrentLine( );
        }
    }
}

void TermControl::handleEnter( ) {
    if ( echo_enabled && stream ) {
        stream->print( JERIAL_NEWLINE_OUT );
        stream->flush( );
    }

    // Add to history if we have a history manager and non-empty line
    if ( history && line_length > 0 ) {
        current_line[ line_length ] = '\0';
        history->addToHistory( String( current_line ) );
    }

    // Make line available for parsing
    completed_line = String( current_line );
    line_ready = true;

    // Reset current line
    clearCurrentLine( );
}

void TermControl::handleArrowUp( ) {
    if ( !history )
        return;

    String prev = history->getPreviousCommand( );
    if ( prev.length( ) > 0 && prev.length( ) < JERIAL_MAX_LINE_LENGTH ) {
        strcpy( current_line, prev.c_str( ) );
        line_length = prev.length( );
        cursor_position = line_length;
        if ( echo_enabled ) {
            renderCurrentLine( );
        }
    }
}

void TermControl::handleArrowDown( ) {
    if ( !history )
        return;

    String next = history->getNextCommand( );
    if ( next.length( ) == 0 ) {
        clearCurrentLine( );
    } else if ( next.length( ) < JERIAL_MAX_LINE_LENGTH ) {
        strcpy( current_line, next.c_str( ) );
        line_length = next.length( );
        cursor_position = line_length;
    }

    if ( echo_enabled ) {
        renderCurrentLine( );
    }
}

void TermControl::handleArrowLeft( ) {
    if ( cursor_position > 0 ) {
        cursor_position--;
        if ( echo_enabled ) {
            moveCursorTo( cursor_position );
        }
    }
}

void TermControl::handleArrowRight( ) {
    if ( cursor_position < line_length ) {
        cursor_position++;
        if ( echo_enabled ) {
            moveCursorTo( cursor_position );
        }
    }
}

void TermControl::handleCtrlU( ) {
    clearCurrentLine( );
    if ( echo_enabled ) {
        renderCurrentLine( );
    }
}

void TermControl::handleTab( ) {
    // Could implement tab completion here in the future
    // For now, just ignore
}

void TermControl::handleMainSerialENQ( ) {
    replyWithSerialInfo( 1 );
}

// Terminal Rendering and Cursor Management
void TermControl::renderCurrentLine( ) {
    if ( !stream || !echo_enabled )
        return;

    // Create clean string from current line buffer
    char clean_line[ JERIAL_MAX_LINE_LENGTH + 1 ];
    memcpy( clean_line, current_line, line_length );
    clean_line[ line_length ] = '\0';

    // Get highlighted version from SyntaxHighlighting
    syntax_highlighter.setStream( stream );
    char* highlighted = syntax_highlighter.highlightString( clean_line, SYNTAX_JUMPERLESS_CONNECTIONS );

    // Store in display buffer for cursor calculations
    if ( highlighted ) {
        strncpy( display_buffer, highlighted, sizeof( display_buffer ) - 1 );
        display_buffer[ sizeof( display_buffer ) - 1 ] = '\0';
    } else {
        strncpy( display_buffer, clean_line, sizeof( display_buffer ) - 1 );
        display_buffer[ sizeof( display_buffer ) - 1 ] = '\0';
    }

    // Render the line
    stream->print( '\r' );

    // Print prompt if we have one
    if ( prompt_text.length( ) > 0 ) {
        stream->print( prompt_text );
    }

    // Print the content (either highlighted or clean)
    if ( line_length > 0 ) {
        if ( highlighted && strlen( highlighted ) > 0 ) {
            stream->print( highlighted );
            display_length = calculateVisualLength( String( highlighted ) );
        } else {
            stream->print( clean_line );
            display_length = line_length;
        }
    } else {
        display_length = 0;
    }

    // Clear to end of line to erase old characters
    stream->print( "\x1b[K" );

    // Position cursor correctly using display length
    moveCursorToPosition( cursor_position );

    stream->flush( );
}

void TermControl::clearCurrentLine( ) {
    memset( current_line, 0, sizeof( current_line ) );
    memset( display_buffer, 0, sizeof( display_buffer ) );
    line_length = 0;
    display_length = 0;
    cursor_position = 0;
}

void TermControl::moveCursorTo( int position ) {
    if ( !stream || !echo_enabled )
        return;

    int visual_prompt_len = calculateVisualLength( prompt_text );
    int target_column = visual_prompt_len + position + 1;

    stream->print( '\r' );
    if ( target_column > 1 ) {
        stream->print( "\x1b[" );
        stream->print( target_column );
        stream->print( 'G' );
    }
    stream->flush( );
}

int TermControl::calculateVisualLength( const String& text ) {
    int visual_len = 0;
    bool in_ansi = false;

    for ( int i = 0; i < text.length( ); i++ ) {
        char c = text.charAt( i );
        if ( c == '\x1b' ) {
            in_ansi = true;
        } else if ( in_ansi && c == 'm' ) {
            in_ansi = false;
        } else if ( !in_ansi ) {
            visual_len++;
        }
    }

    return visual_len;
}

void TermControl::moveCursorToPosition( int position ) {
    if ( !stream || !echo_enabled )
        return;

    int visual_prompt_len = calculateVisualLength( prompt_text );
    int visual_position = position;
    int target_column = visual_prompt_len + visual_position + 1;

    stream->print( '\r' );
    if ( target_column > 1 ) {
        stream->print( "\x1b[" );
        stream->print( target_column );
        stream->print( 'G' );
    }
    stream->flush( );
}

void TermControl::insertCharAtCursor( char c ) {
    if ( line_length >= JERIAL_MAX_LINE_LENGTH - 1 ) {
        if ( stream && echo_enabled ) {
            stream->print( "\x07" ); // Bell sound
            stream->flush( );
        }
        return;
    }

    if ( cursor_position < line_length ) {
        memmove( &current_line[ cursor_position + 1 ],
                 &current_line[ cursor_position ],
                 line_length - cursor_position );
    }

    current_line[ cursor_position ] = c;
    cursor_position++;
    line_length++;
    display_length++;
}

void TermControl::deleteCharAtCursor( ) {
    if ( cursor_position >= line_length ) {
        return;
    }

    memmove( &current_line[ cursor_position ],
             &current_line[ cursor_position + 1 ],
             line_length - cursor_position - 1 );

    line_length--;
    display_length--;
    current_line[ line_length ] = '\0';
}

// Tag Filtering
bool TermControl::shouldSkipChar( char c ) {
    if ( tag_buffer_pos == 0 ) {
        if ( c == '<' ) {
            tag_buffer[0] = '<';
            tag_buffer_pos = 1;
            return true;
        }
        return false;
    }
    
    tag_buffer[tag_buffer_pos++] = c;
    
    // Check for complete opening tag "<j>"
    if ( tag_buffer_pos == 3 && 
        tag_buffer[0] == '<' && tag_buffer[1] == 'j' && tag_buffer[2] == '>' ) {
        tag_buffer_pos = 0;
        return true;
    }
    
    // Check for complete closing tag "</j>"
    if ( tag_buffer_pos == 4 && 
        tag_buffer[0] == '<' && tag_buffer[1] == '/' && 
        tag_buffer[2] == 'j' && tag_buffer[3] == '>' ) {
        tag_buffer_pos = 0;
        return true;
    }
    
    // Check if this can't possibly be a tag anymore
    bool not_a_tag = false;
    if ( tag_buffer_pos == 2 && tag_buffer[1] != 'j' && tag_buffer[1] != '/' ) {
        not_a_tag = true;
    } else if ( tag_buffer_pos == 3 && tag_buffer[1] == 'j' && tag_buffer[2] != '>' ) {
        not_a_tag = true;
    } else if ( tag_buffer_pos == 3 && tag_buffer[1] == '/' && tag_buffer[2] != 'j' ) {
        not_a_tag = true;
    } else if ( tag_buffer_pos >= 4 ) {
        not_a_tag = true;
    }
    
    if ( not_a_tag ) {
        for ( int i = 0; i < tag_buffer_pos - 1; i++ ) {
            handleNormalChar( tag_buffer[i] );
        }
        tag_buffer_pos = 0;
        return false;
    }
    
    return true;
}


// ============================================================================
// JerialClass Implementation
// ============================================================================

// Global Jerial instance
JerialClass Jerial;

JerialClass::JerialClass() 
    : output_count(0),
      broadcast_enabled(true),
      input_stream(nullptr),
      input_source_count(0),
      term_control(nullptr),
      term_control_active(false),
      injection_read_pos(0),
      injection_write_pos(0),
      auto_strip_tags(false),
      tag_buffer_pos(0),
      in_tag(false) {
    // Initialize arrays
    for (int i = 0; i < JERIAL_MAX_OUTPUTS; i++) {
        output_streams[i] = nullptr;
        input_sources[i] = nullptr;
    }
    memset(injection_buffer, 0, sizeof(injection_buffer));
    memset(tag_buffer, 0, sizeof(tag_buffer));
}

JerialClass::~JerialClass() {
    destroyTermControl();
}

// ============================================================================
// Stream Interface Implementation
// ============================================================================

int JerialClass::available() {
    // Check injection buffer first
    if (injection_read_pos != injection_write_pos) {
        int injected_count = injection_write_pos - injection_read_pos;
        if (injected_count < 0) {
            injected_count += sizeof(injection_buffer);
        }
        return injected_count;
    }
    
    // If terminal control is active, check it
    if (term_control_active && term_control) {
        return term_control->available();
    }
    
    // Check actual input stream
    if (!input_stream) {
        return 0;
    }
    return input_stream->available();
}

int JerialClass::read() {
    // Read from injection buffer first (priority over actual input)
    if (injection_read_pos != injection_write_pos) {
        uint8_t c = injection_buffer[injection_read_pos];
        injection_read_pos = (injection_read_pos + 1) % sizeof(injection_buffer);
        return c;
    }
    
    // If terminal control is active, read through it
    if (term_control_active && term_control) {
        return term_control->read();
    }
    
    // Read from actual input stream
    if (!input_stream) {
        return -1;
    }
    
    // Apply tag filtering if enabled
    if (auto_strip_tags) {
        return readWithTagFilter();
    }
    
    return input_stream->read();
}

int JerialClass::peek() {
    // Peek at injection buffer first
    if (injection_read_pos != injection_write_pos) {
        return injection_buffer[injection_read_pos];
    }
    
    // If terminal control is active, peek through it
    if (term_control_active && term_control) {
        return term_control->peek();
    }
    
    // Peek at actual input stream
    if (!input_stream) {
        return -1;
    }
    return input_stream->peek();
}

void JerialClass::flush() {
    // Flush all output streams
    for (int i = 0; i < output_count; i++) {
        if (output_streams[i]) {
            output_streams[i]->flush();
        }
    }
}

size_t JerialClass::write(uint8_t byte) {
    return writeToOutputs(byte);
}

size_t JerialClass::write(const uint8_t *buffer, size_t size) {
    return writeToOutputs(buffer, size);
}

int JerialClass::availableForWrite() {
    if (output_count == 0) {
        return 0;
    }
    
    int min_available = INT_MAX;
    for (int i = 0; i < output_count; i++) {
        if (output_streams[i]) {
            int avail = 128; // Conservative default
            
            // Check if it's a known CDC endpoint
            if (output_streams[i] == &Serial || 
                output_streams[i] == &USBSer1 || 
                output_streams[i] == &USBSer2 || 
                output_streams[i] == &USBSer3) {
                Adafruit_USBD_CDC* cdc = static_cast<Adafruit_USBD_CDC*>(output_streams[i]);
                avail = cdc->availableForWrite();
            }
            
            if (avail < min_available) {
                min_available = avail;
            }
        }
    }
    
    return (min_available == INT_MAX) ? 0 : min_available;
}

// ============================================================================
// Terminal Control Integration
// ============================================================================

bool JerialClass::service() {
    if (term_control_active && term_control) {
        return term_control->service();
    }
    return false;
}

bool JerialClass::hasCompletedLine() const {
    if (term_control_active && term_control) {
        return term_control->hasCompletedLine();
    }
    return false;
}

String JerialClass::getCompletedLine() {
    if (term_control_active && term_control) {
        return term_control->getCompletedLine();
    }
    return "";
}

String JerialClass::peekCompletedLine() const {
    if (term_control_active && term_control) {
        return term_control->peekCompletedLine();
    }
    return "";
}

void JerialClass::clearCompletedLine() {
    if (term_control_active && term_control) {
        term_control->clearCompletedLine();
    }
}

void JerialClass::injectCompletedLine(const char* line) {
    if (term_control_active && term_control) {
        term_control->injectCompletedLine(line);
    }
}

const char* JerialClass::getCurrentLineBuffer() {
    if (term_control_active && term_control) {
        return term_control->getCurrentLineBuffer();
    }
    return "";
}

void JerialClass::setPrompt(const char* prompt) {
    if (term_control_active && term_control) {
        term_control->setPrompt(prompt);
    }
}

void JerialClass::setColoredPrompt(const char* prompt, int color_code) {
    if (term_control_active && term_control) {
        term_control->setColoredPrompt(prompt, color_code);
    }
}

void JerialClass::enableEcho(bool enabled) {
    if (term_control_active && term_control) {
        term_control->enableEcho(enabled);
    }
}

ScriptHistory* JerialClass::getHistory() {
    if (term_control_active && term_control) {
        return term_control->getHistory();
    }
    return nullptr;
}

// ============================================================================
// Output Routing
// ============================================================================

bool JerialClass::addOutputStream(Stream* stream) {
    if (!stream) {
        return false;
    }
    
    // Check if already registered
    if (hasOutputStream(stream)) {
        return true;
    }
    
    // Check if we have space
    if (output_count >= JERIAL_MAX_OUTPUTS) {
        return false;
    }
    
    // Add to array
    output_streams[output_count++] = stream;
    return true;
}

bool JerialClass::addOutputStream(JerialEndpoint endpoint) {
    Stream* stream = getStreamForEndpoint(endpoint);
    if (!stream) {
        return false;
    }
    return addOutputStream(stream);
}

bool JerialClass::removeOutputStream(Stream* stream) {
    if (!stream) {
        return false;
    }
    
    bool found = false;
    for (int i = 0; i < output_count; i++) {
        if (output_streams[i] == stream) {
            found = true;
            for (int j = i; j < output_count - 1; j++) {
                output_streams[j] = output_streams[j + 1];
            }
            output_streams[output_count - 1] = nullptr;
            output_count--;
            break;
        }
    }
    
    return found;
}

bool JerialClass::removeOutputStream(JerialEndpoint endpoint) {
    Stream* stream = getStreamForEndpoint(endpoint);
    if (!stream) {
        return false;
    }
    return removeOutputStream(stream);
}

void JerialClass::clearOutputStreams() {
    for (int i = 0; i < JERIAL_MAX_OUTPUTS; i++) {
        output_streams[i] = nullptr;
    }
    output_count = 0;
}

void JerialClass::setOutputStream(Stream* stream) {
    clearOutputStreams();
    addOutputStream(stream);
}

void JerialClass::setOutputStream(JerialEndpoint endpoint) {
    clearOutputStreams();
    addOutputStream(endpoint);
}

bool JerialClass::hasOutputStream(Stream* stream) const {
    for (int i = 0; i < output_count; i++) {
        if (output_streams[i] == stream) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// Input Routing
// ============================================================================

void JerialClass::setInputStream(Stream* stream) {
    input_stream = stream;
    
    // Create or destroy terminal control based on endpoint type
    JerialEndpoint endpoint = getEndpointType(stream);
    if (supportsTerminalControl(endpoint)) {
        createTermControlIfNeeded(stream);
    } else {
        destroyTermControl();
    }
}

void JerialClass::setInputStream(JerialEndpoint endpoint) {
    input_stream = getStreamForEndpoint(endpoint);
    
    // Create or destroy terminal control based on endpoint type
    if (supportsTerminalControl(endpoint)) {
        createTermControlIfNeeded(input_stream);
    } else {
        destroyTermControl();
    }
}

bool JerialClass::addInputSource(Stream* stream) {
    if (!stream) {
        return false;
    }
    
    // Check if already registered
    for (int i = 0; i < input_source_count; i++) {
        if (input_sources[i] == stream) {
            return true;
        }
    }
    
    // Check if we have space
    if (input_source_count >= JERIAL_MAX_OUTPUTS) {
        return false;
    }
    
    // Add to array
    input_sources[input_source_count++] = stream;
    return true;
}

bool JerialClass::addInputSource(JerialEndpoint endpoint) {
    Stream* stream = getStreamForEndpoint(endpoint);
    if (!stream) {
        return false;
    }
    return addInputSource(stream);
}

bool JerialClass::serviceInputs() {
    // Injection buffer has priority
    if (injection_read_pos != injection_write_pos) {
        return true;
    }
    
    // Check all registered input sources for available data
    for (int i = 0; i < input_source_count; i++) {
        if (input_sources[i] && input_sources[i]->available() > 0) {
            setInputStream(input_sources[i]);
            return true;
        }
    }
    return false;
}

bool JerialClass::injectInput(const char* data, bool strip_tags) {
    if (!data) {
        return false;
    }
    
    // Apply auto-strip if enabled, or explicit strip_tags parameter
    if (strip_tags || auto_strip_tags) {
        return stripTagsAndInject(data, strlen(data)) > 0;
    }
    
    return injectInput((const uint8_t*)data, strlen(data), false);
}

bool JerialClass::injectInput(const uint8_t* data, size_t size, bool strip_tags) {
    if (!data || size == 0) {
        return false;
    }
    
    // Apply auto-strip if enabled, or explicit strip_tags parameter
    if (strip_tags || auto_strip_tags) {
        return stripTagsAndInject((const char*)data, size) > 0;
    }
    
    // Check if we have space in injection buffer
    for (size_t i = 0; i < size; i++) {
        uint16_t next_write = (injection_write_pos + 1) % sizeof(injection_buffer);
        
        if (next_write == injection_read_pos) {
            return false;
        }
        
        injection_buffer[injection_write_pos] = data[i];
        injection_write_pos = next_write;
    }
    
    return true;
}

void JerialClass::clearInjectedInput() {
    injection_read_pos = 0;
    injection_write_pos = 0;
    memset(injection_buffer, 0, sizeof(injection_buffer));
}

// ============================================================================
// Tag Stripping
// ============================================================================

size_t JerialClass::stripTagsAndInject(const char* data, size_t length) {
    if (!data || length == 0) {
        return 0;
    }
    
    size_t written = 0;
    size_t i = 0;
    
    while (i < length) {
        // Check for opening tag "<j>"
        if (i + 2 < length && data[i] == '<' && data[i+1] == 'j' && data[i+2] == '>') {
            i += 3;
            continue;
        }
        
        // Check for closing tag "</j>"
        if (i + 3 < length && data[i] == '<' && data[i+1] == '/' && data[i+2] == 'j' && data[i+3] == '>') {
            i += 4;
            continue;
        }
        
        // Not a tag - inject this character
        uint16_t next_write = (injection_write_pos + 1) % sizeof(injection_buffer);
        
        if (next_write == injection_read_pos) {
            return written;
        }
        
        injection_buffer[injection_write_pos] = data[i];
        injection_write_pos = next_write;
        written++;
        i++;
    }
    
    return written;
}

int JerialClass::readWithTagFilter() {
    if (!input_stream) {
        return -1;
    }
    
    // If we have buffered characters from a non-tag sequence, return them first
    if (tag_buffer_pos > 0 && tag_buffer[0] != '<') {
        char result = tag_buffer[0];
        for (int i = 1; i < tag_buffer_pos; i++) {
            tag_buffer[i-1] = tag_buffer[i];
        }
        tag_buffer_pos--;
        return result;
    }
    
    while (true) {
        // If we have a partial tag buffer, wait briefly for more data
        if (tag_buffer_pos > 0 && input_stream->available() == 0) {
            // Wait up to 5ms for more data when we might have a tag
            uint32_t wait_start = millis();
            while (input_stream->available() == 0 && (millis() - wait_start) < 5) {
                delayMicroseconds(100); // Small delay to allow data to arrive
            }
            
            // If still no data after waiting, return buffered characters
            if (input_stream->available() == 0) {
                char result = tag_buffer[0];
                for (int i = 1; i < tag_buffer_pos; i++) {
                    tag_buffer[i-1] = tag_buffer[i];
                }
                tag_buffer_pos--;
                return result;
            }
        }
        
        int c = input_stream->read();
        if (c == -1) {
            if (tag_buffer_pos > 0) {
                char result = tag_buffer[0];
                for (int i = 1; i < tag_buffer_pos; i++) {
                    tag_buffer[i-1] = tag_buffer[i];
                }
                tag_buffer_pos--;
                return result;
            }
            return -1;
        }
        
        if (tag_buffer_pos == 0) {
            if (c == '<') {
                tag_buffer[0] = '<';
                tag_buffer_pos = 1;
                continue;
            } else {
                return c;
            }
        }
        
        tag_buffer[tag_buffer_pos++] = (char)c;
        
        // Check for complete opening tag "<j>"
        if (tag_buffer_pos == 3 && 
            tag_buffer[0] == '<' && tag_buffer[1] == 'j' && tag_buffer[2] == '>') {
            tag_buffer_pos = 0;
            continue;
        }
        
        // Check for complete closing tag "</j>"
        if (tag_buffer_pos == 4 && 
            tag_buffer[0] == '<' && tag_buffer[1] == '/' && 
            tag_buffer[2] == 'j' && tag_buffer[3] == '>') {
            tag_buffer_pos = 0;
            continue;
        }
        
        // Check if this can't possibly be a tag anymore
        bool not_a_tag = false;
        if (tag_buffer_pos == 2 && tag_buffer[1] != 'j' && tag_buffer[1] != '/') {
            not_a_tag = true;
        } else if (tag_buffer_pos == 3 && tag_buffer[1] == 'j' && tag_buffer[2] != '>') {
            not_a_tag = true;
        } else if (tag_buffer_pos == 3 && tag_buffer[1] == '/' && tag_buffer[2] != 'j') {
            not_a_tag = true;
        } else if (tag_buffer_pos >= 4) {
            not_a_tag = true;
        }
        
        if (not_a_tag) {
            char result = tag_buffer[0];
            for (int i = 1; i < tag_buffer_pos; i++) {
                tag_buffer[i-1] = tag_buffer[i];
            }
            tag_buffer_pos--;
            return result;
        }
    }
}

// ============================================================================
// Internal TermControl Management
// ============================================================================

void JerialClass::createTermControlIfNeeded(Stream* stream) {
    if (!stream) {
        return;
    }
    
    // If we already have terminal control, reuse it
    if (term_control) {
        term_control_active = true;
        return;
    }
    
    // Create new terminal control instance
    term_control = new TermControl(stream, true);
    term_control_active = true;
    
    // Apply auto strip tags setting
    if (term_control) {
        term_control->setAutoStripTags(auto_strip_tags);
    }
}

void JerialClass::destroyTermControl() {
    if (term_control) {
        delete term_control;
        term_control = nullptr;
    }
    term_control_active = false;
}

// ============================================================================
// Static Helper Functions
// ============================================================================

JerialEndpoint JerialClass::getEndpointType(Stream* stream) {
    if (!stream) {
        return JerialEndpoint::NONE;
    }
    
    if (stream == &Serial) {
        return JerialEndpoint::USB_SERIAL;
    } else if (stream == &USBSer1) {
        return JerialEndpoint::USB_SER1;
    } else if (stream == &USBSer2) {
        return JerialEndpoint::USB_SER2;
    } else if (stream == &USBSer3) {
        return JerialEndpoint::USB_SER3;
    } else if (stream == &Serial1) {
        return JerialEndpoint::SERIAL1;
    } else if (stream == &OLEDOut) {
        return JerialEndpoint::OLED;
    }
    return JerialEndpoint::CUSTOM;
}

Stream* JerialClass::getStreamForEndpoint(JerialEndpoint endpoint) {
    switch (endpoint) {
        case JerialEndpoint::USB_SERIAL:
            return &Serial;
        case JerialEndpoint::USB_SER1:
            return &USBSer1;
        case JerialEndpoint::USB_SER2:
            return &USBSer2;
        case JerialEndpoint::USB_SER3:
            return &USBSer3;
        case JerialEndpoint::SERIAL1:
            return &Serial1;
        case JerialEndpoint::OLED:
            return &OLEDOut;
        case JerialEndpoint::UART0:
        case JerialEndpoint::UART1:
            return nullptr; // User must create UARTStream instances
        default:
            return nullptr;
    }
}

const char* JerialClass::endpointToString(JerialEndpoint endpoint) {
    switch (endpoint) {
        case JerialEndpoint::NONE:        return "NONE";
        case JerialEndpoint::USB_SERIAL:  return "USB_SERIAL";
        case JerialEndpoint::USB_SER1:    return "USB_SER1";
        case JerialEndpoint::USB_SER2:    return "USB_SER2";
        case JerialEndpoint::USB_SER3:    return "USB_SER3";
        case JerialEndpoint::SERIAL1:     return "SERIAL1";
        case JerialEndpoint::UART0:       return "UART0";
        case JerialEndpoint::UART1:       return "UART1";
        case JerialEndpoint::OLED:        return "OLED";
        case JerialEndpoint::CUSTOM:      return "CUSTOM";
        default:                          return "UNKNOWN";
    }
}

bool JerialClass::supportsTerminalControl(JerialEndpoint endpoint) {
    switch (endpoint) {
        case JerialEndpoint::USB_SERIAL:
        case JerialEndpoint::USB_SER1:
        case JerialEndpoint::USB_SER2:
        case JerialEndpoint::USB_SER3:
            return true;
        default:
            return false;
    }
}

// ============================================================================
// Direct Print Methods - Override normal routing
// ============================================================================

size_t JerialClass::printTo(JerialEndpoint endpoint, const char* data) {
    Stream* stream = getStreamForEndpoint(endpoint);
    if (!stream || !data) {
        return 0;
    }
    return stream->print(data);
}

size_t JerialClass::printTo(JerialEndpoint endpoint, const String& data) {
    Stream* stream = getStreamForEndpoint(endpoint);
    if (!stream) {
        return 0;
    }
    return stream->print(data);
}

size_t JerialClass::printTo(JerialEndpoint endpoint, uint8_t byte) {
    Stream* stream = getStreamForEndpoint(endpoint);
    if (!stream) {
        return 0;
    }
    return stream->write(byte);
}

size_t JerialClass::printTo(JerialEndpoint endpoint, const uint8_t* buffer, size_t size) {
    Stream* stream = getStreamForEndpoint(endpoint);
    if (!stream || !buffer || size == 0) {
        return 0;
    }
    return stream->write(buffer, size);
}

size_t JerialClass::printTo(Stream* stream, const char* data) {
    if (!stream || !data) {
        return 0;
    }
    return stream->print(data);
}

size_t JerialClass::printTo(Stream* stream, const String& data) {
    if (!stream) {
        return 0;
    }
    return stream->print(data);
}

size_t JerialClass::printTo(Stream* stream, uint8_t byte) {
    if (!stream) {
        return 0;
    }
    return stream->write(byte);
}

size_t JerialClass::printTo(Stream* stream, const uint8_t* buffer, size_t size) {
    if (!stream || !buffer || size == 0) {
        return 0;
    }
    return stream->write(buffer, size);
}

size_t JerialClass::printlnTo(JerialEndpoint endpoint, const char* data) {
    Stream* stream = getStreamForEndpoint(endpoint);
    if (!stream) {
        return 0;
    }
    return stream->println(data);
}

size_t JerialClass::printlnTo(JerialEndpoint endpoint, const String& data) {
    Stream* stream = getStreamForEndpoint(endpoint);
    if (!stream) {
        return 0;
    }
    return stream->println(data);
}

size_t JerialClass::printlnTo(Stream* stream, const char* data) {
    if (!stream) {
        return 0;
    }
    return stream->println(data);
}

size_t JerialClass::printlnTo(Stream* stream, const String& data) {
    if (!stream) {
        return 0;
    }
    return stream->println(data);
}

// ============================================================================
// Internal Write Functions
// ============================================================================

size_t JerialClass::writeToOutputs(uint8_t byte) {
    if (output_count == 0) {
        return 0;
    }
    
    size_t written = 0;
    
    if (broadcast_enabled) {
        for (int i = 0; i < output_count; i++) {
            if (output_streams[i]) {
                size_t result = output_streams[i]->write(byte);
                if (i == 0) {
                    written = result;
                }
            }
        }
    } else {
        if (output_streams[0]) {
            written = output_streams[0]->write(byte);
        }
    }
    
    return written;
}

size_t JerialClass::writeToOutputs(const uint8_t *buffer, size_t size) {
    if (output_count == 0 || !buffer || size == 0) {
        return 0;
    }
    
    size_t written = 0;
    
    if (broadcast_enabled) {
        for (int i = 0; i < output_count; i++) {
            if (output_streams[i]) {
                size_t result = output_streams[i]->write(buffer, size);
                if (i == 0) {
                    written = result;
                }
            }
        }
    } else {
        if (output_streams[0]) {
            written = output_streams[0]->write(buffer, size);
        }
    }
    
    return written;
}


// ============================================================================
// OLEDStream Implementation (with enhanced filtering)
// ============================================================================

// Global instance
OLEDStream OLEDOut;

OLEDStream::OLEDStream() 
    : current_line(0),
      current_col(0),
      max_lines(4), // Default for 32px height, will be recalculated
      current_font(DEFAULT_SMALL_FONT),
      auto_update(true),
      scroll_enabled(true),
      in_ansi_escape(false),
      last_was_newline(false) {
    // Initialize line buffer
    for (int i = 0; i < OLEDSTREAM_MAX_POSSIBLE_LINES; i++) {
        memset(line_buffer[i], 0, OLEDSTREAM_LINE_LENGTH);
    }
    recalculateMaxLines();
}

OLEDStream::~OLEDStream() {
    // Nothing to clean up
}

// Stream Interface Implementation
int OLEDStream::available() {
    return 0; // Write-only
}

int OLEDStream::read() {
    return -1; // Write-only
}

int OLEDStream::peek() {
    return -1; // Write-only
}

void OLEDStream::flush() {
    updateDisplay();
}

size_t OLEDStream::write(uint8_t byte) {
    if (!isConnected()) {
        return 0;
    }
    
    char c = (char)byte;
    
    // Handle ANSI escape sequences - filter them out
    if (c == '\x1b') { // ESC
        in_ansi_escape = true;
        return 1;
    }
    
    if (in_ansi_escape) {
        // Stay in escape mode until we see 'm' (color code end) or other terminators
        if (c == 'm' || c == 'H' || c == 'J' || c == 'K' || c == 'G') {
            in_ansi_escape = false;
        }
        return 1; // Character consumed, don't display
    }
    
    // Handle special characters
    if (c == '\n' || c == '\r') {
        // Filter consecutive newlines - only process if last wasn't a newline
        if (!last_was_newline) {
            newline();
            last_was_newline = true;
        }
        // Otherwise skip it
    } else if (c == '\t') {
        // Tab = 4 spaces
        for (int i = 0; i < 4; i++) {
            printChar(' ');
        }
        last_was_newline = false;
    } else if (c == '\b') {
        // Backspace
        if (current_col > 0) {
            current_col--;
            line_buffer[current_line][current_col] = ' ';
        }
        last_was_newline = false;
    } else if (c >= 32 && c <= 126) {
        // Printable ASCII character
        printChar(c);
        last_was_newline = false;
    }
    // Ignore other control characters
    
    if (auto_update) {
        updateDisplay();
    }
    
    return 1;
}

size_t OLEDStream::write(const uint8_t *buffer, size_t size) {
    if (!isConnected() || !buffer) {
        return 0;
    }
    
    for (size_t i = 0; i < size; i++) {
        bool saved_auto = auto_update;
        if (i < size - 1) {
            auto_update = false;
        }
        
        write(buffer[i]);
        
        auto_update = saved_auto;
    }
    
    return size;
}

// OLED-specific Functions
void OLEDStream::setSmallFont(SmallFont font) {
    current_font = font;
    if (auto_update) {
        updateDisplay();
    }
}

void OLEDStream::clear() {
    // Recalculate max lines in case display size changed
    recalculateMaxLines();
    
    for (int i = 0; i < max_lines; i++) {
        memset(line_buffer[i], 0, OLEDSTREAM_LINE_LENGTH);
    }
    
    current_line = 0;
    current_col = 0;
    last_was_newline = false;
    in_ansi_escape = false;
    
    if (isConnected()) {
        oled.clear();
        oled.show();
    }
}

bool OLEDStream::isConnected() const {
    return oled.isConnected();
}

// Internal Helpers
void OLEDStream::printChar(char c) {
    if (current_col >= OLEDSTREAM_LINE_LENGTH - 1) {
        newline();
    }
    
    line_buffer[current_line][current_col++] = c;
    line_buffer[current_line][current_col] = '\0';
}

void OLEDStream::newline() {
    current_col = 0;
    current_line++;
    
    if (current_line >= max_lines) {
        if (scroll_enabled) {
            scrollUp();
            current_line = max_lines - 1;
        } else {
            current_line = 0;
        }
    }
    
    memset(line_buffer[current_line], 0, OLEDSTREAM_LINE_LENGTH);
}

void OLEDStream::scrollUp() {
    for (int i = 0; i < max_lines - 1; i++) {
        memcpy(line_buffer[i], line_buffer[i + 1], OLEDSTREAM_LINE_LENGTH);
    }
    
    memset(line_buffer[max_lines - 1], 0, OLEDSTREAM_LINE_LENGTH);
}

void OLEDStream::updateDisplay() {
    if (!isConnected()) {
        return;
    }
    
    oled.clearFramebuffer();
    oled.setSmallFont(current_font);
    
    for (int i = 0; i < max_lines; i++) {
        if (line_buffer[i][0] != '\0') {
            int16_t y = (i * 8) + 8;
            oled.setCursor(0, y);
            oled.print(line_buffer[i]);
        }
    }
    
    oled.restoreNormalFont();
    oled.show();
}

void OLEDStream::recalculateMaxLines() {
    if (isConnected()) {
        max_lines = oled.displayHeight / 8;
        // Ensure we don't exceed buffer size
        if (max_lines > OLEDSTREAM_MAX_POSSIBLE_LINES) {
            max_lines = OLEDSTREAM_MAX_POSSIBLE_LINES;
        }
        // Minimum of 1 line
        if (max_lines < 1) {
            max_lines = 1;
        }
    } else {
        max_lines = 4; // Default for 32px height
    }
}


// ============================================================================
// UARTStream Implementation
// ============================================================================

UARTStream::UARTStream(uart_inst_t* uart_inst, uint tx_pin_num, uint rx_pin_num)
    : uart(uart_inst),
      tx_pin(tx_pin_num),
      rx_pin(rx_pin_num),
      current_baud(115200),
      initialized(false),
      has_peeked(false),
      peek_byte(0) {
}

UARTStream::~UARTStream() {
    if (initialized) {
        end();
    }
}

// Initialization
void UARTStream::begin(uint32_t baud, uint data_bits, uint stop_bits, uart_parity_t parity) {
    if (initialized) {
        end();
    }
    
    gpio_set_function(tx_pin, GPIO_FUNC_UART);
    gpio_set_function(rx_pin, GPIO_FUNC_UART);
    
    uart_init(uart, baud);
    uart_set_format(uart, data_bits, stop_bits, parity);
    uart_set_fifo_enabled(uart, true);
    
    current_baud = baud;
    initialized = true;
    has_peeked = false;
}

void UARTStream::end() {
    if (!initialized) {
        return;
    }
    
    uart_tx_wait_blocking(uart);
    uart_deinit(uart);
    
    initialized = false;
}

void UARTStream::setBaudRate(uint32_t baud) {
    if (!initialized) {
        return;
    }
    
    uart_set_baudrate(uart, baud);
    current_baud = baud;
}

void UARTStream::setFlowControl(bool enable, uint cts_pin_num, uint rts_pin_num) {
    if (!initialized) {
        return;
    }
    
    if (enable) {
        uart_set_hw_flow(uart, true, true);
        gpio_set_function(cts_pin_num, GPIO_FUNC_UART);
        gpio_set_function(rts_pin_num, GPIO_FUNC_UART);
    } else {
        uart_set_hw_flow(uart, false, false);
    }
}

// Stream Interface Implementation
int UARTStream::available() {
    if (!initialized) {
        return 0;
    }
    
    int count = 0;
    
    if (has_peeked) {
        count++;
    }
    
    if (uart_is_readable(uart)) {
        count = 1;
    }
    
    return count + (has_peeked ? 1 : 0);
}

int UARTStream::read() {
    if (!initialized) {
        return -1;
    }
    
    if (has_peeked) {
        has_peeked = false;
        return peek_byte;
    }
    
    if (uart_is_readable(uart)) {
        return uart_getc(uart);
    }
    
    return -1;
}

int UARTStream::peek() {
    if (!initialized) {
        return -1;
    }
    
    if (has_peeked) {
        return peek_byte;
    }
    
    if (uart_is_readable(uart)) {
        peek_byte = uart_getc(uart);
        has_peeked = true;
        return peek_byte;
    }
    
    return -1;
}

void UARTStream::flush() {
    if (!initialized) {
        return;
    }
    
    uart_tx_wait_blocking(uart);
}

size_t UARTStream::write(uint8_t byte) {
    if (!initialized) {
        return 0;
    }
    
    uart_putc_raw(uart, byte);
    
    return 1;
}

size_t UARTStream::write(const uint8_t *buffer, size_t size) {
    if (!initialized || !buffer) {
        return 0;
    }
    
    for (size_t i = 0; i < size; i++) {
        uart_putc_raw(uart, buffer[i]);
    }
    
    return size;
}

int UARTStream::availableForWrite() {
    if (!initialized) {
        return 0;
    }
    
    if (uart_is_writable(uart)) {
        return 32;
    }
    
    return 0;
}

// UART-specific Functions
bool UARTStream::isWritable() const {
    if (!initialized) {
        return false;
    }
    
    return uart_is_writable(uart);
}

bool UARTStream::isReadable() const {
    if (!initialized) {
        return false;
    }
    
    return uart_is_readable(uart) || has_peeked;
}

