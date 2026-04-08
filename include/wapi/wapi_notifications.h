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

#include "wapi.h"

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
 * Layout (56 bytes, align 8):
 *   Offset  0: wapi_string_view_t title
 *   Offset 16: wapi_string_view_t body
 *   Offset 32: wapi_string_view_t icon_url  (NULL for no icon)
 *   Offset 48: uint32_t urgency
 *   Offset 52: uint32_t _reserved
 */
typedef struct wapi_notify_desc_t {
    wapi_string_view_t title;
    wapi_string_view_t body;
    wapi_string_view_t icon_url;
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
