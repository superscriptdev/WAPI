/**
 * WAPI CLAP Plugin Wrapper - Entry Point
 *
 * Exports the CLAP entry point (clap_entry) that DAWs use to discover
 * and instantiate plugins. The wrapper discovers a .wasm binary and
 * bridges between the CLAP plugin API and the WAPI audio plugin ABI.
 *
 * Wasm discovery order:
 *   1. WAPI_PLUGIN_WASM environment variable
 *   2. A .wasm file next to the .clap binary with the same stem name
 */

#include <clap/clap.h>
#include "wapi_plugin_host.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <libgen.h>
#include <unistd.h>
#endif

/* ============================================================
 * Global State
 * ============================================================ */

/* Path to the .wasm file to load */
const char* wapi_plugin_wasm_path = NULL;
static char s_wasm_path_buf[1024];

/* Shared Wasmtime engine (one per .clap library) */
static wasm_engine_t* s_engine = NULL;

/* Prototype plugin instance used to read the descriptor at init time.
 * We load the Wasm module once to read metadata, then unload it.
 * Each real instance gets its own store/module. */
static wapi_wasm_plugin_t s_proto;
static bool s_proto_loaded = false;

/* CLAP plugin descriptor (built from the Wasm module's wapi_plugin_desc_t) */
static clap_plugin_descriptor_t s_clap_desc;
static char s_clap_id[512];    /* "com.vendor.pluginname" */
static char s_clap_name[256];
static char s_clap_vendor[256];
static char s_clap_version[64];
static char s_clap_url[512];
static char s_clap_desc_text[512];

/* Features array (null-terminated) */
static const char* s_features_effect[]     = { "audio-effect", NULL };
static const char* s_features_instrument[] = { "instrument", NULL };
static const char* s_features_analyzer[]   = { "analyzer", NULL };

/* ============================================================
 * Path Discovery
 * ============================================================ */

/* Get the directory containing this .clap shared library */
static bool get_library_dir(char* buf, size_t buf_size) {
#ifdef _WIN32
    HMODULE hm = NULL;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)&get_library_dir, &hm);
    if (!hm) return false;
    DWORD len = GetModuleFileNameA(hm, buf, (DWORD)buf_size);
    if (len == 0 || len >= buf_size) return false;
    /* Strip filename to get directory */
    char* last_sep = strrchr(buf, '\\');
    if (!last_sep) last_sep = strrchr(buf, '/');
    if (last_sep) *last_sep = '\0';
    return true;
#else
    Dl_info info;
    if (!dladdr((void*)&get_library_dir, &info)) return false;
    char* path = realpath(info.dli_fname, NULL);
    if (!path) return false;
    char* dir = dirname(path);
    if (strlen(dir) >= buf_size) { free(path); return false; }
    strncpy(buf, dir, buf_size - 1);
    buf[buf_size - 1] = '\0';
    free(path);
    return true;
#endif
}

/* Get the stem name of this .clap file (without extension) */
static bool get_library_stem(char* buf, size_t buf_size) {
#ifdef _WIN32
    HMODULE hm = NULL;
    char full[1024];
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)&get_library_stem, &hm);
    if (!hm) return false;
    DWORD len = GetModuleFileNameA(hm, full, sizeof(full));
    if (len == 0) return false;
    char* last_sep = strrchr(full, '\\');
    if (!last_sep) last_sep = strrchr(full, '/');
    const char* fname = last_sep ? last_sep + 1 : full;
    strncpy(buf, fname, buf_size - 1);
    buf[buf_size - 1] = '\0';
    /* Remove .clap extension */
    char* dot = strrchr(buf, '.');
    if (dot) *dot = '\0';
    return true;
#else
    Dl_info info;
    if (!dladdr((void*)&get_library_stem, &info)) return false;
    const char* fname = strrchr(info.dli_fname, '/');
    fname = fname ? fname + 1 : info.dli_fname;
    strncpy(buf, fname, buf_size - 1);
    buf[buf_size - 1] = '\0';
    char* dot = strrchr(buf, '.');
    if (dot) *dot = '\0';
    return true;
