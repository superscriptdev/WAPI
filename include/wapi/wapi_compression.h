/**
 * WAPI - Compression
 * Version 1.0.0
 *
 * Compression and decompression (gzip, deflate, zstd, lz4).
 *
 * Operations go through the async I/O object as WAPI_IO_OP_COMPRESS_PROCESS.
 * Each submit processes one input buffer into one output buffer in full
 * and delivers a single WAPI_EVENT_IO_COMPLETION with the number of
 * bytes written. There is no streaming handle, no create/destroy, no
 * per-chunk write/read — that fits the browser's async
 * CompressionStream / DecompressionStream APIs naturally without
 * introducing a stateful sync contract that browsers cannot honour.
 *
 * Platform mapping:
 *   Web:     Compression Streams API (gzip/deflate), Wasm polyfill (zstd/lz4)
 *   Native:  zlib, libzstd, lz4
 *
 * Usage:
 *
 *   wapi_io_op_t op = {0};
 *   op.opcode    = WAPI_IO_OP_COMPRESS_PROCESS;
 *   op.addr      = (uintptr_t)input_bytes;
 *   op.len       = input_len;
 *   op.addr2     = (uintptr_t)output_buffer;
 *   op.len2      = output_capacity;
 *   op.flags     = WAPI_COMPRESS_GZIP;
 *   op.flags2    = WAPI_COMPRESS_DECOMPRESS;
 *   op.user_data = my_correlation_token;
 *   io->submit(io->impl, &op, 1);
 *
 *   // Later, a WAPI_EVENT_IO_COMPLETION arrives where
 *   //   event.io.user_data == my_correlation_token
 *   //   event.io.result    == bytes_written (or negative error)
 */

#ifndef WAPI_COMPRESSION_H
#define WAPI_COMPRESSION_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Compression Algorithms (flags field on WAPI_IO_OP_COMPRESS_PROCESS)
 * ============================================================ */

typedef enum wapi_compress_algo_t {
    WAPI_COMPRESS_GZIP        = 0,
    WAPI_COMPRESS_DEFLATE     = 1,
    WAPI_COMPRESS_DEFLATE_RAW = 2,
    WAPI_COMPRESS_ZSTD        = 3,
    WAPI_COMPRESS_LZ4         = 4,
    WAPI_COMPRESS_FORCE32     = 0x7FFFFFFF
} wapi_compress_algo_t;

/* ============================================================
 * Mode (flags2 field on WAPI_IO_OP_COMPRESS_PROCESS)
 * ============================================================ */

typedef enum wapi_compress_mode_t {
    WAPI_COMPRESS_COMPRESS     = 0,
    WAPI_COMPRESS_DECOMPRESS   = 1,
    WAPI_COMPRESS_FORCE32_MODE = 0x7FFFFFFF
} wapi_compress_mode_t;

/* ============================================================
 * Inline helper — fills a wapi_io_op_t for a COMPRESS_PROCESS
 * op and submits it through the supplied IO vtable. The caller
 * keeps ownership of input and output; both must remain valid
 * until the matching WAPI_EVENT_IO_COMPLETION arrives.
 *
 * Returns the number of ops the backend accepted (0 or 1).
 * ============================================================ */

static inline int32_t wapi_compression_process(const wapi_io_t*     io,
                                               const void*          input,
                                               uint32_t             input_len,
                                               void*                output,
                                               uint32_t             output_capacity,
                                               wapi_compress_algo_t algo,
                                               wapi_compress_mode_t mode,
                                               uint64_t             user_data)
{
    wapi_io_op_t op;
    for (unsigned i = 0; i < sizeof(op); ++i) ((uint8_t*)&op)[i] = 0;
    op.opcode    = WAPI_IO_OP_COMPRESS_PROCESS;
    op.flags     = (uint32_t)algo;
    op.flags2    = (uint32_t)mode;
    op.addr      = (uint64_t)(uintptr_t)input;
    op.len       = (uint64_t)input_len;
    op.addr2     = (uint64_t)(uintptr_t)output;
    op.len2      = (uint64_t)output_capacity;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

#ifdef __cplusplus
}
#endif

#endif /* WAPI_COMPRESSION_H */
