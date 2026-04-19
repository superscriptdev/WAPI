/**
 * WAPI - Runtime Module Linking
 * Version 1.0.0
 *
 * This module defines how Wasm modules compose at RUNTIME: loading
 * isolated modules by content hash, calling their functions, and
 * sharing data via a borrow system on shared memory.
 *
 * I/O MODEL: Every module (top-level or child) obtains its own
 * I/O and allocator vtables from the host via wapi_io_get() and
 * wapi_allocator_get(). These are host-controlled and cannot be
 * influenced by any parent — safe for shared instances. For
 * capabilities beyond the sandbox, callers pass vtables explicitly.
 *
 * MEMORY MODEL:
 *
 *   Memory 0 (private):
 *     Each module's own linear memory. Stack, globals, internal state.
 *     Fully isolated — no other module can access it. Allocated via
 *     the module's wapi_allocator_t (from wapi_allocator_get()).
 *
 *   Memory 1 (shared):
 *     A single shared linear memory owned by the application. All
 *     loaded child modules share the same memory 1 instance.
 *
 *     OWNERSHIP: The application always has full access to all of
 *     shared memory — it owns it. Child modules can only access
 *     regions they have been explicitly lent via wapi_module_lend.
 *     Both the application and children can allocate regions in
 *     shared memory (wapi_module_shared_alloc), but children can
 *     only access their own allocations or regions lent to them.
 *
 *     This means child output is simple: the child allocates in
 *     shared memory, writes its result, returns the offset. The
 *     application reads it directly — no borrow needed.
 *
 *     Two access paths:
 *       - Multi-memory: import memory 1, load/store directly (zero copy)
 *       - Host-call: wapi_module_shared_read/write (one copy, portable C)
 *
 * BUILD-TIME vs RUNTIME:
 *
 *   Build-time linked libraries share memory 0 naturally. They take
 *   wapi_allocator_t vtables as explicit parameters (Zig-style).
 *   No WAPI mechanism needed — it's just C linking.
 *
 *   Runtime linked modules each have their own memory 0. They exchange
 *   data through shared memory (memory 1) with the borrow system, or
 *   via explicit copies (wapi_module_copy_in).
 *
 *   A library can be written to work in both modes: accept a
 *   wapi_allocator_t for private memory operations. At build time,
 *   the vtable points to memory 0 directly. At runtime, same code.
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
    wapi_stringview_t  name;       /* Human-readable name (e.g., "image-decoder") */
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
_Static_assert(_Alignof(wapi_module_hash_t) == 1,
               "wapi_module_hash_t must be 1-byte aligned");

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
_Static_assert(_Alignof(wapi_val_t) == 8, "wapi_val_t must be 8-byte aligned");
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

/* ------------------------------------------------------------
 * Deployment modes
 * ------------------------------------------------------------
 * The same Wasm binary can be deployed in two different ways.
 * The binary does not declare which — the caller picks per load.
 *
 *   LIBRARY MODE (wapi_module_load):
 *     Fresh instance per call. Private linear memory. Isolated
 *     state. Equivalent to linking libfoo.a into your program.
 *     Cheap to tear down, no sharing concerns.
 *
 *   SERVICE MODE (wapi_module_join):
 *     Refcounted shared instance. First caller starts it;
 *     subsequent callers attach. Alive until the last handle is
 *     released. Equivalent to talking to a running daemon.
 *     One instance, one linear memory, many callers — the memory
 *     savings compound for modules with heavy data tables
 *     (Unicode, fonts, ML weights, codecs, tzdata).
 *
 * Both modes share the same bytes cache (hash-addressed), so a
 * module is never downloaded or compiled more than once per host
 * regardless of mode. Service mode additionally shares the live
 * instance across all callers agreeing on the same (hash, name).
 *
 * Lifetime:
 *   - Library: caller-controlled via wapi_module_release.
 *   - Service: host-controlled via refcounting. An instance stays
 *     alive as long as at least one handle references it. The host
 *     MAY park an idle instance (refcount zero) for a short grace
 *     period to absorb join/release churn before tearing it down.
 *
 * Graceful teardown for both modes uses the host-module shutdown
 * handshake defined in wapi.h (WAPI_EVENT_QUIT / wapi_exit).
 * wapi_module has no shutdown API of its own — every instance,
 * whether loaded as a library, joined as a service, or running as
 * the top-level app, exits through the same path.
 */

