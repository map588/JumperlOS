#include "oled.h"
#include "States.h"
#include "configManager.h"
#include "PersistentStuff.h"
#include "LEDs.h"
#include "MatrixState.h"
#include "Probing.h"
#include "Apps.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "Wire.h"
#include "Peripherals.h"
#include "RotaryEncoder.h"
#include "config.h"
#include "CH446Q.h"
#include "Commands.h"
#include "FileParsing.h"
#include "ArduinoStuff.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <vector>
#include <new>
#include "Graphics.h"
//#include "font_bounds_example.h"
#include "Adafruit_GFX.h"
#include "Highlighting.h"
bool oledConnected = false;
bool oledUsingHardwiredPins = false; // Global flag: true if using RP6/RP7 (GPIO 6/7), false if using crossbar

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

Adafruit_SSD1306 display(jumperlessConfig.top_oled.width, jumperlessConfig.top_oled.height, &Wire1, OLED_RESET);

int oledAddress = -1;
int numFonts = 26;

// Global instance
class oled oled;

// // Font family mapping for automatic size selection
// enum FontFamily {
//     FONT_EUROSTILE = 0,
//     //FONT_BERKELEY = 1,
//     FONT_JOKERMAN = 2, 
//     FONT_COMIC_SANS = 3,
//     FONT_COURIER_NEW = 4,
//     FONT_NEW_SCIENCE_MEDIUM = 5,
//     FONT_NEW_SCIENCE_MEDIUM_EXTENDED = 6,
//     //FONT_JUMPERLESS = 3
// };

struct font fontList[] = {
  // Core fonts - Size 1 (small) and Size 2 (large) for auto-fallback
  { &Eurostile8pt7b, "Eurostl", "Eurostile", 0, FONT_EUROSTILE },                           // Index 0 - Size 1
  { &Eurostile12pt7b, "Eurostl", "Eurostile", 0, FONT_EUROSTILE },                          // Index 1 - Size 2
  { &Jokerman8pt7b, "Jokermn", "Jokerman", 0, FONT_JOKERMAN },                              // Index 2 - Size 1
  { &Jokerman12pt7b, "Jokermn", "Jokerman", 0, FONT_JOKERMAN },                             // Index 3 - Size 2
  { &Comic_Sans8pt7b, "ComicSns", "Comic Sans", 0, FONT_COMIC_SANS },                       // Index 4 - Size 1
  { &Comic_Sans12pt7b, "ComicSns", "Comic Sans", 0, FONT_COMIC_SANS },                      // Index 5 - Size 2
  { &Courier_New8pt7b, "Courier", "Courier New", 0, FONT_COURIER_NEW },                     // Index 6 - Size 1
  { &Courier_New12pt7b, "Courier", "Courier New", 0, FONT_COURIER_NEW },                    // Index 7 - Size 2
  { &new_science_medium8pt7b, "Science", "New Science", 0, FONT_NEW_SCIENCE_MEDIUM },       // Index 8 - Size 1
  { &new_science_medium12pt7b, "Science", "New Science", 0, FONT_NEW_SCIENCE_MEDIUM },      // Index 9 - Size 2
  { &new_science_medium_extended8pt7b, "SciExt", "New Science Ext", 0, FONT_NEW_SCIENCE_MEDIUM_EXTENDED },  // Index 10 - Size 1
  { &new_science_medium_extended12pt7b, "SciExt", "New Science Ext", 0, FONT_NEW_SCIENCE_MEDIUM_EXTENDED }, // Index 11 - Size 2
  { &ANDALEMO5pt7b, "AndlMno", "Andale Mono", 0, FONT_ANDALE_MONO },                        // Index 12 - Size 1
  { &FreeMono4pt7b, "FreMno", "Free Mono", 0, FONT_FREE_MONO },                             // Index 13 - Size 1
  
  // New fonts - Size 1 (small) and Size 2 (large)
  { &BerkeleyMono8pt7b, "BerkMono", "Berkeley Mono", 0, FONT_BERKELEY_MONO },               // Index 14 - Size 1
  { &BerkeleyMono12pt7b, "BerkMono", "Berkeley Mono", 0, FONT_BERKELEY_MONO },              // Index 15 - Size 2
  { &Pragmatism8pt7b, "Pragmtsm", "Pragmatism", 0, FONT_PRAGMATISM },                       // Index 16 - Size 1
  { &Pragmatism12pt7b, "Pragmtsm", "Pragmatism", 0, FONT_PRAGMATISM },                      // Index 17 - Size 2
  { &IosevkaSS08_Regular9pt7b, "IosevkaR", "Iosevka Regular", -1, FONT_IOSEVKA_REGULAR },   // Index 18 - Size 1
  { &IosevkaSS08_Regular11pt7b, "IosevkaR", "Iosevka Regular", -1, FONT_IOSEVKA_REGULAR },  // Index 19 - Size 2

  // Small fonts for file manager
  { &ubuntu5pt7b, "Ubuntu", "Ubuntu", 0, FONT_EUROSTILE },                                   // Index 20 - Small font
  { &DotGothic16_Regular4pt7b, "DotGoth", "DotGothic", 0, FONT_EUROSTILE },                  // Index 21 - Small font
  { &IosevkaSS08_Light5pt7b, "IosevkaS5", "Iosevka Small 5pt", 0, FONT_IOSEVKA_REGULAR },    // Index 22 - Small font
  { &Pragmatism5pt7b, "PragS5", "Pragmatism Small 5pt", 0, FONT_PRAGMATISM },                // Index 23 - Small font
  { &FreeMono5pt7b, "FreMonoS5", "Free Mono Small 5pt", 0, FONT_FREE_MONO },                 // Index 24 - Small font
  { &EnvyCodeRNerdFont_Regular5pt7b, "EnvyS5", "EnvyCode Small 5pt", 0, FONT_EUROSTILE },    // Index 25 - Small font
};

// Font family mappings: {size1_index, size2_index}
// -1 means no variant available for that size
struct FontSizeMapping {
    int size1Index;
    int size2Index;
};

FontSizeMapping fontFamilyMap[] = {
    {0, 1},   // FONT_EUROSTILE: 8pt for size 1, 12pt for size 2
    {2, 3},   // FONT_JOKERMAN: 8pt for size 1, 12pt for size 2
    {4, 5},   // FONT_COMIC_SANS: 8pt for size 1, 12pt for size 2
    {6, 7},   // FONT_COURIER_NEW: 8pt for size 1, 12pt for size 2
    {8, 9},   // FONT_NEW_SCIENCE_MEDIUM: 8pt for size 1, 12pt for size 2
    {10, 11}, // FONT_NEW_SCIENCE_MEDIUM_EXTENDED: 8pt for size 1, 12pt for size 2
    {12, 12}, // FONT_ANDALE_MONO: 5pt for both sizes
    {13, 13}, // FONT_FREE_MONO: 4pt for both sizes
    {18, 19}, // FONT_IOSEVKA_REGULAR: 9pt for size 1, 11pt for size 2 (index 8 in enum)
    {14, 15}, // FONT_BERKELEY_MONO: 8pt for size 1, 12pt for size 2 (index 9 in enum)
    {16, 17}, // FONT_PRAGMATISM: 8pt for size 1, 12pt for size 2 (index 10 in enum)
};

// SIMPLIFIED POSITIONING SYSTEM
// ============================

// Constructor
oled::oled() {}




