"""
PlatformIO Pre-Build Script: Lock FatFS partition layout

Pins FS_START / FS_END to the layout that field units have already been
shipped with (4 MB FatFS partition at the top of 16 MB flash, with 4 KB
EEPROM at the very top). This is a defensive safety latch: if a future
contributor changes board_build.filesystem_size in platformio.ini
without realising the side-effect, this script silently keeps the
partition where existing units' user data lives, so an OTA update
cannot wipe slot files / scripts / config.

If we ever DO want to relocate the partition (after writing a real
migration shim that copies live data to the new location first), this
is the place to change it - one obvious diff and a release note.

Layout (16 MB flash on Jumperless V5, MUST match what field units have):
    0x10000000 .. 0x10BFF000   sketch (~12 MB - 4 KB)
    0x10BFF000 .. 0x10FFF000   FatFS  (  4 MB exactly)
    0x10FFF000 .. 0x11000000   EEPROM (  4 KB)

These numbers are exactly what platform-raspberrypi's fetch_fs_size()
produces from `board_build.filesystem_size = 4m` on a 16 MB board:
    fs_end   = 0x10000000 + 16M - 4K     = 0x10FFF000
    fs_start = fs_end - 4M               = 0x10BFF000
We hard-pin them so a future contributor changing `filesystem_size`
in platformio.ini cannot accidentally shift FS_START and wipe the
user's slots / scripts / config on OTA update.
"""

Import("env")  # noqa: F821 - provided by SCons / PlatformIO

# Field-shipped layout. DO NOT change without writing a migration shim
# AND making it a release-note item.
JL_FLASH_BASE = 0x10000000
JL_FS_START = 0x10BFF000
JL_FS_END = 0x10FFF000


def _lock(*_args, **_kwargs):
    env["FS_START"] = JL_FS_START
    env["FS_END"] = JL_FS_END
    # Sketch length must follow so the linker script's flash region matches.
    env["PICO_FLASH_LENGTH"] = JL_FS_START - JL_FLASH_BASE


# Lock immediately so any later script (e.g. the platform's own
# fetch_fs_size emitter) sees our values, and again right before the
# linker script is regenerated as a belt-and-suspenders safety net.
_lock()
env.AddPreAction("$BUILD_DIR/memmap_default.ld", _lock)

print(
    "FS partition locked: FS_START=0x%08X FS_END=0x%08X (size=%d KB, sketch=%d KB)"
    % (
        JL_FS_START,
        JL_FS_END,
        (JL_FS_END - JL_FS_START) // 1024,
        (JL_FS_START - JL_FLASH_BASE) // 1024,
    )
)
