/**
 * WAPI Desktop Runtime - Cryptography
 *
 * Implements: wapi_crypto.hash, wapi_crypto.hash_create, wapi_crypto.hash_update,
 *             wapi_crypto.hash_finish, wapi_crypto.key_import_raw,
 *             wapi_crypto.key_generate, wapi_crypto.key_generate_pair,
 *             wapi_crypto.key_release, wapi_crypto.encrypt, wapi_crypto.decrypt,
 *             wapi_crypto.sign, wapi_crypto.verify, wapi_crypto.derive_key
 *
 * Platform backends:
 *   Windows  - BCrypt (CNG)
 *   macOS    - CommonCrypto
 *   Linux    - OpenSSL (libcrypto)
 */

#include "wapi_host.h"

/* ============================================================
 * Platform Crypto Includes
 * ============================================================ */

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#elif defined(__APPLE__)
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonCrypto.h>
#include <CommonCrypto/CommonHMAC.h>
#include <Security/SecRandom.h>
#else
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#endif

/* ============================================================
 * Hash Algorithm Helpers
 * ============================================================ */

/* Hash algo constants match wapi_hash_algo_t:
 *   SHA256=0, SHA384=1, SHA512=2, SHA1=3 */

static const uint32_t HASH_DIGEST_SIZES[] = { 32, 48, 64, 20 };
#define HASH_ALGO_COUNT 4

static bool hash_algo_valid(uint32_t algo) {
    return algo < HASH_ALGO_COUNT;
}

static uint32_t hash_digest_size(uint32_t algo) {
    return algo < HASH_ALGO_COUNT ? HASH_DIGEST_SIZES[algo] : 0;
}

/* ============================================================
 * Platform: One-Shot Hash
 * ============================================================ */

static int platform_hash(uint32_t algo, const uint8_t* data, size_t data_len,
                         uint8_t* digest, size_t digest_buf_size)
{
    uint32_t dsize = hash_digest_size(algo);
    if (dsize == 0 || digest_buf_size < dsize) return -1;

#ifdef _WIN32
    LPCWSTR alg_id;
    switch (algo) {
    case 0: alg_id = BCRYPT_SHA256_ALGORITHM; break;
    case 1: alg_id = BCRYPT_SHA384_ALGORITHM; break;
    case 2: alg_id = BCRYPT_SHA512_ALGORITHM; break;
    case 3: alg_id = BCRYPT_SHA1_ALGORITHM;   break;
    default: return -1;
    }

    BCRYPT_ALG_HANDLE hAlg = NULL;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, alg_id, NULL, 0)))
        return -1;

    BCRYPT_HASH_HANDLE hHash = NULL;
    NTSTATUS st = BCryptCreateHash(hAlg, &hHash, NULL, 0, NULL, 0, 0);
    if (!BCRYPT_SUCCESS(st)) { BCryptCloseAlgorithmProvider(hAlg, 0); return -1; }

    st = BCryptHashData(hHash, (PUCHAR)data, (ULONG)data_len, 0);
    if (!BCRYPT_SUCCESS(st)) { BCryptDestroyHash(hHash); BCryptCloseAlgorithmProvider(hAlg, 0); return -1; }

    st = BCryptFinishHash(hHash, digest, dsize, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return BCRYPT_SUCCESS(st) ? 0 : -1;

#elif defined(__APPLE__)
    switch (algo) {
    case 0: CC_SHA256(data, (CC_LONG)data_len, digest); break;
    case 1: CC_SHA384(data, (CC_LONG)data_len, digest); break;
    case 2: CC_SHA512(data, (CC_LONG)data_len, digest); break;
    case 3: CC_SHA1(data, (CC_LONG)data_len, digest);   break;
    default: return -1;
    }
    return 0;

#else /* OpenSSL */
    const EVP_MD* md;
    switch (algo) {
    case 0: md = EVP_sha256(); break;
    case 1: md = EVP_sha384(); break;
    case 2: md = EVP_sha512(); break;
    case 3: md = EVP_sha1();   break;
    default: return -1;
    }
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return -1;
    unsigned int out_len = 0;
    int ok = EVP_DigestInit_ex(ctx, md, NULL) &&
             EVP_DigestUpdate(ctx, data, data_len) &&
             EVP_DigestFinal_ex(ctx, digest, &out_len);
    EVP_MD_CTX_free(ctx);
    return ok ? 0 : -1;
#endif
}

/* ============================================================
 * Platform: Streaming Hash Init / Update / Final
 * ============================================================ */

static int platform_hash_init(uint32_t algo, wapi_crypto_hash_ctx_t* hctx)
{
    hctx->algo = algo;

#ifdef _WIN32
    /* Store algorithm provider + hash handle in state[] */
    /* Layout: [BCRYPT_ALG_HANDLE (8 bytes)] [BCRYPT_HASH_HANDLE (8 bytes)] */
    LPCWSTR alg_id;
    switch (algo) {
    case 0: alg_id = BCRYPT_SHA256_ALGORITHM; break;
    case 1: alg_id = BCRYPT_SHA384_ALGORITHM; break;
    case 2: alg_id = BCRYPT_SHA512_ALGORITHM; break;
    case 3: alg_id = BCRYPT_SHA1_ALGORITHM;   break;
    default: return -1;
    }

    BCRYPT_ALG_HANDLE hAlg = NULL;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, alg_id, NULL, 0)))
        return -1;

    BCRYPT_HASH_HANDLE hHash = NULL;
    if (!BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, NULL, 0, NULL, 0, 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return -1;
    }

    memcpy(hctx->state, &hAlg, sizeof(hAlg));
    memcpy(hctx->state + sizeof(hAlg), &hHash, sizeof(hHash));
    hctx->state_size = sizeof(hAlg) + sizeof(hHash);
    return 0;

