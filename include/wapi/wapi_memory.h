/**
 * WAPI - Memory Allocation
 * Version 1.0.0
 *
 * Host-provided memory allocation, explicit at the boundary.
 * The allocator is provided by the environment, not assumed.
 *
 * In Wasm, this operates on the module's linear memory. The host
 * manages the allocation bookkeeping; the module gets pointers.
 *
 * Import module: "wapi_memory"
 */

#ifndef WAPI_MEMORY_H
#define WAPI_MEMORY_H

#include "wapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Memory Allocation
 * ============================================================
 * The host manages allocation within the module's linear memory.
 * The module exports its memory, and the host grows it as needed.
 *
 * These functions are the boundary-level allocation API. Modules
 * are free to sub-allocate internally (arena, pool, bump) using
 * memory obtained from these functions.
 */

/**
 * Allocate a block of memory.
 *
 * @param size   Number of bytes to allocate. Must be > 0.
 * @param align  Required alignment in bytes. Must be a power of 2.
 *               Common values: 1, 4, 8, 16.
 * @return Pointer to allocated memory, or NULL on failure.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_memory, alloc)
void* wapi_mem_alloc(wapi_size_t size, wapi_size_t align);

/**
 * Free a previously allocated block.
 *
 * @param ptr  Pointer returned by wapi_mem_alloc or wapi_mem_realloc.
 *             NULL is a valid no-op.
 *
 * Wasm signature: (i32) -> void
 */
WAPI_IMPORT(wapi_memory, free)
void wapi_mem_free(void* ptr);

/**
 * Resize a previously allocated block.
 *
 * @param ptr       Pointer returned by wapi_mem_alloc or wapi_mem_realloc.
 *                  If NULL, behaves like wapi_mem_alloc.
 * @param new_size  New size in bytes. If 0, behaves like wapi_mem_free
 *                  and returns NULL.
 * @param align     Required alignment. Must match the original allocation.
 * @return Pointer to resized memory, or NULL on failure.
 *         On failure, the original block is unchanged.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_memory, realloc)
void* wapi_mem_realloc(void* ptr, wapi_size_t new_size, wapi_size_t align);

/**
 * Query the usable size of an allocated block.
 * The actual allocated size may be larger than requested.
 *
 * @param ptr  Pointer returned by wapi_mem_alloc or wapi_mem_realloc.
 * @return Usable size in bytes, or 0 if ptr is NULL/invalid.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_memory, usable_size)
wapi_size_t wapi_mem_usable_size(void* ptr);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_MEMORY_H */
