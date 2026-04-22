/**
 * WAPI Desktop Runtime - Shared Host Types
 *
 * Handle table, Wasm memory helpers, and runtime state used by
 * all wapi_host_*.c files. Windowing / input / audio / clock /
 * clipboard go through wapi_plat.h (platform abstraction) — no
 * SDL, no framework: backends call OS APIs directly.
 */

#ifndef WAPI_HOST_H
#define WAPI_HOST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Wasmtime C API */
#include <wasmtime.h>

/* wgpu-native (WebGPU) */
#include <webgpu.h>

/* Platform abstraction — no SDL here. */
#include "wapi_plat.h"

/* WAPI ABI headers */
#include "wapi/wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Handle Table
 * ============================================================
 * int32_t > 0. 0 is invalid. 1-3 are stdin/stdout/stderr.
 * 4+ are preopen dirs, then dynamically allocated.
 */

#define WAPI_MAX_HANDLES 4096

typedef enum wapi_handle_type_t {
    WAPI_HTYPE_FREE = 0,
    WAPI_HTYPE_FILE,
    WAPI_HTYPE_DIRECTORY,
    WAPI_HTYPE_SURFACE,
    WAPI_HTYPE_GPU_INSTANCE,
    WAPI_HTYPE_GPU_ADAPTER,
    WAPI_HTYPE_GPU_DEVICE,
    WAPI_HTYPE_GPU_QUEUE,
    WAPI_HTYPE_GPU_SURFACE,
    WAPI_HTYPE_GPU_TEXTURE,
    WAPI_HTYPE_GPU_TEXTURE_VIEW,
    WAPI_HTYPE_GPU_BUFFER,
    WAPI_HTYPE_GPU_SAMPLER,
    WAPI_HTYPE_GPU_BIND_GROUP,
    WAPI_HTYPE_GPU_BIND_GROUP_LAYOUT,
    WAPI_HTYPE_GPU_PIPELINE_LAYOUT,
    WAPI_HTYPE_GPU_SHADER_MODULE,
    WAPI_HTYPE_GPU_RENDER_PIPELINE,
    WAPI_HTYPE_GPU_COMMAND_ENCODER,
    WAPI_HTYPE_GPU_COMMAND_BUFFER,
    WAPI_HTYPE_GPU_RENDER_PASS,
    WAPI_HTYPE_AUDIO_DEVICE,
    WAPI_HTYPE_AUDIO_STREAM,
    WAPI_HTYPE_PREOPEN_DIR,
    WAPI_HTYPE_IO_QUEUE,
    WAPI_HTYPE_NET_CONN,
    WAPI_HTYPE_NET_LISTENER,
    WAPI_HTYPE_NET_STREAM,
    WAPI_HTYPE_CONTENT,
    WAPI_HTYPE_CRYPTO_HASH_CTX,
    WAPI_HTYPE_CRYPTO_KEY,
    WAPI_HTYPE_VIDEO,
    WAPI_HTYPE_MODULE,
    WAPI_HTYPE_LEASE,
} wapi_handle_type_t;

/* ---- I/O Queue ---- */
#define WAPI_IO_QUEUE_MAX_OPS 256

typedef struct wapi_io_completion_entry_t {
    uint64_t user_data;
    int32_t  result;
    uint32_t flags;
} wapi_io_completion_entry_t;

typedef struct wapi_io_queue_t {
    wapi_io_completion_entry_t completions[WAPI_IO_QUEUE_MAX_OPS];
    int head;
    int tail;
    int count;
    int capacity;
    struct {
        uint64_t user_data;
        uint64_t deadline_ns;
        bool     active;
    } timeouts[64];
    int timeout_count;
} wapi_io_queue_t;

/* ---- Networking ---- */
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET wapi_socket_t;
#define WAPI_INVALID_SOCKET INVALID_SOCKET
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
typedef int wapi_socket_t;
#define WAPI_INVALID_SOCKET (-1)
#endif

typedef struct wapi_net_conn_t {
    wapi_socket_t sock;
    uint32_t      transport;
    bool          connected;
    bool          nonblocking;
} wapi_net_conn_t;

/* ---- Crypto ---- */
typedef struct wapi_crypto_hash_ctx_t {
    uint32_t algo;
    uint8_t  state[256];
    size_t   state_size;
} wapi_crypto_hash_ctx_t;

