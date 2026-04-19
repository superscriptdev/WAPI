/**
 * WAPI - Cryptography
 * Version 1.0.0
 *
 * Maps to: Web Crypto API, OS-level crypto providers
 *          (CommonCrypto, CNG, OpenSSL)
 *
 * Provides hardware-accelerated hashing, signing, encryption,
 * and key derivation. Complements wapi_env_random_get() which
 * provides raw random bytes.
 *
 * Import module: "wapi_crypto"
 */

#ifndef WAPI_CRYPTO_H
#define WAPI_CRYPTO_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Algorithms
 * ============================================================ */

typedef enum wapi_hash_algo_t {
    WAPI_HASH_SHA256    = 0,
    WAPI_HASH_SHA384    = 1,
    WAPI_HASH_SHA512    = 2,
    WAPI_HASH_SHA1      = 3,  /* Legacy, not recommended */
    WAPI_HASH_FORCE32   = 0x7FFFFFFF
} wapi_hash_algo_t;

typedef enum wapi_cipher_algo_t {
    WAPI_CIPHER_AES128_GCM = 0,
    WAPI_CIPHER_AES256_GCM = 1,
    WAPI_CIPHER_AES128_CBC = 2,
    WAPI_CIPHER_AES256_CBC = 3,
    WAPI_CIPHER_CHACHA20_POLY1305 = 4,
    WAPI_CIPHER_FORCE32     = 0x7FFFFFFF
} wapi_cipher_algo_t;

typedef enum wapi_sign_algo_t {
    WAPI_SIGN_HMAC_SHA256    = 0,
    WAPI_SIGN_HMAC_SHA512    = 1,
    WAPI_SIGN_ECDSA_P256     = 2,
    WAPI_SIGN_ECDSA_P384     = 3,
    WAPI_SIGN_ED25519        = 4,
    WAPI_SIGN_RSA_PSS_SHA256 = 5,
    WAPI_SIGN_FORCE32        = 0x7FFFFFFF
} wapi_sign_algo_t;

typedef enum wapi_kdf_algo_t {
    WAPI_KDF_PBKDF2_SHA256 = 0,
    WAPI_KDF_HKDF_SHA256   = 1,
    WAPI_KDF_HKDF_SHA512   = 2,
    WAPI_KDF_FORCE32       = 0x7FFFFFFF
} wapi_kdf_algo_t;

/* ============================================================
 * Crypto Operations (all async, submitted via wapi_io_t)
 *
 * One-shot variants (hash, encrypt, decrypt, sign, verify, derive_key)
 * go through IO because the underlying platform (Web Crypto) is
 * Promise-based and may be hardware-accelerated off-thread.
 *
 * Streaming hash — hash_create/hash_update/hash_finish — also goes
 * through IO for a consistent surface. On hosts where update() is
 * CPU-bounded it still completes in-line from the module's POV.
 * ============================================================ */

/** One-shot hash. Completion inlines the digest (up to 64B) in
 *  payload[0..digest_len-1] with WAPI_IO_CQE_F_INLINE set. */
