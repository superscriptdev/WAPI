/**
 * WAPI Desktop Runtime - Shared Host Types
 *
 * Global handle table, Wasm memory helpers, and shared state
 * used by all wapi_host_*.c implementation files.
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

/* SDL3 (windowing, input, audio -- no SDL GPU) */
#include <SDL3/SDL.h>

/* wgpu-native (WebGPU) */
#include <webgpu.h>

/* WAPI ABI headers (for constants and type sizes only) */
#include "wapi/wapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Handle Table
 * ============================================================
 * Maps wapi_handle_t (int32_t > 0) to host-side objects.
 * Handle 0 is invalid. Handles 1-3 are stdin/stdout/stderr.
 * Handle 4+ are pre-opened directories, then dynamically allocated.
 */

#define WAPI_MAX_HANDLES 4096

typedef enum wapi_handle_type_t {
    WAPI_HTYPE_FREE = 0,
    WAPI_HTYPE_FILE,
    WAPI_HTYPE_DIRECTORY,
    WAPI_HTYPE_SURFACE,
    WAPI_HTYPE_GPU_DEVICE,
    WAPI_HTYPE_GPU_QUEUE,
    WAPI_HTYPE_GPU_TEXTURE,
    WAPI_HTYPE_GPU_TEXTURE_VIEW,
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
    /* Pending timeouts */
    struct {
        uint64_t user_data;
        uint64_t deadline_ns;  /* absolute monotonic time */
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
    uint32_t    transport;  /* wapi_net_transport_t */
    bool        connected;
    bool        nonblocking;
} wapi_net_conn_t;

/* ---- Crypto ---- */
typedef struct wapi_crypto_hash_ctx_t {
    uint32_t algo;
    uint8_t  state[256]; /* opaque platform hash state */
    size_t   state_size;
} wapi_crypto_hash_ctx_t;

typedef struct wapi_crypto_key_t {
    uint8_t  data[512];
    size_t   len;
    uint32_t usages;
} wapi_crypto_key_t;

/* ---- Memory allocator state ---- */
typedef struct wapi_mem_alloc_t {
    uint32_t offset; /* offset in wasm linear memory */
    uint32_t size;
} wapi_mem_alloc_t;

#define WAPI_MEM_MAX_ALLOCS 4096

typedef struct wapi_handle_entry_t {
    wapi_handle_type_t type;
    union {
        FILE*               file;
        struct {
            char             path[512];
        } dir;
        SDL_Window*         window;
        struct {
            WGPUDevice       device;
            WGPUAdapter      adapter;
            WGPUInstance      instance;
        } gpu_device;
        WGPUQueue           gpu_queue;
        WGPUTexture         gpu_texture;
        WGPUTextureView     gpu_texture_view;
        SDL_AudioDeviceID   audio_device_id;
        SDL_AudioStream*    audio_stream;
        wapi_io_queue_t*    io_queue;
        wapi_net_conn_t     net_conn;
        wapi_crypto_hash_ctx_t crypto_hash;
        wapi_crypto_key_t   crypto_key;
    } data;
} wapi_handle_entry_t;

/* ============================================================
 * Event Queue
 * ============================================================ */

#define WAPI_EVENT_QUEUE_SIZE 256

/* A 128-byte event blob matching wapi_event_t layout */
typedef struct wapi_host_event_t {
    uint8_t data[128];
} wapi_host_event_t;

typedef struct wapi_event_queue_t {
    wapi_host_event_t events[WAPI_EVENT_QUEUE_SIZE];
    int             head;
    int             tail;
    int             count;
} wapi_event_queue_t;

/* ============================================================
 * GPU Surface State
 * ============================================================
 * Tracks the wgpu surface object associated with a WAPI surface handle.
 */

#define WAPI_MAX_GPU_SURFACES 16

typedef struct wapi_gpu_surface_state_t {
    int32_t         wapi_surface_handle;  /* WAPI surface handle */
    WGPUSurface     wgpu_surface;
    WGPUDevice      wgpu_device;
    uint32_t        format;
    uint32_t        present_mode;
    uint32_t        usage;
    bool            configured;
} wapi_gpu_surface_state_t;

/* ============================================================
 * Pre-opened Directory
 * ============================================================ */

#define WAPI_MAX_PREOPENS 32

typedef struct wapi_preopen_t {
    char        guest_path[256];   /* Path as seen by the guest */
    char        host_path[512];    /* Actual host filesystem path */
    int32_t     handle;            /* Handle in the handle table */
} wapi_preopen_t;

/* ============================================================
 * Runtime State (global singleton)
 * ============================================================ */

typedef struct wapi_runtime_t {
    /* Wasmtime */
    wasm_engine_t*          engine;
    wasmtime_store_t*       store;
    wasmtime_context_t*     context;
    wasmtime_module_t*      module;
    wasmtime_instance_t     instance;
    wasmtime_memory_t       memory;
    bool                    memory_valid;

    /* Handle table */
    wapi_handle_entry_t       handles[WAPI_MAX_HANDLES];
    int32_t                 next_handle;  /* Next free handle to try */

    /* Event queue */
    wapi_event_queue_t        event_queue;

    /* GPU surface tracking */
    wapi_gpu_surface_state_t  gpu_surfaces[WAPI_MAX_GPU_SURFACES];
    int                     gpu_surface_count;

    /* Pre-opened directories */
    wapi_preopen_t            preopens[WAPI_MAX_PREOPENS];
    int                     preopen_count;

    /* Application args passed to the Wasm module */
    int                     app_argc;
    char**                  app_argv;

    /* Keyboard state (256 scancodes) */
    bool                    key_state[256];
    uint16_t                mod_state;

    /* Mouse state */
    float                   mouse_x;
    float                   mouse_y;
    uint32_t                mouse_buttons;

    /* Host memory allocator for wasm heap */
    wapi_mem_alloc_t        mem_allocs[WAPI_MEM_MAX_ALLOCS];
    int                     mem_alloc_count;
    uint32_t                mem_heap_top;  /* bump pointer in wasm memory */
    bool                    mem_initialized;

    /* KV storage directory path */
    char                    kv_storage_path[512];

    /* Net init state */
    bool                    net_initialized;

    /* Last error message */
    char                    last_error[512];

    /* Running state */
    bool                    running;
    bool                    has_wapi_frame;
} wapi_runtime_t;

/* Global runtime instance */
extern wapi_runtime_t g_rt;

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
    /* Wrap around */
    for (int i = 4; i < g_rt.next_handle; i++) {
        if (g_rt.handles[i].type == WAPI_HTYPE_FREE) {
            g_rt.handles[i].type = type;
            memset(&g_rt.handles[i].data, 0, sizeof(g_rt.handles[i].data));
            g_rt.next_handle = i + 1;
            return (int32_t)i;
        }
    }
    return 0; /* WAPI_HANDLE_INVALID */
}

