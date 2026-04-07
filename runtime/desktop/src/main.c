/**
 * WAPI Desktop Runtime - Entry Point
 *
 * Loads a .wasm file, instantiates it with all WAPI host imports,
 * calls wapi_main, then runs the frame loop calling wapi_frame.
 *
 * Usage:
 *   wapi_runtime app.wasm [--dir /path/to/assets] [-- app args...]
 */

#include "wapi_host.h"

/* Forward declaration from wapi_host_input.c */
extern void wapi_input_process_sdl_event(const SDL_Event* sdl_ev);

/* ============================================================
 * Global Runtime State
 * ============================================================ */

wapi_runtime_t g_rt = {0};

/* ============================================================
 * Initialization
 * ============================================================ */

static void init_stdio_handles(void) {
    /* Handle 1 = stdin (WAPI_STDIN) */
    g_rt.handles[1].type = WAPI_HTYPE_FILE;
    g_rt.handles[1].data.file = stdin;

    /* Handle 2 = stdout (WAPI_STDOUT) */
    g_rt.handles[2].type = WAPI_HTYPE_FILE;
    g_rt.handles[2].data.file = stdout;

    /* Handle 3 = stderr (WAPI_STDERR) */
    g_rt.handles[3].type = WAPI_HTYPE_FILE;
    g_rt.handles[3].data.file = stderr;

    /* Start allocating user handles after pre-opens */
    g_rt.next_handle = 4; /* WAPI_FS_PREOPEN_BASE */
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
    snprintf(po->host_path, sizeof(po->host_path), "%s", host_path);
    po->handle = handle;

    /* Also store the path in the handle's dir data */
    size_t len = strlen(host_path);
    if (len >= sizeof(g_rt.handles[handle].data.dir.path))
        len = sizeof(g_rt.handles[handle].data.dir.path) - 1;
    memcpy(g_rt.handles[handle].data.dir.path, host_path, len);
    g_rt.handles[handle].data.dir.path[len] = '\0';
}

