/**
 * WAPI - Web Authentication Capability
 * Version 1.0.0
 *
 * Create and use passkeys/FIDO2 credentials for passwordless
 * authentication.
 *
 * Maps to: Web Authentication API (WebAuthn), ASAuthorization (iOS/macOS),
 *          FIDO2 CredentialManager (Android), Windows Hello / WebAuthn Win32
 *
 * Import module: "wapi_authn"
 *
 * Query availability with wapi_capability_supported("wapi.authn", 9)
 */

#ifndef WAPI_AUTHN_H
#define WAPI_AUTHN_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Authenticator Attachment
 * ============================================================ */

typedef enum wapi_authn_type_t {
    WAPI_AUTHN_PLATFORM       = 0,  /* Platform authenticator (e.g., Touch ID, Windows Hello) */
    WAPI_AUTHN_CROSS_PLATFORM = 1,  /* Roaming authenticator (e.g., USB security key) */
    WAPI_AUTHN_FORCE32        = 0x7FFFFFFF
} wapi_authn_type_t;

/* ============================================================
 * Credential
 * ============================================================
 *
 * Layout (64 bytes, align 8):
 *   Offset  0: uint64_t id             Linear memory address of credential ID
 *   Offset  8: uint32_t id_len
 *   Offset 12: uint32_t type           (wapi_authn_type_t)
 *   Offset 16: uint64_t pubkey         Linear memory address of public key
 *   Offset 24: uint32_t pubkey_len
 *   Offset 28: uint32_t _pad0
 *   Offset 32: uint64_t user           Linear memory address of user data
 *   Offset 40: uint32_t user_len
 *   Offset 44: uint8_t  _reserved[20]  (reserved, must be zero)
 */

typedef struct wapi_authn_credential_t {
    uint64_t        id;             /* Linear memory address of credential ID */
    wapi_size_t     id_len;
    uint32_t        type;           /* wapi_authn_type_t */
    uint64_t        pubkey;         /* Linear memory address of public key */
    wapi_size_t     pubkey_len;
    uint32_t        _pad0;
    uint64_t        user;           /* Linear memory address of user data */
    wapi_size_t     user_len;
    uint8_t         _reserved[20];
} wapi_authn_credential_t;

_Static_assert(sizeof(wapi_authn_credential_t) == 64,
               "wapi_authn_credential_t must be 64 bytes");

/* ============================================================
 * Authentication Functions
 * ============================================================ */

/**
 * Create a new credential (register a passkey).
 *
 * @see WAPI_IO_OP_AUTHN_CREATE_CREDENTIAL
 *
 * @param rp_id          Relying party identifier (UTF-8, e.g., "example.com").
 * @param user_ptr       Pointer to user entity data (opaque to host).
 * @param challenge_ptr  Pointer to challenge bytes.
 * @param challenge_len  Challenge length.
 * @return WAPI_OK on success, WAPI_ERR_CANCELED if user canceled,
 *         WAPI_ERR_NOTSUP if not supported.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_authn, create_credential)
wapi_result_t wapi_authn_create_credential(wapi_string_view_t rp_id,
                                           const void* user_ptr,
                                           const void* challenge_ptr,
                                           wapi_size_t challenge_len);

/**
 * Get an assertion (authenticate with an existing credential).
 *
 * @param rp_id          Relying party identifier (UTF-8).
 * @param challenge_ptr  Pointer to challenge bytes.
 * @param challenge_len  Challenge length.
 * @return WAPI_OK on success, WAPI_ERR_CANCELED if user canceled,
 *         WAPI_ERR_NOENT if no matching credential found.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_authn, get_assertion)
wapi_result_t wapi_authn_get_assertion(wapi_string_view_t rp_id,
                                       const void* challenge_ptr,
                                       wapi_size_t challenge_len);

/**
 * Check if WebAuthn / passkey support is available on this platform.
 *
 * @return Non-zero if available, zero otherwise.
 *
 * Wasm signature: () -> i32
 */
WAPI_IMPORT(wapi_authn, is_available)
wapi_bool_t wapi_authn_is_available(void);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_AUTHN_H */
