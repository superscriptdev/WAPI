/**
 * WAPI - Audio Plugin Capability
 * Version 1.0.0
 *
 * Enables Wasm modules to function as audio plugins (VST3-equivalent).
 * A DAW or audio host implements this capability to load Wasm audio
 * plugins with the same sandboxing guarantees as web content.
 *
 * The plugin receives audio buffers, processes them, and returns
 * the result. The host provides parameter automation, MIDI events,
 * transport state, and optionally a GUI surface.
 *
 * This replaces the need for VST3/AU/LV2/CLAP format-specific builds.
 * One Wasm binary works in any DAW that implements this capability.
 *
 * Import module: "wapi_plugin"
 *
 * Query availability with wapi_capability_supported("wapi.audio_plugin", 15)
 */

#ifndef WAPI_AUDIO_PLUGIN_H
#define WAPI_AUDIO_PLUGIN_H

#include "wapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Plugin Categories
 * ============================================================ */

typedef enum wapi_plugin_category_t {
    WAPI_PLUGIN_EFFECT       = 0,  /* Audio effect (insert/send) */
    WAPI_PLUGIN_INSTRUMENT   = 1,  /* Virtual instrument (generates audio) */
    WAPI_PLUGIN_ANALYZER     = 2,  /* Analysis only (no audio output) */
    WAPI_PLUGIN_FORCE32      = 0x7FFFFFFF
} wapi_plugin_category_t;

/* ============================================================
 * Plugin Descriptor
 * ============================================================
 * Exported by the plugin module at init time.
 */

typedef struct wapi_plugin_desc_t {
    const char* name;
    wapi_size_t   name_len;
    const char* vendor;
    wapi_size_t   vendor_len;
    const char* version;
    wapi_size_t   version_len;
    uint32_t    category;
    uint32_t    default_input_channels;   /* 0 for instruments */
    uint32_t    default_output_channels;  /* Typically 2 for stereo */
    uint32_t    flags;
} wapi_plugin_desc_t;

#define WAPI_PLUGIN_FLAG_HAS_GUI         0x0001
#define WAPI_PLUGIN_FLAG_SUPPORTS_MIDI   0x0002
#define WAPI_PLUGIN_FLAG_SUPPORTS_SIDECHAIN 0x0004

/* ============================================================
 * Parameters
 * ============================================================ */

typedef enum wapi_param_type_t {
    WAPI_PARAM_FLOAT     = 0,  /* Continuous 0.0-1.0 (normalized) */
    WAPI_PARAM_INT       = 1,  /* Integer with min/max range */
    WAPI_PARAM_BOOL      = 2,  /* On/off toggle */
    WAPI_PARAM_ENUM      = 3,  /* Discrete choices */
    WAPI_PARAM_FORCE32   = 0x7FFFFFFF
} wapi_param_type_t;

typedef struct wapi_param_info_t {
    uint32_t    id;
    const char* name;
    wapi_size_t   name_len;
    uint32_t    type;          /* wapi_param_type_t */
    float       default_value; /* Normalized 0.0-1.0 */
    float       min_value;     /* For INT type */
    float       max_value;     /* For INT type */
    uint32_t    flags;
} wapi_param_info_t;

#define WAPI_PARAM_FLAG_AUTOMATABLE  0x0001
#define WAPI_PARAM_FLAG_READONLY     0x0002

/* ============================================================
 * Transport State
 * ============================================================
 * Provided by the host each process call.
 */

typedef struct wapi_transport_t {
    double   tempo;           /* BPM */
    double   beat_position;   /* Current position in beats */
    int32_t  time_sig_num;    /* Time signature numerator */
    int32_t  time_sig_denom;  /* Time signature denominator */
    int32_t  sample_pos;      /* Current sample position */
    uint32_t flags;
} wapi_transport_t;

#define WAPI_TRANSPORT_PLAYING    0x0001
#define WAPI_TRANSPORT_RECORDING  0x0002
#define WAPI_TRANSPORT_LOOPING    0x0004

/* ============================================================
 * MIDI Event (simplified)
 * ============================================================ */

