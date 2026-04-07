/**
 * WAPI Desktop Runtime - Content Tree Declaration
 *
 * Implements all wapi_content.* imports.
 *
 * The content tree is an app-owned buffer in Wasm memory.
 * The host registers a pointer to it and reads it for
 * accessibility, keyboard navigation, and indexing.
 *
 * TODO: implement a11y tree mapping (UI Automation, AT-SPI2, NSAccessibility)
 * TODO: implement keyboard focus management and Tab navigation
 */

#include "wapi_host.h"

/* ============================================================
 * State: registered content tree
 * ============================================================ */

static uint32_t g_content_tree_ptr = 0;  /* Wasm memory offset */
static uint32_t g_content_capacity = 0;

/* ============================================================
 * Callback: register_tree
 * (i32 tree_ptr, i32 capacity) -> i32
 * ============================================================ */
static wasm_trap_t* cb_register_tree(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;

    g_content_tree_ptr = (uint32_t)WAPI_ARG_I32(0);
    g_content_capacity = (uint32_t)WAPI_ARG_I32(1);

    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Callback: notify
 * () -> i32
 * ============================================================ */
static wasm_trap_t* cb_notify(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    (void)args;

    /* TODO: trigger immediate a11y tree sync from g_content_tree_ptr */
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_content(wasmtime_linker_t* linker) {
    /* register_tree: (i32, i32) -> i32 */
    WAPI_DEFINE_2_1(linker, "wapi_content", "register_tree", cb_register_tree);

    /* notify: () -> i32 */
    WAPI_DEFINE_0_1(linker, "wapi_content", "notify", cb_notify);
}
