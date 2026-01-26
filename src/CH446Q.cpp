// SPDX-License-Identifier: MIT

#include <cstdint>  // For uint16_t
#include "CH446Q.h"
#include "Colors.h"       // For changeTerminalColor
#include "JumperlessDefines.h"
#include "JumperlOS.h"    // For LiveCrossbarService
#include "LEDs.h"
#include "MatrixState.h"
#include "NetManager.h"   // For assignTermColor
#include "States.h"
#include "NetsToChipConnections.h"
#include "Peripherals.h"


#include "hardware/pio.h"
#include <hardware/sync.h>  // For __dmb() memory barrier

#include "ch446.pio.h"
#include "FileParsing.h"

//#include "SerialWrapper.h"


// #include "pio_spi.h"

#define MYNAMEISERIC                                                           \
  0 // on the board I sent to eric, the data and clock lines are bodged to GPIO
    // 18 and 19. To allow for using hardware SPI

// int chipToPinArray[12] = {CS_A, CS_B, CS_C, CS_D, CS_E, CS_F, CS_G, CS_H,
// CS_I, CS_J, CS_K, CS_L};
PIO pio = pio0;

uint sm = pio_claim_unused_sm(pio, true);

uint offset = 0;

volatile int chipSelect = 0;
volatile uint32_t irq_flags = 0;

// struct justXY {
//   bool connected[16][8]; // 16 X values, 8 Y values, stores whether a connection exists
//   };

chipXYBitfield lastChipXY[12];

// OPTIMIZATION: Track which chips have connections to avoid scanning empty chips
static bool chipHadConnections[12] = {false};

void isrFromPio(void) {

  // delayMicroseconds(500);
  setCSex(chipSelect, 1);
  //  Serial.println("interrupt from pio  ");
  // Serial.print(chipSelect);
  // Serial.print(" \n\r");
  //delayMicroseconds(10);

  setCSex(chipSelect, 0);

  // Small delay to ensure signal stability
  //delayMicroseconds(10);
  chipSelect = -1;

  // Clear the state machine interrupt (not PIO0_IRQ_0)
  // The PIO program uses "irq 1" and "wait 0 irq 1 rel", so we need to clear interrupt 1 for this state machine
  pio_interrupt_clear(pio, 1);  // Clear interrupt 1 (absolute)
  
  // Also clear the PIO IRQ register for good measure
  irq_flags = pio0_hw->irq;
  hw_clear_bits(&pio0_hw->irq, irq_flags);
  

  }

int changedPaths[MAX_BRIDGES];
int changedPathsCount = 0;

// Index array for chip-ordered processing while keeping main path array in net order
int chipOrderedIndex[MAX_BRIDGES];
bool chipOrderValid = false;

// Timeout counter for PIO debugging
int ch446q_timeout_count = 0;

