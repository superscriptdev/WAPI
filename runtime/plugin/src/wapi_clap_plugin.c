/**
 * WAPI CLAP Plugin Wrapper - Plugin Lifecycle
 *
 * Implements the CLAP plugin callbacks (init, destroy, activate, etc.)
 * and the Wasm module loading/unloading logic. Also implements the
 * CLAP extension interfaces: audio-ports, note-ports, params, state.
 *
 * Each CLAP plugin instance owns a wapi_wasm_plugin_t which holds
 * its own Wasmtime store and instantiated module.
 */

#include <clap/clap.h>
#include "wapi_plugin_host.h"

/* ============================================================
 * Helper: Call a Wasm function with i32 args and i32 result
 * ============================================================ */

static bool call_wasm_void(wapi_wasm_plugin_t* p, wasmtime_func_t* fn) {
    wasmtime_context_t* ctx = wasmtime_store_context(p->store);
    wasmtime_val_t results[1];
    wasm_trap_t* trap = NULL;
    wasmtime_error_t* err = wasmtime_func_call(ctx, fn, NULL, 0, results, 0, &trap);
    if (err) { wasmtime_error_delete(err); return false; }
    if (trap) { wasm_trap_delete(trap); return false; }
    return true;
}

static int32_t call_wasm_i32_result(wapi_wasm_plugin_t* p, wasmtime_func_t* fn,
                                     wasmtime_val_t* args, size_t nargs) {
    wasmtime_context_t* ctx = wasmtime_store_context(p->store);
    wasmtime_val_t results[1];
    wasm_trap_t* trap = NULL;
    wasmtime_error_t* err = wasmtime_func_call(ctx, fn, args, nargs, results, 1, &trap);
    if (err) { wasmtime_error_delete(err); return WAPI_ERR_UNKNOWN; }
    if (trap) { wasm_trap_delete(trap); return WAPI_ERR_UNKNOWN; }
    return results[0].of.i32;
}

/* ============================================================
 * Wasm Module Loading
 * ============================================================ */

/* Try to find an exported function by name. Returns true if found. */
static bool find_export(wasmtime_context_t* ctx, wasmtime_instance_t* inst,
                        const char* name, wasmtime_func_t* out_func) {
    wasmtime_extern_t ext;
    if (!wasmtime_instance_export_get(ctx, inst, name, strlen(name), &ext)) {
        return false;
    }
    if (ext.kind != WASMTIME_EXTERN_FUNC) {
        return false;
    }
    *out_func = ext.of.func;
    return true;
}

/* Try to find the exported memory. */
static bool find_memory(wasmtime_context_t* ctx, wasmtime_instance_t* inst,
                        wasmtime_memory_t* out_mem) {
    wasmtime_extern_t ext;
    if (!wasmtime_instance_export_get(ctx, inst, "memory", 6, &ext)) {
        return false;
    }
    if (ext.kind != WASMTIME_EXTERN_MEMORY) {
        return false;
    }
    *out_mem = ext.of.memory;
    return true;
}