// Initialization
int oled::init() {
    if (jumperlessConfig.top_oled.enabled == 0) {
        jumperlessConfig.top_oled.enabled = 1;
    }
    
    int success = 0;
    address = jumperlessConfig.top_oled.i2c_address;
    sda_pin = jumperlessConfig.top_oled.sda_pin;
    scl_pin = jumperlessConfig.top_oled.scl_pin;
    sda_row = jumperlessConfig.top_oled.sda_row;
    scl_row = jumperlessConfig.top_oled.scl_row;
    
    // Check if using hardwired GPIO 6/7 (RP6/RP7)
    oledUsingHardwiredPins = (sda_pin == 6 && scl_pin == 7);
    
    // Read display dimensions from config
    displayWidth = jumperlessConfig.top_oled.width;
    displayHeight = jumperlessConfig.top_oled.height;
    
    // Note: The global display object is constructed with initial width/height from config
    // Changing dimensions at runtime requires the display object to be reconstructed with new dimensions
    // This is a limitation of the static global display object
    // Workaround: Edit config file and restart to change display dimensions
    // Serial.println("oled::init");
    // Serial.println(millis());

    success = connect();
    // Serial.println("oled::init connect done");
    // Serial.println(millis());
    // delay(100);
    if (checkConnection() == false) {
        oledConnected = false;
        // Serial.println("oled::init checkConnection failed");
        // Serial.println(millis());
        // Serial.flush();
       /// disconnect();
        return 0;
    }
    // Set font from config value
    if (jumperlessConfig.top_oled.font >= 0 && jumperlessConfig.top_oled.font <= 10) {
        int fontIndex = configValueToFontIndex[jumperlessConfig.top_oled.font];
        setFont(fontIndex);
    } else {
        // Fallback to Eurostile
        setFont(0);
    }
    
    display.begin(SSD1306_SWITCHCAPVCC, address, false, false);
    
    // Apply rotation from config (0 = 0°, 1 = 90°, 2 = 180°, 3 = 270°)
    display.setRotation(jumperlessConfig.top_oled.rotation);
    
    display.setTextColor(SSD1306_WHITE);
    display.invertDisplay(false);
    display.setFont(currentFont);    
    display.clearDisplay();
    
    // Center the logo on the display
    int x = (displayWidth - jumperlessConfig.top_oled.width) / 2;
    int y = (displayHeight - jumperlessConfig.top_oled.height) / 2 + 1;
    if (strcmp(jumperlessConfig.top_oled.startup_message, "") != 0) {
        clearPrintShow(jumperlessConfig.top_oled.startup_message, 2, true, true, true);
    } else {
        display.drawBitmap(x, y, jogo32h, 128, 32, SSD1306_WHITE);
    }
    display.display();
    
    setCursor(0, 0);
    Wire1.setTimeout(15);
    charPos = 0;
    refreshConnections(-1);
    
    return success;
}

// Helper to check I2C communication
bool oled::checkConnection(void) {
    // if (jumperlessConfig.top_oled.enabled == 0) {
    //     oledConnected = false;
    //     return false;
    // }
    //if (millis() - lastConnectionCheck > 1000) {
        Wire1.beginTransmission(address);
        if (Wire1.endTransmission() != 0) {
            lastConnectionCheck = millis();
            oledConnected = false;
            return false;
        }
        lastConnectionCheck = millis();
        oledConnected = true;
    //}
    return true;
}

// Font management

// Map config value (0-10) to font index (size 2 / large version)
int configValueToFontIndex[] = {
    1,  // 0: Eurostile → index 1 (12pt)
    3,  // 1: Jokerman → index 3 (12pt)
    5,  // 2: Comic Sans → index 5 (12pt)
    7,  // 3: Courier → index 7 (12pt)
    9,  // 4: Science → index 9 (12pt)
    11, // 5: SciExt → index 11 (12pt)
    12, // 6: Andale → index 12 (5pt - no large version)
    13, // 7: FreeMono → index 13 (4pt - no large version)
    15, // 8: BerkMono → index 15 (12pt)
    17, // 9: Pragmatism → index 17 (12pt)
    19  // 10: IosevkaR → index 19 (11pt)
};

// Map config value (0-10) to FontFamily enum
FontFamily mapConfigValueToFontFamily(int configValue) {
    switch (configValue) {
        case 0: return FONT_EUROSTILE;
        case 1: return FONT_JOKERMAN;
        case 2: return FONT_COMIC_SANS;
        case 3: return FONT_COURIER_NEW;
        case 4: return FONT_NEW_SCIENCE_MEDIUM;
        case 5: return FONT_NEW_SCIENCE_MEDIUM_EXTENDED;
        case 6: return FONT_ANDALE_MONO;
        case 7: return FONT_FREE_MONO;
        case 8: return FONT_BERKELEY_MONO;
        case 9: return FONT_PRAGMATISM;
        case 10: return FONT_IOSEVKA_REGULAR;
        default: return FONT_EUROSTILE;
    }
}

int oled::cycleFont(void) {
    currentFontFamily = (FontFamily)(currentFontFamily + 1);
    if (currentFontFamily > FONT_PRAGMATISM) {
        currentFontFamily = FONT_EUROSTILE;
    }
    setFontForSize(currentFontFamily, currentTextSize);
    clearPrintShow((String)fontList[fontFamilyMap[currentFontFamily].size2Index].longName, 2);
    
    // Map FontFamily back to config value for proper saving
    int configValue = (int)currentFontFamily; // Direct mapping works now!
    jumperlessConfig.top_oled.font = configValue;
    saveConfig();
    return currentFontFamily;
}

FontFamily oled::getFontFamily(String fontName) {
    for (int i = 0; i < numFonts; i++) {
        if (fontList[i].longName == fontName) {
            return fontList[i].family;
        }
        if (fontList[i].shortName == fontName) {
            return fontList[i].family;
        }
    }
    return FONT_EUROSTILE;
}
int oled::setFont(String fontName, int justGetIndex) {
    for (int i = 0; i < numFonts; i++) {
        if (fontList[i].longName == fontName) {
            if (justGetIndex == 0) {
                setFont(i);    
            }
            return i;
        }
        if (fontList[i].shortName == fontName) {
            if (justGetIndex == 0) {
                setFont(i);
            }
            return i;
        }
           
    }
    return -1;
}
void oled::setFont(const GFXfont* font) {
    currentFont = font;
    
    display.setFont(currentFont);

    for (int i = 0; i < numFonts; i++) {
        if (fontList[i].font == currentFont) {
            currentFontFamily = fontList[i].family;
            break;
        }
    }
}
int oled::setFont(char* fontName, int justGetIndex) {
    return setFont((String)fontName, justGetIndex);
}
void oled::setFont(FontFamily fontFamily) {
    currentFontFamily = fontFamily;
    currentFont = fontList[fontFamily].font;
    display.setFont(currentFont);
}
void oled::setFont(int fontIndex) {
    if (fontIndex < 0 || fontIndex >= numFonts) {
        currentFont = fontList[0].font;
    } else {
        currentFont = fontList[fontIndex].font;
    }
    currentFontFamily = fontList[fontIndex].family;
    
    
    display.setFont(currentFont);
}

// Smart font selection based on family and text size
void oled::setFontForSize(FontFamily family, int textSize) {
    if (family < 0 || family > FONT_PRAGMATISM) {
        family = FONT_EUROSTILE; // Default to Eurostile
    }
    currentFontFamily = family;
    FontSizeMapping mapping = fontFamilyMap[family];
    int fontIndex;
    
    // Enhanced size mapping:
    // 0 = 4-5pt (extra small) - use small fonts from index 20-25
    // 1 = 8-9pt (small) - use size1Index
    // 2 = 11-12pt (large) - use size2Index
    
    if (textSize == 0) {
        // Extra small - use dedicated 4-5pt fonts
        switch (family) {
            case FONT_FREE_MONO:
                fontIndex = 24; // FreeMono5pt
                break;
            case FONT_PRAGMATISM:
                fontIndex = 23; // Pragmatism5pt
                break;
            case FONT_ANDALE_MONO:
                fontIndex = 12; // ANDALEMO5pt
                break;
            default:
                fontIndex = 23; // Default to Pragmatism5pt for best readability
                break;
        }
    } else if (textSize == 1) {
        // Use size 1 font (8-9pt), fallback to size 2 if not available
        fontIndex = (mapping.size1Index != -1) ? mapping.size1Index : mapping.size2Index;
    } else {
        // Use size 2 font (11-12pt), fallback to size 1 if not available
        fontIndex = (mapping.size2Index != -1) ? mapping.size2Index : mapping.size1Index;
    }
    
    // Final fallback to first font if something goes wrong
    if (fontIndex == -1) {
        fontIndex = 0;
    }
    
    setFont(fontIndex);
}

String oled::getFontName(FontFamily fontFamily) {

    for (int i = 0; i < numFonts; i++) {
        if (fontList[i].family == fontFamily) {
            return fontList[i].longName;
        }
    }
    return "Unknown";
}

// CORE POSITIONING FUNCTIONS
// ==========================

// Get character width for current font (useful for monospaced fonts)
int oled::getCharacterWidth() {
    if (!currentFont) {
        return 6; // Default monospace width
    }
    
    // For GFX fonts, get the advance width of a typical character (space)
    if (currentFont->first <= 0x20 && 0x20 <= currentFont->last) {
        GFXglyph *glyph = (GFXglyph *)&currentFont->glyph[0x20 - currentFont->first]; // space character
        return glyph->xAdvance * currentTextSize;
    }
    
    // Fallback: get advance of first available character
    if (currentFont->glyph) {
        GFXglyph *glyph = (GFXglyph *)&currentFont->glyph[0];
        return glyph->xAdvance * currentTextSize;
    }
    
    return 6; // Default fallback
}

