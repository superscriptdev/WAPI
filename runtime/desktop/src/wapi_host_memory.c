/**
 * WAPI Desktop Runtime - Memory Allocation
 *
 * Implements: wapi_memory.alloc, wapi_memory.free,
 *             wapi_memory.realloc, wapi_memory.usable_size
 *
 * Provides bump allocation within the wasm linear memory.
 * The allocator tracks allocations in g_rt.mem_allocs[] for
 * free/realloc/usable_size support.
 */

#include "wapi_host.h"

/* ============================================================
 * Helpers
 * ============================================================ */

/* Align value up to the given alignment (must be power of 2) */
static inline uint32_t align_up(uint32_t val, uint32_t align) {
    return (val + align - 1) & ~(align - 1);
}

/* WASM page size is 64 KiB */
#define WASM_PAGE_SIZE 65536

/* Initialize the heap top on first use.
 * Sets mem_heap_top to the current end of wasm data, rounded up to 16 bytes. */
static void mem_ensure_init(void) {
    if (g_rt.mem_initialized) return;

    size_t data_size = wapi_wasm_memory_size();
    g_rt.mem_heap_top = align_up((uint32_t)data_size, 16);
    g_rt.mem_alloc_count = 0;
    g_rt.mem_initialized = true;
}

/* Find an allocation entry by wasm offset. Returns index or -1. */
static int mem_find_alloc(uint32_t offset) {
    for (int i = 0; i < g_rt.mem_alloc_count; i++) {
        if (g_rt.mem_allocs[i].offset == offset && g_rt.mem_allocs[i].size > 0) {
            return i;
        }
    }
    return -1;
}

/* Try to reuse a freed slot that fits the requested size and alignment. */
static int mem_find_free_slot(uint32_t size, uint32_t align) {
    for (int i = 0; i < g_rt.mem_alloc_count; i++) {
        if (g_rt.mem_allocs[i].size == 0 && g_rt.mem_allocs[i].offset != 0) {
            /* This is a freed slot -- but we track freed blocks differently.
             * Freed blocks are marked with size=0 and offset preserved.
             * We don't reuse freed blocks in the bump allocator currently;
             * free just marks for potential future compaction. */
            continue;
        }
    }
    (void)size; (void)align;
    return -1;
}

/* ============================================================
 * alloc(size: i32, align: i32) -> i32
 * ============================================================
 * Bump allocator on the wasm heap. Grows memory if needed. */
static wasm_trap_t* cb_alloc(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t size  = WAPI_ARG_I32(0);
    int32_t alignment = WAPI_ARG_I32(1);

    mem_ensure_init();

    /* Validate arguments */
    if (size <= 0 || alignment <= 0) {
        WAPI_RET_I32(0);
        return NULL;
    }

    /* Alignment must be a power of 2 */
    if ((alignment & (alignment - 1)) != 0) {
        WAPI_RET_I32(0);
        return NULL;
    }

    /* Check allocation table capacity */
    if (g_rt.mem_alloc_count >= WAPI_MEM_MAX_ALLOCS) {
        wapi_set_error("Memory allocation table full");
        WAPI_RET_I32(0);
        return NULL;
    }

    /* Align the heap top to the requested alignment */
    uint32_t aligned_top = align_up(g_rt.mem_heap_top, (uint32_t)alignment);
    uint32_t new_top = aligned_top + (uint32_t)size;

    /* Check for overflow */
    if (new_top < aligned_top) {
        wapi_set_error("Memory allocation overflow");
        WAPI_RET_I32(0);
        return NULL;
    }

    /* Grow wasm memory if needed */
    size_t current_size = wapi_wasm_memory_size();
    if (new_top > current_size) {
        uint64_t needed_bytes = (uint64_t)new_top - current_size;
        uint64_t pages_needed = (needed_bytes + WASM_PAGE_SIZE - 1) / WASM_PAGE_SIZE;

        uint64_t prev_pages = 0;
        wasmtime_error_t* err = wasmtime_memory_grow(
            g_rt.context, &g_rt.memory, pages_needed, &prev_pages);
        if (err) {
            wasm_message_t msg;
            wasmtime_error_message(err, &msg);
            fprintf(stderr, "wapi_memory.alloc: grow failed: %.*s\n",
                    (int)msg.size, msg.data);
            wasm_byte_vec_delete(&msg);
            wasmtime_error_delete(err);
            wapi_set_error("Failed to grow wasm memory");
            WAPI_RET_I32(0);
            return NULL;
        }
    }

    /* Record the allocation */
    g_rt.mem_allocs[g_rt.mem_alloc_count].offset = aligned_top;
    g_rt.mem_allocs[g_rt.mem_alloc_count].size = (uint32_t)size;
    g_rt.mem_alloc_count++;

    /* Advance the bump pointer */
    g_rt.mem_heap_top = new_top;

    WAPI_RET_I32((int32_t)aligned_top);
    return NULL;
}

/* ============================================================
 * free(ptr: i32) -> void
 * ============================================================
 * Find allocation by offset, mark as freed for potential reuse. */
static wasm_trap_t* cb_free(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t ptr = WAPI_ARG_I32(0);

    if (ptr == 0) return NULL; /* Freeing null is a no-op */

    mem_ensure_init();

    int idx = mem_find_alloc((uint32_t)ptr);
    if (idx >= 0) {
        /* Mark as free by zeroing the size */
        g_rt.mem_allocs[idx].size = 0;
    }

    return NULL;
}

/* ============================================================
 * realloc(ptr: i32, new_size: i32, align: i32) -> i32
 * ============================================================
 * Allocate new block, copy old data, free old block. */