int wapi_wasm_plugin_load(wapi_wasm_plugin_t* plugin, const char* wasm_path) {
    strncpy(plugin->wasm_path, wasm_path, sizeof(plugin->wasm_path) - 1);
    plugin->wasm_path[sizeof(plugin->wasm_path) - 1] = '\0';

    /* Read the .wasm file */
    FILE* f = fopen(wasm_path, "rb");
    if (!f) {
        fprintf(stderr, "[wapi_plugin] Cannot open %s\n", wasm_path);
        return 0;
    }
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (file_size <= 0) {
        fclose(f);
        fprintf(stderr, "[wapi_plugin] Empty or invalid wasm file: %s\n", wasm_path);
        return 0;
    }
    uint8_t* wasm_bytes = (uint8_t*)malloc((size_t)file_size);
    if (!wasm_bytes) {
        fclose(f);
        fprintf(stderr, "[wapi_plugin] Out of memory reading %s\n", wasm_path);
        return 0;
    }
    size_t read = fread(wasm_bytes, 1, (size_t)file_size, f);
    fclose(f);
    if ((long)read != file_size) {
        free(wasm_bytes);
        fprintf(stderr, "[wapi_plugin] Short read on %s\n", wasm_path);
        return 0;
    }

    /* Create a Wasmtime store. The store's data pointer holds
     * the plugin instance so host callbacks can access it. */
    plugin->store = wasmtime_store_new(plugin->engine, plugin, NULL);
    if (!plugin->store) {
        free(wasm_bytes);
        fprintf(stderr, "[wapi_plugin] Failed to create Wasmtime store\n");
        return 0;
    }
    wasmtime_context_t* ctx = wasmtime_store_context(plugin->store);

    /* Compile the Wasm module */
    wasmtime_error_t* err = wasmtime_module_new(
        plugin->engine, wasm_bytes, (size_t)file_size, &plugin->module);
    free(wasm_bytes);
    if (err) {
        wasm_message_t msg;
        wasmtime_error_message(err, &msg);
        fprintf(stderr, "[wapi_plugin] Compile error: %.*s\n", (int)msg.size, msg.data);
        wasm_byte_vec_delete(&msg);
        wasmtime_error_delete(err);
        wasmtime_store_delete(plugin->store);
        plugin->store = NULL;
        return 0;
    }

    /* Create a linker and register WAPI host imports */
    wasmtime_linker_t* linker = wasmtime_linker_new(plugin->engine);
    if (!linker) {
        fprintf(stderr, "[wapi_plugin] Failed to create linker\n");
        wasmtime_module_delete(plugin->module);
        wasmtime_store_delete(plugin->store);
        plugin->module = NULL;
        plugin->store = NULL;
        return 0;
    }

    /* Register the WAPI plugin host functions (wapi_plugin.* imports) */
    wapi_plugin_host_register(linker);

    /* Instantiate the module */
    wasm_trap_t* trap = NULL;
    err = wasmtime_linker_instantiate(linker, ctx, plugin->module,
                                      &plugin->instance, &trap);
    wasmtime_linker_delete(linker);

    if (err) {
        wasm_message_t msg;
        wasmtime_error_message(err, &msg);
        fprintf(stderr, "[wapi_plugin] Instantiation error: %.*s\n", (int)msg.size, msg.data);
        wasm_byte_vec_delete(&msg);
        wasmtime_error_delete(err);
        wasmtime_module_delete(plugin->module);
        wasmtime_store_delete(plugin->store);
        plugin->module = NULL;
        plugin->store = NULL;
        return 0;
    }
    if (trap) {
        wasm_message_t msg;
        wasm_trap_message(trap, &msg);
        fprintf(stderr, "[wapi_plugin] Instantiation trap: %.*s\n", (int)msg.size, msg.data);
        wasm_byte_vec_delete(&msg);
        wasm_trap_delete(trap);
        wasmtime_module_delete(plugin->module);
        wasmtime_store_delete(plugin->store);
        plugin->module = NULL;
        plugin->store = NULL;
        return 0;
    }

    /* Find exported memory */
    plugin->memory_valid = find_memory(ctx, &plugin->instance, &plugin->memory);
    if (!plugin->memory_valid) {
        fprintf(stderr, "[wapi_plugin] Warning: no exported memory\n");
    }

    /* Find required exported functions */
    bool ok = true;
    ok = ok && find_export(ctx, &plugin->instance, "wapi_plugin_get_desc", &plugin->fn_get_desc);
    ok = ok && find_export(ctx, &plugin->instance, "wapi_plugin_process", &plugin->fn_process);
    if (!ok) {
        fprintf(stderr, "[wapi_plugin] Missing required exports (wapi_plugin_get_desc, wapi_plugin_process)\n");
        wasmtime_module_delete(plugin->module);
        wasmtime_store_delete(plugin->store);
        plugin->module = NULL;
        plugin->store = NULL;
        return 0;
    }

    /* Find optional exported functions */
    find_export(ctx, &plugin->instance, "wapi_plugin_get_param_info", &plugin->fn_get_param_info);
    find_export(ctx, &plugin->instance, "wapi_plugin_activate", &plugin->fn_activate);
    find_export(ctx, &plugin->instance, "wapi_plugin_deactivate", &plugin->fn_deactivate);
    find_export(ctx, &plugin->instance, "wapi_plugin_param_changed", &plugin->fn_param_changed);
    find_export(ctx, &plugin->instance, "wapi_plugin_state_save", &plugin->fn_state_save);
    find_export(ctx, &plugin->instance, "wapi_plugin_state_load", &plugin->fn_state_load);

    /* GUI exports (optional) */
    plugin->has_gui = find_export(ctx, &plugin->instance, "wapi_plugin_gui_create", &plugin->fn_gui_create)
                   && find_export(ctx, &plugin->instance, "wapi_plugin_gui_destroy", &plugin->fn_gui_destroy);

    /* Optional memory allocator */
    plugin->has_mem_alloc = find_export(ctx, &plugin->instance, "wapi_mem_alloc", &plugin->fn_mem_alloc);

    return 1;
}

void wapi_wasm_plugin_unload(wapi_wasm_plugin_t* plugin) {
    if (plugin->module) {
        wasmtime_module_delete(plugin->module);
        plugin->module = NULL;
    }
    if (plugin->store) {
        wasmtime_store_delete(plugin->store);
        plugin->store = NULL;
    }
    plugin->memory_valid = false;
    plugin->activated = false;
    plugin->processing = false;
}