void initCH446Q(void) {

  uint dat = 14;
  uint clk = 15;

  // uint cs = 7;

  // CRITICAL: Initialize GPIO pins for PIO use (required for RP2350B)
  pio_gpio_init(pio, dat);  // Initialize GPIO 14 for PIO0
  pio_gpio_init(pio, clk);  // Initialize GPIO 15 for PIO0

  irq_add_shared_handler(PIO0_IRQ_1, isrFromPio,
                         PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
  irq_set_enabled(PIO0_IRQ_1, true);

   offset = pio_add_program(pio, &spi_ch446_multi_cs_program);
  // uint offsetCS = pio_add_program(pio, &spi_ch446_cs_handler_program);

  // Serial.print("offset: ");
  // Serial.println(offset);

  pio_spi_ch446_multi_cs_init(pio, sm, offset, 8, 1, 0, 1, clk, dat);

  for (int i = 0; i < 12; i++) {
    pinMode(28 + i, OUTPUT_12MA);
    // digitalWrite(28+i, LOW);
    }
  // pio_spi_ch446_cs_handler_init(pio, smCS, offsetCS, 256, 1, 8, 20, 6);
  // pinMode(CS_A, OUTPUT);
  // digitalWrite(CS_A, HIGH);

  // Initialize lastChipXY array (all connections off)
  // OPTIMIZATION: Use memset with bitfield (much faster)
  memset(lastChipXY, 0, sizeof(lastChipXY));
  
  chipOrderValid = false; // Initialize chip order as invalid
  }

// CRITICAL: Run from RAM to prevent XIP flash cache contention with Core 0
// When both cores execute code from flash simultaneously, they compete for XIP cache
// This causes unpredictable slowdowns. Running Core 2 from RAM eliminates this issue.
void __not_in_flash_func(sendPaths)(int clean) {
  // Performance profiling (matches PROFILE_FAST_REFRESH in Commands.cpp)
  #define PROFILE_CORE2_SENDPATHS 0
  unsigned long core2_start = micros();
  unsigned long core2_step = core2_start;

  core2busy = true;

  // OPTIMIZATION: Only create chip-ordered index if invalid or doing clean refresh
  // For incremental updates (clean==0), we send paths in net order which is fine
  if (clean == 1 || !chipOrderValid) {
    createChipOrderedIndex();
    #if PROFILE_CORE2_SENDPATHS
    Serial.print("Core 2: createChipOrderedIndex: "); 
    Serial.print(micros() - core2_step); 
    Serial.println(" us");
    core2_step = micros();
    #endif
  }

  if (clean == 1) {
    digitalWrite(RESETPIN, HIGH);
    delayMicroseconds(1000);
    digitalWrite(RESETPIN, LOW);
    #if PROFILE_CORE2_SENDPATHS
    Serial.print("Core 2: RESET pulse: "); 
    Serial.print(micros() - core2_step); 
    Serial.println(" us");
    core2_step = micros();
    #endif
  }
  
  sendAllPaths(clean);
  #if PROFILE_CORE2_SENDPATHS
  Serial.print("Core 2: sendAllPaths: "); 
  Serial.print(micros() - core2_step); 
  Serial.println(" us");
  core2_step = micros();
  #endif
  
  core2busy = false;
  sendAllPathsCore2 = 0;
  __dmb();  // Memory barrier so Core 0 sees the update
  
  #if PROFILE_CORE2_SENDPATHS
  unsigned long core2_total = micros() - core2_start;
  Serial.print("Core 2 TOTAL: "); 
  Serial.print(core2_total); 
  Serial.println(" us");
  Serial.println();
  #endif
}



void refreshPaths(void) {
  for (int i = 0; i < MAX_BRIDGES; i++) {
    changedPaths[i] = -2;
    }
  chipOrderValid = false; // Invalidate chip order index
  sendAllPaths(1);
  }

// CRITICAL: Run from RAM to prevent XIP flash cache contention
void __not_in_flash_func(sendAllPaths)(int clean) {
  #define PROFILE_SENDALLPATHS 0
  unsigned long startTime = micros();
  unsigned long stepTime = startTime;
  
  if (clean == 1) {
    // OPTIMIZATION: Use memset to clear lastChipXY (faster than nested loops)
    memset(lastChipXY, 0, sizeof(lastChipXY));
    memset(chipHadConnections, 0, sizeof(chipHadConnections));  // Clear tracking array
    #if PROFILE_SENDALLPATHS
    Serial.print("  clear lastChipXY: "); 
    Serial.print(micros() - stepTime); 
    Serial.println(" us");
    stepTime = micros();
    #endif

    // Send all paths in chip order for hardware efficiency
    int pathsSent = 0;
    for (int i = 0; i < numberOfPaths; i++) {
      int pathIdx = chipOrderValid ? chipOrderedIndex[i] : i;
      sendPath(pathIdx, 1, 0);
      pathsSent++;

      // Update lastChipXY and tracking array
      for (int j = 0; j < 4; j++) {
        if (globalState.connections.paths[pathIdx].chip[j] != -1 && 
            globalState.connections.paths[pathIdx].x[j] != -1 && 
            globalState.connections.paths[pathIdx].y[j] != -1) {
          int chip = globalState.connections.paths[pathIdx].chip[j];
          int x = globalState.connections.paths[pathIdx].x[j];
          int y = globalState.connections.paths[pathIdx].y[j];

          if (chip >= 0 && chip < 12 && x >= 0 && x < 16 && y >= 0 && y < 8) {
            lastChipXY[chip].connected[y] |= (1 << x);  // Set bit in bitfield
            chipHadConnections[chip] = true;  // Track that this chip has connections
          }
        }
      }
    }
    #if PROFILE_SENDALLPATHS
    Serial.print("  send all paths ("); 
    Serial.print(pathsSent); 
    Serial.print("): "); 
    Serial.print(micros() - stepTime); 
    Serial.println(" us");
    #endif
    
    // Request live crossbar display update via service (waits for colors)
    liveCrossbarService.requestUpdate();
    return;
  } else {
    // INCREMENTAL: Only send changed paths
    findDifferentPaths();
    #if PROFILE_SENDALLPATHS
    Serial.print("  findDifferentPaths: "); 
    Serial.print(micros() - stepTime); 
    Serial.println(" us");
    stepTime = micros();
    #endif
    
    int changedCount = 0;
    for (int i = 0; i < numberOfPaths; i++) {
      if (changedPaths[i] == 1) {
        sendPath(i, 1, 0);
        changedCount++;
      }
    }
    #if PROFILE_SENDALLPATHS
    Serial.print("  send changed paths ("); 
    Serial.print(changedCount); 
    Serial.print("): "); 
    Serial.print(micros() - stepTime); 
    Serial.println(" us");
    #endif
    
    // Request live crossbar display update via service (waits for colors)
    liveCrossbarService.requestUpdate();
  }
}


void printChipStateArray(void) {

  Serial.println("Analog Crossbar Array\n\r");


  // for (int i = 0; i < 12; i++) {
  //   Serial.print("chip ");
  //   Serial.print(i);
  //   Serial.print(" ");
  //   for (int j = 0; j < 16; j++) {
  //     Serial.print(xName(i, j));
  //     Serial.print(" ");
  //     }
  //   Serial.println();
  //   }
  // Serial.println();

  // for (int i = 0; i < 12; i++) {
  //   for (int j = 0; j < 8; j++) {
  //     Serial.print(yName(i, j));
  //     Serial.print(" ");
  //     }
  //   Serial.println();
  //   }
  // Serial.println();
  int showX = 1;
  int showY = 1;



  for (int blockRow = 0; blockRow < 3; blockRow++) {
    int startChip = blockRow * 4;
    int endChip = startChip + 4;
    // Print chip headers
    Serial.print("           ");
    for (int chip = startChip; chip < endChip; chip++) {
      Serial.print("  chip ");
      Serial.print(chipNumToChar(chip));
      Serial.print(" ");
      if (chip < endChip - 1) {
        for (int s = 0; s < 25; s++) Serial.print(" "); // spacing between blocks
        }
      }
    Serial.println("");
    // Print Y headers for each chip block
    if (showY) {
    Serial.print("     ");
    } else {
    Serial.print("     ");
      }
    for (int j = 0; j < 4; j++)
      {
      for (int i = 0; i < 8; i++)
        {
        //Serial.print(" ");
        if (showY)
          {
          Serial.print(i);
          Serial.print("  ");
          } else {
            Serial.print("   ");
            }
        }
        if (showY && j < 3)
          {
          Serial.print("          ");
          } else {
          Serial.print("          ");
            }
      }

    Serial.println();
    // Print each X row for all 4 chips in this block row
    for (int x = 0; x < 16; x++) {
      for (int chip = startChip; chip < endChip; chip++) {
        //Serial.print("x");
        if (showX) {
        Serial.print(" ");
        if (x < 10) Serial.print(" ");
        Serial.print(x);
        } else {
          Serial.print("   ");
          }

        Serial.print(" "); // space between chip blocks

        for (int y = 0; y < 8; y++) {
          int verticalLine = 0;
          int horizontalLine = 0;
          // Check if any x is connected at this y (vertical line)
          if (lastChipXY[chip].connected[y] != 0) {
            verticalLine = 1;
          }
          // Check if this x is connected at any y (horizontal line)
          for (int j = 0; j < 8; j++) {
            if (lastChipXY[chip].connected[j] & (1 << x)) {
              horizontalLine = 1;
              break;
            }
          }
          
          if (lastChipXY[chip].connected[y] & (1 << x)) {
            Serial.print("─█─");
            Serial.flush();
          } else {
            if (verticalLine && horizontalLine) {
              Serial.print("─┼─");
              Serial.flush();
            } else if (verticalLine) {
              Serial.print(" │ ");
              Serial.flush();
            } else if (horizontalLine) {
              Serial.print("───");
              Serial.flush();
            } else {
              Serial.print(" . ");
              Serial.flush();
            }
          }
        }

        Serial.print(" "); // space between chip blocks
        Serial.print(xName(chip, x));
        Serial.print("  ");
        }

      Serial.println();
      }
    for (int chip = startChip; chip < endChip; chip++) {
      Serial.print("     ");
      //Serial.print("y");
      for (int y = 0; y < 8; y++) {

        Serial.print(yName(chip, y));
        //Serial.print(" ");
        }
      Serial.print("     "); // spacing between blocks
      }
    Serial.println("\n\n\r"); // extra space between block rows
    }
    Serial.flush();
  }

void printLastChipStateArray(void) {

}

/// @brief Print the crossbar array with colors showing which net owns each line
/// At crossings, horizontal segments show in the horizontal net's color,
/// vertical segments show in the vertical net's color
void printChipStateArrayColor(void) {
  Serial.println("Analog Crossbar Array (Colored by Net)\n\r");
  
  // Make sure terminal colors are assigned to nets
  assignTermColor();

  int showX = 1;
  int showY = 1;

  for (int blockRow = 0; blockRow < 3; blockRow++) {
    int startChip = blockRow * 4;
    int endChip = startChip + 4;
    
    // Print chip headers
    Serial.print("           ");
    for (int chip = startChip; chip < endChip; chip++) {
      Serial.print("  chip ");
      Serial.print(chipNumToChar(chip));
      Serial.print(" ");
      if (chip < endChip - 1) {
        for (int s = 0; s < 25; s++) Serial.print(" ");
      }
    }
    Serial.println("");
    
    // Print Y headers
    if (showY) {
      Serial.print("     ");
    } else {
      Serial.print("     ");
    }
    for (int j = 0; j < 4; j++) {
      for (int i = 0; i < 8; i++) {
        if (showY) {
          Serial.print(i);
          Serial.print("  ");
        } else {
          Serial.print("   ");
        }
      }
      if (showY && j < 3) {
        Serial.print("          ");
      } else {
        Serial.print("          ");
      }
    }
    Serial.println();

    // Print each X row for all 4 chips in this block row
    for (int x = 0; x < 16; x++) {
      for (int chip = startChip; chip < endChip; chip++) {
        if (showX) {
          Serial.print(" ");
          if (x < 10) Serial.print(" ");
          Serial.print(x);
        } else {
          Serial.print("   ");
        }
        Serial.print(" ");

        for (int y = 0; y < 8; y++) {
          int verticalLine = 0;
          int horizontalLine = 0;
          
          // Check if any x is connected at this y (vertical line exists)
          if (lastChipXY[chip].connected[y] != 0) {
            verticalLine = 1;
          }
          
          // Check if this x is connected at any y (horizontal line exists)
          for (int j = 0; j < 8; j++) {
            if (lastChipXY[chip].connected[j] & (1 << x)) {
              horizontalLine = 1;
              break;
            }
          }
          
          // Get the net colors for this x and y
          int xNet = globalState.connections.chipStates[chip].xStatus[x];
          int yNet = globalState.connections.chipStates[chip].yStatus[y];
          int xColor = (xNet > 0 && xNet < MAX_NETS) ? globalState.connections.nets[xNet].termColor : -1;
          int yColor = (yNet > 0 && yNet < MAX_NETS) ? globalState.connections.nets[yNet].termColor : -1;
          
          if (lastChipXY[chip].connected[y] & (1 << x)) {
            // Connection point - both lines belong to the same net (or at least meet here)
            // Use the net that "owns" this crosspoint - typically the X line's net
            int connNet = xNet > 0 ? xNet : yNet;
            int connColor = (connNet > 0 && connNet < MAX_NETS) ? globalState.connections.nets[connNet].termColor : -1;
            changeTerminalColor(connColor, false, &Serial);
            Serial.print("─█─");
            changeTerminalColor(-1, false, &Serial);
          } else {
            if (verticalLine && horizontalLine) {
              // Crossing - horizontal in X color, center in Y color
              changeTerminalColor(xColor, false, &Serial);
              Serial.print("─");
              changeTerminalColor(yColor, false, &Serial);
              Serial.print("┼");
              changeTerminalColor(xColor, false, &Serial);
              Serial.print("─");
              changeTerminalColor(-1, false, &Serial);
            } else if (verticalLine) {
              // Just vertical line
              changeTerminalColor(yColor, false, &Serial);
              Serial.print(" │ ");
              changeTerminalColor(-1, false, &Serial);
            } else if (horizontalLine) {
              // Just horizontal line
              changeTerminalColor(xColor, false, &Serial);
              Serial.print("───");
              changeTerminalColor(-1, false, &Serial);
            } else {
              // No line - just a dot
              Serial.print(" . ");
            }
          }
        }

        Serial.print(" ");
        Serial.print(xName(chip, x));
        Serial.print("  ");
      }
      Serial.println();
    }
    
    // Print Y names at bottom
    for (int chip = startChip; chip < endChip; chip++) {
      Serial.print("     ");
      for (int y = 0; y < 8; y++) {
        Serial.print(yName(chip, y));
      }
      Serial.print("     ");
    }
    Serial.println("\n\n\r");
  }
  Serial.flush();
}

/// @brief Compact color-coded crossbar display - uses single characters per cell
/// Fits more information in less screen space while still showing net colors
/// @param chipsPerRow Number of chips to display per row (default 6)
void printChipStateArrayColorCompact(int chipsPerRow, char blankChar) {
  Serial.println("Crossbar (Compact)\n\r");
  
  // Make sure terminal colors are assigned to nets
  assignTermColor();

  // Clamp chipsPerRow to valid range
  if (chipsPerRow < 2) chipsPerRow = 2;
  if (chipsPerRow > 12) chipsPerRow = 12;

  int numRows = (12 + chipsPerRow - 1) / chipsPerRow;  // Ceiling division

  for (int blockRow = 0; blockRow < numRows; blockRow++) {
    int startChip = blockRow * chipsPerRow;
    int endChip = startChip + chipsPerRow;
    if (endChip > 12) endChip = 12;
    
    // Print chip headers (compact)
    Serial.print("   ");
    for (int chip = startChip; chip < endChip; chip++) {
      Serial.print(chipNumToChar(chip));
      Serial.print("        ");  // 8 chars for 8 Y columns
    }
    Serial.println();

    // Print each X row for all chips in this block row
    for (int x = 0; x < 16; x++) {
      // Row number (compact, no leading space for single digit)
      if (x < 10) Serial.print(" ");
      Serial.print(x);
      Serial.print(" ");

      for (int chip = startChip; chip < endChip; chip++) {
        for (int y = 0; y < 8; y++) {
          int verticalLine = 0;
          int horizontalLine = 0;
          
          // Check if any x is connected at this y (vertical line exists)
          if (lastChipXY[chip].connected[y] != 0) {
            verticalLine = 1;
          }
          
          // Check if this x is connected at any y (horizontal line exists)
          for (int j = 0; j < 8; j++) {
            if (lastChipXY[chip].connected[j] & (1 << x)) {
              horizontalLine = 1;
              break;
            }
          }
          
          // Get the net colors for this x and y
          int xNet = globalState.connections.chipStates[chip].xStatus[x];
          int yNet = globalState.connections.chipStates[chip].yStatus[y];
          int xColor = (xNet > 0 && xNet < MAX_NETS) ? globalState.connections.nets[xNet].termColor : -1;
          int yColor = (yNet > 0 && yNet < MAX_NETS) ? globalState.connections.nets[yNet].termColor : -1;
          
          if (lastChipXY[chip].connected[y] & (1 << x)) {
            // Connection point - use the net color
            int connNet = xNet > 0 ? xNet : yNet;
            int connColor = (connNet > 0 && connNet < MAX_NETS) ? globalState.connections.nets[connNet].termColor : -1;
            changeTerminalColor(connColor, false, &Serial);
            Serial.print("█");
            changeTerminalColor(-1, false, &Serial);
          } else if (verticalLine && horizontalLine) {
            // Crossing - show in vertical line's color (Y net)
            changeTerminalColor(yColor, false, &Serial);
            Serial.print("┼");
            changeTerminalColor(-1, false, &Serial);
          } else if (verticalLine) {
            // Just vertical line
            changeTerminalColor(yColor, false, &Serial);
            Serial.print("│");
            changeTerminalColor(-1, false, &Serial);
          } else if (horizontalLine) {
            // Just horizontal line
            changeTerminalColor(xColor, false, &Serial);
            Serial.print("─");
            changeTerminalColor(-1, false, &Serial);
          } else {
            // No line
            Serial.print(blankChar);
          }
        }
        Serial.print("  ");  // Single space between chips
      }
      Serial.println();
      Serial.flush();
    }
    Serial.println();  // Single blank line between block rows
  }
  Serial.flush();
}

// ============================================================================
// Live Crossbar Display - Updates at top of terminal on connection changes
// Uses DECSTBM (Set Top and Bottom Margins) to create a non-scrolling header
// ============================================================================

bool liveCrossbarEnabled = false;
static const int LIVE_CROSSBAR_HEIGHT = 18;  // Header + 16 rows + separator line

void setLiveCrossbarEnabled(bool enabled) {
  liveCrossbarEnabled = enabled;
  if (enabled) {
    // Clear screen and move to home
    Serial.print("\033[2J\033[H");
    
    // Draw the initial crossbar display
    updateLiveCrossbarDisplay();
    
    // Set scrolling region BELOW the crossbar area (DECSTBM)
    // This makes rows 1-LIVE_CROSSBAR_HEIGHT fixed (non-scrolling)
    // and rows LIVE_CROSSBAR_HEIGHT+1 to bottom scrollable
    Serial.printf("\033[%d;999r", LIVE_CROSSBAR_HEIGHT + 1);
    
    // Move cursor to the scrolling region and print status
    Serial.printf("\033[%d;1H", LIVE_CROSSBAR_HEIGHT + 1);
    Serial.println("--- Live Crossbar Mode (c! to disable) ---\r");
    Serial.println("\r");
    Serial.flush();
  } else {
    // Reset scrolling region to full screen (DECSTBM with no params)
    Serial.print("\033[r");
    // Clear screen and home
    Serial.print("\033[2J\033[H");
    Serial.println("Live crossbar display disabled.\r");
    Serial.flush();
  }
}

/// @brief Update the live crossbar display at top of terminal
/// Uses DECSTBM scrolling region - crossbar is in non-scrolling area at top
/// Uses DECSC/DECRC (ESC 7 / ESC 8) to save/restore cursor position
void updateLiveCrossbarDisplay(void) {
  if (!liveCrossbarEnabled) return;
  
  // Make sure terminal colors are assigned
  assignTermColor();
  
  // Build net lookup from paths array (more reliable than chipStates for multi-hop paths)
  // connectionNet[chip][x][y] = net number for that connection point
  static int8_t connectionNet[12][16][8];
  memset(connectionNet, 0, sizeof(connectionNet));
  
  // Scan all paths and map each (chip, x, y) to its net
  for (int i = 0; i < numberOfPaths; i++) {
    int net = globalState.connections.paths[i].net;
    if (net <= 0) continue;
    
    // Check all 4 possible hops in the path
    for (int hop = 0; hop < 4; hop++) {
      int chip = globalState.connections.paths[i].chip[hop];
      int x = globalState.connections.paths[i].x[hop];
      int y = globalState.connections.paths[i].y[hop];
      
      if (chip >= 0 && chip < 12 && x >= 0 && x < 16 && y >= 0 && y < 8) {
        connectionNet[chip][x][y] = net;
      }
    }
  }
  
  // Save cursor position with DECSC (more reliable than CSI s)
  Serial.print("\0337");
  
  // Move to home position for drawing (top-left, in non-scrolling area)
  Serial.print("\033[H");
  
  // Print chip headers
  Serial.print("   ");
  for (int chip = 0; chip < 12; chip++) {
    Serial.print(chipNumToChar(chip));
    Serial.print("        ");
  }
  Serial.print("\033[K\r\n");  // Clear to EOL + CR + newline

  // Print each X row (16 rows)
  for (int x = 0; x < 16; x++) {
    // Row number
    // Serial.printf("%2d ", x);

    for (int chip = 0; chip < 12; chip++) {
      for (int y = 0; y < 8; y++) {
        // Check connection state
        bool isConnected = lastChipXY[chip].connected[y] & (1 << x);
        bool verticalLine = lastChipXY[chip].connected[y] != 0;
        bool horizontalLine = false;
        
        for (int j = 0; j < 8; j++) {
          if (lastChipXY[chip].connected[j] & (1 << x)) {
            horizontalLine = true;
            break;
          }
        }
        
        // Get net from our lookup (built from paths, not chipStates)
        int connNet = connectionNet[chip][x][y];
        int color = (connNet > 0 && connNet < MAX_NETS) ? globalState.connections.nets[connNet].termColor : -1;
        
        // For lines that pass through but don't connect here, find a net from the same X or Y line
        if (connNet == 0) {
          // Check if there's a net on this X line (horizontal)
          if (horizontalLine) {
            for (int j = 0; j < 8 && connNet == 0; j++) {
              connNet = connectionNet[chip][x][j];
            }
          }
          // Check if there's a net on this Y line (vertical)
          if (verticalLine && connNet == 0) {
            for (int j = 0; j < 16 && connNet == 0; j++) {
              if (connectionNet[chip][j][y] > 0) {
                connNet = connectionNet[chip][j][y];
              }
            }
          }
          color = (connNet > 0 && connNet < MAX_NETS) ? globalState.connections.nets[connNet].termColor : -1;
        }
        
        if (isConnected) {
          if (color >= 0) Serial.printf("\033[38;5;%dm", color);
          Serial.print("█");
          if (color >= 0) Serial.print("\033[0m");
        } else if (verticalLine && horizontalLine) {
          if (color >= 0) Serial.printf("\033[38;5;%dm", color);
          Serial.print("┼");
          if (color >= 0) Serial.print("\033[0m");
        } else if (verticalLine) {
          if (color >= 0) Serial.printf("\033[38;5;%dm", color);
          Serial.print("│");
          if (color >= 0) Serial.print("\033[0m");
        } else if (horizontalLine) {
          if (color >= 0) Serial.printf("\033[38;5;%dm", color);
          Serial.print("─");
          if (color >= 0) Serial.print("\033[0m");
        } else {
          Serial.print(" ");
        }
      }
      Serial.print(" ");
    }
    Serial.print("\033[K\r\n");  // Clear to EOL + CR + newline
  }
  
  // Separator line (row 18)
  Serial.print("                                                                                            \033[K");
  
  // Restore cursor position with DECRC (returns to where it was in scrolling region)
  Serial.print("\0338");
  Serial.flush();
}

// New function to update the current chip state array based on paths
// CRITICAL: Run from RAM to prevent XIP flash cache contention
void __not_in_flash_func(updateChipStateArray)() {
  // Clear changed paths array (only clear what we need)
  memset(changedPaths, -1, numberOfPaths * sizeof(int));
  
  // OPTIMIZATION: Use bitfield instead of bool array (8x smaller, fits in cache)
  // 16 bytes per chip vs 128 bytes
  chipXYBitfield newChipXY[12];
  memset(newChipXY, 0, sizeof(newChipXY));
  bool newChipHasConnections[12] = {false};
  
  // Build new connection map and track which chips are used
  for (int i = 0; i < numberOfPaths; i++) {
    for (int j = 0; j < 4; j++) {
      int chip = globalState.connections.paths[i].chip[j];
      int x = globalState.connections.paths[i].x[j];
      int y = globalState.connections.paths[i].y[j];
      
      // Validate and mark connection using bitfield
      if (chip >= 0 && chip < 12 && x >= 0 && x < 16 && y >= 0 && y < 8) {
        newChipXY[chip].connected[y] |= (1 << x);  // Set bit
        newChipHasConnections[chip] = true;
      }
    }
  }

  // Mark paths that have changed (new or modified connections)
  for (int i = 0; i < numberOfPaths; i++) {
    bool pathChanged = false;
    
    for (int j = 0; j < 4; j++) {
      int chip = globalState.connections.paths[i].chip[j];
      int x = globalState.connections.paths[i].x[j];
      int y = globalState.connections.paths[i].y[j];
      
      if (chip >= 0 && chip < 12 && x >= 0 && x < 16 && y >= 0 && y < 8) {
        // Check if this crosspoint changed state using bitfield
        bool wasConnected = (lastChipXY[chip].connected[y] & (1 << x)) != 0;
        bool nowConnected = (newChipXY[chip].connected[y] & (1 << x)) != 0;
        if (wasConnected != nowConnected) {
          pathChanged = true;
          break;
        }
      }
    }
    
    if (pathChanged) {
      changedPaths[i] = 1;
    }
  }

  // CRITICAL: Handle disconnections
  // Only scan chips that HAD connections in the previous state (tracked in chipHadConnections)
  // OPTIMIZATION: Use bitwise operations for faster scanning
  for (int chip = 0; chip < 12; chip++) {
    if (!chipHadConnections[chip]) continue;  // Skip chips that never had connections
    
    // Scan this chip for disconnections using bitfield
    for (int y = 0; y < 8; y++) {
      // XOR to find differences: bits that changed from 1 to 0
      uint16_t removed = lastChipXY[chip].connected[y] & ~newChipXY[chip].connected[y];
      
      // Send disconnect for each removed connection
      if (removed) {
        for (int x = 0; x < 16; x++) {
          if (removed & (1 << x)) {
            sendXYraw(chip, x, y, 0);
          }
        }
      }
    }
  }

  // Update tracking for next iteration
  memcpy(chipHadConnections, newChipHasConnections, sizeof(chipHadConnections));

  // OPTIMIZATION: Copy new state to lastChipXY efficiently using memcpy
  // Bitfield version is 8x smaller so this is very fast
  memcpy(lastChipXY, newChipXY, sizeof(lastChipXY));
}

// Updated findDifferentPaths to use the chip state approach
// CRITICAL: Run from RAM to prevent XIP flash cache contention
void __not_in_flash_func(findDifferentPaths)(void) {
  updateChipStateArray();
  }

void sendPath(int i, int setOrClear, int newOrLast) {

  uint32_t chAddress = 0;

  int chipToConnect = 0;
  int chYdata = 0;
  int chXdata = 0;


    for (int chip = 0; chip < 4; chip++) {
      if (globalState.connections.paths[i].chip[chip] != -1) {
        chipSelect = globalState.connections.paths[i].chip[chip];

        chipToConnect = globalState.connections.paths[i].chip[chip];

        if (globalState.connections.paths[i].y[chip] == -1 || globalState.connections.paths[i].x[chip] == -1) {
          if (debugNTCC)
            Serial.print("!");

          continue;
          }

        sendXYraw(chipToConnect, globalState.connections.paths[i].x[chip], globalState.connections.paths[i].y[chip], setOrClear);
        }
      }
  
  }

void sendXYraw(int chip, int x, int y, int setOrClear) {
  uint32_t chAddress = 0;
  chipSelect = chip;

  // CRITICAL SAFETY: Chip K voltage source protection
  // Chip K X positions: 4=TOP_RAIL, 5=BOTTOM_RAIL, 6=DAC1, 7=DAC0, 15=GND
  // NEVER allow multiple voltage sources on the same Y (would short them together!)
  #define CHIP_K 10
  #define CHIP_K_VOLTAGE_SOURCES 0x80F0  // Bits: 15,7,6,5,4
  
  if (chip == CHIP_K && setOrClear == 1 && x >= 0 && x < 16 && y >= 0 && y < 8) {
    // Check if this X is a voltage source
    if ((1 << x) & CHIP_K_VOLTAGE_SOURCES) {
      // FAST CHECK: Are any OTHER voltage sources already connected to this Y?
      // Use bitwise AND to check all voltage positions at once (single instruction!)
      uint16_t otherVoltages = CHIP_K_VOLTAGE_SOURCES & ~(1 << x);  // All voltages except the one we're setting
      uint16_t conflicting = lastChipXY[CHIP_K].connected[y] & otherVoltages;
      
      if (conflicting) {
        // SAFETY VIOLATION: Another voltage source is connected to this Y
        // Disconnect ALL conflicting voltage sources before connecting the new one
        for (int conflictX = 0; conflictX < 16; conflictX++) {
          if (conflicting & (1 << conflictX)) {
            // Recursively disconnect (this won't recurse further since setOrClear=0)
            sendXYraw(CHIP_K, conflictX, y, 0);
          }
        }
      }
    }
  }

  //unsigned long start = micros();

  int chYdata = y;
  int chXdata = x;

  chYdata = chYdata << 5;
  chYdata = chYdata & 0b11100000;

  chXdata = chXdata << 1;
  chXdata = chXdata & 0b00011110;

  chAddress = chYdata | chXdata;

  if (setOrClear == 1) {
    chAddress = chAddress | 0b00000001; // this last bit determines whether we set or unset the path
    }

  chAddress = chAddress << 24;


  pio_sm_put(pio, sm, chAddress);



  unsigned long wait_start = micros();
  unsigned long wait_end = micros() + 1000000;
  while (chipSelect != -1) {
    //delayMicroseconds(1);
    tight_loop_contents();
    
    // DEBUG: Warn if waiting too long
    if (micros() > wait_end) {  // 1 second timeout
      ch446q_timeout_count++;
      Serial.print("WARNING: CH446Q sendXYraw waiting for chipSelect for more than 1 second! (timeout #");
      Serial.print(ch446q_timeout_count);
      Serial.println(")");
      Serial.println("CH446Q: Attempting emergency PIO reset...");
      
      // Emergency reset: force chipSelect to -1 and reset PIO
      chipSelect = -1;
      pio_sm_set_enabled(pio, sm, false);
      delayMicroseconds(100);
      pio_sm_set_enabled(pio, sm, true);
      pio_interrupt_clear(pio, sm);
      
      // Debug PIO state after reset
      Serial.print("CH446Q: After emergency reset - SM enabled: ");
      Serial.print((pio->ctrl & (1u << (PIO_CTRL_SM_ENABLE_LSB + sm))) ? "YES" : "NO");
      Serial.print(", IRQ enabled: ");
      Serial.print(irq_is_enabled(PIO0_IRQ_1) ? "YES" : "NO");
      Serial.print(", chipSelect: ");
      Serial.println(chipSelect);
      isrFromPio();
      break;  // Break out of the loop to prevent infinite hang
    }
  }


  }

void createXYarray(void) { }

// Creates an index array sorted by chip, x, y while keeping main path array in net order
void createChipOrderedIndex() {
  // Initialize index array
  for (int i = 0; i < numberOfPaths; i++) {
    chipOrderedIndex[i] = i;
    }
  
  // Sort the index array based on chip, x, y values of the paths they point to
  for (int i = 0; i < numberOfPaths - 1; i++) {
    for (int j = 0; j < numberOfPaths - i - 1; j++) {
      bool swap = false;
      for (int k = 0; k < 4; k++) {
        int idx1 = chipOrderedIndex[j];
        int idx2 = chipOrderedIndex[j + 1];
        
        if (globalState.connections.paths[idx1].chip[k] < globalState.connections.paths[idx2].chip[k]) break;
        if (globalState.connections.paths[idx1].chip[k] > globalState.connections.paths[idx2].chip[k]) { swap = true; break; }
        if (globalState.connections.paths[idx1].x[k] < globalState.connections.paths[idx2].x[k]) break;
        if (globalState.connections.paths[idx1].x[k] > globalState.connections.paths[idx2].x[k]) { swap = true; break; }
        if (globalState.connections.paths[idx1].y[k] < globalState.connections.paths[idx2].y[k]) break;
        if (globalState.connections.paths[idx1].y[k] > globalState.connections.paths[idx2].y[k]) { swap = true; break; }
        }
      if (swap) {
        int temp = chipOrderedIndex[j];
        chipOrderedIndex[j] = chipOrderedIndex[j + 1];
        chipOrderedIndex[j + 1] = temp;
        }
      }
    }
  chipOrderValid = true;
  }

// Legacy function name kept for compatibility, but now creates index instead of sorting
void sortPathsByChipXY() {
  createChipOrderedIndex();
  }

// Copy justXY bool array to bitfield (for conversion if needed)
void copyJustXYToBitfield(const struct justXY& src, chipXYBitfield& dst) {
  for (int y = 0; y < 8; y++) {
    dst.connected[y] = 0;
    for (int x = 0; x < 16; x++) {
      if (src.connected[x][y]) {
        dst.connected[y] |= (1 << x);
        }
      }
    }
  }

// Copy bitfield to justXY bool array (for conversion if needed)
void copyBitfieldToJustXY(const chipXYBitfield& src, struct justXY& dst) {
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 16; x++) {
      dst.connected[x][y] = (src.connected[y] & (1 << x)) != 0;
      }
    }
  }

