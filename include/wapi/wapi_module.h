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
 * Published by shared modules at init time. Human-readable
 * metadata about the module. Not used for linking (that's by hash).
 *
 * Chain a wapi_module_shared_memory_t to opt into shared linear
 * memory (no isolation). Without it, the module is sandboxed.
 */

typedef struct wapi_module_desc_t {
    wapi_chained_struct_t* nextInChain;
    wapi_string_view_t  name;       /* Human-readable name (e.g., "ui-framework") */
    wapi_version_t      version;    /* Semver of the module's ABI surface */
} wapi_module_desc_t;

/**
 * Shared memory configuration (Chained Struct).
 * Chain onto wapi_module_desc_t::nextInChain to opt into sharing
 * linear memory with the caller (no isolation). Without it, the
 * module is fully sandboxed.
 * sType = WAPI_STYPE_MODULE_SHARED_MEMORY.
 */
typedef struct wapi_module_shared_memory_t {
    wapi_chained_struct_t   chain;
    /* Future: allowed_regions, capability_grants, etc. */
} wapi_module_shared_memory_t;

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
 * Initialize a loaded module with a context.
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
 * Get a function pointer from a loaded module by name.
 *
 * @param module     Module handle.
 * @param func_name  Function name (UTF-8).
 * @return Function pointer (as i32 in Wasm), or 0 if not found.
 */
WAPI_IMPORT(wapi_module, get_func)
uint32_t wapi_module_get_func(wapi_handle_t module, wapi_string_view_t func_name);

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
 * Memory Sharing
 * ============================================================
 * Two modes, determined by the module descriptor:
 *
 * SHARED MEMORY (opt-in via wapi_module_shared_memory_t chain):
 *   Both modules share the same linear memory. Pointers work
 *   directly across function calls. No runtime enforcement —
 *   like threads in a process. Use for performance-critical
 *   dependencies (GPU libraries, physics engines, allocators).
 *   No special API needed: call functions, pass pointers.
 *
 * ISOLATED (default):
 *   Separate linear memories. Data transfer uses segments:
 *   host-managed memory regions with ownership and borrowing.
 *   The host controls all mappings, enforces permissions, and
 *   can revoke access — none of this is possible with raw
 *   pointers in linear memory.
 *
 * Segments and Borrowing (isolated modules only)
 * -----------------------------------------------
 * A segment is a host-managed memory region. The creating module
 * owns it and gets read/write access. To share data, the owner
 * lends the segment to a borrower:
 *
 *   Exclusive lend (readonly=false):
 *     Owner's access is suspended. Borrower gets read/write.
 *     Like Rust's &mut T — one writer, no readers.
 *
 *   Shared lend (readonly=true):
 *     Owner retains read access. Borrower gets read-only.
 *     Like Rust's &T — multiple readers, no writers.
 *
 * The host enforces this by controlling the memory mappings.
 * On lend, it maps the segment into the borrower's linear memory.
 * On return/revoke, it unmaps and restores the owner's access.
 */

/**
 * Create a shared memory segment.
 * The calling module owns the segment and gets read/write access.
 * The segment is immediately mapped into the owner's linear memory.
 *
 * @param size     Segment size in bytes.
 * @param segment  [out] Segment handle.
 * @return WAPI_OK on success, WAPI_ERR_NOMEM if allocation fails.
 */
WAPI_IMPORT(wapi_module, create_segment)
wapi_result_t wapi_module_create_segment(wapi_size_t size,
                                         wapi_handle_t* segment);

/**
 * Get the owner's pointer to a segment in their linear memory.
 *
 * @param segment  Segment handle (must be owned by the caller).
 * @param ptr      [out] Pointer to the segment data.
 * @return WAPI_OK on success, WAPI_ERR_BADF if not owned by caller,
 *         WAPI_ERR_ACCES if access is suspended (exclusive lend active).
 */
WAPI_IMPORT(wapi_module, segment_ptr)
wapi_result_t wapi_module_segment_ptr(wapi_handle_t segment, void** ptr);

/**
 * Lend a segment to another module.
 * The host maps the segment into the borrower's linear memory.
 *
 * If readonly is false (exclusive lend):
 *   Owner's access is suspended until the lease is returned/revoked.
 *   Borrower gets read/write. Only one exclusive lease at a time.
 *
 * If readonly is true (shared lend):
 *   Owner retains read access. Borrower gets read-only.
 *   Multiple shared leases can coexist, but not with an exclusive one.
 *
 * @param segment  Segment handle (must be owned by the caller).
 * @param borrower Module handle of the borrower.
 * @param readonly Non-zero for shared (read-only) lend.
 * @param lease    [out] Lease handle.
 * @return WAPI_OK on success, WAPI_ERR_BUSY if an incompatible
 *         lease is already active.
 */
WAPI_IMPORT(wapi_module, lend)
wapi_result_t wapi_module_lend(wapi_handle_t segment,
                               wapi_handle_t borrower,
                               wapi_bool_t readonly,
                               wapi_handle_t* lease);

/**
 * Return a borrowed segment.
 * Called by the borrower. The host unmaps the segment from the
 * borrower's memory and restores the owner's full access.
 *
 * @param lease  Lease handle from wapi_module_lend.
 */
WAPI_IMPORT(wapi_module, return_lease)
wapi_result_t wapi_module_return_lease(wapi_handle_t lease);

/**
 * Revoke a lease (owner-initiated).
 * Same effect as return_lease but called by the owner to forcibly
 * reclaim the segment. The borrower's mapping is removed.
 *
 * @param lease  Lease handle from wapi_module_lend.
 */
WAPI_IMPORT(wapi_module, revoke_lease)
wapi_result_t wapi_module_revoke_lease(wapi_handle_t lease);

/**
 * Destroy a shared memory segment.
 * All active leases are automatically revoked. The segment is
 * unmapped from all modules and freed.
 *
 * @param segment  Segment handle (must be owned by the caller).
 */
WAPI_IMPORT(wapi_module, destroy_segment)
wapi_result_t wapi_module_destroy_segment(wapi_handle_t segment);

/* ============================================================
 * Module Cache
 * ============================================================
 * The runtime maintains a cache of shared modules keyed by
 * content hash. Analogous to the browser's HTTP cache for JS
 * libraries or a Nix store.
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
