// SPDX-License-Identifier: MIT
//
// Undo system - in-RAM only.
//
// Compact delta ring (16k+ ops on PSRAM, ~512 SRAM-only) plus a small
// PSRAM-resident snapshot ring that backs non-symmetric ops (clear-all).
// Apply/revert is symmetric for connect/disconnect/DAC/GPIO; clear-all
// uses a blob captured at record time. History is fresh on every boot.

#include "Undo.h"
#include "Graphics.h"
#include "PsramArena.h"
#include "FilesystemStuff.h"
#include "States.h"
#include "Commands.h"  // refreshConnections() - push bridge list to CH446Q + LEDs
#include "Peripherals.h"  // setRailsAndDACs() - push DAC/rail voltages
#include "config.h"
#include "externVars.h"
#include "oled.h"
#include "NetManager.h"  // definesToChar() - friendly node names (GND/D2/...)
#include "JumperlessDefines.h"  // DAC0/DAC1/ROUTABLE_BUFFER_IN node IDs
#include "RotaryEncoder.h"  // NUM_SLOTS, extern netSlot

extern volatile unsigned long undoActivityUntil;
extern int netSlot;

#include <Arduino.h>
#include <FatFS.h>
#include <string.h>
#include <algorithm>
#include "hardware/structs/sio.h"

// Per-area debug flag. psram_debug stays as a master that also enables this.
// Matches the linkage of the declaration in Undo.h (inside extern "C" block).
extern "C" {
    volatile int undo_debug = 0;
}

#define UNDBG(fmt, ...) do { if (undo_debug || psram_debug) { Serial.printf("[%lu c%d] UNDO> " fmt "\n", (unsigned long)millis(), (int)(sio_hw->cpuid & 1), ##__VA_ARGS__); Serial.flush(); } } while(0)

extern struct config jumperlessConfig;
extern JumperlessState globalState;

// =============================================================================
// Op / txn record formats
// =============================================================================

namespace {

#pragma pack(push, 1)
struct UndoOp {
    uint8_t  type;           // UndoOpType
    uint8_t  flags;
    uint16_t txnIdx;         // back-pointer to enclosing txn (lower 16 bits)
    union {
        struct { int16_t n1, n2; uint32_t color; uint16_t _pad; } bridge;       // 12B body
        struct { uint8_t ch; uint8_t _pad[3]; float prev, next; } dac;          // 12B
        struct { uint8_t pin, prevVal, nextVal, prevDir; uint8_t _pad[8]; } gpio; // 12B
        struct { uint8_t prev, next; uint8_t _pad[10]; } slot;                  // 12B
        // snapTxnId is a leftover from the old persistent-history layout;
        // kept zero in this version (in-RAM blob refs are always live for
        // the life of the power cycle).
        struct { uint32_t blobOffset; uint32_t blobSize; uint32_t snapTxnId; } blob; // 12B
        uint8_t raw[12];
    };
};

struct UndoTxn {
    uint32_t startMs;
    uint32_t firstOp;        // global op index (monotonic, not ring offset)
    uint32_t globalId;       // monotonic txn ID at endTxn() time
    uint16_t opCount;
    uint8_t  flags;          // UNDO_TXN_* (waypoint, committed, user, obsolete)
    uint8_t  source;
    uint8_t  slot;           // owning slot - undo/redo only walks matching txns
    char     label[23];
};
#pragma pack(pop)

static_assert(sizeof(UndoOp) == 16, "UndoOp must be exactly 16 bytes");
static_assert(sizeof(UndoTxn) == 40, "UndoTxn must be exactly 40 bytes");

// Internal flag (not exposed in Undo.h - this is purely a ring bookkeeping
// concern). When the user records a new action on slot S after having
// undone some history on slot S, the txns past slot S's cursor that
// belong to slot S become unreachable. Rather than evicting them (which
// would also blow away other slots' txns recorded in the same time
// window), we mark them obsolete. The walk helpers skip obsolete entries
// in both directions; the storage gets reclaimed naturally when those
// ring slots reach the global tail and evict.
#define UNDO_TXN_OBSOLETE 0x08u

}  // namespace

// =============================================================================
// Shared ring + per-slot cursors
// =============================================================================
//
// Earlier iterations of this file kept a separate ring per slot, which
// either (a) wasted PSRAM by allocating identical-sized rings even for
// untouched slots, or (b) blew up at runtime when 16 KB allocations
// failed against a fragmented arena. Both modes ran into the same wall:
// rigid per-slot quotas don't match how people use the device.
//
// The current model is dead-simple: ONE big ring (ops + txns + blob
// arena), shared across all slots. Each transaction carries a 1-byte
// `slot` tag, and undo/redo for slot S walks the ring skipping txns
// that belong to other slots. Per-slot bookkeeping is just an 8-entry
// array of ring cursors (one size_t per slot) - 64 bytes total.
//
// The "split the space as we need" property falls out for free: a user
// who only ever touches slot 0 gets the whole ring for slot 0; a user
// who bounces between three slots shares the same ring proportionally
// to how much each slot mutated.
//
// Crucially, an undo on slot 1 still cannot replay ops recorded on
// slot 0 - the walk helpers refuse to step onto a foreign-slot txn.

namespace {

constexpr uint32_t TXN_AUTOCLOSE_MS = 200;

// Forward decl for SnapshotEntry; defined later with the snapshot helpers.
struct SnapshotEntry;

}  // namespace

namespace {

// Sizes - selected at init based on PSRAM availability.
size_t g_opCap = 0;
size_t g_txnCap = 0;
size_t g_blobCap = 0;

UndoOp* g_ops = nullptr;
UndoTxn* g_txns = nullptr;
uint8_t* g_blobs = nullptr;

// Monotonic indices that wrap around the ring. We track a head (newest)
// and tail (oldest) in the ring. The cursor is now per-slot (below).
size_t g_opHead = 0;        // next free slot
size_t g_opTail = 0;        // oldest valid op
size_t g_txnHead = 0;       // next free txn slot
size_t g_txnTail = 0;       // oldest valid txn
size_t g_blobUsed = 0;
uint32_t g_globalTxnId = 0;

// Per-slot cursor: ring index into g_txns. Equal to g_txnHead means
// "live, no redo pending"; less than head (in ring sense) means the
// slot has been undone and can redo. Initialized to g_txnHead (== 0)
// for every slot at boot - no slot has any history yet.
size_t g_slotCursor[NUM_SLOTS] = {0};

// Active slot for the open transaction (and for the auto-detect path).
// -1 means undo isn't initialized; otherwise tracks netSlot.
int g_activeSlot = -1;

// Open transaction state - exactly one txn open at a time, for the
// active slot. If a different slot starts recording, the open txn is
// closed first.
bool g_inTxn = false;
size_t g_openTxnIdx = 0;
uint32_t g_openTxnStartMs = 0;
char g_openTxnLabel[24] = {0};
uint8_t g_openTxnSource = UNDO_SRC_UNKNOWN;
uint8_t g_openTxnSlot = 0;
uint16_t g_openTxnOpStart = 0;

uint32_t g_lastOpMs = 0;

// Ingest depth: when > 0, all record* calls become no-ops. Bracketed by
// undoBeginIngest()/undoEndIngest() around slot loads, boot-time state
// restores, and other "system is loading state" regions. Without this,
// every addConnection() that runs during a slot file load gets recorded
// as a fresh user action against the destination slot, polluting the
// new slot's history with phantom entries.
int g_ingestDepth = 0;

// Lazy waypoint recompute. Marked stale on every commit; recomputed once
// when undoNextWaypoint is asked for a fresh answer.
uint32_t g_waypointsValidAt = 0;
bool     g_waypointsDirty = true;

// Stat counters
uint32_t g_undoCount = 0;
uint32_t g_redoCount = 0;
uint32_t g_dropCount = 0;

// Snapshot ring (single, not per-slot - clear-all snapshots are tagged
// with the slot they were taken on via the SnapshotEntry.slot field).
SnapshotEntry* g_snapshots = nullptr;
size_t g_snapshotCap = 0;
size_t g_snapshotHead = 0;
uint32_t g_lastSnapshotTxnId = 0;
uint32_t g_lastSnapshotMs = 0;

// ============================================================================
// Blob arena - stores full-state captures for non-symmetric ops (clear_all)
// and for periodic snapshots used as resync anchors.
//
// Format of each blob:
//   [u32 magic 'JLBS'][u32 size][u32 crc32][u32 reserved] -> 16-byte header
//   [u16 numBridges]
//   [Bridge × N]: { i16 n1, i16 n2, u32 color }                  // 8 B
//   [float dacVolts[4]]  // dac0, dac1, topRail, bottomRail
//   [u8 numCustomColors][NetColor × M]                           // optional
//
// For ~200 bridges + DAC state, a single blob is ~1.7 KB. Snapshot ring
// holds 16 of these on PSRAM (~30 KB), plenty for resync anchors.
// ============================================================================

constexpr uint32_t BLOB_MAGIC = 0x53424C4A;  // 'JLBS'

#pragma pack(push, 1)
struct BlobHeader {
    uint32_t magic;
    uint32_t size;     // body size only (not including header)
    uint32_t crc32;    // crc of body
    uint32_t reserved;
};

struct BlobBridge {
    int16_t n1;
    int16_t n2;
    uint32_t color;
};
#pragma pack(pop)

static_assert(sizeof(BlobHeader) == 16, "BlobHeader sized wrong");
static_assert(sizeof(BlobBridge) == 8, "BlobBridge sized wrong");

// Tiny CRC32 - matches the one in FileCache.cpp's journal code.
uint32_t blobCrc32(const uint8_t* data, size_t n) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
            crc = (crc >> 1) ^ (0xEDB88320u & -(int32_t)(crc & 1));
    }
    return ~crc;
}

