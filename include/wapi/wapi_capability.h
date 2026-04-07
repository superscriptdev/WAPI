/**
 * WAPI - Capability Enumeration
 * Version 1.0.0
 *
 * Capability detection and enumeration. A module queries which capabilities
 * the host provides at startup. No runtime identification -- only feature
 * detection. The module asks "do you support GPU?" not "are you Chrome?"
 *
 * All capabilities are equal. There is no distinction between "core" and
 * "extension" capabilities. A host supports whatever set of capabilities
 * it chooses. The spec defines capabilities under the "wapi.*" namespace;
 * third-party vendors define their own under "vendor.<name>.*".
 *
 * Capability names use dot-separated namespacing:
 *   "wapi.gpu"                 - Spec-defined capability
 *   "wapi.geolocation"         - Spec-defined capability
 *   "vendor.acme.feature"    - Vendor-defined capability
 *
 * Import module: "wapi"
 */

#ifndef WAPI_CAPABILITY_H
#define WAPI_CAPABILITY_H

#include "wapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Capability Names
 * ============================================================
 * String constants for all spec-defined capabilities.
 * These are the canonical names used with wapi_capability_supported().
 */

#define WAPI_CAP_MEMORY        "wapi.memory"
#define WAPI_CAP_FILESYSTEM    "wapi.filesystem"
#define WAPI_CAP_NETWORK       "wapi.network"
#define WAPI_CAP_CLOCK         "wapi.clock"
#define WAPI_CAP_RANDOM        "wapi.random"
#define WAPI_CAP_GPU           "wapi.gpu"
#define WAPI_CAP_SURFACE       "wapi.surface"
#define WAPI_CAP_INPUT         "wapi.input"
#define WAPI_CAP_AUDIO         "wapi.audio"
#define WAPI_CAP_CONTENT       "wapi.content"
#define WAPI_CAP_CLIPBOARD     "wapi.clipboard"
#define WAPI_CAP_IO            "wapi.io"
#define WAPI_CAP_ENV           "wapi.env"
#define WAPI_CAP_MODULE        "wapi.module"
#define WAPI_CAP_FONT          "wapi.font"
#define WAPI_CAP_VIDEO         "wapi.video"
#define WAPI_CAP_GEOLOCATION   "wapi.geolocation"
#define WAPI_CAP_NOTIFICATIONS "wapi.notifications"
#define WAPI_CAP_SENSORS       "wapi.sensors"
#define WAPI_CAP_SPEECH        "wapi.speech"
#define WAPI_CAP_CRYPTO        "wapi.crypto"
#define WAPI_CAP_BIOMETRIC     "wapi.biometric"
#define WAPI_CAP_SHARE         "wapi.share"
#define WAPI_CAP_KV_STORAGE    "wapi.kv_storage"
#define WAPI_CAP_PAYMENTS      "wapi.payments"
#define WAPI_CAP_USB           "wapi.usb"
#define WAPI_CAP_MIDI          "wapi.midi"
#define WAPI_CAP_BLUETOOTH     "wapi.bluetooth"
#define WAPI_CAP_CAMERA        "wapi.camera"
#define WAPI_CAP_XR            "wapi.xr"
#define WAPI_CAP_AUDIO_PLUGIN  "wapi.audio_plugin"
#define WAPI_CAP_THREAD        "wapi.thread"
#define WAPI_CAP_SYNC          "wapi.sync"
#define WAPI_CAP_PROCESS       "wapi.process"
#define WAPI_CAP_DIALOG        "wapi.dialog"
#define WAPI_CAP_SYSINFO       "wapi.sysinfo"

/* ============================================================
 * Presets
 * ============================================================
 * Presets are recommended bundles that give developers a stable
 * target. A host claims conformance to a preset by supporting
 * all capabilities in it. Presets are just convenience -- the
 * module can always query capabilities individually.
 */

static const char* const WAPI_PRESET_HEADLESS[] = {
    "wapi.memory", "wapi.filesystem", "wapi.network", "wapi.clock",
    "wapi.random", "wapi.io", "wapi.env", "wapi.sysinfo",
    "wapi.thread", "wapi.sync", "wapi.process", NULL
};

static const char* const WAPI_PRESET_COMPUTE[] = {
    "wapi.memory", "wapi.filesystem", "wapi.network", "wapi.clock",
    "wapi.random", "wapi.io", "wapi.env", "wapi.sysinfo",
    "wapi.gpu", "wapi.thread", "wapi.sync", "wapi.process", NULL
};

static const char* const WAPI_PRESET_AUDIO[] = {
    "wapi.memory", "wapi.filesystem", "wapi.network", "wapi.clock",
    "wapi.random", "wapi.io", "wapi.env", "wapi.sysinfo",
    "wapi.audio", "wapi.thread", "wapi.sync", NULL
};

