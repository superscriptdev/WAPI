/**
 * WAPI - Threads and Synchronization
 * Version 1.0.0
 *
 * Thread creation/lifetime, thread-local storage, and the full
 * family of synchronization primitives (mutex, rwlock, semaphore,
 * condition variable, barrier, call-once). The host creates OS
 * threads and provides native sync primitives (futex, pthread,
 * SRWLOCK, etc.); the module supplies entry points and handles.
 *
 * Shaped after SDL3 SDL_thread.h + SDL_mutex.h.
 *
 * Maps to: pthread_create/mutex/cond/rwlock/sem (POSIX),
 *          CreateThread/SRWLOCK/CONDITION_VARIABLE (Windows),
 *          SDL_CreateThread / SDL_CreateMutex et al (SDL3)
 *
 * Import module: "wapi_thread"
 *
 * Query availability with wapi_capability_supported("wapi.thread", 11)
 */

#ifndef WAPI_THREAD_H
#define WAPI_THREAD_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Thread QoS Class
 * ============================================================
 * Scheduler hint, not a core-placement directive. On heterogeneous
 * CPUs (Apple Silicon, Alder Lake+, Android big.LITTLE) the host
 * scheduler uses this to decide whether to place the thread on
 * performance cores or efficiency cores, and whether to throttle
 * it under power-saver mode. The app never pins to a specific
 * core type -- the scheduler owns placement based on live thermals,
 * load, and power state.
 *
 * Mapping:
 *   macOS/iOS: pthread_set_qos_class_self_np for BACKGROUND..INTERACTIVE,
 *              thread_policy_set(THREAD_TIME_CONSTRAINT_POLICY) for TIMECRITICAL
 *   Windows:   SetThreadInformation(ThreadPowerThrottling, EcoQoS) for
 *              BACKGROUND/UTILITY; SetThreadPriority for higher tiers;
 *              THREAD_PRIORITY_TIME_CRITICAL for TIMECRITICAL
 *   Android/Linux: nice + cgroup scheduling class; RT priority for TIMECRITICAL
 *   Web:       no-op (no QoS knob exposed to wasm)
 */

typedef enum wapi_thread_qos_t {
    WAPI_THREAD_QOS_BACKGROUND   = 0,  /* Maintenance, prefetch -- E-core-friendly */
    WAPI_THREAD_QOS_UTILITY      = 1,  /* Long-running user-visible work */
    WAPI_THREAD_QOS_INITIATED    = 2,  /* User-initiated, short-lived */
    WAPI_THREAD_QOS_INTERACTIVE  = 3,  /* UI, input, render driver */
    WAPI_THREAD_QOS_TIMECRITICAL = 4,  /* Audio callback, realtime deadlines */
    WAPI_THREAD_QOS_FORCE32      = 0x7FFFFFFF
} wapi_thread_qos_t;

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
 * Layout (32 bytes, align 8):
 *   Offset  0: uint32_t entry_func   Function pointer / table index
 *   Offset  4: uint32_t user_data    Opaque pointer passed to entry
 *   Offset  8: wapi_stringview_t name  Thread name (UTF-8, for debugging)
 *   Offset 24: uint32_t stack_size   Requested stack size (0 = default)
 *   Offset 28: uint32_t qos          wapi_thread_qos_t
 */

typedef struct wapi_thread_desc_t {
    uint32_t    entry_func;
    uint32_t    user_data;
    wapi_stringview_t name;
    wapi_size_t stack_size;
    uint32_t    qos;
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
 * Set the QoS class of the calling thread.
 *
 * @param qos  Desired QoS class.
 * @return WAPI_OK on success, WAPI_ERR_ACCES if not permitted.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, set_qos)
wapi_result_t wapi_thread_set_qos(wapi_thread_qos_t qos);

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

/* ============================================================
 * Mutex
 * ============================================================
 * Non-recursive mutual exclusion lock.
 */