/* ============================================================
 * Read Plugin Descriptor from Wasm
 * ============================================================
 * Calls wapi_plugin_get_desc(desc_ptr) which writes a wapi_plugin_desc_t
 * into Wasm linear memory. We then read the struct and extract
 * the strings into host-side buffers.
 *
 * wapi_plugin_desc_t layout in wasm32 (all pointers are i32):
 *   Offset  0: name      (i32 ptr)
 *   Offset  4: name_len  (u32)
 *   Offset  8: vendor    (i32 ptr)
 *   Offset 12: vendor_len(u32)
 *   Offset 16: version   (i32 ptr)
 *   Offset 20: version_len(u32)
 *   Offset 24: category  (u32)
 *   Offset 28: default_input_channels  (u32)
 *   Offset 32: default_output_channels (u32)
 *   Offset 36: flags     (u32)
 *   Total: 40 bytes
 */

#define WAPI_DESC_SIZE 40

int wapi_wasm_plugin_read_desc(wapi_wasm_plugin_t* plugin) {
    if (!plugin->memory_valid) return 0;

    wasmtime_context_t* ctx = wasmtime_store_context(plugin->store);
    size_t mem_size = wasmtime_memory_data_size(ctx, &plugin->memory);

    /* Allocate scratch space near the end of available memory.
     * Use a high offset that is aligned and within bounds. */
    uint32_t desc_offset = (uint32_t)(mem_size - 4096);
    desc_offset &= ~(uint32_t)15;  /* Align to 16 bytes */

    if (!wapi_wasm_validate(plugin, desc_offset, WAPI_DESC_SIZE)) {
        fprintf(stderr, "[wapi_plugin] Not enough Wasm memory for descriptor\n");
        return 0;
    }

    /* Zero the descriptor area */
    memset(wapi_wasm_ptr(plugin, desc_offset, WAPI_DESC_SIZE), 0, WAPI_DESC_SIZE);

    /* Call wapi_plugin_get_desc(desc_ptr) -> i32 */
    wasmtime_val_t args[1] = {{ .kind = WASMTIME_I32, .of.i32 = (int32_t)desc_offset }};
    int32_t result = call_wasm_i32_result(plugin, &plugin->fn_get_desc, args, 1);
    if (WAPI_FAILED(result)) {
        fprintf(stderr, "[wapi_plugin] wapi_plugin_get_desc returned error %d\n", result);
        return 0;
    }

    /* Read the descriptor fields from Wasm memory */
    uint8_t* base = wapi_wasm_mem(plugin);
    uint8_t* desc = base + desc_offset;

    uint32_t name_ptr, name_len, vendor_ptr, vendor_len, version_ptr, version_len;
    memcpy(&name_ptr,    desc + 0,  4);
    memcpy(&name_len,    desc + 4,  4);
    memcpy(&vendor_ptr,  desc + 8,  4);
    memcpy(&vendor_len,  desc + 12, 4);
    memcpy(&version_ptr, desc + 16, 4);
    memcpy(&version_len, desc + 20, 4);
    memcpy(&plugin->category,        desc + 24, 4);
    memcpy(&plugin->input_channels,  desc + 28, 4);
    memcpy(&plugin->output_channels, desc + 32, 4);
    memcpy(&plugin->flags,           desc + 36, 4);

    /* Read strings from Wasm memory into host buffers */
    wapi_wasm_read_string(plugin, name_ptr, name_len,
                        plugin->name, sizeof(plugin->name));
    wapi_wasm_read_string(plugin, vendor_ptr, vendor_len,
                        plugin->vendor, sizeof(plugin->vendor));
    wapi_wasm_read_string(plugin, version_ptr, version_len,
                        plugin->version, sizeof(plugin->version));

    return 1;
}

/* ============================================================
 * Read Parameter Info from Wasm
 * ============================================================
 * Calls wapi_plugin_get_param_info(index, info_ptr) for each parameter.
 *
 * wapi_param_info_t layout in wasm32:
 *   Offset  0: id           (u32)
 *   Offset  4: name         (i32 ptr)
 *   Offset  8: name_len     (u32)
 *   Offset 12: type         (u32)
 *   Offset 16: default_value(f32)
 *   Offset 20: min_value    (f32)
 *   Offset 24: max_value    (f32)
 *   Offset 28: flags        (u32)
 *   Total: 32 bytes
 */

#define WAPI_PARAM_INFO_SIZE 32