// Get font metrics with proper text size scaling
FontMetrics oled::getFontMetrics() {
    FontMetrics metrics = {0};
    
    if (!currentFont) {
        // Default font metrics scaled by text size
        metrics.lineHeight = 8 * currentTextSize;
        metrics.ascent = 8 * currentTextSize;
        metrics.descent = 0;
        metrics.maxWidth = 6 * currentTextSize;
        return metrics;
    }
    
    // For GFX fonts, calculate proper metrics
    metrics.lineHeight = currentFont->yAdvance * currentTextSize;
    
    // Calculate real ascent by examining font glyphs
    int16_t maxAscent = 0;
    for (uint8_t c = currentFont->first; c <= currentFont->last && c < currentFont->first + 10; c++) {
        GFXglyph *glyph = (GFXglyph *)&currentFont->glyph[c - currentFont->first];
        int16_t ascent = -glyph->yOffset;
        if (ascent > maxAscent) maxAscent = ascent;
    }
    
    if (maxAscent == 0) {
        maxAscent = currentFont->yAdvance * 0.75; // Conservative estimate
    }
    
    metrics.ascent = maxAscent * currentTextSize;
    metrics.descent = (currentFont->yAdvance - maxAscent) * currentTextSize;
    metrics.maxWidth = currentFont->yAdvance * currentTextSize;
    
    return metrics;
}

// Get text bounds using current font and text size
TextBounds oled::getTextBounds(const char* str) {
    TextBounds bounds = {0};
    
    if (!str || strlen(str) == 0) {
        return bounds;
    }
    
    // Use Adafruit GFX's built-in function for accuracy
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
    
    bounds.width = w;
    bounds.height = h;
    bounds.x1 = x1;
    bounds.y1 = y1;
    bounds.x2 = x1 + w;
    bounds.y2 = y1 + h;
    
    // Calculate ascent and descent for positioning
    if (currentFont) {
        bounds.ascent = -y1;
        bounds.descent = h + y1;
        bounds.baseline = bounds.ascent;
    } else {
        bounds.ascent = h;
        bounds.descent = 0;
        bounds.baseline = h;
    }

    // Serial.print("bounds.ascent: ");
    // Serial.println(bounds.ascent);
    // Serial.print("bounds.descent: ");
    // Serial.println(bounds.descent);
    // Serial.print("bounds.baseline: ");
    // Serial.println(bounds.baseline);
    
    return bounds;
}

// String versions
TextBounds oled::getTextBounds(const String& str) {
    return getTextBounds(str.c_str());
}

// Get position to center text (unified function)
void oled::getCenteredPosition(const char* str, int16_t* x, int16_t* y, PositionMode mode) {
    if (!str || !x || !y) return;
    
    // Use Adafruit GFX's built-in getTextBounds for accurate centering
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
    
    // Center horizontally
    *x = (displayWidth - w) / 2;
    // Serial.print("w: ");
    // Serial.println(w);
    // Serial.print("h: ");
    // Serial.println(h);
    // Serial.print("y1: ");
    // Serial.println(y1);
    // Serial.print("y: ");
    // Serial.println(*y);
    // Serial.println("--------------------------------");

    
    // Get text bounds to use ascent for proper centering
    TextBounds bounds = getTextBounds(str);
    
    // Vertical positioning - return the Y coordinate that setCursor should use
    if (mode == POS_TIGHT || (mode == POS_AUTO && currentTextSize > 1)) {
        // For larger text, center only the ascent (visible part above baseline)
        // Put baseline at screen_center + (ascent/2) so visual center is at screen center
        *y = (displayHeight / 2) + (bounds.ascent / 2);
    } else {
        // Standard baseline positioning - center only the ascent
        *y = (displayHeight / 2) + (bounds.ascent / 2);
    }
}

// Default to auto mode
void oled::getCenteredPosition(const char* str, int16_t* x, int16_t* y) {
    getCenteredPosition(str, x, y, POS_AUTO);
}

// Check if text fits on display
bool oled::textFits(const char* str) {
    if (!str) return true;
    TextBounds bounds = getTextBounds(str);
    return (bounds.width <= displayWidth && bounds.height <= displayHeight);
}

// UNIFIED CURSOR POSITIONING
// =========================

// Main cursor setting function with automatic positioning logic
void oled::setCursor(int16_t x, int16_t y, PositionMode mode) {
    if (!oledConnected) return;
    
    if (!currentFont) {
        // Default font - direct positioning
        display.setCursor(x, y);
        return;
    }
    
    // For GFX fonts, apply intelligent positioning
    FontMetrics metrics = getFontMetrics();
    int16_t finalY = y;
    
    // Determine positioning mode
    if (mode == POS_AUTO) {
        // Auto mode: use tight positioning for y=0 or large text
        if (y == 0 || currentTextSize > 1) {
            mode = POS_TIGHT;
        } else {
            mode = POS_BASELINE;
        }
    }
    
    if (mode == POS_TIGHT) {
        // Tight positioning: for special case of y=0, position at top
        if (y == 0) {
            // Special case for top of screen - position baseline so text starts at y=0
            finalY = metrics.ascent;
            
            // Apply font-specific offset for multi-line displays
            for (int i = 0; i < numFonts; i++) {
                if (fontList[i].font == currentFont && fontList[i].topRowOffset != 0) {
                    if (displayHeight >= 24) { // Multi-line capable
                        finalY += fontList[i].topRowOffset; // Can be positive or negative
                    }
                    break;
                }
            }
        } else {
            // For other positions, y is already the correct baseline coordinate
            finalY = y;
        }
    } else {
        // Baseline positioning - y is where the baseline should be
        finalY = y;
    }
    
    display.setCursor(x, finalY);
}

// Default cursor setting (auto mode)
void oled::setCursor(int16_t x, int16_t y) {
    setCursor(x, y, POS_AUTO);
}

// SIMPLIFIED DISPLAY FUNCTIONS
// ============================

// Main display function with all options
void oled::clearPrintShow(const char* text, int textSize, bool clear, bool showOled, bool center, int x_pos, int y_pos, int waitToFinish) {
    if (!oledConnected || !text) return;

    if (clear) {
        charPos = 0;
        display.clearDisplay();
    }
    
    // Auto-detect and switch framebuffer mode based on text size
    autoDetectMode(textSize);
    
    // Use smart font selection instead of text scaling
    // Default to Eurostile family, but could be made configurable
    setFontForSize(currentFontFamily, textSize);
    
    // Always use text size 1 since we're using native font sizes
    setTextSize(1);
    this->currentTextSize = 1;  // Track that we're using native fonts
    
    // Check if text fits, fallback to smaller font if needed
    int wrap = 0;
    while (!textFits(text) && textSize > 1) {
        textSize--;
        setFontForSize(currentFontFamily, textSize);
        if (textSize == 1) {
            wrap = 1;
            break;
        }
    }
    // Serial.print("wrap: ");
    // Serial.println(wrap);
    
    // Handle multi-line text
    if (strchr(text, '\n') != nullptr) {
        displayMultiLineText(text, center);
    } else {
        // Single line text
        int16_t x, y;
        
        if (center || x_pos == -1 || y_pos == -1) {
            getCenteredPosition(text, &x, &y);
        } else {
            x = x_pos;
            y = y_pos;
        }
        
        setCursor(x, y);
        display.print(text);
        charPos += strlen(text);
    }
    
    if (showOled) {
        show(waitToFinish);
        //display.display();
        //dumpFrameBufferQuarterSize(1);
    }
}

// Main display function with font family selection
    void oled::clearPrintShow(const char* text, int textSize, FontFamily family, bool clear, bool showOled, bool center, int x_pos, int y_pos, int waitToFinish) {
        if (!oledConnected || !text) return;

    if (clear) {
        charPos = 0;
        display.clearDisplay();
    }
    
    // Use smart font selection with specified family
    setFontForSize(family, textSize);
    
    // Always use text size 1 since we're using native font sizes
    setTextSize(1);
    this->currentTextSize = 1;  // Track that we're using native fonts
    
    int wrap = 0;
    // Check if text fits, fallback to smaller font if needed
    while (!textFits(text) && textSize > 1) {
        textSize--;
        setFontForSize(family, textSize);
        if (textSize == 1) {
            wrap = 1;
            break;
        }
    }
            
    // Handle multi-line text
    if (strchr(text, '\n') != nullptr) {
        displayMultiLineText(text, center);
    } else {
        // Single line text
        int16_t x, y;
        
        if (center || x_pos == -1 || y_pos == -1) {
            getCenteredPosition(text, &x, &y);
        } else {
            x = x_pos;
            y = y_pos;
        }
        
        setCursor(x, y);
        display.print(text);
        charPos += strlen(text);
    }
    
    if (showOled) { 
        show(waitToFinish);
        //display.display();
        //dumpFrameBufferQuarterSize(1);
    }
}

