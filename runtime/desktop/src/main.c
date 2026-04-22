/**
 * WAPI Desktop Runtime - Entry Point
 *
 * Loads a .wasm, instantiates with all WAPI host imports, calls
 * wapi_main, then drives the frame loop calling wapi_frame.
 * Windowing / input / audio / clock go through wapi_plat.h
 * (platform abstraction). No SDL.
 *
 * Usage:
 *   wapi_runtime app.wasm [--dir /path/to/assets] [-- app args...]
 */

#include "wapi_host.h"
#include "wapi_plat.h"
#include <ctype.h>
#include <errno.h>

wapi_runtime_t g_rt = {0};

/* ============================================================
 * Initialization
 * ============================================================ */

static void init_stdio_handles(void) {
    g_rt.handles[1].type = WAPI_HTYPE_FILE; g_rt.handles[1].data.file = stdin;
    g_rt.handles[2].type = WAPI_HTYPE_FILE; g_rt.handles[2].data.file = stdout;
    g_rt.handles[3].type = WAPI_HTYPE_FILE; g_rt.handles[3].data.file = stderr;
    g_rt.next_handle = 4;
}

static void add_preopen(const char* host_path, const char* guest_path) {
    if (g_rt.preopen_count >= WAPI_MAX_PREOPENS) {
        fprintf(stderr, "Warning: too many pre-opened directories\n");
        return;
    }
    int32_t handle = wapi_handle_alloc(WAPI_HTYPE_PREOPEN_DIR);
    if (handle == 0) {
        fprintf(stderr, "Warning: handle table full for pre-open\n");
        return;
    }
    wapi_preopen_t* po = &g_rt.preopens[g_rt.preopen_count++];
    snprintf(po->guest_path, sizeof(po->guest_path), "%s", guest_path);
    snprintf(po->host_path,  sizeof(po->host_path),  "%s", host_path);
    po->handle = handle;

    size_t len = strlen(host_path);
    if (len >= sizeof(g_rt.handles[handle].data.dir.path))
        len = sizeof(g_rt.handles[handle].data.dir.path) - 1;
    memcpy(g_rt.handles[handle].data.dir.path, host_path, len);
    g_rt.handles[handle].data.dir.path[len] = '\0';
}

/* ============================================================
 * CLI
 * ============================================================ */

typedef struct {
    const char* wasm_path;
    int         app_argc;
    char**      app_argv;
} cli_args_t;

static cli_args_t parse_args(int argc, char** argv) {
    cli_args_t result = {0};
    int app_args_start = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) {
            app_args_start = i + 1;
            break;
        }
        if (strcmp(argv[i], "--dir") == 0 && i + 1 < argc) {
            const char* path = argv[++i];
            const char* guest = path;
            const char* colon = strchr(path, ':');
            #ifdef _WIN32
            if (colon && colon == path + 1 && isalpha((unsigned char)path[0])) {
                colon = strchr(colon + 1, ':');
            }
            #endif
            if (colon) {
                static char guest_buf[256];
                size_t glen = (size_t)(colon - path);
                if (glen >= sizeof(guest_buf)) glen = sizeof(guest_buf) - 1;
                memcpy(guest_buf, path, glen);
                guest_buf[glen] = '\0';
                guest = guest_buf;
                path = colon + 1;
            }
            add_preopen(path, guest);
            continue;
        }
        if (strcmp(argv[i], "--module") == 0 && i + 1 < argc) {
            if (!wapi_host_module_cache_add(argv[++i])) {
                fprintf(stderr, "Warning: invalid --module spec (need '<64-hex>=<path>')\n");
            }
            continue;
        }
        if (strcmp(argv[i], "--mapdir") == 0 && i + 1 < argc) {
            const char* mapping = argv[++i];
            const char* sep = strstr(mapping, "::");
            if (sep) {
                static char g_buf[256], h_buf[512];
                size_t glen = (size_t)(sep - mapping);
                if (glen >= sizeof(g_buf)) glen = sizeof(g_buf) - 1;
                memcpy(g_buf, mapping, glen);
                g_buf[glen] = '\0';
                snprintf(h_buf, sizeof(h_buf), "%s", sep + 2);
                add_preopen(h_buf, g_buf);
            }
            continue;
        }
        if (!result.wasm_path && argv[i][0] != '-') {
            result.wasm_path = argv[i];
            continue;
        }
    }

    if (app_args_start >= 0 && app_args_start < argc) {
        result.app_argc = argc - app_args_start;
        result.app_argv = &argv[app_args_start];
    } else if (result.wasm_path) {
        result.app_argc = 1;
        result.app_argv = (char**)&result.wasm_path;
    }

    return result;
}

