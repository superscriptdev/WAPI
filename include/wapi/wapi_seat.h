/**
 * WAPI - Seat
 * Version 1.0.0
 *
 * Import module: "wapi_seat"
 */

#ifndef WAPI_SEAT_H
#define WAPI_SEAT_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef wapi_handle_t wapi_seat_t;

#define WAPI_SEAT_DEFAULT  ((wapi_seat_t)0)

/* Wasm signature: () -> i64 */
WAPI_IMPORT(wapi_seat, count)
wapi_size_t wapi_seat_count(void);

/* Wasm signature: (i64) -> i32 */
WAPI_IMPORT(wapi_seat, at)
wapi_seat_t wapi_seat_at(wapi_size_t index);

/* Wasm signature: (i32, i32, i64, i32) -> i32 */
WAPI_IMPORT(wapi_seat, name)
wapi_result_t wapi_seat_name(wapi_seat_t seat, char* buf,
                             wapi_size_t buf_len, wapi_size_t* out_len);

/* Stable opaque id of the account occupying `seat`; matches
 * wapi_user_get_field(WAPI_USER_FIELD_ID, ...) for WAPI_SEAT_DEFAULT.
 * WAPI_ERR_NOENT if unoccupied or no per-seat attribution.
 * Wasm signature: (i32, i32, i64, i32) -> i32 */
WAPI_IMPORT(wapi_seat, user_id)
wapi_result_t wapi_seat_user_id(wapi_seat_t seat, char* buf,
                                wapi_size_t buf_len, wapi_size_t* out_len);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_SEAT_H */
