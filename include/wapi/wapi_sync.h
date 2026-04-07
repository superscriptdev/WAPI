/**
 * WAPI - Thread Synchronization Primitives
 * Version 1.0.0
 *
 * Mutexes, read-write locks, semaphores, and condition variables.
 * All primitives are host-managed handles. The host implements
 * them using the platform's native sync (futex, pthread_mutex,
 * SRWLOCK, etc.).
 *
 * Shaped after SDL3 SDL_mutex.h.
 *
 * Maps to: pthread_mutex/cond/rwlock/sem (POSIX),
 *          SRWLOCK/CONDITION_VARIABLE/HANDLE (Windows),
 *          SDL_CreateMutex et al (SDL3)
 *
 * Import module: "wapi_sync"
 *
 * Query availability with wapi_capability_supported("wapi.sync", 9)
 */

#ifndef WAPI_SYNC_H
#define WAPI_SYNC_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

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
WAPI_IMPORT(wapi_sync, mutex_create)
wapi_result_t wapi_sync_mutex_create(wapi_handle_t* mutex);

/**
 * Destroy a mutex. Must not be locked.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_sync, mutex_destroy)
wapi_result_t wapi_sync_mutex_destroy(wapi_handle_t mutex);

/**
 * Lock a mutex. Blocks until the mutex is acquired.
 *
 * @param mutex  Mutex handle.
 * @return WAPI_OK on success, WAPI_ERR_DEADLK if already held
 *         by this thread (non-recursive).
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_sync, mutex_lock)
wapi_result_t wapi_sync_mutex_lock(wapi_handle_t mutex);

/**
 * Try to lock a mutex without blocking.
 *
 * @param mutex  Mutex handle.
 * @return WAPI_OK if acquired, WAPI_ERR_BUSY if already held.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_sync, mutex_try_lock)
wapi_result_t wapi_sync_mutex_try_lock(wapi_handle_t mutex);

/**
 * Unlock a mutex.
 *
 * @param mutex  Mutex handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_sync, mutex_unlock)
wapi_result_t wapi_sync_mutex_unlock(wapi_handle_t mutex);

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
WAPI_IMPORT(wapi_sync, rwlock_create)
wapi_result_t wapi_sync_rwlock_create(wapi_handle_t* rwlock);

/**
 * Destroy a read-write lock. Must not be held.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_sync, rwlock_destroy)
wapi_result_t wapi_sync_rwlock_destroy(wapi_handle_t rwlock);

/**
 * Acquire a read lock. Multiple threads may hold read locks
 * concurrently. Blocks if a writer holds the lock.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_sync, rwlock_read_lock)
wapi_result_t wapi_sync_rwlock_read_lock(wapi_handle_t rwlock);

/**
 * Try to acquire a read lock without blocking.
 *
 * @return WAPI_OK if acquired, WAPI_ERR_BUSY if a writer holds it.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_sync, rwlock_try_read_lock)
wapi_result_t wapi_sync_rwlock_try_read_lock(wapi_handle_t rwlock);

/**
 * Acquire a write lock. Exclusive: blocks until all readers
 * and writers release.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_sync, rwlock_write_lock)
wapi_result_t wapi_sync_rwlock_write_lock(wapi_handle_t rwlock);

/**
 * Try to acquire a write lock without blocking.
 *
 * @return WAPI_OK if acquired, WAPI_ERR_BUSY if held.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_sync, rwlock_try_write_lock)
wapi_result_t wapi_sync_rwlock_try_write_lock(wapi_handle_t rwlock);

/**
 * Release a read or write lock.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_sync, rwlock_unlock)
wapi_result_t wapi_sync_rwlock_unlock(wapi_handle_t rwlock);

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
WAPI_IMPORT(wapi_sync, sem_create)
wapi_result_t wapi_sync_sem_create(uint32_t initial_value,
                                   wapi_handle_t* semaphore);

/**
 * Destroy a semaphore.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_sync, sem_destroy)
wapi_result_t wapi_sync_sem_destroy(wapi_handle_t semaphore);

/**
 * Wait (decrement) on a semaphore. Blocks if count is zero.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_sync, sem_wait)
wapi_result_t wapi_sync_sem_wait(wapi_handle_t semaphore);

/**
 * Try to wait on a semaphore without blocking.
 *
 * @return WAPI_OK if decremented, WAPI_ERR_AGAIN if count was zero.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_sync, sem_try_wait)
wapi_result_t wapi_sync_sem_try_wait(wapi_handle_t semaphore);

/**
 * Wait on a semaphore with a timeout.
 *
 * @param semaphore   Semaphore handle.
 * @param timeout_ns  Timeout in nanoseconds.
 * @return WAPI_OK if decremented, WAPI_ERR_TIMEDOUT on timeout.
 *
 * Wasm signature: (i32, i64) -> i32
 */
WAPI_IMPORT(wapi_sync, sem_wait_timeout)
wapi_result_t wapi_sync_sem_wait_timeout(wapi_handle_t semaphore,
                                         uint64_t timeout_ns);

/**
 * Signal (increment) a semaphore. Wakes one waiting thread.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_sync, sem_signal)
wapi_result_t wapi_sync_sem_signal(wapi_handle_t semaphore);

/**
 * Get the current value of a semaphore.
 *
 * @param semaphore  Semaphore handle.
 * @return Current count (snapshot, may change immediately).
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_sync, sem_get_value)
uint32_t wapi_sync_sem_get_value(wapi_handle_t semaphore);

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
WAPI_IMPORT(wapi_sync, cond_create)
wapi_result_t wapi_sync_cond_create(wapi_handle_t* cond);

/**
 * Destroy a condition variable.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_sync, cond_destroy)
wapi_result_t wapi_sync_cond_destroy(wapi_handle_t cond);

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
WAPI_IMPORT(wapi_sync, cond_wait)
wapi_result_t wapi_sync_cond_wait(wapi_handle_t cond, wapi_handle_t mutex);

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
WAPI_IMPORT(wapi_sync, cond_wait_timeout)
wapi_result_t wapi_sync_cond_wait_timeout(wapi_handle_t cond,
                                          wapi_handle_t mutex,
                                          uint64_t timeout_ns);

/**
 * Signal one thread waiting on a condition variable.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_sync, cond_signal)
wapi_result_t wapi_sync_cond_signal(wapi_handle_t cond);

/**
 * Signal all threads waiting on a condition variable.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_sync, cond_broadcast)
wapi_result_t wapi_sync_cond_broadcast(wapi_handle_t cond);

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
WAPI_IMPORT(wapi_sync, barrier_create)
wapi_result_t wapi_sync_barrier_create(uint32_t count,
                                       wapi_handle_t* barrier);

/**
 * Destroy a barrier. No threads may be waiting on it.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_sync, barrier_destroy)
wapi_result_t wapi_sync_barrier_destroy(wapi_handle_t barrier);

/**
 * Wait at a barrier. Blocks until all threads have arrived.
 *
 * @param barrier  Barrier handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_sync, barrier_wait)
wapi_result_t wapi_sync_barrier_wait(wapi_handle_t barrier);

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
WAPI_IMPORT(wapi_sync, call_once)
wapi_result_t wapi_sync_call_once(uint32_t* once_flag, uint32_t init_func);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_SYNC_H */
