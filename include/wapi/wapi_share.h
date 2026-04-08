/**
 * WAPI - Share Capability
 * Version 1.0.0
 *
 * Maps to: Web Share API, UIActivityViewController (iOS),
 *          Android Intents (ACTION_SEND), NSSharingServicePicker (macOS),
 *          DataTransferManager (Windows)
 *
 * Import module: "wapi_share"
 *
 * Query availability with wapi_capability_supported("wapi.share", 8)
 */

#ifndef WAPI_SHARE_H
#define WAPI_SHARE_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Share data descriptor.
 *
 * Layout (64 bytes, align 8):
 *   Offset  0: wapi_string_view_t title
 *   Offset 16: wapi_string_view_t text
 *   Offset 32: wapi_string_view_t url
 *   Offset 48: wapi_string_view_t file_path  (NULL for no file)
 */
typedef struct wapi_share_data_t {
    wapi_string_view_t title;
    wapi_string_view_t text;
    wapi_string_view_t url;
    wapi_string_view_t file_path;
} wapi_share_data_t;

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