int wapi_wasm_plugin_read_params(wapi_wasm_plugin_t* plugin) {
    if (!plugin->memory_valid) return 0;

    wasmtime_context_t* ctx = wasmtime_store_context(plugin->store);
    size_t mem_size = wasmtime_memory_data_size(ctx, &plugin->memory);

    /* Scratch area for param info */
    uint32_t info_offset = (uint32_t)(mem_size - 4096 + 64);
    info_offset &= ~(uint32_t)15;

    if (!wapi_wasm_validate(plugin, info_offset, WAPI_PARAM_INFO_SIZE)) {
        return 0;
    }

    /* Determine param count. Try calling the Wasm module iteratively
     * until it returns an error. Start with index 0. */
    plugin->param_count = 0;

    for (uint32_t i = 0; i < WAPI_MAX_PARAMS; i++) {
        /* Zero the info area */
        memset(wapi_wasm_ptr(plugin, info_offset, WAPI_PARAM_INFO_SIZE), 0, WAPI_PARAM_INFO_SIZE);

        wasmtime_val_t args[2] = {
            { .kind = WASMTIME_I32, .of.i32 = (int32_t)i },
            { .kind = WASMTIME_I32, .of.i32 = (int32_t)info_offset },
        };
        int32_t result = call_wasm_i32_result(plugin, &plugin->fn_get_param_info, args, 2);
        if (WAPI_FAILED(result)) {
            break;  /* No more parameters */
        }

        /* Read param info from Wasm memory */
        uint8_t* base = wapi_wasm_mem(plugin);
        uint8_t* info = base + info_offset;

        uint32_t param_name_ptr, param_name_len;
        memcpy(&plugin->params[i].id,            info + 0,  4);
        memcpy(&param_name_ptr,                   info + 4,  4);
        memcpy(&param_name_len,                   info + 8,  4);
        memcpy(&plugin->params[i].type,           info + 12, 4);
        memcpy(&plugin->params[i].default_value,  info + 16, 4);
        memcpy(&plugin->params[i].min_value,      info + 20, 4);
        memcpy(&plugin->params[i].max_value,      info + 24, 4);
        memcpy(&plugin->params[i].flags,          info + 28, 4);

        /* Read param name into owned storage */
        wapi_wasm_read_string(plugin, param_name_ptr, param_name_len,
                            plugin->param_names[i], sizeof(plugin->param_names[i]));
        plugin->params[i].name = plugin->param_names[i];
        plugin->params[i].name_len = (wapi_size_t)strlen(plugin->param_names[i]);

        /* Initialize current value to default */
        plugin->param_values[i] = plugin->params[i].default_value;

        plugin->param_count = i + 1;
    }

    return 1;
}

/* ============================================================
 * CLAP Plugin Callbacks
 * ============================================================ */

/* Get our plugin state from a clap_plugin_t pointer */
static inline wapi_wasm_plugin_t* get_plugin(const clap_plugin_t* plug) {
    return (wapi_wasm_plugin_t*)plug->plugin_data;
}

static bool plugin_init(const clap_plugin_t* plug) {
    wapi_wasm_plugin_t* p = get_plugin(plug);

    /* The Wasm module is loaded in wapi_clap_plugin_create.
     * Read descriptor and params here for the real instance. */
    if (!wapi_wasm_plugin_read_desc(p)) {
        fprintf(stderr, "[wapi_plugin] Failed to read descriptor in init\n");
        return false;
    }

    if (!wapi_wasm_plugin_read_params(p)) {
        fprintf(stderr, "[wapi_plugin] Warning: failed to read params in init\n");
    }

    return true;
}

static void plugin_destroy(const clap_plugin_t* plug) {
    wapi_wasm_plugin_t* p = get_plugin(plug);
    wapi_wasm_plugin_unload(p);
    free(p);                  /* Free the Wasm plugin state */
    free((void*)plug);        /* Free the clap_plugin_t itself */
}