/* ============================================================
 * Command-Line Parsing
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
            /* Everything after -- is passed to the Wasm module */
            app_args_start = i + 1;
            break;
        }
        if (strcmp(argv[i], "--dir") == 0 && i + 1 < argc) {
            const char* path = argv[++i];
            /* Guest sees the directory by its basename or full path */
            const char* guest = path;
            /* Check for guest:host syntax */
            const char* colon = strchr(path, ':');
            /* On Windows, skip drive letter colons like C: */
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
        if (strcmp(argv[i], "--mapdir") == 0 && i + 1 < argc) {
            /* --mapdir guest::host */
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

    /* Collect app args */
    if (app_args_start >= 0 && app_args_start < argc) {
        result.app_argc = argc - app_args_start;
        result.app_argv = &argv[app_args_start];
    } else {
        /* Pass the wasm filename as argv[0] */
        if (result.wasm_path) {
            result.app_argc = 1;
            result.app_argv = (char**)&result.wasm_path;
        }
    }

    return result;
}

/* ============================================================
 * Wasmtime Setup
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

    if (size <= 0) {
        fclose(f);
        fprintf(stderr, "Error: empty file '%s'\n", path);
        return false;
    }

    wasm_byte_vec_new_uninitialized(out, (size_t)size);
    size_t read = fread(out->data, 1, (size_t)size, f);
    fclose(f);

    if (read != (size_t)size) {
        wasm_byte_vec_delete(out);
        fprintf(stderr, "Error: short read on '%s'\n", path);
        return false;
    }

    return true;
}

static void print_wasmtime_error(const char* context, wasmtime_error_t* error,
                                  wasm_trap_t* trap) {
    wasm_message_t msg;
    if (error) {
        wasmtime_error_message(error, &msg);
        fprintf(stderr, "%s: %.*s\n", context, (int)msg.size, msg.data);
        wasm_byte_vec_delete(&msg);
        wasmtime_error_delete(error);
    }
    if (trap) {
        wasm_trap_message(trap, &msg);
        fprintf(stderr, "%s trap: %.*s\n", context, (int)msg.size, msg.data);
        wasm_byte_vec_delete(&msg);
        wasm_trap_delete(trap);
    }
}

/* ============================================================
 * Main
 * ============================================================ */

int main(int argc, char** argv) {
    /* Parse command line */
    init_stdio_handles();
    cli_args_t cli = parse_args(argc, argv);

    if (!cli.wasm_path) {
        fprintf(stderr,
            "WAPI Desktop Runtime\n"
            "Usage: %s <app.wasm> [--dir <path>]... [-- <app args>...]\n"
            "\n"
            "Options:\n"
            "  --dir <path>          Pre-open a directory for the module\n"
            "  --dir <guest>:<host>  Pre-open with guest path mapping\n"
            "  --mapdir <g>::<h>     Alternative mapping syntax\n"
            "  -- <args>             Arguments passed to the Wasm module\n",
            argv[0]);
        return 1;
    }

    g_rt.app_argc = cli.app_argc;
    g_rt.app_argv = cli.app_argv;

    /* ---- Initialize SDL3 ---- */
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    /* ---- Create Wasmtime engine ---- */
    wasm_engine_t* engine = wasm_engine_new();
    if (!engine) {
        fprintf(stderr, "Failed to create Wasmtime engine\n");
        SDL_Quit();
        return 1;
    }
    g_rt.engine = engine;

    /* ---- Create store ---- */
    wasmtime_store_t* store = wasmtime_store_new(engine, NULL, NULL);
    if (!store) {
        fprintf(stderr, "Failed to create Wasmtime store\n");
        wasm_engine_delete(engine);
        SDL_Quit();
        return 1;
    }
    g_rt.store = store;
    g_rt.context = wasmtime_store_context(store);

    /* ---- Load Wasm module ---- */
    wasm_byte_vec_t wasm_bytes;
    if (!load_wasm_file(cli.wasm_path, &wasm_bytes)) {
        wasmtime_store_delete(store);
        wasm_engine_delete(engine);
        SDL_Quit();
        return 1;
    }

    wasmtime_module_t* module = NULL;
    wasmtime_error_t* error = wasmtime_module_new(engine, (uint8_t*)wasm_bytes.data,
                                                   wasm_bytes.size, &module);
    wasm_byte_vec_delete(&wasm_bytes);

    if (error) {
        print_wasmtime_error("Module compilation", error, NULL);
        wasmtime_store_delete(store);
        wasm_engine_delete(engine);
        SDL_Quit();
        return 1;
    }
    g_rt.module = module;

    /* ---- Create linker and define all WAPI imports ---- */
    wasmtime_linker_t* linker = wasmtime_linker_new(engine);

    wapi_host_register_capability(linker);
    wapi_host_register_env(linker);
    wapi_host_register_memory(linker);
    wapi_host_register_io(linker);
    wapi_host_register_clock(linker);
    wapi_host_register_fs(linker);
    wapi_host_register_net(linker);
    wapi_host_register_surface(linker);
    wapi_host_register_input(linker);
    wapi_host_register_audio(linker);
    wapi_host_register_gpu(linker);
    wapi_host_register_content(linker);
    wapi_host_register_text(linker);
    wapi_host_register_clipboard(linker);
    wapi_host_register_font(linker);
    wapi_host_register_video(linker);
    wapi_host_register_kv(linker);
    wapi_host_register_crypto(linker);
    wapi_host_register_module(linker);
    wapi_host_register_notifications(linker);
    wapi_host_register_geolocation(linker);
    wapi_host_register_sensors(linker);
    wapi_host_register_speech(linker);
    wapi_host_register_biometric(linker);
    wapi_host_register_share(linker);
    wapi_host_register_payments(linker);
    wapi_host_register_usb(linker);
    wapi_host_register_midi(linker);
    wapi_host_register_bluetooth(linker);
    wapi_host_register_camera(linker);
    wapi_host_register_xr(linker);
    wapi_host_register_register(linker);
    wapi_host_register_taskbar(linker);
    wapi_host_register_permissions(linker);
    wapi_host_register_wake_lock(linker);
    wapi_host_register_orientation(linker);
    wapi_host_register_codec(linker);
    wapi_host_register_compression(linker);
    wapi_host_register_media_session(linker);
    wapi_host_register_media_caps(linker);
    wapi_host_register_encoding(linker);
    wapi_host_register_authn(linker);
    wapi_host_register_network_info(linker);
    wapi_host_register_battery(linker);
    wapi_host_register_idle(linker);
    wapi_host_register_haptics(linker);
    wapi_host_register_p2p(linker);
    wapi_host_register_hid(linker);
    wapi_host_register_serial(linker);
    wapi_host_register_screen_capture(linker);
    wapi_host_register_contacts(linker);
    wapi_host_register_barcode(linker);
    wapi_host_register_nfc(linker);
    wapi_host_register_dnd(linker);

    /* ---- Instantiate the module ---- */
    wasm_trap_t* trap = NULL;
    error = wasmtime_linker_instantiate(linker, g_rt.context, module,
                                         &g_rt.instance, &trap);
    if (error || trap) {
        print_wasmtime_error("Instantiation", error, trap);
        wasmtime_linker_delete(linker);
        wasmtime_module_delete(module);
        wasmtime_store_delete(store);
        wasm_engine_delete(engine);
        SDL_Quit();
        return 1;
    }

    /* ---- Find the module's exported memory ---- */
    wasmtime_extern_t memory_extern;
    bool found_memory = wasmtime_instance_export_get(g_rt.context, &g_rt.instance,
                                                      "memory", 6, &memory_extern);
    if (found_memory && memory_extern.kind == WASMTIME_EXTERN_MEMORY) {
        g_rt.memory = memory_extern.of.memory;
        g_rt.memory_valid = true;
    } else {
        fprintf(stderr, "Warning: module does not export 'memory'\n");
    }

    /* ---- Call wapi_main(ctx) ---- */
    wasmtime_extern_t wapi_main_extern;
    bool found_main = wasmtime_instance_export_get(g_rt.context, &g_rt.instance,
                                                    "wapi_main", 9, &wapi_main_extern);
    if (!found_main || wapi_main_extern.kind != WASMTIME_EXTERN_FUNC) {
        fprintf(stderr, "Error: module does not export 'wapi_main'\n");
        wasmtime_linker_delete(linker);
        wasmtime_module_delete(module);
        wasmtime_store_delete(store);
        wasm_engine_delete(engine);
        SDL_Quit();
        return 1;
    }

    /* Write a wapi_context_t into the module's linear memory.
     * Layout (20 bytes):
     *   Offset  0: ptr allocator   (0 = use default wapi_mem_* imports)
     *   Offset  4: ptr io          (ptr to wapi_io_t in linear memory)
     *   Offset  8: ptr panic       (0 = runtime default)
     *   Offset 12: i32 gpu_device  (0 = not yet requested)
     *   Offset 16: u32 flags       (0)
     *
     * The wapi_io_t vtable (24 bytes) is written at offset 1024.
     * The wapi_context_t is written at offset 1048 (1024 + 24).
     *
     * The vtable function pointers are 0 for now — modules fall back
     * to direct "wapi_io" host imports. A full implementation would
     * write wasm table indices for indirect calls, enabling parent
     * modules to wrap the vtable for children.
     */
    uint32_t io_vtable_offset = 1024;
    uint32_t ctx_offset = io_vtable_offset + 24;

    /* Write wapi_io_t vtable (24 bytes, all zeros for now) */
    uint8_t io_vtable[24] = {0};
    wapi_wasm_write_bytes(io_vtable_offset, io_vtable, 24);

    /* Write wapi_context_t (20 bytes) */
    uint8_t ctx_bytes[20] = {0};
    /* ctx->io = pointer to io vtable (offset 4) */
    uint32_t io_ptr = io_vtable_offset;
    memcpy(ctx_bytes + 4, &io_ptr, 4);
    wapi_wasm_write_bytes(ctx_offset, ctx_bytes, 20);

    wasmtime_val_t main_args[1] = {
        { .kind = WASMTIME_I32, .of.i32 = (int32_t)ctx_offset }
    };
    wasmtime_val_t main_results[1];
    error = wasmtime_func_call(g_rt.context, &wapi_main_extern.of.func,
                                main_args, 1, main_results, 1, &trap);
    if (error || trap) {
        /* Check if this was a wapi_env_exit trap (expected) */
        if (trap) {
            wasm_message_t msg;
            wasm_trap_message(trap, &msg);
            bool is_exit = (msg.size >= 11 &&
                           strncmp(msg.data, "wapi_env_exit", 11) == 0);
            wasm_byte_vec_delete(&msg);
            if (is_exit) {
                wasm_trap_delete(trap);
                goto cleanup;
            }
        }
        print_wasmtime_error("wapi_main", error, trap);
        goto cleanup;
    }

    /* Check wapi_main return value */
    if (main_results[0].kind == WASMTIME_I32 && main_results[0].of.i32 < 0) {
        fprintf(stderr, "wapi_main returned error: %d\n", main_results[0].of.i32);
        goto cleanup;
    }

    /* ---- Check for wapi_frame export ---- */
    wasmtime_extern_t wapi_frame_extern;
    g_rt.has_wapi_frame = wasmtime_instance_export_get(g_rt.context, &g_rt.instance,
                                                      "wapi_frame", 8, &wapi_frame_extern);
    if (g_rt.has_wapi_frame && wapi_frame_extern.kind != WASMTIME_EXTERN_FUNC) {
        g_rt.has_wapi_frame = false;
    }

    /* ---- Frame Loop ---- */
    g_rt.running = true;

    if (g_rt.has_wapi_frame) {
        while (g_rt.running) {
            /* Process all pending SDL events */
            SDL_Event sdl_ev;
            while (SDL_PollEvent(&sdl_ev)) {
                wapi_input_process_sdl_event(&sdl_ev);
            }

            /* Get current timestamp in nanoseconds */
            uint64_t timestamp_ns = SDL_GetTicksNS();

            /* Call wapi_frame(timestamp) */
            wasmtime_val_t frame_args[1] = {
                { .kind = WASMTIME_I64, .of.i64 = (int64_t)timestamp_ns }
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
                    if (is_exit) {
                        wasm_trap_delete(trap);
                        break;
                    }
                }
                print_wasmtime_error("wapi_frame", error, trap);
                break;
            }

            /* Check return value: WAPI_ERR_CANCELED means exit */
            if (frame_results[0].kind == WASMTIME_I32 &&
                frame_results[0].of.i32 == WAPI_ERR_CANCELED) {
                break;
            }
        }
    }
    /* If no wapi_frame, wapi_main already ran to completion */

cleanup:
    /* ---- Cleanup ---- */
    wasmtime_linker_delete(linker);
    wasmtime_module_delete(module);
    wasmtime_store_delete(store);
    wasm_engine_delete(engine);

    /* Close all remaining handles */
    for (int i = 4; i < WAPI_MAX_HANDLES; i++) {
        if (g_rt.handles[i].type == WAPI_HTYPE_FILE) {
            fclose(g_rt.handles[i].data.file);
        } else if (g_rt.handles[i].type == WAPI_HTYPE_SURFACE) {
            SDL_DestroyWindow(g_rt.handles[i].data.window);
        } else if (g_rt.handles[i].type == WAPI_HTYPE_AUDIO_DEVICE) {
            SDL_CloseAudioDevice(g_rt.handles[i].data.audio_device_id);
        } else if (g_rt.handles[i].type == WAPI_HTYPE_AUDIO_STREAM) {
            SDL_DestroyAudioStream(g_rt.handles[i].data.audio_stream);
        } else if (g_rt.handles[i].type == WAPI_HTYPE_GPU_DEVICE) {
            wgpuDeviceRelease(g_rt.handles[i].data.gpu_device.device);
            wgpuAdapterRelease(g_rt.handles[i].data.gpu_device.adapter);
            wgpuInstanceRelease(g_rt.handles[i].data.gpu_device.instance);
        } else if (g_rt.handles[i].type == WAPI_HTYPE_GPU_QUEUE) {
            wgpuQueueRelease(g_rt.handles[i].data.gpu_queue);
        } else if (g_rt.handles[i].type == WAPI_HTYPE_GPU_TEXTURE_VIEW) {
            wgpuTextureViewRelease(g_rt.handles[i].data.gpu_texture_view);
        }
    }

    /* Release GPU surfaces */
    for (int i = 0; i < g_rt.gpu_surface_count; i++) {
        if (g_rt.gpu_surfaces[i].wgpu_surface) {
            wgpuSurfaceRelease(g_rt.gpu_surfaces[i].wgpu_surface);
        }
    }

    SDL_Quit();
    return 0;
}
