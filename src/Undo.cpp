// SPDX-License-Identifier: MIT
//
// Undo system.
//
// Compact delta ring (16k+ ops on PSRAM, ~512 SRAM-only) plus a small
// PSRAM-resident snapshot ring that backs non-symmetric ops (clear-all).
// Apply/revert is symmetric for connect/disconnect/DAC/GPIO; clear-all
// captures the full board state as YAML at record time. A bounded slice of
// the ring is persisted to /undo.hist on every slot save and restored at
// boot, so history now survives reboots (on every board - see persistence
// section below).

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
#include "OledGui.h"  // OledVars::setStr() - publish last undo/redo label to live screens
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

// NOTE: deliberately NO Serial.flush() here. These traces fire in hot paths
// (including under core_sync / fs_mutex on the save path), and a per-line
// blocking flush waits for the USB CDC TX FIFO to fully drain. On an SRAM-only
// board that path has little headroom, and a flood of flushed trace lines can
// stall the USB servicing long enough to wedge the device. Without flush the
// lines coalesce and drain in the background; we may lose the last few lines on
// a hard crash, which is an acceptable trade for not causing one.
#define UNDBG(fmt, ...) do { if (undo_debug || psram_debug) { Serial.printf("[%lu c%d] UNDO> " fmt "\n", (unsigned long)millis(), (int)(sio_hw->cpuid & 1), ##__VA_ARGS__); } } while(0)

extern struct config jumperlessConfig;
extern JumperlessState globalState;

// Backing-store + cache hooks used by the persistence path below. Declared at
// file scope (C++ linkage) on purpose: undoPersistHistory() lives inside the
// extern "C" lifecycle block, so a local `extern` there would bind C linkage
// and fail to resolve the C++ definitions in FilesystemStuff.cpp / FileCache.cpp.
extern bool safeFileWriteAllRaw(const char* path, const char* content,
                                size_t content_len, uint32_t timeout_ms);
extern bool fileCacheInvalidate(const char* path);

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

// Set whenever the committed ring (txns/ops/cursors) changes in a way that
// would alter the persisted /undo.hist file. undoPersistHistory() early-outs
// when this is false so the frequent autosave path doesn't re-serialize +
// re-CRC unchanged history. Cleared inside undoPersistHistory().
bool g_historyDirty = false;

// Runtime persistence limits, chosen in undoInit() based on PSRAM. Persistence
// works on every board now (the user only needs a handful of states without
// PSRAM); these keep the no-PSRAM scratch buffer small. See UNDO_PERSIST_MAX_*
// for the compile-time ceilings the static scratch arrays are sized to.
size_t g_persistMaxTxns  = 0;   // most-recent txns to keep in the file
size_t g_persistMaxBytes = 0;   // body byte budget (drops oldest past this)
size_t g_persistBufBytes = 0;   // serialization scratch buffer size

// Snapshot ring (single, not per-slot - clear-all snapshots are tagged
// with the slot they were taken on via the SnapshotEntry.slot field).
SnapshotEntry* g_snapshots = nullptr;
size_t g_snapshotCap = 0;
size_t g_snapshotHead = 0;
uint32_t g_lastSnapshotTxnId = 0;
uint32_t g_lastSnapshotMs = 0;

// ============================================================================
// Blob arena - stores full-state captures for non-symmetric ops (clear_all)
// and for snapshots used as resync anchors.
//
// Each blob is the COMPLETE board state serialized as the same YAML a slot
// save writes (globalState.toYAML()), so a clear-all undo restores the entire
// board - bridges, colors, DAC/rail voltages, GPIO, paths - not just a subset.
// This is also what gets copied into /undo.hist, so persisted clear-all undo
// reconstructs everything.
//
// Format of each blob:
//   [u32 magic 'JLBS'][u32 size][u32 crc32][u32 reserved] -> 16-byte header
//   [char yaml[size]]                                       -> toYAML() output
//
// A typical board is ~0.5-2 KB of YAML.
// ============================================================================

constexpr uint32_t BLOB_MAGIC = 0x53424C4A;  // 'JLBS'

#pragma pack(push, 1)
struct BlobHeader {
    uint32_t magic;
    uint32_t size;     // body size only (not including header)
    uint32_t crc32;    // crc of body
    uint32_t reserved;
};
#pragma pack(pop)

static_assert(sizeof(BlobHeader) == 16, "BlobHeader sized wrong");

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

