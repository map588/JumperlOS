#include "Graphics.h"
#include "Adafruit_NeoPixel.h"
#include "Commands.h"
#include "Highlighting.h"
#include "JumperlessDefines.h"
#include "LEDs.h"
#include "MatrixState.h"
#include "States.h"
#include "Menus.h"
#include "NetManager.h"
#include "Peripherals.h"
#include "PersistentStuff.h"
#include "Probing.h"

#include <cstdarg>

#include "ArduinoStuff.h"
#include "Images.h"
#include "Tui.h"
#include "TuiGlue.h"

#include "Jerial.h"

#ifdef DONOTUSE_SERIALWRAPPER
#include "SerialWrapper.h"
#define Serial SerialWrap
#endif

bool disableTerminalColors = false;

// External function declarations (to avoid including headers)
extern void jl_pause_core2(bool pause);
extern void changeTerminalColor(int termColor, bool flush, Stream *stream);
extern void cycleTermColor(bool reset,  float step, bool flush);

/* clang-format off */

// Non-blocking flush function with timeout
bool safeFlush(Stream *stream, unsigned long timeoutMs = 50) {
  if (!stream) return false;
  
  unsigned long startTime = millis();
  size_t initialAvailable = Jerial.availableForWrite();
  
  // If buffer is nearly empty, try a quick flush
  if (initialAvailable > Jerial.availableForWrite() * 0.8) {
    Jerial.flush();
    return true;
  }
  
  // Wait for buffer to drain with timeout
  while (millis() - startTime < timeoutMs) {
    size_t currentAvailable = Jerial.availableForWrite();
    
    // If buffer has drained significantly, it's safe to flush
    if (currentAvailable > initialAvailable * 0.5) {
      Jerial.flush();
      return true;
    }
    
    // Small delay to prevent busy waiting
    delayMicroseconds(50);  // Reduced from 100
  }
  
  // Timeout reached - don't flush
  return false;
}

// Safe print function that checks buffer space
bool safePrint(Stream *stream, const char *text, unsigned long timeoutMs = 50) {
  if (!stream || !text) return false;
  
  size_t textLen = strlen(text);
  unsigned long startTime = millis();
  
  // Wait for enough buffer space
  while (Jerial.availableForWrite() < textLen && (millis() - startTime) < timeoutMs) {
    delayMicroseconds(100);
  }
  
  if (Jerial.availableForWrite() >= textLen) {
    Jerial.print(text);
    return true;
  }
  
  return false; // Timeout or insufficient buffer
}

// Safe printf function
bool safePrintf(Stream *stream, unsigned long timeoutMs, const char *format, ...) {
  if (!stream || !format) return false;
  
  char buffer[256]; // Adjust size as needed
  va_list args;
  va_start(args, format);
  int len = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  
  if (len > 0 && len < sizeof(buffer)) {
    return safePrint(stream, buffer, timeoutMs);
  }
  
  return false;
}

const int screenMap[445] =
  { 300, 300, 301, 302, 303, 304, 305, 305, 306, 307, 308, 309, 310, 310, 311, 312, 313, 314, 315, 315, 316, 317, 318, 319, 320, 320, 321, 322, 323, 324,
    325, 325, 326, 327, 328, 329, 330, 330, 331, 332, 333, 334, 335, 335, 336, 337, 338, 339, 340, 340, 341, 342, 343, 344, 345, 345, 346, 347, 348, 349,
      0,   5,  10,  15,  20,  25,  30,  35,  40,  45,  50,  55,  60,  65,  70,  75,  80,  85,  90,  95, 100, 105, 110, 115, 120, 125, 130, 135, 140, 145,
      1,   6,  11,  16,  21,  26,  31,  36,  41,  46,  51,  56,  61,  66,  71,  76,  81,  86,  91,  96, 101, 106, 111, 116, 121, 126, 131, 136, 141, 146,
      2,   7,  12,  17,  22,  27,  32,  37,  42,  47,  52,  57,  62,  67,  72,  77,  82,  87,  92,  97, 102, 107, 112, 117, 122, 127, 132, 137, 142, 147,
      3,   8,  13,  18,  23,  28,  33,  38,  43,  48,  53,  58,  63,  68,  73,  78,  83,  88,  93,  98, 103, 108, 113, 118, 123, 128, 133, 138, 143, 148,
      4,   9,  14,  19,  24,  29,  34,  39,  44,  49,  54,  59,  64,  69,  74,  79,  84,  89,  94,  99, 104, 109, 114, 119, 124, 129, 134, 139, 144, 149,

    150, 155, 160, 165, 170, 175, 180, 185, 190, 195, 200, 205, 210, 215, 220, 225, 230, 235, 240, 245, 250, 255, 260, 265, 270, 275, 280, 285, 290, 295,
    151, 156, 161, 166, 171, 176, 181, 186, 191, 196, 201, 206, 211, 216, 221, 226, 231, 236, 241, 246, 251, 256, 261, 266, 271, 276, 281, 286, 291, 296,
    152, 157, 162, 167, 172, 177, 182, 187, 192, 197, 202, 207, 212, 217, 222, 227, 232, 237, 242, 247, 252, 257, 262, 267, 272, 277, 282, 287, 292, 297,
    153, 158, 163, 168, 173, 178, 183, 188, 193, 198, 203, 208, 213, 218, 223, 228, 233, 238, 243, 248, 253, 258, 263, 268, 273, 278, 283, 288, 293, 298,
    154, 159, 164, 169, 174, 179, 184, 189, 194, 199, 204, 209, 214, 219, 224, 229, 234, 239, 244, 249, 254, 259, 264, 269, 274, 279, 284, 289, 294, 299,


    350, 350, 351, 352, 353, 354, 355, 355, 356, 357, 358, 359, 360, 360, 361, 362, 363, 364, 365, 365, 366, 367, 368, 369, 370, 370, 371, 372, 373, 374,
    375, 375, 376, 377, 378, 379, 380, 380, 381, 382, 383, 384, 385, 385, 386, 387, 388, 389, 390, 390, 391, 392, 393, 394, 395, 395, 396, 397, 398, 399,

  };

const int screenMapNoRails[445] =
  { 0,   5,  10,  15,  20,  25,  30,  35,  40,  45,  50,  55,  60,  65,  70,  75,  80,  85,  90,  95, 100, 105, 110, 115, 120, 125, 130, 135, 140, 145,
      1,   6,  11,  16,  21,  26,  31,  36,  41,  46,  51,  56,  61,  66,  71,  76,  81,  86,  91,  96, 101, 106, 111, 116, 121, 126, 131, 136, 141, 146,
      2,   7,  12,  17,  22,  27,  32,  37,  42,  47,  52,  57,  62,  67,  72,  77,  82,  87,  92,  97, 102, 107, 112, 117, 122, 127, 132, 137, 142, 147,
      3,   8,  13,  18,  23,  28,  33,  38,  43,  48,  53,  58,  63,  68,  73,  78,  83,  88,  93,  98, 103, 108, 113, 118, 123, 128, 133, 138, 143, 148,
      4,   9,  14,  19,  24,  29,  34,  39,  44,  49,  54,  59,  64,  69,  74,  79,  84,  89,  94,  99, 104, 109, 114, 119, 124, 129, 134, 139, 144, 149,

    150, 155, 160, 165, 170, 175, 180, 185, 190, 195, 200, 205, 210, 215, 220, 225, 230, 235, 240, 245, 250, 255, 260, 265, 270, 275, 280, 285, 290, 295,
    151, 156, 161, 166, 171, 176, 181, 186, 191, 196, 201, 206, 211, 216, 221, 226, 231, 236, 241, 246, 251, 256, 261, 266, 271, 276, 281, 286, 291, 296,
    152, 157, 162, 167, 172, 177, 182, 187, 192, 197, 202, 207, 212, 217, 222, 227, 232, 237, 242, 247, 252, 257, 262, 267, 272, 277, 282, 287, 292, 297,
    153, 158, 163, 168, 173, 178, 183, 188, 193, 198, 203, 208, 213, 218, 223, 228, 233, 238, 243, 248, 253, 258, 263, 268, 273, 278, 283, 288, 293, 298,
    154, 159, 164, 169, 174, 179, 184, 189, 194, 199, 204, 209, 214, 219, 224, 229, 234, 239, 244, 249, 254, 259, 264, 269, 274, 279, 284, 289, 294, 299,


  };


const uint8_t upperCase[30][3] = { {
0x1e, 0x05, 0x1e, },{ 0x1f, 0x15, 0x0a, },{
0x1f, 0x11, 0x11, },{ 0x1f, 0x11, 0x0e, },{ 0x1f, 0x15, 0x11, },{ 0x1f, 0x05, 0x01, },{
0x0e, 0x15, 0x1d, },{ 0x1f, 0x04, 0x1f, },{ 0x11, 0x1f, 0x11, },{ 0x08, 0x10, 0x0f, },{
0x1f, 0x04, 0x1b, },{ 0x1f, 0x10, 0x10, },{ 0x1f, 0x07, 0x1f, },{ 0x1f, 0x01, 0x1f, },{
0x1f, 0x11, 0x1f, },{ 0x1f, 0x05, 0x07, },{ 0x0f, 0x09, 0x17, },{ 0x1f, 0x0d, 0x17, },{
0x17, 0x15, 0x1d, },{ 0x01, 0x1f, 0x01, },{ 0x1f, 0x10, 0x1f, },{ 0x0f, 0x10, 0x0f, },{
0x1f, 0x0c, 0x1f, },{ 0x1b, 0x04, 0x1b, },{ 0x03, 0x1c, 0x03, },{ 0x19, 0x15, 0x13, },{ //Z

} };

const uint8_t lowerCase[30][3] = { {
    0x1c, 0x0a, 0x1c, },{ 0x1e, 0x14, 0x08, },{ 0x0c, 0x12, 0x12, },{ 0x08, 0x14, 0x1e, },{
0x0e, 0x16, 0x14, },{ 0x1c, 0x0a, 0x02, },{ 0x14, 0x16, 0x0e, },{ 0x1e, 0x04, 0x18, },{
0x00, 0x1d, 0x00, },{ 0x10, 0x0d, 0x00, },{ 0x1e, 0x08, 0x16, },{ 0x00, 0x1e, 0x10, },{
0x1e, 0x06, 0x1e, },{ 0x1e, 0x02, 0x1c, },{ 0x1e, 0x12, 0x1e, },{ 0x1e, 0x0a, 0x04, },{
0x04, 0x0a, 0x1c, },{ 0x1e, 0x02, 0x04, },{ 0x14, 0x1a, 0x0a, },{ 0x04, 0x1e, 0x14, },{
0x1e, 0x10, 0x1e, },{ 0x0e, 0x10, 0x0e, },{ 0x1e, 0x18, 0x1e, },{ 0x16, 0x08, 0x16, },{
0x06, 0x18, 0x06, },{ 0x12, 0x1a, 0x16, } }; //z

const uint8_t fontNumbers[10][3] = { {
0x1f, 0x11, 0x1f, },{ 0x12, 0x1f, 0x10, },{ 0x1d, 0x15, 0x17, },{ 0x11, 0x15, 0x1f, },{
0x07, 0x04, 0x1f, },{ 0x17, 0x15, 0x1d, },{ 0x1f, 0x15, 0x1d, },{ 0x19, 0x05, 0x03, },{
0x1f, 0x15, 0x1f, },{ 0x17, 0x15, 0x1f, } }; //9

const uint8_t symbols[50][3] = {
{ 0x00, 0x17, 0x00, }, //'!'
{ 0x16, 0x1f, 0x0d, }, //$
{ 0x19, 0x04, 0x13, }, //%
{ 0x02, 0x01, 0x02, }, //^
{ 0x02, 0x07, 0x02, }, //'*'
{ 0x10, 0x10, 0x10, }, //_
{ 0x04, 0x04, 0x04, }, //-
{ 0x04, 0x0e, 0x04, }, //+
{ 0x04, 0x15, 0x04, }, //÷
{ 0x0a, 0x04, 0x0a, }, //x
{ 0x0a, 0x0a, 0x0a, }, //=
{ 0x12, 0x17, 0x12, }, //±
{ 0x01, 0x1d, 0x07, }, //?
{ 0x04, 0x0a, 0x11, }, //<
{ 0x11, 0x0a, 0x04, }, //>
{ 0x06, 0x04, 0x0c, }, //~
{ 0x01, 0x02, 0x00, }, //'
{ 0x10, 0x08, 0x00, }, //,
{ 0x00, 0x10, 0x00, }, //.
{ 0x18, 0x04, 0x03, }, // '/'
{ 0x03, 0x04, 0x18, }, // '\'
{ 0x00, 0x0e, 0x11, }, // (
{ 0x11, 0x0e, 0x00, }, // )
{ 0x00, 0x1f, 0x11, }, // [
{ 0x00, 0x11, 0x1f, }, // ]
{ 0x04, 0x0e, 0x1b, }, // {
{ 0x1b, 0x0e, 0x04, }, // }
{ 0x00, 0x1f, 0x00, }, // |
{ 0x10, 0x0a, 0x00, }, // ;
{ 0x00, 0x0a, 0x00, }, // :
{ 0x1e, 0x08, 0x06, }, // µ
{ 0x07, 0x05, 0x07, }, // °
{ 0x04, 0x0e, 0x1f, }, // ❬ thicc <
{ 0x1f, 0x0e, 0x04, }, // ❭ thicc >
{ 0x03, 0x00, 0x03, }, // "
{ 0x00, 0x03, 0x00, }, // '
{ 0x0a, 0x0f, 0x08, }, // 𝟷
{ 0x0d, 0x0b, 0x00, }, // 𝟸
{ 0x09, 0x0b, 0x0f, } };// 𝟹

const uint8_t arrow[8][3] = {
{ 0x08, 0x1f, 0x08, }, // ↑ up arrow
{ 0x02, 0x1f, 0x02, }, // ↓ down arrow  
{ 0x04, 0xA, 0x11, }, // ← left arrow
{ 0x04, 0x0a, 0x11, }, // → right arrow
{ 0x1c, 0x18, 0x16, }, // ↖ up-left arrow
{ 0x16, 0x18, 0x1c, }, // ↗ up-right arrow
{ 0x0d, 0x03, 0x07, }, // ↘ down-left arrow
{ 0x07, 0x03, 0x0d, }, // ↙ down-right arrow
};

const uint8_t lowercaseArrow[8][3] = {

  { 0x04, 0x08, 0x04, }, // ↑ up arrow
  { 0x04, 0x02, 0x04, }, // ↓ down arrow  
  { 0x04, 0x1a, 0x00, }, // ← left arrow
  { 0x00, 0x1a, 0x04, }, // → right arrow
  { 0x06, 0x04, 0x00, }, // ↖ up-left arrow
  { 0x00, 0x04, 0x06, }, // ↗ up-right arrow
  { 0x00, 0x02, 0x06, }, // ↘ down-left arrow
  { 0x06, 0x02, 0x00, }, // ↙ down-right arrow
};

// char symbolMap[40] = {
// '!', '$', '%', '^', '*', '_', '-', '+', '÷', 'x', '=', '±', '?', '<', '>', '~', '\'', ',', '.', '/', '\\', '(', ')', '[', ']', '{', '}', '|', ';', ':', 'µ', '°', '❬', '❭', '"', '\'', '𝟷', '𝟸', '𝟹'};

