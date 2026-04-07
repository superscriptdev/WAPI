/**
 * WAPI - Share Capability
 * Version 1.0.0
 *
 * Maps to: Web Share API, UIActivityViewController (iOS),
 *          Android Intents (ACTION_SEND)
 *
 * Import module: "wapi_share"
 *
 * Query availability with wapi_capability_supported("wapi.share", 8)
 */

#ifndef WAPI_SHARE_H
#define WAPI_SHARE_H

#include "wapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Share data descriptor.
 *
 * Layout (32 bytes, align 4):
 *   Offset  0: ptr      title
 *   Offset  4: uint32_t title_len
 *   Offset  8: ptr      text
 *   Offset 12: uint32_t text_len
 *   Offset 16: ptr      url
 *   Offset 20: uint32_t url_len
 *   Offset 24: ptr      file_path      (NULL for no file)
 *   Offset 28: uint32_t file_path_len
 */
typedef struct wapi_share_data_t {
    const char* title;
    wapi_size_t   title_len;
    const char* text;
    wapi_size_t   text_len;
    const char* url;
    wapi_size_t   url_len;
    const char* file_path;
    wapi_size_t   file_path_len;
} wapi_share_data_t;

/**
 * Check if sharing is supported.
 */
WAPI_IMPORT(wapi_share, can_share)
wapi_bool_t wapi_share_can_share(void);

/**
 * Invoke the system share sheet.
 *
 * @param data  Content to share.
 * @return WAPI_OK if shared, WAPI_ERR_CANCELED if dismissed.
 */
WAPI_IMPORT(wapi_share, share)
wapi_result_t wapi_share_share(const wapi_share_data_t* data);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_SHARE_H */
