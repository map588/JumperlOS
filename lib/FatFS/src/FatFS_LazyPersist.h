/*
    FatFS_LazyPersist.h - JumperlOS-specific public API to drive the
    SPIFTL lazy-persist mode of the vendored FatFS library.

    These are NOT in upstream arduino-pico FatFS. They are part of the
    JumperlOS fork (see lib/FatFS/JL_PATCH.md). Including this header is
    the supported way for application code to flip lazy persist on/off
    and to force a metadata sync at coarse-grained safe points.

    Power-loss safety contract:
      - When lazy persist is on, individual file writes still hit flash
        (data sectors AND FAT/dir entries) via SPIFTL::write(). Only the
        L2P / peCount / ebState METADATA serialization is deferred.
      - If power dies between a save and the next forceSync, on reboot
        SPIFTL reverts to the most recently persisted L2P, which means
        the lazy-written sectors become unreachable - the file system
        sees the previous version of the file. Pair with an application-
        level mirror (e.g. JumperlOS' /.bak ABA-pair flusher) if the
        small "last burst" loss window is unacceptable.
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Enable/disable lazy persist mode. Default is OFF (matches upstream
// behavior) so callers must explicitly opt in. Calling this before
// FatFS.begin() is a no-op (the SPIFTL instance hasn't been constructed
// yet); call it once after begin().
void fatFsSetLazyPersist(bool enable);

// Force the SPIFTL metadata to be persisted to flash NOW. Returns true
// on success or if there was nothing to persist. Returns false on flush
// failure. Always safe to call regardless of whether lazy persist is on
// - if the metadata is already coherent, this is a fast no-op.
bool fatFsForceSync(void);

// Enable/disable the SPIFTL delta-journal at runtime. When enabled (the
// JumperlOS default, see FATFS_SPIFTL_JOURNAL in FatFS.cpp), persist() on
// every f_close appends ONE already-erased flash page holding just the
// changed L2P/peCount entries instead of rewriting the whole ~16 KB metadata
// snapshot. That makes a save ~sub-millisecond AND immediately power-loss
// durable, so lazy persist is no longer required for fast saves.
//
// Enabling only takes effect if the FTL was constructed with journaling
// reserved (geometry + dirty bitsets); otherwise this is a no-op. Disabling
// reverts persist()/forceSync() to full snapshots. No-op if the FTL is off.
void fatFsSetJournal(bool enable);
bool fatFsIsJournal(void);

// Enable/disable per-CTRL_SYNC FTL persist timing logs. When on, each f_close's
// metadata persist prints its microsecond cost and whether it was a fast
// journal append or a full snapshot - handy for confirming the journal win.
void fatFsSetTimingDebug(bool enable);

#ifdef __cplusplus
}
#endif