const wchar_t fontMap[120] = {
'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
'!', '$', '%', '^', '*', '_', '-', '+', L'÷', 'x', '=', L'±', '?', '<', '>', '~', '\'', ',', '.', '/', '\\',
'(', ')', '[', ']', '{', '}', '|', ';', ':', L'µ', L'°', L'❬', L'❭', '"', '\'', L'𝟷', L'𝟸', L'𝟹', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', L'↓', L'↑' };



const uint8_t font[][3] = // 'JumperlessFontmap', 500x5px
  { {
  0x1f, 0x11, 0x1f, },{ 0x12, 0x1f, 0x10, },{ 0x1d, 0x15, 0x17, },{ 0x11, 0x15, 0x1f, },{
  0x07, 0x04, 0x1f, },{ 0x17, 0x15, 0x1d, },{ 0x1f, 0x15, 0x1d, },{ 0x19, 0x05, 0x03, },{
  0x1f, 0x15, 0x1f, },{ 0x17, 0x15, 0x1f, },{ //9

  0x1e, 0x05, 0x1e, },{ 0x1f, 0x15, 0x0a, },{
  0x1f, 0x11, 0x11, },{ 0x1f, 0x11, 0x0e, },{ 0x1f, 0x15, 0x11, },{ 0x1f, 0x05, 0x01, },{
  0x0e, 0x15, 0x1d, },{ 0x1f, 0x04, 0x1f, },{ 0x11, 0x1f, 0x11, },{ 0x08, 0x10, 0x0f, },{
  0x1f, 0x04, 0x1b, },{ 0x1f, 0x10, 0x10, },{ 0x1f, 0x07, 0x1f, },{ 0x1f, 0x01, 0x1f, },{
  0x1f, 0x11, 0x1f, },{ 0x1f, 0x05, 0x07, },{ 0x0f, 0x09, 0x17, },{ 0x1f, 0x0d, 0x17, },{
  0x17, 0x15, 0x1d, },{ 0x01, 0x1f, 0x01, },{ 0x1f, 0x10, 0x1f, },{ 0x0f, 0x10, 0x0f, },{
  0x1f, 0x0c, 0x1f, },{ 0x1b, 0x04, 0x1b, },{ 0x03, 0x1c, 0x03, },{ 0x19, 0x15, 0x13, },{ //Z

  0x1c, 0x0a, 0x1c, },{ 0x1e, 0x14, 0x08, },{ 0x0c, 0x12, 0x12, },{ 0x08, 0x14, 0x1e, },{
  0x0e, 0x16, 0x14, },{ 0x1c, 0x0a, 0x02, },{ 0x14, 0x16, 0x0e, },{ 0x1e, 0x04, 0x18, },{
  0x00, 0x1d, 0x00, },{ 0x10, 0x0d, 0x00, },{ 0x1e, 0x08, 0x16, },{ 0x00, 0x1e, 0x10, },{
  0x1e, 0x06, 0x1e, },{ 0x1e, 0x02, 0x1c, },{ 0x1e, 0x12, 0x1e, },{ 0x1e, 0x0a, 0x04, },{
  0x04, 0x0a, 0x1c, },{ 0x1e, 0x02, 0x04, },{ 0x14, 0x1a, 0x0a, },{ 0x04, 0x1e, 0x14, },{
  0x1e, 0x10, 0x1e, },{ 0x0e, 0x10, 0x0e, },{ 0x1e, 0x18, 0x1e, },{ 0x16, 0x08, 0x16, },{
  0x06, 0x18, 0x06, },{ 0x12, 0x1a, 0x16, },{ //z

  0x00, 0x17, 0x00, },{ 0x16, 0x1f, 0x0d, },{
  0x19, 0x04, 0x13, },{ 0x02, 0x01, 0x02, },{ 0x02, 0x07, 0x02, },{ 0x04, 0x04, 0x04, },{
  0x04, 0x04, 0x04, },{ 0x04, 0x0e, 0x04, },{ 0x04, 0x15, 0x04, },{ 0x0a, 0x04, 0x0a, },{
  0x0a, 0x0a, 0x0a, },{ 0x12, 0x17, 0x12, },{ 0x01, 0x1d, 0x07, },{ 0x04, 0x0a, 0x11, },{
  0x11, 0x0a, 0x04, },{ 0x12, 0x17, 0x12, },{ 0x01, 0x02, 0x00, },{ 0x10, 0x08, 0x00, },{
  0x00, 0x10, 0x00, },{ 0x18, 0x04, 0x03, },{ 0x03, 0x04, 0x18, },{ 0x00, 0x0e, 0x11, },{
  0x11, 0x0e, 0x00, },{ 0x00, 0x1f, 0x11, },{ 0x00, 0x11, 0x1f, },{ 0x04, 0x0e, 0x1b, },{
  0x1b, 0x0e, 0x04, },{ 0x00, 0x1f, 0x00, },{ 0x10, 0x0a, 0x00, },{ 0x00, 0x0a, 0x00, },{
  0x1e, 0x08, 0x06, },{ 0x07, 0x05, 0x07, },{ 0x04, 0x0e, 0x1f, },{ 0x1f, 0x0e, 0x04, },{
  0x03, 0x00, 0x03, },{ 0x00, 0x03, 0x00, },{ 0x0a, 0x0f, 0x08, },{ 0x0d, 0x0b, 0x00, },{
  0x09, 0x0b, 0x0f, },{

  0x1e, 0x12, 0x1e, },{ 0x14, 0x1e, 0x10, },{ 0x1a, 0x12, 0x16, },{ 0x12, 0x16, 0x1e, },{ //lowercase Numbers
  0x0e, 0x08, 0x1e, },{ 0x16, 0x12, 0x1a, },{ 0x1e, 0x1a, 0x1a, },{ 0x12, 0x0a, 0x06, },{
  0x1e, 0x1a, 0x1e, },{ 0x16, 0x16, 0x1e, }, {0b00000010, 0b00000001, 0b00000010}, {0b00001000, 0b00010000, 0b00001000}

  };


//0=top rail, 1= gnd, 2 = bottom rail, 3 = gnd again, 4 = adc 1, 5 = adc 2, 6 = adc 3, 7 = adc 4, 8 = adc 5, 9 = adc 6, 10 = dac 0, 11 = dac 1, 12 = routable buffer in, 13 = routable buffer out, 14 = i sense +, 15 = isense -, 16 = gpio Tx, 17 = gpio Rx, 
uint32_t specialColors[13][5] = {
    {0x000000, 0x000000, 0x000000, 0x000000, 0x000000},
    {0x000000, 0x000000, 0x000000, 0x000000, 0x000000},
    {0x000000, 0x000000, 0x000000, 0x000000, 0x000000},
    {0x000000, 0x000000, 0x000000, 0x000000, 0x000000},
    {0x000000, 0x000000, 0x000000, 0x000000, 0x000000},
    {0x000000, 0x000000, 0x000000, 0x000000, 0x000000},
    {0x000000, 0x000000, 0x000000, 0x000000, 0x000000},
    {0x000000, 0x000000, 0x000000, 0x000000, 0x000000},
    {0x000000, 0x000000, 0x000000, 0x000000, 0x000000},
    {0x000000, 0x000000, 0x000000, 0x000000, 0x000000} };


// struct specialRowAnimation{
//     int net;
//     int currentFrame;
//     int numberOfFrames = 8;
//     uint32_t frames[8][5] = {0xffffff};


// };


int menuBrightnessSetting = -40; // -100 - 100 (0 default)

bool animationsEnabled = true;
specialRowAnimation rowAnimations[50];
volatile int doomOn = 0;

int wireStatus[64][5]; // row, led (net stored)
//char defconString[16] = " Fuck    You   ";
char defconString[16] = "Jumper less V5 ";

/* clang-format on */
int colorCycle = 0;
int defNudge = 0;


const int highSaturationSpectrumColors[51] = {
  // Red hues (0-30°)
  160, 196, 202, 166,
  // Orange hues (30-60°)
  208,  214, 178, 220,
  // Yellow hues (60-90°)
  184, 226, 190, 148, 154, 112, 118,
  // Yellow-Green hues (90-120°)
  76, 82,
  // Green hues (120-150°)
  40, 46, 47, 41,
  // Green-Cyan hues (150-180°)
  48, 42, 49, 43, 50,
  // Cyan hues (180-210°)
  44, 51, 45, 38, 39, 32, 33,
  // Cyan-Blue hues (210-240°)
  26, 27,
  // Blue hues (240-270°)
    63, 62,
  // Blue-Magenta hues (270-300°)
  93, 92, 129, 128, 165,
  // Magenta hues (300-330°)
  164, 201, 200, 163, 199, 162, 198,
  // Red-Magenta hues (330-360°)
  161, 197
};

const int highSaturationSpectrumColorsCount = 51;


const int highSaturationBrightColors[29] = {
  // Red hues (0-30°)
   196, 202, 
  // Orange hues (30-60°)
  208,  214,  220,
  // Yellow hues (60-90°)
   226, 190,  154,  118,
  // Yellow-Green hues (90-120°)
   82,
  // Green hues (120-150°)
   46, 47, 
  // Green-Cyan hues (150-180°)
  48,  49,  50,
  // Cyan hues (180-210°)
   51, 45,  39,  33,
  // Cyan-Blue hues (210-240°)
   27,
  // Blue hues (240-270°)
  63, 
  // Blue-Magenta hues (270-300°)
  99, 129, 165,
  // Magenta hues (300-330°)
   201, 200,  199,  198,
  // Red-Magenta hues (330-360°)
   197
};

const int highSaturationBrightColorsCount = 29;


void changeTerminalColor(int termColor, bool flush, Stream *stream) {

  if (disableTerminalColors) {
    return;
  }

  if (termColor != -1) {
    if (flush) {
      Jerial.flush();
    }
    Jerial.printf("\033[38;5;%dm", termColor);
    if (flush) {
      Jerial.flush();
    }
  } else {
    if (flush) {
      Jerial.flush();
    }
    Jerial.print("\033[0m"); // Reset all colors and formatting
    if (flush) {
      Jerial.flush();
    }
  }
}

extern "C" {

void changeTerminalColorC(int color, bool flush) {
  changeTerminalColor(color, flush, &Serial);
}

void cycleTermColor(bool reset,  float step, bool flush) {
  cycleTerminalColor(reset, step, flush, &Serial, 0, 0);
}
}

///@brief cycle through the high saturation spectrum colors (54 colors)
///@param reset if true, reset the color accumulator
///@param step the step size (0.1 - 100.0)
///@param flush if true, flush the stream
///@param stream the stream to print to
///@param startColorIndex the color index to start at (0-53) (only if reset is true)
void cycleTerminalColor(bool reset,  float step, bool flush, Stream *stream, int startColorIndex, int bright) {
  if (disableTerminalColors) {
    return;
  }
  static float stepDistance = 5.0f;
  static float colorAccumulator = 0.0f;

  if (stream == NULL) {
    stream = &Serial;
  }
  if (step < 80.0f) {
    stepDistance = step;
  } else {
    //stepDistance = 1.0f;
  }
  static int currentColor = 0;
  if (reset) {
    currentColor = startColorIndex;
    colorAccumulator = 0.0f;
  } else {
    // if (reverse) {
    //   colorAccumulator -= stepDistance;
    // } else {
      colorAccumulator += stepDistance;
    //}
    
    // Only update currentColor when we've accumulated enough for a full step
    while (colorAccumulator >= 1.0f) {
      currentColor++;
      colorAccumulator -= 1.0f;
      if (bright == 1) {
        if (currentColor >= highSaturationBrightColorsCount) {
          currentColor = 0;
        }
      } else {
        if (currentColor >= highSaturationSpectrumColorsCount) {
          currentColor = 0;
        }
      }
    }
    while (colorAccumulator <= -1.0f) {
      currentColor--;
      colorAccumulator += 1.0f;
      if (currentColor < 0) {
        if (bright == 1) {
          currentColor = highSaturationBrightColorsCount - 1;
        } else {
          currentColor = highSaturationSpectrumColorsCount - 1;
        }
      }
    }
  }
  int color = highSaturationSpectrumColors[currentColor];
  if (bright == 1) {
    color = highSaturationBrightColors[currentColor];
  }
  Jerial.printf("\033[38;5;%dm", color);
  if (flush) {
    Jerial.flush();
  }
}

///@brief change the terminal color to a high saturation color
///@param colorIndex the color index to change to (0-53)
///@param flush if true, flush the stream
///@param stream the stream to print to
void changeTerminalColorHighSat(int colorIndex, bool flush, Stream *stream, int bright) {
  static int currentColorIndex = 0;
  if (colorIndex == -1) {
   
    currentColorIndex++;
    if (bright == 1) {
      if (currentColorIndex >= highSaturationBrightColorsCount) {
        currentColorIndex = 0;
      }
    } else {
      if (currentColorIndex >= highSaturationSpectrumColorsCount) {
        currentColorIndex = 0;
      }
    }
    colorIndex = currentColorIndex;
  }

  int color = highSaturationSpectrumColors[colorIndex];
  if (bright == 1) {
    color = highSaturationBrightColors[colorIndex];
  }
  
  Jerial.printf("\033[38;5;%dm", color);
  if (flush) {
    Jerial.flush();
  }
}

void cycleTerminalColorHighSat(bool flush, Stream *stream) {
  static int currentColorIndex = 0;
  currentColorIndex++;
  if (currentColorIndex >= highSaturationBrightColorsCount) {
    currentColorIndex = 0;
  }
} 



void printSpectrumOrderedColorCube(void) {
  for (int i = 0; i < highSaturationSpectrumColorsCount; i++) {
    changeTerminalColor(highSaturationSpectrumColors[i], true, &Serial);
    Jerial.print("\t");
    Jerial.print(highSaturationSpectrumColors[i]);
    
    if (i % 12 == 0 && i != 0) {
      Jerial.println();
    }
  }
  Jerial.println();
}



int filledPaths[MAX_BRIDGES][4] = {-1}; // node1 node2 rowfilled


void drawWires(int net) {
  // int fillSequence[6] = {0,2,4,1,3,};
  // debugLEDs = 0;
  // Jerial.print("c2debugLEDs = ");
  // Jerial.println(debugLEDs);
  assignNetColors();

  // Jerial.println("drawWires");
  // Jerial.print("numberOfNets = ");

  // Jerial.println(numberOfNets);
  // Jerial.print("probeActive = ");
  // Jerial.println(probeActive);
  // Jerial.print("numberOfShownNets = ");
  // Jerial.println(numberOfShownNets);

  // Jerial.print("numberOfPaths = ");
  // Jerial.println(numberOfPaths);

  int fillSequence[6] = {0, 1, 2, 3, 4, 0};
  int fillIndex = 0;

  for (int i = 0; i < MAX_BRIDGES; i++) {
    for (int j = 0; j < 4; j++) {
      filledPaths[i][j] = -1;
    }
  }

  for (int i = 0; i < 62; i++) {
    for (int j = 0; j < 5; j++) {
      wireStatus[i][j] = 0;
    }
  }

  // for (int i = 0; i < numberOfNets; i++) {
  //   // Jerial.print(i);
  //   Jerial.print("netColors[");
  //   Jerial.print(i);
  //   Jerial.print("] = ");
  //   Jerial.print(netColors[i].r, HEX);
  //   Jerial.print(" ");
  //   Jerial.print(netColors[i].g, HEX);
  //   Jerial.print(" ");
  //   Jerial.println(netColors[i].b, HEX);
  // }
  if (net == -1) {

    for (int i = 0; i < numberOfPaths && i < MAX_BRIDGES; i++) {

      int sameLevel = 0;
      int bothOnTop = 0;
      int bothOnBottom = 0;
      int bothOnBB = 0;
      int whichIsLarger = 0;

      if (globalState.connections.paths[i].duplicate == 1) {
        continue;
      }

      if (globalState.connections.paths[i].node1 != -1 && globalState.connections.paths[i].node2 != -1 &&
          globalState.connections.paths[i].node1 != globalState.connections.paths[i].node2) {
        if ((globalState.connections.paths[i].node1 <= 60 &&
             globalState.connections.paths[i].node2 <= 60)) { //|| (globalState.connections.paths[i].node1 >= 110 &&
          // globalState.connections.paths[i].node1 <= 113) || (globalState.connections.paths[i].node2 >= 110 && globalState.connections.paths[i].node2 <=
          // 113)) {
          bothOnBB = 1;
          if (globalState.connections.paths[i].node1 > 0 && globalState.connections.paths[i].node1 < 30 && globalState.connections.paths[i].node2 > 0 &&
              globalState.connections.paths[i].node2 <= 30) {
            bothOnTop = 1;
            sameLevel = 1;
            if (globalState.connections.paths[i].node1 > globalState.connections.paths[i].node2) {
              whichIsLarger = 1;
            } else {
              whichIsLarger = 2;
            }
          } else if (globalState.connections.paths[i].node1 > 30 && globalState.connections.paths[i].node1 <= 60 &&
                     globalState.connections.paths[i].node2 > 30 && globalState.connections.paths[i].node2 <= 60) {
            bothOnBottom = 1;
            sameLevel = 1;
            if (globalState.connections.paths[i].node1 > globalState.connections.paths[i].node2) {
              whichIsLarger = 1;
            } else {
              whichIsLarger = 2;
            }
          }
        } else {
          // Jerial.println("else ");
          // Jerial.print("globalState.connections.paths[");
          // Jerial.print(i);
          // Jerial.print("].net = ");
          // Jerial.print(globalState.connections.paths[i].net);

          lightUpNet(globalState.connections.paths[i].net);
        }

        // if (sameLevel == 0 && ((globalState.connections.paths[i].node1 >= 110 && globalState.connections.paths[i].node1 <= 113)
        // || (globalState.connections.paths[i].node2 >= 110 && globalState.connections.paths[i].node2 <= 113)))
        // {
        //   sameLevel = 1;

        // }

        if (sameLevel == 1) {
          int range = 0;
          int first = 0;
          int last = 0;
          if (whichIsLarger == 1) {
            range = globalState.connections.paths[i].node1 - globalState.connections.paths[i].node2;
            first = globalState.connections.paths[i].node2;
            last = globalState.connections.paths[i].node1;
          } else {
            range = globalState.connections.paths[i].node2 - globalState.connections.paths[i].node1;
            first = globalState.connections.paths[i].node1;
            last = globalState.connections.paths[i].node2;
          }

          // Jerial.print("\nfirst = ");
          // Jerial.println(first);
          // Jerial.print("last = ");
          // Jerial.println(last);
          // Jerial.print("range = ");
          // Jerial.println(range);
          // Jerial.print("net = ");
          // Jerial.println(globalState.connections.paths[i].net);

          int inside = 0;
          int largestFillIndex = 0;

          for (int j = first; j <= first + range; j++) {
            // Jerial.print("j = ");
            // Jerial.println(j);
            for (int w = 0; w < 5; w++) {
              if ((wireStatus[j][w] == globalState.connections.paths[i].net || wireStatus[j][w] == 0) &&
                  w >= largestFillIndex) {

                // wireStatus[j][w] = globalState.connections.paths[i].net;
                if (w > largestFillIndex) {
                  largestFillIndex = w;
                }
                // Jerial.print("j = ");
                // Jerial.println(j);
                // if (first > 30) {
                //   Jerial.print("bottom ");
                // }

                break;
              }
            }
          }
          //           Jerial.print("largestFillIndex = ");
          // Jerial.println(largestFillIndex);
          // if (largestFillIndex > 4) {
          //   largestFillIndex = 0;
          // }

          for (int j = first; j <= first + range; j++) {
            if (j == first || j == last) {
              for (int k = largestFillIndex; k < 5; k++) {

                wireStatus[j][k] = globalState.connections.paths[i].net;
                // wireStatus[j][largestFillIndex] = globalState.connections.paths[i].net;
              }
            } else {
              wireStatus[j][largestFillIndex] = globalState.connections.paths[i].net;
            }
          }

          fillIndex = largestFillIndex;

          filledPaths[i][0] = first;
          filledPaths[i][1] = last;
          filledPaths[i][2] = fillSequence[fillIndex];

          // showLEDsCore2 = 1;
        } else {
          for (int j = 0; j < 5; j++) {

            if (globalState.connections.paths[i].node1 > 0 && globalState.connections.paths[i].node1 <= 60) {
              if (wireStatus[globalState.connections.paths[i].node1][j] == 0) {
                wireStatus[globalState.connections.paths[i].node1][j] = globalState.connections.paths[i].net;
              }

              // Jerial.print("globalState.connections.paths[i].node1 = ");
              // Jerial.println(globalState.connections.paths[i].node1);
            }
            if (globalState.connections.paths[i].node2 > 0 && globalState.connections.paths[i].node2 <= 60) {
              if (wireStatus[globalState.connections.paths[i].node2][j] == 0) {
                wireStatus[globalState.connections.paths[i].node2][j] = globalState.connections.paths[i].net;
              }
              // Jerial.print("globalState.connections.paths[i].node2 = ");
              // Jerial.println(globalState.connections.paths[i].node2);
            }
          }
          // lightUpNet(globalState.connections.paths[i].net);
        }
      } else {

        lightUpNet(globalState.connections.paths[i].net);
      }
    }
    for (int i = 0; i <= 60; i++) {
      for (int j = 0; j < 4; j++) {
        if (wireStatus[i][j] != 0) {
          if (wireStatus[i][j + 1] != wireStatus[i][j] &&
              wireStatus[i][j + 1] != 0 &&
              wireStatus[i][4] == wireStatus[i][j]) {
            wireStatus[i][j + 1] = wireStatus[i][j];
            // leds.setPixelColor((i * 5) + fillSequence[j], 0x000000);
          } else {
            // leds.setPixelColor((i * 5) + fillSequence[j], 0x100010);
          }
        }
      }
    }

    for (int i = 31; i <= 60; i++) { // reverse the bottom row

      int tempRow[5] = {wireStatus[i][0], wireStatus[i][1], wireStatus[i][2],
                        wireStatus[i][3], wireStatus[i][4]};
      wireStatus[i][0] = tempRow[4];
      wireStatus[i][1] = tempRow[3];
      wireStatus[i][2] = tempRow[2];
      wireStatus[i][3] = tempRow[1];
      wireStatus[i][4] = tempRow[0];
    }

    for (int i = 1; i <= 60; i++) {
      if (i <= 60) {

        for (int j = 0; j < 5; j++) {

          uint32_t color3 = 0x100010;

          rgbColor colorRGB = (wireStatus[i][j] < MAX_NETS)
                                  ? netColors[wireStatus[i][j]]
                                  : netColors[0];

          hsvColor colorHSV = RgbToHsv(colorRGB);

          // colorHSV.v = colorHSV.v * 0.25;
          // colorHSV.s = colorHSV.s * 0.5;
          colorRGB = HsvToRgb(colorHSV);

          uint32_t color = packRgb(colorRGB.r, colorRGB.g, colorRGB.b);

          if (wireStatus[i][j] == 0) {
            // leds.setPixelColor((i * 5) + fillSequence[j], 0x000000);
          } else if (probeHighlight != i) {
            leds.setPixelColor((((i - 1) * 5) + j), color);
          }
        }
      } else {
        for (int j = 0; j < 5; j++) {

          uint32_t color3 = 0x100010;

          rgbColor colorRGB = (wireStatus[i][j] < MAX_NETS)
                                  ? netColors[wireStatus[i][j]]
                                  : netColors[0];
          // Jerial.print("netColors[wireStatus[");
          // Jerial.print(i);
          // Jerial.print("][");
          // Jerial.print(j);
          // Jerial.print("] = ");
          // Jerial.print(netColors[wireStatus[i][j]].r, HEX);
          // Jerial.print(" ");
          // Jerial.print(netColors[wireStatus[i][j]].g, HEX);
          // Jerial.print(" ");
          // Jerial.println(netColors[wireStatus[i][j]].b, HEX);

          // int adcShow = 0;

          // hsvColor colorHSV = RgbToHsv(colorRGB);

          // colorHSV.v = colorHSV.v * 0.25;
          // colorHSV.s = colorHSV.s * 0.5;
          // colorRGB = HsvToRgb(colorHSV);

          uint32_t color = packRgb(colorRGB.r, colorRGB.g, colorRGB.b);
          // Jerial.print("color = ");
          // Jerial.println(color);

          if (wireStatus[i][j] == 0) {
            // leds.setPixelColor((i * 5) + fillSequence[j], 0x000000);
          } else if (probeHighlight != i) {
            leds.setPixelColor((((i - 1) * 5) + (4 - j)), color);
            // Jerial.print((((i - 1) * 5) + (4 - j)));
            // Jerial.print(" ");
          }
        }
      }
    }
  } else {
    // lightUpNet(net);
  }
}

// warningRowAnimation.index = warningRow;
// warningRowAnimation.net = warningNet;
// warningRowAnimation.currentFrame = 0;
// warningRowAnimation.numberOfFrames = 8;
// warningRowAnimation.type = 3; // warning row
// warningRowAnimation.direction = 1;
// warningRowAnimation.frameInterval = 100;
specialRowAnimation warningNetAnimation;

specialRowAnimation warningRowAnimation;

int animationOrder[26] = {TOP_RAIL,
                          GND,
                          BOTTOM_RAIL,
                          GND,
                          RP_GPIO_1,
                          RP_GPIO_2,
                          RP_GPIO_3,
                          RP_GPIO_4,
                          RP_GPIO_5,
                          RP_GPIO_6,
                          RP_GPIO_7,
                          RP_GPIO_8,
                          RP_UART_TX,
                          RP_UART_RX,
                          DAC0,
                          DAC1,
                          ROUTABLE_BUFFER_IN,
                          ROUTABLE_BUFFER_OUT,
                          ISENSE_PLUS,
                          ISENSE_MINUS,
                          ADC0,
                          ADC1,
                          ADC2,
                          ADC3,
                          ADC4,
                          ADC7};

/* clang-format off */



uint32_t highlightedRowFrames[15] = {
  0x001090, 0x002080, 0x003070, 0x004060, 0x005050,
  0x004040, 0x004050, 0x003060, 0x002070, 0x001080,
  0x010070, 0x020060, 0x030070, 0x020080, 0x010090 };

uint32_t warningRowFrames[15] = {
  0x090300, 0x080600, 0x070503, 0x060400, 0x050300,
  0x070200, 0x090103, 0x090001, 0x090003, 0x090100 };
//0x050800, 0x080600, 0x070500, 0x060401, 0x050302};

uint32_t animations[26][15] = {
  //top rail
      {0x080001, 0x080002, 0x070003, 0x080002, 0x080001,
       0x090000, 0x090000, 0x080100, 0x070200, 0x060300,
       0x070200, 0x080100, 0x080000, 0x090000, 0x080000},
       //gnd
           {0x000900, 0x000a00, 0x020b00, 0x000a00, 0x000900,
            0x000901, 0x000702, 0x000603, 0x000702, 0x000801,
            0x000800, 0x010800, 0x020800, 0x020800, 0x010800},
            //bottom rail
                {0x080001, 0x080002, 0x070003, 0x080002, 0x080001,
                 0x090000, 0x090000, 0x080100, 0x070200, 0x060300,
                 0x070200, 0x080100, 0x080000, 0x090000, 0x080000},
  };

uint8_t gpioAnimationBaseHues[10] = { 6, 28, 58, 84, 110, 146, 185, 204, 22, 131 };
// uint8_t highlightedRowOffsetHues[15] = { 0, 9, , 32, 46, 59, 71, 88, 78, 65, 38, 25, 15, 8, 3 };
uint8_t highlightedRowOffsetHues[15] = { 0, 5, 14, 22, 31, 46, 57, 64, 55, 52, 43, 32, 20, 10, 3 };

uint8_t probeConnectHighlightHues[15] = { 0, 5, 14, 22, 31, 46, 57, 64, 55, 52, 43, 32, 20, 10, 3 };

/* clang-format on */

uint8_t satValues[15] = {15,  25,  45,  65,  85,  105, 125, 145,
                         165, 185, 200, 190, 170, 80,  40};

unsigned long lastRowAnimationTime = 0;
unsigned long rowAnimationInterval = 150;

int numberOfRowAnimations = 0;

void initRowAnimations() {
  // 0=top rail, 1= gnd, 2 = bottom rail, 3 = gnd again, 4 = gpio 1, 5 = gpio 2,
  // 6 = gpio 3, 7 = gpio 4, 8 = gpio 5, 9 = gpio 6, 10 = gpio 7, 11 = gpio 8,
  uint8_t maxSat = 250;
  uint8_t minSat = 3;
  uint8_t gpioIdleBrightness = 8;

  int currentIndex = 0;

  // delay(1000);

  for (int i = 0; i < 10;
       i++) { // make the array of raw uint32_t values for the animations
    for (int j = 0; j < 15; j++) {
      hsvColor colorHSV;
      int hue = gpioAnimationBaseHues[i];
      if (hue < 0) {
        hue = 255 + hue;
      }
      if (hue > 255) {
        hue = hue - 255;
      }

      colorHSV = {(uint8_t)hue, (uint8_t)(satValues[j] + 15),
                  gpioIdleBrightness};

      uint32_t color = HsvToRaw(colorHSV);

      animations[i + 3][j] = color;
      // Jerial.print(color, HEX);
      // Jerial.print(" ");
    }
    // Jerial.println();
  }

  for (int i = 0; i < 3; i++) {
    rowAnimations[i].index = currentIndex;
    currentIndex++;
    rowAnimations[i].net = animationOrder[i];
    rowAnimations[i].currentFrame = 0;
    rowAnimations[i].numberOfFrames = 8;
    rowAnimations[i].type = 1; // top rail, gnd, bottom rail
    for (int j = 0; j < 15; j++) {
      rowAnimations[i].frames[j] = animations[i][j];
    }
  }
  rowAnimations[0].direction = 1;
  rowAnimations[0].frameInterval = 160;
  rowAnimations[1].direction = 0;
  rowAnimations[1].frameInterval = 100;
  rowAnimations[2].direction = 0;
  rowAnimations[2].frameInterval = 160;

  //! gpio input idle animations
  for (int i = 0; i < 10; i++) {
    rowAnimations[i + 3].index = currentIndex;
    currentIndex++;
    rowAnimations[i + 3].net = animationOrder[i + 3];
    rowAnimations[i + 3].currentFrame = i;
    rowAnimations[i + 3].numberOfFrames = 15;
    rowAnimations[i + 3].type = 2; // gpio idle
    for (int j = 0; j < 15; j++) {
      rowAnimations[i + 3].frames[j] = animations[i + 3][j];
    }
    rowAnimations[i + 3].direction = 1;
    rowAnimations[i + 3].frameInterval = 110;
  }

  //! gpio keeper input animations  
  for (int i = 0; i < 10; i++) {
    // High state keeper animation (type 7)
    rowAnimations[currentIndex].index = currentIndex;
    currentIndex++;
    rowAnimations[currentIndex - 1].net = animationOrder[i + 3];
    rowAnimations[currentIndex - 1].currentFrame = i;
    rowAnimations[currentIndex - 1].numberOfFrames = 15;
    rowAnimations[currentIndex - 1].type = 7; // gpio keeper high
    rowAnimations[currentIndex - 1].direction = 1;
    rowAnimations[currentIndex - 1].frameInterval = 120;
    
    // Create frames with red overlay for high state
    for (int j = 0; j < 15; j++) {
      int hue = gpioAnimationBaseHues[i]/3;

 
      

        int redOverlayHue = (hue * (100 - (j*8))) / 100; // 12% of base hue mixed with red (0)
        // if (j == 0) redOverlayHue = 5;
        // if (j == 15) redOverlayHue = 250;
        // if (j == 7) redOverlayHue = 0;




      hsvColor highColor = {(uint8_t)redOverlayHue, (uint8_t)min(255, satValues[j] + 180), (uint8_t)(gpioIdleBrightness + 18)};
      rowAnimations[currentIndex - 1].frames[j] = HsvToRaw(highColor);
    }
    
    // Low state keeper animation (type 8) 
    rowAnimations[currentIndex].index = currentIndex;
    currentIndex++;
    rowAnimations[currentIndex - 1].net = animationOrder[i + 3];
    rowAnimations[currentIndex - 1].currentFrame = i;
    rowAnimations[currentIndex - 1].numberOfFrames = 15;
    rowAnimations[currentIndex - 1].type = 8; // gpio keeper low
    rowAnimations[currentIndex - 1].direction = 0;
    rowAnimations[currentIndex - 1].frameInterval = 120;
    
    // Create frames with green overlay for low state
    for (int j = 0; j < 15; j++) {
      int hue = gpioAnimationBaseHues[i];

      int greenOverlayHue = (hue * (100 - (j*6)) + (85 * (j*6))) / 100; // 12% of base hue mixed with green (85)

      greenOverlayHue = (greenOverlayHue + (85 * 3))/4;
      
      
      hsvColor lowColor = {(uint8_t)greenOverlayHue, (uint8_t)min(255, satValues[j] + 180), (uint8_t)(gpioIdleBrightness + 18)};
      rowAnimations[currentIndex - 1].frames[j] = HsvToRaw(lowColor);
    }
  }

  //! warning row animation
  rowAnimations[currentIndex].index = currentIndex;
  currentIndex++;
  rowAnimations[currentIndex].net = 0;
  rowAnimations[currentIndex].currentFrame = 0;
  rowAnimations[currentIndex].numberOfFrames = 10;
  rowAnimations[currentIndex].type = 3; // warning row
  rowAnimations[currentIndex].direction = 1;
  rowAnimations[currentIndex].frameInterval = 100;
  for (int j = 0; j < rowAnimations[currentIndex].numberOfFrames; j++) {
    rowAnimations[currentIndex].frames[j] = warningRowFrames[j];
  }

  //! highlighted net animation
  rowAnimations[currentIndex].index = currentIndex;
  currentIndex++;
  rowAnimations[currentIndex].net = 0;
  rowAnimations[currentIndex].currentFrame = 0;
  rowAnimations[currentIndex].numberOfFrames = 15;
  rowAnimations[currentIndex].type = 4; // highlighted net
  rowAnimations[currentIndex].direction = 0;
  rowAnimations[currentIndex].frameInterval = 120;
  for (int j = 0; j < rowAnimations[currentIndex].numberOfFrames; j++) {
    rowAnimations[currentIndex].frames[j] = highlightedRowFrames[j % 10];
  }

  //! highlighted row animation
  rowAnimations[currentIndex].index = currentIndex;
  currentIndex++;
  rowAnimations[currentIndex].net = 0;
  rowAnimations[currentIndex].currentFrame = 0;
  rowAnimations[currentIndex].numberOfFrames = 15;
  rowAnimations[currentIndex].type = 6; // highlighted row
  rowAnimations[currentIndex].direction = 0;
  rowAnimations[currentIndex].frameInterval = 40;
  for (int j = 0; j < rowAnimations[currentIndex].numberOfFrames; j++) {
    rowAnimations[currentIndex].frames[j] = highlightedRowFrames[j % 10];
  }

  //! probe connect highlight row animation
  rowAnimations[currentIndex].index = currentIndex;
  currentIndex++;
  rowAnimations[currentIndex].net = 0;
  rowAnimations[currentIndex].currentFrame = 0;
  rowAnimations[currentIndex].numberOfFrames = 15;
  rowAnimations[currentIndex].type = 5; // probe connect highlight row
  rowAnimations[currentIndex].direction = 0;
  rowAnimations[currentIndex].frameInterval = 30;
  for (int j = 0; j < rowAnimations[currentIndex].numberOfFrames; j++) {
    rowAnimations[currentIndex].frames[j] = highlightedRowFrames[j % 10];
  }

  numberOfRowAnimations = currentIndex;
}

// 0-2 = top rail, gnd, bottom rail // type 1
// 3-12 = gpio 1-10 floating // type 2
// 13-32 = gpio 1-10 keeper (odd=high, even=low) // type 7/8
// 33 = warning row // type 3
// 34 = highlighted net // type 4
// 35 = highlighted row // type 6
// 36 = probe connect highlight row // type 5

/// index is the net, value is the animation index
int assignedAnimations[MAX_NETS] = {-1};

// int rowAnimations[100] = {0};

void assignRowAnimations(void) {

  for (int i = 0; i < numberOfNets + 3; i++) {
    assignedAnimations[i] = -1;
    // if (i < 26) {
    //   rowAnimations[i].net = -1;
    //   }
  }

  for (int net = 0; net < numberOfNets; net++) {

    if (net < 6) {
      switch (net) {
      case 1:
        assignedAnimations[net] = 1;
        rowAnimations[1].net = net;
        break;
      case 2:
        assignedAnimations[net] = 0;
        rowAnimations[0].net = net;
        break;
      case 3:
        assignedAnimations[net] = 2;
        rowAnimations[2].net = net;
        break;
      }
    }

    for (int i = 0; i < 10; i++) {
      if (gpioNet[i] > numberOfNets) {
        // Jerial.print("gpioNet[");
        // Jerial.print(i);
        // Jerial.print("] = ");
        // Jerial.println(gpioNet[i]);
        // Jerial.print("numberOfNets = ");
        // Jerial.println(numberOfNets);

        // Revert: always clear out-of-range nets; they will be reassigned on refresh
        gpioNet[i] = -1;
        assignedAnimations[gpioNet[i]] = -1;
        continue;
      }
      if (gpioNet[i] > 0) {
        // Check if GPIO is in bus keeper mode (state 7)
        if (gpioState[i] == 7) {
          // Assign keeper animation based on current reading
          int keeperAnimationIndex = 13 + (i * 2); // Start after idle animations
          if (keeperAnimationIndex + 1 < 50) { // Bounds check
            if (gpioReading[i] == 1) {
              // High state - use keeper high animation
              assignedAnimations[gpioNet[i]] = keeperAnimationIndex;
              rowAnimations[keeperAnimationIndex].net = gpioNet[i];
            } else {
              // Low state - use keeper low animation  
              assignedAnimations[gpioNet[i]] = keeperAnimationIndex + 1;
              rowAnimations[keeperAnimationIndex + 1].net = gpioNet[i];
            }
          }
        } else {
          // Regular idle animation for non-keeper modes
          assignedAnimations[gpioNet[i]] = i + 3;
          rowAnimations[i + 3].net = gpioNet[i];
        }
      }
    }

    if (brightenedNet > 0) {
      assignedAnimations[brightenedNet] = 34; // Updated index after keeper animations
      rowAnimations[34].net = brightenedNet;
    }

    if (warningNet > 0) {
      assignedAnimations[warningNet] = 33; // Updated index after keeper animations  
      rowAnimations[33].net = warningNet;
    }
  }
  // Jerial.println(" ");
  //   for (int i = 0; i < numberOfNets; i++) {

  //     Jerial.print(i);
  //     Jerial.print(" ");
  //     Jerial.print(assignedAnimations[i]);
  //     Jerial.print("\t");

  //     for (int j = 0; j < 15; j++) {
  //       Jerial.printf("%06x ",
  //       rowAnimations[assignedAnimations[i]].frames[j]); Jerial.print(" ");
  //     }
  //     Jerial.println();

  //   }
  //  Jerial.println();
}

void showRowAnimation(int net) {

  if (assignedAnimations[net] == -1) {
    return;
  }

  showRowAnimation(assignedAnimations[net], net);
}

void animateBrightenedRow(int row) {}

//! do a hightlighted row animation



void showRowAnimation(int index, int net) {

  // net = 0;

  int structIndex = -1;
  int actualNet = -1;
  // if (inPadMenu == 1) {
  //   return;
  //   }
  if (rowAnimations[index].net < 0) {
    return;
  }
  // if (rowAnimations[index].type == 3) {
  //   rowAnimations[index].net = warningNet;

  //   // Jerial.print("warningNet = ");
  //   // Jerial.println(warningNet);
  //   }

  actualNet = net; // findNodeInNet(net);

  if (actualNet <= 0) {
    return;
  }

  uint32_t frameColors[5];
  uint32_t brightenedNodeColors[5];

  if (rowAnimations[index].net == warningNet) {
    // Jerial.print("warningNet: ");
    // Jerial.println(warningNet);
    rowAnimations[index].row = warningRow;
    uint32_t color;

    for (int i = 0; i < rowAnimations[index].numberOfFrames; i++) {

      hsvColor colorHSV = RgbToHsv(netColors[warningNet]);
      colorHSV.h =
          ((colorHSV.h - (int)((highlightedRowOffsetHues[i] * 1.5))) / 8) % 255;
      // Jerial.print("colorHSV.h: ") ;
      // Jerial.println(colorHSV.h);
      colorHSV.v = jumperlessConfig.display.led_brightness + 10;
      // Jerial.print("colorHSV.h = ");
      // Jerial.println(colorHSV.h);
      // colorHSV.s = satValues[i];
      color = HsvToRaw(colorHSV);

      rowAnimations[index].frames[i] = color;
    }

    for (int i = 0; i < 5; i++) {
      frameColors[i] =
          rowAnimations[index].frames[(rowAnimations[index].currentFrame + i) %
                                      rowAnimations[index].numberOfFrames];
    }

    // handle the row animation for a single row, rather than the whole net
    if (rowAnimations[index].row > 0) {
      // Jerial.print("rowAnimations[index].row: ");
      // Jerial.println(rowAnimations[index].row);
      for (int i = 0; i < 5; i++) {

        uint32_t color = rowAnimations[index]
                             .frames[(rowAnimations[index].currentFrame + i) %
                                     rowAnimations[index].numberOfFrames];
        hsvColor colorHSV = RgbToHsv(unpackRgb(color));
        // colorHSV.h = (colorHSV.h + i*10) % 255;
        if (i == 2) {
          // colorHSV.s = 120;
          colorHSV.h = (rowAnimations[index].currentFrame % 3) * 10;
          // Jerial.print("rowAnimations[index].currentFrame: ");
          // Jerial.println(rowAnimations[index].currentFrame);
          colorHSV.v = 200; // jumperlessConfig.display.led_brightness+70;
        } else {
          // colorHSV.s = 230;
          colorHSV.v = jumperlessConfig.display.led_brightness + 40;
        }

        // colorHSV.h = (colorHSV.h + (int)(highlightedRowOffsetHues[(i +
        // rowAnimations[index].currentFrame) %
        // rowAnimations[index].numberOfFrames] )) % 255; colorHSV.v =
        // jumperlessConfig.display.led_brightness;
        brightenedNodeColors[4 - i] = HsvToRaw(colorHSV);

        // brightenedNodeColors[i] =
        // highlightedRowFrames[(rowAnimations[net].currentFrame + i) %
        // rowAnimations[net].numberOfFrames];
      }
    }

  } else if (brightenedNet > 0 && net == brightenedNet) {

    rowAnimations[index].row = brightenedNode - 1;
    uint32_t color;

    for (int i = 0; i < rowAnimations[index].numberOfFrames; i++) {
      hsvColor colorHSV;

      // Jerial.print("netColors[brightenedNet]: ");
      // Jerial.println(packRgb( netColors[brightenedNet]), HEX);
      // if (rowAnimations[index].net > 3) {
      colorHSV = RgbToHsv(netColors[brightenedNet]);
      colorHSV.h =
          (colorHSV.h + (int)(highlightedRowOffsetHues[i] / 1.8)) % 255;
      // } else {
      //  colorHSV = RgbToHsv(netColors[rowAnimations[index].net]);
      //  }
      colorHSV.v = jumperlessConfig.display.led_brightness + 10;
      // Jerial.print("colorHSV.h = ");
      // Jerial.println(colorHSV.h);
      // colorHSV.s = satValues[i];
      color = HsvToRaw(colorHSV);

      rowAnimations[index].frames[i] = color;
    }

    for (int i = 0; i < 5; i++) {
      frameColors[i] =
          rowAnimations[index].frames[(rowAnimations[index].currentFrame + i) %
                                      rowAnimations[index].numberOfFrames];
    }

    // handle the row animation for a single row, rather than the whole net
    if (rowAnimations[index].row >= 0) {
      // Jerial.print("rowAnimations[index].row: ");
      // Jerial.println(rowAnimations[index].row);
      for (int i = 0; i < 5; i++) {

        uint32_t color = rowAnimations[index]
                             .frames[(rowAnimations[index].currentFrame + i) %
                                     rowAnimations[index].numberOfFrames];
        hsvColor colorHSV = RgbToHsv(unpackRgb(color));
        // colorHSV.h = (colorHSV.h + i*10) % 255;
        // if (rowAnimations[index].net > 3) {
        if (i == 2) {
          colorHSV.s = 120;
          colorHSV.v = jumperlessConfig.display.led_brightness + 70;
        } else {
          colorHSV.s = 230;
          colorHSV.v = jumperlessConfig.display.led_brightness + 50;
        }
        // }

        // colorHSV.h = (colorHSV.h + (int)(highlightedRowOffsetHues[(i +
        // rowAnimations[index].currentFrame) %
        // rowAnimations[index].numberOfFrames] )) % 255; colorHSV.v =
        // jumperlessConfig.display.led_brightness;
        brightenedNodeColors[4 - i] = HsvToRaw(colorHSV);

        // brightenedNodeColors[i] =
        // highlightedRowFrames[(rowAnimations[net].currentFrame + i) %
        // rowAnimations[net].numberOfFrames];
      }
    }

  } else {
    for (int i = 0; i < 5; i++) {
      frameColors[i] =
          rowAnimations[index].frames[(rowAnimations[index].currentFrame + i) %
                                      rowAnimations[index].numberOfFrames];
    }
  }

  if (rowAnimations[index].type == 2) {
    int gpioIndex = index - 3;
    if (gpioNet[gpioIndex] == actualNet && gpioNet[gpioIndex] != -1) {
      if (gpioReading[gpioIndex] == 0 ||
          gpioReading[gpioIndex] == 1) { // if any gpio is low or high, don't
                                         // show the animation, let gpioRead()

        return;
        // continue;
      }
    }
  } else if (rowAnimations[index].type == 7 || rowAnimations[index].type == 8) {
    // Bus keeper animations - show only if GPIO state matches animation type
    int gpioIndex = (index - 13) / 2; // Calculate GPIO index from keeper animation index
    if (gpioIndex >= 0 && gpioIndex < 10 && gpioNet[gpioIndex] == actualNet && gpioNet[gpioIndex] != -1) {
      if (gpioState[gpioIndex] == 7) { // Confirm it's in keeper mode
        // Type 7 = keeper high, Type 8 = keeper low
        bool shouldShowHigh = (rowAnimations[index].type == 7);
        bool gpioIsHigh = (gpioReading[gpioIndex] == 1);
        
        if (shouldShowHigh != gpioIsHigh) {
          return; // Don't show wrong state animation
        }
      } else {
        return; // Not in keeper mode anymore
      }
    }
  }

  if (millis() - rowAnimations[index].lastFrameTime >
      rowAnimations[index].frameInterval) {
    rowAnimations[index].currentFrame++;
    if (rowAnimations[index].currentFrame >
        rowAnimations[index].numberOfFrames) {
      rowAnimations[index].currentFrame = 0;
    }
    rowAnimations[index].lastFrameTime = millis();
  }

  brightenedNodeColors[4] = brightenedNodeColors[0];
  brightenedNodeColors[3] = brightenedNodeColors[1];
  // brightenedNodeColors[2] = brightenedNodeColors[2];
  // brightenedNodeColors[3] = brightenedNodeColors[1];
  // brightenedNodeColors[4] = brightenedNodeColors[0];

  // Jerial.println(" ");
  int row = 2;

  if (rowAnimations[index].direction == 0) {

    uint32_t tempFrame[5] = {frameColors[0], frameColors[1], frameColors[2],
                             frameColors[3], frameColors[4]};
    frameColors[0] = tempFrame[4];
    frameColors[1] = tempFrame[3];
    frameColors[2] = tempFrame[2];
    frameColors[3] = tempFrame[1];
    frameColors[4] = tempFrame[0];
  }
  // Jerial.print("\n\n\rnet = ");
  // Jerial.print(net);
  // Jerial.print("   actualNet = ");
  // Jerial.print(actualNet);
  // Jerial.print("   direction = ");
  // Jerial.println(rowAnimations[net].direction);

  if (jumperlessConfig.display.lines_wires == 0 ||
      numberOfShownNets > MAX_NETS_FOR_WIRES) {
    for (int i = 0; i < numberOfPaths && i < MAX_BRIDGES; i++) {
      if (globalState.connections.paths[i].net == actualNet) {
        if (globalState.connections.paths[i].skip == true) {
          continue;
        }

        if (globalState.connections.paths[i].node1 > 0 && globalState.connections.paths[i].node1 <= 60 &&
            globalState.connections.paths[i].node1 != probeHighlight) {
          for (int j = 0; j < 5; j++) {

            b.printRawRow(0b00010000 >> j, globalState.connections.paths[i].node1 - 1, frameColors[j],
                          0xfffffe, 4);
          }
        }
        if (globalState.connections.paths[i].node2 > 0 && globalState.connections.paths[i].node2 <= 60 &&
            globalState.connections.paths[i].node2 != probeHighlight) {
          for (int j = 0; j < 5; j++) {

            b.printRawRow(0b00010000 >> j, globalState.connections.paths[i].node2 - 1, frameColors[j],
                          0xfffffe, 4);
          }
        }
      }
    }

    // for (int i = 0; i < 5; i++) {
    //   b.printRawRow(0b00000001 << i, row, frameColors[i], 0xfffffe);
    // }
  } else {

    for (int i = 0; i <= 60; i++) {
      for (int j = 0; j < 5; j++) {
        if (wireStatus[i][j] == actualNet) {
          if (i == probeHighlight) {

          } else if (brightenedNode > 0 && i == brightenedNode) {
            leds.setPixelColor(((i - 1) * 5) + j, brightenedNodeColors[j]);
            // Jerial.print("brightenedNode: ");
            // Jerial.println(brightenedNode);
          } else {
            leds.setPixelColor(((i - 1) * 5) + j, frameColors[j]);
          }
        }
      }
    }
  }

  for (int i = 0; i < numberOfPaths && i < MAX_BRIDGES; i++) {
    //     if (globalState.connections.paths[i].skip == true) {
    //   continue;
    // }
    if (globalState.connections.paths[i].net == actualNet) {

      if (globalState.connections.paths[i].node1 > NANO_D0 && globalState.connections.paths[i].node1 <= NANO_GND_1) {
        for (int j = 0; j < 35; j++) {
          if (bbPixelToNodesMapV5[j][0] == globalState.connections.paths[i].node1) {
            int brightness = (brightenedNode == globalState.connections.paths[i].node1)
                                 ? brightenedNodeAmount
                                 : brightenedNetAmount;
            leds.setPixelColor(bbPixelToNodesMapV5[j][1],
                               scaleBrightness(frameColors[2], brightness));
          }
        }
      }
      if (globalState.connections.paths[i].node2 > NANO_D0 && globalState.connections.paths[i].node2 <= NANO_GND_1) {
        for (int j = 0; j < 35; j++) {
          if (bbPixelToNodesMapV5[j][0] == globalState.connections.paths[i].node2) {
            int brightness = (brightenedNode == globalState.connections.paths[i].node2)
                                 ? brightenedNodeAmount
                                 : brightenedNetAmount;
            leds.setPixelColor(bbPixelToNodesMapV5[j][1],
                               scaleBrightness(frameColors[2], brightness));
          }
        }
      }
    }
  }

  // b.printRawRow(0b00000001, row, frameColors[0], 0xfffffe);
  // b.printRawRow(0b00000010, row, frameColors[1], 0xfffffe);
  // b.printRawRow(0b00000100, row, frameColors[2], 0xfffffe);
  // b.printRawRow(0b00001000, row, frameColors[3], 0xfffffe);
  // b.printRawRow(0b00010000, row, frameColors[4], 0xfffffe);

  // showLEDsCore2 = 2;
  showSkippedNodes();
}

void showAllRowAnimations() {
  // showRowAnimation(2, rowAnimations[1].net);
  // showRowAnimation(2, rowAnimations[2].net);
  // for (int i = 0; i < 10; i++) {
  //   rowAnimations[i+3].net = gpioNet[i];
  //   }

  assignRowAnimations();

  for (int i = 0; i <= numberOfNets; i++) {

    showRowAnimation(i);
    if (rowAnimations[i].type == 3) {
      // Jerial.print("warningNet = ");
      // Jerial.print(warningNet);
      // Jerial.print(" ");
      // Jerial.println(millis());
    }
  }
}

void printWireStatus(void) {

  for (int s = 1; s <= 30; s++) {
    Jerial.print(s);
    Jerial.print(" ");
    if (s < 9) {
      Jerial.print(" ");
    }
  }
  Jerial.println();

  int level = 1;
  for (int r = 0; r < 5; r++) {
    for (int s = 1; s <= 30; s++) {
      Jerial.print(wireStatus[s][r]);
      Jerial.print(" ");
      if (wireStatus[s][r] < 10) {
        Jerial.print(" ");
      }
    }
    Jerial.println();
  }
  Jerial.println("\n\n");
  for (int s = 31; s <= 60; s++) {
    Jerial.print(s);
    Jerial.print(" ");
    if (s < 9) {
      Jerial.print(" ");
    }
  }
  Jerial.println();
  for (int r = 0; r < 5; r++) {
    for (int s = 31; s <= 60; s++) {
      Jerial.print(wireStatus[s][r]);
      Jerial.print(" ");
      if (wireStatus[s][r] < 10) {
        Jerial.print(" ");
      }
    }
    Jerial.println();
  }
}
// }
uint32_t defaultColor = 0x001012;

bread::bread() {
  // defaultColor = 0x060205;
}

void bread::print(const char c) { printChar(c, defaultColor); }

void bread::print(const char c, int position) {
  printChar(c, defaultColor, position);
}

void bread::print(const char c, uint32_t color) { printChar(c, color); }

void bread::print(const char c, uint32_t color, int position, int topBottom) {
  printChar(c, color, position, topBottom);
}

void bread::print(const char c, uint32_t color, int topBottom) {
  printChar(c, color, topBottom);
}

void bread::print(const char c, uint32_t color, uint32_t backgroundColor) {
  printChar(c, color, backgroundColor);
}

void bread::print(const char c, uint32_t color, uint32_t backgroundColor,
                  int position, int topBottom) {
  printChar(c, color, backgroundColor, position, topBottom);
}

void bread::print(const char c, uint32_t color, uint32_t backgroundColor,
                  int position, int topBottom, int nudge) {
  printChar(c, color, backgroundColor, position, topBottom, nudge);
}

void bread::print(const char c, uint32_t color, uint32_t backgroundColor,
                  int position, int topBottom, int nudge, int lowercaseNumber) {
  printChar(c, color, backgroundColor, position, topBottom, nudge,
            lowercaseNumber);
}

void bread::print(const char *s) {
  // Jerial.println("1");
  printString(s, defaultColor);
}

void bread::print(const char *s, int position) {
  // Jerial.println("2");
  printString(s, defaultColor, 0xffffff, position);
}

void bread::print(const char *s, uint32_t color) {
  // Jerial.println("3");
  printString(s, color);
}

void bread::print(const char *s, uint32_t color, uint32_t backgroundColor) {
  // Jerial.println("4");
  printString(s, color, backgroundColor);
}

void bread::print(const char *s, uint32_t color, uint32_t backgroundColor,
                  int position, int topBottom) {
  // Jerial.println("5");
  printString(s, color, backgroundColor, position, topBottom);
}

void bread::print(const char *s, uint32_t color, uint32_t backgroundColor,
                  int position, int topBottom, int nudge) {
  // Jerial.println("5");
  printString(s, color, backgroundColor, position, topBottom, nudge);
}

void bread::print(const char *s, uint32_t color, uint32_t backgroundColor,
                  int position, int topBottom, int nudge, int lowercaseNumber) {
  // Jerial.println("5");
  printString(s, color, backgroundColor, position, topBottom, nudge,
              lowercaseNumber);
}

void bread::print(const char *s, uint32_t color, uint32_t backgroundColor,
                  int topBottom) {
  // Jerial.println("6");
  printString(s, color, backgroundColor, 0, topBottom);
}

void bread::print(int i) {
  // Jerial.println("7");
  char buffer[15];
  itoa(i, buffer, 10);
  printString(buffer, defaultColor);
  // Jerial.println(buffer);
}

// void bread::print(int i, int position) {
//   char buffer[15];
//   itoa(i, buffer, 10);
//   printString(buffer, defaultColor, 0xffffff, position);
// }

void bread::print(int i, uint32_t color) {
  char buffer[15];
  itoa(i, buffer, 10);
  printString(buffer, color);
}

void bread::print(int i, uint32_t color, int position) {
  char buffer[15];
  itoa(i, buffer, 10);
  printString(buffer, color, 0xffffff, position);
}

void bread::print(int i, uint32_t color, int position, int topBottom) {
  char buffer[15];
  itoa(i, buffer, 10);
  printString(buffer, color, 0xffffff, position, topBottom);
}
void bread::print(int i, uint32_t color, int position, int topBottom,
                  int nudge) {
  char buffer[15];
  itoa(i, buffer, 10);
  printString(buffer, color, 0xffffff, position, topBottom, nudge);
}
void bread::print(int i, uint32_t color, int position, int topBottom, int nudge,
                  int lowercase) {
  char buffer[15];
  itoa(i, buffer, 10);
  printString(buffer, color, 0xffffff, position, topBottom, nudge);
}

void bread::print(int i, uint32_t color, uint32_t backgroundColor) {
  char buffer[15];
  itoa(i, buffer, 10);
  printString(buffer, color, backgroundColor);
}

void bread::printMenuReminder(int menuDepth, uint32_t color) {
  uint8_t columnMask[5] = // 'JumperlessFontmap', 500x5px
      {0b00000001, 0b00000010, 0b00000100, 0b00001000, 0b00010000};
  uint8_t graphicRow[3] = {0x00, 0x00, 0x00};

  if (menuDepth > 6) {
    menuDepth = 6;
  }

  switch (menuDepth) {
  case 1:

    graphicRow[2] = 0b00000000;
    graphicRow[1] = 0b00010000;
    graphicRow[0] = 0b00010000;
    break;
  case 2:
    graphicRow[2] = 0b00000000;
    graphicRow[1] = 0b00001000;
    graphicRow[0] = 0b00011000;

    break;

  case 3:
    graphicRow[2] = 0b00000000;
    graphicRow[1] = 0b00000100;
    graphicRow[0] = 0b00011100;

    break;

  case 4:

    graphicRow[2] = 0b00000000;
    graphicRow[1] = 0b00000010;
    graphicRow[0] = 0b00011110;

    break;

  case 5:

    graphicRow[2] = 0b00000000;
    graphicRow[1] = 0b00000001;
    graphicRow[0] = 0b00011111;

    break;

  case 6:

    graphicRow[2] = 0b00000001;
    graphicRow[1] = 0b00000001;
    graphicRow[0] = 0b00011111;

    break;
  }

  if (color == 0xFFFFFF) {
    color = defaultColor;
  }

  for (int i = 0; i < 3; i++) {

    printGraphicsRow(graphicRow[i], i, color);
  }
}

void bread::printRawRow(uint8_t data, int row, uint32_t color, uint32_t bg,
                        int scale) {

  // color = scaleBrightness(color, (menuBrightnessSetting / scale));
  if (row <= 60) {
    printGraphicsRow(data, row, color, bg);
  } else {
    for (int i = 0; i < 35; i++) {
      if (bbPixelToNodesMapV5[i][0] == row + 1) {
        leds.setPixelColor(bbPixelToNodesMapV5[i][1], color);
        return;
      }
    }
  }
}


void bread::lightUpNode(int node, uint32_t color) {
  if (node <= 0) {
    return;
  }

  // Breadboard rows 1..60
  if (node >= 1 && node <= 60) {
    // Fill all 5 pixels in the row
    printGraphicsRow(0b00011111, node - 1, color, 0xfffffe);
    return;
  }

  // Header/special nodes mapped via bbPixelToNodesMapV5

  if (node >= 69 && node <= 99) {
  for (int i = 0; i < 35; i++) {
    if (bbPixelToNodesMapV5[i][0] == node) {
      leds.setPixelColor(bbPixelToNodesMapV5[i][1], color);
      return;
    }
  }
}

  if (node >= 100 && node <= 126) {


  if (node == ADC_PAD) {
    setLogoOverride(ADC_0, color);
    setLogoOverride(ADC_1, color);
    return;
  }
  if (node == DAC_PAD) {
    setLogoOverride(DAC_0, color);
    setLogoOverride(DAC_1, color);
    return;
  }
  if (node == GPIO_PAD) {
    setLogoOverride(GPIO_0, color);
    setLogoOverride(GPIO_1, color);
        return;
  }
  if (node == LOGO_PAD_TOP) {
    setLogoOverride(LOGO_TOP, color);
    return;
  }
  if (node == LOGO_PAD_BOTTOM) {
    setLogoOverride(LOGO_BOTTOM, color);
    return;
  }
  if (node == TOP_RAIL) {
    brightenedRail = 0;
    return;
  }
  if (node == BOTTOM_RAIL) {
    brightenedRail = 2;
    return;
  }
  if (node == TOP_RAIL_GND) {
    brightenedRail = 1;
    return;
  }
  if (node == BOTTOM_RAIL_GND) {
    brightenedRail = 3;
    return;
  }
}
  // if (node == BUILDING_PAD_TOP) {
  //   setLogoOverride(LOGO, color);
  //   return;
  // }
  // if (node == BUILDING_PAD_BOTTOM) {
  //   setLogoOverride(LOGO, color);
  //   return;
  // }
}


void bread::barGraph(int position, int value, int maxValue, int leftRight,
                     uint32_t color, uint32_t bg) {}
/*

||||||||||||||||||||||||||||||
  |0| |1| |2| |3| |4| |5| |6|
||||||||||||||||||||||||||||||

||||||||||||||||||||||||||||||
  |7| |8| |9| |A| |B| |C| |D|
||||||||||||||||||||||||||||||


*/
void printGraphicsRow(uint8_t data, int row, uint32_t color, uint32_t bg) {
  uint8_t columnMask[5] = // 'JumperlessFontmap', 500x5px
      {0b00010000, 0b00001000, 0b00000100, 0b00000010, 0b00000001};

  if (color == 0xFFFFFF) {
    color = defaultColor;
  }
  if (bg == 0xFFFFFF) {

    for (int j = 4; j >= 0; j--) {
      // Jerial.println(((data) & columnMask[j]) != 0 ? "1" : "0");
      if (((data)&columnMask[j]) != 0) {
        color = scaleBrightness(color, menuBrightnessSetting);
        leds.setPixelColor(((row) * 5) + j, color);
      } else {
        leds.setPixelColor(((row) * 5) + j, 0);
      }
    }
  } else if (bg == 0xFFFFFE) {

    for (int j = 4; j >= 0; j--) {
      // Jerial.println(((data) & columnMask[j]) != 0 ? "1" : "0");
      if (((data)&columnMask[j]) != 0) {
        color = scaleBrightness(color, menuBrightnessSetting);
        leds.setPixelColor(((row) * 5) + j, color);
      } else {
        // leds.getPixelColor(((row) * 5) + j);
        // leds.setPixelColor(((row) * 5) + j, 0);
      }
    }
  } else {
    for (int j = 4; j >= 0; j--) {
      if (((data)&columnMask[j]) != 0) {
        color = scaleBrightness(color, menuBrightnessSetting);
        leds.setPixelColor(((row) * 5) + j, color);
      } else {
        leds.setPixelColor(((row) * 5) + j, bg);
      }
    }
  }
}

int nodeToPrintRow(int node) {
  if (node <= 0) {
    return -1;
  }
  return node - 1;
}

void printChar(const char c, uint32_t color, uint32_t bg, int position,
               int topBottom, int nudge, int lowercaseNumber) {

  int charPosition = position;

  color = scaleBrightness(color, menuBrightnessSetting);

  if (topBottom == 1) {
    charPosition = charPosition % 7;
    charPosition += 7;
    if (charPosition > 13) {
      return;
    }
  }
  if (topBottom == 0) {
    if (charPosition > 6) {
      return;
      // charPosition = charPosition % 7;
    }
    charPosition = charPosition % 7;
  }

  charPosition = charPosition % 14;

  charPosition = charPosition * 4;

  if (charPosition > (6 * 4)) {
    charPosition = charPosition + 2;
  }
  // charPosition = charPosition * 4;
  charPosition = charPosition + 2;

  if (color == 0xFFFFFF) {
    color = defaultColor;
  }
  int fontMapIndex = -1;
  int start = 0;

  if (lowercaseNumber > 0 && c >= 48 && c <= 57) {
    start = 90;
  }

  for (int i = start; i < 120; i++) {
    if (c == fontMap[i]) {
      fontMapIndex = i;
      break;
    }
  }
  if (fontMapIndex == -1) {
    return;
  }
  uint8_t columnMask[5] = // 'JumperlessFontmap', 500x5px
      {0b00000001, 0b00000010, 0b00000100, 0b00001000, 0b00010000};

  if (bg == 0xFFFFFF) {
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 5; j++) {
        if (((font[fontMapIndex][i]) & columnMask[j]) != 0) {
          leds.setPixelColor(((charPosition + i + nudge) * 5) + j, color);
        } else {
          leds.setPixelColor(((charPosition + i + nudge) * 5) + j, 0);
        }
      }
    }
  } else if (bg == 0xFFFFFD) {
    for (int j = 0; j < 5; j++) {

      leds.setPixelColor(((charPosition + nudge - 1) * 5) + j, 0);
    }

    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 5; j++) {
        if (((font[fontMapIndex][i]) & columnMask[j]) != 0) {
          leds.setPixelColor(((charPosition + i + nudge) * 5) + j, color);
        } else {
          leds.setPixelColor(((charPosition + i + nudge) * 5) + j, 0);
        }
      }
    }
    for (int j = 0; j < 5; j++) {

      leds.setPixelColor(((charPosition + nudge + 3) * 5) + j, 0);
    }
  } else if (bg == 0xFFFFFE) {
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 5; j++) {
        if (((font[fontMapIndex][i]) & columnMask[j]) != 0) {
          leds.setPixelColor(((charPosition + i + nudge) * 5) + j, color);
        } else {
          // leds.setPixelColor((i*5)+j, bg);
        }
      }
    }
  } else {
    if (charPosition + nudge != 0) {
      for (int j = 0; j < 5; j++) {

        leds.setPixelColor(((charPosition + nudge - 1) * 5) + j, bg);
      }
    }
    for (int i = 0; i < 4; i++) {
      if (i < 3) {
        for (int j = 0; j < 5; j++) {
          if (((font[fontMapIndex][i]) & columnMask[j]) != 0) {
            leds.setPixelColor(((charPosition + i + nudge) * 5) + j, color);
          } else {
            leds.setPixelColor(((charPosition + i + nudge) * 5) + j, bg);
          }
        }
      } else {
        for (int j = 0; j < 5; j++) {
          leds.setPixelColor(((charPosition + i + nudge) * 5) + j, bg);
        }
      }
    }
  }
}

