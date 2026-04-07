/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Payments Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* can_make_payment: () -> i32 */
static wasm_trap_t* cb_pay_can_make_payment(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(0); /* false -- payments not supported */
    return NULL;
}

/* request_payment: (i32 request_ptr, i32 token_buf, i32 token_len_ptr) -> i32 */
static wasm_trap_t* cb_pay_request_payment(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_payments(wasmtime_linker_t* linker) {
    WAPI_DEFINE_0_1(linker, "wapi_pay", "can_make_payment",  cb_pay_can_make_payment);
    WAPI_DEFINE_3_1(linker, "wapi_pay", "request_payment",   cb_pay_request_payment);
}
