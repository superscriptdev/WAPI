/**
 * WAPI - Environment and Process
 * Version 1.0.0
 *
 * Command-line arguments, environment variables, random bytes,
 * and process exit. Modeled on WASI Preview 1.
 *
 * Import module: "wapi_env"
 */

#ifndef WAPI_ENV_H
#define WAPI_ENV_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Command-Line Arguments
 * ============================================================ */

/**
 * Get the number of command-line arguments.
 *
 * Wasm signature: () -> i32
 */
WAPI_IMPORT(wapi_env, args_count)
int32_t wapi_env_args_count(void);

/**
 * Get a command-line argument by index.
 *
 * @param index    Argument index (0-based, where 0 is the program name).
 * @param buf      Buffer to receive the argument (UTF-8).
 * @param buf_len  Buffer capacity.
 * @param arg_len  [out] Actual argument length.
 * @return WAPI_OK on success, WAPI_ERR_RANGE if index out of bounds.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_env, args_get)
wapi_result_t wapi_env_args_get(int32_t index, char* buf, wapi_size_t buf_len,
                             wapi_size_t* arg_len);

/* ============================================================
 * Environment Variables
 * ============================================================ */

/**
 * Get the number of environment variables.
 *
 * Wasm signature: () -> i32
 */
WAPI_IMPORT(wapi_env, environ_count)
int32_t wapi_env_environ_count(void);

/**
 * Get an environment variable by index.
 * The format is "KEY=VALUE" (UTF-8).
 *
 * @param index    Variable index (0-based).
 * @param buf      Buffer to receive "KEY=VALUE".
 * @param buf_len  Buffer capacity.
 * @param var_len  [out] Actual length.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_env, environ_get)
wapi_result_t wapi_env_environ_get(int32_t index, char* buf, wapi_size_t buf_len,
                                wapi_size_t* var_len);

/**
 * Look up an environment variable by name.
 *
 * @param name      Variable name (UTF-8).
 * @param buf       Buffer to receive the value.
 * @param buf_len   Buffer capacity.
 * @param val_len   [out] Actual value length.
 * @return WAPI_OK on success, WAPI_ERR_NOENT if not found.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_env, getenv)
wapi_result_t wapi_env_getenv(wapi_string_view_t name,
                           char* buf, wapi_size_t buf_len, wapi_size_t* val_len);

/* ============================================================
 * Cryptographic Random
 * ============================================================ */

/**
 * Fill a buffer with cryptographically secure random bytes.
 *
 * @param buf  Buffer to fill.
 * @param len  Number of random bytes to generate.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_env, random_get)
wapi_result_t wapi_env_random_get(void* buf, wapi_size_t len);

/* ============================================================
 * Process Control
 * ============================================================ */

/**
 * Exit the process with a status code.
 * This function does not return.
 *
 * @param code  Exit code (0 = success).
 *
 * Wasm signature: (i32) -> noreturn
 */
WAPI_IMPORT(wapi_env, exit)
_Noreturn void wapi_env_exit(int32_t code);

/* ============================================================
 * URL Launch
 * ============================================================ */

/**
 * Open a URL in the system's default handler (browser, app, etc.).
 *
 * Maps to: SDL_OpenURL (SDL3), window.open (Web),
 *          ShellExecute (Windows), NSWorkspace.open (macOS),
 *          startActivity (Android)
 *
 * @param url      URL to open (UTF-8). e.g. "https://example.com"
 *                 or "mailto:user@example.com".
 * @return WAPI_OK on success, WAPI_ERR_NOTSUP if not available.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_env, open_url)
wapi_result_t wapi_env_open_url(wapi_string_view_t url);

/* ============================================================
 * Error Messages
 * ============================================================ */

/**
 * Get a human-readable description of the last error.
 * The message is valid until the next WAPI API call on this thread.
 *
 * @return String view of the error message, or empty if no error.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_env, get_error)
wapi_result_t wapi_env_get_error(char* buf, wapi_size_t buf_len, wapi_size_t* msg_len);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_ENV_H */
