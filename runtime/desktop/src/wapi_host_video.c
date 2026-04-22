/* Stub -- not yet implemented for desktop */

#include "wapi_host.h"

/* ============================================================
 * Video Callbacks (all return WAPI_ERR_NOTSUP)
 * ============================================================ */

/* create: (i32 desc_ptr, i32 out_handle) -> i32 */
static wasm_trap_t* cb_video_create(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* destroy: (i32 video) -> i32 */
static wasm_trap_t* cb_video_destroy(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* get_info: (i32 video, i32 info_ptr) -> i32 */
static wasm_trap_t* cb_video_get_info(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* play: (i32 video) -> i32 */
static wasm_trap_t* cb_video_play(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* pause: (i32 video) -> i32 */
static wasm_trap_t* cb_video_pause(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* seek: (i32 video, f32 time_seconds) -> i32 */
static wasm_trap_t* cb_video_seek(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* get_state: (i32 video, i32 state_ptr) -> i32 */
static wasm_trap_t* cb_video_get_state(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* get_position: (i32 video, i32 time_ptr) -> i32 */
static wasm_trap_t* cb_video_get_position(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* get_frame_texture: (i32 video, i32 texture_ptr) -> i32 */
static wasm_trap_t* cb_video_get_frame_texture(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* render_to_texture: (i32 video, i32 texture, i32 x, i32 y, i32 w, i32 h) -> i32 */
static wasm_trap_t* cb_video_render_to_texture(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* bind_audio: (i32 video, i32 audio_stream) -> i32 */
static wasm_trap_t* cb_video_bind_audio(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* set_volume: (i32 video, f32 volume) -> i32 */
static wasm_trap_t* cb_video_set_volume(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* set_muted: (i32 video, i32 muted) -> i32 */
static wasm_trap_t* cb_video_set_muted(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* set_loop: (i32 video, i32 loop) -> i32 */
static wasm_trap_t* cb_video_set_loop(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* set_playback_rate: (i32 video, f32 rate) -> i32 */
static wasm_trap_t* cb_video_set_playback_rate(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults) {
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOTSUP);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_video(wasmtime_linker_t* linker) {
    WAPI_DEFINE_2_1(linker, "wapi_video", "create",            cb_video_create);
    WAPI_DEFINE_1_1(linker, "wapi_video", "destroy",           cb_video_destroy);
    WAPI_DEFINE_2_1(linker, "wapi_video", "get_info",          cb_video_get_info);
    WAPI_DEFINE_1_1(linker, "wapi_video", "play",              cb_video_play);
    WAPI_DEFINE_1_1(linker, "wapi_video", "pause",             cb_video_pause);

    /* seek: (i32, f32) -> i32 */
    wapi_linker_define(linker, "wapi_video", "seek", cb_video_seek,
                     2, (wasm_valkind_t[]){WASM_I32, WASM_F32},
                     1, (wasm_valkind_t[]){WASM_I32});

    WAPI_DEFINE_2_1(linker, "wapi_video", "get_state",         cb_video_get_state);
    WAPI_DEFINE_2_1(linker, "wapi_video", "get_position",      cb_video_get_position);
    WAPI_DEFINE_2_1(linker, "wapi_video", "get_frame_texture", cb_video_get_frame_texture);
    /* Header declares this as `blit`, not `render_to_texture` */
    WAPI_DEFINE_6_1(linker, "wapi_video", "blit",              cb_video_render_to_texture);
    WAPI_DEFINE_2_1(linker, "wapi_video", "bind_audio",        cb_video_bind_audio);

    /* set_volume: (i32, f32) -> i32 */
    wapi_linker_define(linker, "wapi_video", "set_volume", cb_video_set_volume,
                     2, (wasm_valkind_t[]){WASM_I32, WASM_F32},
                     1, (wasm_valkind_t[]){WASM_I32});

    WAPI_DEFINE_2_1(linker, "wapi_video", "set_muted",         cb_video_set_muted);
    WAPI_DEFINE_2_1(linker, "wapi_video", "set_loop",          cb_video_set_loop);

    /* set_playback_rate: (i32, f32) -> i32 */
    wapi_linker_define(linker, "wapi_video", "set_playback_rate", cb_video_set_playback_rate,
                     2, (wasm_valkind_t[]){WASM_I32, WASM_F32},
                     1, (wasm_valkind_t[]){WASM_I32});
}