/**
 * Load a module by content hash, fetching from URL if not cached.
 *
 * LIBRARY MODE: every call produces a fresh, private instance with
 * its own linear memory. Callers do not share state.
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
                               wapi_stringview_t url,
                               wapi_handle_t* module);

/**
 * Join (or start) a shared service instance of a module.
 *
 * SERVICE MODE: if an instance with the same (hash, name) is already
 * running, attach to it and increment the refcount. Otherwise start
 * a new instance, then attach. The returned handle must be released
 * via wapi_module_release when the caller no longer needs the
 * service — the host destroys the instance only when all handles
 * have been released.
 *
 * Two callers joining with the same (hash, name) get handles that
 * refer to the SAME running instance. Different names under the
 * same hash produce distinct instances (e.g., two independent
 * copies of a crypto service with different key material).
 *
 * @param hash    SHA-256 hash of the expected Wasm binary.
 * @param url     Fetch URL. Ignored on bytes-cache hit, required
 *                otherwise. A join that finds the service already
 *                running uses neither cache nor URL.
 * @param name    Human-readable service name. Empty string for
 *                "the default instance of this hash". Callers that
 *                agree on the name agree on the instance.
 * @param module  [out] Module handle.
 * @return WAPI_OK on success,
 *         WAPI_ERR_NOENT if not cached and url is absent or fetch failed,
 *         WAPI_ERR_LOOP if joining this service would form a cycle
 *                       in the caller's dependency graph.
 */
WAPI_IMPORT(wapi_module, join)
wapi_result_t wapi_module_join(const wapi_module_hash_t* hash,
                               wapi_stringview_t url,
                               wapi_stringview_t name,
                               wapi_handle_t* module);

/* Shutdown: see wapi.h. All modules (app, library, service)
 * exit through the shared wapi_exit / WAPI_EVENT_QUIT handshake. */

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
                                   wapi_stringview_t func_name,
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
 * To pass data, use shared memory with borrows or copy_in, then
 * pass the offset/pointer as an i32 argument.
 *
 * Input pattern (shared memory, zero-copy):
 *   1. Allocate shared: off = wapi_module_shared_alloc(size, align)
 *   2. Write data:      wapi_module_shared_write(off, data, len)
 *   3. Lend to child:   wapi_module_lend(mod, off, WAPI_LEND_READ, &b)
 *   4. Call function:   wapi_module_call(mod, func, [off, len], ...)
 *   5. Reclaim:         wapi_module_reclaim(b)
 *   6. Free shared:     wapi_module_shared_free(off)
 *
 * Output pattern (child allocates, app reads directly):
 *   1. Call function:   wapi_module_call(mod, func, args, ...)
 *      (child allocates in shared memory, returns offset)
 *   2. Read result:     wapi_module_shared_read(result_off, buf, len)
 *      (app owns shared memory — no borrow needed)
 *
 * Simple pattern (copy, no shared memory needed):
 *   1. Copy in:         wapi_module_copy_in(mod, data, len, &child_ptr)
 *   2. Call function:   wapi_module_call(mod, func, [child_ptr, len], ...)
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
 * Shared Memory Allocation
 * ============================================================
 * Allocate and free regions in the application's shared memory
 * (Wasm memory 1). The host manages the allocator. Any module
 * (application or child) can allocate. Allocations define borrow
 * granularity — you can only lend what you allocated.
 *
 * Ownership rules:
 *   - The application owns all shared memory. It can read, write,
 *     free, and lend ANY region — regardless of who allocated it.
 *   - A child module can access: (a) its own allocations, and
 *     (b) regions explicitly lent to it via wapi_module_lend.
 *   - A child can only free and lend its own allocations.
 *   - Freeing a region with active borrows fails (WAPI_ERR_BUSY).
 *     Reclaim all borrows before freeing.
 *
 * Returns wapi_size_t offsets (positions in memory 1), not
 * void* pointers (which would be memory 0 addresses).
 *
 * Two access paths for the allocated regions:
 *   - Multi-memory: import memory 1, load/store directly (zero copy)
 *   - Host-call: wapi_module_shared_read/write (one copy, portable C)
 */