// Helper for multi-line text display
void oled::displayMultiLineText(const char* text, bool center) {
    std::vector<String> lines;
    String textStr = String(text);
    int start = 0;
    int pos = 0;
    
    // Split by newlines
    while (pos < textStr.length()) {
        if (textStr[pos] == '\n' || pos == textStr.length() - 1) {
            int end = (textStr[pos] == '\n') ? pos : pos + 1;
            String line = textStr.substring(start, end);
            if (line.length() > 0) {
                lines.push_back(line);
            }
            start = pos + 1;
        }
        pos++;
    }
    
    // Calculate positioning - fit all lines with bottom baseline at screen bottom
    FontMetrics metrics = getFontMetrics();
    int16_t lineSpacing = metrics.lineHeight;
    
    // Calculate actual ascent and descent for each line and find maximum
    int16_t maxActualAscent = 0;
    int16_t maxActualDescent = 0;
    for (int i = 0; i < lines.size(); i++) {
        TextBounds lineBounds = getTextBounds(lines[i].c_str());
        if (lineBounds.ascent > maxActualAscent) {
            maxActualAscent = lineBounds.ascent;
        }
        if (lineBounds.descent > maxActualDescent) {
            maxActualDescent = lineBounds.descent;
        }
    }
    
    // Use actual measurements if reasonable, otherwise fall back to font metrics
    int16_t ascentToUse = (maxActualAscent > 0) ? maxActualAscent : metrics.ascent;
    int16_t descentToUse = (maxActualDescent > 0) ? maxActualDescent : metrics.descent;
    
    // Calculate total ascent span needed
    int16_t totalAscentSpan = (lines.size() - 1) * lineSpacing + ascentToUse;
    
    // If it doesn't fit, compress line spacing to fit all ascent areas
    if (totalAscentSpan > displayHeight) {
        // Compress spacing: available space minus ascent, divided by number of gaps
        if (lines.size() > 1) {
            lineSpacing = (displayHeight - ascentToUse) / (lines.size() - 1) - 3;
            // Ensure minimum spacing of at least ascent height
            if (lineSpacing < ascentToUse) {
                lineSpacing = ascentToUse-3;
            }
        }
    }
    
    // Position last line so its ascent ends exactly at screen bottom
    int16_t lastLineBaseline = displayHeight- 2 ;
    int16_t firstLineBaseline = lastLineBaseline - (lines.size() - 1) * lineSpacing;
    
    // Apply font-specific topRowOffset for multi-line text positioning
    // This ensures fonts with different vertical metrics render at the correct position
    for (int i = 0; i < numFonts; i++) {
        if (fontList[i].font == currentFont && fontList[i].topRowOffset != 0) {
            if (displayHeight >= 24) { // Multi-line capable display
                firstLineBaseline += fontList[i].topRowOffset; // Can be positive or negative
            }
            break;
        }
    }
    
    // Display each line
    for (int i = 0; i < lines.size(); i++) {
        int16_t lineX = 0;
        int16_t lineY = firstLineBaseline + (i * lineSpacing);
        
        if (center) {
            // Get horizontal centering only
            int16_t tempY;
            getCenteredPosition(lines[i].c_str(), &lineX, &tempY);
            // Keep our calculated Y position for proper line spacing
        }
        
        setCursor(lineX, lineY, POS_BASELINE); // Use baseline for multi-line
        display.print(lines[i]);
        charPos += lines[i].length();
    }
}

// Simplified overloads
void oled::clearPrintShow(const char* text, int textSize, int waitToFinish) {
    clearPrintShow(text, textSize, true, true, true, -1, -1, waitToFinish);
}

void oled::clearPrintShow(const String& text, int textSize, int waitToFinish) {
    clearPrintShow(text.c_str(), textSize, true, true, true, -1, -1, waitToFinish);
}

void oled::clearPrintShow(const String& text, int textSize, bool clear, bool showOled, bool center, int x_pos, int y_pos, int waitToFinish) {
    clearPrintShow(text.c_str(), textSize, clear, showOled, center, x_pos, y_pos, waitToFinish);
}

void oled::clearPrintShow(const String& text, int textSize, FontFamily family, bool clear, bool showOled, bool center, int x_pos, int y_pos, int waitToFinish) {
    clearPrintShow(text.c_str(), textSize, family, clear, showOled, center, x_pos, y_pos, waitToFinish);
}



// BASIC PRINT FUNCTIONS
// ====================

void oled::print(const char* s) {
    if (!oledConnected || !s) return;
    
    // Auto-adjust cursor if at top of screen - simplified
    int16_t currentY = display.getCursorY();
    if (currentFont && currentY <= 4) {
        setCursor(display.getCursorX(), 0, POS_TIGHT);
    }

    if (currentTextSize > 2) {
        currentTextSize = 2;
        setFontForSize(currentFontFamily, currentTextSize);
    }
    
    display.print(s);
    charPos += strlen(s);
}

void oled::print(const String& s) {
    print(s.c_str());
}

void oled::print(int i) {
    if (!oledConnected) return;
    display.print(i);
    charPos += String(i).length();
}

void oled::print(float f) {
    if (!oledConnected) return;
    display.print(f);
    charPos += String(f).length();
}

void oled::print(char c) {
    if (!oledConnected) return;
    display.print(c);
    charPos += 1;
}

// Println functions
void oled::println(const char* s) {
    print(s);
    moveToNextLine();
}

void oled::println(const String& s) {
    print(s);
    moveToNextLine();
}

void oled::println(int i) {
    print(i);
    moveToNextLine();
}

void oled::println(float f) {
    print(f);
    moveToNextLine();
}

// UTILITY FUNCTIONS
// ================

bool oled::clear(int waitToFinish) {
    if (!oledConnected) {
        charPos = 0;
        return false;
    }
    charPos = 0;
    display.clearDisplay();
    setCursor(0, 0); // Auto-positioning for clear
    //int waited = 0;
    // if (waitToFinish > 0) {
    //     while (Wire1.finishedAsync() == false) {
    //         delayMicroseconds(1);
    //         waited += 1;
    //         if (waited > waitToFinish) {
    //             break;
    //         }
    //     }
    // }
    return true;
}

bool oled::show(int waitToFinish) {
    
    if (jumperlessConfig.top_oled.show_in_terminal > 0) {
        dumpFrameBufferQuarterSize(1);
    }
    if (!oledConnected) return false;
    display.display();
    // int waited = 0;
    // if (waitToFinish > 0) {
    //     while (Wire1.finishedAsync() == false) {
    //         delayMicroseconds(1);
    //         waited += 1;
    //         if (waited > waitToFinish) {
    //             Serial.println("OLED show timed out");
    //             break;
    //         }
    //     }
    // }
    return true;
}

void oled::moveToNextLine() {
    if (!oledConnected) return;
    
    FontMetrics metrics = getFontMetrics();
    int16_t currentY = display.getCursorY();
    int16_t nextY = currentY + metrics.lineHeight;
    
    if (nextY >= displayHeight) {
        nextY = metrics.ascent; // Wrap to top
    }
    
    setCursor(0, nextY, POS_BASELINE);
}

// Display settings
void oled::setTextSize(uint8_t size) {
    if (!oledConnected) return;
    if (size > 2) {
        size = 2;
    }
    this->currentTextSize = size;
    display.setTextSize(size);
}

void oled::setTextColor(uint32_t color) {
    if (!oledConnected) return;
    display.setTextColor(color);
}

void oled::invertDisplay(bool inv) {
    if (!oledConnected) return;
    display.invertDisplay(inv);
}