static inline bool wapi_handle_valid(int32_t h, wapi_handle_type_t expected) {
    if (h <= 0 || h >= WAPI_MAX_HANDLES) return false;
    if (g_rt.handles[h].type == WAPI_HTYPE_FREE) return false;
    if (expected != WAPI_HTYPE_FREE && g_rt.handles[h].type != expected) return false;
    return true;
}

/* Validate handle accepting any of several types */
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

/* ============================================================
 * Wasm Memory Helpers
 * ============================================================
 * Read/write data from the Wasm linear memory. All guest pointers
 * are uint32_t offsets into the memory.
 */

static inline uint8_t* wapi_wasm_memory_base(void) {
    if (!g_rt.memory_valid) return NULL;
    return wasmtime_memory_data(g_rt.context, &g_rt.memory);
}

static inline size_t wapi_wasm_memory_size(void) {
    if (!g_rt.memory_valid) return 0;
    return wasmtime_memory_data_size(g_rt.context, &g_rt.memory);
}

/* Validate a guest pointer + length is within memory bounds */
static inline bool wapi_wasm_validate_ptr(uint32_t ptr, uint32_t len) {
    if (len == 0) return true;
    size_t mem_size = wapi_wasm_memory_size();
    return (ptr < mem_size && (uint64_t)ptr + len <= mem_size);
}

/* Get a host pointer from a guest pointer, with bounds checking */
static inline void* wapi_wasm_ptr(uint32_t guest_ptr, uint32_t len) {
    if (!wapi_wasm_validate_ptr(guest_ptr, len)) return NULL;
    return wapi_wasm_memory_base() + guest_ptr;
}

/* Read an i32 from guest memory */
static inline bool wapi_wasm_read_i32(uint32_t ptr, int32_t* out) {
    void* host = wapi_wasm_ptr(ptr, 4);
    if (!host) return false;
    memcpy(out, host, 4);
    return true;
}

/* Write an i32 to guest memory */
static inline bool wapi_wasm_write_i32(uint32_t ptr, int32_t val) {
    void* host = wapi_wasm_ptr(ptr, 4);
    if (!host) return false;
    memcpy(host, &val, 4);
    return true;
}

/* Write a u32 to guest memory */
static inline bool wapi_wasm_write_u32(uint32_t ptr, uint32_t val) {
    void* host = wapi_wasm_ptr(ptr, 4);
    if (!host) return false;
    memcpy(host, &val, 4);
    return true;
}

