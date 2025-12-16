// SPDX-License-Identifier: MIT
#ifndef CH446Q_H
#define CH446Q_H

#include <stdint.h>  // For uint16_t


extern int netNumberC2;
extern int onOffC2;
extern int nodeC2;
extern int brightnessC2;
extern int hueShiftC2;
extern int lightUpNetCore2;

struct justXY {
    bool connected[16][8]; // 16 X values, 8 Y values, stores whether a connection exists
    };

// Bitfield version - 128 bits (16 bytes) per chip vs 128 bytes for bool array
// Memory efficient for storing chipXY state snapshots in fake GPIO configs
// MUCH faster and more memory-efficient: 16 bytes vs 128 bytes (8x smaller, fits better in cache)
struct chipXYBitfield {
    uint16_t connected[8]; // Each uint16_t stores 16 X values for one Y
    // Bit operations: connected[y] & (1 << x) to test
    // connected[y] |= (1 << x) to set
    // connected[y] &= ~(1 << x) to clear
};

// Use bitfield version for lastChipXY (global state tracking)
extern chipXYBitfield lastChipXY[12];

// Bitfield helper functions
inline bool getConnectionBit(const chipXYBitfield& state, int x, int y) {
    if (x < 0 || x >= 16 || y < 0 || y >= 8) return false;
    return (state.connected[y] & (1 << x)) != 0;
}

inline void setConnectionBit(chipXYBitfield& state, int x, int y, bool value) {
    if (x < 0 || x >= 16 || y < 0 || y >= 8) return;
    if (value) {
        state.connected[y] |= (1 << x);
    } else {
        state.connected[y] &= ~(1 << x);
    }
}

void copyJustXYToBitfield(const struct justXY& src, chipXYBitfield& dst);
void copyBitfieldToJustXY(const chipXYBitfield& src, struct justXY& dst);

void sendPaths(int clean = 0);
void initCH446Q(void);
void sendXYraw(int chip, int x, int y, int setorclear);

void sendAllPaths(int clean = 0); // should we sort them by chip? for now, no

void sendPath(int path, int setOrClear = 1, int newOrLast = 0);
void findDifferentPaths(void);
void createXYarray(void);
void refreshPaths(void);
void sortPathsByChipXY(void);
void printChipStateArray(void);
void updateChipStateArray(void);
void createChipOrderedIndex(void);
void printLastChipStateArray(void);

// ChipXY state management for fake GPIO
void applyChipXYState(const chipXYBitfield targetState[12]);
void captureCurrentChipXYState(chipXYBitfield snapshot[12]);

#endif