// Small text functions for file browser and detailed display
void oled::printSmallText(const char* text, int16_t x, int16_t y, bool clear) {
  if (!oledConnected) return;
  
  if (clear) {
    display.clearDisplay();
  }
  
  // Store current font before changing
  const GFXfont* savedFont = currentFont;
  FontFamily savedFamily = currentFontFamily;
  
  // Use default small font
  setSmallFont(DEFAULT_SMALL_FONT);
  setCursor(x, y + 8); // Adjust Y for 4-5pt font baseline
  display.print(text);
  
  // Restore previous font
  currentFont = savedFont;
  currentFontFamily = savedFamily;
  display.setFont(currentFont);
  usingSmallFont = false;
  
  if (clear) {
    display.display();
  }
}

void oled::printSmallTextLine(const char* text, int line, bool clear) {
  if (!oledConnected) return;
  
  if (clear) {
    display.clearDisplay();
  }
  
  // Store current font before changing
  const GFXfont* savedFont = currentFont;
  FontFamily savedFamily = currentFontFamily;
  
  // Use default small font
  setSmallFont(DEFAULT_SMALL_FONT);
  setCursor(0, (line * 8) + 8); // Adjust Y for 4-5pt font baseline
  display.print(text);
  
  // Restore previous font
  currentFont = savedFont;
  currentFontFamily = savedFamily;
  display.setFont(currentFont);
  usingSmallFont = false;
  
  if (clear) {
    display.display();
  }
}

void oled::clearLine(int line) {
  if (!oledConnected) return;
  
  // Clear a specific line by drawing a black rectangle
  display.fillRect(0, line * 8, displayWidth, 8, SSD1306_BLACK);
}

void oled::showFileStatus(const char* currentPath, int fileCount, const char* selectedFile) {
  if (!oledConnected) return;
  
  display.clearDisplay();
  
  // Store current font before changing
  const GFXfont* savedFont = currentFont;
  FontFamily savedFamily = currentFontFamily;
  
  // Use default small font for file manager display
  setSmallFont(DEFAULT_SMALL_FONT);
  
  // Path display without truncation
  display.setTextWrap(false);
  setCursor(0, 8);
  String path = String(currentPath);
  display.print(path.c_str());
  display.print("/");
    
  // Selected file (if provided) with cursor indicator
  if (selectedFile && strlen(selectedFile) > 0) {
    String selected = String(selectedFile);
    display.print(selected.c_str());
    
    // Add cursor indicator - draw underline under the selected file
    TextBounds bounds = getTextBounds(selected.c_str());
    int16_t pathWidth = getTextBounds((path + "/").c_str()).width;
    
    // Draw underline to show cursor position
    display.drawLine(pathWidth, 8 + 2, pathWidth + bounds.width - 1, 8 + 2, SSD1306_WHITE);
  }
  
  display.setTextWrap(true);
  display.display();
  
  // Restore previous font
  currentFont = savedFont;
  currentFontFamily = savedFamily;
  display.setFont(currentFont);
  usingSmallFont = false;
}

void oled::showFileStatusScrolled(const char* visibleText, int fileCount, int cursorPosition) {
  if (!oledConnected) return;
  
  // Clear display
  clearFramebuffer();
  
  // Store current font before changing
  const GFXfont* savedFont = currentFont;
  FontFamily savedFamily = currentFontFamily;
  
  // Use default small font for file manager display
  setSmallFont(DEFAULT_SMALL_FONT);
  display.setTextWrap(false);
  
  String text = String(visibleText);
  int newlinePos = text.indexOf('\n');
  
  if (newlinePos != -1) {
    // Multi-line display: path on first line, filename on second line
    String pathLine = text.substring(0, newlinePos);
    String filenameLine = text.substring(newlinePos + 1);
    
    // Draw path on first line
    drawText(0, 8, pathLine.c_str());
    
    // Draw filename on second line
    drawText(0, 20, filenameLine.c_str());
    
    // Highlight cursor character
    if (cursorPosition >= 0 && cursorPosition < text.length()) {
      int charWidth = getCharacterWidth();
      
      if (cursorPosition <= newlinePos) {
        // Cursor is on the path line
        char cursorChar = (cursorPosition < pathLine.length()) ? pathLine[cursorPosition] : ' ';
        int cursorX = cursorPosition * charWidth;
        int cursorY = 8;
        drawHighlightedChar(cursorX, cursorY, cursorChar);
      } else {
        // Cursor is on the filename line
        int filenameCursorPos = cursorPosition - newlinePos - 1;
        char cursorChar = (filenameCursorPos < filenameLine.length()) ? filenameLine[filenameCursorPos] : ' ';
        int cursorX = filenameCursorPos * charWidth;
        int cursorY = 20;
        drawHighlightedChar(cursorX, cursorY, cursorChar);
      }
    }
  } else {
    // Single line display (root directory case)
    drawText(0, 8, visibleText);
    
    // Highlight cursor character
    if (cursorPosition >= 0 && cursorPosition < strlen(visibleText)) {
      char cursorChar = visibleText[cursorPosition];
      if (cursorChar == '\0') {
        cursorChar = ' '; // Show space for end of text
      }
      
      int charWidth = getCharacterWidth();
      int cursorX = cursorPosition * charWidth;
      int cursorY = 8;
      
      drawHighlightedChar(cursorX, cursorY, cursorChar);
    }
  }
  
  // Flush to display
  flushFramebuffer();
  
  // Restore previous font
  currentFont = savedFont;
  currentFontFamily = savedFamily;
  display.setFont(currentFont);
  usingSmallFont = false;
}

void oled::showMultiLineSmallText(const char* text, bool clear) {
  if (!oledConnected) return;
  
  if (clear) {
    display.clearDisplay();
  }
  
  // Store current font before changing
  const GFXfont* savedFont = currentFont;
  FontFamily savedFamily = currentFontFamily;
  
  // Use default small font for better text density
  setSmallFont(DEFAULT_SMALL_FONT);
  display.setTextWrap(false); // Disable text wrapping - let text fall off the edge
  
  // Simply print the text without wrapping
  setCursor(0, 8); // Start at top with proper baseline
  display.print(text);
  
  if (clear) {
    display.display();
  }
  
  // Restore previous font
  currentFont = savedFont;
  currentFontFamily = savedFamily;
  display.setFont(currentFont);
  usingSmallFont = false;
}

// Connection status
bool oled::isConnected() const {
    return oledConnected;
}

// Logo display
void oled::showJogo32h() {
    if (!oledConnected) return;
    display.clearDisplay();
    
    int x = (displayWidth - jumperlessConfig.top_oled.width) / 2 + 1;
    int y = (displayHeight - jumperlessConfig.top_oled.height) / 2 + 1;
    
    display.drawBitmap(x, y, jogo32h, jumperlessConfig.top_oled.width, jumperlessConfig.top_oled.height, SSD1306_WHITE);
    display.display();
}

void oled::oledPeriodic() {
    return;
    if (millis() - lastConnectionCheck > connectionCheckInterval && jumperlessConfig.top_oled.enabled == 1) {
        lastConnectionCheck = millis();
        if (globalState.hasConnection(jumperlessConfig.top_oled.sda_row, jumperlessConfig.top_oled.gpio_sda) == true) {
            if (globalState.hasConnection(jumperlessConfig.top_oled.scl_row, jumperlessConfig.top_oled.gpio_scl) == true) {
                if (checkConnection() == false) {
                    oledConnected = false;
                    if (connectionRetries < maxConnectionRetries) {
                        if (init() != 0) {
                            Serial.print("\r                                          \r");
                            Serial.flush();
                        }
                    }
                } else {
                    connectionRetries = 0;
                }
            }
        }
    }
}

// TEST AND DEBUG FUNCTIONS
// ========================

void oled::test() {
    const char* testText = "Test";
    
    // Test with different sizes
    for (int size = 1; size <= 3; size++) {
        clearPrintShow(testText, size, true, true, true, -1, -1);
        delay(1000);
    }
    
    showJogo32h();
    delay(2000);
}

// Test function to debug menu positioning issues
void oled::testMenuPositioning() {
    // Test the exact same calls the menu system uses
    const char* menuItems[] = {"$Rails$", "Apps", "Slots", "Show", "Output"};
    
    for (int i = 0; i < 5; i++) {
        // Test with the exact same parameters as the menu system
        clearPrintShow(menuItems[i], 2, true, true, true, -1, -1);
        delay(2000);
        
        // Also test with the submenu parameters for comparison
        clearPrintShow(menuItems[i], 2, true, true, true, 5, 8);
        delay(2000);
    }
    
    // Test specifically with "Apps" to see if there's something special about it
    clearPrintShow("Apps", 2, true, true, true, -1, -1);
    delay(2000);
    clearPrintShow("Test", 2, true, true, true, -1, -1);
    delay(2000);
}

