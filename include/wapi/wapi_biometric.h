/**
 * WAPI - Biometric Authentication
 * Version 1.0.0
 *
 * Maps to: WebAuthn/FIDO2, Face ID/Touch ID (iOS),
 *          BiometricPrompt (Android), Windows Hello
 *
 * Import module: "wapi_bio"
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
 * Bounded local query — kept as a direct sync import.
 * @return Bitmask of wapi_bio_type_t values.
 */
WAPI_IMPORT(wapi_bio, available_types)
uint32_t wapi_bio_available_types(void);

/* ============================================================
 * Biometric Operations (async, submitted via wapi_io_t)
 * ============================================================ */

/**
 * Submit a biometric authentication request. Completion:
 *   result = WAPI_OK on success, WAPI_ERR_ACCES on denial/failure,
 *            WAPI_ERR_CANCELED if the user canceled.
 */
static inline wapi_result_t wapi_bio_authenticate(
    const wapi_io_t* io,
    uint32_t type, wapi_stringview_t reason,
    uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_BIO_AUTHENTICATE;
    op.flags     = type;
    op.addr      = reason.data;
    op.len       = reason.length;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

#ifdef __cplusplus
}
#endif

#endif /* WAPI_BIOMETRIC_H */