// Capture current chipXY state into bitfield array
// This snapshots the complete crossbar state for all 12 chips
void captureCurrentChipXYState(chipXYBitfield snapshot[12]) {
  // OPTIMIZATION: Since lastChipXY is already a bitfield, just copy it directly!
  memcpy(snapshot, lastChipXY, sizeof(chipXYBitfield) * 12);
}

// Apply a complete chipXY state snapshot, sending only changed connections
// This preserves existing unchanged connections while switching ADC routing
// KEY OPTIMIZATION: Only sends changes, not entire state
void applyChipXYState(const chipXYBitfield targetState[12]) {
  for (int chip = 0; chip < 12; chip++) {
    for (int y = 0; y < 8; y++) {
      uint16_t currentRow = lastChipXY[chip].connected[y];  // Already bitfield!
      uint16_t targetRow = targetState[chip].connected[y];
      
      // Find differences using XOR
      uint16_t changes = currentRow ^ targetRow;
      if (changes) {
        // Send only changed connections
        for (int x = 0; x < 16; x++) {
          if (changes & (1 << x)) {
            bool newState = (targetRow & (1 << x)) != 0;
            sendXYraw(chip, x, y, newState ? 1 : 0);
            // Update lastChipXY bitfield
            if (newState) {
              lastChipXY[chip].connected[y] |= (1 << x);   // Set bit
            } else {
              lastChipXY[chip].connected[y] &= ~(1 << x);  // Clear bit
            }
          }
        }
      }
    }
  }
}