/**
 * Create a mutex.
 *
 * @param mutex  [out] Mutex handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, mutex_create)
wapi_result_t wapi_thread_mutex_create(wapi_handle_t* mutex);

/**
 * Destroy a mutex. Must not be locked.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, mutex_destroy)
wapi_result_t wapi_thread_mutex_destroy(wapi_handle_t mutex);

/**
 * Lock a mutex. Blocks until the mutex is acquired.
 *
 * @param mutex  Mutex handle.
 * @return WAPI_OK on success, WAPI_ERR_DEADLK if already held
 *         by this thread (non-recursive).
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, mutex_lock)
wapi_result_t wapi_thread_mutex_lock(wapi_handle_t mutex);

/**
 * Try to lock a mutex without blocking.
 *
 * @param mutex  Mutex handle.
 * @return WAPI_OK if acquired, WAPI_ERR_BUSY if already held.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, mutex_try_lock)
wapi_result_t wapi_thread_mutex_try_lock(wapi_handle_t mutex);

/**
 * Unlock a mutex.
 *
 * @param mutex  Mutex handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, mutex_unlock)
wapi_result_t wapi_thread_mutex_unlock(wapi_handle_t mutex);

/* ============================================================
 * Read-Write Lock
 * ============================================================
 * Multiple concurrent readers OR one exclusive writer.
 */

/**
 * Create a read-write lock.
 *
 * @param rwlock  [out] RWLock handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, rwlock_create)
wapi_result_t wapi_thread_rwlock_create(wapi_handle_t* rwlock);

/**
 * Destroy a read-write lock. Must not be held.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, rwlock_destroy)
wapi_result_t wapi_thread_rwlock_destroy(wapi_handle_t rwlock);

/**
 * Acquire a read lock. Multiple threads may hold read locks
 * concurrently. Blocks if a writer holds the lock.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, rwlock_read_lock)
wapi_result_t wapi_thread_rwlock_read_lock(wapi_handle_t rwlock);

/**
 * Try to acquire a read lock without blocking.
 *
 * @return WAPI_OK if acquired, WAPI_ERR_BUSY if a writer holds it.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, rwlock_try_read_lock)
wapi_result_t wapi_thread_rwlock_try_read_lock(wapi_handle_t rwlock);

/**
 * Acquire a write lock. Exclusive: blocks until all readers
 * and writers release.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, rwlock_write_lock)
wapi_result_t wapi_thread_rwlock_write_lock(wapi_handle_t rwlock);

/**
 * Try to acquire a write lock without blocking.
 *
 * @return WAPI_OK if acquired, WAPI_ERR_BUSY if held.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, rwlock_try_write_lock)
wapi_result_t wapi_thread_rwlock_try_write_lock(wapi_handle_t rwlock);

/**
 * Release a read or write lock.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, rwlock_unlock)
wapi_result_t wapi_thread_rwlock_unlock(wapi_handle_t rwlock);

/* ============================================================
 * Semaphore
 * ============================================================
 * Counting semaphore. Useful for producer-consumer patterns
 * and resource pooling.
 */

/**
 * Create a counting semaphore.
 *
 * @param initial_value  Starting count.
 * @param semaphore      [out] Semaphore handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_thread, sem_create)
wapi_result_t wapi_thread_sem_create(uint32_t initial_value,
                                     wapi_handle_t* semaphore);

/**
 * Destroy a semaphore.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, sem_destroy)
wapi_result_t wapi_thread_sem_destroy(wapi_handle_t semaphore);

/**
 * Wait (decrement) on a semaphore. Blocks if count is zero.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, sem_wait)
wapi_result_t wapi_thread_sem_wait(wapi_handle_t semaphore);

/**
 * Try to wait on a semaphore without blocking.
 *
 * @return WAPI_OK if decremented, WAPI_ERR_AGAIN if count was zero.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, sem_try_wait)
wapi_result_t wapi_thread_sem_try_wait(wapi_handle_t semaphore);

/**
 * Wait on a semaphore with a timeout.
 *
 * @param semaphore   Semaphore handle.
 * @param timeout_ns  Timeout in nanoseconds.
 * @return WAPI_OK if decremented, WAPI_ERR_TIMEDOUT on timeout.
 *
 * Wasm signature: (i32, i64) -> i32
 */