void oled::testSmallFonts() {
    if (!oledConnected) return;
    
    // Test each small font
    const char* testTexts[] = {"Ubuntu 5pt", "DotGothic 4pt", "Jokerman 4pt"};
    SmallFont fonts[] = {SMALL_FONT_UBUNTU, SMALL_FONT_DOTGOTHIC, SMALL_FONT_JOKERMAN};
    
    for (int i = 0; i < 3; i++) {
        clear();
        
        // Test single line
        useSmallFontAndRestore(fonts[i], testTexts[i], 0, 0, false, false);
        
        // Test multiple lines
        String multiText = String(testTexts[i]) + "\nLine 2\nLine 3\nLine 4";
        showMultiLineSmallText(multiText.c_str(), false);
        
        show();
        delay(3000);
        
        restoreNormalFont(); // Make sure font is restored
    }
    
    // Test file status display
    clear();
    showFileStatus("/test/path", 15, "example_file.py");
    delay(3000);
    restoreNormalFont();
    
    // Return to logo
    showJogo32h();
}

// Debug frame buffer dump function (simplified)

unsigned long lastDumpTime = 0;
unsigned long clearInterval = 2000;

void oled::dumpFrameBufferQuarterSize(int clearFirst, int x_pos, int y_pos, int border, Stream* stream) {
    if (!oledConnected) {
       // Serial.println("OLED not connected");
        return;
    }
    
    uint8_t* buffer = display.getBuffer();
    if (!buffer) {
       // Serial.println("No framebuffer available");
        return;
    }

    if (dumpingToSerial == false) {
        dumpingToSerial = true;
    } else {
        return;
    }

    if (jumperlessConfig.serial_2.function == 4 || jumperlessConfig.serial_2.function == 6) {
      stream = &USBSer2;
      y_pos = 1;
      x_pos = 1;
      if (millis() - lastDumpTime > clearInterval) {
        stream->print("\033[2J\033[?25l");
        lastDumpTime = millis();
      }
    } else if (jumperlessConfig.serial_1.function == 4 || jumperlessConfig.serial_1.function == 6) {
      stream = &USBSer1;
      y_pos = 1;
      x_pos = 1;
      if (millis() - lastDumpTime > clearInterval) {
        stream->print("\033[2J\033[?25l");
        lastDumpTime = millis();
      }
    } else {
      stream = &Serial;
      saveCursorPosition(stream);
    stream->printf("\033[%d;%dH", y_pos-1, x_pos +1);
    stream->printf("\033[0K");
    stream->printf("\033[1B");
    stream->printf("\033[0K");


    }




    if (border == 1) {
        stream->println("╭────────────────────────────────────────────────────────────────╮");
    } else if (border == 2) {
        stream->println("▗▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▖");
    } else {
        stream->println("                                                                  ");
    }
    
    // Quarter block characters for different pixel combinations
    const char* quarterBlocks[] = {
        " ",  // 0000 - No pixels
        "▘",  // 0001 - Upper left only
        "▝",  // 0010 - Upper right only  
        "▀",  // 0011 - Upper half
        "▖",  // 0100 - Lower left only
        "▌",  // 0101 - Left half
        "▞",  // 0110 - Upper right + lower left
        "▛",  // 0111 - Upper half + lower left
        "▗",  // 1000 - Lower right only
        "▚",  // 1001 - Upper left + lower right
        "▐",  // 1010 - Right half
        "▜",  // 1011 - Upper half + lower right
        "▄",  // 1100 - Lower half
        "▙",  // 1101 - Upper left + lower half
        "▟",  // 1110 - Upper right + lower half
        "█"   // 1111 - Full block
    };
    
    // Process framebuffer in 2x2 blocks to create 64x16 output
    for (int blockRow = 0; blockRow < displayHeight / 2; blockRow++) {
        stream->printf("\033[%dC",x_pos);
        stream->printf("\033[0K");
        if (border == 1) {
            stream->print("│"); // Left border
        } else if (border == 2) {
            stream->print("▐"); // Left border
        } else {
            stream->print(" "); // Left border
        }
        
        for (int blockCol = 0; blockCol < displayWidth / 2; blockCol++) {
            uint8_t pixelMask = 0;
            
            // Check each pixel in the 2x2 block
            for (int dy = 0; dy < 2; dy++) {
                for (int dx = 0; dx < 2; dx++) {
                    int row = blockRow * 2 + dy;
                    int col = blockCol * 2 + dx;
                    
                    // Calculate buffer position for this pixel
                    int page = row / 8;
                    int bit = row % 8;
                    int bufferIndex = page * displayWidth + col;
                    
                    // Extract the pixel value
                    uint8_t pixelByte = buffer[bufferIndex];
                    bool pixelOn = (pixelByte >> bit) & 0x01;
                    
                    // Set bit in pixelMask
                    if (pixelOn) {
                        pixelMask |= (1 << (dy * 2 + dx));
                    }
                }
            }
            
            // Print the appropriate quarter block character
            stream->print(quarterBlocks[pixelMask]);
        }
        
        if (border == 1) {
            stream->println("│"); // Right border and newline
        } else if (border == 2) {
            stream->println("▌"); // Right border and newline
        } else {
            stream->println(" "); // Right border and newline
        }
    }
    stream->printf("\033[%dC",x_pos);
    stream->printf("\033[0K");
    if (border == 1) {
        stream->println("╰────────────────────────────────────────────────────────────────╯");
    } else if (border == 2) {
        stream->println("▝▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▘");
    } else {
        stream->println("                                                                  ");
    }
    stream->printf("\033[%dB",y_pos - (displayHeight / 2 ) + 2);
    if (stream == &Serial) {
      restoreCursorPosition(stream);
    } else {
      stream->printf("\033[%d;%dH", y_pos-1, x_pos +1);
    }
    dumpingToSerial = false;
}


void oled::dumpFrameBuffer() {
    if (!oledConnected) {
        Serial.println("OLED not connected");
        return;
    }
    
    uint8_t* buffer = display.getBuffer();
    if (!buffer) {
        Serial.println("No framebuffer available");
        return;
    }
    
    Serial.println("OLED Framebuffer Dump (128x32):");
    Serial.println("┌────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐");
    
    // The SSD1306 framebuffer is organized as:
    // - 128 columns (width)
    // - 32 rows / 8 = 4 pages (height in pages of 8 pixels each)
    // - Each byte represents 8 vertical pixels in a column
    // - Buffer layout: [col0_page0, col1_page0, ..., col127_page0, col0_page1, col1_page1, ...]
    
    for (int row = 0; row < displayHeight; row++) {
        Serial.print("│"); // Left border
        
        for (int col = 0; col < displayWidth; col++) {
            // Calculate which page (group of 8 rows) this pixel is in
            int page = row / 8;
            // Calculate which bit within the byte (0 = top of page, 7 = bottom of page)
            int bit = row % 8;
            // Calculate buffer index: page * width + column
            int bufferIndex = page * displayWidth + col;
            
            // Extract the specific bit for this pixel
            uint8_t pixelByte = buffer[bufferIndex];
            bool pixelOn = (pixelByte >> bit) & 0x01;
            
            // Print block character based on pixel state
            if (pixelOn) {
                Serial.print("█"); // Full block for lit pixel
            } else {
                Serial.print(" "); // Space for dark pixel
            }
        }
        
        Serial.println("│"); // Right border and newline
    }
    
    Serial.println("└────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘");
    Serial.print("Buffer size: ");
    Serial.print(displayWidth * displayHeight / 8);
    Serial.println(" bytes");
}


// SMALL FONT FUNCTIONS
// ===================

