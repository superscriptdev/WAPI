/**
 * WAPI Desktop Runtime - Clipboard
 *
 * Implements all wapi_clipboard.* imports using SDL3 clipboard functions.
 * Currently supports TEXT format; HTML and IMAGE return appropriate errors.
 *
 * Import module: "wapi_clipboard"
 */

#include "wapi_host.h"

/* ============================================================
 * clipboard.has_format
 * ============================================================
 * Wasm signature: (i32 format) -> i32
 * Returns 1 if clipboard contains data in the given format, 0 otherwise.
 */
static wasm_trap_t* host_clipboard_has_format(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t format = WAPI_ARG_I32(0);

    switch (format) {
    case 0: /* WAPI_CLIPBOARD_TEXT */
        WAPI_RET_I32(SDL_HasClipboardText() ? 1 : 0);
        break;
    case 1: /* WAPI_CLIPBOARD_HTML */
    case 2: /* WAPI_CLIPBOARD_IMAGE */
        /* Not yet implemented via SDL3 */
        WAPI_RET_I32(0);
        break;
    default:
        WAPI_RET_I32(0);
        break;
    }

    return NULL;
}

/* ============================================================
 * clipboard.read
 * ============================================================
 * Wasm signature: (i32 format, i32 buf, i32 buf_len, i32 bytes_written_ptr) -> i32
 */
static wasm_trap_t* host_clipboard_read(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t  format            = WAPI_ARG_I32(0);
    uint32_t buf_ptr           = WAPI_ARG_U32(1);
    uint32_t buf_len           = WAPI_ARG_U32(2);
    uint32_t bytes_written_ptr = WAPI_ARG_U32(3);

    if (format != 0 /* WAPI_CLIPBOARD_TEXT */) {
        wapi_set_error("clipboard_read: only TEXT format is supported");
        WAPI_RET_I32(WAPI_ERR_NOTSUP);
        return NULL;
    }

    if (!SDL_HasClipboardText()) {
        wapi_wasm_write_u32(bytes_written_ptr, 0);
        WAPI_RET_I32(WAPI_ERR_NOENT);
        return NULL;
    }

    char* text = SDL_GetClipboardText();
    if (!text) {
        wapi_wasm_write_u32(bytes_written_ptr, 0);
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }

    size_t text_len = strlen(text);

    /* Determine how many bytes to copy */
    uint32_t copy_len = (uint32_t)text_len;
    if (copy_len > buf_len) {
        copy_len = buf_len;
    }

    if (copy_len > 0) {
        void* buf = wapi_wasm_ptr(buf_ptr, copy_len);
        if (!buf) {
            SDL_free(text);
            wapi_set_error("clipboard_read: invalid buffer pointer");
            WAPI_RET_I32(WAPI_ERR_INVAL);
            return NULL;
        }
        memcpy(buf, text, copy_len);
    }

    SDL_free(text);

    wapi_wasm_write_u32(bytes_written_ptr, copy_len);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * clipboard.write
 * ============================================================
 * Wasm signature: (i32 format, i32 data, i32 len) -> i32
 */
static wasm_trap_t* host_clipboard_write(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t  format   = WAPI_ARG_I32(0);
    uint32_t data_ptr = WAPI_ARG_U32(1);
    uint32_t len      = WAPI_ARG_U32(2);

    if (format != 0 /* WAPI_CLIPBOARD_TEXT */) {
        wapi_set_error("clipboard_write: only TEXT format is supported");
        WAPI_RET_I32(WAPI_ERR_NOTSUP);
        return NULL;
    }

    const char* data = wapi_wasm_read_string(data_ptr, len);
    if (!data && len > 0) {
        wapi_set_error("clipboard_write: invalid data pointer");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    /* SDL_SetClipboardText requires a null-terminated string */
    char* tmp = (char*)malloc(len + 1);
    if (!tmp) {
        wapi_set_error("clipboard_write: out of memory");
        WAPI_RET_I32(WAPI_ERR_NOMEM);
        return NULL;
    }
    if (len > 0) {
        memcpy(tmp, data, len);
    }
    tmp[len] = '\0';

    bool ok = SDL_SetClipboardText(tmp);
    free(tmp);

    if (!ok) {
        wapi_set_error("clipboard_write: SDL_SetClipboardText failed");
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }

    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * clipboard.clear
 * ============================================================
 * Wasm signature: () -> i32
 */
static wasm_trap_t* host_clipboard_clear(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;

    bool ok = SDL_SetClipboardText("");
    if (!ok) {
        wapi_set_error("clipboard_clear: SDL_SetClipboardText failed");
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }

    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_clipboard(wasmtime_linker_t* linker) {
    WAPI_DEFINE_1_1(linker, "wapi_clipboard", "has_format", host_clipboard_has_format);
    WAPI_DEFINE_4_1(linker, "wapi_clipboard", "read",       host_clipboard_read);
    WAPI_DEFINE_3_1(linker, "wapi_clipboard", "write",      host_clipboard_write);
    WAPI_DEFINE_0_1(linker, "wapi_clipboard", "clear",      host_clipboard_clear);
}
