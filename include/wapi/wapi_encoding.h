/**
 * WAPI - Text Encoding
 * Version 1.0.0
 *
 * Text encoding conversion between character sets.
 * Maps to: TextEncoder/TextDecoder (Web), iconv (POSIX),
 *          MultiByteToWideChar (Windows)
 *
 * Import module: "wapi_encode"
 */

#ifndef WAPI_ENCODING_H
#define WAPI_ENCODING_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Text Encodings
 * ============================================================ */

typedef enum wapi_text_encoding_t {
    WAPI_ENCODING_UTF8         = 0,
    WAPI_ENCODING_UTF16_LE     = 1,
    WAPI_ENCODING_UTF16_BE     = 2,
    WAPI_ENCODING_ISO_8859_1   = 3,
    WAPI_ENCODING_WINDOWS_1252 = 4,
    WAPI_ENCODING_SHIFT_JIS    = 5,
    WAPI_ENCODING_EUC_JP       = 6,
    WAPI_ENCODING_GB2312       = 7,
    WAPI_ENCODING_BIG5         = 8,
    WAPI_ENCODING_EUC_KR       = 9,
    WAPI_ENCODING_FORCE32      = 0x7FFFFFFF
} wapi_text_encoding_t;

/* ============================================================
 * Encoding Conversion
 * ============================================================ */

/**
 * Convert text from one encoding to another.
 *
 * @param from           Source encoding.
 * @param to             Destination encoding.
 * @param input          Input data.
 * @param input_len      Input data length in bytes.
 * @param output         Output buffer.
 * @param output_len     Output buffer capacity in bytes.
 * @param bytes_written  [out] Actual bytes written to output.
 * @return WAPI_OK on success, WAPI_ERR_OVERFLOW if output buffer too small.
 *
 * Wasm signature: (i32, i32, i32, i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_encode, convert)
wapi_result_t wapi_encode_convert(wapi_text_encoding_t from,
                                  wapi_text_encoding_t to,
                                  const void* input, wapi_size_t input_len,
                                  void* output, wapi_size_t output_len,
                                  wapi_size_t* bytes_written);

/**
 * Query the required output buffer size for an encoding conversion.
 *
 * @param from          Source encoding.
 * @param to            Destination encoding.
 * @param input         Input data.
 * @param input_len     Input data length in bytes.
 * @param required_len  [out] Required output buffer size in bytes.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_encode, query_size)
wapi_result_t wapi_encode_query_size(wapi_text_encoding_t from,
                                     wapi_text_encoding_t to,
                                     const void* input, wapi_size_t input_len,
                                     wapi_size_t* required_len);

/**
 * Detect the encoding of a byte sequence (best-effort heuristic).
 *
 * @param data        Input data to analyze.
 * @param len         Input data length in bytes.
 * @param encoding    [out] Detected encoding.
 * @param confidence  [out] Confidence score (0.0 to 1.0).
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_encode, detect)
wapi_result_t wapi_encode_detect(const void* data, wapi_size_t len,
                                 wapi_text_encoding_t* encoding,
                                 float* confidence);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_ENCODING_H */
