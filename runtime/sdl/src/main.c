/**
 * WAPI SDL Runtime - Entry Point
 *
 * Loads a .wasm file, instantiates it with all WAPI host imports,
 * calls wapi_main, then runs the frame loop calling wapi_frame.
 *
 *   wapi_sdl_runtime app.wasm [--dir <path>]... [-- <app args>...]
 *
 * Backend: SDL3 + Dawn (WebGPU) + WAMR. Capabilities that SDL3 cannot
 * back are registered as stubs returning WAPI_ERR_NOTCAPABLE.
 */

#include "wapi_host.h"
#include <ctype.h>

wapi_runtime_t g_rt = {0};

/* ============================================================
 * Initialization
 * ============================================================ */

static void init_stdio_handles(void) {
    g_rt.handles[1].type = WAPI_HTYPE_FILE;
    g_rt.handles[1].data.file = stdin;

    g_rt.handles[2].type = WAPI_HTYPE_FILE;
    g_rt.handles[2].data.file = stdout;

    g_rt.handles[3].type = WAPI_HTYPE_FILE;
    g_rt.handles[3].data.file = stderr;

    g_rt.next_handle = 4;
}

static void add_preopen(const char* host_path, const char* guest_path) {
    if (g_rt.preopen_count >= WAPI_MAX_PREOPENS) {
        fprintf(stderr, "warning: too many pre-opened directories\n");
        return;
    }
    int32_t handle = wapi_handle_alloc(WAPI_HTYPE_PREOPEN_DIR);
    if (handle == 0) {
        fprintf(stderr, "warning: handle table full for pre-open\n");
        return;
    }
    wapi_preopen_t* po = &g_rt.preopens[g_rt.preopen_count++];
    snprintf(po->guest_path, sizeof(po->guest_path), "%s", guest_path);
    snprintf(po->host_path, sizeof(po->host_path), "%s", host_path);
    po->handle = handle;

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
 * File loader
 * ============================================================ */

static uint8_t* load_wasm_file(const char* path, size_t* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "error: cannot open '%s': %s\n", path, strerror(errno));
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        fprintf(stderr, "error: empty file '%s'\n", path);
        return NULL;
    }
    uint8_t* buf = (uint8_t*)malloc((size_t)size);
    if (!buf) { fclose(f); return NULL; }
    size_t read = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (read != (size_t)size) {
        free(buf);
        fprintf(stderr, "error: short read on '%s'\n", path);
        return NULL;
    }
    *out_size = (size_t)size;
    return buf;
}

/* ============================================================
 * Native symbol registration
 * ============================================================
 * Each wapi_host_*.c returns its NativeSymbol array. We register all
 * of them before instantiation so the module's imports resolve.
 */

static bool register_all_natives(void) {
    wapi_cap_registration_t regs[] = {
        wapi_host_capability_registration(),
        wapi_host_env_registration(),
        wapi_host_memory_registration(),
        wapi_host_io_registration(),
        wapi_host_clock_registration(),
        wapi_host_fs_registration(),
        wapi_host_surface_registration(),
        wapi_host_window_registration(),
        wapi_host_display_registration(),
        wapi_host_input_registration(),
        wapi_host_audio_registration(),
        wapi_host_haptics_registration(),
        wapi_host_power_registration(),
        wapi_host_sensors_registration(),
        wapi_host_transfer_registration(),
        wapi_host_seat_registration(),
        wapi_host_input_seat_registration(),
        wapi_host_gpu_registration(),
    };
    const int n = (int)(sizeof(regs) / sizeof(regs[0]));
    for (int i = 0; i < n; i++) {
        if (!wasm_runtime_register_natives(regs[i].module_name,
                                           regs[i].symbols,
                                           regs[i].count)) {
            fprintf(stderr, "error: failed to register natives for '%s'\n",
                    regs[i].module_name);
            return false;
        }
    }
    return true;
}

/* ============================================================
 * Main
 * ============================================================ */