static wasm_trap_t* cb_realloc(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t ptr      = WAPI_ARG_I32(0);
    int32_t new_size = WAPI_ARG_I32(1);
    int32_t alignment    = WAPI_ARG_I32(2);

    mem_ensure_init();

    /* realloc(NULL, size, align) == alloc(size, align) */
    if (ptr == 0) {
        /* Forward to alloc by calling the allocator logic directly */
        if (new_size <= 0 || alignment <= 0 || (alignment & (alignment - 1)) != 0) {
            WAPI_RET_I32(0);
            return NULL;
        }
        if (g_rt.mem_alloc_count >= WAPI_MEM_MAX_ALLOCS) {
            WAPI_RET_I32(0);
            return NULL;
        }
        uint32_t aligned_top = align_up(g_rt.mem_heap_top, (uint32_t)alignment);
        uint32_t top = aligned_top + (uint32_t)new_size;
        if (top < aligned_top) { WAPI_RET_I32(0); return NULL; }

        size_t current_size = wapi_wasm_memory_size();
        if (top > current_size) {
            uint64_t pages = ((uint64_t)top - current_size + WASM_PAGE_SIZE - 1) / WASM_PAGE_SIZE;
            uint64_t prev = 0;
            wasmtime_error_t* err = wasmtime_memory_grow(g_rt.context, &g_rt.memory, pages, &prev);
            if (err) { wasmtime_error_delete(err); WAPI_RET_I32(0); return NULL; }
        }
        g_rt.mem_allocs[g_rt.mem_alloc_count].offset = aligned_top;
        g_rt.mem_allocs[g_rt.mem_alloc_count].size = (uint32_t)new_size;
        g_rt.mem_alloc_count++;
        g_rt.mem_heap_top = top;
        WAPI_RET_I32((int32_t)aligned_top);
        return NULL;
    }

    /* Find the old allocation */
    int idx = mem_find_alloc((uint32_t)ptr);
    if (idx < 0) {
        wapi_set_error("realloc: unknown pointer");
        WAPI_RET_I32(0);
        return NULL;
    }

    uint32_t old_size = g_rt.mem_allocs[idx].size;

    /* realloc(ptr, 0, align) == free(ptr) */
    if (new_size <= 0) {
        g_rt.mem_allocs[idx].size = 0;
        WAPI_RET_I32(0);
        return NULL;
    }

    /* If the new size fits in the old allocation, just update the size */
    if ((uint32_t)new_size <= old_size) {
        WAPI_RET_I32(ptr);
        return NULL;
    }

    /* Allocate new block */
    if (alignment <= 0 || (alignment & (alignment - 1)) != 0) {
        alignment = 16; /* Default alignment */
    }

    if (g_rt.mem_alloc_count >= WAPI_MEM_MAX_ALLOCS) {
        WAPI_RET_I32(0);
        return NULL;
    }

    uint32_t aligned_top = align_up(g_rt.mem_heap_top, (uint32_t)alignment);
    uint32_t new_top = aligned_top + (uint32_t)new_size;
    if (new_top < aligned_top) { WAPI_RET_I32(0); return NULL; }

    size_t current_mem = wapi_wasm_memory_size();
    if (new_top > current_mem) {
        uint64_t pages = ((uint64_t)new_top - current_mem + WASM_PAGE_SIZE - 1) / WASM_PAGE_SIZE;
        uint64_t prev = 0;
        wasmtime_error_t* err = wasmtime_memory_grow(g_rt.context, &g_rt.memory, pages, &prev);
        if (err) { wasmtime_error_delete(err); WAPI_RET_I32(0); return NULL; }
    }

    /* Copy old data to new location */
    void* old_host = wapi_wasm_ptr((uint32_t)ptr, old_size);
    void* new_host = wapi_wasm_ptr(aligned_top, (uint32_t)new_size);
    if (old_host && new_host) {
        memcpy(new_host, old_host, old_size);
    }

    /* Free old allocation */
    g_rt.mem_allocs[idx].size = 0;

    /* Record new allocation */
    g_rt.mem_allocs[g_rt.mem_alloc_count].offset = aligned_top;
    g_rt.mem_allocs[g_rt.mem_alloc_count].size = (uint32_t)new_size;
    g_rt.mem_alloc_count++;

    g_rt.mem_heap_top = new_top;

    WAPI_RET_I32((int32_t)aligned_top);
    return NULL;
}

/* ============================================================
 * usable_size(ptr: i32) -> i32
 * ============================================================
 * Find allocation by offset, return its size. */
static wasm_trap_t* cb_usable_size(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t ptr = WAPI_ARG_I32(0);

    mem_ensure_init();

    if (ptr == 0) {
        WAPI_RET_I32(0);
        return NULL;
    }

    int idx = mem_find_alloc((uint32_t)ptr);
    if (idx < 0) {
        WAPI_RET_I32(0);
        return NULL;
    }

    WAPI_RET_I32((int32_t)g_rt.mem_allocs[idx].size);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

/* Per wapi.h (§Allocator Vtable / §PART 5 Vtables): the guest obtains
 * its allocator via the `wapi.allocator_get` import returning a
 * wapi_allocator_t vtable (impl + 3 fn pointers) in wasm memory. There
 * is no `wapi_memory` import module. The cb_alloc / cb_free / cb_realloc
 * / cb_usable_size callbacks below become the backing for that vtable
 * once the reactor-shim allocator integration in wapi_host_core.c
 * lands — see NEXT_STEPS.md. */
void wapi_host_register_memory(wasmtime_linker_t* linker) {
    (void)linker;
    (void)cb_alloc; (void)cb_free; (void)cb_realloc; (void)cb_usable_size;
}
