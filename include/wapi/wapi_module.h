/**
 * WAPI - Shared Module Linking & Inter-Component Communication
 * Version 1.0.0
 *
 * This module defines how Wasm modules compose: shared dependencies,
 * cross-module function calls, and resource passing.
 *
 * KEY DESIGN: Modules that agree on a struct layout share memory directly
 * via pointers. No lifting, no lowering, no serialization. This is how
 * shared libraries have worked for 40 years. The Wasm sandbox provides
 * the security boundary, not per-module memory isolation.
 *
 * Import module: "wapi_module"
 *
 * @see wapi_context.h for allocator, I/O vtable, and context types.
 */

#ifndef WAPI_MODULE_H
#define WAPI_MODULE_H

#include "wapi_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Module Identity and Versioning
 * ============================================================
 * Every shared module publishes a version identifier that covers
 * its full API surface: function signatures, struct layouts, and
 * semantic guarantees. When any of these change, the version changes.
 *
 * This is semver applied to the ABI contract:
 * - Major: breaking changes (struct layout, removed functions)
 * - Minor: additive changes (new functions, new optional fields)
 * - Patch: bug fixes with identical ABI
 *
 * The runtime checks major version for compatibility. A module
 * requesting "ui-framework:v2" won't link against "ui-framework:v3".
 */

/**
 * Module descriptor published by shared modules.
 *
 * Chain a wapi_module_shared_memory_t to opt into shared linear
 * memory (no isolation). Without it, the module is sandboxed.
 *
 * Layout (28 bytes on wasm32, align 4):
 *   Offset  0: ptr      nextInChain
 *   Offset  4: ptr      name        Module name (e.g., "ui-framework")
 *   Offset  8: uint32_t name_len
 *   Offset 12: uint16_t version_major
 *   Offset 14: uint16_t version_minor
 *   Offset 16: uint16_t version_patch
 *   Offset 18: uint16_t _reserved
 *   Offset 20: ptr      abi_hash    Hash of the full ABI surface (SHA-256)
 *   Offset 24: uint32_t abi_hash_len (32 bytes for SHA-256)
 */
typedef struct wapi_module_desc_t {
    wapi_chained_struct_t* nextInChain;
    const char* name;
    wapi_size_t   name_len;
    uint16_t    version_major;
    uint16_t    version_minor;
    uint16_t    version_patch;
    uint16_t    _reserved;
    const void* abi_hash;
    wapi_size_t   abi_hash_len;
} wapi_module_desc_t;

/**
 * Shared memory configuration (Chained Struct).
 * Chain onto wapi_module_desc_t::nextInChain to opt into sharing
 * linear memory with the caller (no isolation). Without it, the
 * module is fully sandboxed.
 * sType = WAPI_STYPE_MODULE_SHARED_MEMORY.
 *
 * Layout (8 bytes on wasm32, align 4):
 *   Offset  0: wapi_chained_struct_t chain
 */
typedef struct wapi_module_shared_memory_t {
    wapi_chained_struct_t   chain;
    /* Future: allowed_regions, capability_grants, etc. */
} wapi_module_shared_memory_t;

/* ============================================================
 * Module Loading and Linking
 * ============================================================
 * The host/runtime manages shared module resolution and caching.
 * First app that needs "ui-framework:v2.3" triggers a download.
 * Second app finds it already cached.
 */

/**
 * Import descriptor: what a module needs from a shared dependency.
 *
 * Layout (16 bytes, align 4):
 *   Offset  0: ptr      module_name
 *   Offset  4: uint32_t module_name_len
 *   Offset  8: uint16_t min_major      Minimum major version
 *   Offset 10: uint16_t min_minor      Minimum minor version
 *   Offset 12: uint16_t max_major      Maximum major version (for compat)
 *   Offset 14: uint16_t _reserved
 */
typedef struct wapi_module_import_t {
    const char* module_name;
    wapi_size_t   module_name_len;
    uint16_t    min_major;
    uint16_t    min_minor;
    uint16_t    max_major;
    uint16_t    _reserved;
} wapi_module_import_t;

