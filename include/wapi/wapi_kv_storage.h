/**
 * WAPI - Key-Value Storage Capability
 * Version 1.0.0
 *
 * Maps to: localStorage (Web), Keychain (macOS/iOS),
 *          SharedPreferences (Android), Registry (Windows)
 *
 * Persistent, sandboxed key-value store per application.
 * Values survive process restarts. Not suitable for large data
 * (use filesystem for that).
 *
 * Import module: "wapi_kv"
 *
 * Query availability with wapi_capability_supported("wapi.kv_storage", 13)
 */

#ifndef WAPI_KV_STORAGE_H
#define WAPI_KV_STORAGE_H

#include "wapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get a value by key.
 *
 * @param key       Key string (UTF-8).
 * @param key_len   Key length.
 * @param buf       Buffer for value.
 * @param buf_len   Buffer capacity.
 * @param val_len   [out] Actual value length.
 * @return WAPI_OK on success, WAPI_ERR_NOENT if key not found.
 */
WAPI_IMPORT(wapi_kv, get)
wapi_result_t wapi_kv_get(const char* key, wapi_size_t key_len,
                       void* buf, wapi_size_t buf_len, wapi_size_t* val_len);

/**
 * Set a value for a key.
 *
 * @param key       Key string (UTF-8).
 * @param key_len   Key length.
 * @param value     Value data.
 * @param val_len   Value length.
 * @return WAPI_OK on success.
 */
WAPI_IMPORT(wapi_kv, set)
wapi_result_t wapi_kv_set(const char* key, wapi_size_t key_len,
                       const void* value, wapi_size_t val_len);

/**
 * Delete a key.
 */
WAPI_IMPORT(wapi_kv, delete)
wapi_result_t wapi_kv_delete(const char* key, wapi_size_t key_len);

/**
 * Check if a key exists.
 */
WAPI_IMPORT(wapi_kv, has)
wapi_bool_t wapi_kv_has(const char* key, wapi_size_t key_len);

/**
 * Clear all keys.
 */
WAPI_IMPORT(wapi_kv, clear)
wapi_result_t wapi_kv_clear(void);

/**
 * Get the number of stored keys.
 */
WAPI_IMPORT(wapi_kv, count)
int32_t wapi_kv_count(void);

/**
 * Get a key by index (for enumeration).
 *
 * @param index     Key index (0-based).
 * @param buf       Buffer for key name.
 * @param buf_len   Buffer capacity.
 * @param key_len   [out] Actual key length.
 */
WAPI_IMPORT(wapi_kv, key_at)
wapi_result_t wapi_kv_key_at(int32_t index, char* buf, wapi_size_t buf_len,
                          wapi_size_t* key_len);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_KV_STORAGE_H */