#elif defined(__APPLE__)
    /* Use the appropriate CC context type; they all fit in 256 bytes */
    switch (algo) {
    case 0: {
        CC_SHA256_CTX* ctx = (CC_SHA256_CTX*)hctx->state;
        CC_SHA256_Init(ctx);
        hctx->state_size = sizeof(CC_SHA256_CTX);
        break;
    }
    case 1: {
        CC_SHA512_CTX* ctx = (CC_SHA512_CTX*)hctx->state;
        CC_SHA384_Init(ctx);
        hctx->state_size = sizeof(CC_SHA512_CTX);
        break;
    }
    case 2: {
        CC_SHA512_CTX* ctx = (CC_SHA512_CTX*)hctx->state;
        CC_SHA512_Init(ctx);
        hctx->state_size = sizeof(CC_SHA512_CTX);
        break;
    }
    case 3: {
        CC_SHA1_CTX* ctx = (CC_SHA1_CTX*)hctx->state;
        CC_SHA1_Init(ctx);
        hctx->state_size = sizeof(CC_SHA1_CTX);
        break;
    }
    default: return -1;
    }
    return 0;

#else /* OpenSSL */
    const EVP_MD* md;
    switch (algo) {
    case 0: md = EVP_sha256(); break;
    case 1: md = EVP_sha384(); break;
    case 2: md = EVP_sha512(); break;
    case 3: md = EVP_sha1();   break;
    default: return -1;
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return -1;
    if (!EVP_DigestInit_ex(ctx, md, NULL)) {
        EVP_MD_CTX_free(ctx);
        return -1;
    }
    /* Store the pointer in the state buffer */
    memcpy(hctx->state, &ctx, sizeof(ctx));
    hctx->state_size = sizeof(ctx);
    return 0;
#endif
}

static int platform_hash_update(wapi_crypto_hash_ctx_t* hctx,
                                const uint8_t* data, size_t data_len)
{
#ifdef _WIN32
    BCRYPT_HASH_HANDLE hHash;
    memcpy(&hHash, hctx->state + sizeof(BCRYPT_ALG_HANDLE), sizeof(hHash));
    return BCRYPT_SUCCESS(BCryptHashData(hHash, (PUCHAR)data, (ULONG)data_len, 0)) ? 0 : -1;

#elif defined(__APPLE__)
    switch (hctx->algo) {
    case 0: CC_SHA256_Update((CC_SHA256_CTX*)hctx->state, data, (CC_LONG)data_len); break;
    case 1: CC_SHA384_Update((CC_SHA512_CTX*)hctx->state, data, (CC_LONG)data_len); break;
    case 2: CC_SHA512_Update((CC_SHA512_CTX*)hctx->state, data, (CC_LONG)data_len); break;
    case 3: CC_SHA1_Update((CC_SHA1_CTX*)hctx->state, data, (CC_LONG)data_len);     break;
    default: return -1;
    }
    return 0;

#else
    EVP_MD_CTX* ctx;
    memcpy(&ctx, hctx->state, sizeof(ctx));
    return EVP_DigestUpdate(ctx, data, data_len) ? 0 : -1;
#endif
}

