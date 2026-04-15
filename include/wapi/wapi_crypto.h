/**
 * WAPI - Cryptography Capability
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
 *
 * Query availability with wapi_capability_supported("wapi.crypto", 9)
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
 * Hashing
 * ============================================================ */

/**
 * Compute a hash digest in one shot.
 *
 * @param algo       Hash algorithm.
 * @param data       Input data.
 * @param data_len   Input length.
 * @param digest     [out] Digest buffer (must be large enough for algo).
 * @param digest_len [out] Actual digest length.
 */
WAPI_IMPORT(wapi_crypto, hash)
wapi_result_t wapi_crypto_hash(wapi_hash_algo_t algo, const void* data,
                            wapi_size_t data_len, void* digest,
                            wapi_size_t* digest_len);

/**
 * Create a streaming hash context for incremental hashing.
 * @param algo    Hash algorithm.
 * @param ctx     [out] Hash context handle.
 */
WAPI_IMPORT(wapi_crypto, hash_create)
wapi_result_t wapi_crypto_hash_create(wapi_hash_algo_t algo, wapi_handle_t* ctx);

/**
 * Feed data into a streaming hash.
 */
WAPI_IMPORT(wapi_crypto, hash_update)
wapi_result_t wapi_crypto_hash_update(wapi_handle_t ctx, const void* data,
                                   wapi_size_t data_len);

/**
 * Finalize and get the digest. Destroys the context.
 */
WAPI_IMPORT(wapi_crypto, hash_finish)
wapi_result_t wapi_crypto_hash_finish(wapi_handle_t ctx, void* digest,
                                   wapi_size_t* digest_len);

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

/**
 * Import a raw symmetric key.
 */
WAPI_IMPORT(wapi_crypto, key_import_raw)
wapi_result_t wapi_crypto_key_import_raw(const void* key_data, wapi_size_t key_len,
                                      uint32_t usages, wapi_handle_t* key);

/**
 * Generate a random symmetric key.
 *
 * @param algo    Algorithm the key will be used with.
 * @param usages  Allowed key usages.
 * @param key     [out] Key handle.
 */
WAPI_IMPORT(wapi_crypto, key_generate)
wapi_result_t wapi_crypto_key_generate(wapi_cipher_algo_t algo, uint32_t usages,
                                    wapi_handle_t* key);

/**
 * Generate an asymmetric key pair.
 *
 * @param algo        Signing algorithm.
 * @param usages      Allowed key usages.
 * @param public_key  [out] Public key handle.
 * @param private_key [out] Private key handle.
 */
WAPI_IMPORT(wapi_crypto, key_generate_pair)
wapi_result_t wapi_crypto_key_generate_pair(wapi_sign_algo_t algo, uint32_t usages,
                                         wapi_handle_t* public_key,
                                         wapi_handle_t* private_key);

/**
 * Release a key handle.
 */
WAPI_IMPORT(wapi_crypto, key_release)
wapi_result_t wapi_crypto_key_release(wapi_handle_t key);

/* ============================================================
 * Encryption / Decryption
 * ============================================================ */

/**
 * Encrypt data.
 *
 * @param algo        Cipher algorithm.
 * @param key         Key handle.
 * @param iv          Initialization vector.
 * @param iv_len      IV length.
 * @param plaintext   Input data.
 * @param pt_len      Input length.
 * @param ciphertext  [out] Output buffer (must be pt_len + tag size).
 * @param ct_len      [out] Actual output length.
 */
WAPI_IMPORT(wapi_crypto, encrypt)
wapi_result_t wapi_crypto_encrypt(wapi_cipher_algo_t algo, wapi_handle_t key,
                               const void* iv, wapi_size_t iv_len,
                               const void* plaintext, wapi_size_t pt_len,
                               void* ciphertext, wapi_size_t* ct_len);

/**
 * Decrypt data.
 */
WAPI_IMPORT(wapi_crypto, decrypt)
wapi_result_t wapi_crypto_decrypt(wapi_cipher_algo_t algo, wapi_handle_t key,
                               const void* iv, wapi_size_t iv_len,
                               const void* ciphertext, wapi_size_t ct_len,
                               void* plaintext, wapi_size_t* pt_len);

/* ============================================================
 * Signing / Verification
 * ============================================================ */

/**
 * Sign data.
 */
WAPI_IMPORT(wapi_crypto, sign)
wapi_result_t wapi_crypto_sign(wapi_sign_algo_t algo, wapi_handle_t key,
                            const void* data, wapi_size_t data_len,
                            void* signature, wapi_size_t* sig_len);

/**
 * Verify a signature.
 *
 * @return WAPI_OK if valid, WAPI_ERR_INVAL if invalid.
 */
WAPI_IMPORT(wapi_crypto, verify)
wapi_result_t wapi_crypto_verify(wapi_sign_algo_t algo, wapi_handle_t key,
                              const void* data, wapi_size_t data_len,
                              const void* signature, wapi_size_t sig_len);

/* ============================================================
 * Key Derivation
 * ============================================================ */

/**
 * Derive a key from a password or master key.
 */
WAPI_IMPORT(wapi_crypto, derive_key)
wapi_result_t wapi_crypto_derive_key(wapi_kdf_algo_t algo, wapi_handle_t base_key,
                                  const void* salt, wapi_size_t salt_len,
                                  const void* info, wapi_size_t info_len,
                                  uint32_t iterations, wapi_size_t key_len,
                                  void* derived, wapi_size_t* derived_len);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_CRYPTO_H */