WAPI_IMPORT(wapi_thread, sem_wait_timeout)
wapi_result_t wapi_thread_sem_wait_timeout(wapi_handle_t semaphore,
                                           uint64_t timeout_ns);

/**
 * Signal (increment) a semaphore. Wakes one waiting thread.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, sem_signal)
wapi_result_t wapi_thread_sem_signal(wapi_handle_t semaphore);

/**
 * Get the current value of a semaphore.
 *
 * @param semaphore  Semaphore handle.
 * @return Current count (snapshot, may change immediately).
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, sem_get_value)
uint32_t wapi_thread_sem_get_value(wapi_handle_t semaphore);

/* ============================================================
 * Condition Variable
 * ============================================================
 * Used with a mutex to wait for a condition to become true.
 */

/**
 * Create a condition variable.
 *
 * @param cond  [out] Condition variable handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, cond_create)
wapi_result_t wapi_thread_cond_create(wapi_handle_t* cond);

/**
 * Destroy a condition variable.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, cond_destroy)
wapi_result_t wapi_thread_cond_destroy(wapi_handle_t cond);

/**
 * Wait on a condition variable. The mutex must be locked by the
 * calling thread. It is atomically released and re-acquired
 * when the condition is signaled.
 *
 * @param cond   Condition variable handle.
 * @param mutex  Mutex handle (must be locked).
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_thread, cond_wait)
wapi_result_t wapi_thread_cond_wait(wapi_handle_t cond, wapi_handle_t mutex);

/**
 * Wait on a condition variable with a timeout.
 *
 * @param cond       Condition variable handle.
 * @param mutex      Mutex handle (must be locked).
 * @param timeout_ns Timeout in nanoseconds.
 * @return WAPI_OK if signaled, WAPI_ERR_TIMEDOUT on timeout.
 *
 * Wasm signature: (i32, i32, i64) -> i32
 */
WAPI_IMPORT(wapi_thread, cond_wait_timeout)
wapi_result_t wapi_thread_cond_wait_timeout(wapi_handle_t cond,
                                            wapi_handle_t mutex,
                                            uint64_t timeout_ns);

/**
 * Signal one thread waiting on a condition variable.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, cond_signal)
wapi_result_t wapi_thread_cond_signal(wapi_handle_t cond);

/**
 * Signal all threads waiting on a condition variable.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, cond_broadcast)
wapi_result_t wapi_thread_cond_broadcast(wapi_handle_t cond);

/* ============================================================
 * Barrier
 * ============================================================
 * Synchronization point where N threads must all arrive
 * before any of them proceed.
 */

/**
 * Create a barrier.
 *
 * @param count    Number of threads that must reach the barrier.
 * @param barrier  [out] Barrier handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_thread, barrier_create)
wapi_result_t wapi_thread_barrier_create(uint32_t count,
                                         wapi_handle_t* barrier);

/**
 * Destroy a barrier. No threads may be waiting on it.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, barrier_destroy)
wapi_result_t wapi_thread_barrier_destroy(wapi_handle_t barrier);

/**
 * Wait at a barrier. Blocks until all threads have arrived.
 *
 * @param barrier  Barrier handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_thread, barrier_wait)
wapi_result_t wapi_thread_barrier_wait(wapi_handle_t barrier);

/* ============================================================
 * Once Initialization
 * ============================================================
 * Ensure a function runs exactly once across all threads.
 * The init function is identified by function table index (Wasm)
 * or function pointer (native).
 */

/**
 * Run an initialization function exactly once.
 * All callers block until the init function completes.
 *
 * @param once_flag  Pointer to a uint32_t in linear memory,
 *                   initialized to 0. Set to 1 after init completes.
 * @param init_func  Function index to call once.
 *                   Signature: void init_func(void);
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_thread, call_once)
wapi_result_t wapi_thread_call_once(uint32_t* once_flag, uint32_t init_func);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_THREAD_H */