static bool plugin_activate(const clap_plugin_t* plug,
                            double sample_rate,
                            uint32_t min_frames_count,
                            uint32_t max_frames_count)
{
    (void)min_frames_count;
    wapi_wasm_plugin_t* p = get_plugin(plug);

    p->sample_rate = (float)sample_rate;
    p->max_block_size = max_frames_count;

    /* Allocate scratch area in Wasm linear memory for process data.
     *
     * Layout:
     *   process_data_offset:   wapi_process_data_t (36 bytes, padded to 48)
     *   transport_offset:      wapi_transport_t (32 bytes)
     *   midi_buf_offset:       wapi_midi_event_t[256] (8 * 256 = 2048 bytes)
     *   input_ptrs_offset:     uint32_t[max_channels] (4 * channels)
     *   output_ptrs_offset:    uint32_t[max_channels] (4 * channels)
     *   input_buf_offset:      float[max_channels * max_block_size]
     *   output_buf_offset:     float[max_channels * max_block_size]
     */
    uint32_t max_ch = p->input_channels > p->output_channels
                    ? p->input_channels : p->output_channels;
    if (max_ch == 0) max_ch = 2;
    if (max_ch > WAPI_MAX_CHANNELS) max_ch = WAPI_MAX_CHANNELS;

    uint32_t process_data_size = 48;   /* wapi_process_data_t padded */
    uint32_t transport_size    = 32;   /* wapi_transport_t */
    uint32_t midi_size         = (uint32_t)(sizeof(wapi_midi_event_t) * WAPI_MAX_MIDI_EVENTS);
    uint32_t ptrs_size         = 4 * max_ch;
    uint32_t audio_buf_size    = 4 * max_ch * max_frames_count;

    uint32_t total = process_data_size + transport_size + midi_size
                   + ptrs_size * 2 + audio_buf_size * 2;

    /* Try to allocate via the module's wapi_mem_alloc if available */
    uint32_t base_offset = 0;
    if (p->has_mem_alloc) {
        wasmtime_val_t alloc_args[1] = {{ .kind = WASMTIME_I32, .of.i32 = (int32_t)total }};
        int32_t alloc_result = call_wasm_i32_result(p, &p->fn_mem_alloc, alloc_args, 1);
        if (alloc_result > 0) {
            base_offset = (uint32_t)alloc_result;
        }
    }

    /* Fallback: use high memory region */
    if (base_offset == 0) {
        wasmtime_context_t* ctx = wasmtime_store_context(p->store);
        size_t mem_size = wasmtime_memory_data_size(ctx, &p->memory);

        /* Try to grow memory if needed */
        uint32_t needed_pages = (total + 65535) / 65536;
        uint64_t prev_pages;
        wasmtime_error_t* grow_err = wasmtime_memory_grow(ctx, &p->memory,
                                                           needed_pages, &prev_pages);
        if (grow_err) {
            wasmtime_error_delete(grow_err);
        }

        mem_size = wasmtime_memory_data_size(ctx, &p->memory);
        if (total + 256 > mem_size) {
            fprintf(stderr, "[wapi_plugin] Not enough Wasm memory for audio scratch area\n");
            return false;
        }

        /* Use the top of memory, leaving a small gap */
        base_offset = (uint32_t)(mem_size - total - 64);
        base_offset &= ~(uint32_t)15;  /* 16-byte alignment */
    }

    /* Assign offsets */
    uint32_t off = base_offset;
    p->process_data_offset = off;  off += process_data_size;
    p->transport_offset    = off;  off += transport_size;
    p->midi_buf_offset     = off;  off += midi_size;
    p->input_ptrs_offset   = off;  off += ptrs_size;
    p->output_ptrs_offset  = off;  off += ptrs_size;
    p->input_buf_offset    = off;  off += audio_buf_size;
    p->output_buf_offset   = off;  off += audio_buf_size;
    p->scratch_total       = off - base_offset;

    /* Call Wasm wapi_plugin_activate(sample_rate, max_block_size) */
    wasmtime_context_t* ctx = wasmtime_store_context(p->store);
    wasmtime_val_t args[2] = {
        { .kind = WASMTIME_F32, .of.f32 = (float)sample_rate },
        { .kind = WASMTIME_I32, .of.i32 = (int32_t)max_frames_count },
    };
    wasmtime_val_t results[1];
    wasm_trap_t* trap = NULL;
    wasmtime_error_t* err = wasmtime_func_call(ctx, &p->fn_activate,
                                                args, 2, results, 1, &trap);
    if (err) {
        wasm_message_t msg;
        wasmtime_error_message(err, &msg);
        fprintf(stderr, "[wapi_plugin] activate error: %.*s\n", (int)msg.size, msg.data);
        wasm_byte_vec_delete(&msg);
        wasmtime_error_delete(err);
        return false;
    }
    if (trap) {
        wasm_message_t msg;
        wasm_trap_message(trap, &msg);
        fprintf(stderr, "[wapi_plugin] activate trap: %.*s\n", (int)msg.size, msg.data);
        wasm_byte_vec_delete(&msg);
        wasm_trap_delete(trap);
        return false;
    }

    p->activated = true;
    return true;
}

static void plugin_deactivate(const clap_plugin_t* plug) {
    wapi_wasm_plugin_t* p = get_plugin(plug);
    if (!p->activated) return;

    call_wasm_void(p, &p->fn_deactivate);
    p->activated = false;
    p->processing = false;
}

static bool plugin_start_processing(const clap_plugin_t* plug) {
    wapi_wasm_plugin_t* p = get_plugin(plug);
    p->processing = true;
    return true;
}

static void plugin_stop_processing(const clap_plugin_t* plug) {
    wapi_wasm_plugin_t* p = get_plugin(plug);
    p->processing = false;
}