/**
 * Request loading of a shared module.
 * The runtime resolves the latest compatible version.
 *
 * @param import  Import descriptor.
 * @param module  [out] Module handle.
 * @return WAPI_OK on success, WAPI_ERR_NOENT if not found.
 */
WAPI_IMPORT(wapi_module, load)
wapi_result_t wapi_module_load(const wapi_module_import_t* import,
                            wapi_handle_t* module);

/**
 * Initialize a loaded shared module with a context.
 * The module's exported init function is called with the context.
 *
 * @param module  Module handle.
 * @param ctx     Context with allocator, I/O, GPU device, etc.
 * @return WAPI_OK on success.
 */
WAPI_IMPORT(wapi_module, init)
wapi_result_t wapi_module_init(wapi_handle_t module,
                            const wapi_context_t* ctx);

/**
 * Get a function pointer from a loaded module.
 *
 * @param module     Module handle.
 * @param func_name  Function name.
 * @param name_len   Name length.
 * @return Function pointer (as i32 in Wasm), or 0 if not found.
 */
WAPI_IMPORT(wapi_module, get_func)
uint32_t wapi_module_get_func(wapi_handle_t module, const char* func_name,
                             wapi_size_t name_len);

/**
 * Get the module's published descriptor.
 *
 * @param module  Module handle.
 * @param desc    [out] Module descriptor.
 */
WAPI_IMPORT(wapi_module, get_desc)
wapi_result_t wapi_module_get_desc(wapi_handle_t module, wapi_module_desc_t* desc);

/**
 * Release a shared module (decrements ref count).
 */
WAPI_IMPORT(wapi_module, release)
wapi_result_t wapi_module_release(wapi_handle_t module);

/* ============================================================
 * Memory Regions and Borrowing
 * ============================================================
 * For cooperating modules sharing linear memory, the runtime
 * can enforce borrowing rules: when Module A passes a buffer
 * to Module B, A cannot access that region until B returns it.
 *
 * This is Rust's ownership model enforced at runtime by the host,
 * not at compile time by the language.
 */

/**
 * Lend a memory region to another module.
 * The lending module cannot access this region until it's returned.
 *
 * @param ptr   Pointer to the start of the region.
 * @param len   Region length.
 * @param lease [out] Lease handle (used to reclaim).
 * @return WAPI_OK on success.
 */
WAPI_IMPORT(wapi_module, lend)
wapi_result_t wapi_module_lend(const void* ptr, wapi_size_t len,
                            wapi_handle_t* lease);

/**
 * Return a borrowed memory region.
 * The lending module can access the region again.
 *
 * @param lease  Lease handle from wapi_module_lend.
 */
WAPI_IMPORT(wapi_module, return_lease)
wapi_result_t wapi_module_return_lease(wapi_handle_t lease);

/**
 * Check if a memory region is currently lent out.
 *
 * @param ptr  Pointer to check.
 * @param len  Region length.
 * @return 1 if lent (inaccessible), 0 if available.
 */
WAPI_IMPORT(wapi_module, is_lent)
wapi_bool_t wapi_module_is_lent(const void* ptr, wapi_size_t len);

/* ============================================================
 * Module Cache
 * ============================================================
 * The runtime maintains a cache of shared modules. This is
 * analogous to the browser's HTTP cache for JS libraries or
 * the OS's shared library cache (/lib, /usr/lib).
 */

/**
 * Query whether a module is already cached locally.
 */
WAPI_IMPORT(wapi_module, is_cached)
wapi_bool_t wapi_module_is_cached(const char* module_name, wapi_size_t name_len,
                               uint16_t major);

/**
 * Pre-fetch a module into the cache for future use.
 * Non-blocking; the download happens in the background.
 */
WAPI_IMPORT(wapi_module, prefetch)
wapi_result_t wapi_module_prefetch(const wapi_module_import_t* import);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_MODULE_H */
