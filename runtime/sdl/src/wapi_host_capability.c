/**
 * WAPI SDL Runtime - Capability Queries
 *
 * Implements: wapi.cap_supported / _version / _count / _name,
 *             wapi.abi_version, wapi.panic_report
 *
 * The list of supported capabilities is intentionally narrower than the
 * desktop runtime — only those that SDL3, Dawn, or an in-process
 * implementation can actually back.
 */

#include "wapi_host.h"

static const char* SUPPORTED_CAPS[] = {
    /* Core */
    "wapi.env",
    "wapi.memory",
    "wapi.io",
    "wapi.clock",
    "wapi.random",
    /* Filesystem */
    "wapi.filesystem",
    "wapi.sandbox",
    "wapi.cache",
    /* Graphics / windowing (SDL3 + Dawn) */
    "wapi.gpu",
    "wapi.surface",
    "wapi.window",
    "wapi.display",
    "wapi.input",
    /* Audio / haptics / sensors (SDL3) */
    "wapi.audio",
    "wapi.haptics",
    "wapi.sensors",
    "wapi.power",
    /* Clipboard / DnD (SDL3) */
    "wapi.clipboard",
    "wapi.dnd",
};

#define SUPPORTED_CAP_COUNT \
    ((int32_t)(sizeof(SUPPORTED_CAPS) / sizeof(SUPPORTED_CAPS[0])))

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
 * Host implementations
 * ============================================================ */

static int32_t host_cap_supported(wasm_exec_env_t env,
                                         uint32_t name_ptr, uint32_t name_len) {
    (void)env;
    const char* name = (const char*)wapi_wasm_ptr(name_ptr, name_len);
    if (!name && name_len > 0) return 0;
    return find_capability(name, name_len) >= 0 ? 1 : 0;
}

static int32_t host_cap_version(wasm_exec_env_t env,
                                       uint32_t name_ptr, uint32_t name_len,
                                       uint32_t version_ptr) {
    (void)env;
    const char* name = (const char*)wapi_wasm_ptr(name_ptr, name_len);
    if (!name && name_len > 0) {
        uint64_t zero = 0;
        wapi_wasm_write_bytes(version_ptr, &zero, 8);
        return WAPI_ERR_INVAL;
    }
    int idx = find_capability(name, name_len);
    if (idx < 0) {
        uint64_t zero = 0;
        wapi_wasm_write_bytes(version_ptr, &zero, 8);
        return WAPI_ERR_NOTCAPABLE;
    }
    uint16_t ver[4] = { 1, 0, 0, 0 };
    wapi_wasm_write_bytes(version_ptr, ver, 8);
    return WAPI_OK;
}

static int32_t host_cap_count(wasm_exec_env_t env) {
    (void)env;
    return SUPPORTED_CAP_COUNT;
}

static int32_t host_cap_name(wasm_exec_env_t env,
                                    uint32_t index, uint32_t buf_ptr,
                                    uint32_t buf_len, uint32_t name_len_ptr) {
    (void)env;
    if ((int32_t)index < 0 || (int32_t)index >= SUPPORTED_CAP_COUNT) {
        return WAPI_ERR_RANGE;
    }
    const char* cap_name = SUPPORTED_CAPS[index];
    uint32_t len = (uint32_t)strlen(cap_name);
    wapi_wasm_write_u32(name_len_ptr, len);
    uint32_t copy_len = len < buf_len ? len : buf_len;
    if (copy_len > 0) {
        wapi_wasm_write_bytes(buf_ptr, cap_name, copy_len);
    }
    return WAPI_OK;
}

static int32_t host_abi_version(wasm_exec_env_t env, uint32_t version_ptr) {
    (void)env;
    uint16_t ver[4] = {
        WAPI_ABI_VERSION_MAJOR, WAPI_ABI_VERSION_MINOR,
        WAPI_ABI_VERSION_PATCH, 0
    };
    wapi_wasm_write_bytes(version_ptr, ver, 8);
    return WAPI_OK;
}

static void host_panic_report(wasm_exec_env_t env,
                              uint32_t msg_ptr, uint32_t msg_len) {
    (void)env;
    const char* msg = (const char*)wapi_wasm_ptr(msg_ptr, msg_len);
    if (msg && msg_len > 0) {
        fprintf(stderr, "WAPI PANIC: %.*s\n", (int)msg_len, msg);
    } else {
        fprintf(stderr, "WAPI PANIC: (no message)\n");
    }
}

/* ============================================================
 * Registration
 * ============================================================ */

static NativeSymbol g_symbols[] = {
    { "cap_supported", (void*)host_cap_supported, "(ii)i", NULL },
    { "cap_version",   (void*)host_cap_version,   "(iii)i", NULL },
    { "cap_count",     (void*)host_cap_count,     "()i",    NULL },
    { "cap_name",      (void*)host_cap_name,      "(iiii)i", NULL },
    { "abi_version",          (void*)host_abi_version,          "(i)i",   NULL },
    { "panic_report",         (void*)host_panic_report,         "(ii)",   NULL },
};

wapi_cap_registration_t wapi_host_capability_registration(void) {
    wapi_cap_registration_t r = {
        .module_name = "wapi",
        .symbols = g_symbols,
        .count = (uint32_t)(sizeof(g_symbols) / sizeof(g_symbols[0])),
    };
    return r;
}