/* Write an i64/u64 to guest memory */
static inline bool wapi_wasm_write_u64(uint32_t ptr, uint64_t val) {
    void* host = wapi_wasm_ptr(ptr, 8);
    if (!host) return false;
    memcpy(host, &val, 8);
    return true;
}

/* Write a float to guest memory */
static inline bool wapi_wasm_write_f32(uint32_t ptr, float val) {
    void* host = wapi_wasm_ptr(ptr, 4);
    if (!host) return false;
    memcpy(host, &val, 4);
    return true;
}

/* Read a string from guest memory (returns host pointer, not owned) */
static inline const char* wapi_wasm_read_string(uint32_t ptr, uint32_t len) {
    if (len == 0) return "";
    void* host = wapi_wasm_ptr(ptr, len);
    return (const char*)host;
}

/* Write bytes to guest memory */
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
    /* Flush only specific type: rebuild queue in place */
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
 * GPU Surface Lookup
 * ============================================================ */

static inline wapi_gpu_surface_state_t* wapi_gpu_surface_find(int32_t surface_handle) {
    for (int i = 0; i < g_rt.gpu_surface_count; i++) {
        if (g_rt.gpu_surfaces[i].wapi_surface_handle == surface_handle) {
            return &g_rt.gpu_surfaces[i];
        }
    }
    return NULL;
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
 * ============================================================
 * Convenience for defining wasmtime host functions (import callbacks).
 *
 * All WAPI import callbacks use the wasmtime_func_callback_t signature:
 *   wasm_trap_t* callback(void* env, wasmtime_caller_t* caller,
 *                         const wasmtime_val_t* args, size_t nargs,
 *                         wasmtime_val_t* results, size_t nresults);
 */

/* Forward declarations for all host import registration functions */
void wapi_host_register_capability(wasmtime_linker_t* linker);
void wapi_host_register_env(wasmtime_linker_t* linker);
void wapi_host_register_clock(wasmtime_linker_t* linker);
void wapi_host_register_surface(wasmtime_linker_t* linker);
void wapi_host_register_input(wasmtime_linker_t* linker);
void wapi_host_register_audio(wasmtime_linker_t* linker);
void wapi_host_register_gpu(wasmtime_linker_t* linker);
void wapi_host_register_fs(wasmtime_linker_t* linker);
void wapi_host_register_memory(wasmtime_linker_t* linker);
void wapi_host_register_io(wasmtime_linker_t* linker);
void wapi_host_register_net(wasmtime_linker_t* linker);
void wapi_host_register_clipboard(wasmtime_linker_t* linker);
void wapi_host_register_kv(wasmtime_linker_t* linker);
void wapi_host_register_crypto(wasmtime_linker_t* linker);
void wapi_host_register_content(wasmtime_linker_t* linker);
void wapi_host_register_text(wasmtime_linker_t* linker);
void wapi_host_register_font(wasmtime_linker_t* linker);
void wapi_host_register_video(wasmtime_linker_t* linker);
void wapi_host_register_module(wasmtime_linker_t* linker);
void wapi_host_register_notifications(wasmtime_linker_t* linker);
void wapi_host_register_geolocation(wasmtime_linker_t* linker);
void wapi_host_register_sensors(wasmtime_linker_t* linker);
void wapi_host_register_speech(wasmtime_linker_t* linker);
void wapi_host_register_biometric(wasmtime_linker_t* linker);
void wapi_host_register_share(wasmtime_linker_t* linker);
void wapi_host_register_payments(wasmtime_linker_t* linker);
void wapi_host_register_usb(wasmtime_linker_t* linker);
void wapi_host_register_midi(wasmtime_linker_t* linker);
void wapi_host_register_bluetooth(wasmtime_linker_t* linker);
void wapi_host_register_camera(wasmtime_linker_t* linker);
void wapi_host_register_xr(wasmtime_linker_t* linker);
void wapi_host_register_register(wasmtime_linker_t* linker);
void wapi_host_register_taskbar(wasmtime_linker_t* linker);
void wapi_host_register_permissions(wasmtime_linker_t* linker);
void wapi_host_register_wake_lock(wasmtime_linker_t* linker);
void wapi_host_register_orientation(wasmtime_linker_t* linker);
void wapi_host_register_codec(wasmtime_linker_t* linker);
void wapi_host_register_compression(wasmtime_linker_t* linker);
void wapi_host_register_media_session(wasmtime_linker_t* linker);
void wapi_host_register_media_caps(wasmtime_linker_t* linker);
void wapi_host_register_encoding(wasmtime_linker_t* linker);
void wapi_host_register_authn(wasmtime_linker_t* linker);
void wapi_host_register_network_info(wasmtime_linker_t* linker);
void wapi_host_register_battery(wasmtime_linker_t* linker);
void wapi_host_register_idle(wasmtime_linker_t* linker);
void wapi_host_register_haptics(wasmtime_linker_t* linker);
void wapi_host_register_p2p(wasmtime_linker_t* linker);
void wapi_host_register_hid(wasmtime_linker_t* linker);
void wapi_host_register_serial(wasmtime_linker_t* linker);
void wapi_host_register_screen_capture(wasmtime_linker_t* linker);
void wapi_host_register_contacts(wasmtime_linker_t* linker);
void wapi_host_register_barcode(wasmtime_linker_t* linker);
void wapi_host_register_nfc(wasmtime_linker_t* linker);
void wapi_host_register_dnd(wasmtime_linker_t* linker);

/* Helper to define a linker function with proper error checking */
#define WAPI_LINK_FUNC(linker, module, name, callback, param_types, param_count, result_types, result_count) \
    do { \
        wasm_functype_t* ftype = wasm_functype_new( \
            &(wasm_valtype_vec_t){ .size = param_count, .data = param_types }, \
            &(wasm_valtype_vec_t){ .size = result_count, .data = result_types }); \
        wasmtime_error_t* err = wasmtime_linker_define_func( \
            linker, module, strlen(module), name, strlen(name), \
            ftype, callback, NULL, NULL); \
        wasm_functype_delete(ftype); \
        if (err) { \
            wasm_message_t msg; \
            wasmtime_error_message(err, &msg); \
            fprintf(stderr, "Failed to link %s.%s: %.*s\n", \
                    module, name, (int)msg.size, msg.data); \
            wasm_byte_vec_delete(&msg); \
            wasmtime_error_delete(err); \
        } \
    } while(0)

/* Shorthand wasm_valtype creators */
static inline wasm_valtype_t* vt_i32(void) { return wasm_valtype_new(WASM_I32); }
static inline wasm_valtype_t* vt_i64(void) { return wasm_valtype_new(WASM_I64); }
static inline wasm_valtype_t* vt_f32(void) { return wasm_valtype_new(WASM_F32); }

/* Helper to build functype more easily */
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

/* Even more convenient: build functype from inline arrays */
#define WAPI_FUNCTYPE(ptypes, pcnt, rtypes, rcnt) \
    wapi_functype(pcnt, (wasm_valkind_t[]){ ptypes }, rcnt, (wasm_valkind_t[]){ rtypes })

/* Common WAPI callback macro for Wasmtime: extract i32 args easily */
#define WAPI_ARG_I32(n) ((int32_t)args[n].of.i32)
#define WAPI_ARG_I64(n) ((int64_t)args[n].of.i64)
#define WAPI_ARG_U32(n) ((uint32_t)args[n].of.i32)
#define WAPI_ARG_U64(n) ((uint64_t)args[n].of.i64)

#define WAPI_RET_I32(val) do { results[0].kind = WASMTIME_I32; results[0].of.i32 = (int32_t)(val); } while(0)
#define WAPI_RET_I64(val) do { results[0].kind = WASMTIME_I64; results[0].of.i64 = (int64_t)(val); } while(0)

/* Define a linker func using the helper that constructs functype */
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

/* Convenience wrappers for common signatures */
#define WAPI_DEFINE_0_1(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 0, NULL, 1, (wasm_valkind_t[]){WASM_I32})

#define WAPI_DEFINE_1_1(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 1, (wasm_valkind_t[]){WASM_I32}, 1, (wasm_valkind_t[]){WASM_I32})

#define WAPI_DEFINE_2_1(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 2, (wasm_valkind_t[]){WASM_I32,WASM_I32}, 1, (wasm_valkind_t[]){WASM_I32})

#define WAPI_DEFINE_3_1(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 3, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I32}, 1, (wasm_valkind_t[]){WASM_I32})

#define WAPI_DEFINE_4_1(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 4, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I32,WASM_I32}, 1, (wasm_valkind_t[]){WASM_I32})

#define WAPI_DEFINE_5_1(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 5, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I32,WASM_I32,WASM_I32}, 1, (wasm_valkind_t[]){WASM_I32})

#define WAPI_DEFINE_6_1(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 6, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I32,WASM_I32,WASM_I32,WASM_I32}, 1, (wasm_valkind_t[]){WASM_I32})

#define WAPI_DEFINE_1_0(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 1, (wasm_valkind_t[]){WASM_I32}, 0, NULL)

#define WAPI_DEFINE_2_0(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 2, (wasm_valkind_t[]){WASM_I32,WASM_I32}, 0, NULL)

#define WAPI_DEFINE_0_0(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 0, NULL, 0, NULL)

#ifdef __cplusplus
}
#endif

#endif /* WAPI_HOST_H */
