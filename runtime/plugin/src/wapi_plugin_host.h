/**
 * WAPI CLAP Plugin Wrapper - Shared Host Types
 *
 * Holds per-plugin-instance state: Wasmtime engine, compiled module,
 * exported function handles, parameter cache, and audio scratch area
 * offsets into the Wasm linear memory.
 *
 * This header is shared by all wapi_clap_*.c and wapi_plugin_host.c files.
 */

#ifndef WAPI_PLUGIN_HOST_H
#define WAPI_PLUGIN_HOST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Wasmtime C API */
#include <wasmtime.h>

/* WAPI ABI headers */
#include <wapi/wapi_types.h>
#include <wapi/wapi_audio_plugin.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum parameters a plugin can have */
#define WAPI_MAX_PARAMS 256

/* Maximum MIDI events per process block */
#define WAPI_MAX_MIDI_EVENTS 256

/* Maximum audio channels */
#define WAPI_MAX_CHANNELS 64

/* ============================================================
 * Plugin Instance State
 * ============================================================
 * One per CLAP plugin instance. Owns the Wasmtime store and
 * all Wasm-side resources.
 */

typedef struct wapi_wasm_plugin_t {
    /* Wasmtime core objects */
    wasm_engine_t*      engine;
    wasmtime_store_t*   store;
    wasmtime_module_t*  module;
    wasmtime_instance_t instance;
    wasmtime_memory_t   memory;
    bool                memory_valid;

    /* Exported Wasm functions (resolved at instantiation) */
    wasmtime_func_t     fn_get_desc;
    wasmtime_func_t     fn_get_param_info;
    wasmtime_func_t     fn_activate;
    wasmtime_func_t     fn_deactivate;
    wasmtime_func_t     fn_process;
    wasmtime_func_t     fn_param_changed;
    wasmtime_func_t     fn_state_save;
    wasmtime_func_t     fn_state_load;
    bool                has_gui;
    wasmtime_func_t     fn_gui_create;
    wasmtime_func_t     fn_gui_destroy;

    /* Optional: wapi_mem_alloc exported by the Wasm module */
    bool                has_mem_alloc;
    wasmtime_func_t     fn_mem_alloc;

    /* Plugin metadata (read from Wasm at init) */
    char                name[256];
    char                vendor[256];
    char                version[64];
    uint32_t            category;
    uint32_t            input_channels;
    uint32_t            output_channels;
    uint32_t            flags;

    /* Parameters */
    uint32_t            param_count;
    wapi_param_info_t     params[WAPI_MAX_PARAMS];
    char                param_names[WAPI_MAX_PARAMS][128];  /* Owned name storage */
    float               param_values[WAPI_MAX_PARAMS];      /* Current values */

    /* Audio processing state */
    float               sample_rate;
    uint32_t            max_block_size;
    bool                activated;
    bool                processing;

    /* Wasm memory scratch area offsets for process data */
    uint32_t            process_data_offset;   /* wapi_process_data_t */
    uint32_t            transport_offset;      /* wapi_transport_t */
    uint32_t            midi_buf_offset;       /* wapi_midi_event_t[WAPI_MAX_MIDI_EVENTS] */
    uint32_t            input_buf_offset;      /* float[channels * max_block_size] */
    uint32_t            output_buf_offset;     /* float[channels * max_block_size] */
    uint32_t            input_ptrs_offset;     /* uint32_t[channels] (Wasm pointers) */
    uint32_t            output_ptrs_offset;    /* uint32_t[channels] (Wasm pointers) */
    uint32_t            scratch_total;         /* Total scratch bytes allocated */

    /* Path to .wasm file */
    char                wasm_path[1024];

    /* MIDI output queue (filled by Wasm calling wapi_plugin_send_midi) */
    wapi_midi_event_t     midi_out[WAPI_MAX_MIDI_EVENTS];
    uint32_t            midi_out_count;

    /* Parameter output flags (set when Wasm calls wapi_plugin_param_set) */
    bool                param_changed[WAPI_MAX_PARAMS];
} wapi_wasm_plugin_t;