typedef struct wapi_crypto_key_t {
    uint8_t  data[512];
    size_t   len;
    uint32_t usages;
} wapi_crypto_key_t;

/* ---- Memory allocator state ---- */
typedef struct wapi_mem_alloc_t {
    uint32_t offset;
    uint32_t size;
} wapi_mem_alloc_t;

#define WAPI_MEM_MAX_ALLOCS 4096

/* ---- Module linking (wapi_module) ---- */

#define WAPI_MODULE_MAX_FUNCS 64

typedef struct wapi_module_func_slot_t {
    wasmtime_func_t fn;
    bool            in_use;
} wapi_module_func_slot_t;

typedef struct wapi_module_slot_t {
    wasmtime_module_t*       module;
    wasmtime_instance_t      instance;
    wasmtime_memory_t        memory;      /* child's memory 0 */
    bool                     memory_valid;
    uint8_t                  hash[32];    /* SHA-256 of wasm bytes */
    wapi_module_func_slot_t  funcs[WAPI_MODULE_MAX_FUNCS];
} wapi_module_slot_t;

/* ---- Shared memory pool (wapi_module memory 1, host-owned) ---- */

#define WAPI_SHARED_MEM_CAPACITY (16u * 1024u * 1024u)  /* 16 MiB */
#define WAPI_SHARED_MAX_ALLOCS   1024

typedef struct wapi_shared_alloc_t {
    uint64_t offset;       /* byte offset into g_rt.shared_mem (0 = empty slot) */
    uint64_t size;         /* allocation size in bytes */
    bool     in_use;
} wapi_shared_alloc_t;

typedef struct wapi_shared_mem_t {
    uint8_t*             bytes;
    uint64_t             capacity;
    uint64_t             bump;         /* high-water mark */
    wapi_shared_alloc_t  allocs[WAPI_SHARED_MAX_ALLOCS];
} wapi_shared_mem_t;

/* Hash → filesystem path cache, seeded from CLI --module flags. */
#define WAPI_MODULE_CACHE_MAX 16

typedef struct wapi_module_cache_entry_t {
    uint8_t  hash[32];
    char     path[512];
    bool     in_use;
} wapi_module_cache_entry_t;

/* ---- Handle entry ---- */
typedef struct wapi_handle_entry_t {
    wapi_handle_type_t type;
    union {
        FILE*                      file;
        struct { char path[512]; } dir;
        wapi_plat_window_t*        window;
        WGPUInstance               gpu_instance;
        WGPUAdapter                gpu_adapter;
        WGPUDevice                 gpu_device;
        WGPUQueue                  gpu_queue;
        WGPUSurface                gpu_surface;
        WGPUTexture                gpu_texture;
        WGPUTextureView            gpu_texture_view;
        WGPUBuffer                 gpu_buffer;
        WGPUSampler                gpu_sampler;
        WGPUBindGroup              gpu_bind_group;
        WGPUBindGroupLayout        gpu_bind_group_layout;
        WGPUPipelineLayout         gpu_pipeline_layout;
        WGPUShaderModule           gpu_shader_module;
        WGPURenderPipeline         gpu_render_pipeline;
        WGPUCommandEncoder         gpu_command_encoder;
        WGPUCommandBuffer          gpu_command_buffer;
        WGPURenderPassEncoder      gpu_render_pass;
        wapi_module_slot_t*        module_slot;
        wapi_plat_audio_device_t*  audio_device;
        wapi_plat_audio_stream_t*  audio_stream;
        wapi_io_queue_t*           io_queue;
        wapi_net_conn_t            net_conn;
        wapi_crypto_hash_ctx_t     crypto_hash;
        wapi_crypto_key_t          crypto_key;
    } data;
} wapi_handle_entry_t;

/* ============================================================
 * Event Queue
 * ============================================================ */

#define WAPI_EVENT_QUEUE_SIZE 256

typedef struct wapi_host_event_t {
    uint8_t data[128];
} wapi_host_event_t;

typedef struct wapi_event_queue_t {
    wapi_host_event_t events[WAPI_EVENT_QUEUE_SIZE];
    int               head;
    int               tail;
    int               count;
} wapi_event_queue_t;

/* ============================================================
 * Pre-opened Directory
 * ============================================================ */

#define WAPI_MAX_PREOPENS 32

