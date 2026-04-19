/**
 * WAPI CLAP Plugin Wrapper - Host Import Functions
 *
 * Implements the WAPI host functions that the Wasm plugin module calls
 * (the import side). These are registered under the "wapi_plugin" import
 * module and under "wapi" for generic capability queries.
 *
 * The plugin instance (wapi_wasm_plugin_t*) is stored in the wasmtime
 * store's data pointer, accessible via wasmtime_context_data() from
 * within any host callback.
 *
 * Import functions:
 *   wapi_plugin.param_count   () -> i32
 *   wapi_plugin.param_set     (u32 param_id, f32 value) -> i32
 *   wapi_plugin.param_get     (u32 param_id) -> f32
 *   wapi_plugin.request_gui_resize (i32 width, i32 height) -> i32
 *   wapi_plugin.send_midi     (i32 status, i32 data1, i32 data2) -> i32
 *   wapi.cap_supported (i32 name_ptr, i32 name_len) -> i32
 *   wapi.abi_version          (i32 version_ptr) -> i32
 */

#include "wapi_plugin_host.h"

/* ============================================================
 * Helper: Get plugin instance from caller context
 * ============================================================
 * The store was created with wapi_wasm_plugin_t* as its data,
 * so we retrieve it from the caller's context.
 */

static inline wapi_wasm_plugin_t* get_plugin_from_caller(wasmtime_caller_t* caller) {
    wasmtime_context_t* ctx = wasmtime_caller_context(caller);
    return (wapi_wasm_plugin_t*)wasmtime_context_get_data(ctx);
}

/* ============================================================
 * wapi_plugin.param_count: () -> i32
 * ============================================================
 * Returns the number of parameters the plugin declared.
 */

static wasm_trap_t* host_param_count(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)args; (void)nargs;
    wapi_wasm_plugin_t* p = get_plugin_from_caller(caller);
    WAPI_RET_I32(p ? (int32_t)p->param_count : 0);
    return NULL;
}

/* ============================================================
 * wapi_plugin.param_set: (u32 param_id, f32 value) -> i32
 * ============================================================
 * Called by the Wasm plugin (typically from GUI interaction)
 * to report a parameter change to the host. We update the
 * cached value and flag it for output to the DAW.
 *
 * Note: The Wasm ABI passes param_id as i32 and value as f32.
 * The actual Wasm function signature needs param_id as i32
 * and value as f32, so we define a custom func type.
 */

static wasm_trap_t* host_param_set(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)nargs;
    wapi_wasm_plugin_t* p = get_plugin_from_caller(caller);
    if (!p) { WAPI_RET_I32(WAPI_ERR_UNKNOWN); return NULL; }

    uint32_t param_id = WAPI_ARG_U32(0);
    float value = WAPI_ARG_F32(1);

    /* Find the parameter by ID and update */
    for (uint32_t i = 0; i < p->param_count; i++) {
        if (p->params[i].id == param_id) {
            p->param_values[i] = value;
            p->param_changed[i] = true;  /* Flag for CLAP output event */
            WAPI_RET_I32(WAPI_OK);
            return NULL;
        }
    }

    WAPI_RET_I32(WAPI_ERR_INVAL);
    return NULL;
}

/* ============================================================
 * wapi_plugin.param_get: (u32 param_id) -> f32
 * ============================================================
 * Returns the current value of a parameter (set by host automation).
 */

static wasm_trap_t* host_param_get(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)nargs;
    wapi_wasm_plugin_t* p = get_plugin_from_caller(caller);
    if (!p) { WAPI_RET_F32(0.0f); return NULL; }

    uint32_t param_id = WAPI_ARG_U32(0);

    for (uint32_t i = 0; i < p->param_count; i++) {
        if (p->params[i].id == param_id) {
            WAPI_RET_F32(p->param_values[i]);
            return NULL;
        }
    }

    WAPI_RET_F32(0.0f);
    return NULL;
}

/* ============================================================
 * wapi_plugin.request_gui_resize: (i32 width, i32 height) -> i32
 * ============================================================
 * Request the host DAW to resize the plugin GUI window.
 * Currently returns WAPI_ERR_NOTSUP since we don't implement GUI yet.
 */

static wasm_trap_t* host_request_gui_resize(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)nargs;
    wapi_wasm_plugin_t* p = get_plugin_from_caller(caller);
    if (!p) { WAPI_RET_I32(WAPI_ERR_UNKNOWN); return NULL; }

    /* int32_t width  = WAPI_ARG_I32(0); */
    /* int32_t height = WAPI_ARG_I32(1); */

    /* GUI resize is not implemented in this initial version */
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * wapi_plugin.send_midi: (i32 status, i32 data1, i32 data2) -> i32
 * ============================================================
 * Queue a MIDI output event (from instruments that generate MIDI).
 * The events are flushed back to CLAP in the process callback.
 */

static wasm_trap_t* host_send_midi(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)nargs;
    wapi_wasm_plugin_t* p = get_plugin_from_caller(caller);
    if (!p) { WAPI_RET_I32(WAPI_ERR_UNKNOWN); return NULL; }

    if (p->midi_out_count >= WAPI_MAX_MIDI_EVENTS) {
        WAPI_RET_I32(WAPI_ERR_OVERFLOW);
        return NULL;
    }

    wapi_midi_event_t* ev = &p->midi_out[p->midi_out_count++];
    ev->sample_offset = 0;  /* Output at start of buffer */
    ev->status = (uint8_t)WAPI_ARG_I32(0);
    ev->data1  = (uint8_t)WAPI_ARG_I32(1);
    ev->data2  = (uint8_t)WAPI_ARG_I32(2);
    ev->_pad   = 0;

    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * wapi.cap_supported: (i32 name_ptr, i32 name_len) -> i32
 * ============================================================
 * Reports whether a capability is supported. The plugin wrapper
 * only supports "wapi.audio_plugin".
 */