/* ============================================================
 * Wasm Memory Helpers
 * ============================================================ */

/* Get a pointer to the base of Wasm linear memory */
static inline uint8_t* wapi_wasm_mem(wapi_wasm_plugin_t* p) {
    if (!p->memory_valid) return NULL;
    wasmtime_context_t* ctx = wasmtime_store_context(p->store);
    return wasmtime_memory_data(ctx, &p->memory);
}

static inline size_t wapi_wasm_mem_size(wapi_wasm_plugin_t* p) {
    if (!p->memory_valid) return 0;
    wasmtime_context_t* ctx = wasmtime_store_context(p->store);
    return wasmtime_memory_data_size(ctx, &p->memory);
}

/* Validate a guest pointer + length is within Wasm memory bounds */
static inline bool wapi_wasm_validate(wapi_wasm_plugin_t* p, uint32_t ptr, uint32_t len) {
    if (len == 0) return true;
    size_t mem_size = wapi_wasm_mem_size(p);
    return (ptr < mem_size && (uint64_t)ptr + len <= mem_size);
}

/* Get a host pointer from a guest pointer with bounds checking */
static inline void* wapi_wasm_ptr(wapi_wasm_plugin_t* p, uint32_t guest_ptr, uint32_t len) {
    if (!wapi_wasm_validate(p, guest_ptr, len)) return NULL;
    return wapi_wasm_mem(p) + guest_ptr;
}

/* Read bytes from guest memory */
static inline bool wapi_wasm_read_bytes(wapi_wasm_plugin_t* p, uint32_t ptr,
                                       void* dst, uint32_t len) {
    void* host = wapi_wasm_ptr(p, ptr, len);
    if (!host) return false;
    memcpy(dst, host, len);
    return true;
}

/* Write bytes to guest memory */
static inline bool wapi_wasm_write_bytes(wapi_wasm_plugin_t* p, uint32_t ptr,
                                        const void* src, uint32_t len) {
    if (len == 0) return true;
    void* host = wapi_wasm_ptr(p, ptr, len);
    if (!host) return false;
    memcpy(host, src, len);
    return true;
}

/* Read a uint32_t from guest memory */
static inline bool wapi_wasm_read_u32(wapi_wasm_plugin_t* p, uint32_t ptr, uint32_t* out) {
    return wapi_wasm_read_bytes(p, ptr, out, 4);
}

/* Write a uint32_t to guest memory */
static inline bool wapi_wasm_write_u32(wapi_wasm_plugin_t* p, uint32_t ptr, uint32_t val) {
    return wapi_wasm_write_bytes(p, ptr, &val, 4);
}

/* Write a float to guest memory */
static inline bool wapi_wasm_write_f32(wapi_wasm_plugin_t* p, uint32_t ptr, float val) {
    return wapi_wasm_write_bytes(p, ptr, &val, 4);
}

/* Read a float from guest memory */
static inline bool wapi_wasm_read_f32(wapi_wasm_plugin_t* p, uint32_t ptr, float* out) {
    return wapi_wasm_read_bytes(p, ptr, out, 4);
}

/* Read a string from Wasm memory into a host buffer */
static inline bool wapi_wasm_read_string(wapi_wasm_plugin_t* p, uint32_t ptr, uint32_t len,
                                        char* dst, size_t dst_size) {
    if (len == 0) { dst[0] = '\0'; return true; }
    if (len >= dst_size) len = (uint32_t)(dst_size - 1);
    void* host = wapi_wasm_ptr(p, ptr, len);
    if (!host) { dst[0] = '\0'; return false; }
    memcpy(dst, host, len);
    dst[len] = '\0';
    return true;
}

/* ============================================================
 * Wasmtime Helper Macros
 * ============================================================
 * Matching the desktop runtime pattern for arg extraction.
 */