// Capture the full board state (as slot YAML) into a blob at *outOffset.
// Returns true on success and sets outOffset/outSize. Caller must hold no
// other locks.
bool blobAppendCurrentState(uint32_t* outOffset, uint32_t* outSize) {
    if (!g_blobs || g_blobCap == 0) return false;

    String yaml;
    if (!globalState.toYAML(yaml)) {
        UNDBG("blobAppendCurrentState: toYAML failed");
        return false;
    }
    size_t bodySize = yaml.length();
    size_t totalSize = sizeof(BlobHeader) + bodySize;
    size_t aligned = (totalSize + 3u) & ~size_t(3u);
    UNDBG("blobAppendCurrentState yaml=%u total=%u aligned=%u used=%u cap=%u",
          (unsigned)bodySize, (unsigned)totalSize, (unsigned)aligned,
          (unsigned)g_blobUsed, (unsigned)g_blobCap);

    if (aligned > g_blobCap) {
        UNDBG("  blob > arena cap, abort");
        return false;
    }
    if (g_blobUsed + aligned > g_blobCap) {
        UNDBG("  blob arena full - wrapping to 0");
        g_blobUsed = 0;
    }

    uint8_t* dst = g_blobs + g_blobUsed;
    BlobHeader* hdr = (BlobHeader*)dst;
    uint8_t* body = dst + sizeof(BlobHeader);
    memcpy(body, yaml.c_str(), bodySize);

    hdr->magic = BLOB_MAGIC;
    hdr->size = (uint32_t)bodySize;
    hdr->reserved = 0;
    hdr->crc32 = blobCrc32(body, bodySize);

    if (outOffset) *outOffset = (uint32_t)g_blobUsed;
    if (outSize) *outSize = (uint32_t)aligned;
    g_blobUsed += aligned;

    UNDBG("blob append OK off=%u size=%u (yaml %u bytes)",
          outOffset ? *outOffset : 0, outSize ? *outSize : 0, (unsigned)bodySize);
    return true;
}

