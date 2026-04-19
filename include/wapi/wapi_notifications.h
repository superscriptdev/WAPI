/**
 * WAPI - Notifications
 * Version 1.0.0
 *
 * Maps to: Web Notifications API, native notification centers
 *          (NSUserNotificationCenter, Android NotificationManager)
 *
 * Import module: "wapi_notify"
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
 *   Offset  0: wapi_stringview_t title
 *   Offset 16: wapi_stringview_t body
 *   Offset 32: wapi_stringview_t icon_url  (NULL for no icon)
 *   Offset 48: uint32_t urgency
 *   Offset 52: uint32_t _reserved
 */
typedef struct wapi_notify_desc_t {
    wapi_stringview_t title;
    wapi_stringview_t body;
    wapi_stringview_t icon_url;
    uint32_t      urgency;
    uint32_t      _reserved;
} wapi_notify_desc_t;

/**
 * Close / dismiss a previously shown notification. Bounded-local
 * (operates on an already-minted handle); direct sync import.
 */
WAPI_IMPORT(wapi_notify, close)
wapi_result_t wapi_notify_close(wapi_handle_t id);

/* ============================================================
 * Notification Operations (async, submitted via wapi_io_t)
 * ============================================================ */

/**
 * Submit a notification show. *out_id receives the notification
 * handle on completion (usable with wapi_notify_close).
 */
static inline wapi_result_t wapi_notify_show(
    const wapi_io_t* io,
    const wapi_notify_desc_t* desc,
    wapi_handle_t* out_id,
    uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode     = WAPI_IO_OP_NOTIFY_SHOW;
    op.addr       = (uint64_t)(uintptr_t)desc;
    op.len        = sizeof(*desc);
    op.result_ptr = (uint64_t)(uintptr_t)out_id;
    op.user_data  = user_data;
    return io->submit(io->impl, &op, 1);
}

#ifdef __cplusplus
}
#endif

#endif /* WAPI_NOTIFICATIONS_H */