typedef struct wapi_midi_event_t {
    uint32_t    sample_offset;  /* Sample offset within the buffer */
    uint8_t     status;         /* MIDI status byte */
    uint8_t     data1;          /* First data byte (note, CC#) */
    uint8_t     data2;          /* Second data byte (velocity, value) */
    uint8_t     _pad;
} wapi_midi_event_t;

/* ============================================================
 * Process Data
 * ============================================================
 * Passed to the plugin's process function each audio block.
 */

typedef struct wapi_process_data_t {
    float**                  inputs;        /* Array of input channel buffers */
    float**                  outputs;       /* Array of output channel buffers */
    uint32_t                 num_inputs;
    uint32_t                 num_outputs;
    uint32_t                 num_samples;   /* Buffer size */
    float                    sample_rate;
    const wapi_transport_t*    transport;
    const wapi_midi_event_t*   midi_events;
    uint32_t                 midi_event_count;
} wapi_process_data_t;

/* ============================================================
 * Host Functions (called by plugin)
 * ============================================================ */

/**
 * Get the number of parameters the host expects.
 */
WAPI_IMPORT(wapi_plugin, param_count)
int32_t wapi_plugin_param_count(void);

/**
 * Report a parameter change to the host (from GUI interaction).
 *
 * @param param_id  Parameter ID.
 * @param value     Normalized value 0.0-1.0.
 */
WAPI_IMPORT(wapi_plugin, param_set)
wapi_result_t wapi_plugin_param_set(uint32_t param_id, float value);

/**
 * Get the current value of a parameter (set by host automation).
 *
 * @param param_id  Parameter ID.
 * @return Normalized value 0.0-1.0.
 */
WAPI_IMPORT(wapi_plugin, param_get)
float wapi_plugin_param_get(uint32_t param_id);

/**
 * Request the host to resize the plugin GUI.
 */
WAPI_IMPORT(wapi_plugin, request_gui_resize)
wapi_result_t wapi_plugin_request_gui_resize(int32_t width, int32_t height);

/**
 * Send a MIDI message to the host (for instruments that generate MIDI).
 */
WAPI_IMPORT(wapi_plugin, send_midi)
wapi_result_t wapi_plugin_send_midi(uint8_t status, uint8_t data1, uint8_t data2);

/* ============================================================
 * Plugin Exports (implemented by the plugin module)
 * ============================================================
 * The plugin module MUST export these functions:
 *
 * wapi_plugin_get_desc(wapi_plugin_desc_t* desc) -> wapi_result_t
 *     Called once at load time to get plugin metadata.
 *
 * wapi_plugin_get_param_info(uint32_t index, wapi_param_info_t* info) -> wapi_result_t
 *     Called for each parameter to get its metadata.
 *
 * wapi_plugin_activate(float sample_rate, uint32_t max_block_size) -> wapi_result_t
 *     Called when the plugin is about to start processing.
 *
 * wapi_plugin_deactivate() -> void
 *     Called when the plugin stops processing.
 *
 * wapi_plugin_process(const wapi_process_data_t* data) -> wapi_result_t
 *     Called for each audio block. THE hot path.
 *     Must be real-time safe: no allocation, no I/O, no blocking.
 *
 * wapi_plugin_param_changed(uint32_t param_id, float value) -> void
 *     Called when the host changes a parameter (automation).
 *
 * wapi_plugin_gui_create(wapi_handle_t surface) -> wapi_result_t
 *     Called to create the plugin GUI on the given surface.
 *     Only if WAPI_PLUGIN_FLAG_HAS_GUI is set.
 *
 * wapi_plugin_gui_destroy() -> void
 *     Called to destroy the plugin GUI.
 *
 * wapi_plugin_state_save(void* buf, wapi_size_t buf_len, wapi_size_t* size) -> wapi_result_t
 *     Save plugin state (presets) to a buffer.
 *
 * wapi_plugin_state_load(const void* buf, wapi_size_t len) -> wapi_result_t
 *     Load plugin state from a buffer.
 */

#ifdef __cplusplus
}
#endif

#endif /* WAPI_AUDIO_PLUGIN_H */