void printString(const char *s, uint32_t color, uint32_t bg, int position,
                 int topBottom, int nudge, int lowercaseNumber) {
  // int position = 0;

  for (int i = 0; i < strlen(s); i++) {

    // if (topBottom == 1) {
    //   position = position % 7;

    // } else if (topBottom == 0) {
    //   position = position % 7;

    // } else {
    //   position = position % 14;
    // }
    // Jerial.print(s[i]);
    // Jerial.print(" ");
    // Jerial.println(position);
    // if (i > strlen(s))
    // {
    //     printChar(' ', 0x000000, 0x000000, position, topBottom);
    // } else {
    // Jerial.println(position);
    printChar(s[i], color, bg, position, topBottom, nudge, lowercaseNumber);
    // }

    position++;
  }
  // Jerial.println();
}

void bread::clear(int topBottom) {
  if (topBottom == -1) {
    for (int i = 0; i < 60; i++) {
      for (int j = 0; j < 5; j++) {
        leds.setPixelColor((i * 5) + j, 0x00, 0x00, 0x00);
      }
    }
  } else if (topBottom == 0) {
    for (int i = 0; i < 30; i++) {
      for (int j = 0; j < 5; j++) {
        leds.setPixelColor((i * 5) + j, 0x00, 0x00, 0x00);
      }
    }
  } else if (topBottom == 1) {
    for (int i = 30; i < 60; i++) {
      for (int j = 0; j < 5; j++) {
        leds.setPixelColor((i * 5) + j, 0x00, 0x00, 0x00);
      }
    }
  }
  // leds.show();
}