static void plugin_reset(const clap_plugin_t* plug) {
    wapi_wasm_plugin_t* p = get_plugin(plug);
    if (p->activated) {
        call_wasm_void(p, &p->fn_deactivate);
        /* Re-activate with the same parameters */
        wasmtime_context_t* ctx = wasmtime_store_context(p->store);
        wasmtime_val_t args[2] = {
            { .kind = WASMTIME_F32, .of.f32 = p->sample_rate },
            { .kind = WASMTIME_I32, .of.i32 = (int32_t)p->max_block_size },
        };
        wasmtime_val_t results[1];
        wasm_trap_t* trap = NULL;
        wasmtime_error_t* err = wasmtime_func_call(ctx, &p->fn_activate,
                                                    args, 2, results, 1, &trap);
        if (err) wasmtime_error_delete(err);
        if (trap) wasm_trap_delete(trap);
    }
}

static void plugin_on_main_thread(const clap_plugin_t* plug) {
    (void)plug;
    /* Nothing to do on main thread for now */
}

/* ============================================================
 * Process Callback (delegates to wapi_clap_process.c)
 * ============================================================ */

/* Forward declaration from wapi_clap_process.c */
extern clap_process_status plugin_process(const clap_plugin_t* plug,
                                          const clap_process_t* process);

/* ============================================================
 * Audio Ports Extension
 * ============================================================ */

static uint32_t audio_ports_count(const clap_plugin_t* plug, bool is_input) {
    wapi_wasm_plugin_t* p = get_plugin(plug);
    if (is_input) return p->input_channels > 0 ? 1 : 0;
    return p->output_channels > 0 ? 1 : 0;
}

static bool audio_ports_get(const clap_plugin_t* plug, uint32_t index,
                            bool is_input, clap_audio_port_info_t* info) {
    wapi_wasm_plugin_t* p = get_plugin(plug);
    if (index != 0) return false;

    memset(info, 0, sizeof(*info));
    info->id = is_input ? 0 : 1;
    snprintf(info->name, sizeof(info->name), "%s", is_input ? "Input" : "Output");
    info->channel_count = is_input ? p->input_channels : p->output_channels;
    info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    info->port_type = (info->channel_count == 2) ? CLAP_PORT_STEREO
                    : (info->channel_count == 1) ? CLAP_PORT_MONO
                    : NULL;
    info->in_place_pair = CLAP_INVALID_ID;

    return true;
}

static const clap_plugin_audio_ports_t s_audio_ports = {
    .count = audio_ports_count,
    .get = audio_ports_get,
};

/* ============================================================
 * Note Ports Extension (MIDI)
 * ============================================================ */

static uint32_t note_ports_count(const clap_plugin_t* plug, bool is_input) {
    wapi_wasm_plugin_t* p = get_plugin(plug);
    if (is_input && (p->flags & WAPI_PLUGIN_FLAG_SUPPORTS_MIDI)) return 1;
    return 0;
}

static bool note_ports_get(const clap_plugin_t* plug, uint32_t index,
                           bool is_input, clap_note_port_info_t* info) {
    wapi_wasm_plugin_t* p = get_plugin(plug);
    if (index != 0 || !is_input) return false;
    if (!(p->flags & WAPI_PLUGIN_FLAG_SUPPORTS_MIDI)) return false;

    memset(info, 0, sizeof(*info));
    info->id = 0;
    snprintf(info->name, sizeof(info->name), "MIDI In");
    info->supported_dialects = CLAP_NOTE_DIALECT_MIDI;
    info->preferred_dialect = CLAP_NOTE_DIALECT_MIDI;

    return true;
}

static const clap_plugin_note_ports_t s_note_ports = {
    .count = note_ports_count,
    .get = note_ports_get,
};

/* ============================================================
 * Parameters Extension
 * ============================================================ */

static uint32_t params_count(const clap_plugin_t* plug) {
    return get_plugin(plug)->param_count;
}

static bool params_get_info(const clap_plugin_t* plug, uint32_t param_index,
                            clap_param_info_t* info) {
    wapi_wasm_plugin_t* p = get_plugin(plug);
    if (param_index >= p->param_count) return false;

    const wapi_param_info_t* wapi = &p->params[param_index];
    memset(info, 0, sizeof(*info));
    info->id = wapi->id;
    snprintf(info->name, sizeof(info->name), "%s", p->param_names[param_index]);
    snprintf(info->module, sizeof(info->module), "");
    info->default_value = (double)wapi->default_value;
    info->min_value = (double)wapi->min_value;
    info->max_value = (double)wapi->max_value;

    info->flags = 0;
    if (wapi->flags & WAPI_PARAM_FLAG_AUTOMATABLE) {
        info->flags |= CLAP_PARAM_IS_AUTOMATABLE;
    }
    if (wapi->flags & WAPI_PARAM_FLAG_READONLY) {
        info->flags |= CLAP_PARAM_IS_READONLY;
    }

    /* Map WAPI param types to CLAP param behavior */
    switch (wapi->type) {
        case WAPI_PARAM_BOOL:
            info->flags |= CLAP_PARAM_IS_STEPPED;
            info->min_value = 0.0;
            info->max_value = 1.0;
            break;
        case WAPI_PARAM_INT:
        case WAPI_PARAM_ENUM:
            info->flags |= CLAP_PARAM_IS_STEPPED;
            break;
        case WAPI_PARAM_FLOAT:
        default:
            break;
    }

    return true;
}

