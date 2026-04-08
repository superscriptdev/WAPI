/**
 * WAPI - Biometric Authentication Capability
 * Version 1.0.0
 *
 * Maps to: WebAuthn/FIDO2, Face ID/Touch ID (iOS),
 *          BiometricPrompt (Android), Windows Hello
 *
 * Import module: "wapi_bio"
 *
 * Query availability with wapi_capability_supported("wapi.biometric", 12)
 */

#ifndef WAPI_BIOMETRIC_H
#define WAPI_BIOMETRIC_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum wapi_bio_type_t {
    WAPI_BIO_NONE         = 0,
    WAPI_BIO_FINGERPRINT  = 0x01,
    WAPI_BIO_FACE         = 0x02,
    WAPI_BIO_IRIS         = 0x04,
    WAPI_BIO_ANY          = 0xFF,
    WAPI_BIO_FORCE32      = 0x7FFFFFFF
} wapi_bio_type_t;

/**
 * Query available biometric types.
 * @return Bitmask of wapi_bio_type_t values.
 */
WAPI_IMPORT(wapi_bio, available_types)
uint32_t wapi_bio_available_types(void);

/**
 * Authenticate the user with biometrics.
 *
 * @see WAPI_IO_OP_BIO_AUTHENTICATE
 *
 * @param type       Accepted biometric types (bitmask).
 * @param reason     User-visible reason string (UTF-8).
 * @return WAPI_OK if authenticated, WAPI_ERR_ACCES if denied/failed,
 *         WAPI_ERR_CANCELED if user canceled.
 */
WAPI_IMPORT(wapi_bio, authenticate)
wapi_result_t wapi_bio_authenticate(uint32_t type, wapi_string_view_t reason);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_BIOMETRIC_H */