// Serialize globalState's connections + DACs into a blob at *outOffset.
// Returns true on success and sets outOffset/outSize. Caller must hold no
// other locks.
bool blobAppendCurrentState(uint32_t* outOffset, uint32_t* outSize) {
    UNDBG("blobAppendCurrentState ENTER blobs=%p cap=%u used=%u",
          (void*)g_blobs, (unsigned)g_blobCap, (unsigned)g_blobUsed);
    if (!g_blobs || g_blobCap == 0) return false;

    // First compute the body size we need.
    int numBridges = globalState.connections.numBridges;
    UNDBG("  numBridges=%d MAX_BRIDGES=%d", numBridges, MAX_BRIDGES);
    if (numBridges > MAX_BRIDGES) numBridges = MAX_BRIDGES;
    if (numBridges < 0) numBridges = 0;  // sanity

    size_t bodySize = sizeof(uint16_t)                 // numBridges
                    + (size_t)numBridges * sizeof(BlobBridge)
                    + 4 * sizeof(float);               // dac voltages
    size_t totalSize = sizeof(BlobHeader) + bodySize;

    // Round up to 4 bytes for alignment.
    size_t aligned = (totalSize + 3u) & ~size_t(3u);
    UNDBG("  bodySize=%u total=%u aligned=%u",
          (unsigned)bodySize, (unsigned)totalSize, (unsigned)aligned);

    if (aligned > g_blobCap) {
        UNDBG("  aligned > cap, abort");
        return false;
    }
    if (g_blobUsed + aligned > g_blobCap) {
        UNDBG("blob arena full (used=%u cap=%u need=%u) - reset",
              (unsigned)g_blobUsed, (unsigned)g_blobCap, (unsigned)aligned);
        g_blobUsed = 0;
    }

    uint8_t* dst = g_blobs + g_blobUsed;
    BlobHeader* hdr = (BlobHeader*)dst;
    uint8_t* body = dst + sizeof(BlobHeader);
    UNDBG("  dst=%p hdr=%p body=%p", (void*)dst, (void*)hdr, (void*)body);

    uint8_t* p = body;
    UNDBG("  writing numBridges field");
    *(uint16_t*)p = (uint16_t)numBridges; p += sizeof(uint16_t);
    UNDBG("  copying %d bridges", numBridges);
    for (int i = 0; i < numBridges; i++) {
        BlobBridge bb;
        bb.n1 = (int16_t)globalState.connections.bridges[i][0];
        bb.n2 = (int16_t)globalState.connections.bridges[i][1];
        bb.color = globalState.connections.bridgeColors[i];
        memcpy(p, &bb, sizeof(bb));
        p += sizeof(bb);
    }
    UNDBG("  copying DAC voltages");
    float dacs[4] = {
        globalState.power.dac0,
        globalState.power.dac1,
        globalState.power.topRail,
        globalState.power.bottomRail,
    };
    memcpy(p, dacs, sizeof(dacs)); p += sizeof(dacs);

    UNDBG("  writing header + crc");
    hdr->magic = BLOB_MAGIC;
    hdr->size = (uint32_t)bodySize;
    hdr->reserved = 0;
    hdr->crc32 = blobCrc32(body, bodySize);

    if (outOffset) *outOffset = (uint32_t)g_blobUsed;
    if (outSize) *outSize = (uint32_t)aligned;
    g_blobUsed += aligned;

    UNDBG("blob append OK off=%u size=%u (bridges=%d)",
          *outOffset, *outSize, numBridges);
    return true;
}

// Restore globalState from a blob. Returns true if the blob is valid and
// state was restored. Caller is responsible for setting g_undoApplying so
// the restored mutations don't recurse into the undo log.
bool blobRestoreState(uint32_t offset, uint32_t size) {
    if (!g_blobs || offset + sizeof(BlobHeader) > g_blobCap) return false;
    BlobHeader* hdr = (BlobHeader*)(g_blobs + offset);
    if (hdr->magic != BLOB_MAGIC) {
        UNDBG("blob restore FAIL bad magic 0x%08X at off=%u",
              (unsigned)hdr->magic, (unsigned)offset);
        return false;
    }
    uint8_t* body = g_blobs + offset + sizeof(BlobHeader);
    uint32_t want = blobCrc32(body, hdr->size);
    if (want != hdr->crc32) {
        UNDBG("blob restore FAIL crc mismatch at off=%u", (unsigned)offset);
        return false;
    }

    uint8_t* p = body;
    uint16_t numBridges = *(uint16_t*)p; p += sizeof(uint16_t);

    String err;
    // Clear current connections without recording a new clear-all op.
    // We're already inside revert(), so g_undoApplying is true.
    globalState.connections.clear();

    for (uint16_t i = 0; i < numBridges; i++) {
        BlobBridge bb;
        memcpy(&bb, p, sizeof(bb)); p += sizeof(bb);
        globalState.addConnection(bb.n1, bb.n2, err);
        // Restore color hint
        if (globalState.connections.numBridges > 0) {
            int idx = globalState.connections.numBridges - 1;
            globalState.connections.bridgeColors[idx] = bb.color;
        }
    }

    float dacs[4];
    memcpy(dacs, p, sizeof(dacs)); p += sizeof(dacs);
    globalState.power.dac0 = dacs[0];
    globalState.power.dac1 = dacs[1];
    globalState.power.topRail = dacs[2];
    globalState.power.bottomRail = dacs[3];

    UNDBG("blob restore OK bridges=%u", (unsigned)numBridges);
    return true;
}

}  // namespace

volatile bool g_undoApplying = false;

// =============================================================================
// Allocation helpers - PSRAM if available, SRAM heap otherwise.
// =============================================================================

