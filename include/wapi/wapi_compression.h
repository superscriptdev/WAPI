/**
 * WAPI - Compression
 * Version 1.0.0
 *
 * Streaming compression and decompression (gzip, deflate, zstd, lz4).
 *
 * Two modes: streaming (create → write → read → finish → drain) and
 * one-shot (single call for small buffers). Synchronous -- output is
 * available immediately after each write, no event queue involved.
 *
 * Platform mapping:
 *   Web:     Compression Streams API (gzip/deflate), Wasm polyfill (zstd/lz4)
 *   All:     zlib, libzstd, lz4 (native libraries)
 *
 * Import module: "wapi_compress"
 */

#ifndef WAPI_COMPRESSION_H
#define WAPI_COMPRESSION_H

#include "wapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Compression Algorithms
 * ============================================================ */

typedef enum wapi_compress_algo_t {
    WAPI_COMPRESS_GZIP        = 0,
    WAPI_COMPRESS_DEFLATE     = 1,
    WAPI_COMPRESS_DEFLATE_RAW = 2,
    WAPI_COMPRESS_ZSTD        = 3,
    WAPI_COMPRESS_LZ4         = 4,
    WAPI_COMPRESS_FORCE32     = 0x7FFFFFFF
} wapi_compress_algo_t;

typedef enum wapi_compress_mode_t {
    WAPI_COMPRESS_COMPRESS     = 0,
    WAPI_COMPRESS_DECOMPRESS   = 1,
    WAPI_COMPRESS_FORCE32_MODE = 0x7FFFFFFF
} wapi_compress_mode_t;

/* ============================================================
 * Streaming Compression
 * ============================================================ */

/**
 * Create a streaming compressor or decompressor.
 *
 * @param algo    Compression algorithm.
 * @param mode    Compress or decompress.
 * @param level   Compression level (algorithm-specific, 0 = default).
 *                Ignored for decompression.
 * @param stream  [out] Stream handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_compress, create)
wapi_result_t wapi_compress_create(wapi_compress_algo_t algo,
                                   wapi_compress_mode_t mode, int32_t level,
                                   wapi_handle_t* stream);

/**
 * Destroy a compression stream and release resources.
 *
 * @param stream  Stream handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_compress, destroy)
wapi_result_t wapi_compress_destroy(wapi_handle_t stream);

/**
 * Feed input data into the stream. May produce partial output
 * that can be retrieved with wapi_compress_read().
 *
 * @param stream  Stream handle.
 * @param data    Input data.
 * @param len     Input data length in bytes.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_compress, write)
wapi_result_t wapi_compress_write(wapi_handle_t stream, const void* data,
                                  wapi_size_t len);

/**
 * Read available output from the stream.
 *
 * @param stream      Stream handle.
 * @param buf         Buffer to receive output data.
 * @param buf_len     Buffer capacity.
 * @param bytes_read  [out] Actual bytes read.
 * @return WAPI_OK on success, WAPI_ERR_AGAIN if no data available.
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_compress, read)
wapi_result_t wapi_compress_read(wapi_handle_t stream, void* buf,
                                 wapi_size_t buf_len, wapi_size_t* bytes_read);

/**
 * Signal end of input and flush remaining output.
 * After finishing, call wapi_compress_read() until WAPI_ERR_AGAIN
 * to drain all remaining output.
 *
 * @param stream  Stream handle.
 * @return WAPI_OK on success.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi_compress, finish)
wapi_result_t wapi_compress_finish(wapi_handle_t stream);

/* ============================================================
 * One-Shot Convenience
 * ============================================================ */

/**
 * Compress or decompress data in a single call.
 *
 * @param algo         Compression algorithm.
 * @param mode         Compress or decompress.
 * @param in           Input data.
 * @param in_len       Input data length.
 * @param out          Output buffer.
 * @param out_len      Output buffer capacity.
 * @param out_written  [out] Actual bytes written to output.
 * @return WAPI_OK on success, WAPI_ERR_OVERFLOW if output buffer too small.
 *
 * Wasm signature: (i32, i32, i32, i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi_compress, oneshot)
wapi_result_t wapi_compress_oneshot(wapi_compress_algo_t algo,
                                    wapi_compress_mode_t mode,
                                    const void* in, wapi_size_t in_len,
                                    void* out, wapi_size_t out_len,
                                    wapi_size_t* out_written);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_COMPRESSION_H */