typedef struct wapi_preopen_t {
    char    guest_path[256];
    char    host_path[512];
    int32_t handle;
} wapi_preopen_t;

/* ============================================================
 * Runtime State
 * ============================================================ */

typedef struct wapi_runtime_t {
    /* Wasmtime */
    wasm_engine_t*      engine;
    wasmtime_store_t*   store;
    wasmtime_context_t* context;
    wasmtime_module_t*  module;
    wasmtime_instance_t instance;
    wasmtime_memory_t   memory;
    bool                memory_valid;

    /* Handle table */
    wapi_handle_entry_t handles[WAPI_MAX_HANDLES];
    int32_t             next_handle;

    /* Event queue (host side — carries I/O completions + translated platform events) */
    wapi_event_queue_t  event_queue;

    /* Guest indirect function table (for invoking guest-supplied callbacks) */
    wasmtime_table_t    indirect_table;
    bool                indirect_table_valid;

    /* Preopens */
    wapi_preopen_t preopens[WAPI_MAX_PREOPENS];
    int            preopen_count;

    /* App args */
    int    app_argc;
    char** app_argv;

    /* Host memory allocator for wasm heap */
    wapi_mem_alloc_t mem_allocs[WAPI_MEM_MAX_ALLOCS];
    int              mem_alloc_count;
    uint32_t         mem_heap_top;
    bool             mem_initialized;

    /* KV storage */
    char kv_storage_path[512];

    /* Module linking: host-owned shared memory + hash-path cache */
    wapi_shared_mem_t         shared_mem;
    wapi_module_cache_entry_t module_cache[WAPI_MODULE_CACHE_MAX];
    wasmtime_linker_t*        linker;     /* kept live for child instantiation */

    /* Net init state */
    bool net_initialized;

    /* Last error message */
    char last_error[512];

    /* Running state */
    bool running;
    bool has_wapi_frame;
} wapi_runtime_t;

extern wapi_runtime_t g_rt;

/* ============================================================
 * I/O Dispatch Context
 * ============================================================
 * Shared between wapi_host_io.c (dispatch table) and per-capability
 * modules that implement async opcode handlers.  Field layout is
 * pinned — the IO bridge fills it in from the 80-byte wapi_io_op_t
 * read out of wasm memory, handlers write `result` / `cqe_flags` /
 * `payload` / `inline_payload` / `suppress_completion`.
 */
typedef struct op_ctx_t {
    /* Op fields, already read from wasm memory */
    uint32_t opcode;
    uint32_t flags;
    int32_t  fd;
    uint32_t flags2;
    uint64_t offset;
    uint64_t addr;
    uint64_t len;
    uint64_t addr2;
    uint64_t len2;
    uint64_t user_data;
    uint64_t result_ptr;

    /* Completion fields — handler writes these */
    int32_t  result;
    uint32_t cqe_flags;
    uint8_t  payload[96];
    bool     inline_payload;
    bool     suppress_completion; /* fire-and-forget (LOG, TIMEOUT) */
} op_ctx_t;

/* ============================================================
 * Handle Table Operations
 * ============================================================ */

static inline int32_t wapi_handle_alloc(wapi_handle_type_t type) {
    for (int i = g_rt.next_handle; i < WAPI_MAX_HANDLES; i++) {
        if (g_rt.handles[i].type == WAPI_HTYPE_FREE) {
            g_rt.handles[i].type = type;
            memset(&g_rt.handles[i].data, 0, sizeof(g_rt.handles[i].data));
            g_rt.next_handle = i + 1;
            return (int32_t)i;
        }
    }
    for (int i = 4; i < g_rt.next_handle; i++) {
        if (g_rt.handles[i].type == WAPI_HTYPE_FREE) {
            g_rt.handles[i].type = type;
            memset(&g_rt.handles[i].data, 0, sizeof(g_rt.handles[i].data));
            g_rt.next_handle = i + 1;
            return (int32_t)i;
        }
    }
    return 0;
}

static inline bool wapi_handle_valid(int32_t h, wapi_handle_type_t expected) {
    if (h <= 0 || h >= WAPI_MAX_HANDLES) return false;
    if (g_rt.handles[h].type == WAPI_HTYPE_FREE) return false;
    if (expected != WAPI_HTYPE_FREE && g_rt.handles[h].type != expected) return false;
    return true;
}