namespace {

void* undoAlloc(size_t bytes) {
    void* p = psram_alloc(bytes);
    if (!p) p = malloc(bytes);
    return p;
}

void undoFree(void* p) {
    if (!p) return;
    // Decide whether the block is in PSRAM (0x11000000-0x11800000) or SRAM.
    uintptr_t addr = reinterpret_cast<uintptr_t>(p);
    if (addr >= 0x11000000u && addr < 0x12000000u) {
        psram_free(p);
    } else {
        free(p);
    }
}

inline size_t opRingNext(size_t i) { return (i + 1) % g_opCap; }
inline size_t opRingDelta(size_t a, size_t b) {
    // # of ops from a to b in forward direction
    return (b + g_opCap - a) % g_opCap;
}
inline size_t txnRingNext(size_t i) { return (i + 1) % g_txnCap; }

inline bool ringEmpty(size_t head, size_t tail) { return head == tail; }
inline size_t ringCount(size_t head, size_t tail, size_t cap) {
    return (head + cap - tail) % cap;
}

// One-past-the-last-op slot for transaction `txnIdx`. Used both when
// trimming the redo tail (we're about to drop everything past it) and
// when evicting the oldest txn (the new op tail starts at the next op).
inline size_t opEndOfTxn(size_t txnIdx) {
    const UndoTxn& t = g_txns[txnIdx];
    return (t.firstOp + t.opCount) % g_opCap;
}

}  // namespace

// =============================================================================
// Internal: applying / reverting a single op
// =============================================================================

namespace {

// Forward declaration - the snapshot ring is defined further down. The
// in-RAM blob ref is always live (no cross-reboot recovery in this
// build), but we keep the lookup as a fallback for OP_CLEAR_ALL whose
// blob got clobbered by arena pressure since the op was recorded.
bool restoreFromSnapshotByTxnId(uint32_t snapTxnId);

void applyOp(const UndoOp& op) {
    UNDBG("applyOp type=%u", (unsigned)op.type);
    g_undoApplying = true;
    String err;
    switch (op.type) {
        case UNDO_OP_CONNECT: {
            globalState.addConnection(op.bridge.n1, op.bridge.n2, err);
            break;
        }
        case UNDO_OP_DISCONNECT: {
            globalState.removeConnection(op.bridge.n1, op.bridge.n2, err);
            break;
        }
        case UNDO_OP_DAC_SET: {
            // Channel encoding: 0=DAC0, 1=DAC1, 2=top rail, 3=bottom rail.
            if (op.dac.ch >= 2) {
                globalState.setRailVoltage(op.dac.ch == 2, op.dac.next);
            } else {
                globalState.setDacVoltage(op.dac.ch, op.dac.next);
            }
            break;
        }
        case UNDO_OP_GPIO_SET:
        case UNDO_OP_GPIO_DIR:
        case UNDO_OP_SLOT_SWITCH:
            // TODO: hook GPIO/slot mutation paths to record prev/next.
            break;
        case UNDO_OP_CLEAR_ALL:
            // Re-applying a clear means clear again. Connections are wiped
            // without re-recording (we're inside applyOp, g_undoApplying
            // is true, so the hook short-circuits).
            globalState.connections.clear();
            break;
        case UNDO_OP_BLOB_REPLACE:
            blobRestoreState(op.blob.blobOffset, op.blob.blobSize);
            break;
        default:
            break;
    }
    g_undoApplying = false;
}

void revertOp(const UndoOp& op) {
    UNDBG("revertOp type=%u", (unsigned)op.type);
    g_undoApplying = true;
    String err;
    switch (op.type) {
        case UNDO_OP_CONNECT: {
            globalState.removeConnection(op.bridge.n1, op.bridge.n2, err);
            break;
        }
        case UNDO_OP_DISCONNECT: {
            globalState.addConnection(op.bridge.n1, op.bridge.n2, err);
            break;
        }
        case UNDO_OP_DAC_SET: {
            if (op.dac.ch >= 2) {
                globalState.setRailVoltage(op.dac.ch == 2, op.dac.prev);
            } else {
                globalState.setDacVoltage(op.dac.ch, op.dac.prev);
            }
            break;
        }
        case UNDO_OP_CLEAR_ALL:
        case UNDO_OP_BLOB_REPLACE:
            // The in-arena blob ref is always live - no flash recovery.
            // If it failed validation the op is unrecoverable; leave
            // state alone so history walking past this point still works.
            if (!blobRestoreState(op.blob.blobOffset, op.blob.blobSize)) {
                UNDBG("revert CLEAR_ALL: blob restore failed at off=%u",
                      (unsigned)op.blob.blobOffset);
            }
            break;
        case UNDO_OP_GPIO_SET:
        case UNDO_OP_GPIO_DIR:
        case UNDO_OP_SLOT_SWITCH:
            break;
        default:
            break;
    }
    g_undoApplying = false;
}

// Re-apply or revert an entire transaction (range of ops) given direction.
// dir = +1 forward (redo), -1 backward (undo).
void applyTxn(const UndoTxn& txn, int dir) {
    UNDBG("applyTxn dir=%d firstOp=%u opCount=%u", dir, (unsigned)txn.firstOp, (unsigned)txn.opCount);
    if (txn.opCount == 0) return;
    if (dir > 0) {
        for (uint16_t i = 0; i < txn.opCount; i++) {
            size_t ringIdx = (txn.firstOp + i) % g_opCap;
            applyOp(g_ops[ringIdx]);
        }
    } else {
        for (int i = (int)txn.opCount - 1; i >= 0; i--) {
            size_t ringIdx = (txn.firstOp + i) % g_opCap;
            revertOp(g_ops[ringIdx]);
        }
    }
    UNDBG("applyTxn DONE");
}

}  // namespace

// =============================================================================
// Decimation - mark waypoints based on age tier
// =============================================================================

namespace {

void recomputeWaypoints() {
    UNDBG("recomputeWaypoints ENTER cap=%u head=%u tail=%u",
          (unsigned)g_txnCap, (unsigned)g_txnHead, (unsigned)g_txnTail);
    // Position 0 = head; bigger positive = older.
    bool small = (g_txnCap < 256);
    size_t tierBoundaries[8];
    size_t tierSpacing[8];
    int nTiers;
    if (small) {
        nTiers = 3;
        tierBoundaries[0] = 16;  tierSpacing[0] = 1;
        tierBoundaries[1] = 48;  tierSpacing[1] = 2;
        tierBoundaries[2] = (size_t)-1; tierSpacing[2] = 4;
    } else {
        nTiers = 6;
        tierBoundaries[0] = 32;   tierSpacing[0] = 1;
        tierBoundaries[1] = 96;   tierSpacing[1] = 2;
        tierBoundaries[2] = 224;  tierSpacing[2] = 4;
        tierBoundaries[3] = 480;  tierSpacing[3] = 8;
        tierBoundaries[4] = 992;  tierSpacing[4] = 16;
        tierBoundaries[5] = (size_t)-1; tierSpacing[5] = 32;
    }
    size_t txnCount = ringCount(g_txnHead, g_txnTail, g_txnCap);
    UNDBG("recomputeWaypoints txnCount=%u", (unsigned)txnCount);
    for (size_t back = 0; back < txnCount; back++) {
        size_t idx = (g_txnHead + g_txnCap - 1 - back) % g_txnCap;
        UndoTxn& t = g_txns[idx];
        size_t spacing = tierSpacing[nTiers - 1];
        for (int i = 0; i < nTiers; i++) {
            if (back < tierBoundaries[i]) { spacing = tierSpacing[i]; break; }
        }
        if (back % spacing == 0) t.flags |= UNDO_TXN_WAYPOINT;
        else                     t.flags &= ~UNDO_TXN_WAYPOINT;
    }
    UNDBG("recomputeWaypoints DONE");
}

}  // namespace

// =============================================================================
// Snapshot ring storage - declared here so undoInit can call into it. The
// triggering policy (undoMaybeTakeSnapshot) and public C-linkage helpers
// live further down.
// =============================================================================