#endif
}

static bool discover_wasm_path(void) {
    /* Method 1: environment variable */
    const char* env = getenv("WAPI_PLUGIN_WASM");
    if (env && env[0]) {
        strncpy(s_wasm_path_buf, env, sizeof(s_wasm_path_buf) - 1);
        s_wasm_path_buf[sizeof(s_wasm_path_buf) - 1] = '\0';
        wapi_plugin_wasm_path = s_wasm_path_buf;
        return true;
    }

    /* Method 2: .wasm file next to .clap with same stem name */
    char dir[512];
    char stem[256];
    if (get_library_dir(dir, sizeof(dir)) && get_library_stem(stem, sizeof(stem))) {
#ifdef _WIN32
        snprintf(s_wasm_path_buf, sizeof(s_wasm_path_buf), "%s\\%s.wasm", dir, stem);
#else
        snprintf(s_wasm_path_buf, sizeof(s_wasm_path_buf), "%s/%s.wasm", dir, stem);
#endif
        /* Check if the file exists */
        FILE* f = fopen(s_wasm_path_buf, "rb");
        if (f) {
            fclose(f);
            wapi_plugin_wasm_path = s_wasm_path_buf;
            return true;
        }
    }

    fprintf(stderr, "[wapi_plugin] No .wasm file found. "
            "Set WAPI_PLUGIN_WASM or place a .wasm next to the .clap file.\n");
    return false;
}

/* ============================================================
 * Build CLAP Descriptor from Wasm Metadata
 * ============================================================ */

static bool build_clap_descriptor(void) {
    /* Copy metadata from prototype */
    strncpy(s_clap_name, s_proto.name, sizeof(s_clap_name) - 1);
    strncpy(s_clap_vendor, s_proto.vendor, sizeof(s_clap_vendor) - 1);
    strncpy(s_clap_version, s_proto.version, sizeof(s_clap_version) - 1);

    /* Build a CLAP ID from vendor and name: "com.vendor.name" (lowercased) */
    snprintf(s_clap_id, sizeof(s_clap_id), "com.%s.%s", s_proto.vendor, s_proto.name);
    for (char* p = s_clap_id; *p; p++) {
        if (*p == ' ') *p = '-';
        else if (*p >= 'A' && *p <= 'Z') *p = *p + ('a' - 'A');
    }

    snprintf(s_clap_desc_text, sizeof(s_clap_desc_text),
             "WAPI Wasm plugin: %s by %s", s_proto.name, s_proto.vendor);
    snprintf(s_clap_url, sizeof(s_clap_url), "https://thinplatform.org");

    memset(&s_clap_desc, 0, sizeof(s_clap_desc));
    s_clap_desc.clap_version = CLAP_VERSION;
    s_clap_desc.id = s_clap_id;
    s_clap_desc.name = s_clap_name;
    s_clap_desc.vendor = s_clap_vendor;
    s_clap_desc.url = s_clap_url;
    s_clap_desc.manual_url = "";
    s_clap_desc.support_url = "";
    s_clap_desc.version = s_clap_version;
    s_clap_desc.description = s_clap_desc_text;

    /* Features based on category */
    switch (s_proto.category) {
        case WAPI_PLUGIN_INSTRUMENT:
            s_clap_desc.features = s_features_instrument;
            break;
        case WAPI_PLUGIN_ANALYZER:
            s_clap_desc.features = s_features_analyzer;
            break;
        case WAPI_PLUGIN_EFFECT:
        default:
            s_clap_desc.features = s_features_effect;
            break;
    }

    return true;
}

/* ============================================================
 * Forward Declaration: CLAP Plugin Creation
 * ============================================================
 * Defined in wapi_clap_plugin.c -- creates a fully wired clap_plugin_t
 */

extern const clap_plugin_t* wapi_clap_plugin_create(
    const clap_host_t* host,
    const clap_plugin_descriptor_t* desc,
    const char* wasm_path,
    wasm_engine_t* engine);