static wasm_trap_t* host_cap_supported(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)nargs;
    wapi_wasm_plugin_t* p = get_plugin_from_caller(caller);
    if (!p || !p->memory_valid) { WAPI_RET_I32(0); return NULL; }

    uint32_t name_ptr = WAPI_ARG_U32(0);
    uint32_t name_len = WAPI_ARG_U32(1);

    /* Read the capability name from Wasm memory */
    char name_buf[128];
    if (name_len >= sizeof(name_buf)) name_len = (uint32_t)(sizeof(name_buf) - 1);

    void* host_ptr = wapi_wasm_ptr(p, name_ptr, name_len);
    if (!host_ptr && name_len > 0) {
        WAPI_RET_I32(0);
        return NULL;
    }

    memcpy(name_buf, host_ptr, name_len);
    name_buf[name_len] = '\0';

    /* We support "wapi.audio_plugin" */
    if (name_len == 15 && memcmp(name_buf, "wapi.audio_plugin", 15) == 0) {
        WAPI_RET_I32(1);
    } else {
        WAPI_RET_I32(0);
    }

    return NULL;
}

/* ============================================================
 * wapi.abi_version: (i32 version_ptr) -> i32
 * ============================================================
 * Writes the WAPI ABI version into the given Wasm memory location.
 */

static wasm_trap_t* host_abi_version(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)nargs;
    wapi_wasm_plugin_t* p = get_plugin_from_caller(caller);
    if (!p || !p->memory_valid) { WAPI_RET_I32(WAPI_ERR_UNKNOWN); return NULL; }

    uint32_t version_ptr = WAPI_ARG_U32(0);

    /* wapi_version_t: major(u16) minor(u16) patch(u16) reserved(u16) = 8 bytes */
    uint16_t ver[4] = {
        WAPI_ABI_VERSION_MAJOR,
        WAPI_ABI_VERSION_MINOR,
        WAPI_ABI_VERSION_PATCH,
        0
    };

    if (!wapi_wasm_write_bytes(p, version_ptr, ver, 8)) {
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================
 * Registers all WAPI host import functions with the Wasmtime linker.
 * Plugin-specific functions go under "wapi_plugin" module.
 * Generic WAPI functions go under "wapi" module.
 */

void wapi_plugin_host_register(wasmtime_linker_t* linker) {
    /* --- wapi_plugin module imports --- */

    /* param_count: () -> i32 */
    WAPI_DEFINE_0_1(linker, "wapi_plugin", "param_count", host_param_count);

    /* param_set: (i32 param_id, f32 value) -> i32
     * Note: the second argument is f32, not i32. Need custom type. */
    {
        wasm_functype_t* ft = wapi_functype(
            2, (wasm_valkind_t[]){ WASM_I32, WASM_F32 },
            1, (wasm_valkind_t[]){ WASM_I32 });
        wasmtime_error_t* err = wasmtime_linker_define_func(
            linker, "wapi_plugin", 9, "param_set", 9,
            ft, host_param_set, NULL, NULL);
        wasm_functype_delete(ft);
        if (err) {
            wasm_message_t msg;
            wasmtime_error_message(err, &msg);
            fprintf(stderr, "[wapi_plugin] link error wapi_plugin::param_set: %.*s\n",
                    (int)msg.size, msg.data);
            wasm_byte_vec_delete(&msg);
            wasmtime_error_delete(err);
        }
    }

    /* param_get: (i32 param_id) -> f32 */
    {
        wasm_functype_t* ft = wapi_functype(
            1, (wasm_valkind_t[]){ WASM_I32 },
            1, (wasm_valkind_t[]){ WASM_F32 });
        wasmtime_error_t* err = wasmtime_linker_define_func(
            linker, "wapi_plugin", 9, "param_get", 9,
            ft, host_param_get, NULL, NULL);
        wasm_functype_delete(ft);
        if (err) {
            wasm_message_t msg;
            wasmtime_error_message(err, &msg);
            fprintf(stderr, "[wapi_plugin] link error wapi_plugin::param_get: %.*s\n",
                    (int)msg.size, msg.data);
            wasm_byte_vec_delete(&msg);
            wasmtime_error_delete(err);
        }
    }

    /* request_gui_resize: (i32 width, i32 height) -> i32 */
    WAPI_DEFINE_2_1(linker, "wapi_plugin", "request_gui_resize", host_request_gui_resize);

    /* send_midi: (i32 status, i32 data1, i32 data2) -> i32 */
    WAPI_DEFINE_3_1(linker, "wapi_plugin", "send_midi", host_send_midi);

    /* --- wapi module imports (generic capability queries) --- */

    /* cap_supported: (i32 name_ptr, i32 name_len) -> i32 */
    WAPI_DEFINE_2_1(linker, "wapi", "cap_supported", host_cap_supported);

    /* abi_version: (i32 version_ptr) -> i32 */
    WAPI_DEFINE_1_1(linker, "wapi", "abi_version", host_abi_version);
}