static inline wapi_result_t wapi_crypto_hash(
    const wapi_io_t* io, wapi_hash_algo_t algo,
    const void* data, wapi_size_t data_len, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_CRYPTO_HASH;
    op.flags     = (uint32_t)algo;
    op.addr      = (uint64_t)(uintptr_t)data;
    op.len       = data_len;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Create a streaming hash context. */
static inline wapi_result_t wapi_crypto_hash_create(
    const wapi_io_t* io, wapi_hash_algo_t algo,
    wapi_handle_t* out_ctx, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode     = WAPI_IO_OP_CRYPTO_HASH_CREATE;
    op.flags      = (uint32_t)algo;
    op.result_ptr = (uint64_t)(uintptr_t)out_ctx;
    op.user_data  = user_data;
    return io->submit(io->impl, &op, 1);
}

/* hash_update / hash_finish use the same CRYPTO_HASH_CREATE sibling
 * machinery internally — they're driven off the context's fd through
 * the same namespace. The shim exposes them as sub-variants via
 * flags: 0=update, 1=finish on the CRYPTO_HASH_CREATE opcode, with
 * fd=ctx. Wrappers: */

static inline wapi_result_t wapi_crypto_hash_update(
    const wapi_io_t* io, wapi_handle_t ctx,
    const void* data, wapi_size_t data_len, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_CRYPTO_HASH_CREATE;
    op.fd        = ctx;
    op.flags     = 0; /* update */
    op.addr      = (uint64_t)(uintptr_t)data;
    op.len       = data_len;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

static inline wapi_result_t wapi_crypto_hash_finish(
    const wapi_io_t* io, wapi_handle_t ctx, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_CRYPTO_HASH_CREATE;
    op.fd        = ctx;
    op.flags     = 1; /* finish */
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/* ============================================================
 * Key Management
 * ============================================================ */

typedef enum wapi_key_usage_t {
    WAPI_KEY_USAGE_ENCRYPT  = 0x01,
    WAPI_KEY_USAGE_DECRYPT  = 0x02,
    WAPI_KEY_USAGE_SIGN     = 0x04,
    WAPI_KEY_USAGE_VERIFY   = 0x08,
    WAPI_KEY_USAGE_DERIVE   = 0x10,
    WAPI_KEY_USAGE_FORCE32  = 0x7FFFFFFF
} wapi_key_usage_t;

/** Import a raw symmetric key. */
static inline wapi_result_t wapi_crypto_key_import_raw(
    const wapi_io_t* io,
    const void* key_data, wapi_size_t key_len, uint32_t usages,
    wapi_handle_t* out_key, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode     = WAPI_IO_OP_CRYPTO_KEY_IMPORT_RAW;
    op.flags      = usages;
    op.addr       = (uint64_t)(uintptr_t)key_data;
    op.len        = key_len;
    op.result_ptr = (uint64_t)(uintptr_t)out_key;
    op.user_data  = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Generate a symmetric key. */
static inline wapi_result_t wapi_crypto_key_generate(
    const wapi_io_t* io, wapi_cipher_algo_t algo, uint32_t usages,
    wapi_handle_t* out_key, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode     = WAPI_IO_OP_CRYPTO_KEY_GENERATE;
    op.flags      = (uint32_t)algo;
    op.flags2     = usages;
    op.result_ptr = (uint64_t)(uintptr_t)out_key;
    op.user_data  = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Generate an asymmetric key pair. */
static inline wapi_result_t wapi_crypto_key_generate_pair(
    const wapi_io_t* io, wapi_sign_algo_t algo, uint32_t usages,
    wapi_handle_t* out_public, wapi_handle_t* out_private,
    uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_CRYPTO_KEY_GENERATE_PAIR;
    op.flags     = (uint32_t)algo;
    op.flags2    = usages;
    op.addr      = (uint64_t)(uintptr_t)out_public;
    op.addr2     = (uint64_t)(uintptr_t)out_private;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Release a key handle — bounded-local (drops host reference). */
WAPI_IMPORT(wapi_crypto, key_release)
wapi_result_t wapi_crypto_key_release(wapi_handle_t key);

/** Encrypt. IV pointer is packed into op.offset, IV length into the
 *  top 32 bits. */
static inline wapi_result_t wapi_crypto_encrypt(
    const wapi_io_t* io, wapi_cipher_algo_t algo, wapi_handle_t key,
    const void* iv, wapi_size_t iv_len,
    const void* plaintext, wapi_size_t pt_len,
    void* ciphertext, wapi_size_t ct_capacity,
    uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_CRYPTO_ENCRYPT;
    op.fd        = key;
    op.flags     = (uint32_t)algo;
    op.offset    = ((uint64_t)(uintptr_t)iv) | ((uint64_t)iv_len << 32);
    op.addr      = (uint64_t)(uintptr_t)plaintext;
    op.len       = pt_len;
    op.addr2     = (uint64_t)(uintptr_t)ciphertext;
    op.len2      = ct_capacity;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Decrypt. Same packing convention as encrypt. */
static inline wapi_result_t wapi_crypto_decrypt(
    const wapi_io_t* io, wapi_cipher_algo_t algo, wapi_handle_t key,
    const void* iv, wapi_size_t iv_len,
    const void* ciphertext, wapi_size_t ct_len,
    void* plaintext, wapi_size_t pt_capacity,
    uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_CRYPTO_DECRYPT;
    op.fd        = key;
    op.flags     = (uint32_t)algo;
    op.offset    = ((uint64_t)(uintptr_t)iv) | ((uint64_t)iv_len << 32);
    op.addr      = (uint64_t)(uintptr_t)ciphertext;
    op.len       = ct_len;
    op.addr2     = (uint64_t)(uintptr_t)plaintext;
    op.len2      = pt_capacity;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Sign. Signature up to 64 bytes inlines in the completion payload. */
static inline wapi_result_t wapi_crypto_sign(
    const wapi_io_t* io, wapi_sign_algo_t algo, wapi_handle_t key,
    const void* data, wapi_size_t data_len,
    void* signature, wapi_size_t sig_capacity,
    uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_CRYPTO_SIGN;
    op.fd        = key;
    op.flags     = (uint32_t)algo;
    op.addr      = (uint64_t)(uintptr_t)data;
    op.len       = data_len;
    op.addr2     = (uint64_t)(uintptr_t)signature;
    op.len2      = sig_capacity;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Verify a signature. Completion result = 1 valid, 0 invalid. */
static inline wapi_result_t wapi_crypto_verify(
    const wapi_io_t* io, wapi_sign_algo_t algo, wapi_handle_t key,
    const void* data, wapi_size_t data_len,
    const void* signature, wapi_size_t sig_len,
    uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_CRYPTO_VERIFY;
    op.fd        = key;
    op.flags     = (uint32_t)algo;
    op.addr      = (uint64_t)(uintptr_t)data;
    op.len       = data_len;
    op.addr2     = (uint64_t)(uintptr_t)signature;
    op.len2      = sig_len;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Derive a key. Result = new key handle (in result_ptr). */
static inline wapi_result_t wapi_crypto_derive_key(
    const wapi_io_t* io, wapi_kdf_algo_t algo, wapi_handle_t base_key,
    const void* salt, wapi_size_t salt_len,
    const void* info, wapi_size_t info_len,
    uint32_t iterations, wapi_size_t key_len_bits,
    wapi_handle_t* out_key, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode     = WAPI_IO_OP_CRYPTO_DERIVE_KEY;
    op.fd         = base_key;
    op.flags      = (uint32_t)algo;
    op.flags2     = iterations;
    op.offset     = key_len_bits;
    op.addr       = (uint64_t)(uintptr_t)salt;
    op.len        = salt_len;
    op.addr2      = (uint64_t)(uintptr_t)info;
    op.len2       = info_len;
    op.result_ptr = (uint64_t)(uintptr_t)out_key;
    op.user_data  = user_data;
    return io->submit(io->impl, &op, 1);
}

#ifdef __cplusplus
}
#endif

#endif /* WAPI_CRYPTO_H */