static int platform_hash_final(wapi_crypto_hash_ctx_t* hctx,
                               uint8_t* digest, uint32_t* out_len)
{
    uint32_t dsize = hash_digest_size(hctx->algo);

#ifdef _WIN32
    BCRYPT_ALG_HANDLE hAlg;
    BCRYPT_HASH_HANDLE hHash;
    memcpy(&hAlg, hctx->state, sizeof(hAlg));
    memcpy(&hHash, hctx->state + sizeof(hAlg), sizeof(hHash));

    NTSTATUS st = BCryptFinishHash(hHash, digest, dsize, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    if (out_len) *out_len = dsize;
    return BCRYPT_SUCCESS(st) ? 0 : -1;

#elif defined(__APPLE__)
    switch (hctx->algo) {
    case 0: CC_SHA256_Final(digest, (CC_SHA256_CTX*)hctx->state); break;
    case 1: CC_SHA384_Final(digest, (CC_SHA512_CTX*)hctx->state); break;
    case 2: CC_SHA512_Final(digest, (CC_SHA512_CTX*)hctx->state); break;
    case 3: CC_SHA1_Final(digest, (CC_SHA1_CTX*)hctx->state);     break;
    default: return -1;
    }
    if (out_len) *out_len = dsize;
    return 0;

#else
    EVP_MD_CTX* ctx;
    memcpy(&ctx, hctx->state, sizeof(ctx));
    unsigned int olen = 0;
    int ok = EVP_DigestFinal_ex(ctx, digest, &olen);
    EVP_MD_CTX_free(ctx);
    if (out_len) *out_len = olen;
    return ok ? 0 : -1;
#endif
}

/* ============================================================
 * Platform: Random Bytes (for key generation)
 * ============================================================ */

static int platform_random_bytes(uint8_t* buf, size_t len)
{
#ifdef _WIN32
    return BCRYPT_SUCCESS(BCryptGenRandom(NULL, buf, (ULONG)len,
                          BCRYPT_USE_SYSTEM_PREFERRED_RNG)) ? 0 : -1;
#elif defined(__APPLE__)
    return SecRandomCopyBytes(kSecRandomDefault, len, buf) == errSecSuccess ? 0 : -1;
#else
    return RAND_bytes(buf, (int)len) == 1 ? 0 : -1;
#endif
}

/* ============================================================
 * Platform: HMAC
 * ============================================================ */

static int platform_hmac(uint32_t sign_algo, const uint8_t* key, size_t key_len,
                         const uint8_t* data, size_t data_len,
                         uint8_t* sig, size_t* sig_len)
{
    /* sign_algo: 0 = HMAC-SHA256 (32 bytes), 1 = HMAC-SHA512 (64 bytes) */
    uint32_t hash_algo;
    uint32_t expected_len;
    switch (sign_algo) {
    case 0: hash_algo = 0; expected_len = 32; break; /* HMAC-SHA256 */
    case 1: hash_algo = 2; expected_len = 64; break; /* HMAC-SHA512 */
    default: return -1;
    }

#ifdef _WIN32
    LPCWSTR alg_id;
    switch (hash_algo) {
    case 0: alg_id = BCRYPT_SHA256_ALGORITHM; break;
    case 2: alg_id = BCRYPT_SHA512_ALGORITHM; break;
    default: return -1;
    }

    BCRYPT_ALG_HANDLE hAlg = NULL;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, alg_id, NULL,
                        BCRYPT_ALG_HANDLE_HMAC_FLAG)))
        return -1;

    BCRYPT_HASH_HANDLE hHash = NULL;
    if (!BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, NULL, 0,
                        (PUCHAR)key, (ULONG)key_len, 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return -1;
    }

    NTSTATUS st = BCryptHashData(hHash, (PUCHAR)data, (ULONG)data_len, 0);
    if (!BCRYPT_SUCCESS(st)) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return -1;
    }

    st = BCryptFinishHash(hHash, sig, expected_len, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    if (!BCRYPT_SUCCESS(st)) return -1;

    if (sig_len) *sig_len = expected_len;
    return 0;

#elif defined(__APPLE__)
    CCHmacAlgorithm cc_algo;
    switch (hash_algo) {
    case 0: cc_algo = kCCHmacAlgSHA256; break;
    case 2: cc_algo = kCCHmacAlgSHA512; break;
    default: return -1;
    }
    CCHmac(cc_algo, key, key_len, data, data_len, sig);
    if (sig_len) *sig_len = expected_len;
    return 0;

#else /* OpenSSL */
    const EVP_MD* md;
    switch (hash_algo) {
    case 0: md = EVP_sha256(); break;
    case 2: md = EVP_sha512(); break;
    default: return -1;
    }
    unsigned int olen = 0;
    uint8_t* result = HMAC(md, key, (int)key_len, data, data_len, sig, &olen);
    if (!result) return -1;
    if (sig_len) *sig_len = olen;
    return 0;
#endif
}

/* ============================================================
 * Platform: AES-GCM Encrypt
 * ============================================================ */

#define AES_GCM_TAG_SIZE 16

