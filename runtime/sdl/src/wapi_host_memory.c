/**
 * WAPI SDL Runtime - Memory Allocation
 *
 * Delegates to WAMR's built-in module heap allocator
 * (wasm_runtime_module_malloc / _free / _realloc). The heap size is
 * fixed at instantiation time in main.c (WASM_HEAP_SIZE).
 *
 * WAMR's allocator doesn't accept an explicit alignment. To honour the
 * wapi_memory.alloc alignment parameter we over-allocate by (align - 1)
 * and stash the original allocation offset just before the aligned
 * pointer so free/usable_size can recover it.
 */

#include "wapi_host.h"

/* Header size stored before each aligned alloc: the raw offset returned
 * by wasm_runtime_module_malloc. 8 bytes for alignment room (most
 * requested alignments are <= 16). */
#define WAPI_MEM_HDR 8

static bool is_pow2(uint32_t v) { return v && (v & (v - 1)) == 0; }

static uint32_t align_up_u32(uint32_t v, uint32_t align) {
    return (v + align - 1u) & ~(align - 1u);
}

/* Allocate `size` bytes aligned to `alignment` bytes in the guest heap.
 * Returns a guest pointer, or 0 on failure. Header (4 bytes raw_ptr +
 * 4 bytes user_size) sits immediately before the returned pointer. */
static uint32_t aligned_alloc_guest(uint32_t size, uint32_t alignment) {
    if (size == 0 || !is_pow2(alignment)) return 0;
    if (alignment < 8) alignment = 8;

    uint32_t raw_size = size + alignment + WAPI_MEM_HDR;
    void* native = NULL;
    uint64_t raw = wasm_runtime_module_malloc(g_rt.module_inst,
                                              (uint64_t)raw_size, &native);
    if (raw == 0 || !native) return 0;

    uint32_t aligned = align_up_u32((uint32_t)raw + WAPI_MEM_HDR, alignment);
    uint32_t raw_u32 = (uint32_t)raw;

    /* Stash raw_ptr + user_size in the 8 bytes preceding the aligned ptr. */
    void* hdr = wapi_wasm_ptr(aligned - WAPI_MEM_HDR, WAPI_MEM_HDR);
    if (!hdr) {
        wasm_runtime_module_free(g_rt.module_inst, raw);
        return 0;
    }
    memcpy((uint8_t*)hdr + 0, &raw_u32, 4);
    memcpy((uint8_t*)hdr + 4, &size,    4);
    return aligned;
}

static void aligned_free_guest(uint32_t ptr) {
    if (ptr == 0) return;
    void* hdr = wapi_wasm_ptr(ptr - WAPI_MEM_HDR, WAPI_MEM_HDR);
    if (!hdr) return;
    uint32_t raw = 0;
    memcpy(&raw, hdr, 4);
    if (raw) wasm_runtime_module_free(g_rt.module_inst, (uint64_t)raw);
}

static uint32_t aligned_usable_size(uint32_t ptr) {
    if (ptr == 0) return 0;
    void* hdr = wapi_wasm_ptr(ptr - WAPI_MEM_HDR, WAPI_MEM_HDR);
    if (!hdr) return 0;
    uint32_t size = 0;
    memcpy(&size, (uint8_t*)hdr + 4, 4);
    return size;
}

/* ---- ABI callbacks ---- */

static int32_t host_alloc(wasm_exec_env_t env,
                          int32_t size, int32_t alignment) {
    (void)env;
    if (size <= 0 || alignment <= 0) return 0;
    return (int32_t)aligned_alloc_guest((uint32_t)size, (uint32_t)alignment);
}

static void host_free(wasm_exec_env_t env, int32_t ptr) {
    (void)env;
    aligned_free_guest((uint32_t)ptr);
}

static int32_t host_realloc(wasm_exec_env_t env,
                            int32_t ptr, int32_t new_size, int32_t alignment) {
    (void)env;
    if (ptr == 0) return host_alloc(env, new_size, alignment);
    if (new_size <= 0) { host_free(env, ptr); return 0; }
    uint32_t align = (uint32_t)(alignment > 0 ? alignment : 16);
    if (!is_pow2(align)) return 0;

    uint32_t old_size = aligned_usable_size((uint32_t)ptr);
    if (old_size == 0) return 0;
    if ((uint32_t)new_size <= old_size) return ptr;

    uint32_t new_ptr = aligned_alloc_guest((uint32_t)new_size, align);
    if (new_ptr == 0) return 0;

    void* src = wapi_wasm_ptr((uint32_t)ptr, old_size);
    void* dst = wapi_wasm_ptr(new_ptr, old_size);
    if (src && dst) memcpy(dst, src, old_size);

    aligned_free_guest((uint32_t)ptr);
    return (int32_t)new_ptr;
}

static int32_t host_usable_size(wasm_exec_env_t env, int32_t ptr) {
    (void)env;
    return (int32_t)aligned_usable_size((uint32_t)ptr);
}

static NativeSymbol g_symbols[] = {
    { "alloc",       (void*)host_alloc,       "(ii)i",  NULL },
    { "free",        (void*)host_free,        "(i)",    NULL },
    { "realloc",     (void*)host_realloc,     "(iii)i", NULL },
    { "usable_size", (void*)host_usable_size, "(i)i",   NULL },
};

wapi_cap_registration_t wapi_host_memory_registration(void) {
    wapi_cap_registration_t r = {
        .module_name = "wapi_memory",
        .symbols = g_symbols,
        .count = (uint32_t)(sizeof(g_symbols) / sizeof(g_symbols[0])),
    };
    return r;
}
