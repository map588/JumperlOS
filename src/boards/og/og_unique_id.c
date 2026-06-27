// SPDX-License-Identifier: MIT
//
// flash_get_unique_id() override for the RP2040 (OG Jumperless).
//
// The RP2040 has NO on-chip unique-ID register. The pico-sdk reads a board
// unique ID from the external QSPI flash chip via a 0x4B command
// (flash_get_unique_id -> __flash_do_cmd), which must disable XIP and run from
// RAM. On RP2040 this happens in a PRE-MAIN constructor
// (_retrieve_unique_id_on_boot in pico-sdk unique_id.c, RP2040 branch). On the
// OG that early flash command hard-faults (a flash->RAM long-call veneer / XIP
// transition before the system is fully initialized), reset-looping the board
// before main()/setup() ever run. (The RP2350/V5 uses rom_get_sys_info and
// never hits this path.)
//
// We don't need the real flash unique ID: the OG USB serial number is the fixed
// "JLOGport" string (see usb_descriptors.cpp). So override flash_get_unique_id
// with a no-flash, fixed-ID implementation. The build links src/ objects before
// the pico-sdk archive and passes -Wl,--allow-multiple-definition, so this
// definition is the one that gets used; the SDK's flash.c is still pulled in for
// the other flash_range_program/erase functions FatFS needs.
//
// NOTE: this means pico_get_unique_board_id() returns a constant on OG. If a
// future feature needs a real per-unit ID, read it later (post-boot, from a RAM
// function with the other core parked) rather than in the boot constructor.

#if defined(OG_JUMPERLESS)

#include <stdint.h>
#include <string.h>
#include "hardware/flash.h"  // declaration + FLASH_UNIQUE_ID_SIZE_BYTES

void flash_get_unique_id(uint8_t *id_out) {
    static const uint8_t kOgUniqueId[FLASH_UNIQUE_ID_SIZE_BYTES] = {
        'J', 'L', 'O', 'G', 0x00, 0x00, 0x00, 0x01
    };
    if (id_out) {
        memcpy(id_out, kOgUniqueId, FLASH_UNIQUE_ID_SIZE_BYTES);
    }
}

#endif // OG_JUMPERLESS
