/**
 * WAPI - Process Control
 * Version 1.0.0
 *
 * Spawn, communicate with, and manage OS subprocesses.
 * Provides stdin/stdout/stderr pipes for bidirectional I/O.
 *
 * Shaped after SDL3 SDL_process.h.
 *
 * Maps to: posix_spawn / fork+exec (POSIX),
 *          CreateProcess (Windows), SDL_CreateProcess (SDL3)
 *
 * Import module: "wapi_process"
 *
 * Query availability with wapi_capability_supported("wapi.process", 12)
 *
 * Platform availability:
 *   Desktop (Win/Mac/Linux): Full support
 *   Server/Headless:         Full support
 *   Browser:                 Not available
 *   Phone (iOS/Android):     Not available (sandboxed)
 *   Console:                 Host-dependent
 */

#ifndef WAPI_PROCESS_H
#define WAPI_PROCESS_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Process I/O Mode
 * ============================================================ */

typedef enum wapi_process_io_t {
    WAPI_PROCESS_IO_INHERITED = 0,  /* Inherit parent's stdin/stdout/stderr */
    WAPI_PROCESS_IO_PIPE      = 1,  /* Create a pipe (readable/writable) */
    WAPI_PROCESS_IO_NULL      = 2,  /* /dev/null (discard) */
    WAPI_PROCESS_IO_FORCE32   = 0x7FFFFFFF
} wapi_process_io_t;

/* ============================================================
 * Process Descriptor
 * ============================================================
 *
 * Layout (56 bytes, align 8):
 *   Offset  0: ptr          argv         Pointer to array of wapi_stringview_t
 *   Offset  4: uint32_t     argc         Argument count (argv[0] is the program)
 *   Offset  8: ptr          envp         Pointer to array of wapi_stringview_t
 *                                        ("KEY=VALUE" pairs), NULL to inherit
 *   Offset 12: uint32_t     envc         Environment variable count
 *   Offset 16: wapi_stringview_t cwd    Working directory (UTF-8), NULL = inherit (16 bytes)
 *   Offset 32: uint32_t     stdin_mode   wapi_process_io_t
 *   Offset 36: uint32_t     stdout_mode  wapi_process_io_t
 *   Offset 40: uint32_t     stderr_mode  wapi_process_io_t
 *   Offset 44: uint32_t     _pad
 *   Offset 48: wapi_flags_t flags        (8 bytes)
 */

typedef struct wapi_process_desc_t {
    const wapi_stringview_t* argv;
    uint32_t                  argc;
    const wapi_stringview_t* envp;
    uint32_t                  envc;
    wapi_stringview_t        cwd;
    uint32_t                  stdin_mode;
    uint32_t                  stdout_mode;
    uint32_t                  stderr_mode;
    uint32_t                  _pad;
    wapi_flags_t              flags;
} wapi_process_desc_t;

/* ============================================================
 * Process Functions
 * ============================================================ */

/**
 * Spawn a new subprocess.
 *
 * @param desc     Process descriptor.
 * @param process  [out] Process handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_process, create)
wapi_result_t wapi_process_create(const wapi_process_desc_t* desc,
                                  wapi_handle_t* process);

/**
 * Get the stdin pipe handle for a subprocess.
 * Only valid if stdin_mode was WAPI_PROCESS_IO_PIPE.
 * Write to this handle to send data to the child's stdin.
 *
 * @param process  Process handle.
 * @param pipe     [out] Writable pipe handle.
 * @return WAPI_OK on success, WAPI_ERR_INVAL if not piped.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_process, get_stdin)
wapi_result_t wapi_process_get_stdin(wapi_handle_t process,
                                     wapi_handle_t* pipe);

/**
 * Get the stdout pipe handle for a subprocess.
 * Only valid if stdout_mode was WAPI_PROCESS_IO_PIPE.
 * Read from this handle to get the child's stdout output.
 *
 * @param process  Process handle.
 * @param pipe     [out] Readable pipe handle.
 * @return WAPI_OK on success, WAPI_ERR_INVAL if not piped.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_process, get_stdout)
wapi_result_t wapi_process_get_stdout(wapi_handle_t process,
                                      wapi_handle_t* pipe);

/**
 * Get the stderr pipe handle for a subprocess.
 * Only valid if stderr_mode was WAPI_PROCESS_IO_PIPE.
 *
 * @param process  Process handle.
 * @param pipe     [out] Readable pipe handle.
 * @return WAPI_OK on success, WAPI_ERR_INVAL if not piped.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi_process, get_stderr)
wapi_result_t wapi_process_get_stderr(wapi_handle_t process,
                                      wapi_handle_t* pipe);

/**
 * Write data to a subprocess pipe (stdin).
 *
 * @param pipe     Pipe handle from wapi_process_get_stdin.
 * @param buf      Data to write.
 * @param len      Data length in bytes.
 * @param written  [out] Bytes actually written.
 * @return WAPI_OK on success, WAPI_ERR_PIPE if pipe is broken.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_process, pipe_write)
wapi_result_t wapi_process_pipe_write(wapi_handle_t pipe, const void* buf,
                                      wapi_size_t len, wapi_size_t* written);

/**
 * Read data from a subprocess pipe (stdout/stderr).
 *
 * @param pipe       Pipe handle from wapi_process_get_stdout/stderr.
 * @param buf        Buffer to receive data.
 * @param len        Buffer capacity in bytes.
 * @param bytes_read [out] Bytes actually read. 0 = EOF.
 * @return WAPI_OK on success, WAPI_ERR_PIPE if pipe is broken.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_process, pipe_read)
wapi_result_t wapi_process_pipe_read(wapi_handle_t pipe, void* buf,
                                     wapi_size_t len, wapi_size_t* bytes_read);

/**
 * Close a pipe handle. Signals EOF to the subprocess on stdin.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_process, pipe_close)
wapi_result_t wapi_process_pipe_close(wapi_handle_t pipe);

/**
 * Check if a subprocess has exited, or wait for it.
 *
 * @param process    Process handle.
 * @param block      WAPI_TRUE to block until exit,
 *                   WAPI_FALSE to poll (non-blocking).
 * @param exit_code  [out] Exit code if the process has exited.
 * @return WAPI_OK if the process has exited (exit_code is valid),
 *         WAPI_ERR_AGAIN if still running (non-blocking mode).
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_process, wait)
wapi_result_t wapi_process_wait(wapi_handle_t process, wapi_bool_t block,
                                int32_t* exit_code);

/**
 * Forcefully terminate a subprocess.
 *
 * @param process  Process handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_process, kill)
wapi_result_t wapi_process_kill(wapi_handle_t process);

/**
 * Release a process handle and all associated resources.
 * If the process is still running, it continues in the background
 * (detached). Closes any open pipes.
 *
 * @param process  Process handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_process, destroy)
wapi_result_t wapi_process_destroy(wapi_handle_t process);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_PROCESS_H */
