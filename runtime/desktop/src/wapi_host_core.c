/**
 * WAPI Desktop Runtime - Core ("wapi" module) host imports
 *
 * Three imports live under the "wapi" module name per wapi.h:
 *   wapi.panic_report(msg_ptr, msg_len) -> void
 *   wapi.exit()                         -> i32 (never returns; traps)
 *   wapi.allocator_get()                -> i32 (guest ptr to wapi_allocator_t)
 *
 * allocator_get: the host returns the guest-address of a
 * pre-constructed wapi_allocator_t struct whose function
 * pointers reference the module's own malloc/free/realloc
 * export entries.  We can't synthesize wasm function pointers
 * from the host side without cooperating with a shim on the
 * guest; for this phase, we return 0 (NULL) and the reactor
 * shim is expected to provide its own allocator.  See
 * NEXT_STEPS.md for the full story.
 */

#include "wapi_host.h"

static wasm_trap_t* host_panic_report(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)results; (void)nresults;
    uint32_t ptr = WAPI_ARG_U32(0);
    uint64_t len = WAPI_ARG_U64(1);
    const char* msg = (const char*)wapi_wasm_ptr(ptr, (uint32_t)len);
    if (msg) fprintf(stderr, "[WAPI PANIC] %.*s\n", (int)len, msg);
    return NULL;
}

static wasm_trap_t* host_exit(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    g_rt.running = false;
    WAPI_RET_I32(WAPI_OK);
    /* Returning a trap signals the frame loop to unwind cleanly. */
    return wasmtime_trap_new("wapi_env_exit", 13);
}

static wasm_trap_t* host_allocator_get(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    /* Phase 2: reactor shim provides the allocator. Host-synthesized
     * allocator requires writing a wapi_allocator_t into guest memory
     * whose function pointers are wasm funcref indices into the guest
     * table — nontrivial plumbing, see NEXT_STEPS.md. */
    WAPI_RET_I32(0);
    return NULL;
}

void wapi_host_register_core(wasmtime_linker_t* linker) {
    /* panic_report: (i32 ptr, i64 len) -> void */
    wapi_linker_define(linker, "wapi", "panic_report", host_panic_report,
        2, (wasm_valkind_t[]){WASM_I32, WASM_I64}, 0, NULL);

    /* exit: () -> i32 */
    WAPI_DEFINE_0_1(linker, "wapi", "exit", host_exit);

    /* allocator_get: () -> i32 */
    WAPI_DEFINE_0_1(linker, "wapi", "allocator_get", host_allocator_get);
}
