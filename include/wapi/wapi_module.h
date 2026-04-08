/**
 * WAPI - Runtime Module Linking
 * Version 1.0.0
 *
 * This module defines how Wasm modules compose at RUNTIME: loading
 * isolated modules by content hash, calling their functions, and
 * transferring data across memory boundaries.
 *
 * TWO WORLDS of module composition:
 *
 *   BUILD-TIME (shared memory, one binary):
 *     Libraries are linked into a single Wasm module. They share
 *     linear memory naturally. Pass pointers, call functions —
 *     this is how shared libraries have worked for 40 years.
 *     No WAPI mechanism needed; it's just C linking.
 *
 *   RUNTIME (isolated memory, separate modules):
 *     Each module has its own linear memory. The host mediates all
 *     data transfer. This file defines the runtime linking API:
 *       - Load modules by content hash (SHA-256)
 *       - Map buffers across module boundaries
 *       - Create allocator handles for variable-length output
 *       - Call functions with host-mediated argument passing
 *       - Control child I/O via policy
 *
 * IDENTITY: Modules are content-addressed by the SHA-256 hash of their
 * Wasm binary. The hash IS the identity. Name and version are human-
 * readable metadata, not the linking key. This means:
 *   - The runtime fetches bytes from any source (registry, cache, peer)
 *   - Verification is built in: hash the bytes, compare to expected hash
 *   - No mutable names that resolve differently at different times
 *   - Two modules with the same hash are the same module, period
 *
 * Import module: "wapi_module"
 */