#define WAPI_ARG_I32(n) ((int32_t)args[n].of.i32)
#define WAPI_ARG_U32(n) ((uint32_t)args[n].of.i32)
#define WAPI_ARG_F32(n) (args[n].of.f32)
#define WAPI_RET_I32(val) do { results[0].kind = WASMTIME_I32; results[0].of.i32 = (int32_t)(val); } while(0)
#define WAPI_RET_F32(val) do { results[0].kind = WASMTIME_F32; results[0].of.f32 = (val); } while(0)

/* ============================================================
 * Function Type Builder (from desktop runtime pattern)
 * ============================================================ */

static inline wasm_functype_t* wapi_functype(int np, wasm_valkind_t* ptypes,
                                            int nr, wasm_valkind_t* rtypes) {
    wasm_valtype_vec_t params, results;
    if (np > 0) {
        wasm_valtype_t* pv[16];
        for (int i = 0; i < np && i < 16; i++) pv[i] = wasm_valtype_new(ptypes[i]);
        wasm_valtype_vec_new(&params, np, pv);
    } else {
        wasm_valtype_vec_new_empty(&params);
    }
    if (nr > 0) {
        wasm_valtype_t* rv[4];
        for (int i = 0; i < nr && i < 4; i++) rv[i] = wasm_valtype_new(rtypes[i]);
        wasm_valtype_vec_new(&results, nr, rv);
    } else {
        wasm_valtype_vec_new_empty(&results);
    }
    return wasm_functype_new(&params, &results);
}

/* Define a linker function with error checking */
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
        fprintf(stderr, "[wapi_plugin] link error %s::%s: %.*s\n",
                module, name, (int)msg.size, msg.data);
        wasm_byte_vec_delete(&msg);
        wasmtime_error_delete(err);
    }
}

/* Convenience macros matching the desktop runtime pattern */
#define WAPI_DEFINE_0_0(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 0, NULL, 0, NULL)

#define WAPI_DEFINE_0_1(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 0, NULL, 1, (wasm_valkind_t[]){WASM_I32})

#define WAPI_DEFINE_1_0(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 1, (wasm_valkind_t[]){WASM_I32}, 0, NULL)

#define WAPI_DEFINE_1_1(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 1, (wasm_valkind_t[]){WASM_I32}, 1, (wasm_valkind_t[]){WASM_I32})

#define WAPI_DEFINE_2_1(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 2, (wasm_valkind_t[]){WASM_I32,WASM_I32}, 1, (wasm_valkind_t[]){WASM_I32})

#define WAPI_DEFINE_3_1(linker, mod, name, cb) \
    wapi_linker_define(linker, mod, name, cb, 3, (wasm_valkind_t[]){WASM_I32,WASM_I32,WASM_I32}, 1, (wasm_valkind_t[]){WASM_I32})

/* ============================================================
 * Forward Declarations
 * ============================================================ */

/* wapi_clap_entry.c */
extern const char* wapi_plugin_wasm_path;

/* wapi_clap_plugin.c */
int  wapi_wasm_plugin_load(wapi_wasm_plugin_t* plugin, const char* wasm_path);
void wapi_wasm_plugin_unload(wapi_wasm_plugin_t* plugin);
int  wapi_wasm_plugin_read_desc(wapi_wasm_plugin_t* plugin);
int  wapi_wasm_plugin_read_params(wapi_wasm_plugin_t* plugin);

/* wapi_clap_process.c */
int wapi_wasm_process(wapi_wasm_plugin_t* plugin,
                    float** inputs, uint32_t num_inputs,
                    float** outputs, uint32_t num_outputs,
                    uint32_t num_samples,
                    const wapi_transport_t* transport,
                    const wapi_midi_event_t* midi_events, uint32_t midi_count);

/* wapi_plugin_host.c */
void wapi_plugin_host_register(wasmtime_linker_t* linker);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_PLUGIN_HOST_H */