/**
 * Allocate a region in shared memory.
 *
 * @param size   Number of bytes to allocate. Must be > 0.
 * @param align  Required alignment in bytes. Must be a power of 2.
 * @return Offset in shared memory (memory 1), or 0 on failure.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_module, shared_alloc)
wapi_size_t wapi_module_shared_alloc(wapi_size_t size, wapi_size_t align);

/**
 * Free a previously allocated shared memory region.
 * The application can free any region. Children can only free
 * their own allocations. Fails if active borrows exist on
 * the region (reclaim first).
 *
 * @param offset  Offset returned by wapi_module_shared_alloc.
 * @return WAPI_OK on success, WAPI_ERR_ACCES if child doesn't own
 *         the region, WAPI_ERR_BUSY if active borrows exist.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_module, shared_free)
wapi_result_t wapi_module_shared_free(wapi_size_t offset);

/**
 * Resize a shared memory region.
 *
 * @param offset    Offset returned by wapi_module_shared_alloc.
 *                  If 0, behaves like wapi_module_shared_alloc.
 * @param new_size  New size in bytes. If 0, behaves like
 *                  wapi_module_shared_free and returns 0.
 * @param align     Required alignment. Must match original allocation.
 * @return New offset in shared memory, or 0 on failure.
 *         On failure, the original region is unchanged.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_module, shared_realloc)
wapi_size_t wapi_module_shared_realloc(wapi_size_t offset,
                                       wapi_size_t new_size,
                                       wapi_size_t align);

/**
 * Query the usable size of a shared memory allocation.
 *
 * @param offset  Offset returned by wapi_module_shared_alloc.
 * @return Usable size in bytes, or 0 if offset is invalid.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_module, shared_usable_size)
wapi_size_t wapi_module_shared_usable_size(wapi_size_t offset);

/* ============================================================
 * Shared Memory Access (Portable Path)
 * ============================================================
 * For modules that don't use multi-memory. Copies data between
 * private memory (memory 0) and shared memory (memory 1).
 *
 * These are explicit copies — the developer knows data is moving.
 *
 * Multi-memory modules skip these entirely and use direct
 * load/store on memory 1 for zero-copy access.
 *
 * Access rules: the application (shared memory owner) always
 * succeeds. Child modules must have an active borrow covering
 * the accessed region, otherwise WAPI_ERR_ACCES.
 */

