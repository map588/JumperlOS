/*
 * rp2 module header for Jumperless embed port
 * Adapted from micropython/ports/rp2/modrp2.h
 *
 * The MIT License (MIT)
 * Copyright (c) 2020-2021 Damien P. George
 */
#ifndef MICROPY_INCLUDED_JL_MODRP2_H
#define MICROPY_INCLUDED_JL_MODRP2_H

#include "py/obj.h"

extern const mp_obj_type_t rp2_pio_type;
extern const mp_obj_type_t rp2_state_machine_type;
extern const mp_obj_type_t rp2_dma_type;

void rp2_pio_init(void);
void rp2_pio_deinit(void);

void rp2_dma_init(void);
void rp2_dma_deinit(void);

#endif // MICROPY_INCLUDED_JL_MODRP2_H