static bool params_get_value(const clap_plugin_t* plug, clap_id param_id,
                             double* out_value) {
    wapi_wasm_plugin_t* p = get_plugin(plug);
    for (uint32_t i = 0; i < p->param_count; i++) {
        if (p->params[i].id == param_id) {
            *out_value = (double)p->param_values[i];
            return true;
        }
    }
    return false;
}

static bool params_value_to_text(const clap_plugin_t* plug, clap_id param_id,
                                 double value, char* out_buffer,
                                 uint32_t out_buffer_capacity) {
    (void)plug; (void)param_id;
    snprintf(out_buffer, out_buffer_capacity, "%.4f", value);
    return true;
}

static bool params_text_to_value(const clap_plugin_t* plug, clap_id param_id,
                                 const char* param_value_text, double* out_value) {
    (void)plug; (void)param_id;
    char* end;
    double v = strtod(param_value_text, &end);
    if (end == param_value_text) return false;
    *out_value = v;
    return true;
}

static void params_flush(const clap_plugin_t* plug,
                         const clap_input_events_t* in,
                         const clap_output_events_t* out) {
    wapi_wasm_plugin_t* p = get_plugin(plug);

    /* Process incoming parameter changes */
    uint32_t count = in->size(in);
    for (uint32_t i = 0; i < count; i++) {
        const clap_event_header_t* hdr = in->get(in, i);
        if (hdr->space_id != CLAP_CORE_EVENT_SPACE_ID) continue;

        if (hdr->type == CLAP_EVENT_PARAM_VALUE) {
            const clap_event_param_value_t* ev = (const clap_event_param_value_t*)hdr;
            for (uint32_t j = 0; j < p->param_count; j++) {
                if (p->params[j].id == ev->param_id) {
                    p->param_values[j] = (float)ev->value;

                    /* Notify the Wasm module of the parameter change */
                    if (p->activated) {
                        wasmtime_context_t* ctx = wasmtime_store_context(p->store);
                        wasmtime_val_t args[2] = {
                            { .kind = WASMTIME_I32, .of.i32 = (int32_t)ev->param_id },
                            { .kind = WASMTIME_F32, .of.f32 = (float)ev->value },
                        };
                        wasm_trap_t* trap = NULL;
                        wasmtime_error_t* err = wasmtime_func_call(
                            ctx, &p->fn_param_changed, args, 2, NULL, 0, &trap);
                        if (err) wasmtime_error_delete(err);
                        if (trap) wasm_trap_delete(trap);
                    }
                    break;
                }
            }
        }
    }

    /* Push any parameter output events (from Wasm calling wapi_plugin_param_set) */
    for (uint32_t i = 0; i < p->param_count; i++) {
        if (p->param_changed[i]) {
            clap_event_param_value_t ev;
            memset(&ev, 0, sizeof(ev));
            ev.header.size = sizeof(ev);
            ev.header.type = CLAP_EVENT_PARAM_VALUE;
            ev.header.time = 0;
            ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            ev.header.flags = 0;
            ev.param_id = p->params[i].id;
            ev.value = (double)p->param_values[i];
            out->try_push(out, &ev.header);
            p->param_changed[i] = false;
        }
    }
}

static const clap_plugin_params_t s_params = {
    .count = params_count,
    .get_info = params_get_info,
    .get_value = params_get_value,
    .value_to_text = params_value_to_text,
    .text_to_value = params_text_to_value,
    .flush = params_flush,
};

/* ============================================================
 * State Extension
 * ============================================================ */

static bool state_save(const clap_plugin_t* plug, const clap_ostream_t* stream) {
    wapi_wasm_plugin_t* p = get_plugin(plug);

    /* Allocate a buffer in Wasm memory for the state */
    uint32_t buf_size = 65536;  /* 64KB max state */
    wasmtime_context_t* ctx = wasmtime_store_context(p->store);
    size_t mem_size = wasmtime_memory_data_size(ctx, &p->memory);

    uint32_t buf_offset = (uint32_t)(mem_size - buf_size - 128);
    buf_offset &= ~(uint32_t)15;
    uint32_t size_offset = buf_offset - 4;

    if (!wapi_wasm_validate(p, buf_offset, buf_size) ||
        !wapi_wasm_validate(p, size_offset, 4)) {
        return false;
    }

    /* Call wapi_plugin_state_save(buf, buf_len, size_ptr) -> i32 */
    wasmtime_val_t args[3] = {
        { .kind = WASMTIME_I32, .of.i32 = (int32_t)buf_offset },
        { .kind = WASMTIME_I32, .of.i32 = (int32_t)buf_size },
        { .kind = WASMTIME_I32, .of.i32 = (int32_t)size_offset },
    };
    int32_t result = call_wasm_i32_result(p, &p->fn_state_save, args, 3);
    if (WAPI_FAILED(result)) return false;

    /* Read the actual size */
    uint32_t actual_size;
    if (!wapi_wasm_read_u32(p, size_offset, &actual_size)) return false;
    if (actual_size > buf_size) return false;

    /* Write to the CLAP stream */
    void* state_data = wapi_wasm_ptr(p, buf_offset, actual_size);
    if (!state_data) return false;

    int64_t written = stream->write(stream, state_data, actual_size);
    return written == (int64_t)actual_size;
}

