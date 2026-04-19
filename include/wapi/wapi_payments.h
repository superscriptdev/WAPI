/**
 * WAPI - Payments
 * Version 1.0.0
 *
 * Maps to: Payment Request API (Web), Apple Pay,
 *          Google Pay / Android Pay
 *
 * Import module: "wapi_pay"
 */

#ifndef WAPI_PAYMENTS_H
#define WAPI_PAYMENTS_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Payment item (line item in the order).
 *
 * Layout (48 bytes, align 8):
 *   Offset  0: wapi_stringview_t label
 *   Offset 16: wapi_stringview_t amount    e.g., "9.99"
 *   Offset 32: wapi_stringview_t currency  ISO 4217 (e.g., "USD")
 */
typedef struct wapi_pay_item_t {
    wapi_stringview_t label;
    wapi_stringview_t amount;
    wapi_stringview_t currency;
} wapi_pay_item_t;

/**
 * Payment request descriptor.
 *
 * Layout (32 bytes, align 8):
 *   Offset  0: wapi_stringview_t merchant_id
 *   Offset 16: ptr          items          Array of wapi_pay_item_t
 *   Offset 20: uint32_t     item_count
 *   Offset 24: wapi_flags_t flags
 */
typedef struct wapi_pay_request_t {
    wapi_stringview_t      merchant_id;
    const wapi_pay_item_t*  items;
    uint32_t              item_count;
    wapi_flags_t          flags;
} wapi_pay_request_t;

#define WAPI_PAY_FLAG_REQUEST_SHIPPING  0x0001
#define WAPI_PAY_FLAG_REQUEST_EMAIL     0x0002
#define WAPI_PAY_FLAG_REQUEST_PHONE     0x0004

/* ============================================================
 * Payment Operations (async, submitted via wapi_io_t)
 * ============================================================ */

/**
 * Submit a payment request. Shows the system payment sheet. On
 * completion the payment token/nonce is written to `token` and the
 * actual length is returned in the completion event's
 * `result` field.
 */
static inline wapi_result_t wapi_pay_request_payment(
    const wapi_io_t* io,
    const wapi_pay_request_t* request,
    void* token, wapi_size_t token_capacity,
    uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode     = WAPI_IO_OP_PAY_PAYMENT_REQUEST;
    op.addr       = (uint64_t)(uintptr_t)request;
    op.len        = sizeof(*request);
    op.addr2      = (uint64_t)(uintptr_t)token;
    op.len2       = token_capacity;
    op.user_data  = user_data;
    return io->submit(io->impl, &op, 1);
}

#ifdef __cplusplus
}
#endif

#endif /* WAPI_PAYMENTS_H */
