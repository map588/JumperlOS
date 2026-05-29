# Vendored FatFS for JumperlOS

This directory is a project-local copy of the FatFS library bundled with
the [arduino-pico] core, with two changes from upstream:

1. The bundled `lib/SPIFTL/` is replaced by the JumperlOS fork of SPIFTL
   ([Architeuthis-Flux/SPIFTL]), now wired in as a **git submodule** at
   `lib/FatFS/lib/SPIFTL` (see "SPIFTL submodule" below), which adds two
   opt-in modes:
   - **lazy-persist**: defer metadata serialization off `CTRL_SYNC`.
   - **delta-journal**: append only the changed L2P/`peCount` entries to
     an already-erased flash page on each `persist()` instead of rewriting
     the whole ~16 KB snapshot. This drops a save from ~750 ms to roughly
     one page program (sub-ms) AND keeps it immediately power-loss durable,
     so we no longer need lazy deferral for fast saves. See the fork's
     README ("Delta-Journal Mode") for the on-flash format and recovery.

   JumperlOS enables delta-journal by default (`FATFS_SPIFTL_JOURNAL` in
   `src/FatFS.cpp`) and leaves lazy-persist OFF (`src/main.cpp`). The journal
   uses identical on-flash geometry to a non-journaled volume (the 2 ring
   blocks are borrowed from the free pool at runtime and released under
   near-full pressure), so enabling it mounts an existing volume with NO
   reformat and no data loss.
2. `extern "C"` shim functions in `src/FatFS.cpp` so JumperlOS code can
   drive these modes without C++ namespace lookups for the file-static
   `_ftl` pointer: `fatFsSetLazyPersist`, `fatFsForceSync`,
   `fatFsSetJournal`, `fatFsIsJournal`, and `fatFsSetTimingDebug`
   (declared in `src/FatFS_LazyPersist.h`). `fatFsSetTimingDebug(true)`
   logs each CTRL_SYNC persist's microsecond cost and whether it was a fast
   journal append or a full snapshot - toggled in JumperlOS via the debug
   flags menu ('q. SPIFTL persist timing').

The library `name` in `library.properties` is changed from `FatFS` to
`FatFS-JL` so PlatformIO's library finder doesn't get confused by two
libraries with identical names; `lib_ignore = FatFSUSB, FatFS` in
`platformio.ini` then drops the framework-bundled copy. Header includes
(`<FatFS.h>`) and the `FS FatFS` global are unchanged.

## Source versions

- Upstream `arduino-pico` framework commit:
  `948e9bfd224d69c60bd4bb1de92d423fc5b6bc17` (package version
  `framework-arduinopico 1.40504.0`).
- Upstream FatFS library version: `0.15.0` (see
  `library.properties`).
- JumperlOS SPIFTL fork: <https://github.com/Architeuthis-Flux/SPIFTL>
  (replaces `lib/SPIFTL/`). The fork's `setLazyPersist(bool)` and
  `forceSync()` additions are documented in its own README.

## SPIFTL submodule

`lib/FatFS/lib/SPIFTL` is a git submodule pinned to a commit of
<https://github.com/Architeuthis-Flux/SPIFTL.git> (`.gitmodules` at the repo
root). The rest of `lib/FatFS/` (the patched arduino-pico FatFS) is still a
project-local copy.

- Fresh checkout: `git submodule update --init lib/FatFS/lib/SPIFTL`
  (the fork's own `nbdkit` submodule is NOT needed for the build - don't use
  `--recursive`; only the `*.h` files are compiled in).
- Pull fork updates: `git submodule update --remote lib/FatFS/lib/SPIFTL`,
  then commit the new submodule pointer in this repo.
- Develop against the fork: edit inside the submodule, commit + push to the
  fork's `main`, then bump the pinned commit here.

## Refreshing the FatFS copy from upstream

1. `cp -R ~/.platformio/packages/framework-arduinopico/libraries/FatFS/{src,library.properties,keywords.txt} lib/FatFS/`
   (NOT `lib/` - that holds the SPIFTL submodule; don't overwrite it).
2. Re-apply the `name=FatFS-JL` rename in `lib/FatFS/library.properties`.
3. Re-apply the `extern "C"` shim functions at the bottom of
   `lib/FatFS/src/FatFS.cpp` (look near `static SPIFTL *_ftl`).
4. Update the version line at the top of this file.
