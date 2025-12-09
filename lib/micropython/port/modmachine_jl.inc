// Extra glue for Jumperless embed port

// Forward declaration for soft reboot function from C++ code
#ifdef __cplusplus
extern "C" {
#endif
extern void jl_soft_reboot(void);
#ifdef __cplusplus
}
#endif

// Provide idle hook required by extmod/modmachine.c
static void mp_machine_idle(void) {
    // Allow background processing; nothing required for now
    MICROPY_INTERNAL_EVENT_HOOK;
}

// Custom soft_reset implementation for Jumperless
static mp_obj_t machine_soft_reset_jl(void) {
    jl_soft_reboot();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(machine_soft_reset_jl_obj, machine_soft_reset_jl);

// Expose additional globals in machine module
#define MICROPY_PY_MACHINE_EXTRA_GLOBALS \
    { MP_ROM_QSTR(MP_QSTR_Pin), MP_ROM_PTR(&machine_pin_type) }, \
    { MP_ROM_QSTR(MP_QSTR_soft_reset), MP_ROM_PTR(&machine_soft_reset_jl_obj) },

// Force QSTRs used by Pin into the embed QSTR scan
static void jl_qstr_refs(void) {
    (void)MP_QSTR_Pin; (void)MP_QSTR_low; (void)MP_QSTR_high; (void)MP_QSTR_toggle; (void)MP_QSTR_ALT;
    (void)MP_QSTR_on;  (void)MP_QSTR_off; (void)MP_QSTR_pull; (void)MP_QSTR_alt;
}