static const char* const WAPI_PRESET_GRAPHICAL[] = {
    "wapi.memory", "wapi.filesystem", "wapi.network", "wapi.clock",
    "wapi.random", "wapi.io", "wapi.env", "wapi.sysinfo",
    "wapi.gpu", "wapi.surface", "wapi.input", "wapi.audio",
    "wapi.content", "wapi.clipboard", "wapi.thread", "wapi.sync",
    "wapi.process", "wapi.dialog", NULL
};

static const char* const WAPI_PRESET_MOBILE[] = {
    "wapi.memory", "wapi.filesystem", "wapi.network", "wapi.clock",
    "wapi.random", "wapi.io", "wapi.env", "wapi.sysinfo",
    "wapi.gpu", "wapi.surface", "wapi.input", "wapi.audio",
    "wapi.content", "wapi.clipboard", "wapi.thread", "wapi.sync",
    "wapi.geolocation", "wapi.camera", "wapi.notifications",
    "wapi.sensors", "wapi.biometric", NULL
};

/* ============================================================
 * Capability Query Functions
 * ============================================================
 * These are the first functions a module calls at startup.
 * The host reports what it provides; the module adapts.
 *
 * All capabilities -- spec-defined and vendor-defined -- use
 * the same query mechanism. No separate "extension" API.
 */

/**
 * Query whether a capability is supported by name.
 *
 * @param name      Capability name (e.g., "wapi.gpu", "vendor.acme.feature").
 * @param name_len  Name length in bytes.
 * @return WAPI_TRUE if the host provides this capability.
 *
 * Wasm signature: (i32, i32) -> i32
 */
WAPI_IMPORT(wapi, capability_supported)
wapi_bool_t wapi_capability_supported(const char* name, wapi_size_t name_len);

/**
 * Get the version of a supported capability.
 *
 * @param name      Capability name.
 * @param name_len  Name length in bytes.
 * @param version   [out] Version struct (major, minor, patch).
 * @return WAPI_OK on success, WAPI_ERR_NOENT if not supported.
 *
 * Wasm signature: (i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi, capability_version)
wapi_result_t wapi_capability_version(const char* name, wapi_size_t name_len,
                                   wapi_version_t* version);

/**
 * Get the number of capabilities the host supports.
 * Use with wapi_capability_name() to enumerate all capabilities.
 *
 * @return Number of supported capabilities.
 *
 * Wasm signature: () -> i32
 */
WAPI_IMPORT(wapi, capability_count)
int32_t wapi_capability_count(void);

/**
 * Get the name of a supported capability by index.
 * For enumerating all capabilities at startup.
 *
 * @param index     Capability index (0 .. wapi_capability_count()-1).
 * @param buf       [out] Buffer to write the name into.
 * @param buf_len   Buffer capacity in bytes.
 * @param name_len  [out] Actual name length in bytes.
 * @return WAPI_OK on success, WAPI_ERR_RANGE if index out of bounds,
 *         WAPI_ERR_NOMEM if buffer too small (name_len still set).
 *
 * Wasm signature: (i32, i32, i32, i32) -> i32
 */
WAPI_IMPORT(wapi, capability_name)
wapi_result_t wapi_capability_name(uint32_t index, char* buf, wapi_size_t buf_len,
                                wapi_size_t* name_len);

/**
 * Get the WAPI version the host implements.
 *
 * Wasm signature: (i32) -> i32
 */
WAPI_IMPORT(wapi, abi_version)
wapi_result_t wapi_abi_version(wapi_version_t* version);

/* ============================================================
 * Convenience: Preset Checking
 * ============================================================ */

/**
 * Check if all capabilities in a NULL-terminated preset array
 * are supported by the host.
 *
 * Usage:
 *   if (wapi_preset_supported(WAPI_PRESET_GRAPHICAL)) { ... }
 */
static inline wapi_bool_t wapi_preset_supported(const char* const* preset) {
    for (int i = 0; preset[i] != NULL; i++) {
        wapi_size_t len = 0;
        const char* s = preset[i];
        while (s[len]) len++;
        if (!wapi_capability_supported(preset[i], len)) return 0;
    }
    return 1;
}

/* ============================================================
 * Module Exports
 * ============================================================
 * The module must export these entry points.
 */

/**
 * Module entry point. Called by the host after instantiation.
 * The host provides a wapi_context_t with allocator, I/O, and
 * any granted device handles. The module should query capabilities
 * and initialize here. Returns WAPI_OK on success, or an error
 * code to abort.
 *
 * Wasm signature: (i32) -> i32
 * Exported as: "wapi_main"
 */
/* WAPI_EXPORT(wapi_main) wapi_result_t wapi_main(const wapi_context_t* ctx); */

/**
 * Called each frame for graphical applications.
 * The host calls this at the display refresh rate.
 * Returns WAPI_OK to continue, WAPI_ERR_CANCELED to request exit.
 *
 * Exported as: "wapi_frame"
 */
/* WAPI_EXPORT(wapi_frame) wapi_result_t wapi_frame(wapi_timestamp_t timestamp); */

#ifdef __cplusplus
}
#endif

#endif /* WAPI_CAPABILITY_H */