void scrollFont() {
  // pauseCore2 = 1;
  //  scroll font
  //  uint8_t font[] = // 'JumperlessFontmap', 500x5px
  //  {0x1f, 0x11, 0x1f, 0x00, 0x12, 0x1f, 0x10, 0x00, 0x1d, 0x15, 0x17, 0x00,
  //  0x11, 0x15, 0x1f, 0x00,};
  uint32_t color = 0x060205;
  int scrollSpeed = 120;
  int scrollPosition = 0;

  uint8_t columnMask[5] = // 'JumperlessFontmap', 500x5px
      {0b00000001, 0b00000010, 0b00000100, 0b00001000, 0b00010000};
  while (Jerial.available() == 0) {

    for (int i = 0; i < 60; i++) {
      for (int j = 0; j < 5; j++) {
        if ((font[(i + scrollPosition) % 500][0] & columnMask[j]) != 0) {
          // Jerial.print("1");
          leds.setPixelColor((i * 5) + j, color);
        } else {
          // Jerial.print("0");
          leds.setPixelColor((i * 5) + j, 0x00, 0x00, 0x00);
        }
      }
    }
    leds.show();
    delay(scrollSpeed);
    scrollPosition++;
    if (scrollPosition > 499) {
      scrollPosition = 0;
    }
  }
}

