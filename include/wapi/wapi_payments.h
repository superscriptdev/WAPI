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

#include "wapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Payment item (line item in the order).
 *
 * Layout (20 bytes, align 4):
 *   Offset  0: ptr      label
 *   Offset  4: uint32_t label_len
 *   Offset  8: ptr      amount         e.g., "9.99"
 *   Offset 12: uint32_t amount_len
 *   Offset 16: ptr      currency       ISO 4217 (e.g., "USD")
 *   Offset 20: uint32_t currency_len
 */
typedef struct wapi_pay_item_t {
    const char* label;
    wapi_size_t   label_len;
    const char* amount;
    wapi_size_t   amount_len;
    const char* currency;
    wapi_size_t   currency_len;
} wapi_pay_item_t;

/**
 * Payment request descriptor.
 *
 * Layout (20 bytes, align 4):
 *   Offset  0: ptr      merchant_id
 *   Offset  4: uint32_t merchant_id_len
 *   Offset  8: ptr      items          Array of wapi_pay_item_t
 *   Offset 12: uint32_t item_count
 *   Offset 16: uint32_t flags
 */
typedef struct wapi_pay_request_t {
    const char*           merchant_id;
    wapi_size_t             merchant_id_len;
    const wapi_pay_item_t*  items;
    uint32_t              item_count;
    uint32_t              flags;
} wapi_pay_request_t;

#define WAPI_PAY_FLAG_REQUEST_SHIPPING  0x0001
#define WAPI_PAY_FLAG_REQUEST_EMAIL     0x0002
#define WAPI_PAY_FLAG_REQUEST_PHONE     0x0004

/**
 * Check if payment is available.
 */
WAPI_IMPORT(wapi_pay, can_make_payment)
wapi_bool_t wapi_pay_can_make_payment(void);

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
