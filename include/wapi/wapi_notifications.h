/**
 * WAPI - Notifications Capability
 * Version 1.0.0
 *
 * Maps to: Web Notifications API, native notification centers
 *          (NSUserNotificationCenter, Android NotificationManager)
 *
 * Import module: "wapi_notify"
 *
 * Query availability with wapi_capability_supported("wapi.notifications", 16)
 */

#ifndef WAPI_NOTIFICATIONS_H
#define WAPI_NOTIFICATIONS_H

#include "wapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Notification urgency */
typedef enum wapi_notify_urgency_t {
    WAPI_NOTIFY_LOW     = 0,
    WAPI_NOTIFY_NORMAL  = 1,
    WAPI_NOTIFY_HIGH    = 2,
    WAPI_NOTIFY_FORCE32 = 0x7FFFFFFF
} wapi_notify_urgency_t;

/**
 * Notification descriptor.
 *
 * Layout (32 bytes, align 4):
 *   Offset  0: ptr      title
 *   Offset  4: uint32_t title_len
 *   Offset  8: ptr      body
 *   Offset 12: uint32_t body_len
 *   Offset 16: ptr      icon_url       (NULL for no icon)
 *   Offset 20: uint32_t icon_url_len
 *   Offset 24: uint32_t urgency
 *   Offset 28: uint32_t _reserved
 */
typedef struct wapi_notify_desc_t {
    const char*   title;
    wapi_size_t     title_len;
    const char*   body;
    wapi_size_t     body_len;
    const char*   icon_url;
    wapi_size_t     icon_url_len;
    uint32_t      urgency;
    uint32_t      _reserved;
} wapi_notify_desc_t;

/**
 * Request notification permission.
 * @return WAPI_OK if granted, WAPI_ERR_ACCES if denied.
 */
WAPI_IMPORT(wapi_notify, request_permission)
wapi_result_t wapi_notify_request_permission(void);

/**
 * Check if notifications are permitted.
 */
WAPI_IMPORT(wapi_notify, is_permitted)
wapi_bool_t wapi_notify_is_permitted(void);

/**
 * Show a notification.
 * @param desc    Notification content.
 * @param id      [out] Notification handle (for closing).
 * @return WAPI_OK on success.
 */
WAPI_IMPORT(wapi_notify, show)
wapi_result_t wapi_notify_show(const wapi_notify_desc_t* desc, wapi_handle_t* id);

/**
 * Close / dismiss a notification.
 */
WAPI_IMPORT(wapi_notify, close)
wapi_result_t wapi_notify_close(wapi_handle_t id);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_NOTIFICATIONS_H */