void printTextFromMenu(int print) {

  b.clear();
  int scroll = 0;
  char f[80] = {' '};
  int index = 0;
  // b.clear();
  unsigned long timeout = millis();
  while (Jerial.available() > 0) {
    if (index > 78) {
      break;
    }
    f[index] = Jerial.read();
    index++;
    if (millis() - timeout > 1000) {
      break;
    }
    // b.print(f);
    // delayMicroseconds(30);
    // leds.show();
  }

  if (index > 14) {
    scroll = 1;
  }
  f[index] = ' ';
  f[index + 1] = ' ';
  f[index + 2] = ' ';
  index += 3;
  uint32_t color = 0x100010;
  // Jerial.print(index);
  defconString[0] = f[0];
  defconString[1] = f[1];
  defconString[2] = f[2];
  defconString[3] = f[3];
  defconString[4] = f[4];
  defconString[5] = f[5];
  defconString[6] = f[6];
  defconString[7] = f[7];
  defconString[8] = f[8];
  defconString[9] = f[9];
  defconString[10] = f[10];
  defconString[11] = f[11];
  defconString[12] = f[12];
  defconString[13] = f[13];
  defconString[14] = f[14];
  defconString[15] = f[15];
  // defconString[16] = f[16];

  defconDisplay = 0;
  unsigned long timerScrollTimer = millis();
  // while (Jerial.available() != 0) {
  //   char trash = Jerial.read();
  //   }

  int speed = 200000;
  int cycleCount = 15;
  if (print == 1) {
    Jerial.println("\n\rPress any key to exit\n\r");

    if (scroll == 1) {
      Jerial.println("scroll wheel to change speed)\n\r");
    }
  }
  while (Jerial.available() == 0) {
    if (scroll == 1) {
      rotaryEncoderStuff();
      if (encoderDirectionState == UP) {
        if (speed > 10000) {
          speed = speed - 10000;
        } else {
          speed -= 500;
        }
        if (speed < 0) {
          speed = 0;
        }
        // Jerial.print("\r                          \rspeed = ");
        // Jerial.print(speed);

        // encoderDirectionState = NONE;
      } else if (encoderDirectionState == DOWN) {
        speed = speed + 10000;
        //           Jerial.print("\r                          \rspeed = ");
        // Jerial.print(speed);
        // encoderDirectionState = NONE;
        if (speed > 1000000) {
          speed = 1000000;
        }
      }

      if ((micros() - timerScrollTimer) > speed) {
        // Jerial.print("defNudge = ");
        // Jerial.println(defNudge);
        timerScrollTimer = micros();
        defNudge--;
        if (defNudge < -3) {
          defNudge = 0;
          cycleCount++;

          char temp = f[(cycleCount) % index];
          for (int i = 0; i < 15; i++) {
            defconString[i] = defconString[i + 1];
          }
          defconString[15] = temp;
          // b.print(f, color);
        }
        // delay(100);
        // leds.show();
        // showLEDsCore2 =2;
      }
    }
  }

  if (Jerial.peek() == '#') {
    uint8_t trash = Jerial.read();
    printTextFromMenu(0);
  }

  // while (Jerial.available() == 0) {
  //   // b.print(f, color);
  //   // delay(100);
  //   // leds.show();
  // }
}