namespace {

constexpr int SNAPSHOT_RING_DEFAULT = 16;     // PSRAM mode
constexpr int SNAPSHOT_RING_SRAM = 0;         // SRAM-only: no in-RAM ring
// (previously: periodic SNAPSHOT_INTERVAL_TXNS / _MS triggers - removed.
// undoMaybeTakeSnapshot now only fires on explicit / slot_switch /
// clear_all reasons to avoid churn during probe sessions.)

struct SnapshotEntry {
    uint32_t blobOffset;
    uint32_t blobSize;
    uint32_t txnIndex;
    uint32_t timestampMs;
    uint8_t  slot;       // owning slot - clear-all restore is per-slot
    bool     valid;
};

// Cross-reboot helper: find the snapshot matching `snapTxnId` in the
// ring and restore globalState from its blob. Returns true on success.
// Called by revertOp(OP_CLEAR_ALL) when the in-arena blob ref is stale.
bool restoreFromSnapshotByTxnId(uint32_t snapTxnId) {
    if (!g_snapshots || g_snapshotCap == 0) return false;
    for (size_t i = 0; i < g_snapshotCap; i++) {
        if (g_snapshots[i].valid && g_snapshots[i].txnIndex == snapTxnId) {
            return blobRestoreState(g_snapshots[i].blobOffset,
                                     g_snapshots[i].blobSize);
        }
    }
    return false;
}

void snapshotInit() {
    bool havePsram = psram_available();
    // 16 entries on PSRAM, 0 on SRAM. The ring is shared across slots -
    // each entry remembers which slot it captured so clear-all restore
    // doesn't get confused by snapshots from other slots.
    g_snapshotCap = havePsram ? 16 : 0;
    if (g_snapshotCap == 0) {
        g_snapshots = nullptr;
        return;
    }
    g_snapshots = (SnapshotEntry*)undoAlloc(g_snapshotCap * sizeof(SnapshotEntry));
    if (g_snapshots) memset(g_snapshots, 0, g_snapshotCap * sizeof(SnapshotEntry));
}

void takeSnapshot(const char* reason) {
    UNDBG("takeSnapshot ENTER reason=%s snapshots=%p cap=%u",
          reason, (void*)g_snapshots, (unsigned)g_snapshotCap);
    if (!g_snapshots || g_snapshotCap == 0) return;
    uint32_t off = 0, size = 0;
    if (!blobAppendCurrentState(&off, &size)) {
        UNDBG("snapshot capture FAILED (%s)", reason);
        return;
    }
    UNDBG("takeSnapshot blob ok, populating entry slot=%u", (unsigned)g_snapshotHead);
    SnapshotEntry& s = g_snapshots[g_snapshotHead];
    s.blobOffset = off;
    s.blobSize = size;
    s.txnIndex = g_globalTxnId;
    s.timestampMs = millis();
    s.slot = (g_activeSlot >= 0) ? (uint8_t)g_activeSlot : 0;
    s.valid = true;
    g_snapshotHead = (g_snapshotHead + 1) % g_snapshotCap;
    g_lastSnapshotTxnId = g_globalTxnId;
    g_lastSnapshotMs = s.timestampMs;
    UNDBG("snapshot captured (%s) slot=%u txn=%u",
          reason, (unsigned)((g_snapshotHead + g_snapshotCap - 1) % g_snapshotCap),
          (unsigned)g_globalTxnId);
    UNDBG("takeSnapshot DONE");
}

}  // namespace

// =============================================================================
// Public lifecycle
// =============================================================================

// =============================================================================
// Slot-aware ring helpers
// =============================================================================
// The ring is shared, so undo/redo for slot S must skip txns that belong
// to other slots and txns that we marked obsolete (via the redo-trim
// in undoBeginTxn). These walks are O(ringSize) worst case, but the
// constant is tiny and they only run on user-pressed undo / redo.

namespace {

// Is this txn slot a candidate for slot S's history?
inline bool txnBelongsToSlot(const UndoTxn& t, int slot) {
    if (t.opCount == 0) return false;
    if (t.flags & UNDO_TXN_OBSOLETE) return false;
    return t.slot == (uint8_t)slot;
}

// Walk backward from cursor (exclusive) toward tail; return ring index of
// the first txn matching slot S, or SIZE_MAX if none.
size_t findPrevSlotTxn(int slot, size_t cursor) {
    if (g_txnHead == g_txnTail) return SIZE_MAX;  // empty
    if (cursor == g_txnTail) return SIZE_MAX;     // already at oldest
    size_t pos = cursor;
    while (pos != g_txnTail) {
        pos = (pos + g_txnCap - 1) % g_txnCap;
        if (txnBelongsToSlot(g_txns[pos], slot)) return pos;
        if (pos == g_txnTail) break;
    }
    return SIZE_MAX;
}

// Walk forward from cursor (inclusive) toward head; return ring index of
// the first txn matching slot S, or SIZE_MAX if none.
size_t findNextSlotTxn(int slot, size_t cursor) {
    if (g_txnHead == g_txnTail) return SIZE_MAX;
    size_t pos = cursor;
    while (pos != g_txnHead) {
        if (txnBelongsToSlot(g_txns[pos], slot)) return pos;
        pos = (pos + 1) % g_txnCap;
    }
    return SIZE_MAX;
}

// Snap a slot's cursor back into [tail, head] if eviction has dragged
// the global tail past it. If the cursor was pointing at an evicted txn
// it loses meaning; treat that as "you're now at the oldest end of your
// (now-shrunken) history." Cheap O(1) check via ringCount.
void clampSlotCursor(int slot) {
    size_t& c = g_slotCursor[slot];
    if (g_txnHead == g_txnTail) { c = g_txnHead; return; }
    // c is valid if the forward distance from tail to c is <= forward
    // distance from tail to head. Otherwise it's behind the new tail.
    size_t fromTailToCursor = (c + g_txnCap - g_txnTail) % g_txnCap;
    size_t fromTailToHead   = (g_txnHead + g_txnCap - g_txnTail) % g_txnCap;
    if (fromTailToCursor > fromTailToHead) c = g_txnTail;
}

}  // namespace

// =============================================================================
// Active-slot swap (lightweight - no allocation)
// =============================================================================

namespace {

// Make `slot` the active slot for new recordings. If a txn is open for a
// different slot, close it first so its ops land in the right history.
// No allocation - the ring is shared.
//
// We call undoEndTxn() directly here. It's a C-linkage public API but
// that's fine - the open txn is keyed off g_openTxnSlot, which was set
// when the txn opened, so closing here lands the ops under the correct
// (old) slot tag before we flip g_activeSlot to the new one.
void slotSwap(int slot) {
    if (slot < 0 || slot >= NUM_SLOTS) return;
    if (g_activeSlot == slot) return;
    if (g_inTxn) undoEndTxn();
    g_activeSlot = slot;
}

// Auto-detect: if `netSlot` differs from g_activeSlot, swap. Called at
// the top of every public API entry point.
inline void slotSwapIfNeeded() {
    if (g_activeSlot < 0) return;  // not initialized
    if (netSlot == g_activeSlot) return;
    if (netSlot < 0 || netSlot >= NUM_SLOTS) return;
    slotSwap(netSlot);
}

}  // namespace

// =============================================================================
// Public lifecycle
// =============================================================================

