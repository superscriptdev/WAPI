/**
 * WAPI - Context, Allocator, and I/O Vtable
 * Version 1.0.0
 *
 * Foundational types that connect modules to the host. Every module
 * receives a wapi_context_t from the host (or its parent module) at
 * startup. The context carries an allocator and an I/O vtable --
 * the module's complete interface to the outside world.
 *
 * Both allocator and I/O are function tables with opaque context
 * pointers. This lets parent modules wrap them -- an app can give
 * a sub-module a throttled I/O or an arena allocator, and the
 * sub-module never knows.
 *
 * These types have no import namespace. They are plain structs
 * passed by pointer, not host imports.
 */

#ifndef WAPI_CONTEXT_H
#define WAPI_CONTEXT_H

#include "wapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations -- full definitions in wapi_io.h and wapi_event.h */
struct wapi_io_op_t;
union wapi_event_t;

/* ============================================================
 * Allocator
 * ============================================================
 * Function table with opaque context pointer.
 *
 * The `impl` pointer carries per-instance state (arena position,
 * pool free list, etc.) -- without it, you can't have two different
 * allocator instances alive simultaneously.
 *
 * Layout (16 bytes, align 4):
 *   Offset  0: ptr  impl        Opaque implementation context
 *   Offset  4: ptr  alloc_fn    (impl, size, align) -> ptr
 *   Offset  8: ptr  free_fn     (impl, ptr) -> void
 *   Offset 12: ptr  realloc_fn  (impl, ptr, new_size, align) -> ptr
 */
typedef struct wapi_allocator_t {
    void* impl;
    void* (*alloc_fn)(void* impl, wapi_size_t size, wapi_size_t align);
    void  (*free_fn)(void* impl, void* ptr);
    void* (*realloc_fn)(void* impl, void* ptr, wapi_size_t new_size, wapi_size_t align);
} wapi_allocator_t;

/* ============================================================
 * I/O Vtable
 * ============================================================
 * The module's complete interface to the outside world.
 * Submit operations, cancel them, and poll/wait for events.
 * All events (input, I/O completions, lifecycle) come through
 * poll/wait. There is no separate event system.
 *
 * The host provides a default wapi_io_t whose submit/cancel call
 * host imports. Parent modules can wrap it to control how children
 * perform I/O (logging, throttling, sandboxing, mocking).
 *
 * Layout (24 bytes, align 4):
 *   Offset  0: ptr  impl       Opaque implementation context
 *   Offset  4: ptr  submit     (impl, ops, count) -> i32
 *   Offset  8: ptr  cancel     (impl, user_data) -> i32
 *   Offset 12: ptr  poll       (impl, event) -> i32
 *   Offset 16: ptr  wait       (impl, event, timeout_ms) -> i32
 *   Offset 20: ptr  flush      (impl, event_type) -> void
 */
typedef struct wapi_io_t {
    void*         impl;
    int32_t       (*submit)(void* impl, const struct wapi_io_op_t* ops,
                            wapi_size_t count);
    wapi_result_t (*cancel)(void* impl, uint64_t user_data);
    int32_t       (*poll)(void* impl, union wapi_event_t* event);
    int32_t       (*wait)(void* impl, union wapi_event_t* event,
                          int32_t timeout_ms);
    void          (*flush)(void* impl, uint32_t event_type);
} wapi_io_t;

/* ============================================================
 * Panic Handler
 * ============================================================
 * Called by a module before it traps (unrecoverable error).
 * The handler records the message and returns -- it is NOT
 * noreturn. The module then hits __builtin_trap() which becomes
 * wasm `unreachable`. The runtime catches the trap and has the
 * recorded message available.
 *
 * Whoever provides the context provides the panic handler.
 * A parent module controls how child panics are reported --
 * same principle as wrapping the allocator or I/O vtable.
 *
 * NULL panic pointer in context = runtime default (print + trap).
 *
 * Layout (8 bytes, align 4):
 *   Offset  0: ptr  impl   Opaque implementation context
 *   Offset  4: ptr  fn     (impl, msg, msg_len) -> void
 */
typedef struct wapi_panic_handler_t {
    void* impl;
    void  (*fn)(void* impl, const char* msg, wapi_size_t msg_len);
} wapi_panic_handler_t;

/* ============================================================
 * Context Flags
 * ============================================================ */

#define WAPI_CTX_FLAG_DEBUG       0x0001  /* Debug mode: verbose errors, assertions */
#define WAPI_CTX_FLAG_SHARED_MEM  0x0002  /* Module shares linear memory with parent */

/* ============================================================
 * Context
 * ============================================================
 * Universal execution context passed from host to modules.
 *
 * Contains the execution substrates a module needs to run:
 * memory (allocator), world interaction (I/O), panic reporting,
 * and compute/render (GPU device). Everything else is either
 * controllable through vtable wrappers or application-level
 * (surfaces, locale, preferences are function parameters).
 *
 * The host provides this to wapi_main at startup. Modules pass it
 * (or a wrapped version) to sub-modules via wapi_module_init.
 * This is the same struct at both boundaries:
 *   Host -> wapi_main(ctx)
 *   App  -> wapi_module_init(mod, ctx)
 *
 * Layout (20 bytes on wasm32, align 4):
 *   Offset  0: ptr      allocator    Pointer to wapi_allocator_t
 *   Offset  4: ptr      io           Pointer to wapi_io_t
 *   Offset  8: ptr      panic        Pointer to wapi_panic_handler_t (NULL = default)
 *   Offset 12: int32_t  gpu_device   GPU device handle (0 if no GPU)
 *   Offset 16: uint32_t flags        WAPI_CTX_FLAG_* execution flags
 */
typedef struct wapi_context_t {
    const wapi_allocator_t*      allocator;
    const wapi_io_t*             io;
    const wapi_panic_handler_t*  panic;
    wapi_handle_t                gpu_device;
    uint32_t                     flags;
} wapi_context_t;

#ifdef __wasm__
_Static_assert(sizeof(wapi_context_t) == 20,
               "wapi_context_t must be 20 bytes on wasm32");
#endif

/* ============================================================
 * Panic Helper
 * ============================================================ */

/**
 * Report a panic message and trap. Does not return.
 * If ctx->panic is set, the handler is called first to record
 * the message. Then the module traps unconditionally.
 */
static inline _Noreturn void wapi_panic(const wapi_context_t* ctx,
                                        const char* msg, wapi_size_t msg_len) {
    if (ctx->panic) {
        ctx->panic->fn(ctx->panic->impl, msg, msg_len);
    }
    __builtin_trap();
}

#ifdef __cplusplus
}
#endif

#endif /* WAPI_CONTEXT_H */