void showArray(uint8_t *array, int size) {
  int noRails = 0;
  if (size <= 300) {
    noRails = 1;
    size = 300;
  }
  if (size <= 150) {
    noRails = 2; // only top row
    size = 150;
  }
  if (size > 400) {
    noRails = 0;
    size = 400;
  }

  if (noRails == 1) {

    for (int i = 0; i < size; i++) {

      leds.setPixelColor(array[screenMapNoRails[i]], array[i]);
    }
  } else if (noRails == 2) {
    for (int i = 0; i < size; i++) {
      leds.setPixelColor(array[screenMapNoRails[i]], array[i]);
    }

  } else if (noRails == 0) {
    for (int i = 0; i < size; i++) {
      leds.setPixelColor(array[screenMap[i]], array[i]);
    }
  }
  showLEDsCore2 = -3;
}

int getCursorPositionX() {
  Jerial.printf("\033[6n");
  Jerial.flush();
  String response = Jerial.readStringUntil(';');
  response.remove(0, 2);
  return response.toInt();
}

int getCursorPositionY() {
  Jerial.printf("\033[6n");
  Jerial.flush();
  String response = Jerial.readStringUntil(';');
  response.remove(0, 2);
  return response.toInt();
}
int cursorSaved = 0;

void saveCursorPosition(Stream *stream) {
  Jerial.print("\033[s");
  Jerial.flush();
  cursorSaved = 1;
}

void restoreCursorPosition(Stream *stream) {
  Jerial.print("\033[u");
  Jerial.flush();
  cursorSaved = 0;
}

void moveCursor(int posX, int posY, int absolute, Stream *stream, bool flush) {

  if (posX == -1 && posY == -1) {
    if (cursorSaved == 1) {
      restoreCursorPosition();
    } else {
      saveCursorPosition();
    }
    return;
  }

  if (posX == -1 && posY != -1) {
    Jerial.printf("\033[%dA", posY);
    if (flush == true) {
      Jerial.flush();
    }
    return;
  }

  if (posX != -1 && posY == -1) {
    Jerial.printf("\033[%dC", posX);
    if (flush == true) {
      Jerial.flush();
    }
    return;
  }

  if (absolute == 1) {
    Jerial.printf("\033[%d;%dH", posY, posX);

    if (flush == true) {
      Jerial.flush();
    }
  } else {
    Jerial.printf("\033[%dA", posY);
    Jerial.printf("\033[%dC", posX);
    if (flush == true) {
      Jerial.flush();
    }
  }
  //   Jerial.printf("\033[6n");
  // Jerial.flush();
  // String response = Jerial.readStringUntil('\n');
  // response.remove(0, 2);

  // Jerial.println(response);
  //   Jerial.flush();
}

/*


▀	▁	▂	▃	▄	▅	▆	▇	█	▉
▊	▋	▌	▍	▎	▏ ▐	░	▒	▓	▔
▕	▖	▗	▘	▙	▚	▛	▜	▝	▞
▟

─	━	│	┃	┄	┅	┆	┇	┈	┉
┊	┋	┌	┍	┎	┏ ┐	┑	┒	┓	└
┕	┖	┗	┘	┙	┚	┛	├	┝	┞
┟ ┠	┡	┢	┣	┤	┥	┦	┧	┨	┩
┪	┫	┬	┭	┮	┯ ┰	┱	┲	┳	┴
┵	┶	┷	┸	┹	┺	┻	┼	┽	┾
┿ ╀	╁	╂	╃	╄	╅	╆	╇	╈	╉
╊	╋	╌	╍	╎	╏ ─	│	╒	╓	╭
╕	╖	╮	╘	╙	╰	╛	╜	╯	╞
╟ ╠	╡	╢	╣	╤	╥	╦	╧	╨	╩
╪	╫	╬	╭	╮	╯ ╰	╱	╲	╳	╴
╵	╶	╷	╸	╹	╺	╻	╼	╽	╾
╿


    NUL     ☺︎      ☻      ♥︎      ♦︎      ♣︎      ♠︎
     •      ◘      ○      ◙      ♂︎      ♀︎      ♪
     ♫      ☼      ►      ◄      ↕︎      ‼︎      ¶
     §      ▬      ↨      ↑      ↓      →      ←
     ∟      ↔︎      ▲      ▼
    á      í      ó      ú      ñ      Ñ      ª
    º      ¿      ⌐      ¬      ½      ¼      ¡
    «      »      ░      ▒      ▓      │      ┤
    ╡      ╢      ╖      ╕      ╣      │      ╮
╯      ╜      ╛      ┐
    └      ┴      ┬      ├      ─      ┼      ╞
    ╟      ╰      ╭      ╩      ╦      ╠      ─
    ╬      ╧      ╨      ╤      ╥      ╙      ╘
    ╒      ╓      ╫      ╪      ┘      ┌      █
    ▄      ▌      ▐      ▀      α      ß      Γ
    π      Σ      σ      µ      τ      Φ      Θ
    Ω      δ      ∞      φ      ε      ∩      ≡
    ±      ≥      ≤      ⌠      ⌡      ÷      ≈
    °      ∙      ·      √      ⁿ      ²      ■⌂

  // Simple logo section - just use basic text for now
  snprintf(screenLines[currentLine++], LINE_WIDTH, "           ADC  ");
  snprintf(screenLines[currentLine++], LINE_WIDTH, "     ▗▄▖        ");
  snprintf(screenLines[currentLine++], LINE_WIDTH, "    ▐█▔▚▋   DAC ");
  snprintf(screenLines[currentLine++], LINE_WIDTH, "    ▐▚▁█▋       ");
  snprintf(screenLines[currentLine++], LINE_WIDTH, "     ▝▀▘   GPIO ");


  ▗▄▖
 ▐█▔▚▋
 ▐▚▁█▋
  ▝▀▘

*/
struct logoLedAssoc logoLedAssociations[20] = {
    {LOGO_LED_START, "        ", 0, 0},   {ADC_LED_0, "AD", 1, 197, 0, ADC_0},
    {ADC_LED_0, "C ", 2, 199, 1, ADC_1},  {LOGO_LED_START, " ▗▄▖ \n\r", 0},
    {LOGO_LED_START + 1, "▐█", 0},        {LOGO_LED_START + 6, "▔", 0},
    {LOGO_LED_START + 2, "▚", 0},         {LOGO_LED_START + 2, "▋   ", 0},
    {DAC_LED_1, "DA", 1, 191, 2, DAC_0},  {DAC_LED_1, "C ", 2, 226, 3, DAC_1},
    {LOGO_LED_START + 3, "▐▚", 0},        {LOGO_LED_START + 6, "▁", 0},
    {LOGO_LED_START + 5, "█▋\n\r", 0},    {LOGO_LED_START + 4, " ▝▀▘   ", 0},
    {GPIO_LED_0, "GP", 1, 39, 4, GPIO_0}, {GPIO_LED_0, "IO", 2, 87, 5, GPIO_1},

};

volatile bool dumpingToSerial = false;
volatile bool logoLedAccess = false;

unsigned long lastLEDsDumpTime = 0;
unsigned long clearLEDsInterval = 1000;
unsigned long lastDumpAttemptTime = 0;
unsigned long minDumpInterval = 10; // Minimum 50ms between dump attempts

// Connection state tracking for clear screen on connect
bool serial1Connected = false;
bool serial2Connected = false;
unsigned long serial1ConnectTime = 0;
unsigned long serial2ConnectTime = 0;
int serial1ClearSent = 0;
int serial2ClearSent = 0;

// #define CSI "\033[

void dumpLEDs(int posX, int posY, int pixelsOrRows, int header, int rgbOrRaw,
              int logo, Stream *stream) {

  // return;
  bool mainSerial = false;
  int xOffset = 40;
  // Rate limiting - prevent excessive calls
  if (millis() - lastDumpAttemptTime < minDumpInterval) {
    return;
  }
  lastDumpAttemptTime = millis();

  if (dumpingToSerial == false) {
    dumpingToSerial = true;
  } else {
    return;
  }

  unsigned long functionStartTime = millis();
  const unsigned long FUNCTION_TIMEOUT_MS = 60;

  if (jumperlessConfig.serial_2.function == 5 ||
      jumperlessConfig.serial_2.function == 6) {
    stream = &USBSer2;
    bool currentlyConnected =
        USBSer2.dtr() && (USBSer2.availableForWrite() >= 50);

    if (!currentlyConnected) {
      serial2Connected = false;
      serial2ClearSent = 0;
      dumpingToSerial = false;
      return;
    }

    // Record connection time when first connecting
    if (!serial2Connected && currentlyConnected) {
      serial2Connected = true;
      serial2ConnectTime = millis();
      serial2ClearSent = 0;
      Jerial.println("Serial 2 DTR connected");
    }

    // Send clear screen 1 second after connection is established
    if (serial2Connected && serial2ClearSent < 2 &&
        (millis() - serial2ConnectTime >= 100)) {
      if (!safePrint(stream, "\033[2J\033[?25l\033[0;0H", 10)) {
        dumpingToSerial = false;
        return;
      }
      serial2ClearSent++;
    }
  } else if (jumperlessConfig.serial_1.function == 5 ||
             jumperlessConfig.serial_1.function == 6) {
    stream = &USBSer1;
    bool currentlyConnected =
        USBSer1.dtr() && (USBSer1.availableForWrite() >= 50);

    if (!currentlyConnected) {
      serial1Connected = false;
      serial1ClearSent = 0;
      dumpingToSerial = false;
      return;
    }

    // Record connection time when first connecting
    if (!serial1Connected && currentlyConnected) {
      serial1Connected = true;
      serial1ConnectTime = millis();
      serial1ClearSent = 0;
      Jerial.println("Serial 1 DTR connected");
    }

    // Send clear screen 1 second after connection is established
    if (serial1Connected && serial1ClearSent < 2 &&
        (millis() - serial1ConnectTime >= 100)) {
      if (!safePrint(stream, "\033[2J\033[?25l\033[0;0H", 10)) {
        dumpingToSerial = false;
        return;
      }
      serial1ClearSent++;
    }
  } else {
    // Check if windowing system is active
    extern struct config jumperlessConfig;
    bool useWindowing = jumperlessConfig.windowing.enabled && 
                        jumperlessConfig.windowing.show_leds;
    
    if (!useWindowing) {
      // Legacy positioning for non-windowing mode
      saveCursorPosition(stream);
    }
    // Windowing mode: WindowManager handles all positioning
    mainSerial = true;
  }

  if (logoLedAccess == true) {
    dumpingToSerial = false;
    return;
  }
  logoLedAccess = true;

// Build screen line by line - much simpler approach
#define MAX_LINES 30
#define LINE_WIDTH 700 // Much wider to accommodate ANSI escape sequences
  static char screenLines[MAX_LINES][LINE_WIDTH];
  int currentLine = 0;

  // Clear all lines
  for (int i = 0; i < MAX_LINES; i++) {
    memset(screenLines[i], 0, LINE_WIDTH);
  }

  int logoColor0 = colorToVT100(leds.getPixelColor(LOGO_LED_START), 256);
  int logoColor1 = colorToVT100(leds.getPixelColor(LOGO_LED_START + 1), 256);
  int logoColor2 = colorToVT100(leds.getPixelColor(LOGO_LED_START + 2), 256);
  int logoColor3 = colorToVT100(leds.getPixelColor(LOGO_LED_START + 3), 256);
  int logoColor4 = colorToVT100(leds.getPixelColor(LOGO_LED_START + 4), 256);
  int logoColor5 = colorToVT100(leds.getPixelColor(LOGO_LED_START + 5), 256);
  int logoColor6 = colorToVT100(leds.getPixelColor(LOGO_LED_START + 6), 256);

  // Get logo colors with override handling for all LED pairs
  uint32_t adc0Color = leds.getPixelColor(ADC_LED_0) | 0x00000f;
  uint32_t adc1Color = leds.getPixelColor(ADC_LED_1) | 0x0f0000;
  uint32_t dac0Color = leds.getPixelColor(DAC_LED_0) | 0x1f0300;
  dac0Color &= 0xff20ff;
  uint32_t dac1Color = leds.getPixelColor(DAC_LED_1) | 0x050000;
  uint32_t gpio0Color = leds.getPixelColor(GPIO_LED_0) | 0x054f00;
  uint32_t gpio1Color = leds.getPixelColor(GPIO_LED_1) & 0xff40ff;

  // Check for logo overrides
  if (logoOverriden == true) {
    logoLedAccess = false;
    uint32_t adc0Override = getLogoOverride(ADC_0);
    uint32_t adc1Override = getLogoOverride(ADC_1);
    uint32_t dac0Override = getLogoOverride(DAC_0);
    uint32_t dac1Override = getLogoOverride(DAC_1);
    uint32_t gpio0Override = getLogoOverride(GPIO_0);
    uint32_t gpio1Override = getLogoOverride(GPIO_1);

    if (adc0Override != -1) {
      adc0Color = 0xFFbFFF; // Use white for -2 override
    }
    if (adc1Override != -1) {
      adc1Color = 0xFFbFFF;
    }

    if (dac0Override != -1) {
      dac0Color = 0xFFFFbF;
    }

    if (dac1Override != -1) {
      dac1Color = 0xFFFFbF;
    }

    if (gpio0Override != -1) {
      gpio0Color = 0xBFFFFF;
    }

    if (gpio1Override != -1) {
      gpio1Color = 0xBFFFFF;
    }
  }

  // Convert to terminal colors
  int adc0TermColor = colorToVT100(adc0Color, 256);
  int adc1TermColor = colorToVT100(adc1Color, 256);
  int dac0TermColor = colorToVT100(dac0Color, 256);
  int dac1TermColor = colorToVT100(dac1Color, 256);
  int gpio0TermColor = colorToVT100(gpio0Color, 256);
  int gpio1TermColor = colorToVT100(gpio1Color, 256);

  if (adc0Color == 0x000000)
    adc0TermColor = 0;
  if (adc1Color == 0x000000)
    adc1TermColor = 0;
  if (dac0Color == 0x000000)
    dac0TermColor = 0;
  if (dac1Color == 0x000000)
    dac1TermColor = 0;
  if (gpio0Color == 0x000000)
    gpio0TermColor = 0;
  if (gpio1Color == 0x000000)
    gpio1TermColor = 0;

  // Header section with integrated logo
  snprintf(screenLines[currentLine++], LINE_WIDTH,
           "       "
           "\033[38;5;%dm▐\033[38;5;0m\033[48;5;%dmAD\033[38;5;0m\033[48;5;%dmC\033[0m\033[38;5;"
           "%dm▌\033[0m                                                   ",
           adc0TermColor, adc0TermColor, adc1TermColor, adc1TermColor);//!ADC

  snprintf(screenLines[currentLine++], LINE_WIDTH,
           "   \033[38;5;%dm▗▄▖\033[0m         "
           "╭─────────────────────────────────────────────╮ ",
           logoColor0);

  // Build DAC line with first header row on same line
  char dacHeaderLine[LINE_WIDTH];
  int pos = 0;
  pos += snprintf(dacHeaderLine + pos, LINE_WIDTH - pos,
                  "  \033[38;5;%dm▐█\033[38;5;%dm▔\033[38;5;%dm▚▋\033[0m ",
                  logoColor1, logoColor0, logoColor5);
  pos += snprintf(dacHeaderLine + pos, LINE_WIDTH - pos,
                  "\033[38;5;%dm▐\033[38;5;0m\033[48;5;%dmDA\033[38;5;0m\033[48;5;%dmC\033[0m\033[38;5;"
                  "%dm▌\033[0m  │",
                  dac0TermColor, dac0TermColor, dac1TermColor, dac1TermColor);//!DAC

  for (int i = 0; i < 15; i++) {
    int headerIndex = headerMapPrintOrder[i];
    uint32_t color = leds.getPixelColor(headerMap[headerIndex].pixel);
    int termColor = colorToVT100(color, 256);

    if (color == 0x000000) {
      termColor = 0;
    }

    pos += snprintf(dacHeaderLine + pos, LINE_WIDTH - pos,
                    "\033[48;5;%dm%3s\033[0m", termColor,
                    headerMap[headerIndex].name);
    // if (i < 14) { // Add space between pins except after the last one
    // pos += snprintf(dacHeaderLine + pos, LINE_WIDTH - pos, " ");
    //}
  }
  pos += snprintf(dacHeaderLine + pos, LINE_WIDTH - pos, "│");
  snprintf(screenLines[currentLine++], LINE_WIDTH, "%s", dacHeaderLine);

  // Middle spacer with logo part
  snprintf(screenLines[currentLine++], LINE_WIDTH,
           "  \033[38;5;%dm▐▚\033[38;5;%dm▁\033[38;5;%dm█▋\033[0m        │     "
           "                                        │ ",
           logoColor2, logoColor3, logoColor4);

  // Build GPIO line with second header row on same line
  char gpioHeaderLine[LINE_WIDTH];
  pos = 0;
  pos += snprintf(gpioHeaderLine + pos, LINE_WIDTH - pos,
                  "   \033[38;5;%dm▝▀▘  "
                  "\033[38;5;%dm▐\033[38;5;0m\033[48;5;%dmGP\033[38;5;0m\033[48;5;%dmIO\033[0m\033[38;"
                  "5;%dm▌\033[0m │",
                  logoColor3, gpio0TermColor, gpio0TermColor, gpio1TermColor,
                  gpio1TermColor);//!GPIO

  for (int i = 15; i < 30; i++) {
    int headerIndex = headerMapPrintOrder[i];
    uint32_t color = leds.getPixelColor(headerMap[headerIndex].pixel);
    int termColor = colorToVT100(color, 256);

    if (color == 0x000000) {
      termColor = 0;
    }

    pos += snprintf(gpioHeaderLine + pos, LINE_WIDTH - pos,
                    "\033[48;5;%dm%3s\033[0m", termColor,
                    headerMap[headerIndex].name);
    // if (i < 29) { // Add space between pins except after the last one
    //   pos += snprintf(gpioHeaderLine + pos, LINE_WIDTH - pos, " ");
    // }
  }
  pos += snprintf(gpioHeaderLine + pos, LINE_WIDTH - pos, "│               ");
  snprintf(screenLines[currentLine++], LINE_WIDTH, "%s", gpioHeaderLine);

  // Close header - aligned with the box
  snprintf(screenLines[currentLine++], LINE_WIDTH,
           "               ╰─────────────────────────────────────────────╯          ");

  // Empty line
  snprintf(screenLines[currentLine++], LINE_WIDTH, "                                                                ");

  // Main LED grid
  snprintf(screenLines[currentLine++], LINE_WIDTH,
           "╭─────────────────────────────────────────────────────────────╮             ");

  // Process LED grid rows
  for (int row = 0; row < 14 && currentLine < MAX_LINES - 1; row++) {
    bool rail = (row == 0 || row == 1 || row == 12 || row == 13);

    char ledLine[LINE_WIDTH];
    int pos = 0;
    pos += snprintf(ledLine + pos, LINE_WIDTH - pos, "│ ");

    // Process columns for this row
    for (int col = 0; col < 30; col++) {
      if (rail && (col + 1) % 6 == 0) {
        pos += snprintf(ledLine + pos, LINE_WIDTH - pos, "  ");
        continue;
      }

      int mapIndex = row * 30 + col;
      int ledIndex = screenMap[mapIndex];
      uint32_t color = leds.getPixelColor(ledIndex);
      int termColor = colorToVT100(color, 256);

      if (color == 0x000000) {
        termColor = 0;
      }

      const char *pattern = rail ? "▐█" : "█▏";
      pos += snprintf(ledLine + pos, LINE_WIDTH - pos, "\033[38;5;%dm%s\033[0m",
                      termColor, pattern);
    }

    pos += snprintf(ledLine + pos, LINE_WIDTH - pos, "│          ");
    snprintf(screenLines[currentLine++], LINE_WIDTH, "%s", ledLine);

    // Add special spacing rows
    if (row == 1) {
      snprintf(
          screenLines[currentLine++], LINE_WIDTH,
          "│ ₁       ₅         ₁₀        ₁₅        ₂₀        ₂₅        ₃₀│         ");
    } else if (row == 6) {
      snprintf(screenLines[currentLine++], LINE_WIDTH,
               "\033[0m│ \033[38;5;236m       J    U    M    P    E    R    L  "
               "  E    S    S       \033[0m│        ");
    } else if (row == 11) {
      snprintf(
          screenLines[currentLine++], LINE_WIDTH,
          "│ ³¹      ³⁵        ⁴⁰        ⁴⁵        ⁵⁰        ⁵⁵        ⁶⁰│          ");
    }
  }

  // Close LED grid
  snprintf(screenLines[currentLine++], LINE_WIDTH,
           "╰─────────────────────────────────────────────────────────────╯");

  logoLedAccess = false;

      // Start with cursor reset
    if (mainSerial == true) {
      extern struct config jumperlessConfig;
      bool useWindowing = jumperlessConfig.windowing.enabled && 
                          jumperlessConfig.windowing.show_leds;
      
      if (!useWindowing) {
        // Legacy cursor management for non-windowing mode
        int clearAbove = 6;
        Jerial.printf("\033[%dA", 30);
        Jerial.printf("\033[%dA", clearAbove);
        for (int i = 0; i < clearAbove; i++) {
          Jerial.printf("\033[%dC\033[0K\033[1B\033[0G", xOffset);
        }
        Jerial.print("\033[0G");
      }
      // Windowing mode: skip manual positioning

    } else {
      snprintf(screenLines[currentLine++], LINE_WIDTH, "\033[0;0H\033[0m");
    }


  // Send all lines
  for (int i = 0; i < currentLine; i++) {
    // Quick timeout check
    if (millis() - functionStartTime > FUNCTION_TIMEOUT_MS) {
      break;
    }

    // Wait for buffer space if needed
    int lineLen = strlen(screenLines[i]);
    if (Jerial.availableForWrite() < lineLen + 3) { // +3 for \r\n\0
      unsigned long waitStart = millis();
      while (Jerial.availableForWrite() < lineLen + 3 &&
             (millis() - waitStart) < 10) {
        delayMicroseconds(10);
      }
    }


    // Send line with proper line ending
    if (mainSerial == true) {
      extern struct config jumperlessConfig;
      bool useWindowing = jumperlessConfig.windowing.enabled && 
                          jumperlessConfig.windowing.show_leds;
      
      if (!useWindowing) {
        Jerial.printf("\033[%dC", xOffset);
      }
    }

    Jerial.print(screenLines[i]);
    Jerial.print("\r\n");
  }

  if (mainSerial == true) {
    extern struct config jumperlessConfig;
    bool useWindowing = jumperlessConfig.windowing.enabled && 
                        jumperlessConfig.windowing.show_leds;
    
    if (!useWindowing) {
      Jerial.printf("\033[%dB", 4);
    }
  }





  // Final flush
  safeFlush(stream, 10);
  dumpingToSerial = false;
}