static inline bool wapi_handle_valid_any(int32_t h) {
    if (h <= 0 || h >= WAPI_MAX_HANDLES) return false;
    return g_rt.handles[h].type != WAPI_HTYPE_FREE;
}

static inline void wapi_handle_free(int32_t h) {
    if (h > 0 && h < WAPI_MAX_HANDLES) {
        g_rt.handles[h].type = WAPI_HTYPE_FREE;
        memset(&g_rt.handles[h].data, 0, sizeof(g_rt.handles[h].data));
    }
}

static inline wapi_handle_entry_t* wapi_handle_get(int32_t h) {
    if (h > 0 && h < WAPI_MAX_HANDLES && g_rt.handles[h].type != WAPI_HTYPE_FREE) {
        return &g_rt.handles[h];
    }
    return NULL;
}

/* Map a platform window id to the WAPI surface handle. O(N) over
 * handle table; N is small (<= 4096) and surface creation is rare. */
static inline int32_t wapi_surface_handle_from_window_id(uint32_t window_id) {
    if (window_id == 0) return 0;
    for (int i = 1; i < WAPI_MAX_HANDLES; i++) {
        if (g_rt.handles[i].type == WAPI_HTYPE_SURFACE &&
            g_rt.handles[i].data.window != NULL &&
            wapi_plat_window_id(g_rt.handles[i].data.window) == window_id) {
            return (int32_t)i;
        }
    }
    return 0;
}

/* ============================================================
 * Wasm Memory Helpers
 * ============================================================ */

static inline uint8_t* wapi_wasm_memory_base(void) {
    if (!g_rt.memory_valid) return NULL;
    return wasmtime_memory_data(g_rt.context, &g_rt.memory);
}

static inline size_t wapi_wasm_memory_size(void) {
    if (!g_rt.memory_valid) return 0;
    return wasmtime_memory_data_size(g_rt.context, &g_rt.memory);
}

static inline bool wapi_wasm_validate_ptr(uint32_t ptr, uint32_t len) {
    if (len == 0) return true;
    size_t mem_size = wapi_wasm_memory_size();
    return (ptr < mem_size && (uint64_t)ptr + len <= mem_size);
}

static inline void* wapi_wasm_ptr(uint32_t guest_ptr, uint32_t len) {
    if (!wapi_wasm_validate_ptr(guest_ptr, len)) return NULL;
    return wapi_wasm_memory_base() + guest_ptr;
}

static inline bool wapi_wasm_read_i32(uint32_t ptr, int32_t* out) {
    void* host = wapi_wasm_ptr(ptr, 4);
    if (!host) return false;
    memcpy(out, host, 4);
    return true;
}

static inline bool wapi_wasm_write_i32(uint32_t ptr, int32_t val) {
    void* host = wapi_wasm_ptr(ptr, 4);
    if (!host) return false;
    memcpy(host, &val, 4);
    return true;
}

static inline bool wapi_wasm_write_u32(uint32_t ptr, uint32_t val) {
    void* host = wapi_wasm_ptr(ptr, 4);
    if (!host) return false;
    memcpy(host, &val, 4);
    return true;
}

static inline bool wapi_wasm_write_u64(uint32_t ptr, uint64_t val) {
    void* host = wapi_wasm_ptr(ptr, 8);
    if (!host) return false;
    memcpy(host, &val, 8);
    return true;
}

static inline bool wapi_wasm_write_f32(uint32_t ptr, float val) {
    void* host = wapi_wasm_ptr(ptr, 4);
    if (!host) return false;
    memcpy(host, &val, 4);
    return true;
}

static inline const char* wapi_wasm_read_string(uint32_t ptr, uint32_t len) {
    if (len == 0) return "";
    void* host = wapi_wasm_ptr(ptr, len);
    return (const char*)host;
}

static inline bool wapi_wasm_write_bytes(uint32_t ptr, const void* src, uint32_t len) {
    if (len == 0) return true;
    void* host = wapi_wasm_ptr(ptr, len);
    if (!host) return false;
    memcpy(host, src, len);
    return true;
}

/* ============================================================
 * Event Queue Operations
 * ============================================================ */