static bool state_load(const clap_plugin_t* plug, const clap_istream_t* stream) {
    wapi_wasm_plugin_t* p = get_plugin(plug);

    /* Read the entire state from the CLAP stream into a host buffer */
    uint8_t host_buf[65536];
    size_t total = 0;

    while (total < sizeof(host_buf)) {
        int64_t n = stream->read(stream, host_buf + total, sizeof(host_buf) - total);
        if (n <= 0) break;
        total += (size_t)n;
    }
    if (total == 0) return false;

    /* Write state into Wasm memory */
    wasmtime_context_t* ctx = wasmtime_store_context(p->store);
    size_t mem_size = wasmtime_memory_data_size(ctx, &p->memory);

    uint32_t buf_offset = (uint32_t)(mem_size - total - 128);
    buf_offset &= ~(uint32_t)15;

    if (!wapi_wasm_write_bytes(p, buf_offset, host_buf, (uint32_t)total)) {
        return false;
    }

    /* Call wapi_plugin_state_load(buf, len) -> i32 */
    wasmtime_val_t args[2] = {
        { .kind = WASMTIME_I32, .of.i32 = (int32_t)buf_offset },
        { .kind = WASMTIME_I32, .of.i32 = (int32_t)total },
    };
    int32_t result = call_wasm_i32_result(p, &p->fn_state_load, args, 2);
    return WAPI_SUCCEEDED(result);
}

static const clap_plugin_state_t s_state = {
    .save = state_save,
    .load = state_load,
};

/* ============================================================
 * Extension Dispatch
 * ============================================================ */

static const void* plugin_get_extension(const clap_plugin_t* plug, const char* id) {
    wapi_wasm_plugin_t* p = get_plugin(plug);

    if (strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0)
        return &s_audio_ports;
    if (strcmp(id, CLAP_EXT_NOTE_PORTS) == 0 &&
        (p->flags & WAPI_PLUGIN_FLAG_SUPPORTS_MIDI))
        return &s_note_ports;
    if (strcmp(id, CLAP_EXT_PARAMS) == 0 && p->param_count > 0)
        return &s_params;
    if (strcmp(id, CLAP_EXT_STATE) == 0)
        return &s_state;

    return NULL;
}

/* ============================================================
 * Plugin Creation
 * ============================================================
 * Called from the factory in wapi_clap_entry.c. Allocates both
 * the clap_plugin_t and wapi_wasm_plugin_t, loads the Wasm module.
 */

const clap_plugin_t* wapi_clap_plugin_create(
    const clap_host_t* host,
    const clap_plugin_descriptor_t* desc,
    const char* wasm_path,
    wasm_engine_t* engine)
{
    /* Allocate the CLAP plugin struct */
    clap_plugin_t* plug = (clap_plugin_t*)calloc(1, sizeof(clap_plugin_t));
    if (!plug) return NULL;

    /* Allocate the Wasm plugin state */
    wapi_wasm_plugin_t* p = (wapi_wasm_plugin_t*)calloc(1, sizeof(wapi_wasm_plugin_t));
    if (!p) {
        free(plug);
        return NULL;
    }

    p->engine = engine;

    /* Load the Wasm module (creates store, compiles, instantiates) */
    if (!wapi_wasm_plugin_load(p, wasm_path)) {
        fprintf(stderr, "[wapi_plugin] Failed to load Wasm for instance\n");
        free(p);
        free(plug);
        return NULL;
    }

    /* Wire up the CLAP plugin */
    plug->desc = desc;
    plug->plugin_data = p;
    plug->init = plugin_init;
    plug->destroy = plugin_destroy;
    plug->activate = plugin_activate;
    plug->deactivate = plugin_deactivate;
    plug->start_processing = plugin_start_processing;
    plug->stop_processing = plugin_stop_processing;
    plug->reset = plugin_reset;
    plug->process = plugin_process;
    plug->get_extension = plugin_get_extension;
    plug->on_main_thread = plugin_on_main_thread;

    return plug;
}
