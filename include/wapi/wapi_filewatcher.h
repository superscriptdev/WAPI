/**
 * WAPI - File Watcher Capability
 * Version 1.0.0
 *
 * Directory and file change monitoring.
 *
 * Maps to: ReadDirectoryChangesW (Windows), FSEvents (macOS),
 *          inotify (Linux), FileSystemObserver (Web)
 *
 * Import module: "wapi_fwatch"
 *
 * Query availability with wapi_capability_supported("wapi.fwatch", 10)
 */

#ifndef WAPI_FILEWATCHER_H
#define WAPI_FILEWATCHER_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * File Watcher Types
 * ============================================================ */

typedef enum wapi_fwatch_change_t {
    WAPI_FWATCH_CREATED  = 0,
    WAPI_FWATCH_MODIFIED = 1,
    WAPI_FWATCH_DELETED  = 2,
    WAPI_FWATCH_RENAMED  = 3,
    WAPI_FWATCH_FORCE32  = 0x7FFFFFFF
} wapi_fwatch_change_t;

/* ============================================================
 * File Watcher Functions
 * ============================================================ */

/**
 * Start watching a file or directory for changes.
 *
 * When a change is detected, a file-watcher event is emitted.
 *
 * @param path_ptr    Path string to watch.
 * @param recursive   Non-zero to watch subdirectories recursively.
 * @param out_handle  [out] Receives the watcher handle.
 * @return WAPI_OK on success, WAPI_ERR_NOENT if path does not exist,
 *         WAPI_ERR_ACCES if not permitted.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_fwatch, fwatch_add)
wapi_result_t wapi_fwatch_add(wapi_stringview_t path_ptr,
                              wapi_bool_t recursive,
                              wapi_handle_t* out_handle);

/**
 * Stop watching a previously registered path.
 *
 * @param handle  Watcher handle returned by fwatch_add.
 * @return WAPI_OK on success, WAPI_ERR_BADF if invalid handle.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_fwatch, fwatch_remove)
wapi_result_t wapi_fwatch_remove(wapi_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_FILEWATCHER_H */
