/**
 * WAPI - Authentication
 * Version 1.0.0
 *
 * Create and use passkeys/FIDO2 credentials for passwordless
 * authentication.
 *
 * Maps to: Web Authentication API (WebAuthn), ASAuthorization (iOS/macOS),
 *          FIDO2 CredentialManager (Android), Windows Hello / WebAuthn Win32
 *
 * Import module: "wapi_authn"
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
_Static_assert(_Alignof(wapi_authn_credential_t) == 8,
               "wapi_authn_credential_t must be 8-byte aligned");

/* ============================================================
 * Authentication Operations (async, submitted via wapi_io_t)
 * ============================================================
 * The underlying ABI is IO-op submission; the inline helpers below
 * build a wapi_io_op_t and pass it to io->submit. Completion arrives
 * later on the event queue with the same user_data.
 *
 *   WAPI_IO_OP_AUTHN_CREDENTIAL_CREATE  (ns 0x0000 method 0x192)
 *   WAPI_IO_OP_AUTHN_ASSERTION_GET      (ns 0x0000 method 0x193)
 */

/**
 * Submit a "create credential" (register a passkey) request.
 *
 * @param io            I/O vtable to submit against.
 * @param rp_id         Relying party identifier.
 * @param challenge     Challenge bytes the RP sent.
 * @param challenge_len Challenge length.
 * @param out_cred      Memory where the host writes the credential
 *                      on completion. Must remain valid until the
 *                      completion event arrives.
 * @param user_data     Correlation token echoed in the completion.
 * @return Number of ops submitted (1) on success, negative on error.
 */
static inline wapi_result_t wapi_authn_create_credential(
    const wapi_io_t* io,
    wapi_stringview_t rp_id,
    const void* challenge, wapi_size_t challenge_len,
    wapi_authn_credential_t* out_cred,
    uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode     = WAPI_IO_OP_AUTHN_CREDENTIAL_CREATE;
    op.addr       = rp_id.data;
    op.len        = rp_id.length;
    op.addr2      = (uint64_t)(uintptr_t)challenge;
    op.len2       = challenge_len;
    op.result_ptr = (uint64_t)(uintptr_t)out_cred;
    op.user_data  = user_data;
    return io->submit(io->impl, &op, 1);
}

/**
 * Submit a "get assertion" (authenticate with existing passkey) request.
 */
static inline wapi_result_t wapi_authn_get_assertion(
    const wapi_io_t* io,
    wapi_stringview_t rp_id,
    const void* challenge, wapi_size_t challenge_len,
    wapi_authn_credential_t* out_cred,
    uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode     = WAPI_IO_OP_AUTHN_ASSERTION_GET;
    op.addr       = rp_id.data;
    op.len        = rp_id.length;
    op.addr2      = (uint64_t)(uintptr_t)challenge;
    op.len2       = challenge_len;
    op.result_ptr = (uint64_t)(uintptr_t)out_cred;
    op.user_data  = user_data;
    return io->submit(io->impl, &op, 1);
}

#ifdef __cplusplus
}
#endif

#endif /* WAPI_AUTHN_H */
