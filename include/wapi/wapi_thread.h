/**
 * WAPI - Thread Management
 * Version 1.0.0
 *
 * Create, join, detach, and manage threads. The host creates OS
 * threads; the module provides the entry function. Each new thread
 * gets its own stack and calls into the module's exported thread
 * entry point.
 *
 * Shaped after SDL3 SDL_thread.h.
 *
 * Maps to: pthread_create (POSIX), CreateThread (Windows),
 *          SDL_CreateThread (SDL3), std::thread (C++)
 *
 * Import module: "wapi_thread"
 *
 * Query availability with wapi_capability_supported("wapi.thread", 11)
 */

#ifndef WAPI_THREAD_H
#define WAPI_THREAD_H

#include "wapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Thread Priority
 * ============================================================ */

typedef enum wapi_thread_priority_t {
    WAPI_THREAD_PRIORITY_LOW        = 0,
    WAPI_THREAD_PRIORITY_NORMAL     = 1,
    WAPI_THREAD_PRIORITY_HIGH       = 2,
    WAPI_THREAD_PRIORITY_TIME_CRITICAL = 3,
    WAPI_THREAD_PRIORITY_FORCE32    = 0x7FFFFFFF
} wapi_thread_priority_t;

/* ============================================================
 * Thread State
 * ============================================================ */

typedef enum wapi_thread_state_t {
    WAPI_THREAD_STATE_ALIVE     = 0,  /* Thread is running */
    WAPI_THREAD_STATE_DETACHED  = 1,  /* Thread is detached */
    WAPI_THREAD_STATE_COMPLETE  = 2,  /* Thread has finished */
    WAPI_THREAD_STATE_FORCE32   = 0x7FFFFFFF
} wapi_thread_state_t;

/* ============================================================
 * Thread Descriptor
 * ============================================================
 * Describes a thread to create. The entry function is identified
 * by a function table index (Wasm indirect call) or function
 * pointer (native).
 *
 * Layout (24 bytes, align 4):
 *   Offset  0: uint32_t entry_func   Function pointer / table index
 *   Offset  4: uint32_t user_data    Opaque pointer passed to entry
 *   Offset  8: ptr      name         Thread name (UTF-8, for debugging)
 *   Offset 12: uint32_t name_len
 *   Offset 16: uint32_t stack_size   Requested stack size (0 = default)
 *   Offset 20: uint32_t priority     wapi_thread_priority_t
 */

typedef struct wapi_thread_desc_t {
    uint32_t    entry_func;
    uint32_t    user_data;
    const char* name;
    wapi_size_t name_len;
    wapi_size_t stack_size;
    uint32_t    priority;
} wapi_thread_desc_t;

/* ============================================================
 * Thread Functions
 * ============================================================ */

/**
 * Create and start a new thread.
 *
 * The entry function signature (exported by the module) is:
 *   int32_t thread_entry(uint32_t user_data);
 *
 * @param desc    Thread descriptor.
 * @param thread  [out] Thread handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_thread, create)
wapi_result_t wapi_thread_create(const wapi_thread_desc_t* desc,
                                 wapi_handle_t* thread);

/**
 * Wait for a thread to finish and retrieve its return value.
 * This blocks the calling thread until the target thread exits.
 *
 * @param thread      Thread handle.
 * @param exit_code   [out] The value returned by the thread entry
 *                    function. May be NULL if not needed.
 * @return WAPI_OK on success, WAPI_ERR_INVAL if already detached.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_thread, join)
wapi_result_t wapi_thread_join(wapi_handle_t thread, int32_t* exit_code);

/**
 * Detach a thread so it cleans up automatically on completion.
 * After detaching, the thread handle becomes invalid and
 * wapi_thread_join() cannot be called.
 *
 * @param thread  Thread handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, detach)
wapi_result_t wapi_thread_detach(wapi_handle_t thread);

/**
 * Get the current state of a thread.
 *
 * @param thread  Thread handle.
 * @param state   [out] Current thread state.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_thread, get_state)
wapi_result_t wapi_thread_get_state(wapi_handle_t thread,
                                    wapi_thread_state_t* state);

/**
 * Get the unique ID of the calling thread.
 * Thread IDs are host-assigned and opaque.
 *
 * @return Current thread's ID.
 *
 * Wasm signature: () -> i64
 */
WAPI_IMPORT(wapi_thread, current_id)
uint64_t wapi_thread_current_id(void);

/**
 * Get the unique ID of a specific thread.
 *
 * @param thread  Thread handle.
 * @return Thread's ID, or 0 if handle is invalid.
 *
 * Wasm signature: (i32) -> i64
 */
WAPI_IMPORT(wapi_thread, get_id)
uint64_t wapi_thread_get_id(wapi_handle_t thread);

/**
 * Set the priority of the calling thread.
 *
 * @param priority  Desired priority level.
 * @return WAPI_OK on success, WAPI_ERR_ACCES if not permitted.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, set_priority)
wapi_result_t wapi_thread_set_priority(wapi_thread_priority_t priority);

/* ============================================================
 * Thread-Local Storage (TLS)
 * ============================================================
 * Per-thread key/value storage. The host manages TLS slots.
 * An optional destructor is called when a thread exits.
 *
 * The destructor is identified by function table index (Wasm)
 * or function pointer (native).
 */

/**
 * Create a TLS slot.
 *
 * @param destructor  Function index called with the slot's value
 *                    when a thread exits. 0 = no destructor.
 *                    Signature: void destructor(uint32_t value);
 * @param slot        [out] TLS slot handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_thread, tls_create)
wapi_result_t wapi_thread_tls_create(uint32_t destructor,
                                     wapi_handle_t* slot);

/**
 * Destroy a TLS slot. Does NOT call destructors on existing values.
 *
 * @param slot  TLS slot handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, tls_destroy)
wapi_result_t wapi_thread_tls_destroy(wapi_handle_t slot);

/**
 * Set the current thread's value for a TLS slot.
 *
 * @param slot   TLS slot handle.
 * @param value  Value to store (typically a pointer as i32).
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_thread, tls_set)
wapi_result_t wapi_thread_tls_set(wapi_handle_t slot, uint32_t value);

/**
 * Get the current thread's value for a TLS slot.
 *
 * @param slot  TLS slot handle.
 * @return The stored value, or 0 if not set.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, tls_get)
uint32_t wapi_thread_tls_get(wapi_handle_t slot);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_THREAD_H */