static inline bool wapi_event_queue_push(const wapi_host_event_t* ev) {
    wapi_event_queue_t* q = &g_rt.event_queue;
    if (q->count >= WAPI_EVENT_QUEUE_SIZE) return false;
    q->events[q->tail] = *ev;
    q->tail = (q->tail + 1) % WAPI_EVENT_QUEUE_SIZE;
    q->count++;
    return true;
}

static inline bool wapi_event_queue_pop(wapi_host_event_t* ev) {
    wapi_event_queue_t* q = &g_rt.event_queue;
    if (q->count == 0) return false;
    *ev = q->events[q->head];
    q->head = (q->head + 1) % WAPI_EVENT_QUEUE_SIZE;
    q->count--;
    return true;
}

static inline void wapi_event_queue_flush(uint32_t event_type) {
    wapi_event_queue_t* q = &g_rt.event_queue;
    if (event_type == 0) {
        q->head = q->tail = q->count = 0;
        return;
    }
    wapi_host_event_t temp[WAPI_EVENT_QUEUE_SIZE];
    int new_count = 0;
    while (q->count > 0) {
        wapi_host_event_t ev;
        wapi_event_queue_pop(&ev);
        uint32_t type;
        memcpy(&type, ev.data, 4);
        if (type != event_type) {
            temp[new_count++] = ev;
        }
    }
    for (int i = 0; i < new_count; i++) {
        wapi_event_queue_push(&temp[i]);
    }
}

/* ============================================================
 * Error Helpers
 * ============================================================ */

static inline void wapi_set_error(const char* msg) {
    if (msg) {
        size_t len = strlen(msg);
        if (len >= sizeof(g_rt.last_error)) len = sizeof(g_rt.last_error) - 1;
        memcpy(g_rt.last_error, msg, len);
        g_rt.last_error[len] = '\0';
    } else {
        g_rt.last_error[0] = '\0';
    }
}

/* ============================================================
 * Host Function Registration Helper
 * ============================================================ */

static inline wasm_functype_t* wapi_functype(int np, wasm_valkind_t* ptypes,
                                             int nr, wasm_valkind_t* rtypes) {
    wasm_valtype_vec_t params, results;
    if (np > 0) {
        wasm_valtype_t* pv[16];
        for (int i = 0; i < np; i++) pv[i] = wasm_valtype_new(ptypes[i]);
        wasm_valtype_vec_new(&params, np, pv);
    } else {
        wasm_valtype_vec_new_empty(&params);
    }
    if (nr > 0) {
        wasm_valtype_t* rv[4];
        for (int i = 0; i < nr; i++) rv[i] = wasm_valtype_new(rtypes[i]);
        wasm_valtype_vec_new(&results, nr, rv);
    } else {
        wasm_valtype_vec_new_empty(&results);
    }
    return wasm_functype_new(&params, &results);
}

#define WAPI_ARG_I32(n) ((int32_t)args[n].of.i32)
#define WAPI_ARG_I64(n) ((int64_t)args[n].of.i64)
#define WAPI_ARG_U32(n) ((uint32_t)args[n].of.i32)
#define WAPI_ARG_U64(n) ((uint64_t)args[n].of.i64)
#define WAPI_ARG_F32(n) (args[n].of.f32)

#define WAPI_RET_I32(val) do { results[0].kind = WASMTIME_I32; results[0].of.i32 = (int32_t)(val); } while(0)
#define WAPI_RET_I64(val) do { results[0].kind = WASMTIME_I64; results[0].of.i64 = (int64_t)(val); } while(0)

static inline void wapi_linker_define(wasmtime_linker_t* linker,
                                      const char* module, const char* name,
                                      wasmtime_func_callback_t cb,
                                      int np, wasm_valkind_t ptypes[],
                                      int nr, wasm_valkind_t rtypes[]) {
    wasm_functype_t* ft = wapi_functype(np, ptypes, nr, rtypes);
    wasmtime_error_t* err = wasmtime_linker_define_func(
        linker, module, strlen(module), name, strlen(name),
        ft, cb, NULL, NULL);
    wasm_functype_delete(ft);
    if (err) {
        wasm_message_t msg;
        wasmtime_error_message(err, &msg);
        fprintf(stderr, "Link error %s::%s: %.*s\n",
                module, name, (int)msg.size, msg.data);
        wasm_byte_vec_delete(&msg);
        wasmtime_error_delete(err);
    }
}