int attractMode(void) {

  if (encoderDirectionState == DOWN) {
    // attractMode = 0;
    defconDisplay = -1;
    netSlot++;
    if (netSlot >= NUM_SLOTS) {
      netSlot = -1;
      defconDisplay = 0;
    }
    // Jerial.print("netSlot = ");
    // Jerial.println(netSlot);
    slotChanged = 1;
    showLEDsCore2 = -1;
    encoderDirectionState = NONE;
    return 1;
    // goto menu;
  } else if (encoderDirectionState == UP) {
    // attractMode = 0;
    defconDisplay = -1;
    netSlot--;
    if (netSlot <= -1) {
      netSlot = NUM_SLOTS;
      defconDisplay = 0;
    }
    // Jerial.print("netSlot = ");
    // Jerial.println(netSlot);
    slotChanged = 1;
    showLEDsCore2 = -1;
    encoderDirectionState = NONE;
    return 1;
    // goto menu;
  }
  return 0;
}

void defcon(int start, int spread, int color, int nudge) {
  spread = 13;
  nudge = defNudge;

  int scaleFactor = -70;

  b.clear();
  b.print(
      defconString[0],
      scaleBrightness(logoColorsAll[color][(start) % (LOGO_COLOR_LENGTH - 1)],
                      scaleFactor),
      (uint32_t)0xffffff, 0, 0, nudge);

  b.print(defconString[1],
          scaleBrightness(
              logoColorsAll[color][(start + spread) % (LOGO_COLOR_LENGTH - 1)],
              scaleFactor),
          (uint32_t)0xffffff, 1, 0, nudge);

  b.print(
      defconString[2],
      scaleBrightness(
          logoColorsAll[color][(start + spread * 2) % (LOGO_COLOR_LENGTH - 1)],
          scaleFactor),
      (uint32_t)0xffffff, 2, 0, nudge);
  b.print(
      defconString[3],
      scaleBrightness(
          logoColorsAll[color][(start + spread * 3) % (LOGO_COLOR_LENGTH - 1)],
          scaleFactor),
      (uint32_t)0xffffff, 3, 0, nudge);
  b.print(
      defconString[4],
      scaleBrightness(
          logoColorsAll[color][(start + spread * 4) % (LOGO_COLOR_LENGTH - 1)],
          scaleFactor),
      (uint32_t)0xffffff, 4, 0, nudge);
  b.print(
      defconString[5],
      scaleBrightness(
          logoColorsAll[color][(start + spread * 5) % (LOGO_COLOR_LENGTH - 1)],
          scaleFactor),
      (uint32_t)0xffffff, 5, 0, nudge);
  b.print(
      defconString[6],
      scaleBrightness(
          logoColorsAll[color][(start + spread * 5) % (LOGO_COLOR_LENGTH - 1)],
          scaleFactor),
      (uint32_t)0xffffff, 6, 0, nudge);
  b.print(
      defconString[7],
      scaleBrightness(
          logoColorsAll[color][(start + spread * 6) % (LOGO_COLOR_LENGTH - 1)],
          scaleFactor),
      (uint32_t)0xffffff, 0, 1, nudge);
  b.print(
      defconString[8],
      scaleBrightness(
          logoColorsAll[color][(start + spread * 7) % (LOGO_COLOR_LENGTH - 1)],
          scaleFactor),
      (uint32_t)0xffffff, 1, 1, nudge);
  b.print(
      defconString[9],
      scaleBrightness(
          logoColorsAll[color][(start + spread * 8) % (LOGO_COLOR_LENGTH - 1)],
          scaleFactor),
      (uint32_t)0xffffff, 2, 1, nudge);
  b.print(
      defconString[10],
      scaleBrightness(
          logoColorsAll[color][(start + spread * 9) % (LOGO_COLOR_LENGTH - 1)],
          scaleFactor),
      (uint32_t)0xffffff, 3, 1, nudge);
  b.print(
      defconString[11],
      scaleBrightness(
          logoColorsAll[color][(start + spread * 10) % (LOGO_COLOR_LENGTH - 1)],
          scaleFactor),
      (uint32_t)0xffffff, 4, 1, nudge);
  b.print(
      defconString[12],
      scaleBrightness(
          logoColorsAll[color][(start + spread * 11) % (LOGO_COLOR_LENGTH - 1)],
          scaleFactor),
      (uint32_t)0xffffff, 5, 1, nudge);
  b.print(
      defconString[13],
      scaleBrightness(
          logoColorsAll[color][(start + spread * 12) % (LOGO_COLOR_LENGTH - 1)],
          scaleFactor),
      (uint32_t)0xffffff, 6, 1, nudge);
  // railsToPixelMap[0][20] = 0;
  //  leds.setPixelColor(railsToPixelMap[0][20], 0x004f9f);
  //  leds.setPixelColor(railsToPixelMap[0][21], 0x3f000f);
  //  leds.setPixelColor(railsToPixelMap[0][22], 0x3f000f);
  //  leds.setPixelColor(railsToPixelMap[0][23], 0x3f000f);
  //  leds.setPixelColor(railsToPixelMap[0][24], 0x7f000f);

  // leds.setPixelColor(railsToPixelMap[1][20], 0x007aaf);
  // leds.setPixelColor(railsToPixelMap[1][21], 0x00009f);
  // leds.setPixelColor(railsToPixelMap[1][22], 0x00003f);
  // leds.setPixelColor(railsToPixelMap[1][23], 0x00005f);
  // leds.setPixelColor(railsToPixelMap[1][24], 0x00005f);

  int topScale = 70;

  int spreadnudge = 3;

  leds.setPixelColor(
      bbPixelToNodesMapV5[0][1],
      scaleBrightness(
          logoColorsAll[0][(start - spreadnudge) % (LOGO_COLOR_LENGTH - 1)],
          topScale));
  leds.setPixelColor(
      bbPixelToNodesMapV5[2][1],
      scaleBrightness(logoColorsAll[0][(start + spread - spreadnudge) %
                                       (LOGO_COLOR_LENGTH - 1)],
                      topScale));

  leds.setPixelColor(
      bbPixelToNodesMapV5[4][1],
      scaleBrightness(logoColorsAll[0][(start + spread * 2 - spreadnudge) %
                                       (LOGO_COLOR_LENGTH - 1)],
                      topScale));
  leds.setPixelColor(
      bbPixelToNodesMapV5[6][1],
      scaleBrightness(logoColorsAll[0][(start + spread * 3 - spreadnudge) %
                                       (LOGO_COLOR_LENGTH - 1)],
                      topScale));
  leds.setPixelColor(
      bbPixelToNodesMapV5[8][1],
      scaleBrightness(logoColorsAll[0][(start + spread * 4 - spreadnudge) %
                                       (LOGO_COLOR_LENGTH - 1)],
                      topScale));
  leds.setPixelColor(
      bbPixelToNodesMapV5[10][1],
      scaleBrightness(logoColorsAll[0][(start + spread * 5 - spreadnudge) %
                                       (LOGO_COLOR_LENGTH - 1)],
                      topScale));
  leds.setPixelColor(
      bbPixelToNodesMapV5[12][1],
      scaleBrightness(logoColorsAll[0][(start + spread * 6 - spreadnudge) %
                                       (LOGO_COLOR_LENGTH - 1)],
                      topScale));
  leds.setPixelColor(
      bbPixelToNodesMapV5[14][1],
      scaleBrightness(logoColorsAll[0][(start + spread * 7 - spreadnudge) %
                                       (LOGO_COLOR_LENGTH - 1)],
                      topScale));
  leds.setPixelColor(
      bbPixelToNodesMapV5[16][1],
      scaleBrightness(logoColorsAll[0][(start + spread * 8 - spreadnudge) %
                                       (LOGO_COLOR_LENGTH - 1)],
                      topScale));
  leds.setPixelColor(
      bbPixelToNodesMapV5[18][1],
      scaleBrightness(logoColorsAll[0][(start + spread * 9 - spreadnudge) %
                                       (LOGO_COLOR_LENGTH - 1)],
                      topScale));
  leds.setPixelColor(
      bbPixelToNodesMapV5[20][1],
      scaleBrightness(logoColorsAll[0][(start + spread * 10 - spreadnudge) %
                                       (LOGO_COLOR_LENGTH - 1)],
                      topScale));
  leds.setPixelColor(
      bbPixelToNodesMapV5[22][1],
      scaleBrightness(logoColorsAll[0][(start + spread * 11 - spreadnudge) %
                                       (LOGO_COLOR_LENGTH - 1)],
                      topScale));
  leds.setPixelColor(
      bbPixelToNodesMapV5[24][1],
      scaleBrightness(logoColorsAll[0][(start + spread * 12 - spreadnudge) %
                                       (LOGO_COLOR_LENGTH - 1)],
                      topScale));

  leds.setPixelColor(
      bbPixelToNodesMapV5[1][1],
      scaleBrightness(logoColorsAll[0][(start) % (LOGO_COLOR_LENGTH - 1)],
                      topScale));

  leds.setPixelColor(
      bbPixelToNodesMapV5[3][1],
      scaleBrightness(
          logoColorsAll[0][(start + spread) % (LOGO_COLOR_LENGTH - 1)],
          topScale));

  leds.setPixelColor(
      bbPixelToNodesMapV5[5][1],
      scaleBrightness(
          logoColorsAll[0][(start + spread * 2) % (LOGO_COLOR_LENGTH - 1)],
          topScale));
  leds.setPixelColor(
      bbPixelToNodesMapV5[7][1],
      scaleBrightness(
          logoColorsAll[0][(start + spread * 3) % (LOGO_COLOR_LENGTH - 1)],
          topScale));
  leds.setPixelColor(
      bbPixelToNodesMapV5[9][1],
      scaleBrightness(
          logoColorsAll[0][(start + spread * 4) % (LOGO_COLOR_LENGTH - 1)],
          topScale));
  leds.setPixelColor(
      bbPixelToNodesMapV5[11][1],
      scaleBrightness(
          logoColorsAll[0][(start + spread * 5) % (LOGO_COLOR_LENGTH - 1)],
          topScale));
  leds.setPixelColor(
      bbPixelToNodesMapV5[13][1],
      scaleBrightness(
          logoColorsAll[0][(start + spread * 6) % (LOGO_COLOR_LENGTH - 1)],
          topScale));
  leds.setPixelColor(
      bbPixelToNodesMapV5[15][1],
      scaleBrightness(
          logoColorsAll[0][(start + spread * 7) % (LOGO_COLOR_LENGTH - 1)],
          topScale));
  leds.setPixelColor(
      bbPixelToNodesMapV5[17][1],
      scaleBrightness(
          logoColorsAll[0][(start + spread * 8) % (LOGO_COLOR_LENGTH - 1)],
          topScale));
  leds.setPixelColor(
      bbPixelToNodesMapV5[19][1],
      scaleBrightness(
          logoColorsAll[0][(start + spread * 9) % (LOGO_COLOR_LENGTH - 1)],
          topScale));

  leds.setPixelColor(
      bbPixelToNodesMapV5[21][1],
      scaleBrightness(
          logoColorsAll[0][(start + spread * 10) % (LOGO_COLOR_LENGTH - 1)],
          topScale));
  leds.setPixelColor(
      bbPixelToNodesMapV5[23][1],
      scaleBrightness(
          logoColorsAll[0][(start + spread * 11) % (LOGO_COLOR_LENGTH - 1)],
          topScale));

  // b.print('M', logoColors[(start + spread * 2) % (LOGO_COLOR_LENGTH - 1)],
  // 2,0); b.print('P', logoColors[(start + spread * 3) % (LOGO_COLOR_LENGTH -
  // 1)], 3,0); b.print('E', logoColors[(start + spread * 4) %
  // (LOGO_COLOR_LENGTH - 1)], 4,0); b.print('R', logoColors[(start + spread *
  // 5) % (LOGO_COLOR_LENGTH - 1)], 5,0); b.print('L', logoColors[(start +
  // spread * 6) % (LOGO_COLOR_LENGTH - 1)], 6,0); b.print('E',
  // logoColors[(start + spread * 7) % (LOGO_COLOR_LENGTH - 1)], 8,1);
  // b.print('S', logoColors[(start + spread * 8) % (LOGO_COLOR_LENGTH - 1)],
  // 9,1); b.print('S', logoColors[(start + spread * 9) % (LOGO_COLOR_LENGTH -
  // 1)], 10,1); b.print(' ', logoColors[(start + spread * 10) %
  // (LOGO_COLOR_LENGTH - 1)], 11,1); b.print('V', logoColors[(start + spread *
  // 11) % (LOGO_COLOR_LENGTH - 1)], 12,1); b.print('5', logoColors[(start +
  // spread * 12) % (LOGO_COLOR_LENGTH - 1)], 13,1);
}

