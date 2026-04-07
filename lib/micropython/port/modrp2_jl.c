/*
 * rp2 module for Jumperless embed port
 * Adapted from micropython/ports/rp2/modrp2.c
 *
 * Provides the _rp2 module with PIO, StateMachine, and DMA types.
 * The Python-level rp2.py re-exports from _rp2 and adds the @asm_pio decorator.
 *
 * The MIT License (MIT)
 * Copyright (c) 2020-2021 Damien P. George
 */

#include "py/mphal.h"
#include "py/runtime.h"
#include "modrp2_jl.h"

// bootsel_button stub — Jumperless V5 doesn't expose BOOTSEL as a user button
static mp_obj_t rp2_bootsel_button(void) {
    return MP_OBJ_NEW_SMALL_INT(0);
}
MP_DEFINE_CONST_FUN_OBJ_0(rp2_bootsel_button_obj, rp2_bootsel_button);

static const mp_rom_map_elem_t rp2_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),            MP_ROM_QSTR(MP_QSTR_rp2) },
    { MP_ROM_QSTR(MP_QSTR_PIO),                 MP_ROM_PTR(&rp2_pio_type) },
    { MP_ROM_QSTR(MP_QSTR_StateMachine),        MP_ROM_PTR(&rp2_state_machine_type) },
    { MP_ROM_QSTR(MP_QSTR_DMA),                 MP_ROM_PTR(&rp2_dma_type) },
    { MP_ROM_QSTR(MP_QSTR_bootsel_button),      MP_ROM_PTR(&rp2_bootsel_button_obj) },
};
static MP_DEFINE_CONST_DICT(rp2_module_globals, rp2_module_globals_table);

const mp_obj_module_t mp_module_rp2 = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&rp2_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR__rp2, mp_module_rp2);