void oled::setSmallFont(SmallFont smallFont) {
    if (!oledConnected) return;
    
    // Store current font and family for restoration
    if (!usingSmallFont) {
        previousFont = currentFont;
        previousFontFamily = currentFontFamily;
    }
    
    currentSmallFont = smallFont;
    usingSmallFont = true;
    
    // Map small font enum to font index
    int fontIndex;
    switch (smallFont) {
        case SMALL_FONT_UBUNTU:
            fontIndex = 20; // ubuntu5pt7b
            break;
        case SMALL_FONT_DOTGOTHIC:
            fontIndex = 21; // DotGothic16_Regular4pt7b
            break;
        case SMALL_FONT_JOKERMAN:
            fontIndex = 2; // Jokerman8pt7b
            break;
        case SMALL_FONT_ANDALE_MONO:
            fontIndex = 12; // ANDALEMO5pt7b - monospaced for text highlighting
            break;
        case SMALL_FONT_IOSEVKA_REGULAR:
            fontIndex = 18; // IosevkaSS08_Regular9pt
            break;
        case SMALL_FONT_IOSEVKA_5PT:
            fontIndex = 22; // IosevkaSS08_Light5pt
            break;
        case SMALL_FONT_PRAGMATISM_5PT:
            fontIndex = 23; // Pragmatism5pt
            break;
        case SMALL_FONT_FREEMONO_5PT:
            fontIndex = 24; // FreeMono5pt
            break;
        case SMALL_FONT_ENVYCODE_5PT:
            fontIndex = 25; // EnvyCodeRNerdFont_Regular5pt
            break;
        default:
            fontIndex = 12; // Default to Andale Mono
            break;
    }
    
    setFont(fontIndex);
    setTextSize(1); // Always use size 1 with small fonts
}

void oled::useSmallFont(SmallFont smallFont, const char* text, int16_t x, int16_t y, bool clear) {
    if (!oledConnected || !text) return;
    
    if (clear) {
        display.clearDisplay();
    }
    
    setSmallFont(smallFont);
    setCursor(x, y);
    display.print(text);
}

void oled::useSmallFontAndRestore(SmallFont smallFont, const char* text, int16_t x, int16_t y, bool clear, bool show) {
    if (!oledConnected || !text) return;
    
    if (clear) {
        display.clearDisplay();
    }
    
    setSmallFont(smallFont);
    
    // Auto-adjust Y position for small fonts (add baseline offset if y is too small)
    int16_t adjustedY = y;
    if (y < 8) {
        adjustedY = y + 8; // Add baseline offset for 4-5pt fonts
    }
    
    setCursor(x, adjustedY);
    display.print(text);
    
    if (show) {
        display.display();
    }
    
    restoreNormalFont();
}

void oled::restoreNormalFont() {
    if (!oledConnected) return;
    
    if (usingSmallFont && previousFont) {
        currentFont = previousFont;
        currentFontFamily = previousFontFamily;
        display.setFont(currentFont);
        usingSmallFont = false;
        previousFont = nullptr;
    }
}

// Drawing primitives
void oled::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    if (!oledConnected) return;
    display.drawLine(x0, y0, x1, y1, color);
}

void oled::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (!oledConnected) return;
    display.fillRect(x, y, w, h, color);
}

// Simple framebuffer management
void oled::clearFramebuffer() {
    if (!oledConnected) return;
    display.clearDisplay();
}

void oled::setPixel(int16_t x, int16_t y, uint16_t color) {
    if (!oledConnected) return;
    display.drawPixel(x, y, color);
}

void oled::drawChar(int16_t x, int16_t y, char c) {
    if (!oledConnected) return;
    int16_t savedX = display.getCursorX();
    int16_t savedY = display.getCursorY();
    display.setCursor(x, y);
    display.print(c);
    display.setCursor(savedX, savedY);
}

void oled::drawText(int16_t x, int16_t y, const char* text) {
    if (!oledConnected) return;
    int16_t savedX = display.getCursorX();
    int16_t savedY = display.getCursorY();
    display.setCursor(x, y);
    display.print(text);
    display.setCursor(savedX, savedY);
}

void oled::drawHighlightedChar(int16_t x, int16_t y, char c) {
    if (!oledConnected) return;
    
    // Get character bounds for background rectangle
    int16_t x1, y1;
    uint16_t w, h;
    char charStr[2] = {c, '\0'};
    display.getTextBounds(charStr, x, y, &x1, &y1, &w, &h);
    
    // Draw larger white background for border effect (1 pixel larger on all sides)
    display.fillRect(x1 - 1, y1 - 1, w + 2, h + 2, SSD1306_WHITE);
    
    // Draw character with black text on white background (inverted)
    int16_t savedX = display.getCursorX();
    int16_t savedY = display.getCursorY();
    display.setCursor(x, y);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // Black text on white background
    display.print(c);
    
    // Restore default colors and cursor
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK); // Default colors
    display.setCursor(savedX, savedY);
}

void oled::flushFramebuffer() {
    if (!oledConnected) return;
    display.display();
}

// ============================================================================
// DUAL FRAMEBUFFER MODE MANAGEMENT
// ============================================================================

void oled::setMode(OLEDMode mode) {
    if (!dualFramebufferEnabled) {
        // Dual framebuffer not enabled, just track mode
        currentMode = mode;
        return;
    }
    
    if (mode == currentMode) {
        return; // Already in this mode
    }
    
    // Save current framebuffer contents before switching
    uint8_t* currentBuffer = display.getBuffer();
    if (currentBuffer) {
        if (currentMode == MODE_LARGE_TEXT && largeTextFramebuffer) {
            memcpy(largeTextFramebuffer, currentBuffer, framebufferSize);
        } else if (currentMode == MODE_SMALL_TEXT && smallTextFramebuffer) {
            memcpy(smallTextFramebuffer, currentBuffer, framebufferSize);
        }
    }
    
    // Switch to new mode
    currentMode = mode;
    
    // Load new framebuffer contents
    if (currentBuffer) {
        if (currentMode == MODE_LARGE_TEXT && largeTextFramebuffer) {
            memcpy(currentBuffer, largeTextFramebuffer, framebufferSize);
        } else if (currentMode == MODE_SMALL_TEXT && smallTextFramebuffer) {
            memcpy(currentBuffer, smallTextFramebuffer, framebufferSize);
        }
    }
}

void oled::autoDetectMode(int textSize) {
    if (!dualFramebufferEnabled) {
        return; // Dual framebuffer not enabled
    }
    
    // Automatic mode detection based on text size
    if (textSize >= 2) {
        setMode(MODE_LARGE_TEXT);
    } else {
        setMode(MODE_SMALL_TEXT);
    }
}

// ============================================================================
// TERMINAL EMULATION - ANSI/CONTROL SEQUENCE HANDLING
// ============================================================================

void oled::parseControlSequence(char c) {
    if (!inEscapeSequence) {
        if (c == '\x1b') { // ESC character
            inEscapeSequence = true;
            escapeBufferPos = 0;
            memset(escapeBuffer, 0, sizeof(escapeBuffer));
            return;
        }
        
        // Handle regular control characters
        switch (c) {
            case '\r': // Carriage return
                termCursorX = 0;
                break;
            case '\n': // Line feed
                termCursorY++;
                if (termCursorY >= displayHeight / 8) {
                    termCursorY = displayHeight / 8 - 1;
                }
                break;
            case '\t': // Tab
                termCursorX = ((termCursorX / 8) + 1) * 8;
                break;
            case '\b': // Backspace
                if (termCursorX > 0) termCursorX--;
                break;
            default:
                // Printable character - handle normally
                break;
        }
        return;
    }
    
    // Build escape sequence
    if (escapeBufferPos < sizeof(escapeBuffer) - 1) {
        escapeBuffer[escapeBufferPos++] = c;
        escapeBuffer[escapeBufferPos] = '\0';
    }
    
    // Check if sequence is complete
    if (c >= 'A' && c <= 'Z') { // Final character
        executeEscapeSequence();
        inEscapeSequence = false;
        escapeBufferPos = 0;
    } else if (c >= 'a' && c <= 'z') { // Some sequences end with lowercase
        executeEscapeSequence();
        inEscapeSequence = false;
        escapeBufferPos = 0;
    }
}