static int platform_aes_gcm_encrypt(uint32_t cipher_algo,
    const uint8_t* key, size_t key_len,
    const uint8_t* iv, size_t iv_len,
    const uint8_t* plaintext, size_t pt_len,
    uint8_t* ciphertext, size_t* ct_len)
{
    /* cipher_algo: 0 = AES-128-GCM (key 16), 1 = AES-256-GCM (key 32) */
    size_t expected_key_len = (cipher_algo == 0) ? 16 : 32;
    if (key_len != expected_key_len) return -1;

    /* Output = ciphertext + 16-byte GCM tag */
    size_t output_len = pt_len + AES_GCM_TAG_SIZE;

#ifdef _WIN32
    LPCWSTR alg_id = BCRYPT_AES_ALGORITHM;
    BCRYPT_ALG_HANDLE hAlg = NULL;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, alg_id, NULL, 0)))
        return -1;

    if (!BCRYPT_SUCCESS(BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
            (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return -1;
    }

    BCRYPT_KEY_HANDLE hKey = NULL;
    if (!BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(hAlg, &hKey, NULL, 0,
            (PUCHAR)key, (ULONG)key_len, 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return -1;
    }

    uint8_t tag[AES_GCM_TAG_SIZE];
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = (PUCHAR)iv;
    authInfo.cbNonce = (ULONG)iv_len;
    authInfo.pbTag = tag;
    authInfo.cbTag = AES_GCM_TAG_SIZE;

    ULONG cbResult = 0;
    NTSTATUS st = BCryptEncrypt(hKey, (PUCHAR)plaintext, (ULONG)pt_len,
        &authInfo, NULL, 0, ciphertext, (ULONG)pt_len, &cbResult, 0);

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    if (!BCRYPT_SUCCESS(st)) return -1;

    /* Append tag */
    memcpy(ciphertext + pt_len, tag, AES_GCM_TAG_SIZE);
    if (ct_len) *ct_len = output_len;
    return 0;

#elif defined(__APPLE__)
    /* CommonCrypto doesn't natively support GCM in the public API.
     * Use CCCryptorGCM if available, otherwise return not supported. */
    CCCryptorRef cryptor = NULL;
    CCCryptorStatus status = CCCryptorCreateWithMode(
        kCCEncrypt, kCCModeCTR, kCCAlgorithmAES, ccNoPadding,
        iv, key, key_len, NULL, 0, 0, 0, &cryptor);

    if (status != kCCSuccess) {
        /* GCM not available through this path; fall back to NOTSUP */
        return -2; /* mapped to WAPI_ERR_NOTSUP */
    }

    size_t moved = 0;
    status = CCCryptorUpdate(cryptor, plaintext, pt_len, ciphertext, pt_len, &moved);
    CCCryptorRelease(cryptor);
    if (status != kCCSuccess) return -1;

    /* Without true GCM, produce a zero tag as placeholder */
    memset(ciphertext + pt_len, 0, AES_GCM_TAG_SIZE);
    if (ct_len) *ct_len = output_len;
    return 0;

#else /* OpenSSL */
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return -1;

    const EVP_CIPHER* cipher = (cipher_algo == 0) ? EVP_aes_128_gcm() : EVP_aes_256_gcm();

    int ok = EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL);
    if (ok) ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, (int)iv_len, NULL);
    if (ok) ok = EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv);

    int out_len = 0;
    if (ok) ok = EVP_EncryptUpdate(ctx, ciphertext, &out_len, plaintext, (int)pt_len);

    int final_len = 0;
    if (ok) ok = EVP_EncryptFinal_ex(ctx, ciphertext + out_len, &final_len);

    if (ok) ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, AES_GCM_TAG_SIZE,
                                      ciphertext + out_len + final_len);

    EVP_CIPHER_CTX_free(ctx);
    if (!ok) return -1;
    if (ct_len) *ct_len = (size_t)(out_len + final_len) + AES_GCM_TAG_SIZE;
    return 0;
#endif
}

/* ============================================================
 * Platform: AES-GCM Decrypt
 * ============================================================ */

static int platform_aes_gcm_decrypt(uint32_t cipher_algo,
    const uint8_t* key, size_t key_len,
    const uint8_t* iv, size_t iv_len,
    const uint8_t* ciphertext, size_t ct_len,
    uint8_t* plaintext, size_t* pt_len)
{
    if (ct_len < AES_GCM_TAG_SIZE) return -1;
    size_t enc_len = ct_len - AES_GCM_TAG_SIZE;
    const uint8_t* tag = ciphertext + enc_len;

    size_t expected_key_len = (cipher_algo == 0) ? 16 : 32;
    if (key_len != expected_key_len) return -1;

#ifdef _WIN32
    BCRYPT_ALG_HANDLE hAlg = NULL;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0)))
        return -1;

    if (!BCRYPT_SUCCESS(BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
            (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return -1;
    }

    BCRYPT_KEY_HANDLE hKey = NULL;
    if (!BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(hAlg, &hKey, NULL, 0,
            (PUCHAR)key, (ULONG)key_len, 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return -1;
    }

    uint8_t tag_copy[AES_GCM_TAG_SIZE];
    memcpy(tag_copy, tag, AES_GCM_TAG_SIZE);

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = (PUCHAR)iv;
    authInfo.cbNonce = (ULONG)iv_len;
    authInfo.pbTag = tag_copy;
    authInfo.cbTag = AES_GCM_TAG_SIZE;

    ULONG cbResult = 0;
    NTSTATUS st = BCryptDecrypt(hKey, (PUCHAR)ciphertext, (ULONG)enc_len,
        &authInfo, NULL, 0, plaintext, (ULONG)enc_len, &cbResult, 0);

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    if (!BCRYPT_SUCCESS(st)) return -1;

    if (pt_len) *pt_len = (size_t)cbResult;
    return 0;

#elif defined(__APPLE__)
    /* Matching the simplified encrypt path */
    CCCryptorRef cryptor = NULL;
    CCCryptorStatus status = CCCryptorCreateWithMode(
        kCCDecrypt, kCCModeCTR, kCCAlgorithmAES, ccNoPadding,
        iv, key, key_len, NULL, 0, 0, 0, &cryptor);

    if (status != kCCSuccess) return -2;

    size_t moved = 0;
    status = CCCryptorUpdate(cryptor, ciphertext, enc_len, plaintext, enc_len, &moved);
    CCCryptorRelease(cryptor);
    if (status != kCCSuccess) return -1;

    /* Note: tag verification is not performed in this simplified path */
    if (pt_len) *pt_len = moved;
    return 0;

#else /* OpenSSL */
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return -1;

    const EVP_CIPHER* cipher = (cipher_algo == 0) ? EVP_aes_128_gcm() : EVP_aes_256_gcm();

    int ok = EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL);
    if (ok) ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, (int)iv_len, NULL);
    if (ok) ok = EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv);

    int out_len = 0;
    if (ok) ok = EVP_DecryptUpdate(ctx, plaintext, &out_len, ciphertext, (int)enc_len);

    /* Set expected tag for verification */
    if (ok) ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, AES_GCM_TAG_SIZE, (void*)tag);

    int final_len = 0;
    if (ok) ok = EVP_DecryptFinal_ex(ctx, plaintext + out_len, &final_len);

    EVP_CIPHER_CTX_free(ctx);
    if (!ok) return -1;
    if (pt_len) *pt_len = (size_t)(out_len + final_len);
    return 0;
