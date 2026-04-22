/**
 * WAPI reactor shim (C, guest-side)
 *
 * Every WAPI module needs a concrete `wapi_io_t` and `wapi_allocator_t`
 * to hand back from `wapi_io_get()` / `wapi_allocator_get()`. This
 * translation unit provides both by trampolining to the
 * `wapi_io_bridge.*` host imports declared in wapi.h §4, and by
 * running a bump allocator over linear memory above `__heap_base`.
 *
 * Link this file into every guest wasm alongside the application's
 * own object files.
 */

#include <wapi/wapi.h>

/* ============================================================
 * wapi_io_bridge host imports (wasm32)
 * ============================================================
 *
 * The host registers these in module "wapi_io_bridge". See
 * runtime/desktop/src/wapi_host_io.c for the Wasmtime bindings.
 */

#define WAPI_IO_IMPORT(name) \
    __attribute__((import_module("wapi_io_bridge"), import_name(#name)))

WAPI_IO_IMPORT(submit)
int32_t wapi_io_bridge_submit(const wapi_io_op_t* ops, wapi_size_t count);

WAPI_IO_IMPORT(cancel)
wapi_result_t wapi_io_bridge_cancel(uint64_t user_data);

WAPI_IO_IMPORT(poll)
int32_t wapi_io_bridge_poll(wapi_event_t* event);

WAPI_IO_IMPORT(wait)
int32_t wapi_io_bridge_wait(wapi_event_t* event, int32_t timeout_ms);

WAPI_IO_IMPORT(flush)
void wapi_io_bridge_flush(uint32_t event_type);

WAPI_IO_IMPORT(cap_supported)
wapi_bool_t wapi_io_bridge_cap_supported(wapi_stringview_t name);

WAPI_IO_IMPORT(cap_version)
wapi_result_t wapi_io_bridge_cap_version(wapi_stringview_t name,
                                         wapi_version_t* out);

WAPI_IO_IMPORT(cap_query)
wapi_result_t wapi_io_bridge_cap_query(wapi_stringview_t capability,
                                       wapi_cap_state_t* out);

WAPI_IO_IMPORT(namespace_register)
wapi_result_t wapi_io_bridge_namespace_register(wapi_stringview_t name,
                                                uint16_t* out_id);

WAPI_IO_IMPORT(namespace_name)
wapi_result_t wapi_io_bridge_namespace_name(uint16_t id,
                                            char* buf,
                                            wapi_size_t buf_len,
                                            wapi_size_t* out_len);

/* ============================================================
 * Vtable thunks
 * ============================================================
 *
 * The host imports live in their own function-type space; the
 * vtable fields need function pointers whose signatures include the
 * `void* impl` argument. These thunks drop `impl` and forward.
 */

static int32_t io_submit(void* impl, const wapi_io_op_t* ops, wapi_size_t count) {
    (void)impl;
    return wapi_io_bridge_submit(ops, count);
}
static wapi_result_t io_cancel(void* impl, uint64_t user_data) {
    (void)impl;
    return wapi_io_bridge_cancel(user_data);
}
static int32_t io_poll(void* impl, wapi_event_t* event) {
    (void)impl;
    return wapi_io_bridge_poll(event);
}
static int32_t io_wait(void* impl, wapi_event_t* event, int32_t timeout_ms) {
    (void)impl;
    return wapi_io_bridge_wait(event, timeout_ms);
}
static void io_flush(void* impl, uint32_t event_type) {
    (void)impl;
    wapi_io_bridge_flush(event_type);
}
static wapi_bool_t io_cap_supported(void* impl, wapi_stringview_t name) {
    (void)impl;
    return wapi_io_bridge_cap_supported(name);
}
static wapi_result_t io_cap_version(void* impl, wapi_stringview_t name,
                                    wapi_version_t* out) {
    (void)impl;
    return wapi_io_bridge_cap_version(name, out);
}
static wapi_result_t io_cap_query(void* impl, wapi_stringview_t cap,
                                  wapi_cap_state_t* out) {
    (void)impl;
    return wapi_io_bridge_cap_query(cap, out);
}
static wapi_result_t io_namespace_register(void* impl,
                                           wapi_stringview_t name,
                                           uint16_t* out_id) {
    (void)impl;
    return wapi_io_bridge_namespace_register(name, out_id);
}
static wapi_result_t io_namespace_name(void* impl, uint16_t id,
                                       char* buf, wapi_size_t buf_len,
                                       wapi_size_t* out_len) {
    (void)impl;
    return wapi_io_bridge_namespace_name(id, buf, buf_len, out_len);
}

static const wapi_io_t g_wapi_io = {
    .impl               = 0,
    .submit             = io_submit,
    .cancel             = io_cancel,
    .poll               = io_poll,
    .wait               = io_wait,
    .flush              = io_flush,
    .cap_supported      = io_cap_supported,
    .cap_version        = io_cap_version,
    .cap_query          = io_cap_query,
    .namespace_register = io_namespace_register,
    .namespace_name     = io_namespace_name,
};

const wapi_io_t* wapi_io_get(void) {
    return &g_wapi_io;
}

/* ============================================================
 * Bump allocator
 * ============================================================
 *
 * Linear-memory bump allocator anchored at `__heap_base` (provided
 * by wasm-ld). memory.grow is invoked when we run out of pages. Free
 * is a no-op; realloc copies into a fresh allocation. This matches
 * the "host-controlled, module-owned, deterministic" contract — the
 * host does not touch the guest heap.
 */

extern unsigned char __heap_base;

#define WAPI_WASM_PAGE_SIZE 65536u

static uintptr_t g_heap_top;      /* Current bump pointer (byte offset) */
static uintptr_t g_heap_end;      /* End of committed memory (byte offset) */

static inline uintptr_t align_up(uintptr_t v, wapi_size_t a) {
    if (a == 0) a = 1;
    return (v + (uintptr_t)(a - 1)) & ~(uintptr_t)(a - 1);
}

static void ensure_init(void) {
    if (g_heap_top != 0) return;
    g_heap_top = (uintptr_t)&__heap_base;
    /* wasm32 memory.size returns pages; convert to bytes. */
    g_heap_end = (uintptr_t)__builtin_wasm_memory_size(0) * WAPI_WASM_PAGE_SIZE;
}

static int grow_to(uintptr_t new_top) {
    if (new_top <= g_heap_end) return 1;
    uintptr_t need_bytes   = new_top - g_heap_end;
    uintptr_t need_pages   = (need_bytes + WAPI_WASM_PAGE_SIZE - 1) / WAPI_WASM_PAGE_SIZE;
    intptr_t  prev_pages   = __builtin_wasm_memory_grow(0, (intptr_t)need_pages);
    if (prev_pages < 0) return 0;
    g_heap_end = (uintptr_t)__builtin_wasm_memory_size(0) * WAPI_WASM_PAGE_SIZE;
    return 1;
}

/* Allocation header, placed immediately before the returned pointer.
 * Stores the size so realloc / usable_size can work. */
typedef struct alloc_header_t {
    wapi_size_t size;
    wapi_size_t align;
} alloc_header_t;

static void* bump_alloc(void* impl, wapi_size_t size, wapi_size_t align) {
    (void)impl;
    ensure_init();
    if (align < 8) align = 8;

    uintptr_t hdr_addr  = align_up(g_heap_top, 8);
    uintptr_t data_addr = align_up(hdr_addr + sizeof(alloc_header_t), align);
    uintptr_t new_top   = data_addr + size;
    if (!grow_to(new_top)) return 0;

    alloc_header_t* h = (alloc_header_t*)hdr_addr;
    h->size  = size;
    h->align = align;
    g_heap_top = new_top;
    return (void*)data_addr;
}

static void bump_free(void* impl, void* ptr) {
    /* Bump allocator: free is a no-op. A real freelist allocator
     * would reclaim here; the reactor deliberately keeps this simple
     * since the host does not dictate guest-heap policy. */
    (void)impl; (void)ptr;
}

static void* bump_realloc(void* impl, void* ptr, wapi_size_t new_size,
                          wapi_size_t align) {
    (void)impl;
    if (!ptr) return bump_alloc(0, new_size, align);

    alloc_header_t* h = (alloc_header_t*)((uintptr_t)ptr - sizeof(alloc_header_t));
    wapi_size_t old_size = h->size;

    void* np = bump_alloc(0, new_size, align);
    if (!np) return 0;

    wapi_size_t copy = old_size < new_size ? old_size : new_size;
    unsigned char* dst = (unsigned char*)np;
    unsigned char* src = (unsigned char*)ptr;
    for (wapi_size_t i = 0; i < copy; i++) dst[i] = src[i];
    return np;
}

static const wapi_allocator_t g_wapi_alloc = {
    .impl       = 0,
    .alloc_fn   = bump_alloc,
    .free_fn    = bump_free,
    .realloc_fn = bump_realloc,
};

const wapi_allocator_t* wapi_allocator_get(void) {
    return &g_wapi_alloc;
}