#ifndef WAPI_MODULE_H
#define WAPI_MODULE_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Module Descriptor
 * ============================================================
 * Published by modules at init time. Human-readable metadata
 * about the module. Not used for linking (that's by hash).
 */

typedef struct wapi_module_desc_t {
    wapi_string_view_t  name;       /* Human-readable name (e.g., "image-decoder") */
    wapi_version_t      version;    /* Semver of the module's ABI surface */
} wapi_module_desc_t;

/* ============================================================
 * Content Hash
 * ============================================================
 * SHA-256 hash of the module's Wasm binary. Fixed 32 bytes.
 * This is the canonical module identity for linking.
 */

typedef struct wapi_module_hash_t {
    uint8_t bytes[32];
} wapi_module_hash_t;

_Static_assert(sizeof(wapi_module_hash_t) == 32,
               "wapi_module_hash_t must be 32 bytes");

/* ============================================================
 * Value Type (for cross-module call arguments)
 * ============================================================
 * Represents a Wasm value type passed across module boundaries.
 * The host reads kind to determine which union member to use.
 *
 * Layout (16 bytes, align 8):
 *   Offset 0: uint8_t kind (0=i32, 1=i64, 2=f32, 3=f64)
 *   Offset 1: uint8_t _pad[7]
 *   Offset 8: union   of (value)
 */

#define WAPI_VAL_I32 0
#define WAPI_VAL_I64 1
#define WAPI_VAL_F32 2
#define WAPI_VAL_F64 3

typedef struct wapi_val_t {
    uint8_t kind;
    uint8_t _pad[7];
    union {
        int32_t  i32;
        int64_t  i64;
        float    f32;
        double   f64;
    } of;
} wapi_val_t;

_Static_assert(sizeof(wapi_val_t) == 16, "wapi_val_t must be 16 bytes");
_Static_assert(offsetof(wapi_val_t, kind) == 0, "");
_Static_assert(offsetof(wapi_val_t, of)   == 8, "");

/* ============================================================
 * Module Loading and Linking
 * ============================================================
 * Modules are identified by content hash (SHA-256 of the Wasm binary)
 * and located by URL. Like Zig packages: URL says where, hash says what.
 * No central registry required — anyone can host modules anywhere.
 *
 * The runtime:
 *   1. Checks local cache by hash
 *   2. If not cached, fetches from the provided URL
 *   3. Hashes the fetched bytes, rejects on mismatch
 *   4. Caches by hash, instantiates, returns a handle
 *
 * The URL is a fetch hint, not an identity. Two calls with different
 * URLs but the same hash produce the same module. The URL is ignored
 * entirely on cache hit.
 */

/**
 * Load a module by content hash, fetching from URL if not cached.
 *
 * @param hash    SHA-256 hash of the expected Wasm binary.
 * @param url     Fetch URL (http, file, etc.). Ignored on cache hit.
 *                NULL to require the module already be cached.
 * @param module  [out] Module handle.
 * @return WAPI_OK on success, WAPI_ERR_NOENT if not cached and
 *         url is NULL, or fetch failed.
 */
WAPI_IMPORT(wapi_module, load)
wapi_result_t wapi_module_load(const wapi_module_hash_t* hash,
                               wapi_string_view_t url,
                               wapi_handle_t* module);

/**
 * Get a function handle from a loaded module by name.
 * The returned handle is used with wapi_module_call.
 *
 * @param module     Module handle.
 * @param func_name  Function name (UTF-8).
 * @param func       [out] Function handle, or WAPI_HANDLE_INVALID if not found.
 * @return WAPI_OK on success, WAPI_ERR_NOENT if function not found.
 */
WAPI_IMPORT(wapi_module, get_func)
wapi_result_t wapi_module_get_func(wapi_handle_t module,
                                   wapi_string_view_t func_name,
                                   wapi_handle_t* func);

/**
 * Get the module's published descriptor (name, version).
 *
 * @param module  Module handle.
 * @param desc    [out] Module descriptor.
 */
WAPI_IMPORT(wapi_module, get_desc)
wapi_result_t wapi_module_get_desc(wapi_handle_t module,
                                   wapi_module_desc_t* desc);

/**
 * Get the content hash of a loaded module.
 *
 * @param module  Module handle.
 * @param hash    [out] SHA-256 hash of the module's Wasm binary.
 */
WAPI_IMPORT(wapi_module, get_hash)
wapi_result_t wapi_module_get_hash(wapi_handle_t module,
                                   wapi_module_hash_t* hash);

/**
 * Release a module handle (decrements ref count).
 */
WAPI_IMPORT(wapi_module, release)
wapi_result_t wapi_module_release(wapi_handle_t module);

/* ============================================================
 * Cross-Module Calling
 * ============================================================
 * Call an exported function on an isolated module. The host
 * mediates all data transfer between the caller and the child.
 *
 * Arguments and results are Wasm value types (i32, i64, f32, f64).
 * To pass structured data, map buffers first (wapi_module_map),
 * then pass the child-side pointer as an i32 argument.
 *
 * Typical pattern:
 *   1. Map input buffer:  wapi_module_map(mod, data, len, READ, &child_ptr)
 *   2. Create allocator:  wapi_module_alloc_create(mod, &alloc)
 *   3. Call function:     wapi_module_call(mod, func, args, ...)
 *   4. Read output:       wapi_module_alloc_get(alloc, 0, &out, &out_len)
 *   5. Cleanup:           wapi_module_unmap(mod, child_ptr)
 *                         wapi_module_alloc_destroy(alloc)
 */

/**
 * Call a function on an isolated module.
 *
 * @param module   Module handle.
 * @param func     Function handle from wapi_module_get_func.
 * @param args     Array of argument values.
 * @param nargs    Number of arguments.
 * @param results  [out] Array of result values.
 * @param nresults Number of result slots.
 * @return WAPI_OK on success, or the child's error code.
 */
WAPI_IMPORT(wapi_module, call)
wapi_result_t wapi_module_call(wapi_handle_t module, wapi_handle_t func,
                               const wapi_val_t* args, wapi_size_t nargs,
                               wapi_val_t* results, wapi_size_t nresults);

/* ============================================================
 * Buffer Mapping (Caller -> Child)
 * ============================================================
 * Map a region of the caller's linear memory into a child module's
 * address space. The host copies or maps the data; the child gets
 * a pointer in its own linear memory.
 *
 * Mappings are ephemeral — intended for the duration of one or
 * more calls, then unmapped. The host may copy data (not share
 * physical pages), so changes by the child are visible only if
 * WAPI_MAP_WRITE is set and the host copies back on unmap.
 */

#define WAPI_MAP_READ      0x01  /* Child gets read access */
#define WAPI_MAP_WRITE     0x02  /* Child gets write access (copied back on unmap) */
#define WAPI_MAP_READWRITE (WAPI_MAP_READ | WAPI_MAP_WRITE)

/**
 * Map a region of caller's memory into a child module.
 *
 * @param module     Child module handle.
 * @param src        Pointer to data in caller's memory.
 * @param len        Number of bytes to map.
 * @param flags      WAPI_MAP_READ, WAPI_MAP_WRITE, or WAPI_MAP_READWRITE.
 * @param child_ptr  [out] Address in the child's linear memory.
 * @return WAPI_OK on success, WAPI_ERR_NOMEM if child can't allocate.
 */
WAPI_IMPORT(wapi_module, map)
wapi_result_t wapi_module_map(wapi_handle_t module, const void* src,
                              wapi_size_t len, uint32_t flags,
                              uint64_t* child_ptr);

/**
 * Unmap a previously mapped region.
 * If WAPI_MAP_WRITE was set, the host copies modified data back
 * to the caller's original buffer before unmapping.
 *
 * @param module     Child module handle.
 * @param child_ptr  Address returned by wapi_module_map.
 * @return WAPI_OK on success.
 */
WAPI_IMPORT(wapi_module, unmap)
wapi_result_t wapi_module_unmap(wapi_handle_t module, uint64_t child_ptr);

/* ============================================================
 * Allocator Handles (Variable-Length Output)
 * ============================================================
 * When a child function needs to produce variable-length output,
 * the caller creates an allocator handle before the call. The
 * child uses its normal wapi_memory imports to allocate; the host
 * intercepts and tracks these allocations. After the call returns,
 * the caller reads the allocations via wapi_module_alloc_get.
 *
 * This is the Zig-style per-method allocator pattern, adapted
 * for isolated module boundaries via host mediation.
 */

/**
 * Create a host-mediated allocator for a child module.
 * While active, the child's wapi_memory.alloc calls are tracked.
 *
 * @param module       Child module handle.
 * @param alloc_handle [out] Allocator handle.
 * @return WAPI_OK on success.
 */
WAPI_IMPORT(wapi_module, alloc_create)
wapi_result_t wapi_module_alloc_create(wapi_handle_t module,
                                       wapi_handle_t* alloc_handle);

/**
 * Read what the child allocated during a call.
 *
 * @param alloc_handle  Allocator handle from alloc_create.
 * @param index         Allocation index (0, 1, ... in allocation order).
 * @param ptr           [out] Pointer to the data in caller's memory.
 *                      The host copies from the child's allocation.
 * @param len           [out] Size of the allocation in bytes.
 * @return WAPI_OK on success, WAPI_ERR_RANGE if index out of bounds.
 */
WAPI_IMPORT(wapi_module, alloc_get)
wapi_result_t wapi_module_alloc_get(wapi_handle_t alloc_handle,
                                    uint32_t index,
                                    void** ptr, wapi_size_t* len);

/**
 * Destroy an allocator handle and free all tracked allocations
 * in both the child's and caller's memory.
 *
 * @param alloc_handle  Allocator handle.
 * @return WAPI_OK on success.
 */
WAPI_IMPORT(wapi_module, alloc_destroy)
wapi_result_t wapi_module_alloc_destroy(wapi_handle_t alloc_handle);

/* ============================================================
 * I/O Policy (Parent Controls Child's I/O)
 * ============================================================
 * By default, runtime modules have no I/O access. The parent
 * must explicitly grant I/O capabilities via policy flags.
 * The host enforces this when the child calls wapi_io imports.
 */

#define WAPI_IO_POLICY_NONE     0x00  /* No I/O allowed (default) */
#define WAPI_IO_POLICY_LOG      0x01  /* Logging only */
#define WAPI_IO_POLICY_FS_READ  0x02  /* Filesystem read */
#define WAPI_IO_POLICY_FS_WRITE 0x04  /* Filesystem write */
#define WAPI_IO_POLICY_NET      0x08  /* Networking */
#define WAPI_IO_POLICY_ALL      0xFF  /* Unrestricted */

/**
 * Set the I/O policy for a child module.
 *
 * @param module        Child module handle.
 * @param policy_flags  Bitwise OR of WAPI_IO_POLICY_* flags.
 * @return WAPI_OK on success.
 */
WAPI_IMPORT(wapi_module, set_io_policy)
wapi_result_t wapi_module_set_io_policy(wapi_handle_t module,
                                        uint32_t policy_flags);

/* ============================================================
 * Module Cache
 * ============================================================
 * The runtime maintains a cache of modules keyed by content hash.
 * Analogous to the browser's HTTP cache for JS libraries or a Nix store.
 */

/**
 * Query whether a module is already cached locally.
 *
 * @param hash  Content hash of the module.
 * @return 1 if cached, 0 if not.
 */
WAPI_IMPORT(wapi_module, is_cached)
wapi_bool_t wapi_module_is_cached(const wapi_module_hash_t* hash);

/**
 * Pre-fetch a module into the cache for future use.
 * Non-blocking; the download happens in the background.
 *
 * @param hash    Content hash of the module to fetch.
 * @param url     Fetch URL. Required (unlike load, prefetch always fetches).
 */
WAPI_IMPORT(wapi_module, prefetch)
wapi_result_t wapi_module_prefetch(const wapi_module_hash_t* hash,
                                   wapi_string_view_t url);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_MODULE_H */
