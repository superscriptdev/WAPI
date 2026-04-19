/**
 * WAPI - Transfer
 * Version 1.0.0
 *
 * Import module: "wapi_transfer"
 */

#ifndef WAPI_TRANSFER_H
#define WAPI_TRANSFER_H

#include "wapi.h"
#include "wapi_seat.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Item — shared across all transfer modes
 * ============================================================ */

/* MIME-tagged byte payload.
 *
 * Layout (32 bytes, align 8):
 *   Offset  0: wapi_stringview_t mime    (16 bytes)
 *   Offset 16: uint64_t          data    (linear memory address)
 *   Offset 24: wapi_size_t       data_len
 */
typedef struct wapi_transfer_item_t {
    wapi_stringview_t mime;
    uint64_t          data;
    wapi_size_t       data_len;
} wapi_transfer_item_t;

_Static_assert(sizeof(wapi_transfer_item_t) == 32,
               "wapi_transfer_item_t must be 32 bytes");
_Static_assert(_Alignof(wapi_transfer_item_t) == 8,
               "wapi_transfer_item_t must be 8-byte aligned");

/* ============================================================
 * Modes (bitmask) and actions
 * ============================================================ */

#define WAPI_TRANSFER_LATENT   0x01u   /* clipboard-style */
#define WAPI_TRANSFER_POINTED  0x02u   /* drag-style */
#define WAPI_TRANSFER_ROUTED   0x04u   /* share-style */

typedef enum wapi_transfer_action_t {
    WAPI_TRANSFER_ACTION_NONE    = 0,
    WAPI_TRANSFER_ACTION_COPY    = 1,
    WAPI_TRANSFER_ACTION_MOVE    = 2,
    WAPI_TRANSFER_ACTION_LINK    = 3,
    WAPI_TRANSFER_ACTION_FORCE32 = 0x7FFFFFFF
} wapi_transfer_action_t;

/* ============================================================
 * Offer descriptor
 *
 * Layout (48 bytes, align 8):
 *   Offset  0: uint64_t          items           (linear memory address of wapi_transfer_item_t[])
 *   Offset  8: uint32_t          item_count
 *   Offset 12: uint32_t          allowed_actions (bitmask of wapi_transfer_action_t)
 *   Offset 16: wapi_stringview_t title           (16 bytes)
 *   Offset 32: wapi_handle_t     preview         (surface handle, or WAPI_HANDLE_INVALID)
 *   Offset 36: uint32_t          _reserved
 *   Offset 40: uint64_t          _reserved2
 * ============================================================ */
typedef struct wapi_transfer_offer_t {
    uint64_t          items;
    uint32_t          item_count;
    uint32_t          allowed_actions;
    wapi_stringview_t title;
    wapi_handle_t     preview;
    uint32_t          _reserved;
    uint64_t          _reserved2;
} wapi_transfer_offer_t;

_Static_assert(sizeof(wapi_transfer_offer_t) == 48,
               "wapi_transfer_offer_t must be 48 bytes");
_Static_assert(_Alignof(wapi_transfer_offer_t) == 8,
               "wapi_transfer_offer_t must be 8-byte aligned");

/* ============================================================
 * Source side — make an offer (seat-scoped)
 * ============================================================ */

/* Submit an offer in one or more delivery modes on a given seat.
 * Completion result encodes (mode_consumed << 8) | action_taken,
 * or WAPI_ERR_CANCELED on dismiss. */
static inline wapi_result_t wapi_transfer_offer(
    const wapi_io_t* io, wapi_seat_t seat,
    const wapi_transfer_offer_t* offer,
    uint32_t mode, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_TRANSFER_OFFER;
    op.fd        = (int32_t)seat;
    op.flags     = mode;
    op.addr      = (uint64_t)(uintptr_t)offer;
    op.len       = sizeof(*offer);
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/* Wasm signature: (i32, i32) -> i32 */
WAPI_IMPORT(wapi_transfer, revoke)
wapi_result_t wapi_transfer_revoke(wapi_seat_t seat, uint32_t mode);

/* ============================================================
 * Target side — read what's currently on offer for this seat
 * ============================================================ */

/* Wasm signature: (i32, i32) -> i64 */
WAPI_IMPORT(wapi_transfer, format_count)
wapi_size_t wapi_transfer_format_count(wapi_seat_t seat, uint32_t mode);

/* Wasm signature: (i32, i32, i64, i32, i64, i32) -> i32 */
WAPI_IMPORT(wapi_transfer, format_name)
wapi_result_t wapi_transfer_format_name(wapi_seat_t seat, uint32_t mode,
                                        wapi_size_t index,
                                        char* buf, wapi_size_t buf_len,
                                        wapi_size_t* out_len);

/* Wasm signature: (i32, i32, i32, i64) -> i32 */
WAPI_IMPORT(wapi_transfer, has_format)
wapi_bool_t wapi_transfer_has_format(wapi_seat_t seat, uint32_t mode,
                                     const char* mime_data,
                                     wapi_size_t mime_len);

/* Async read. Gesture-gated for LATENT; immediate for POINTED.
 * Completion result = bytes written. */
static inline wapi_result_t wapi_transfer_read(
    const wapi_io_t* io, wapi_seat_t seat, uint32_t mode,
    wapi_stringview_t mime,
    void* buf, wapi_size_t buf_len, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_TRANSFER_READ;
    op.fd        = (int32_t)seat;
    op.flags     = mode;
    op.addr      = mime.data;
    op.len       = mime.length;
    op.addr2     = (uint64_t)(uintptr_t)buf;
    op.len2      = buf_len;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/* ============================================================
 * POINTED-only: in-flight drag UX
 *
 * Event constants WAPI_EVENT_TRANSFER_* and the wapi_transfer_event_t
 * struct are declared in wapi.h alongside other event types.
 * ============================================================ */

/* Wasm signature: (i32, i32) -> i32 */
WAPI_IMPORT(wapi_transfer, set_action)
wapi_result_t wapi_transfer_set_action(wapi_seat_t seat,
                                       wapi_transfer_action_t action);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_TRANSFER_H */