void doomIntro(void) {

  // for (int f = 0; f <= 20; f++)
  //   {
  //    for (int r = 0; r < 30; r++)
  //    {

  //       for (int i = 0; i < 5 ; i++)
  //       {
  //         leds.setPixelColor((r * 5) + i,
  //         scaleBrightness(doomIntroFrames[r][5+i], -63));
  //       }

  //       for (int i = 0; i < 5  ; i++)
  //       {
  //          leds.setPixelColor((r*2 * 5) + i,
  //          scaleBrightness(doomIntroFrames[r][i], -63));
  //       }

  //    }
  //    leds.show();

  for (int f = 0; f < 60; f++) {
    for (int r = 0; r < 5; r++) {
      leds.setPixelColor((f * 5) + r, scaleBrightness(0x550500, -93));
    }
  }
  leds.show();

  delay(100);

  for (int f = 0; f < 40; f++) {
    for (int i = 0; i < 60; i++) {
      // if (random(0, 100) > 10)
      // {
      //   leds.setPixelColor((i*5)+random(0,5), 0x000000);
      // }
      // if (random(0, 100) > 20)
      // {
      // leds.setPixelColor((i*5)+random(0,5), 0x000000);
      // }
      // if (random(0, 100) > 30)
      // {
      //   leds.setPixelColor((i*5)+random(0,5), 0x000000);
      // }
      // if (random(0, 100) > 40)
      // {
      //   leds.setPixelColor((i*5)+random(0,5), 0x000000);
      //}
      // if (random(0, 100) > 50)
      // {
      //  leds.setPixelColor((i*5)+random(0,5), 0x000000);
      // }
      if (i < 30) {
        if (random(0, 100) > 70) {
          leds.setPixelColor((i * 5) + random(0, 5), 0x000000);
        }
      }
      if (random(0, 100) > 80) {
        leds.setPixelColor((i * 5) + random(0, 5), 0x000000);
      }
      if (random(0, 100) < 20) {
        leds.setPixelColor((i * 5) + random(0, 3), 0x000000);
      }
      if (random(0, 100) < 10) {
        leds.setPixelColor((i * 5) + random(0, 2), 0x000000);
      }

      if (random(0, 100) > 85) {
        leds.setPixelColor((i * 5) + random(0, 5), 0x000000);
      }

      if (random(0, 100) > 90) {
        leds.setPixelColor((i * 5) + random(0, 5), 0x000000);
        leds.setPixelColor((i * 5) + random(0, 3), 0x000000);
      }
      if (random(0, 100) > 95) {
        leds.setPixelColor((i * 5) + random(0, 5), 0x000000);
        leds.setPixelColor((i * 5) + random(0, 2), 0x000000);
      }
    }
    leds.show();
    delay(48);
  }

  //    delay(40);

  //    }
}

void playDoom(void) {
  // core1busy = 1;
#if INCLUDE_DOOM == 1
  core2busy = 1;
  int pixMap[10] = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
  // doomIntro();
  doomIntro();
  for (int l = 0; l < 1; l++) {

    for (int f = 0; f <= 39; f++) {
      for (int r = 0; r < 60; r++) {

        if (r < 30) {

          for (int i = 0; i < 5; i++) {
            leds.setPixelColor(
                (r * 5) + i,
                scaleBrightness(doomFrames[f][(r * 10) + 5 + (4 - i)], -93));
          }
        } else {
          for (int i = 0; i < 5; i++) {
            leds.setPixelColor(
                (r * 5) + i,
                scaleBrightness(doomFrames[f][((r - 30) * 10) + (4 - i)], -93));
          }
        }
      }
      leds.show();
      // if (l % 3 == 0) {

      // leds.clear();
      // leds.show();
      // delayMicroseconds(l*200);
      // }
      //       if (l % 3 == 1) {

      // leds.clear();
      // leds.show();
      // //delayMicroseconds(1);
      // }
      delay(150);
    }
  }
#endif // INCLUDE_DOOM
  // core1busy = 0;
  //core2busy = 0;
}

uint8_t rainbowr[30] = {30, 29, 26, 23, 20, 17, 14, 11, 8,  5,
                        2,  0,  0,  0,  0,  0,  0,  0,  0,  2,
                        5,  8,  11, 14, 17, 20, 23, 26, 29, 30};
uint8_t rainbowg[30] = {0,  1,  2,  5,  11, 14, 17, 20, 23, 26,
                        29, 30, 29, 26, 23, 20, 17, 14, 11, 8,
                        5,  2,  0,  0,  0,  0,  0,  0,  0,  0};
uint8_t rainbowb[30] = {3,  2,  1,  0,  0,  1,  2,  3,  4,  6,
                        8,  11, 14, 17, 20, 23, 26, 29, 30, 29,
                        26, 23, 20, 17, 14, 11, 9,  7,  5,  4};

int cycleCount = 0;
int brightnessSetLogo[45] = {
    -100, -99, -97, -95, -90, -85, -78, -72, -62, -52, -42, -32, -17, -2,  13,
    28,   28,  28,  28,  28,  28,  28,  28,  28,  28,  28,  28,  28,  28,  28,
    28,   28,  28,  28,  26,  23,  20,  12,  4,   -5,  -12, -20, -25, -30, -32};
int brightnessSetLogo2[45] = {
    -100, -99, -97, -95, -90, -85, -78, -72, -62, -52, -42, -32, -17, -2, 13,
    23,   28,  28,  28,  28,  28,  28,  29,  32,  35,  39,  44,  47,  50, 55,
    57,   59,  61,  63,  65,  67,  69,  71,  73,  75,  77,  79,  81,  83, 85};
int brightnessSetHeader[45] = {-100, -99, -97, -95, -90, -85, -78, -72, -62,
                               -52,  -42, -32, -17, -2,  13,  23,  28,  28,
                               28,   28,  28,  28,  28,  28,  28,  23,  18,
                               13,   8,   3,   -2,  -7,  -12, -17, -22, -27,
                               -32,  -37, -42, -47, -57, -70, -85, -95, -100};
int brightnessSet = -100;

void drawAnimatedImage(int imageIndex, int speed) {
  showLEDsCore2 = -3;
  leds.clear();
  //leds.show();
  // delay(100);
  if (imageIndex == 0) {
    cycleCount = 0;
    brightnessSet = -100;
    for (int i = startupFrameLEN - 1; i >= 0; i--) {
      drawImage(i);
      // showLEDsCore2 = 3;
      cycleCount++;
      leds.show();
      delayMicroseconds(speed + (cycleCount * 80));
    }
  } else { // play the animation backwards
    cycleCount = 44;
    for (int i = 0; i < 45; i++) {
      drawImage(i);
      leds.show();
      cycleCount--;
      delayMicroseconds(speed + (cycleCount * 200));
    }
  }
  // lightUpRail();
  /// leds.clear();
  showLEDsCore2 = -1;
}

void drawImage(int imageIndex) {

  int skipLines[21] = {
      1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0,
  };
  int lineIndex = 0;

  for (int i = 1; i < 21; i++) {

    if (skipLines[i] == 1) {
      continue;
    }
    for (int j = 0; j < 30; j++) {

      leds.setPixelColor(
          screenMap[lineIndex * 30 + j],
          scaleBrightness(startupFrameArray[imageIndex][i * 32 + j + 1], -93));
    }
    if (skipLines[i] == 0) {
      lineIndex++;
    }
  }

  // int brightnessCurve[45] = { -75, -50, -15, 10, 35, 50, 75, 75, 75, 75, 75,
  // 70,  65, 55, 50, 45, 30, 10, -15, -75, -100 }; int brightnessCurve[45] = {
  // 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

  if (cycleCount < 4) {
    brightnessSet += 2;
  } else if (cycleCount < 8) {
    brightnessSet += 5;
  } else if (cycleCount < 12) {
    brightnessSet += 10;
  } else if (cycleCount < 16) {
    brightnessSet += 15;
  }

  // else if (cycleCount >= startupFrameLEN - 6) {
  //   brightnessSet -= 2;
  //   } else if (cycleCount >= startupFrameLEN - 16) {
  // brightnessSet -=6;
  // }
  // Jerial.print(brightnessSet);
  // Jerial.print(", ");

  for (int i = 400; i < 445; i++) {

    uint8_t pixel_r = rainbowr[(cycleCount + i + 19) % 30];
    uint8_t pixel_g = rainbowg[(cycleCount + i + 19) % 30];
    uint8_t pixel_b = rainbowb[(cycleCount + i + 19) % 30];
    uint32_t pixel = (pixel_r & 0b11111111) << 16 |
                     (pixel_g & 0b11111111) << 8 | (pixel_b & 0b11111111);

    // if (i > 439) {
    if (i < LOGO_LED_END && i > LOGO_LED_START) {
      pixel = scaleBrightness(pixel, brightnessSetLogo[cycleCount]);
    }
    // } else {}
    //   pixel = scaleBrightness(pixel, 0);
    //   }
    else if (i == ADC_LED_0 || i == ADC_LED_1) {
      pixel_r = rainbowr[(cycleCount + i + 7) % 30];
      pixel_g = rainbowg[(cycleCount + i + 7) % 30];
      pixel_b = rainbowb[(cycleCount + i + 7) % 30];
      pixel = (pixel_r & 0b11111111) << 16 | (pixel_g & 0b11111111) << 8 |
              (pixel_b & 0b11111111);

      pixel = scaleBrightness(pixel, brightnessSetLogo2[cycleCount]);
    } else if (i == DAC_LED_0 || i == DAC_LED_1) {
      pixel_r = rainbowr[(cycleCount + i + 10) % 30];
      pixel_g = rainbowg[(cycleCount + i + 10) % 30];
      pixel_b = rainbowb[(cycleCount + i + 10) % 30];
      pixel = (pixel_r & 0b11111111) << 16 | (pixel_g & 0b11111111) << 8 |
              (pixel_b & 0b11111111);

      pixel = scaleBrightness(pixel, brightnessSetLogo2[cycleCount]);
    } else if (i == GPIO_LED_0 || i == GPIO_LED_1) {
      pixel_r = rainbowr[(cycleCount + i + 15) % 30];
      pixel_g = rainbowg[(cycleCount + i + 15) % 30];
      pixel_b = rainbowb[(cycleCount + i + 15) % 30];
      pixel = (pixel_r & 0b11111111) << 16 | (pixel_g & 0b11111111) << 8 |
              (pixel_b & 0b11111111);

      pixel = scaleBrightness(pixel, brightnessSetLogo2[cycleCount]);
    } else if (i == GND_T_LED || i == VIN_LED) {

      pixel = scaleBrightness(pixel, brightnessSetLogo2[cycleCount]);
    } else if (i == RST_0_LED || i == RST_1_LED || i == GND_B_LED ||
               i == V3V3_LED || i == V5V_LED) {

      pixel = scaleBrightness(pixel, brightnessSetLogo[cycleCount]);
    } else {
      pixel = scaleBrightness(pixel, brightnessSetHeader[cycleCount]);
    }

    leds.setPixelColor(i, pixel);
  }

  // cycleCount++;
}

void printAllRLEimageData() {
  for (int i = 0; i < startupFrameLEN; i++) {
    printRLEimageData(i);
  }
}

void printRLEimageData(int imageIndex) {
  Jerial.print("//RLE image data for imageIndex: ");
  Jerial.println(imageIndex);

  Jerial.print("\n\n\rconst uint32_t startupFrame");
  Jerial.print(imageIndex);
  Jerial.println("[] PROGMEM = {");

  const uint32_t *frameData = startupFrameArray[imageIndex];
  // Assuming each frame has a fixed size, e.g. 32*22, which is 704.
  // You might need to adjust this or pass the size as a parameter.
  int frameSize =
      704; // Example size, replace with actual size or dynamic calculation

  std::vector<uint32_t> nonBlackPixels;
  std::vector<int> blackStartIndexes;
  std::vector<int> blackEndIndexes;

  bool inBlackSequence = false;
  int currentBlackStartIndex = -1;

  for (int i = 0; i < frameSize; i++) {
    if (frameData[i] == 0x000000) {
      if (!inBlackSequence) {
        inBlackSequence = true;
        currentBlackStartIndex = i;
      }
    } else {
      if (inBlackSequence) {
        blackStartIndexes.push_back(currentBlackStartIndex);
        blackEndIndexes.push_back(i - 1);
        inBlackSequence = false;
      }
      nonBlackPixels.push_back(frameData[i]);
    }
  }

  // If the frame ends with a black sequence
  if (inBlackSequence) {
    blackStartIndexes.push_back(currentBlackStartIndex);
    blackEndIndexes.push_back(frameSize - 1);
  }

  // Print non-black pixels
  for (size_t i = 0; i < nonBlackPixels.size(); ++i) {
    Jerial.print("0x");
    if (nonBlackPixels[i] < 0x100000)
      Jerial.print("0");
    if (nonBlackPixels[i] < 0x10000)
      Jerial.print("0");
    if (nonBlackPixels[i] < 0x1000)
      Jerial.print("0");
    if (nonBlackPixels[i] < 0x100)
      Jerial.print("0");
    if (nonBlackPixels[i] < 0x10)
      Jerial.print("0");
    Jerial.print(nonBlackPixels[i], HEX);
    if (i < nonBlackPixels.size() - 1) {
      Jerial.print(", ");
    }
    if ((i + 1) % 16 == 0) { // Print 16 values per line
      Jerial.println();
    }
  }
  Jerial.println("};");
  Jerial.println();

  // Print black start indexes
  Jerial.print("const int blackStartIndexes");
  Jerial.print(imageIndex);
  Jerial.println("[] PROGMEM = {");
  for (size_t i = 0; i < blackStartIndexes.size(); ++i) {
    Jerial.print(blackStartIndexes[i]);
    if (i < blackStartIndexes.size() - 1) {
      Jerial.print(", ");
    }
    if ((i + 1) % 16 == 0) {
      Jerial.println();
    }
  }
  Jerial.println("};");
  Jerial.println();

  // Print black end indexes
  Jerial.print("const int blackEndIndexes");
  Jerial.print(imageIndex);
  Jerial.println("[] PROGMEM = {");
  for (size_t i = 0; i < blackEndIndexes.size(); ++i) {
    Jerial.print(blackEndIndexes[i]);
    if (i < blackEndIndexes.size() - 1) {
      Jerial.print(", ");
    }
    if ((i + 1) % 16 == 0) {
      Jerial.println();
    }
  }
  Jerial.println("};");
  Jerial.println();
  Jerial.flush();
}

// Screen state functions for clean transitions - Windows compatible
void saveScreenState(void) {
  // Clear screen for file manager interface - Windows compatible
  Jerial.print("\033[2J\033[H");
  Jerial.flush();
  // Add delay for Windows terminals to process the command
  delay(50);
}

void restoreScreenState(void) {
  // Clear screen for clean return to REPL - Windows compatible
  Jerial.print("\033[2J\033[H");
  Jerial.flush();
  // Add delay for Windows terminals to process the command
  delay(50);
}

// Printf-like function for menu items with automatic color cycling
int printMenuLine(const char* format, ...) {
  if (!format) return 0;
  
  int printed = 0;
  // Format the string
  char buffer[256];
  va_list args;
  va_start(args, format);
  int len = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  
  if (len > 0 && len < sizeof(buffer)) {
    // Print the formatted string
    Jerial.print(buffer);
   // TUI::log(buffer);
    printed = len;
    // Cycle to next color
   // cycleTerminalColor();
  }
  if (printed > 0) {
    cycleTerminalColor();
    printed = 1;
  }
  return printed;
}

// Printf-like function for menu items with automatic color cycling and conditional display
int printMenuLine(int showExtraMenu, int minLevel, const char* format, ...) {
  if (!format || showExtraMenu < minLevel) return 0;
  
  int printed = 0;
  // Format the string
  char buffer[256];
  va_list args;
  va_start(args, format);
  int len = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  
  if (len > 0 && len < sizeof(buffer)) {
    // Print the formatted string
    Jerial.print(buffer);
    //TUI::log(buffer);
    printed = len;
  }
  if (printed > 0) {
    cycleTerminalColor();
    printed = 1;
  }
  return printed;
}