#define WAPI_DEFINE_0_0(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 0, NULL, 0, NULL)
#define WAPI_DEFINE_0_1(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 0, NULL, 1, (wasm_valkind_t[]){WASM_I32})
#define WAPI_DEFINE_1_0(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 1, (wasm_valkind_t[]){WASM_I32}, 0, NULL)
#define WAPI_DEFINE_1_1(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 1, (wasm_valkind_t[]){WASM_I32}, 1, (wasm_valkind_t[]){WASM_I32})
#define WAPI_DEFINE_2_0(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 2, (wasm_valkind_t[]){WASM_I32,WASM_I32}, 0, NULL)
#define WAPI_DEFINE_2_1(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 2, (wasm_valkind_t[]){WASM_I32,WASM_I32}, 1, (wasm_valkind_t[]){WASM_I32})
#define WAPI_DEFINE_3_0(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 3, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I32}, 0, NULL)
#define WAPI_DEFINE_3_1(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 3, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I32}, 1, (wasm_valkind_t[]){WASM_I32})
#define WAPI_DEFINE_3_I64(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 3, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I32}, 1, (wasm_valkind_t[]){WASM_I64})
#define WAPI_DEFINE_4_1(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 4, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I32,WASM_I32}, 1, (wasm_valkind_t[]){WASM_I32})
#define WAPI_DEFINE_5_0(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 5, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I32,WASM_I32,WASM_I32}, 0, NULL)
#define WAPI_DEFINE_5_1(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 5, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I32,WASM_I32,WASM_I32}, 1, (wasm_valkind_t[]){WASM_I32})
#define WAPI_DEFINE_6_0(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 6, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I32,WASM_I32,WASM_I32,WASM_I32}, 0, NULL)
#define WAPI_DEFINE_6_1(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 6, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I32,WASM_I32,WASM_I32,WASM_I32}, 1, (wasm_valkind_t[]){WASM_I32})
/* 6 i32 params, void return — alias for clarity when reading sprite-path code */
#define WAPI_DEFINE_6_1_0(linker, mod, name, cb) WAPI_DEFINE_6_0(linker, mod, name, cb)

/* ============================================================
 * Host Registration Forward Declarations
 * ============================================================ */

void wapi_host_register_core(wasmtime_linker_t* linker);
void wapi_host_register_env(wasmtime_linker_t* linker);
void wapi_host_register_random(wasmtime_linker_t* linker);
void wapi_host_register_clock(wasmtime_linker_t* linker);
void wapi_host_register_surface(wasmtime_linker_t* linker);
void wapi_host_register_window(wasmtime_linker_t* linker);
void wapi_host_register_input(wasmtime_linker_t* linker);
void wapi_host_register_audio(wasmtime_linker_t* linker);
void wapi_host_register_wgpu(wasmtime_linker_t* linker);
void wapi_host_register_fs(wasmtime_linker_t* linker);
void wapi_host_register_memory(wasmtime_linker_t* linker);
void wapi_host_register_io(wasmtime_linker_t* linker);
void wapi_host_register_net(wasmtime_linker_t* linker);
void wapi_host_register_transfer(wasmtime_linker_t* linker);
void wapi_host_register_seat(wasmtime_linker_t* linker);
void wapi_host_register_kv(wasmtime_linker_t* linker);
void wapi_host_register_crypto(wasmtime_linker_t* linker);
void wapi_host_register_text(wasmtime_linker_t* linker);
void wapi_host_register_font(wasmtime_linker_t* linker);
void wapi_host_register_video(wasmtime_linker_t* linker);
void wapi_host_register_module(wasmtime_linker_t* linker);
void wapi_host_register_notifications(wasmtime_linker_t* linker);
void wapi_host_register_permissions(wasmtime_linker_t* linker);

/* CLI-seeded hash→path cache (wapi_host_module.c). `spec` is
 * "<64-char lowercase sha256 hex>=<filesystem path>". Returns true on
 * success. */
bool wapi_host_module_cache_add(const char* spec);

/* Translate one platform event into WAPI event format and push
 * onto the host event queue. Defined in wapi_host_input.c. */
void wapi_host_translate_event(const wapi_plat_event_t* pe);

/* Drain all pending platform events, translating each onto the
 * host event queue. Called from io.poll / io.wait. */
void wapi_host_pump_platform_events(void);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_HOST_H */