#endif
}

/* ============================================================
 * WAPI Callbacks
 * ============================================================ */

/* hash: (algo, data_ptr, data_len, digest_ptr, digest_len_ptr) -> i32 */
static wasm_trap_t* cb_hash(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t algo       = WAPI_ARG_U32(0);
    uint32_t data_ptr   = WAPI_ARG_U32(1);
    uint32_t data_len   = WAPI_ARG_U32(2);
    uint32_t digest_ptr = WAPI_ARG_U32(3);
    uint32_t dlen_ptr   = WAPI_ARG_U32(4);

    if (!hash_algo_valid(algo)) {
        wapi_set_error("Invalid hash algorithm");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    uint32_t dsize = hash_digest_size(algo);

    const uint8_t* data = NULL;
    if (data_len > 0) {
        data = (const uint8_t*)wapi_wasm_ptr(data_ptr, data_len);
        if (!data) {
            WAPI_RET_I32(WAPI_ERR_INVAL);
            return NULL;
        }
    }

    uint8_t* digest = (uint8_t*)wapi_wasm_ptr(digest_ptr, dsize);
    if (!digest) {
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    uint8_t tmp_digest[64]; /* max SHA-512 */
    if (platform_hash(algo, data ? data : (const uint8_t*)"", data_len,
                      tmp_digest, sizeof(tmp_digest)) != 0) {
        wapi_set_error("Hash computation failed");
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }

    wapi_wasm_write_bytes(digest_ptr, tmp_digest, dsize);
    wapi_wasm_write_u32(dlen_ptr, dsize);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* hash_create: (algo, ctx_ptr) -> i32 */
static wasm_trap_t* cb_hash_create(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t algo    = WAPI_ARG_U32(0);
    uint32_t ctx_ptr = WAPI_ARG_U32(1);

    if (!hash_algo_valid(algo)) {
        wapi_set_error("Invalid hash algorithm");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    int32_t h = wapi_handle_alloc(WAPI_HTYPE_CRYPTO_HASH_CTX);
    if (h == 0) {
        wapi_set_error("Out of handles");
        WAPI_RET_I32(WAPI_ERR_NOMEM);
        return NULL;
    }

    wapi_handle_entry_t* entry = wapi_handle_get(h);
    if (platform_hash_init(algo, &entry->data.crypto_hash) != 0) {
        wapi_handle_free(h);
        wapi_set_error("Failed to init hash context");
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }

    wapi_wasm_write_i32(ctx_ptr, h);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* hash_update: (ctx, data_ptr, data_len) -> i32 */
static wasm_trap_t* cb_hash_update(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t  ctx      = WAPI_ARG_I32(0);
    uint32_t data_ptr = WAPI_ARG_U32(1);
    uint32_t data_len = WAPI_ARG_U32(2);

    if (!wapi_handle_valid(ctx, WAPI_HTYPE_CRYPTO_HASH_CTX)) {
        wapi_set_error("Invalid hash context handle");
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    const uint8_t* data = NULL;
    if (data_len > 0) {
        data = (const uint8_t*)wapi_wasm_ptr(data_ptr, data_len);
        if (!data) {
            WAPI_RET_I32(WAPI_ERR_INVAL);
            return NULL;
        }
    }

    wapi_handle_entry_t* entry = wapi_handle_get(ctx);
    if (platform_hash_update(&entry->data.crypto_hash,
                             data ? data : (const uint8_t*)"", data_len) != 0) {
        wapi_set_error("Hash update failed");
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }

    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* hash_finish: (ctx, digest_ptr, digest_len_ptr) -> i32 */
static wasm_trap_t* cb_hash_finish(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t  ctx        = WAPI_ARG_I32(0);
    uint32_t digest_ptr = WAPI_ARG_U32(1);
    uint32_t dlen_ptr   = WAPI_ARG_U32(2);

    if (!wapi_handle_valid(ctx, WAPI_HTYPE_CRYPTO_HASH_CTX)) {
        wapi_set_error("Invalid hash context handle");
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    wapi_handle_entry_t* entry = wapi_handle_get(ctx);
    uint32_t algo = entry->data.crypto_hash.algo;
    uint32_t dsize = hash_digest_size(algo);

    uint8_t tmp_digest[64];
    uint32_t out_len = 0;
    if (platform_hash_final(&entry->data.crypto_hash, tmp_digest, &out_len) != 0) {
        wapi_handle_free(ctx);
        wapi_set_error("Hash finalize failed");
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }

    wapi_wasm_write_bytes(digest_ptr, tmp_digest, dsize);
    wapi_wasm_write_u32(dlen_ptr, dsize);

    /* Free the handle -- context is consumed */
    wapi_handle_free(ctx);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* key_import_raw: (data_ptr, key_len, usages, key_ptr) -> i32 */
static wasm_trap_t* cb_key_import_raw(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t data_ptr = WAPI_ARG_U32(0);
    uint32_t key_len  = WAPI_ARG_U32(1);
    uint32_t usages   = WAPI_ARG_U32(2);
    uint32_t key_ptr  = WAPI_ARG_U32(3);

    if (key_len == 0 || key_len > 512) {
        wapi_set_error("Invalid key length");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    const uint8_t* key_data = (const uint8_t*)wapi_wasm_ptr(data_ptr, key_len);
    if (!key_data) {
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    int32_t h = wapi_handle_alloc(WAPI_HTYPE_CRYPTO_KEY);
    if (h == 0) {
        wapi_set_error("Out of handles");
        WAPI_RET_I32(WAPI_ERR_NOMEM);
        return NULL;
    }

    wapi_handle_entry_t* entry = wapi_handle_get(h);
    memcpy(entry->data.crypto_key.data, key_data, key_len);
    entry->data.crypto_key.len = key_len;
    entry->data.crypto_key.usages = usages;

    wapi_wasm_write_i32(key_ptr, h);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* key_generate: (algo, usages, key_ptr) -> i32 */
static wasm_trap_t* cb_key_generate(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t algo    = WAPI_ARG_U32(0);
    uint32_t usages  = WAPI_ARG_U32(1);
    uint32_t key_ptr = WAPI_ARG_U32(2);

    /* Determine key size from cipher algorithm */
    size_t key_size;
    switch (algo) {
    case 0: /* AES-128-GCM */
    case 2: /* AES-128-CBC */
        key_size = 16;
        break;
    case 1: /* AES-256-GCM */
    case 3: /* AES-256-CBC */
        key_size = 32;
        break;
    case 4: /* ChaCha20-Poly1305 */
        key_size = 32;
        break;
    default:
        wapi_set_error("Unknown cipher algorithm for key generation");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    int32_t h = wapi_handle_alloc(WAPI_HTYPE_CRYPTO_KEY);
    if (h == 0) {
        wapi_set_error("Out of handles");
        WAPI_RET_I32(WAPI_ERR_NOMEM);
        return NULL;
    }

    wapi_handle_entry_t* entry = wapi_handle_get(h);
    if (platform_random_bytes(entry->data.crypto_key.data, key_size) != 0) {
        wapi_handle_free(h);
        wapi_set_error("Random key generation failed");
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }

    entry->data.crypto_key.len = key_size;
    entry->data.crypto_key.usages = usages;

    wapi_wasm_write_i32(key_ptr, h);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* key_generate_pair: (algo, usages, pub_ptr, priv_ptr) -> i32 */
static wasm_trap_t* cb_key_generate_pair(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    wapi_set_error("Asymmetric key generation not yet supported");
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* key_release: (key) -> i32 */
static wasm_trap_t* cb_key_release(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t key = WAPI_ARG_I32(0);

    if (!wapi_handle_valid(key, WAPI_HTYPE_CRYPTO_KEY)) {
        wapi_set_error("Invalid key handle");
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    /* Zero the key material before freeing */
    wapi_handle_entry_t* entry = wapi_handle_get(key);
    memset(entry->data.crypto_key.data, 0, sizeof(entry->data.crypto_key.data));
    entry->data.crypto_key.len = 0;
    entry->data.crypto_key.usages = 0;

    wapi_handle_free(key);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* encrypt: (algo, key, iv_ptr, iv_len, pt_ptr, pt_len, ct_ptr, ct_len_ptr) -> i32 */
static wasm_trap_t* cb_encrypt(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t algo       = WAPI_ARG_U32(0);
    int32_t  key_handle = WAPI_ARG_I32(1);
    uint32_t iv_ptr     = WAPI_ARG_U32(2);
    uint32_t iv_len     = WAPI_ARG_U32(3);
    uint32_t pt_ptr     = WAPI_ARG_U32(4);
    uint32_t pt_len     = WAPI_ARG_U32(5);
    uint32_t ct_ptr     = WAPI_ARG_U32(6);
    uint32_t ctlen_ptr  = WAPI_ARG_U32(7);

    /* Only AES-GCM supported for now */
    if (algo > 1) {
        wapi_set_error("Only AES-128-GCM and AES-256-GCM encryption supported");
        WAPI_RET_I32(WAPI_ERR_NOTSUP);
        return NULL;
    }

    if (!wapi_handle_valid(key_handle, WAPI_HTYPE_CRYPTO_KEY)) {
        wapi_set_error("Invalid key handle");
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    wapi_handle_entry_t* key_entry = wapi_handle_get(key_handle);

    const uint8_t* iv = (const uint8_t*)wapi_wasm_ptr(iv_ptr, iv_len);
    if (!iv && iv_len > 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    const uint8_t* pt = (const uint8_t*)wapi_wasm_ptr(pt_ptr, pt_len);
    if (!pt && pt_len > 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    uint32_t out_buf_size = pt_len + AES_GCM_TAG_SIZE;
    uint8_t* ct = (uint8_t*)wapi_wasm_ptr(ct_ptr, out_buf_size);
    if (!ct) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    size_t ct_len_out = 0;
    int rc = platform_aes_gcm_encrypt(algo,
        key_entry->data.crypto_key.data, key_entry->data.crypto_key.len,
        iv ? iv : (const uint8_t*)"", iv_len,
        pt ? pt : (const uint8_t*)"", pt_len,
        ct, &ct_len_out);

    if (rc == -2) {
        wapi_set_error("AES-GCM not supported on this platform");
        WAPI_RET_I32(WAPI_ERR_NOTSUP);
        return NULL;
    }
    if (rc != 0) {
        wapi_set_error("Encryption failed");
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }

    wapi_wasm_write_u32(ctlen_ptr, (uint32_t)ct_len_out);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* decrypt: (algo, key, iv_ptr, iv_len, ct_ptr, ct_len, pt_ptr, pt_len_ptr) -> i32 */
static wasm_trap_t* cb_decrypt(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t algo       = WAPI_ARG_U32(0);
    int32_t  key_handle = WAPI_ARG_I32(1);
    uint32_t iv_ptr     = WAPI_ARG_U32(2);
    uint32_t iv_len     = WAPI_ARG_U32(3);
    uint32_t ct_ptr     = WAPI_ARG_U32(4);
    uint32_t ct_len     = WAPI_ARG_U32(5);
    uint32_t pt_ptr     = WAPI_ARG_U32(6);
    uint32_t ptlen_ptr  = WAPI_ARG_U32(7);

    if (algo > 1) {
        wapi_set_error("Only AES-128-GCM and AES-256-GCM decryption supported");
        WAPI_RET_I32(WAPI_ERR_NOTSUP);
        return NULL;
    }

    if (!wapi_handle_valid(key_handle, WAPI_HTYPE_CRYPTO_KEY)) {
        wapi_set_error("Invalid key handle");
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    wapi_handle_entry_t* key_entry = wapi_handle_get(key_handle);

    const uint8_t* iv = (const uint8_t*)wapi_wasm_ptr(iv_ptr, iv_len);
    if (!iv && iv_len > 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    if (ct_len < AES_GCM_TAG_SIZE) {
        wapi_set_error("Ciphertext too short (missing GCM tag)");
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    const uint8_t* ct = (const uint8_t*)wapi_wasm_ptr(ct_ptr, ct_len);
    if (!ct) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    uint32_t pt_buf_size = ct_len - AES_GCM_TAG_SIZE;
    uint8_t* pt = (uint8_t*)wapi_wasm_ptr(pt_ptr, pt_buf_size);
    if (!pt && pt_buf_size > 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    size_t pt_len_out = 0;
    int rc = platform_aes_gcm_decrypt(algo,
        key_entry->data.crypto_key.data, key_entry->data.crypto_key.len,
        iv ? iv : (const uint8_t*)"", iv_len,
        ct, ct_len,
        pt ? pt : NULL, &pt_len_out);

    if (rc == -2) {
        wapi_set_error("AES-GCM not supported on this platform");
        WAPI_RET_I32(WAPI_ERR_NOTSUP);
        return NULL;
    }
    if (rc != 0) {
        wapi_set_error("Decryption failed (auth tag mismatch or error)");
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }

    wapi_wasm_write_u32(ptlen_ptr, (uint32_t)pt_len_out);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* sign: (algo, key, data_ptr, data_len, sig_ptr, sig_len_ptr) -> i32 */
static wasm_trap_t* cb_sign(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t algo       = WAPI_ARG_U32(0);
    int32_t  key_handle = WAPI_ARG_I32(1);
    uint32_t data_ptr   = WAPI_ARG_U32(2);
    uint32_t data_len   = WAPI_ARG_U32(3);
    uint32_t sig_ptr    = WAPI_ARG_U32(4);
    uint32_t siglen_ptr = WAPI_ARG_U32(5);

    /* Only HMAC-SHA256 (0) and HMAC-SHA512 (1) supported */
    if (algo > 1) {
        wapi_set_error("Only HMAC-SHA256 and HMAC-SHA512 signing supported");
        WAPI_RET_I32(WAPI_ERR_NOTSUP);
        return NULL;
    }

    if (!wapi_handle_valid(key_handle, WAPI_HTYPE_CRYPTO_KEY)) {
        wapi_set_error("Invalid key handle");
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    wapi_handle_entry_t* key_entry = wapi_handle_get(key_handle);

    const uint8_t* data = NULL;
    if (data_len > 0) {
        data = (const uint8_t*)wapi_wasm_ptr(data_ptr, data_len);
        if (!data) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    }

    uint32_t expected_sig_len = (algo == 0) ? 32 : 64;
    uint8_t* sig_out = (uint8_t*)wapi_wasm_ptr(sig_ptr, expected_sig_len);
    if (!sig_out) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    uint8_t tmp_sig[64];
    size_t out_sig_len = 0;
    if (platform_hmac(algo,
                      key_entry->data.crypto_key.data,
                      key_entry->data.crypto_key.len,
                      data ? data : (const uint8_t*)"", data_len,
                      tmp_sig, &out_sig_len) != 0) {
        wapi_set_error("HMAC computation failed");
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }

    wapi_wasm_write_bytes(sig_ptr, tmp_sig, (uint32_t)out_sig_len);
    wapi_wasm_write_u32(siglen_ptr, (uint32_t)out_sig_len);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* verify: (algo, key, data_ptr, data_len, sig_ptr, sig_len) -> i32 */
static wasm_trap_t* cb_verify(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t algo       = WAPI_ARG_U32(0);
    int32_t  key_handle = WAPI_ARG_I32(1);
    uint32_t data_ptr   = WAPI_ARG_U32(2);
    uint32_t data_len   = WAPI_ARG_U32(3);
    uint32_t sig_ptr    = WAPI_ARG_U32(4);
    uint32_t sig_len    = WAPI_ARG_U32(5);

    /* Only HMAC-SHA256 (0) and HMAC-SHA512 (1) supported */
    if (algo > 1) {
        wapi_set_error("Only HMAC-SHA256 and HMAC-SHA512 verification supported");
        WAPI_RET_I32(WAPI_ERR_NOTSUP);
        return NULL;
    }

    if (!wapi_handle_valid(key_handle, WAPI_HTYPE_CRYPTO_KEY)) {
        wapi_set_error("Invalid key handle");
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    wapi_handle_entry_t* key_entry = wapi_handle_get(key_handle);

    const uint8_t* data = NULL;
    if (data_len > 0) {
        data = (const uint8_t*)wapi_wasm_ptr(data_ptr, data_len);
        if (!data) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    }

    const uint8_t* sig = (const uint8_t*)wapi_wasm_ptr(sig_ptr, sig_len);
    if (!sig && sig_len > 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    /* Compute HMAC and compare */
    uint8_t computed[64];
    size_t computed_len = 0;
    if (platform_hmac(algo,
                      key_entry->data.crypto_key.data,
                      key_entry->data.crypto_key.len,
                      data ? data : (const uint8_t*)"", data_len,
                      computed, &computed_len) != 0) {
        wapi_set_error("HMAC computation failed");
        WAPI_RET_I32(WAPI_ERR_IO);
        return NULL;
    }

    if (sig_len != (uint32_t)computed_len) {
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }

    /* Constant-time comparison to prevent timing attacks */
    uint8_t diff = 0;
    for (uint32_t i = 0; i < sig_len; i++) {
        diff |= sig[i] ^ computed[i];
    }

    WAPI_RET_I32(diff == 0 ? WAPI_OK : WAPI_ERR_INVAL);
    return NULL;
}

/* derive_key: (algo, base_key, salt_ptr, salt_len, info_ptr, info_len,
 *              iterations, key_len, derived_ptr, derived_len_ptr) -> i32 */
static wasm_trap_t* cb_derive_key(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    wapi_set_error("Key derivation not yet supported");
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

/* Per wapi_crypto.h: only `key_release` is a direct sync import.
 * Everything else (hash / hash_create / hash_update / hash_finish /
 * encrypt / decrypt / sign / verify / key_import_raw / key_generate /
 * key_generate_pair / derive_key) is submitted via wapi_io_t using
 * the WAPI_IO_OP_CRYPTO_* opcodes and dispatched in wapi_host_io.c.
 * The cb_* callbacks below are kept as scaffolding for the op_ctx_t
 * handlers the IO dispatch will eventually call — see NEXT_STEPS.md.
 */
void wapi_host_register_crypto(wasmtime_linker_t* linker) {
    WAPI_DEFINE_1_1(linker, "wapi_crypto", "key_release", cb_key_release);
    (void)cb_hash; (void)cb_hash_create; (void)cb_hash_update; (void)cb_hash_finish;
    (void)cb_key_import_raw; (void)cb_key_generate; (void)cb_key_generate_pair;
    (void)cb_encrypt; (void)cb_decrypt; (void)cb_sign; (void)cb_verify;
    (void)cb_derive_key;
}

/* ============================================================
 * Async I/O op handlers (WAPI_IO_OP_CRYPTO_HASH)
 * ============================================================
 * One-shot hash op: flags = wapi_hash_algo_t, addr/len = input bytes,
 * addr2/len2 = output digest buffer. result = digest bytes written.
 */

void wapi_host_crypto_hash_op(op_ctx_t* c) {
    uint32_t algo = c->flags;
    if (!hash_algo_valid(algo)) { c->result = WAPI_ERR_INVAL; return; }
    uint32_t dsize = hash_digest_size(algo);

    const uint8_t* data = c->len
        ? (const uint8_t*)wapi_wasm_ptr((uint32_t)c->addr, (uint32_t)c->len)
        : NULL;
    if (c->len > 0 && !data) { c->result = WAPI_ERR_INVAL; return; }

    if (c->len2 < dsize) { c->result = WAPI_ERR_OVERFLOW; return; }
    uint8_t* out = (uint8_t*)wapi_wasm_ptr((uint32_t)c->addr2, (uint32_t)c->len2);
    if (!out) { c->result = WAPI_ERR_INVAL; return; }

    if (platform_hash(algo, data ? data : (const uint8_t*)"", (size_t)c->len,
                      out, (size_t)c->len2) != 0) {
        c->result = WAPI_ERR_IO;
        return;
    }
    c->result = (int32_t)dsize;
}
