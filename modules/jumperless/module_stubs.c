#include "py/obj.h"
#include "py/runtime.h"

// Minimal module stubs to satisfy linker requirements

// Force generation of QSTRs for newly-added Jumperless module symbols
// (these identifiers are scanned by the qstr generator)
static const uint32_t _jl_forced_qstrs[] = {
    MP_QSTR_gpio_set_read_floating,
    MP_QSTR_gpio_get_read_floating,
    MP_QSTR_set_gpio_read_floating,
    MP_QSTR_get_gpio_read_floating,
};


// // Empty os module
// static const mp_rom_map_elem_t mp_module_os_globals_table[] = {
//     { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_os) },
// };
// static MP_DEFINE_CONST_DICT(mp_module_os_globals, mp_module_os_globals_table);

// const mp_obj_module_t mp_module_os = {
//     .base = { &mp_type_module },
//     .globals = (mp_obj_dict_t *)&mp_module_os_globals,
// };

// // Empty io module  
// static const mp_rom_map_elem_t mp_module_io_globals_table[] = {
//     { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_io) },
// };
// static MP_DEFINE_CONST_DICT(mp_module_io_globals, mp_module_io_globals_table);

// const mp_obj_module_t mp_module_io = {
//     .base = { &mp_type_module },
//     .globals = (mp_obj_dict_t *)&mp_module_io_globals,
// }; 