#define WASM_ERROR_BUF 256
#define WASM_STACK_SIZE (64 * 1024)
#define WASM_HEAP_SIZE  (512 * 1024)

int main(int argc, char** argv) {
    init_stdio_handles();
    cli_args_t cli = parse_args(argc, argv);

    if (!cli.wasm_path) {
        fprintf(stderr,
            "WAPI SDL Runtime\n"
            "usage: %s <app.wasm> [--dir <path>]... [-- <app args>...]\n"
            "\n"
            "options:\n"
            "  --dir <path>          pre-open a directory for the module\n"
            "  --dir <guest>:<host>  pre-open with guest path mapping\n"
            "  --mapdir <g>::<h>     alternative mapping syntax\n"
            "  -- <args>             arguments passed to the Wasm module\n",
            argv[0]);
        return 1;
    }

    g_rt.app_argc = cli.app_argc;
    g_rt.app_argv = cli.app_argv;

    /* ---- SDL3 init ---- */
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO |
                  SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC | SDL_INIT_SENSOR)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    /* ---- WAMR init ---- */
    RuntimeInitArgs init_args = {0};
    init_args.mem_alloc_type = Alloc_With_System_Allocator;
    if (!wasm_runtime_full_init(&init_args)) {
        fprintf(stderr, "wasm_runtime_full_init failed\n");
        SDL_Quit();
        return 1;
    }

    /* ---- Register all host natives BEFORE loading the module ---- */
    if (!register_all_natives()) {
        wasm_runtime_destroy();
        SDL_Quit();
        return 1;
    }

    /* ---- Load .wasm bytes ---- */
    size_t wasm_size = 0;
    uint8_t* wasm_bytes = load_wasm_file(cli.wasm_path, &wasm_size);
    if (!wasm_bytes) {
        wasm_runtime_destroy();
        SDL_Quit();
        return 1;
    }
    g_rt.wasm_bytes = wasm_bytes;
    g_rt.wasm_size  = wasm_size;

    /* ---- Load module ---- */
    char err[WASM_ERROR_BUF] = {0};
    g_rt.module = wasm_runtime_load(wasm_bytes, (uint32_t)wasm_size,
                                    err, WASM_ERROR_BUF);
    if (!g_rt.module) {
        fprintf(stderr, "wasm_runtime_load: %s\n", err);
        free(wasm_bytes);
        wasm_runtime_destroy();
        SDL_Quit();
        return 1;
    }

    /* ---- Instantiate ---- */
    g_rt.module_inst = wasm_runtime_instantiate(g_rt.module,
                                                WASM_STACK_SIZE,
                                                WASM_HEAP_SIZE,
                                                err, WASM_ERROR_BUF);
    if (!g_rt.module_inst) {
        fprintf(stderr, "wasm_runtime_instantiate: %s\n", err);
        wasm_runtime_unload(g_rt.module);
        free(wasm_bytes);
        wasm_runtime_destroy();
        SDL_Quit();
        return 1;
    }

    /* ---- Create exec env ---- */
    g_rt.exec_env = wasm_runtime_create_exec_env(g_rt.module_inst,
                                                 WASM_STACK_SIZE);
    if (!g_rt.exec_env) {
        fprintf(stderr, "wasm_runtime_create_exec_env failed\n");
        goto cleanup_inst;
    }

    /* ---- Resolve wapi_main ---- */
    wasm_function_inst_t fn_main =
        wasm_runtime_lookup_function(g_rt.module_inst, "wapi_main");
    if (!fn_main) {
        fprintf(stderr, "error: module does not export 'wapi_main'\n");
        goto cleanup_env;
    }

    /* ---- Call wapi_main() -> i32 ---- */
    uint32_t main_results[1] = {0};
    if (!wasm_runtime_call_wasm(g_rt.exec_env, fn_main, 0, main_results)) {
        const char* ex = wasm_runtime_get_exception(g_rt.module_inst);
        if (ex) {
            /* A wapi_env_exit trap is expected; clear and exit cleanly. */
            if (strstr(ex, "wapi_env_exit") != NULL) {
                goto cleanup_env;
            }
            fprintf(stderr, "wapi_main: %s\n", ex);
        }
        goto cleanup_env;
    }
    if ((int32_t)main_results[0] < 0) {
        fprintf(stderr, "wapi_main returned error: %d\n",
                (int32_t)main_results[0]);
        goto cleanup_env;
    }

    /* ---- Optional wapi_frame(timestamp_ns) -> i32 ---- */
    wasm_function_inst_t fn_frame =
        wasm_runtime_lookup_function(g_rt.module_inst, "wapi_frame");
    g_rt.has_wapi_frame = (fn_frame != NULL);

    g_rt.running = true;
    if (fn_frame) {
        while (g_rt.running) {
            SDL_Event sdl_ev;
            while (SDL_PollEvent(&sdl_ev)) {
                wapi_input_process_sdl_event(&sdl_ev);
            }

            uint64_t ts = SDL_GetTicksNS();

            /* i64 is passed as two u32 words in WAMR's call_wasm. */
            uint32_t frame_args[2];
            frame_args[0] = (uint32_t)(ts & 0xFFFFFFFFu);
            frame_args[1] = (uint32_t)(ts >> 32);
            uint32_t frame_results[1] = {0};

            if (!wasm_runtime_call_wasm(g_rt.exec_env, fn_frame,
                                        2, frame_args)) {
                const char* ex = wasm_runtime_get_exception(g_rt.module_inst);
                if (ex && strstr(ex, "wapi_env_exit") != NULL) {
                    break;
                }
                if (ex) fprintf(stderr, "wapi_frame: %s\n", ex);
                break;
            }
            /* wapi_frame returns i32 in results[0]. WAPI_ERR_CANCELED exits. */
            (void)frame_results;
        }
    }

