/**
 * WAPI Desktop Runtime - Notifications (wapi_notifications.h)
 *
 * `close` is a sync import on module "wapi_notify"; `show` is async
 * via WAPI_IO_OP_NOTIFY_SHOW and is dispatched from wapi_host_io.c
 * into wapi_host_notify_show_op.
 *
 * Grant acquisition flows through the universal WAPI_IO_OP_CAP_REQUEST
 * path; no per-module perm imports live here.
 */

#include "wapi_host.h"

/* close: (i32 id) -> i32 */
static wasm_trap_t* cb_notify_close(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t id = WAPI_ARG_I32(0);
    bool ok = wapi_plat_notify_close((uint32_t)id);
    WAPI_RET_I32(ok ? WAPI_OK : WAPI_ERR_IO);
    return NULL;
}

/* Async: WAPI_IO_OP_NOTIFY_SHOW
 *   addr/len  = pointer to wapi_notify_desc_t (56B)
 *   result_ptr = pointer to wapi_handle_t (i32) that receives the id
 *
 * Completion:
 *   result = minted id on success (also inlined in payload bytes 0..3),
 *            or negative WAPI_ERR_* on failure.
 */
void wapi_host_notify_show_op(op_ctx_t* c) {
    /* wapi_notify_desc_t (56B):
     *   +0  stringview title    (u64 data, u64 length) = 16B
     *   +16 stringview body     (16B)
     *   +32 stringview icon_url (16B) — desktop ignores for now
     *   +48 u32 urgency, +52 u32 _reserved */
    if (c->len < 56) { c->result = WAPI_ERR_INVAL; return; }
    const uint8_t* desc = (const uint8_t*)wapi_wasm_ptr((uint32_t)c->addr, 56);
    if (!desc) { c->result = WAPI_ERR_INVAL; return; }

    uint64_t title_ptr, title_len, body_ptr, body_len;
    uint32_t urgency;
    memcpy(&title_ptr, desc +  0, 8);
    memcpy(&title_len, desc +  8, 8);
    memcpy(&body_ptr,  desc + 16, 8);
    memcpy(&body_len,  desc + 24, 8);
    memcpy(&urgency,   desc + 48, 4);

    const char* title = title_len ? (const char*)wapi_wasm_ptr((uint32_t)title_ptr, (uint32_t)title_len) : NULL;
    const char* body  = body_len  ? (const char*)wapi_wasm_ptr((uint32_t)body_ptr,  (uint32_t)body_len)  : NULL;
    if ((title_len && !title) || (body_len && !body)) { c->result = WAPI_ERR_INVAL; return; }

    uint32_t id = wapi_plat_notify_show(title, (size_t)title_len,
                                        body,  (size_t)body_len,
                                        (int)urgency);
    if (id == 0) { c->result = WAPI_ERR_IO; return; }

    c->result = (int32_t)id;
    c->inline_payload = true;
    memcpy(c->payload, &id, 4);
    if (c->result_ptr) wapi_wasm_write_i32((uint32_t)c->result_ptr, (int32_t)id);
}

void wapi_host_register_notifications(wasmtime_linker_t* linker) {
    WAPI_DEFINE_1_1(linker, "wapi_notify", "close", cb_notify_close);
}
