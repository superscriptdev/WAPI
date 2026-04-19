/**
 * WAPI Desktop Runtime - Capability Queries
 *
 * Implements: wapi.cap_supported, wapi.cap_version,
 *             wapi.cap_count, wapi.cap_name, wapi.abi_version
 *
 * Uses string-based capability names (e.g. "wapi.gpu", "wapi.audio").
 */

#include "wapi_host.h"

/* ============================================================
 * Supported capabilities for this desktop runtime
 * ============================================================ */

static const char* SUPPORTED_CAPS[] = {
    /* Core */
    "wapi.env",
    "wapi.memory",
    "wapi.io",
    "wapi.clock",
    "wapi.random",
    /* Filesystem & networking */
    "wapi.filesystem",
    "wapi.sandbox",
    "wapi.cache",
    "wapi.net",
    /* Graphics & windowing */
    "wapi.gpu",
    "wapi.surface",
    "wapi.input",
    /* Audio & media */
    "wapi.audio",
    /* Content & data */
    "wapi.content",
    "wapi.text",
    "wapi.clipboard",
    "wapi.font",
    "wapi.kv_storage",
    /* Security */
    "wapi.crypto",
    /* Registration & system integration */
    "wapi.register",
    "wapi.taskbar",
    "wapi.permissions",
    "wapi.wake_lock",
    "wapi.orientation",
    /* Codecs & compression */
    "wapi.codec",
    "wapi.compression",
    "wapi.media_session",

    "wapi.encoding",
    /* Authentication & connectivity */
    "wapi.authn",
    "wapi.network_info",
    "wapi.battery",
    "wapi.idle",
    "wapi.haptics",
    /* Hardware access */
    "wapi.hid",
    "wapi.serial",
    "wapi.screen_capture",
    "wapi.contacts",
    "wapi.barcode",
    "wapi.nfc",
    "wapi.dnd",
    /* Identity */
    "wapi.user",
};

#define SUPPORTED_CAP_COUNT ((int32_t)(sizeof(SUPPORTED_CAPS) / sizeof(SUPPORTED_CAPS[0])))

/* Check if a name (not null-terminated) matches any supported capability */
static int find_capability(const char* name, uint32_t name_len) {
    for (int i = 0; i < SUPPORTED_CAP_COUNT; i++) {
        size_t cap_len = strlen(SUPPORTED_CAPS[i]);
        if (cap_len == name_len && memcmp(SUPPORTED_CAPS[i], name, name_len) == 0) {
            return i;
        }
    }
    return -1;
}

/* ============================================================
 * cap_supported: (i32 name, i32 name_len) -> i32
 * ============================================================ */

static wasm_trap_t* host_cap_supported(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t name_ptr = WAPI_ARG_U32(0);
    uint32_t name_len = WAPI_ARG_U32(1);

    const char* name = wapi_wasm_read_string(name_ptr, name_len);
    if (!name && name_len > 0) {
        WAPI_RET_I32(0);
        return NULL;
    }

    int idx = find_capability(name, name_len);
    WAPI_RET_I32(idx >= 0 ? 1 : 0);
    return NULL;
}

/* ============================================================
 * cap_version: (i32 name, i32 name_len, i32 version_ptr) -> i32
 * ============================================================ */

static wasm_trap_t* host_cap_version(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t name_ptr = WAPI_ARG_U32(0);
    uint32_t name_len = WAPI_ARG_U32(1);
    uint32_t version_ptr = WAPI_ARG_U32(2);

    const char* name = wapi_wasm_read_string(name_ptr, name_len);
    if (!name && name_len > 0) {
        uint64_t zero = 0;
        wapi_wasm_write_bytes(version_ptr, &zero, 8);
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    int idx = find_capability(name, name_len);
    if (idx < 0) {
        uint64_t zero = 0;
        wapi_wasm_write_bytes(version_ptr, &zero, 8);
        WAPI_RET_I32(WAPI_ERR_NOTCAPABLE);
        return NULL;
    }

    /* All capabilities are version 1.0.0 */
    uint16_t ver[4] = { 1, 0, 0, 0 };
    wapi_wasm_write_bytes(version_ptr, ver, 8);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * cap_count: () -> i32
 * ============================================================ */

static wasm_trap_t* host_cap_count(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    WAPI_RET_I32(SUPPORTED_CAP_COUNT);
    return NULL;
}

/* ============================================================
 * cap_name: (i32 index, i32 buf, i32 buf_len, i32 name_len_out) -> i32
 * ============================================================ */

static wasm_trap_t* host_cap_name(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t index = WAPI_ARG_U32(0);
    uint32_t buf_ptr = WAPI_ARG_U32(1);
    uint32_t buf_len = WAPI_ARG_U32(2);
    uint32_t name_len_ptr = WAPI_ARG_U32(3);

    if ((int32_t)index < 0 || (int32_t)index >= SUPPORTED_CAP_COUNT) {
        WAPI_RET_I32(WAPI_ERR_RANGE);
        return NULL;
    }

    const char* cap_name = SUPPORTED_CAPS[index];
    uint32_t len = (uint32_t)strlen(cap_name);

    wapi_wasm_write_u32(name_len_ptr, len);

    uint32_t copy_len = len < buf_len ? len : buf_len;
    if (copy_len > 0) {
        wapi_wasm_write_bytes(buf_ptr, cap_name, copy_len);
    }

    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * abi_version: (i32 version_ptr) -> i32
 * ============================================================ */

static wasm_trap_t* host_abi_version(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t version_ptr = WAPI_ARG_U32(0);
    uint16_t ver[4] = { WAPI_ABI_VERSION_MAJOR, WAPI_ABI_VERSION_MINOR,
                        WAPI_ABI_VERSION_PATCH, 0 };
    wapi_wasm_write_bytes(version_ptr, ver, 8);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * panic_report: (i32 msg_ptr, i32 msg_len) -> void
 * ============================================================
 * Records a panic message before the module traps.
 */

static wasm_trap_t* host_panic_report(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)results; (void)nresults;
    uint32_t msg_ptr = WAPI_ARG_U32(0);
    uint32_t msg_len = WAPI_ARG_U32(1);

    const char* msg = wapi_wasm_read_string(msg_ptr, msg_len);
    if (msg && msg_len > 0) {
        fprintf(stderr, "WAPI PANIC: %.*s\n", (int)msg_len, msg);
    } else {
        fprintf(stderr, "WAPI PANIC: (no message)\n");
    }
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_capability(wasmtime_linker_t* linker) {
    WAPI_DEFINE_2_1(linker, "wapi", "cap_supported", host_cap_supported);
    WAPI_DEFINE_3_1(linker, "wapi", "cap_version",   host_cap_version);
    WAPI_DEFINE_0_1(linker, "wapi", "cap_count",     host_cap_count);
    WAPI_DEFINE_4_1(linker, "wapi", "cap_name",      host_cap_name);
    WAPI_DEFINE_1_1(linker, "wapi", "abi_version",          host_abi_version);
    WAPI_DEFINE_2_0(linker, "wapi", "panic_report",         host_panic_report);
}