/**
 * Copy from shared memory into private memory.
 *
 * @param src_offset  Source offset in shared memory (memory 1).
 * @param dst         Destination pointer in private memory (memory 0).
 * @param len         Number of bytes to copy.
 * @return WAPI_OK on success, WAPI_ERR_RANGE if out of bounds,
 *         WAPI_ERR_ACCES if child has no borrow covering this region.
 *         The application (owner) always succeeds.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_module, shared_read)
wapi_result_t wapi_module_shared_read(wapi_size_t src_offset,
                                      void* dst, wapi_size_t len);

/**
 * Copy from private memory into shared memory.
 *
 * @param dst_offset  Destination offset in shared memory (memory 1).
 * @param src         Source pointer in private memory (memory 0).
 * @param len         Number of bytes to copy.
 * @return WAPI_OK on success, WAPI_ERR_RANGE if out of bounds,
 *         WAPI_ERR_ACCES if child has no borrow covering this region.
 *         The application (owner) always succeeds.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_module, shared_write)
wapi_result_t wapi_module_shared_write(wapi_size_t dst_offset,
                                       const void* src, wapi_size_t len);

/* ============================================================
 * Borrow System (Cross-Module Shared Memory Access)
 * ============================================================
 * Controls which child modules can access which regions of
 * shared memory. The application (owner of shared memory)
 * always has full access and does not need borrows.
 *
 * Borrows are tied to allocations — you can only lend what you
 * allocated via wapi_module_shared_alloc.
 *
 * Borrow rules (reader-writer lock semantics per region):
 *   - WAPI_LEND_READ:  borrower gets read-only access; lender
 *     retains read access. Multiple concurrent READ borrows
 *     to different modules are allowed.
 *   - WAPI_LEND_WRITE: borrower gets exclusive read-write access.
 *     Lender loses access until reclaim. No concurrent borrows
 *     on the same region.
 *
 * This enables concurrent module execution on different regions:
 * module B reads region R1 while module C writes region R3.
 *
 * Child output pattern: the child allocates in shared memory,
 * writes its result, returns the offset as an i32 return value.
 * The application reads it directly — no borrow or transfer
 * needed, because the application owns shared memory.
 *
 * The host enforces borrow rules for child modules. A child
 * accessing a region it hasn't been lent and didn't allocate
 * receives WAPI_ERR_ACCES.
 */

#define WAPI_LEND_READ   0x01  /* Borrower gets read-only access */
#define WAPI_LEND_WRITE  0x02  /* Borrower gets exclusive read-write */

/**
 * Lend a shared memory region to a child module.
 * The region is identified by the offset returned from
 * wapi_module_shared_alloc — the entire allocation is lent.
 *
 * The application can lend any region (it owns all shared memory).
 * A child can only lend regions it allocated itself.
 *
 * @param module  Child module handle.
 * @param offset  Shared memory offset (from wapi_module_shared_alloc).
 * @param flags   WAPI_LEND_READ or WAPI_LEND_WRITE.
 * @param borrow  [out] Borrow handle (used to reclaim).
 * @return WAPI_OK on success,
 *         WAPI_ERR_ACCES if child caller doesn't own the region,
 *         WAPI_ERR_BUSY if a conflicting borrow exists.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_module, lend)
wapi_result_t wapi_module_lend(wapi_handle_t module,
                               wapi_size_t offset,
                               uint32_t flags,
                               wapi_handle_t* borrow);

/**
 * Reclaim a borrow, revoking the child's access to the region.
 *
 * @param borrow  Borrow handle from wapi_module_lend.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_module, reclaim)
wapi_result_t wapi_module_reclaim(wapi_handle_t borrow);

/* ============================================================
 * Explicit Copy (Private-to-Private)
 * ============================================================
 * Copy data from the caller's private memory into a child
 * module's private memory. For cases where shared memory isn't
 * needed (config, seeds, small one-shot inputs).
 *
 * The host allocates space in the child's memory 0, copies the
 * data, and returns the child-side pointer. The child can free
 * this memory through its own wapi_mem_free.
 */

/**
 * Copy data into a child module's private memory.
 *
 * @param module     Child module handle.
 * @param src        Pointer to data in caller's private memory.
 * @param len        Number of bytes to copy.
 * @param child_ptr  [out] Pointer in the child's private memory (memory 0).
 * @return WAPI_OK on success, WAPI_ERR_NOMEM if child can't allocate.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_module, copy_in)
wapi_result_t wapi_module_copy_in(wapi_handle_t module,
                                  const void* src, wapi_size_t len,
                                  uint32_t* child_ptr);

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
                                   wapi_stringview_t url);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_MODULE_H */
