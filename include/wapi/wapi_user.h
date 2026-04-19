/**
 * WAPI - User
 * Version 1.0.0
 *
 * Import module: "wapi_user"
 */

#ifndef WAPI_USER_H
#define WAPI_USER_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum wapi_user_provider_t {
    WAPI_USER_PROVIDER_UNKNOWN   = 0,
    WAPI_USER_PROVIDER_LOCAL     = 1,
    WAPI_USER_PROVIDER_MICROSOFT = 2,
    WAPI_USER_PROVIDER_APPLE     = 3,
    WAPI_USER_PROVIDER_GOOGLE    = 4,
    WAPI_USER_PROVIDER_DOMAIN    = 5,
    WAPI_USER_PROVIDER_FORCE32   = 0x7FFFFFFF
} wapi_user_provider_t;

typedef enum wapi_user_field_t {
    WAPI_USER_FIELD_LOGIN   = 0,
    WAPI_USER_FIELD_DISPLAY = 1,
    WAPI_USER_FIELD_EMAIL   = 2,
    WAPI_USER_FIELD_UPN     = 3,
    WAPI_USER_FIELD_ID      = 4,
    WAPI_USER_FIELD_FORCE32 = 0x7FFFFFFF
} wapi_user_field_t;

/* Wasm signature: () -> i32 */
WAPI_IMPORT(wapi_user, provider)
wapi_user_provider_t wapi_user_provider(void);

/* Wasm signature: (i32, i32, i64, i32) -> i32 */
WAPI_IMPORT(wapi_user, get_field)
wapi_result_t wapi_user_get_field(uint32_t field,
                                  char* buf, wapi_size_t buf_len,
                                  wapi_size_t* out_len);

/* Wasm signature: (i32, i32, i32, i32, i64) -> i32 */
WAPI_IMPORT(wapi_user, avatar)
wapi_result_t wapi_user_avatar(uint32_t max_edge,
                               uint32_t* out_width, uint32_t* out_height,
                               void* buf, wapi_size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_USER_H */