// Restore globalState from a YAML blob and apply it. Returns true if the blob
// is valid and state was restored. globalState.fromYAML() self-suppresses the
// undo-record hooks for its whole duration, so this won't recurse into the
// log even if the caller forgot to set g_undoApplying. The caller (undoUndo /
// undoRedo) pushes the result to hardware via undoCommitToHardware().
bool blobRestoreState(uint32_t offset, uint32_t size) {
    (void)size;
    if (!g_blobs || (size_t)offset + sizeof(BlobHeader) > g_blobCap) return false;
    BlobHeader* hdr = (BlobHeader*)(g_blobs + offset);
    if (hdr->magic != BLOB_MAGIC) {
        UNDBG("blob restore FAIL bad magic 0x%08X at off=%u",
              (unsigned)hdr->magic, (unsigned)offset);
        return false;
    }
    if ((size_t)offset + sizeof(BlobHeader) + hdr->size > g_blobCap) {
        UNDBG("blob restore FAIL size overflow at off=%u", (unsigned)offset);
        return false;
    }
    uint8_t* body = g_blobs + offset + sizeof(BlobHeader);
    if (blobCrc32(body, hdr->size) != hdr->crc32) {
        UNDBG("blob restore FAIL crc mismatch at off=%u", (unsigned)offset);
        return false;
    }

    // Rebuild the YAML string (the arena copy isn't NUL-terminated) and apply.
    char* tmp = (char*)malloc((size_t)hdr->size + 1);
    if (!tmp) {
        UNDBG("blob restore FAIL OOM for %u bytes", (unsigned)hdr->size);
        return false;
    }
    memcpy(tmp, body, hdr->size);
    tmp[hdr->size] = '\0';
    String yaml(tmp);
    free(tmp);

    String err;
    bool ok = globalState.fromYAML(yaml, err);
    UNDBG("blob restore (yaml %u bytes) ok=%d", (unsigned)hdr->size, (int)ok);
    return ok;
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
    // Never mutate the blob arena while we're applying/reverting - blobRestore
    // is reading from it and a fresh append could clobber the source offset.
    if (g_undoApplying) return;
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
// Cross-reboot persistence - bounded undo-history ring on flash
// =============================================================================
//
// Now that the SPIFTL delta-journal makes flash writes cheap, the undo
// history survives reboots. On every slot save we serialize the most-recent
// slice of the shared ring into /undo.hist (through the write-back cache, so
// it's an instant PSRAM memcpy that the background flush coalesces to flash).
// On boot undoInit() reads it back so undo/redo continue across power cycles.
//
// The file is itself a *bounded* ring: we only persist the newest
// UNDO_PERSIST_MAX_TXNS transactions (capped further at UNDO_PERSIST_MAX_BYTES
// of body), dropping the oldest history so the file never grows without
// bound. Obsolete (orphaned redo-tail) and empty transactions are skipped.
//
// Layout (all little-endian, packed):
//   [UndoFileHeader]
//   [uint32_t slotCursor[numSlots]]   - per-slot cursor as a linear txn index
//   [UndoTxn  txns[txnCount]]         - oldest-first; firstOp rewritten linear
//   [UndoOp   ops[opCount]]           - blob ops' blobOffset rewritten
//   [uint8_t  blobs[blobBytes]]       - compacted clear-all/blob arena
//
// We do NOT persist the PSRAM snapshot ring: snapshots are purely capture-side
// resync anchors and OP_CLEAR_ALL carries its own blob ref (a full board YAML,
// which we copy into the file), so clear-all undo still restores the entire
// board after a reboot. A fresh boot snapshot is taken when there's nothing to
// restore.
//
// Persistence works on every board. With PSRAM we keep up to 256 states; with
// only SRAM the scratch buffer comes from the small heap so we keep the most
// recent 32 (g_persistMaxTxns, set in undoInit).

namespace {

constexpr uint32_t  UNDO_FILE_MAGIC   = 0x4F44554Au;  // 'JUDO'
constexpr uint16_t  UNDO_FILE_VERSION = 2;             // v2: YAML clear-all blobs
constexpr const char* UNDO_FILE_PATH  = "/undo_history.txt";

// Compile-time ceilings the static scratch arrays are sized to. The *actual*
// run-time limits are g_persistMaxTxns / g_persistMaxBytes (set in undoInit
// from PSRAM availability) and are always <= these. Whichever run-time limit
// hits first wins; the single newest transaction is always included even if it
// alone exceeds the byte budget (it still has to fit the scratch buffer or
// persistence is skipped for that save).
constexpr size_t UNDO_PERSIST_MAX_TXNS  = 256;
constexpr size_t UNDO_PERSIST_MAX_BYTES = 64u * 1024u;

#pragma pack(push, 1)
struct UndoFileHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t numSlots;     // NUM_SLOTS at write time
    uint32_t bodyCrc;      // crc32 over the body (everything after this header)
    uint32_t bodySize;     // body byte count
    uint32_t opCount;
    uint32_t txnCount;
    uint32_t blobBytes;
    uint32_t globalTxnId;
    uint32_t opCap;        // ring caps at write time (sanity / future migration)
    uint32_t txnCap;
    uint32_t reserved0;
    uint32_t reserved1;
};
#pragma pack(pop)

// Serialization scratch buffer size: header + cursors + body budget + slack
// so a single oversized newest txn (or blob) still fits without overflow.
constexpr size_t UNDO_PERSIST_BUF_BYTES =
    sizeof(UndoFileHeader) + NUM_SLOTS * sizeof(uint32_t) +
    UNDO_PERSIST_MAX_BYTES + 8192u;

// Copy the blob living at `oldOff` in the live arena into the output blob
// region (buf + blobsBase ..), de-duplicating by old offset. Validates the
// blob's magic + CRC first; a stale/clobbered ref returns false so the caller
// can neutralize the referencing op. On success *newOff gets the arena-
// relative offset the blob will occupy after restore.
bool persistBlob(uint32_t oldOff, uint32_t oldSize, uint8_t* buf, size_t blobsBase,
                 size_t cap, size_t* outBlobUsed,
                 uint32_t* mapOld, uint32_t* mapNew, size_t* nMap, size_t mapMax,
                 uint32_t* newOff) {
    if (!g_blobs || oldSize == 0) return false;
    for (size_t k = 0; k < *nMap; k++) {
        if (mapOld[k] == oldOff) { *newOff = mapNew[k]; return true; }
    }
    // Validate the source blob in the live arena.
    if ((size_t)oldOff + sizeof(BlobHeader) > g_blobCap) return false;
    const BlobHeader* hdr = (const BlobHeader*)(g_blobs + oldOff);
    if (hdr->magic != BLOB_MAGIC) return false;
    if ((size_t)oldOff + sizeof(BlobHeader) + hdr->size > g_blobCap) return false;
    if (blobCrc32(g_blobs + oldOff + sizeof(BlobHeader), hdr->size) != hdr->crc32)
        return false;
    if (blobsBase + *outBlobUsed + oldSize > cap) return false;  // no room
    memcpy(buf + blobsBase + *outBlobUsed, g_blobs + oldOff, oldSize);
    *newOff = (uint32_t)*outBlobUsed;
    *outBlobUsed += oldSize;
    if (*nMap < mapMax) { mapOld[*nMap] = oldOff; mapNew[*nMap] = *newOff; (*nMap)++; }
    return true;
}

// Serialize the live committed ring into `buf` (size `cap`). Returns the total
// byte count (header + body) written, or 0 if there's nothing to persist / the
// buffer is too small.
size_t undoSerialize(uint8_t* buf, size_t cap) {
    if (!g_ops || !g_txns) return 0;
    if (cap < sizeof(UndoFileHeader) + NUM_SLOTS * sizeof(uint32_t)) return 0;

    // Ring distance from the global tail (monotonic ordering helper).
    auto dist = [](size_t x) -> size_t {
        return (x + g_txnCap - g_txnTail) % g_txnCap;
    };

    // --- Select the newest reachable txns within the count + byte budget,
    //     walking backward from head. keptIdx[] holds them newest-first.
    static size_t keptIdx[UNDO_PERSIST_MAX_TXNS];
    static uint32_t budgetBlobOff[UNDO_PERSIST_MAX_TXNS];
    size_t nKept = 0;
    size_t nBudgetBlob = 0;
    size_t bytes = 0;
    size_t maxTxns  = g_persistMaxTxns  ? g_persistMaxTxns  : UNDO_PERSIST_MAX_TXNS;
    size_t maxBytes = g_persistMaxBytes ? g_persistMaxBytes : UNDO_PERSIST_MAX_BYTES;
    if (maxTxns > UNDO_PERSIST_MAX_TXNS) maxTxns = UNDO_PERSIST_MAX_TXNS;
    {
        size_t pos = g_txnHead;
        while (pos != g_txnTail && nKept < maxTxns) {
            pos = (pos + g_txnCap - 1) % g_txnCap;
            const UndoTxn& t = g_txns[pos];
            if (t.opCount != 0 && !(t.flags & UNDO_TXN_OBSOLETE)) {
                size_t add = sizeof(UndoTxn) + (size_t)t.opCount * sizeof(UndoOp);
                size_t blobAdd = 0;
                for (uint16_t i = 0; i < t.opCount; i++) {
                    const UndoOp& op = g_ops[(t.firstOp + i) % g_opCap];
                    if (op.type == UNDO_OP_CLEAR_ALL || op.type == UNDO_OP_BLOB_REPLACE) {
                        uint32_t off = op.blob.blobOffset;
                        bool seen = false;
                        for (size_t k = 0; k < nBudgetBlob; k++)
                            if (budgetBlobOff[k] == off) { seen = true; break; }
                        if (!seen && op.blob.blobSize > 0 &&
                            nBudgetBlob < UNDO_PERSIST_MAX_TXNS) {
                            budgetBlobOff[nBudgetBlob++] = off;
                            blobAdd += op.blob.blobSize;
                        }
                    }
                }
                if (nKept > 0 && bytes + add + blobAdd > maxBytes) break;
                keptIdx[nKept++] = pos;
                bytes += add + blobAdd;
            }
            if (pos == g_txnTail) break;
        }
    }

    // Total ops across the kept txns -> fixes the ops region size + blob base.
    size_t opCount = 0;
    for (size_t k = 0; k < nKept; k++) opCount += g_txns[keptIdx[k]].opCount;

    size_t hdrSize    = sizeof(UndoFileHeader);
    size_t cursorsOff = hdrSize;
    size_t txnsOff    = cursorsOff + (size_t)NUM_SLOTS * sizeof(uint32_t);
    size_t opsOff     = txnsOff + nKept * sizeof(UndoTxn);
    size_t blobsBase  = opsOff + opCount * sizeof(UndoOp);
    if (blobsBase > cap) return 0;  // committed txns/ops alone overflow buffer

    // --- Per-slot cursors as linear txn indices (count of kept txns strictly
    //     older than the cursor). cursor==head therefore maps to nKept (live).
    uint32_t* cur = (uint32_t*)(buf + cursorsOff);
    for (int s = 0; s < NUM_SLOTS; s++) {
        size_t dC = dist(g_slotCursor[s]);
        size_t cnt = 0;
        for (size_t k = 0; k < nKept; k++)
            if (dist(keptIdx[k]) < dC) cnt++;
        cur[s] = (uint32_t)cnt;
    }

    // --- Emit txns oldest-first, linearizing ops and compacting blobs.
    UndoTxn* outTxns = (UndoTxn*)(buf + txnsOff);
    UndoOp*  outOps  = (UndoOp*)(buf + opsOff);
    static uint32_t mapOld[UNDO_PERSIST_MAX_TXNS];
    static uint32_t mapNew[UNDO_PERSIST_MAX_TXNS];
    size_t nMap = 0;
    size_t outBlobUsed = 0;
    size_t opOut = 0;
    size_t outTxnIdx = 0;
    for (int k = (int)nKept - 1; k >= 0; k--) {
        const UndoTxn& t = g_txns[keptIdx[k]];
        UndoTxn nt = t;
        nt.firstOp = (uint32_t)opOut;
        for (uint16_t i = 0; i < t.opCount; i++) {
            UndoOp op = g_ops[(t.firstOp + i) % g_opCap];
            if (op.type == UNDO_OP_CLEAR_ALL || op.type == UNDO_OP_BLOB_REPLACE) {
                uint32_t newOff = 0;
                if (persistBlob(op.blob.blobOffset, op.blob.blobSize, buf, blobsBase,
                                cap, &outBlobUsed, mapOld, mapNew, &nMap,
                                UNDO_PERSIST_MAX_TXNS, &newOff)) {
                    op.blob.blobOffset = newOff;
                } else {
                    // Stale/missing blob - neutralize so restore leaves state
                    // alone (matches the live revert's best-effort behavior).
                    op.blob.blobOffset = 0;
                    op.blob.blobSize = 0;
                }
            }
            op.txnIdx = (uint16_t)(outTxnIdx & 0xFFFF);
            outOps[opOut++] = op;
        }
        outTxns[outTxnIdx++] = nt;
    }

    size_t bodySize = (blobsBase - hdrSize) + outBlobUsed;

    UndoFileHeader* hdr = (UndoFileHeader*)buf;
    memset(hdr, 0, sizeof(*hdr));
    hdr->magic       = UNDO_FILE_MAGIC;
    hdr->version     = UNDO_FILE_VERSION;
    hdr->numSlots    = (uint16_t)NUM_SLOTS;
    hdr->bodySize    = (uint32_t)bodySize;
    hdr->opCount     = (uint32_t)opCount;
    hdr->txnCount    = (uint32_t)nKept;
    hdr->blobBytes   = (uint32_t)outBlobUsed;
    hdr->globalTxnId = g_globalTxnId;
    hdr->opCap       = (uint32_t)g_opCap;
    hdr->txnCap      = (uint32_t)g_txnCap;
    hdr->bodyCrc     = blobCrc32(buf + hdrSize, bodySize);

    UNDBG("serialize txns=%u ops=%u blob=%u body=%u total=%u",
          (unsigned)nKept, (unsigned)opCount, (unsigned)outBlobUsed,
          (unsigned)bodySize, (unsigned)(hdrSize + bodySize));
    return hdrSize + bodySize;
}

// Rebuild the live ring from a serialized image. Returns true if any history
// was restored (txnCount > 0).
bool undoDeserialize(const uint8_t* buf, size_t len) {
    if (!g_ops || !g_txns) return false;
    if (len < sizeof(UndoFileHeader)) return false;

    UndoFileHeader hdr;
    memcpy(&hdr, buf, sizeof(hdr));
    if (hdr.magic != UNDO_FILE_MAGIC || hdr.version != UNDO_FILE_VERSION) {
        Serial.printf("[Undo] restore REJECT: bad magic/version (0x%08X v%u, "
                      "want 0x%08X v%u)\n", (unsigned)hdr.magic,
                      (unsigned)hdr.version, (unsigned)UNDO_FILE_MAGIC,
                      (unsigned)UNDO_FILE_VERSION);
        return false;
    }

    size_t hdrSize = sizeof(UndoFileHeader);
    if (hdrSize + hdr.bodySize > len) {
        Serial.printf("[Undo] restore REJECT: truncated (need %u have %u)\n",
              (unsigned)(hdrSize + hdr.bodySize), (unsigned)len);
        return false;
    }
    if (blobCrc32(buf + hdrSize, hdr.bodySize) != hdr.bodyCrc) {
        Serial.printf("[Undo] restore REJECT: body CRC mismatch (body=%u)\n",
              (unsigned)hdr.bodySize);
        return false;
    }

    // Sanity-bound against this build's ring capacities.
    if (hdr.txnCount >= g_txnCap || hdr.opCount >= g_opCap ||
        hdr.blobBytes > g_blobCap) {
        Serial.printf("[Undo] restore REJECT: counts exceed ring caps "
              "(txn=%u/%u op=%u/%u blob=%u/%u)\n",
              (unsigned)hdr.txnCount, (unsigned)g_txnCap,
              (unsigned)hdr.opCount, (unsigned)g_opCap,
              (unsigned)hdr.blobBytes, (unsigned)g_blobCap);
        return false;
    }

    size_t cursorsOff = hdrSize;
    size_t txnsOff    = cursorsOff + (size_t)hdr.numSlots * sizeof(uint32_t);
    size_t opsOff     = txnsOff + (size_t)hdr.txnCount * sizeof(UndoTxn);
    size_t blobsOff   = opsOff + (size_t)hdr.opCount * sizeof(UndoOp);
    if (blobsOff + hdr.blobBytes != hdrSize + hdr.bodySize) {
        Serial.printf("[Undo] restore REJECT: section layout mismatch "
              "(numSlots=%u txn=%u op=%u blob=%u body=%u)\n",
              (unsigned)hdr.numSlots, (unsigned)hdr.txnCount,
              (unsigned)hdr.opCount, (unsigned)hdr.blobBytes,
              (unsigned)hdr.bodySize);
        return false;
    }

    if (hdr.txnCount > 0)
        memcpy(g_txns, buf + txnsOff, (size_t)hdr.txnCount * sizeof(UndoTxn));
    if (hdr.opCount > 0)
        memcpy(g_ops, buf + opsOff, (size_t)hdr.opCount * sizeof(UndoOp));
    if (hdr.blobBytes > 0)
        memcpy(g_blobs, buf + blobsOff, hdr.blobBytes);

    g_txnTail = 0;
    g_txnHead = hdr.txnCount;   // < g_txnCap, never the full sentinel
    g_opTail = 0;
    g_opHead = hdr.opCount;
    g_blobUsed = hdr.blobBytes;
    g_globalTxnId = hdr.globalTxnId;

    // Cursors were stored as linear txn indices; with tail==0 that's also the
    // ring index. Clamp defensively into [0, txnHead].
    const uint32_t* cur = (const uint32_t*)(buf + cursorsOff);
    for (int s = 0; s < NUM_SLOTS; s++) {
        if (s < (int)hdr.numSlots) {
            uint32_t v = cur[s];
            if (v > hdr.txnCount) v = hdr.txnCount;
            g_slotCursor[s] = v;
        } else {
            g_slotCursor[s] = g_txnHead;
        }
    }

    g_waypointsDirty = true;
    g_historyDirty = false;
    UNDBG("restore OK txns=%u ops=%u blob=%u gid=%u",
          (unsigned)hdr.txnCount, (unsigned)hdr.opCount,
          (unsigned)hdr.blobBytes, (unsigned)hdr.globalTxnId);
    return hdr.txnCount > 0;
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

    // Persistence limits. With PSRAM we keep a deep history; without it the
    // serialization scratch buffer + /undo.hist body live in SRAM. SRAM-only
    // builds run with the heap nearly full (often <15 KB free), so the old
    // ~12 KB scratch buffer routinely failed to allocate and the save was
    // silently dropped (you'd just keep restoring the previous file). Keep the
    // no-PSRAM budget intentionally small so the scratch buffer reliably fits;
    // a handful of undo steps is all that's expected without PSRAM anyway.
    if (havePsram) {
        g_persistMaxTxns  = 256;
        g_persistMaxBytes = 64u * 1024u;
    } else {
        g_persistMaxTxns  = 24;
        g_persistMaxBytes = 3u * 1024u;
    }
    // Slack covers one oversized newest txn/blob beyond the body budget. A
    // clear-all blob is a full-board YAML capture (<2 KB), so 2 KB is plenty on
    // the no-PSRAM path while keeping the total scratch buffer ~5 KB.
    size_t persistSlack = havePsram ? 4096u : 2048u;
    g_persistBufBytes = sizeof(UndoFileHeader) + (size_t)NUM_SLOTS * sizeof(uint32_t)
                      + g_persistMaxBytes + persistSlack;

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
    if (g_snapshots)
        Serial.printf("[Undo] snapshot ring: %u entries\n", (unsigned)g_snapshotCap);

    // Restore persisted history (every board). If there's nothing to restore,
    // seed the snapshot ring with a fresh boot capture as before.
    bool restored = undoRestore();

    // Boot-time restore summary (always on - this runs once on Core 0 before
    // Core 1 is spamming Serial, so it's safe even when undo_debug would not
    // be). For each slot that has reachable history it reports the cursor, the
    // total reachable txns, and how many are undoable (reachable + strictly
    // older than the cursor). If a slot shows tot>0 but undoable=0 the cursor
    // was restored at the wrong end; if restore=0 the file was rejected.
    {
        size_t totalReachable = 0;
        char summary[200];
        size_t off = 0;
        summary[0] = '\0';
        for (int s = 0; s < NUM_SLOTS; s++) {
            size_t reach = 0, undoable = 0;
            bool pastCursor = (g_slotCursor[s] == g_txnTail);
            size_t pos = g_txnTail;
            while (pos != g_txnHead) {
                if (pos == g_slotCursor[s]) pastCursor = true;
                const UndoTxn& t = g_txns[pos];
                if (t.opCount && !(t.flags & UNDO_TXN_OBSOLETE) &&
                    t.slot == (uint8_t)s) {
                    reach++;
                    if (!pastCursor) undoable++;
                }
                pos = (pos + 1) % g_txnCap;
            }
            totalReachable += reach;
            if (reach > 0 && off + 40 < sizeof(summary)) {
                off += (size_t)snprintf(summary + off, sizeof(summary) - off,
                    " s%d[cur=%u tot=%u undoable=%u]", s,
                    (unsigned)g_slotCursor[s], (unsigned)reach, (unsigned)undoable);
            }
        }
        Serial.printf("[Undo] restore=%d head=%u tail=%u gid=%u active=%d "
                      "netSlot=%d reachable=%u%s\n",
            (int)restored, (unsigned)g_txnHead, (unsigned)g_txnTail,
            (unsigned)g_globalTxnId, g_activeSlot, netSlot,
            (unsigned)totalReachable, summary);
    }

    if (g_snapshots && !restored)
        takeSnapshot("boot");

    // One-shot cleanup of artifacts from the old persistent-history
    // implementation (replaced by /undo.hist). Best-effort.
    FatFS.remove("/undo.log");
    FatFS.remove("/undo.snap");
}

void undoShutdown(void) {
    if (g_inTxn) undoEndTxn();
    // Capture history one last time before the lights go out, then drain the
    // write-back cache so /undo.hist actually reaches flash.
    undoPersistHistory();
    extern void fileCacheFlushNowAll(const char* reason);
    fileCacheFlushNowAll("shutdown");
}

// Serialize the live history to /undo.hist via the normal file system.
// Cheap and idempotent: early-outs when nothing changed since the last persist,
// and the write-back cache dedups byte-identical writes. Runs on every board -
// SRAM-only units just keep a smaller bounded slice (see the g_persistMaxTxns /
// g_persistMaxBytes budgets chosen in undoInit). Durability (the eventual flush
// to flash) is handled by the cache's flush service / shutdown drain, same as
// every other slot/config save.
void undoPersistHistory(void) {
    if (!g_ops || !g_txns) return;
    if (!g_historyDirty) return;        // nothing new to write
    size_t bufBytes = g_persistBufBytes ? g_persistBufBytes : UNDO_PERSIST_BUF_BYTES;

    uint8_t* buf = (uint8_t*)undoAlloc(bufBytes);
    if (!buf) {
        // Real durability failure (NOT debug-gated): on an SRAM-only board this
        // ~12 KB scratch comes from the small heap, and if it can't be had the
        // history never reaches flash - the previous (stale) file is what the
        // next boot restores. Leave g_historyDirty set so the next save retries.
        Serial.printf("[Undo] persist FAIL: scratch alloc of %u bytes failed "
                      "- history NOT saved\n", (unsigned)bufBytes);
        return;
    }

    size_t total = undoSerialize(buf, bufBytes);
    if (total == 0) {
        // Nothing reachable to persist. Drop any stale file so a future boot
        // doesn't restore history that no longer exists.
        undoFree(buf);
        if (safeFileExists(UNDO_FILE_PATH)) safeFileDelete(UNDO_FILE_PATH);
        g_historyDirty = false;
        return;
    }

    // Durability is board-type dependent:
    //  - PSRAM: the write-back file cache lives in a large PSRAM arena, so the
    //    normal cache write (instant memcpy, content-dedup, coalesced flush in
    //    the background) is both cheap and safe - the entry is pinned so it
    //    can't be evicted before the idle flush carries it to flash.
    //  - No PSRAM: the cache is a hard 16 KB SRAM budget, and its lazy flush
    //    first CLONES the body (a 2nd SRAM copy) before writing. Under SRAM
    //    pressure that clone - or the cached body itself - can fail, which
    //    leaves the ONLY copy of undo history stuck dirty-in-RAM, never reaching
    //    flash. That's the "opens an old file, new saves never stick" symptom.
    //    There's no deferred-PSRAM-memcpy win to chase here anyway, so write
    //    straight to flash at this natural save point and make it durable now.
    bool ok;
    if (psram_available()) {
        ok = safeFileWriteAll(UNDO_FILE_PATH, (const char*)buf, total);
    } else {
        fileCacheInvalidate(UNDO_FILE_PATH);  // no stale cached copy shadows flash
        ok = safeFileWriteAllRaw(UNDO_FILE_PATH, (const char*)buf, total, 3000);
    }
    undoFree(buf);
    if (ok) {
        g_historyDirty = false;
    } else {
        Serial.printf("[Undo] persist FAIL: write of %u bytes to %s failed\n",
                      (unsigned)total, UNDO_FILE_PATH);
    }
    UNDBG("persist %s -> %u bytes (%s)", UNDO_FILE_PATH,
          (unsigned)total, ok ? "ok" : "FAILED");
}

// Load /undo.hist (if present + valid) into the freshly-allocated ring.
// Called from undoInit() after the rings exist. Returns true if any history
// was restored.
bool undoRestore(void) {
    if (!g_ops || !g_txns) return false;
    if (!safeFileExists(UNDO_FILE_PATH)) {
        Serial.printf("[Undo] restore: no %s on flash (first boot or never "
                      "persisted)\n", UNDO_FILE_PATH);
        return false;
    }
    size_t bufBytes = g_persistBufBytes ? g_persistBufBytes : UNDO_PERSIST_BUF_BYTES;

    int32_t sz = safeFileSize(UNDO_FILE_PATH);
    if (sz <= (int32_t)sizeof(UndoFileHeader) || (size_t)sz > bufBytes) {
        Serial.printf("[Undo] restore REJECT: size %ld out of range "
                      "(hdr=%u max=%u)\n", (long)sz,
                      (unsigned)sizeof(UndoFileHeader), (unsigned)bufBytes);
        return false;
    }

    // fileCacheReadInto (under safeFileReadAll) reserves one byte for a NUL
    // terminator, so size the buffer at file size + 1.
    uint8_t* buf = (uint8_t*)undoAlloc((size_t)sz + 1);
    if (!buf) return false;

    size_t readBytes = 0;
    bool ok = safeFileReadAll(UNDO_FILE_PATH, (char*)buf, (size_t)sz + 1, &readBytes);
    bool restored = false;
    if (ok && readBytes >= sizeof(UndoFileHeader)) {
        restored = undoDeserialize(buf, readBytes);
    }
    undoFree(buf);

    if (restored) {
        Serial.printf("[Undo] restored history from %s (%u bytes)\n",
                      UNDO_FILE_PATH, (unsigned)readBytes);
    }
    return restored;
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
        g_historyDirty = true;
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
    g_historyDirty = true;
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
        return (a == ROUTABLE_BUFFER_IN || a == DAC0 || a == DAC1) &&
               (b == DAC0 || b == DAC1 || b == ROUTABLE_BUFFER_IN);
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
    // clean=1 forces a full re-route + LED repaint. Needed because a clear-all
    // undo now replaces the entire board state (via YAML restore), not just a
    // single bridge, so a partial commit could leave stale crosspoints/LEDs.
    refreshConnections(-1, 1, 1);
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
    g_historyDirty = true;
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
    g_historyDirty = true;
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

// --- OLED undo/redo toast layout ---------------------------------------------
// The toast is built from up to three logical pieces:
//   tag    = "undo" / "redo"
//   action = "connect" / "disconnect" / "top" / "slot" / ... (the verb/source)
//   nodes  = "4-8" / "5.00V" / "2->3" / ...                  (the detail/value)
// The layout below is hardcoded to match the design exported from the OLED
// layout editor (device/screens/layout.json) - see undoBuildToastScreen.

// Split a recorder label into action (first token) + nodes (the detail/value)
// so the toast can render the verb/source small and the detail large. Covers
// every label the undoRecord* helpers emit:
//   "connect 4-8"    -> "connect"    / "4-8"
//   "disconnect 5-9" -> "disconnect" / "5-9"
//   "top 5.00V"      -> "top"        / "5.00V"   (rail / DAC voltage)
//   "bot 3.30V"      -> "bot"        / "3.30V"
//   "DAC0 1.50V"     -> "DAC0"       / "1.50V"
//   "slot 2->3"      -> "slot"       / "2->3"
//   "GPIO5 dir=1"    -> "GPIO5"      / "dir=1"
//   "GPIO5=1"        -> "GPIO5"      / "1"        (no space: split on '=')
//   "clear all"      -> ""           / "clear all" (kept whole - no value)
// Anything with no separator (or unlabeled) is kept whole as the detail so it
// still renders as a single segment.
static void undoSplitLabel(const char* label, char* action, size_t actionSize,
                           char* nodes, size_t nodesSize) {
    action[0] = '\0';
    nodes[0] = '\0';
    if (!label || !label[0]) return;

    // "clear all" (and similar bare commands) have no value half - the whole
    // label is the detail, with no separable action.
    if (strcmp(label, "clear all") == 0) {
        strncpy(nodes, label, nodesSize - 1);
        nodes[nodesSize - 1] = '\0';
        return;
    }

    // Action = first token; detail = the remainder. Prefer a space separator;
    // if there's none, fall back to '=' (e.g. "GPIO5=1") so the pin/source is
    // still split out from its value.
    const char* sep = strchr(label, ' ');
    if (!sep) sep = strchr(label, '=');

    if (sep && sep != label) {
        size_t verbLen = (size_t)(sep - label);
        if (verbLen >= actionSize) verbLen = actionSize - 1;
        memcpy(action, label, verbLen);
        action[verbLen] = '\0';
        strncpy(nodes, sep + 1, nodesSize - 1);   // skip the single separator
        nodes[nodesSize - 1] = '\0';
        return;
    }

    // No separator - keep the whole label as the detail.
    strncpy(nodes, label, nodesSize - 1);
    nodes[nodesSize - 1] = '\0';
}

// Build the toast as a retained OledScreen of anchored Text elements.
// Hardcoded to reproduce the design exported from the OLED layout editor
// (device/screens/layout.json), i.e. exactly what oledGuiLoadScreen() would
// produce from that file:
//   tag    ("undo"/"redo") : New Science 13pt, anchored top-left  (x/y ignored)
//   action ("connect"/...) : Pragmatism  8pt, absolute (7, 18)    (FREE/FREE)
//   nodes  ("8-GP 2"/...)  : Pragmatism 10pt, right-anchored at y=11 (RIGHT/FREE)
// Any piece that is empty is simply skipped.
static void undoBuildToastScreen(OledScreen& screen, const char* tag,
                                 const char* action, const char* nodes) {

                                    // return;
    screen.clearElements();
    screen.w = 128;
    screen.h = 32;

    if (tag && tag[0]) {
        int idx = screen.addText(tag, 3, 1, "New Science", 13);
        if (idx >= 0) screen.setAnchor(idx, OLED_H_LEFT, OLED_V_TOP);
    }
    if (action && action[0]) {
        int idx = screen.addText(action, 5, 18, "Pragmatism", 7);
        if (idx >= 0) screen.setAnchor(idx, OLED_H_LEFT, OLED_V_FREE);
    }
    if (nodes && nodes[0]) {
        int idx = screen.addText(nodes, 79, 11, "Pragmatism", 8);
        if (idx >= 0) screen.setAnchor(idx, OLED_H_RIGHT, OLED_V_FREE);
    }
}

void undoToast(bool isRedo, const char* label) {
    const char* tag = isRedo ? "redo" : "undo";

// return;

    // Match the probeMode entry banner style: blank line above and
    // below, tab-indented body. Standalone block so probe-mode prints
    // and prompts before/after don't smear into the toast line.

    // Publish the last undo/redo action to the OLED variable registry so any
    // live screen bound to {undo} updates automatically (no display required).
    {
        char undoVar[64];
        if (label && label[0]) snprintf(undoVar, sizeof(undoVar), "%s %s", tag, label);
        else                   snprintf(undoVar, sizeof(undoVar), "%s", tag);
        OledVars::setStr("undo", undoVar);
    }

    if (oled.isConnected()) {
    // Parse the label into action + nodes; undoBuildToastScreen lays them out
    // (with the tag) per the hardcoded layout-editor design, e.g.:
    //   undo
    //   connect              8-GP 2
    char action[16];
    char nodes[48];
    undoSplitLabel(label, action, sizeof(action), nodes, sizeof(nodes));

    // Build the toast as a retained oledgui scene graph (anchored Text
    // elements) and render it into the framebuffer. A function-static
    // OledScreen is reused across toasts so a hot undo/redo path pays no
    // per-toast allocation; it is never registered with OledGui (we render it
    // manually here), so the background render service never touches it.
    static OledScreen toastScreen;
    undoBuildToastScreen(toastScreen, tag, action, nodes);

    // Snapshot-aware hold: capture the pre-toast framebuffer FIRST, then
    // paint + priority-flush the toast. oledHoldBegin saves the current live
    // framebuffer (the background that was visible before this toast) into a
    // shadow buffer and arms the post-flush latch. render() repaints the
    // framebuffer with the toast scene graph; priorityFlushHeldFrame() pushes
    // it to the panel and restores the shadow back into the live framebuffer.
    // Any probe-mode / clearHighlighting / status writes during the hold
    // window therefore accumulate against the pre-toast background rather than
    // smearing on top of toast pixels. When the hold expires, oledPeriodic()
    // flushes the live framebuffer once - the panel transitions cleanly from
    // the toast to the up-to-date underlying content with no toast residue.
    //
    // A persistent idle oledgui page steps aside while we own the panel.
    OledGui::getInstance().notePanelTakenByOther();
    oled.oledHoldBegin(1800);
    toastScreen.resolveBindings();
    toastScreen.render(oled);
    oled.priorityFlushHeldFrame();
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