extern "C" {

void undoInit(void) {
    if (g_ops) return;  // already initialized

    bool havePsram = psram_available();
    if (havePsram) {
        // Default sizes: ops 16384, txns 4096, blobs 256KB. The ring is
        // shared across all 8 slots - each entry carries a 1-byte slot
        // tag so undo/redo only walks matching txns. No per-slot
        // allocation, no failure modes when slots come online.
        g_opCap = 16384;
        g_txnCap = 4096;
        g_blobCap = 256 * 1024;
    } else {
        g_opCap = 512;
        g_txnCap = 128;
        g_blobCap = 4 * 1024;
    }

    auto isPow2 = [](size_t n) { return n != 0 && (n & (n - 1)) == 0; };
    if (!isPow2(g_opCap) || !isPow2(g_txnCap)) {
        Serial.println("[Undo] FATAL: ring caps must be powers of two");
        g_opCap = g_txnCap = g_blobCap = 0;
        return;
    }

    g_ops = (UndoOp*)undoAlloc(g_opCap * sizeof(UndoOp));
    g_txns = (UndoTxn*)undoAlloc(g_txnCap * sizeof(UndoTxn));
    g_blobs = (uint8_t*)undoAlloc(g_blobCap);

    if (!g_ops || !g_txns || !g_blobs) {
        undoFree(g_ops); undoFree(g_txns); undoFree(g_blobs);
        g_ops = nullptr; g_txns = nullptr; g_blobs = nullptr;
        g_opCap = g_txnCap = g_blobCap = 0;
        Serial.println("[Undo] allocation failed - undo disabled");
        return;
    }

    memset(g_ops, 0, g_opCap * sizeof(UndoOp));
    memset(g_txns, 0, g_txnCap * sizeof(UndoTxn));
    memset(g_blobs, 0, g_blobCap);

    g_opHead = g_opTail = 0;
    g_txnHead = g_txnTail = 0;
    for (int i = 0; i < NUM_SLOTS; i++) g_slotCursor[i] = 0;
    g_blobUsed = 0;
    g_globalTxnId = 0;
    g_inTxn = false;

    g_activeSlot = (netSlot >= 0 && netSlot < NUM_SLOTS) ? netSlot : 0;

    Serial.printf("[Undo] init: %u ops + %u txns + %u KB blob shared across %d slots (%s, active=%d)\n",
        (unsigned)g_opCap, (unsigned)g_txnCap, (unsigned)(g_blobCap / 1024),
        NUM_SLOTS, havePsram ? "PSRAM" : "SRAM-only", g_activeSlot);

    snapshotInit();
    if (g_snapshots) {
        Serial.printf("[Undo] snapshot ring: %u entries\n", (unsigned)g_snapshotCap);
        takeSnapshot("boot");
    }

    // One-shot cleanup of artifacts from the old persistent-history
    // implementation. Best-effort - if the files don't exist or the FS
    // is busy, we silently move on.
    FatFS.remove("/undo.log");
    FatFS.remove("/undo.snap");
}

void undoShutdown(void) {
    if (g_inTxn) undoEndTxn();
    extern void fileCacheFlushNowAll(const char* reason);
    fileCacheFlushNowAll("shutdown");
}

void undoOnSlotSwitch(int newSlot) {
    if (g_activeSlot < 0) return;
    if (newSlot < 0 || newSlot >= NUM_SLOTS) return;
    slotSwap(newSlot);
}

void undoBeginIngest(void) {
    g_ingestDepth++;
}

void undoEndIngest(void) {
    if (g_ingestDepth > 0) {
        g_ingestDepth--;
        // When the outermost ingest region closes, also reset the
        // last-op timestamp so a stale auto-close from before the
        // ingest doesn't fire on the next user action.
        if (g_ingestDepth == 0) g_lastOpMs = millis();
    }
}

bool undoIsIngesting(void) { return g_ingestDepth > 0; }

bool undoTxnInProgress(void) {
    if (!g_ops) return false;
    slotSwapIfNeeded();
    return g_inTxn;
}

void undoBeginTxn(const char* label, UndoSource source) {
    if (!g_ops) return;
    if (g_undoApplying) return;
    slotSwapIfNeeded();
    if (g_inTxn) undoEndTxn();

    // Mark slot's redo tail obsolete. Once you take a new action on slot
    // S after having undone some history on slot S, the txns belonging
    // to slot S between cursor[S] and head are no longer reachable. Other
    // slots' txns in that ring range stay valid - they're independent
    // histories that shouldn't be touched by slot S's actions.
    clampSlotCursor(g_activeSlot);
    size_t cur = g_slotCursor[g_activeSlot];
    if (cur != g_txnHead) {
        size_t pos = cur;
        size_t marked = 0;
        while (pos != g_txnHead) {
            UndoTxn& t = g_txns[pos];
            if (t.opCount > 0 && t.slot == (uint8_t)g_activeSlot &&
                !(t.flags & UNDO_TXN_OBSOLETE)) {
                t.flags |= UNDO_TXN_OBSOLETE;
                marked++;
            }
            pos = (pos + 1) % g_txnCap;
        }
        g_dropCount += (uint32_t)marked;
        g_waypointsDirty = true;
    }

    // If we're full, evict the oldest txn (and bump any per-slot cursor
    // that was pointing at the evicted entry to the new tail).
    if (txnRingNext(g_txnHead) == g_txnTail) {
        size_t evicting = g_txnTail;
        g_opTail = opEndOfTxn(g_txnTail);
        g_txnTail = txnRingNext(g_txnTail);
        for (int s = 0; s < NUM_SLOTS; s++) {
            if (g_slotCursor[s] == evicting) g_slotCursor[s] = g_txnTail;
        }
        g_waypointsDirty = true;
    }

    g_openTxnIdx = g_txnHead;
    g_openTxnStartMs = millis();
    g_openTxnOpStart = (uint16_t)g_opHead;
    g_openTxnSource = (uint8_t)source;
    g_openTxnSlot = (uint8_t)((g_activeSlot >= 0) ? g_activeSlot : 0);
    if (label) {
        strncpy(g_openTxnLabel, label, sizeof(g_openTxnLabel) - 1);
        g_openTxnLabel[sizeof(g_openTxnLabel) - 1] = '\0';
    } else {
        g_openTxnLabel[0] = '\0';
    }
    g_inTxn = true;
    g_lastOpMs = g_openTxnStartMs;
    UNDBG("beginTxn idx=%u slot=%u src=%u label='%s'",
          (unsigned)g_openTxnIdx, (unsigned)g_openTxnSlot,
          (unsigned)source, label ? label : "");
}

void undoEndTxn(void) {
    if (!g_ops || !g_inTxn) return;
    UNDBG("endTxn ENTER idx=%u opHead=%u opStart=%u txnHead=%u txnTail=%u",
          (unsigned)g_openTxnIdx, (unsigned)g_opHead, (unsigned)g_openTxnOpStart,
          (unsigned)g_txnHead, (unsigned)g_txnTail);
    UndoTxn& t = g_txns[g_openTxnIdx];
    t.startMs = g_openTxnStartMs;
    t.firstOp = (uint32_t)g_openTxnOpStart;
    size_t added = (g_opHead >= g_openTxnOpStart)
                       ? (g_opHead - g_openTxnOpStart)
                       : (g_opHead + g_opCap - g_openTxnOpStart);
    t.opCount = (uint16_t)added;
    t.flags = UNDO_TXN_COMMITTED;
    if (g_openTxnSource != UNDO_SRC_INTERNAL) t.flags |= UNDO_TXN_USER_INITIATED;
    t.source = g_openTxnSource;
    t.slot = g_openTxnSlot;
    strncpy(t.label, g_openTxnLabel, sizeof(t.label) - 1);
    t.label[sizeof(t.label) - 1] = '\0';

    UNDBG("endTxn opCount=%u", (unsigned)t.opCount);
    if (t.opCount > 0) {
        g_globalTxnId++;
        t.globalId = g_globalTxnId;
        g_txnHead = txnRingNext(g_txnHead);
        // Cursor for the slot we just committed to advances to the new
        // head (txn was appended at slot[g_openTxnSlot]'s "live" end).
        g_slotCursor[g_openTxnSlot] = g_txnHead;
        g_waypointsDirty = true;
        undoMaybeTakeSnapshot("periodic");
        UNDBG("endTxn DONE");
    }
    g_inTxn = false;
}

static void maybeAutoClose() {
    if (g_inTxn && (millis() - g_lastOpMs) > TXN_AUTOCLOSE_MS) {
        undoEndTxn();
    }
}

static void appendOp(const UndoOp& op) {
    if (!g_ops) return;
    if (g_undoApplying) return;
    UNDBG("appendOp type=%u inTxn=%d", (unsigned)op.type, (int)g_inTxn);
    if (!g_inTxn) {
        undoBeginTxn(nullptr, UNDO_SRC_UNKNOWN);
    }
    // If full, evict oldest txn (which advances op tail and bumps any
    // per-slot cursor that was pointing at the evicted txn).
    if (opRingNext(g_opHead) == g_opTail) {
        if (!ringEmpty(g_txnHead, g_txnTail)) {
            size_t evicting = g_txnTail;
            g_opTail = opEndOfTxn(g_txnTail);
            g_txnTail = txnRingNext(g_txnTail);
            for (int s = 0; s < NUM_SLOTS; s++) {
                if (g_slotCursor[s] == evicting) g_slotCursor[s] = g_txnTail;
            }
            g_waypointsDirty = true;
        } else {
            g_opTail = opRingNext(g_opTail);
        }
    }
    g_ops[g_opHead] = op;
    g_ops[g_opHead].txnIdx = (uint16_t)(g_openTxnIdx & 0xFFFF);
    g_opHead = opRingNext(g_opHead);
    g_lastOpMs = millis();
}

// Each public record function opens a fresh transaction with a label that
// matches the user's intent, appends one op, and immediately closes it.
// This gives PER-ACTION undo granularity: each connect/disconnect/DAC
// change is its own undo step, even when they happen in rapid succession
// during a probe mode session.
//
// If a caller needs to bundle multiple ops as a single undo unit (e.g. a
// Python script doing 5 connects atomically), it can call undoBeginTxn /
// undoEndTxn explicitly around the operations - the per-record helpers
// detect an already-open external transaction and skip the close so they
// just contribute their op to the bundle.
//
// Note: we still call appendOp first (which auto-opens an implicit txn if
// none is open) and only close it if WE were the ones who opened it.
//
// We also slotSwapIfNeeded() here so that any record* call lands on the
// correct slot's history (the active slot is always == netSlot at the
// moment of recording).
//
// Ingest gate: if a slot file is loading, all the addConnection /
// setRailVoltage calls that run as part of that load are NOT user
// actions and must not enter the history. The slot loader brackets
// its region with undoBeginIngest()/undoEndIngest(); we short-circuit
// here while that's active.
static void recordOneAction(const UndoOp& op, const char* label) {
    if (!g_ops) return;
    if (g_ingestDepth > 0) return;
    slotSwapIfNeeded();
    bool weOpened = !g_inTxn;
    if (weOpened) undoBeginTxn(label, UNDO_SRC_PROBE);
    appendOp(op);
    if (weOpened) undoEndTxn();
}

// Connections that are auto-managed by the system (routableBufferPower
// re-attaches them on boot and whenever the probe needs power) should
// NOT pollute the user-visible undo history - the user didn't make them
// and can't meaningfully reason about undoing them.
static inline bool isAutoManagedBridge(int n1, int n2) {
    auto isBufferPair = [](int a, int b) {
        return (a == ROUTABLE_BUFFER_IN || a == ROUTABLE_BUFFER_OUT) &&
               (b == DAC0 || b == DAC1);
    };
    return isBufferPair(n1, n2) || isBufferPair(n2, n1);
}

// definesToChar() returns a const char* that, for numeric fallback (raw
// breadboard rows etc.), points into a SHARED static buffer. Calling it
// twice in one expression clobbers the first result. Always copy the
// first name out before fetching the second.
static void undoNodePairLabel(char* out, size_t outSize, const char* prefix,
                              int node1, int node2) {
    char n1[12];
    const char* p1 = definesToChar(node1, 0);
    strncpy(n1, p1 ? p1 : "?", sizeof(n1) - 1);
    n1[sizeof(n1) - 1] = '\0';
    const char* p2 = definesToChar(node2, 0);
    const char* n2 = (p2 && p2[0]) ? p2 : "?";
    snprintf(out, outSize, "%s %s-%s", prefix, n1, n2);
}

void undoRecordConnect(int node1, int node2, uint32_t color) {
    UNDBG("recordConnect %d-%d color=0x%08X", node1, node2, (unsigned)color);
    if (isAutoManagedBridge(node1, node2)) {
        UNDBG("recordConnect skipped (auto-managed DAC<->BUF)");
        return;
    }
    UndoOp op = {};
    op.type = UNDO_OP_CONNECT;
    op.bridge.n1 = (int16_t)node1;
    op.bridge.n2 = (int16_t)node2;
    op.bridge.color = color;
    char lbl[24];
    undoNodePairLabel(lbl, sizeof(lbl), "connect", node1, node2);
    recordOneAction(op, lbl);
}

void undoRecordDisconnect(int node1, int node2, uint32_t color) {
    UNDBG("recordDisconnect %d-%d color=0x%08X", node1, node2, (unsigned)color);
    if (isAutoManagedBridge(node1, node2)) {
        UNDBG("recordDisconnect skipped (auto-managed DAC<->BUF)");
        return;
    }
    UndoOp op = {};
    op.type = UNDO_OP_DISCONNECT;
    op.bridge.n1 = (int16_t)node1;
    op.bridge.n2 = (int16_t)node2;
    op.bridge.color = color;
    char lbl[24];
    undoNodePairLabel(lbl, sizeof(lbl), "disconnect", node1, node2);
    recordOneAction(op, lbl);
}

void undoRecordClearAll(void) {
    UNDBG("recordClearAll ENTER bridges=%d", globalState.connections.numBridges);
    // Capture the PRE-clear state into the snapshot ring so we can revert
    // this clear-all later. The OP_CLEAR_ALL record reuses that blob's
    // arena offset as its restore source - no double allocation.
    undoForceSnapshot("clear_all");

    UndoOp op = {};
    op.type = UNDO_OP_CLEAR_ALL;
    // Latest snapshot owns the blob we just captured.
    SnapshotEntry* sn = nullptr;
    if (g_snapshots && g_snapshotCap > 0) {
        size_t lastIdx = (g_snapshotHead + g_snapshotCap - 1) % g_snapshotCap;
        if (g_snapshots[lastIdx].valid) sn = &g_snapshots[lastIdx];
    }
    if (sn) {
        op.blob.blobOffset = sn->blobOffset;
        op.blob.blobSize = sn->blobSize;
        op.blob.snapTxnId = sn->txnIndex;
        UNDBG("recordClearAll using snapshot off=%u sz=%u tid=%u",
              (unsigned)sn->blobOffset, (unsigned)sn->blobSize,
              (unsigned)sn->txnIndex);
    } else {
        // No snapshot available (very small SRAM build or alloc fail).
        // Fall back to a fresh blob copy so in-RAM undo still works.
        uint32_t blobOff = 0, blobSize = 0;
        if (blobAppendCurrentState(&blobOff, &blobSize)) {
            op.blob.blobOffset = blobOff;
            op.blob.blobSize = blobSize;
            op.blob.snapTxnId = 0;
        } else {
            UNDBG("recordClearAll: no snapshot AND blob alloc failed");
        }
    }
    recordOneAction(op, "clear all");
}

void undoRecordDacSet(int channel, float prevVolts, float nextVolts) {
    UndoOp op = {};
    op.type = UNDO_OP_DAC_SET;
    op.dac.ch = (uint8_t)channel;
    op.dac.prev = prevVolts;
    op.dac.next = nextVolts;
    char lbl[24];
    // Channel encoding: 0=DAC0, 1=DAC1, 2=top rail, 3=bottom rail.
    const char* name;
    switch (channel) {
        case 0:  name = "DAC0";  break;
        case 1:  name = "DAC1";  break;
        case 2:  name = "top";   break;
        case 3:  name = "bot";   break;
        default: name = "?";     break;
    }
    snprintf(lbl, sizeof(lbl), "%s %.2fV", name, nextVolts);
    recordOneAction(op, lbl);
}

void undoRecordGpioSet(int pin, int prevVal, int nextVal) {
    UndoOp op = {};
    op.type = UNDO_OP_GPIO_SET;
    op.gpio.pin = (uint8_t)pin;
    op.gpio.prevVal = (uint8_t)prevVal;
    op.gpio.nextVal = (uint8_t)nextVal;
    char lbl[24];
    snprintf(lbl, sizeof(lbl), "GPIO%d=%d", pin, nextVal);
    recordOneAction(op, lbl);
}

void undoRecordGpioDir(int pin, int prevDir, int nextDir) {
    UndoOp op = {};
    op.type = UNDO_OP_GPIO_DIR;
    op.gpio.pin = (uint8_t)pin;
    op.gpio.prevDir = (uint8_t)prevDir;
    op.gpio.nextVal = (uint8_t)nextDir;
    char lbl[24];
    snprintf(lbl, sizeof(lbl), "GPIO%d dir=%d", pin, nextDir);
    recordOneAction(op, lbl);
}

void undoRecordSlotSwitch(int prevSlot, int nextSlot) {
    UndoOp op = {};
    op.type = UNDO_OP_SLOT_SWITCH;
    op.slot.prev = (uint8_t)prevSlot;
    op.slot.next = (uint8_t)nextSlot;
    char lbl[24];
    snprintf(lbl, sizeof(lbl), "slot %d->%d", prevSlot, nextSlot);
    recordOneAction(op, lbl);
}

// =============================================================================
// User-facing controls
// =============================================================================

// Eagerly close any open transaction if its 200ms quiescence window has
// elapsed. Called from canUndo/canRedo so the user-visible state
// reflects mutations that happened a moment ago without requiring a
// follow-up mutation to trigger maybeAutoClose.
static void undoFinalizePendingIfStale(void) {
    if (g_ops && g_inTxn && (millis() - g_lastOpMs) > TXN_AUTOCLOSE_MS) {
        undoEndTxn();
    }
}

bool undoCanUndo(void) {
    if (!g_ops) return false;
    slotSwapIfNeeded();
    undoFinalizePendingIfStale();
    if (g_activeSlot < 0) return false;
    clampSlotCursor(g_activeSlot);
    // Are there any non-obsolete txns belonging to this slot strictly
    // before the slot's cursor? (findPrevSlotTxn does this exact walk.)
    return findPrevSlotTxn(g_activeSlot, g_slotCursor[g_activeSlot]) != SIZE_MAX;
}

bool undoCanRedo(void) {
    if (!g_ops) return false;
    slotSwapIfNeeded();
    undoFinalizePendingIfStale();
    if (g_activeSlot < 0) return false;
    clampSlotCursor(g_activeSlot);
    return findNextSlotTxn(g_activeSlot, g_slotCursor[g_activeSlot]) != SIZE_MAX;
}

// Push reverted state to hardware: bridges -> crosspoints + LEDs, plus
// rails / DACs (cheap: 4 small I2C writes). In-memory mutation alone
// doesn't reroute switches, refresh LEDs, or re-issue DAC voltages.
static void undoCommitToHardware(void) {
    refreshConnections(-1, 1, 0);
    setRailsAndDACs(0);
}

bool undoUndo(void) {
    if (!undoCanUndo()) return false;
    UNDBG("UNDO start cursor=%u (slot %d)",
          (unsigned)g_slotCursor[g_activeSlot], g_activeSlot);
    if (g_inTxn) undoEndTxn();
    size_t prev = findPrevSlotTxn(g_activeSlot, g_slotCursor[g_activeSlot]);
    if (prev == SIZE_MAX) return false;
    UndoTxn& t = g_txns[prev];
    applyTxn(t, -1);
    g_slotCursor[g_activeSlot] = prev;
    g_undoCount++;
    undoCommitToHardware();
    UNDBG("UNDO done cursor=%u", (unsigned)g_slotCursor[g_activeSlot]);
    return true;
}

bool undoRedo(void) {
    if (!undoCanRedo()) return false;
    UNDBG("REDO start cursor=%u (slot %d)",
          (unsigned)g_slotCursor[g_activeSlot], g_activeSlot);
    size_t next = findNextSlotTxn(g_activeSlot, g_slotCursor[g_activeSlot]);
    if (next == SIZE_MAX) return false;
    UndoTxn& t = g_txns[next];
    applyTxn(t, +1);
    g_slotCursor[g_activeSlot] = txnRingNext(next);
    g_redoCount++;
    undoCommitToHardware();
    UNDBG("REDO done cursor=%u", (unsigned)g_slotCursor[g_activeSlot]);
    return true;
}

namespace {
// Count slot's reachable txns in [tail, head). Used by undoTotalTxns and
// position helpers. This is O(ring size) but only runs on user-visible
// queries, not during recording.
int countSlotTxns(int slot) {
    if (!g_ops) return 0;
    if (g_txnHead == g_txnTail) return 0;
    int n = 0;
    size_t pos = g_txnTail;
    while (pos != g_txnHead) {
        if (txnBelongsToSlot(g_txns[pos], slot)) n++;
        pos = (pos + 1) % g_txnCap;
    }
    return n;
}

// How many txns has slot S undone? Counts non-obsolete same-slot txns
// in [cursor, head). Position is reported as -that count.
int countSlotUndone(int slot, size_t cursor) {
    if (!g_ops) return 0;
    if (g_txnHead == g_txnTail) return 0;
    int n = 0;
    size_t pos = cursor;
    while (pos != g_txnHead) {
        if (txnBelongsToSlot(g_txns[pos], slot)) n++;
        pos = (pos + 1) % g_txnCap;
    }
    return n;
}
}  // namespace

int undoPosition(void) {
    if (!g_ops || g_activeSlot < 0) return 0;
    clampSlotCursor(g_activeSlot);
    return -countSlotUndone(g_activeSlot, g_slotCursor[g_activeSlot]);
}

int undoTotalTxns(void) {
    if (!g_ops || g_activeSlot < 0) return 0;
    return countSlotTxns(g_activeSlot);
}

const char* undoLabelAt(int relativeOffset) {
    if (!g_ops || g_activeSlot < 0) return "";
    // relativeOffset: 0 means the txn the next undo would consume
    //                (i.e. the most recent same-slot txn before cursor).
    // -1 means one before that. +1 means the txn the next redo would
    // apply (== same-slot txn at cursor, if any).
    clampSlotCursor(g_activeSlot);
    size_t cursor = g_slotCursor[g_activeSlot];
    if (relativeOffset >= 1) {
        // walk forward `relativeOffset` same-slot txns starting from cursor
        size_t pos = cursor;
        for (int i = 0; i < relativeOffset; i++) {
            size_t found = findNextSlotTxn(g_activeSlot, pos);
            if (found == SIZE_MAX) return "";
            pos = (found + 1) % g_txnCap;
            if (i == relativeOffset - 1) {
                return g_txns[found].label[0] ? g_txns[found].label : "(unlabeled)";
            }
        }
        return "";
    } else {
        // walk backward (1 - relativeOffset) same-slot txns
        int steps = 1 - relativeOffset;  // relativeOffset=0 -> 1 step back
        size_t pos = cursor;
        for (int i = 0; i < steps; i++) {
            size_t found = findPrevSlotTxn(g_activeSlot, pos);
            if (found == SIZE_MAX) return "";
            pos = found;
            if (i == steps - 1) {
                return g_txns[found].label[0] ? g_txns[found].label : "(unlabeled)";
            }
        }
        return "";
    }
}

bool undoScrubTo(int targetPosition) {
    if (!g_ops || g_activeSlot < 0) return false;
    int cur = undoPosition();
    while (cur != targetPosition) {
        if (cur > targetPosition) {
            if (!undoUndo()) return false;
        } else {
            if (!undoRedo()) return false;
        }
        cur = undoPosition();
    }
    return true;
}

int undoNextWaypoint(int fromPosition, int dir) {
    if (!g_ops || g_activeSlot < 0 || dir == 0) return fromPosition;
    if (g_waypointsDirty) {
        recomputeWaypoints();
        g_waypointsDirty = false;
        g_waypointsValidAt = g_globalTxnId;
    }
    // Walk same-slot waypoints. fromPosition is negative (== -back, where
    // back is how many same-slot txns have been undone). We step through
    // same-slot txns in the requested direction, returning when we hit one
    // that is flagged as a waypoint.
    int back = -fromPosition;
    int total = undoTotalTxns();
    if (total <= 0) return fromPosition;
    if (dir > 0) {
        // older: walk backward through same-slot txns
        clampSlotCursor(g_activeSlot);
        size_t pos = g_slotCursor[g_activeSlot];
        // first move to "current position" - back same-slot txns from head
        for (int i = 0; i < back; i++) {
            size_t f = findPrevSlotTxn(g_activeSlot, pos);
            if (f == SIZE_MAX) return -back;
            pos = f;
        }
        while (back < total - 1) {
            back++;
            size_t f = findPrevSlotTxn(g_activeSlot, pos);
            if (f == SIZE_MAX) return -(total - 1);
            pos = f;
            if (g_txns[f].flags & UNDO_TXN_WAYPOINT) return -back;
        }
        return -(total - 1);
    } else {
        // newer: walk forward through same-slot txns
        clampSlotCursor(g_activeSlot);
        size_t pos = g_slotCursor[g_activeSlot];
        for (int i = 0; i < back; i++) {
            size_t f = findPrevSlotTxn(g_activeSlot, pos);
            if (f == SIZE_MAX) break;
            pos = f;
        }
        while (back > 0) {
            size_t f = findNextSlotTxn(g_activeSlot, pos);
            if (f == SIZE_MAX) return 0;
            pos = (f + 1) % g_txnCap;
            back--;
            if (g_txns[f].flags & UNDO_TXN_WAYPOINT) return -back;
        }
        return 0;
    }
}

// =============================================================================
// Snapshot policy / public helpers (storage + capture defined above)
// =============================================================================

bool undoForceSnapshot(const char* reason) {
    if (!g_ops || !g_snapshots) return false;
    takeSnapshot(reason ? reason : "forced");
    return true;
}

int undoSnapshotCount(void) {
    if (!g_snapshots) return 0;
    int valid = 0;
    for (size_t i = 0; i < g_snapshotCap; i++) if (g_snapshots[i].valid) valid++;
    return valid;
}

void undoMaybeTakeSnapshot(const char* reason) {
    if (!g_ops || !g_snapshots) return;

    // Snapshots back the non-symmetric ops (currently just OP_CLEAR_ALL)
    // and provide resync anchors for scrubbing. We used to also snapshot
    // every N transactions / every M ms as cheap "safety nets" - but
    // those triggers fired during normal probe work and added churn.
    // Now we only snapshot when an explicit caller asks for it:
    //   * undoRecordClearAll() calls undoForceSnapshot("clear_all")
    //   * Slot switches call this with reason == "slot_switch"
    //   * Anyone passing reason == "explicit" forces one
    if (!reason) return;
    bool shouldSnapshot = (strcmp(reason, "explicit") == 0 ||
                           strcmp(reason, "slot_switch") == 0 ||
                           strcmp(reason, "clear_all") == 0);
    if (shouldSnapshot) {
        takeSnapshot(reason);
    }
}

// --- OLED toast helpers ------------------------------------------------------
// Format: "undo\n<label>" or "redo\n<label>" at text size 1, centered.
// Falls back to short tag if no label / OLED missing. Cheap call - the
// OLED auto-recovers when Highlighting::service() next repaints.
const char* undoPeekUndoLabel(void) {
    // The next undo will consume the txn immediately behind the cursor:
    //   undoLabelAt(0) returns the txn at cursor-1 (per existing impl).
    return undoLabelAt(0);
}

const char* undoPeekRedoLabel(void) {
    // The next redo will reapply the txn at the cursor itself.
    return undoLabelAt(+1);
}

void undoFlashLogo(uint32_t durationMs) {
    uint32_t target = millis() + durationMs;
    // Only extend the window - never shorten it. A long-running hold should
    // not be cut short by a single-shot flash from undoToast().
    if (target > undoActivityUntil) undoActivityUntil = target;
}

void undoToast(bool isRedo, const char* label) {
    const char* tag = isRedo ? "redo" : "undo";
    // Match the probeMode entry banner style: blank line above and
    // below, tab-indented body. Standalone block so probe-mode prints
    // and prompts before/after don't smear into the toast line.

    if (oled.isConnected()) {
    // Two-line OLED layout, both rows centered, rendered at a fixed 7pt
    // font by clearPrintShowSmall (no auto-fit -> stable size regardless
    // of label width):
    //   undo
    //   connect 4-8       (or "disconnect 50-46", "clear all", ...)
    char buf[96];
    if (label && label[0]) {
        snprintf(buf, sizeof(buf), "%s\n%s", tag, label);
    } else {
        snprintf(buf, sizeof(buf), "%s", tag);
    }
    // Snapshot-aware hold: capture the pre-toast framebuffer FIRST,
    // then paint+priority-flush the toast. oledHoldBegin saves the
    // current live framebuffer (the background that was visible before
    // this toast) into a shadow buffer and arms the post-flush latch.
    // clearPrintShowSmall paints the toast over the live framebuffer,
    // priority-flushes it to the panel, and then restores the shadow
    // back into the live framebuffer. Any probe-mode / clearHighlighting
    // / status writes that happen during the hold window therefore
    // accumulate against the pre-toast background rather than smearing
    // on top of toast pixels. When the hold expires (~1 s later),
    // oledPeriodic() flushes the live framebuffer once - the panel
    // transitions cleanly from the toast to the up-to-date underlying
    // content with no toast residue.
    oled.oledHoldBegin(800);
    oled.clearPrintShowSmall(buf, true, true);
}

    const int disColor = 202;
    const int conColor = 45;
    int actionColor = -1;
    if (label && label[0]) {
        if (strstr(label, "disconnect") != NULL) {
            actionColor = disColor;
        } else if (strstr(label, "connect") != NULL) {
            actionColor = conColor;
        }
    }


    if (isRedo) {
        changeTerminalColor(122, false, &Serial);    
        Serial.print("\r\t[REDO]");
        changeTerminalColor(actionColor, false, &Serial);
        Serial.printf(" %s\033[0K", (label && label[0]) ? label : "<unlabeled>");
        changeTerminalColor(-1, true, &Serial);
    } else {
        changeTerminalColor(209, false, &Serial);
        Serial.print("\r\t[UNDO]");
        changeTerminalColor(actionColor, false, &Serial);
        Serial.printf(" %s\033[0K", (label && label[0]) ? label : "<unlabeled>");
        changeTerminalColor(-1, true, &Serial);
    }
    Serial.flush();    
    // Visual cue: logo turns yellow for ~600 ms (long enough to read the
    // OLED label and notice the color change without lingering).
    undoFlashLogo(500);

}

void undoDumpStatus(void) {
    if (!g_ops) { Serial.println("[Undo] disabled"); return; }
    slotSwapIfNeeded();
    Serial.printf("[Undo] active slot=%d  ops %u/%u  txns %u/%u  blob %u/%u\n",
        g_activeSlot,
        (unsigned)ringCount(g_opHead, g_opTail, g_opCap), (unsigned)g_opCap,
        (unsigned)ringCount(g_txnHead, g_txnTail, g_txnCap), (unsigned)g_txnCap,
        (unsigned)g_blobUsed, (unsigned)g_blobCap);
    Serial.printf("        active cursor %d (head=0)  total %d  undo=%u redo=%u drop=%u\n",
        undoPosition(), undoTotalTxns(),
        (unsigned)g_undoCount, (unsigned)g_redoCount, (unsigned)g_dropCount);
    if (g_snapshots) {
        int valid = 0;
        for (size_t i = 0; i < g_snapshotCap; i++) if (g_snapshots[i].valid) valid++;
        Serial.printf("        snapshots %d/%u  last txn=%u age=%lus\n",
            valid, (unsigned)g_snapshotCap,
            (unsigned)g_lastSnapshotTxnId,
            g_lastSnapshotMs ? (unsigned long)((millis() - g_lastSnapshotMs) / 1000) : 0UL);
    }
    // Per-slot summary - shared ring, just counts how many txns each
    // slot owns and where its cursor sits.
    Serial.println("        per-slot summary:");
    for (int i = 0; i < NUM_SLOTS; i++) {
        int slotTotal = countSlotTxns(i);
        if (slotTotal == 0 && i != g_activeSlot) continue;
        int slotUndone = countSlotUndone(i, g_slotCursor[i]);
        Serial.printf("          slot %d%s  txns %d  cursor %d\n",
            i, (i == g_activeSlot) ? " *" : "  ",
            slotTotal, -slotUndone);
    }
}

}  // extern "C"