void oled::executeEscapeSequence() {
    if (escapeBufferPos == 0) return;
    
    // Parse CSI sequences (starting with '[')
    if (escapeBuffer[0] == '[') {
        char finalChar = escapeBuffer[escapeBufferPos - 1];
        
        switch (finalChar) {
            case 'H': case 'f': { // Cursor position
                // Format: ESC[row;colH or ESC[H (home)
                int row = 0, col = 0;
                if (escapeBufferPos > 1) {
                    sscanf(&escapeBuffer[1], "%d;%d", &row, &col);
                }
                moveCursorTerm(col, row);
                break;
            }
            
            case 'A': { // Cursor up
                int n = 1;
                if (escapeBufferPos > 1) {
                    sscanf(&escapeBuffer[1], "%d", &n);
                }
                termCursorY -= n;
                if (termCursorY < 0) termCursorY = 0;
                break;
            }
            
            case 'B': { // Cursor down
                int n = 1;
                if (escapeBufferPos > 1) {
                    sscanf(&escapeBuffer[1], "%d", &n);
                }
                termCursorY += n;
                if (termCursorY >= displayHeight / 8) termCursorY = displayHeight / 8 - 1;
                break;
            }
            
            case 'C': { // Cursor forward
                int n = 1;
                if (escapeBufferPos > 1) {
                    sscanf(&escapeBuffer[1], "%d", &n);
                }
                termCursorX += n;
                if (termCursorX >= displayWidth / 6) termCursorX = displayWidth / 6 - 1;
                break;
            }
            
            case 'D': { // Cursor back
                int n = 1;
                if (escapeBufferPos > 1) {
                    sscanf(&escapeBuffer[1], "%d", &n);
                }
                termCursorX -= n;
                if (termCursorX < 0) termCursorX = 0;
                break;
            }
            
            case 'J': { // Erase in display
                int n = 0;
                if (escapeBufferPos > 1) {
                    sscanf(&escapeBuffer[1], "%d", &n);
                }
                if (n == 0) clearToEndOfScreen();
                else if (n == 2) clearScreen();
                break;
            }
            
            case 'K': { // Erase in line
                int n = 0;
                if (escapeBufferPos > 1) {
                    sscanf(&escapeBuffer[1], "%d", &n);
                }
                if (n == 0) clearToEndOfLine();
                else if (n == 2) clearLine();
                break;
            }
            
            case 'm': // SGR (color/style) - ignore for monochrome display
                break;
                
            default:
                // Unknown sequence - ignore
                break;
        }
    }
}

void oled::moveCursorTerm(int x, int y) {
    termCursorX = x;
    termCursorY = y;
    
    // Clamp to display bounds
    if (termCursorX < 0) termCursorX = 0;
    if (termCursorY < 0) termCursorY = 0;
    if (termCursorX >= displayWidth / 6) termCursorX = displayWidth / 6 - 1;
    if (termCursorY >= displayHeight / 8) termCursorY = displayHeight / 8 - 1;
}

void oled::clearToEndOfLine() {
    if (!oledConnected) return;
    int x = termCursorX * 6;
    int y = termCursorY * 8;
    display.fillRect(x, y, displayWidth - x, 8, SSD1306_BLACK);
}

void oled::clearLine() {
    if (!oledConnected) return;
    int y = termCursorY * 8;
    display.fillRect(0, y, displayWidth, 8, SSD1306_BLACK);
}

void oled::clearToEndOfScreen() {
    if (!oledConnected) return;
    clearToEndOfLine();
    
    // Clear all lines below current
    for (int line = termCursorY + 1; line < displayHeight / 8; line++) {
        display.fillRect(0, line * 8, displayWidth, 8, SSD1306_BLACK);
    }
}

void oled::clearScreen() {
    if (!oledConnected) return;
    display.clearDisplay();
    termCursorX = 0;
    termCursorY = 0;
}

// CONNECTION MANAGEMENT
// ====================

int oled::connect(void) {
    // if (jumperlessConfig.top_oled.enabled == 0) {
    //     return 0;
    // }
    int found = -1;
    
    // If using hardwired RP6/RP7 (GPIO 6/7), skip crossbar bridge management
    if (!oledUsingHardwiredPins) {
        // Reserve pins on net map so UI shows them as I2C (not generic GPIO)
        // gpioNet[jumperlessConfig.top_oled.sda_pin - 20] = -2;
        // gpioNet[jumperlessConfig.top_oled.scl_pin - 20] = -2;
        // removeBridgeFromNodeFile(jumperlessConfig.top_oled.gpio_sda, -1, netSlot, 0);
        // removeBridgeFromNodeFile(jumperlessConfig.top_oled.gpio_scl, -1, netSlot, 0);
        
        // Use RAM-based state system for crossbar connections
        addBridgeToState(jumperlessConfig.top_oled.gpio_sda, jumperlessConfig.top_oled.sda_row, 1);
        addBridgeToState(jumperlessConfig.top_oled.gpio_scl, jumperlessConfig.top_oled.scl_row, 1);
        
        // Extra refresh to ensure OLED connections are applied
        refreshConnections(1, 0, 0);
        // Serial.print("waitCore2     ");
        // Serial.println(millis());
        waitCore2();
    }

        
    // Serial.print("waitCore2Done  ");
    // Serial.println(millis());
    found = initI2C(jumperlessConfig.top_oled.sda_pin, jumperlessConfig.top_oled.scl_pin, 400000);
    
    // Mark function map so scan/UI reflect I2C role
    if (jumperlessConfig.top_oled.sda_pin >= 20) {
        gpio_function_map[jumperlessConfig.top_oled.sda_pin - 20] = GPIO_FUNC_I2C;
        gpioState[jumperlessConfig.top_oled.sda_pin - 20] = 6;
    }
    if (jumperlessConfig.top_oled.scl_pin >= 20) {
        gpio_function_map[jumperlessConfig.top_oled.scl_pin - 20] = GPIO_FUNC_I2C;
        gpioState[jumperlessConfig.top_oled.scl_pin - 20] = 6;
    }
    // Serial.print("initI2C      ");
    // Serial.println(millis());
    
    if (found == -1) {
        oledConnected = false;
        return 0;
    } else {
        oledConnected = true;
        return found;
    }
}

void oled::disconnect(void) {
    // if (jumperlessConfig.top_oled.enabled == 0) {
    //     return;
    // }
   // oledConnected = false;
    clear(1000);
    show(1000);
    
    // Only remove crossbar bridges if not using hardwired pins
    if (!oledUsingHardwiredPins) {
        // Use RAM-based state system
        removeBridgeFromState(jumperlessConfig.top_oled.gpio_sda, jumperlessConfig.top_oled.sda_row);
        removeBridgeFromState(jumperlessConfig.top_oled.gpio_scl, jumperlessConfig.top_oled.scl_row);
        // Restore pins to unassigned in net map so they show as normal when disconnected
        // gpioNet[jumperlessConfig.top_oled.sda_pin - 20] = -1;
        // gpioNet[jumperlessConfig.top_oled.scl_pin - 20] = -1;
        if (jumperlessConfig.top_oled.sda_pin >= 20) {
            gpioState[jumperlessConfig.top_oled.sda_pin - 20] = 4;
        }
        if (jumperlessConfig.top_oled.scl_pin >= 20) {
            gpioState[jumperlessConfig.top_oled.scl_pin - 20] = 4;
        }
        refreshConnections(-1, 0, 0);
    }
    
    oledConnected = false;
}

char scratchPad[40];

char* oled::getScratchPad(void) {
    return scratchPad;
}

// GLOBAL FUNCTIONS
// ===============

int initOLED(void) {
    return oled.init();
}

int oledTest(int sdaRow, int sclRow, int sdaPin, int sclPin, int leaveConnections) {
    oled.clear();
    
    int delayTime = 8000;
    resetEncoderPosition = true;
    long lastEncoderPosition = 0;
    
    // Set font and get metrics for proper positioning
    oled.setFont(0); // Use default font
    oled.setTextSize(1);
    FontMetrics metrics = oled.getFontMetrics();

    while (1) {
        if (encoderPosition != lastEncoderPosition) {
            lastEncoderPosition = encoderPosition;
            oled.clear();
            
            // Create encoder display string
            String encoderText = "Encoder: " + String((int)encoderPosition);
            
            // Use new positioning system for centered display
            int16_t x, y;
            oled.getCenteredPosition(encoderText.c_str(), &x, &y);
            
            // Set cursor with proper baseline positioning
            TextBounds bounds = oled.getTextBounds(encoderText.c_str());
            oled.setCursor(x, y + bounds.ascent);
            oled.print(encoderText);
            
            oled.show();
        }
        
        if (encoderButtonState == RELEASED && lastButtonEncoderState == PRESSED) {
            encoderButtonState = IDLE;
            break;
        }
        
        if (delayTime < 1) {
            delayTime = 1;
        }
    }
    
    oled.show();
    return 0;
}

void testOLEDSmallFonts(void) {
    oled.testSmallFonts();
}

// BITMAP DATA
// ===========

// 'Jogo255', 128x32px
const unsigned char jogo32h [] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x77, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xfb, 0xff, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x8b, 0xfe, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0xfd, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x03, 0xfb, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x03, 0xf7, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x81, 0xee, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x81, 0xde, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x60, 0xbe, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xf0, 0x0e, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xf0, 0x03, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xfc, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xfb, 0x80, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xf7, 0xc3, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xef, 0xe0, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xdf, 0xf8, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xbf, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xc3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