/* ============================================================
 * Wasmtime Helpers
 * ============================================================ */

static bool load_wasm_file(const char* path, wasm_byte_vec_t* out) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Error: cannot open '%s': %s\n", path, strerror(errno));
        return false;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) { fclose(f); return false; }

    wasm_byte_vec_new_uninitialized(out, (size_t)size);
    size_t read = fread(out->data, 1, (size_t)size, f);
    fclose(f);
    if (read != (size_t)size) { wasm_byte_vec_delete(out); return false; }
    return true;
}

static void print_wasmtime_error(const char* ctx, wasmtime_error_t* err, wasm_trap_t* trap) {
    wasm_message_t msg;
    if (err) {
        wasmtime_error_message(err, &msg);
        fprintf(stderr, "%s: %.*s\n", ctx, (int)msg.size, msg.data);
        wasm_byte_vec_delete(&msg);
        wasmtime_error_delete(err);
    }
    if (trap) {
        wasm_trap_message(trap, &msg);
        fprintf(stderr, "%s trap: %.*s\n", ctx, (int)msg.size, msg.data);
        wasm_byte_vec_delete(&msg);
        wasm_trap_delete(trap);
    }
}

/* ============================================================
 * Main
 * ============================================================ */

int main(int argc, char** argv) {
    init_stdio_handles();
    cli_args_t cli = parse_args(argc, argv);

    if (!cli.wasm_path) {
        fprintf(stderr,
            "WAPI Desktop Runtime\n"
            "Usage: %s <app.wasm> [--dir <path>]... [-- <app args>...]\n",
            argv[0]);
        return 1;
    }

    g_rt.app_argc = cli.app_argc;
    g_rt.app_argv = cli.app_argv;

    if (!wapi_plat_init()) {
        fprintf(stderr, "wapi_plat_init failed\n");
        return 1;
    }

    wasm_engine_t* engine = wasm_engine_new();
    if (!engine) { fprintf(stderr, "wasm_engine_new failed\n"); wapi_plat_shutdown(); return 1; }
    g_rt.engine = engine;

    wasmtime_store_t* store = wasmtime_store_new(engine, NULL, NULL);
    if (!store) { wasm_engine_delete(engine); wapi_plat_shutdown(); return 1; }
    g_rt.store = store;
    g_rt.context = wasmtime_store_context(store);

    wasm_byte_vec_t wasm_bytes;
    if (!load_wasm_file(cli.wasm_path, &wasm_bytes)) {
        wasmtime_store_delete(store);
        wasm_engine_delete(engine);
        wapi_plat_shutdown();
        return 1;
    }

    wasmtime_module_t* module = NULL;
    wasmtime_error_t* error = wasmtime_module_new(engine, (uint8_t*)wasm_bytes.data,
                                                  wasm_bytes.size, &module);
    wasm_byte_vec_delete(&wasm_bytes);
    if (error) {
        print_wasmtime_error("Module compilation", error, NULL);
        wasmtime_store_delete(store); wasm_engine_delete(engine); wapi_plat_shutdown();
        return 1;
    }
    g_rt.module = module;

    wasmtime_linker_t* linker = wasmtime_linker_new(engine);
    g_rt.linker = linker;

    wapi_host_register_core(linker);
    wapi_host_register_env(linker);
    wapi_host_register_random(linker);
    wapi_host_register_memory(linker);
    wapi_host_register_io(linker);
    wapi_host_register_clock(linker);
    wapi_host_register_fs(linker);
    wapi_host_register_net(linker);
    wapi_host_register_surface(linker);
    wapi_host_register_window(linker);
    wapi_host_register_input(linker);
    wapi_host_input_init();
    wapi_gamepaddb_load();
    wapi_devicedb_load();
    wapi_host_register_audio(linker);
    wapi_host_register_wgpu(linker);
    wapi_host_register_text(linker);
    wapi_host_register_transfer(linker);
    wapi_host_register_seat(linker);
    wapi_host_register_font(linker);
    wapi_host_register_video(linker);
    wapi_host_register_kv(linker);
    wapi_host_register_crypto(linker);
    wapi_host_register_module(linker);
    wapi_host_register_notifications(linker);
    wapi_host_register_permissions(linker);
    wapi_host_register_menu(linker);
    wapi_host_register_tray(linker);
    wapi_host_register_taskbar(linker);
    wapi_host_register_sysinfo(linker);
    wapi_host_register_display(linker);
    wapi_host_register_theme(linker);
    wapi_host_register_process(linker);

    wasm_trap_t* trap = NULL;
    error = wasmtime_linker_instantiate(linker, g_rt.context, module, &g_rt.instance, &trap);
    if (error || trap) {
        print_wasmtime_error("Instantiation", error, trap);
        goto cleanup;
    }

    /* Exported memory */
    wasmtime_extern_t memory_extern;
    if (wasmtime_instance_export_get(g_rt.context, &g_rt.instance,
                                     "memory", 6, &memory_extern) &&
        memory_extern.kind == WASMTIME_EXTERN_MEMORY) {
        g_rt.memory = memory_extern.of.memory;
        g_rt.memory_valid = true;
    } else {
        fprintf(stderr, "Warning: module does not export 'memory'\n");
    }

    /* Exported indirect function table (for guest callbacks) */
    wasmtime_extern_t table_extern;
    if (wasmtime_instance_export_get(g_rt.context, &g_rt.instance,
                                     "__indirect_function_table",
                                     strlen("__indirect_function_table"),
                                     &table_extern) &&
        table_extern.kind == WASMTIME_EXTERN_TABLE) {
        g_rt.indirect_table = table_extern.of.table;
        g_rt.indirect_table_valid = true;
    }

    /* wapi_main */
    wasmtime_extern_t wapi_main_extern;
    if (!wasmtime_instance_export_get(g_rt.context, &g_rt.instance,
                                      "wapi_main", 9, &wapi_main_extern) ||
        wapi_main_extern.kind != WASMTIME_EXTERN_FUNC) {
        fprintf(stderr, "Error: module does not export 'wapi_main'\n");
        goto cleanup;
    }

    wasmtime_val_t main_results[1];
    error = wasmtime_func_call(g_rt.context, &wapi_main_extern.of.func,
                               NULL, 0, main_results, 1, &trap);
    if (error || trap) {
        if (trap) {
            wasm_message_t msg;
            wasm_trap_message(trap, &msg);
            bool is_exit = (msg.size >= 11 &&
                            strncmp(msg.data, "wapi_env_exit", 11) == 0);
            wasm_byte_vec_delete(&msg);
            if (is_exit) { wasm_trap_delete(trap); goto cleanup; }
        }
        print_wasmtime_error("wapi_main", error, trap);
        goto cleanup;
    }

    if (main_results[0].kind == WASMTIME_I32 && main_results[0].of.i32 < 0) {
        fprintf(stderr, "wapi_main returned error: %d\n", main_results[0].of.i32);
        goto cleanup;
    }

    /* wapi_frame loop */
    wasmtime_extern_t wapi_frame_extern;
    g_rt.has_wapi_frame = wasmtime_instance_export_get(g_rt.context, &g_rt.instance,
                                                        "wapi_frame", 10, &wapi_frame_extern);
    if (g_rt.has_wapi_frame && wapi_frame_extern.kind != WASMTIME_EXTERN_FUNC) {
        g_rt.has_wapi_frame = false;
    }

    g_rt.running = true;

    if (g_rt.has_wapi_frame) {
        while (g_rt.running) {
            /* Drain platform events, translating onto host queue. */
            wapi_host_pump_platform_events();

            uint64_t ts = wapi_plat_time_monotonic_ns();
            wasmtime_val_t frame_args[1] = {
                { .kind = WASMTIME_I64, .of.i64 = (int64_t)ts }
            };
            wasmtime_val_t frame_results[1];
            error = wasmtime_func_call(g_rt.context, &wapi_frame_extern.of.func,
                                       frame_args, 1, frame_results, 1, &trap);
            if (error || trap) {
                if (trap) {
                    wasm_message_t msg;
                    wasm_trap_message(trap, &msg);
                    bool is_exit = (msg.size >= 11 &&
                                    strncmp(msg.data, "wapi_env_exit", 11) == 0);
                    wasm_byte_vec_delete(&msg);
                    if (is_exit) { wasm_trap_delete(trap); break; }
                }
                print_wasmtime_error("wapi_frame", error, trap);
                break;
            }

            if (frame_results[0].kind == WASMTIME_I32 &&
                frame_results[0].of.i32 == WAPI_ERR_CANCELED) {
                break;
            }
        }
    }

cleanup:
    wasmtime_linker_delete(linker);
    wasmtime_module_delete(module);
    wasmtime_store_delete(store);
    wasm_engine_delete(engine);

    /* Close remaining handles */
    for (int i = 4; i < WAPI_MAX_HANDLES; i++) {
        switch (g_rt.handles[i].type) {
        case WAPI_HTYPE_FILE:
            fclose(g_rt.handles[i].data.file);
            break;
        case WAPI_HTYPE_SURFACE:
            wapi_plat_window_destroy(g_rt.handles[i].data.window);
            break;
        case WAPI_HTYPE_AUDIO_DEVICE:
            wapi_plat_audio_close_device(g_rt.handles[i].data.audio_device);
            break;
        case WAPI_HTYPE_AUDIO_STREAM:
            wapi_plat_audio_stream_destroy(g_rt.handles[i].data.audio_stream);
            break;
        case WAPI_HTYPE_GPU_INSTANCE:
            wgpuInstanceRelease(g_rt.handles[i].data.gpu_instance);
            break;
        case WAPI_HTYPE_GPU_ADAPTER:
            wgpuAdapterRelease(g_rt.handles[i].data.gpu_adapter);
            break;
        case WAPI_HTYPE_GPU_DEVICE:
            wgpuDeviceRelease(g_rt.handles[i].data.gpu_device);
            break;
        case WAPI_HTYPE_GPU_QUEUE:
            wgpuQueueRelease(g_rt.handles[i].data.gpu_queue);
            break;
        case WAPI_HTYPE_GPU_SURFACE:
            wgpuSurfaceRelease(g_rt.handles[i].data.gpu_surface);
            break;
        case WAPI_HTYPE_GPU_TEXTURE:
            wgpuTextureRelease(g_rt.handles[i].data.gpu_texture);
            break;
        case WAPI_HTYPE_GPU_TEXTURE_VIEW:
            wgpuTextureViewRelease(g_rt.handles[i].data.gpu_texture_view);
            break;
        case WAPI_HTYPE_GPU_BUFFER:
            wgpuBufferRelease(g_rt.handles[i].data.gpu_buffer);
            break;
        case WAPI_HTYPE_GPU_SAMPLER:
            wgpuSamplerRelease(g_rt.handles[i].data.gpu_sampler);
            break;
        case WAPI_HTYPE_GPU_BIND_GROUP:
            wgpuBindGroupRelease(g_rt.handles[i].data.gpu_bind_group);
            break;
        case WAPI_HTYPE_GPU_BIND_GROUP_LAYOUT:
            wgpuBindGroupLayoutRelease(g_rt.handles[i].data.gpu_bind_group_layout);
            break;
        case WAPI_HTYPE_GPU_PIPELINE_LAYOUT:
            wgpuPipelineLayoutRelease(g_rt.handles[i].data.gpu_pipeline_layout);
            break;
        case WAPI_HTYPE_GPU_SHADER_MODULE:
            wgpuShaderModuleRelease(g_rt.handles[i].data.gpu_shader_module);
            break;
        case WAPI_HTYPE_GPU_RENDER_PIPELINE:
            wgpuRenderPipelineRelease(g_rt.handles[i].data.gpu_render_pipeline);
            break;
        case WAPI_HTYPE_GPU_COMMAND_ENCODER:
            wgpuCommandEncoderRelease(g_rt.handles[i].data.gpu_command_encoder);
            break;
        case WAPI_HTYPE_GPU_COMMAND_BUFFER:
            wgpuCommandBufferRelease(g_rt.handles[i].data.gpu_command_buffer);
            break;
        case WAPI_HTYPE_GPU_RENDER_PASS:
            wgpuRenderPassEncoderRelease(g_rt.handles[i].data.gpu_render_pass);
            break;
        default: break;
        }
    }

    wapi_plat_shutdown();
    return 0;
}