/* ============================================================
 * CLAP Plugin Factory
 * ============================================================ */

static uint32_t factory_get_plugin_count(const clap_plugin_factory_t* factory) {
    (void)factory;
    return s_proto_loaded ? 1 : 0;
}

static const clap_plugin_descriptor_t* factory_get_plugin_descriptor(
    const clap_plugin_factory_t* factory, uint32_t index)
{
    (void)factory;
    if (index != 0 || !s_proto_loaded) return NULL;
    return &s_clap_desc;
}

static const clap_plugin_t* factory_create_plugin(
    const clap_plugin_factory_t* factory,
    const clap_host_t* host,
    const char* plugin_id)
{
    (void)factory;
    if (!s_proto_loaded) return NULL;
    if (!plugin_id || strcmp(plugin_id, s_clap_desc.id) != 0) return NULL;

    /* Verify host is compatible */
    if (!clap_version_is_compatible(host->clap_version)) {
        fprintf(stderr, "[wapi_plugin] Incompatible CLAP host version\n");
        return NULL;
    }

    return wapi_clap_plugin_create(host, &s_clap_desc, wapi_plugin_wasm_path, s_engine);
}

static const clap_plugin_factory_t s_plugin_factory = {
    .get_plugin_count = factory_get_plugin_count,
    .get_plugin_descriptor = factory_get_plugin_descriptor,
    .create_plugin = factory_create_plugin,
};

/* ============================================================
 * CLAP Entry Point
 * ============================================================ */

static bool entry_init(const char* plugin_path) {
    (void)plugin_path;

    /* Discover the .wasm file */
    if (!discover_wasm_path()) {
        return false;
    }

    /* Create the shared Wasmtime engine */
    s_engine = wasm_engine_new();
    if (!s_engine) {
        fprintf(stderr, "[wapi_plugin] Failed to create Wasmtime engine\n");
        return false;
    }

    /* Load a prototype instance to read the plugin descriptor */
    memset(&s_proto, 0, sizeof(s_proto));
    s_proto.engine = s_engine;

    if (!wapi_wasm_plugin_load(&s_proto, wapi_plugin_wasm_path)) {
        fprintf(stderr, "[wapi_plugin] Failed to load Wasm module: %s\n", wapi_plugin_wasm_path);
        wasm_engine_delete(s_engine);
        s_engine = NULL;
        return false;
    }

    if (!wapi_wasm_plugin_read_desc(&s_proto)) {
        fprintf(stderr, "[wapi_plugin] Failed to read plugin descriptor\n");
        wapi_wasm_plugin_unload(&s_proto);
        wasm_engine_delete(s_engine);
        s_engine = NULL;
        return false;
    }

    if (!wapi_wasm_plugin_read_params(&s_proto)) {
        fprintf(stderr, "[wapi_plugin] Warning: failed to read parameters\n");
        /* Non-fatal: plugin may have zero params */
    }

    s_proto_loaded = true;

    /* Build the CLAP descriptor from the Wasm metadata */
    build_clap_descriptor();

    /* Unload the prototype -- each real instance will load its own */
    wapi_wasm_plugin_unload(&s_proto);

    fprintf(stderr, "[wapi_plugin] Loaded \"%s\" by \"%s\" (v%s) from %s\n",
            s_proto.name, s_proto.vendor, s_proto.version, wapi_plugin_wasm_path);

    return true;
}

static void entry_deinit(void) {
    s_proto_loaded = false;

    if (s_engine) {
        wasm_engine_delete(s_engine);
        s_engine = NULL;
    }
}

static const void* entry_get_factory(const char* factory_id) {
    if (strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0) {
        return &s_plugin_factory;
    }
    return NULL;
}

/* ============================================================
 * CLAP Entry Export
 * ============================================================
 * This is THE symbol that DAWs look for when loading a .clap file.
 */

CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    .clap_version = CLAP_VERSION,
    .init = entry_init,
    .deinit = entry_deinit,
    .get_factory = entry_get_factory,
};
