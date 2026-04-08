/**
 * WAPI - Payments Capability
 * Version 1.0.0
 *
 * Maps to: Payment Request API (Web), Apple Pay,
 *          Google Pay / Android Pay
 *
 * Import module: "wapi_pay"
 *
 * Query availability with wapi_capability_supported("wapi.payments", 11)
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
 *   Offset  0: wapi_string_view_t label
 *   Offset 16: wapi_string_view_t amount    e.g., "9.99"
 *   Offset 32: wapi_string_view_t currency  ISO 4217 (e.g., "USD")
 */
typedef struct wapi_pay_item_t {
    wapi_string_view_t label;
    wapi_string_view_t amount;
    wapi_string_view_t currency;
} wapi_pay_item_t;

/**
 * Payment request descriptor.
 *
 * Layout (32 bytes, align 8):
 *   Offset  0: wapi_string_view_t merchant_id
 *   Offset 16: ptr          items          Array of wapi_pay_item_t
 *   Offset 20: uint32_t     item_count
 *   Offset 24: wapi_flags_t flags
 */
typedef struct wapi_pay_request_t {
    wapi_string_view_t      merchant_id;
    const wapi_pay_item_t*  items;
    uint32_t              item_count;
    wapi_flags_t          flags;
} wapi_pay_request_t;

#define WAPI_PAY_FLAG_REQUEST_SHIPPING  0x0001
#define WAPI_PAY_FLAG_REQUEST_EMAIL     0x0002
#define WAPI_PAY_FLAG_REQUEST_PHONE     0x0004

/**
 * Show the payment sheet and process payment.
 *
 * @param request   Payment request descriptor.
 * @param token     [out] Buffer for payment token/nonce.
 * @param token_len [in] Buffer capacity, [out] actual token length.
 * @return WAPI_OK on success, WAPI_ERR_CANCELED if user canceled.
 */
WAPI_IMPORT(wapi_pay, request_payment)
wapi_result_t wapi_pay_request_payment(const wapi_pay_request_t* request,
                                    void* token, wapi_size_t* token_len);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_PAYMENTS_H */