cleanup_env:
    wasm_runtime_destroy_exec_env(g_rt.exec_env);
cleanup_inst:
    /* Close any handles we own */
    for (int i = 4; i < WAPI_MAX_HANDLES; i++) {
        switch (g_rt.handles[i].type) {
            case WAPI_HTYPE_FILE:
                fclose(g_rt.handles[i].data.file); break;
            case WAPI_HTYPE_SURFACE:
                SDL_DestroyWindow(g_rt.handles[i].data.window); break;
            case WAPI_HTYPE_AUDIO_DEVICE:
                SDL_CloseAudioDevice(g_rt.handles[i].data.audio_device_id); break;
            case WAPI_HTYPE_AUDIO_STREAM:
                SDL_DestroyAudioStream(g_rt.handles[i].data.audio_stream); break;
            case WAPI_HTYPE_HAPTIC:
                SDL_CloseHaptic(g_rt.handles[i].data.haptic); break;
            case WAPI_HTYPE_SENSOR:
                SDL_CloseSensor(g_rt.handles[i].data.sensor); break;
            case WAPI_HTYPE_GPU_DEVICE:
                wgpuDeviceRelease(g_rt.handles[i].data.gpu_device.device);
                wgpuAdapterRelease(g_rt.handles[i].data.gpu_device.adapter);
                break;
            case WAPI_HTYPE_GPU_QUEUE:
                wgpuQueueRelease(g_rt.handles[i].data.gpu_queue); break;
            case WAPI_HTYPE_GPU_TEXTURE_VIEW:
                wgpuTextureViewRelease(g_rt.handles[i].data.gpu_texture_view); break;
            default: break;
        }
    }
    for (int i = 0; i < g_rt.gpu_surface_count; i++) {
        if (g_rt.gpu_surfaces[i].wgpu_surface) {
            wgpuSurfaceRelease(g_rt.gpu_surfaces[i].wgpu_surface);
        }
    }
    if (g_rt.wgpu_instance) wgpuInstanceRelease(g_rt.wgpu_instance);

    wasm_runtime_deinstantiate(g_rt.module_inst);
    wasm_runtime_unload(g_rt.module);
    free(wasm_bytes);
    wasm_runtime_destroy();
    SDL_Quit();
    return 0;
}
