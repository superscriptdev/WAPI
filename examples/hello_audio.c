/**
 * WAPI - Hello Audio Example
 *
 * Generates a 440 Hz sine wave and plays it through the default audio
 * playback endpoint. The endpoint is acquired via a ROLE_REQUEST
 * (spec §9.10); the stream surface in wapi_audio.h pushes samples.
 *
 * Compile with:
 *   clang --target=wasm32 -O2 -nostdlib \
 *     -I../include -o hello_audio.wasm hello_audio.c
 */

#include <wapi/wapi.h>
#include <wapi/wapi_audio.h>

/* ============================================================
 * Simple fixed-point sine approximation (no libm needed)
 * ============================================================ */

static float sine_approx(float x) {
    const float PI = 3.14159265f;
    const float TWO_PI = 6.28318530f;
    while (x < 0.0f) x += TWO_PI;
    while (x >= TWO_PI) x -= TWO_PI;
    x -= PI;
    return (16.0f * x * (PI - x)) /
           (5.0f * PI * PI - 4.0f * x * (PI - x));
}

/* ============================================================
 * Blocking role-request helper
 * ============================================================
 * Submits one role request and pumps the event queue until the
 * completion with the matching user_data tag arrives. Returns the
 * per-role outcome written into *out_result. */

static wapi_result_t request_endpoint(
    const wapi_io_t* io, uint32_t kind, uint32_t flags,
    const void* prefs, uint32_t prefs_len,
    wapi_handle_t* out_handle)
{
    wapi_result_t role_result = WAPI_ERR_AGAIN;
    wapi_role_request_t req = {0};
    req.kind       = kind;
    req.flags      = flags;
    req.prefs_addr = (uint64_t)(uintptr_t)prefs;
    req.prefs_len  = prefs_len;
    req.out_handle = (uint64_t)(uintptr_t)out_handle;
    req.out_result = (uint64_t)(uintptr_t)&role_result;

    const uint64_t tag = 0xA0D10ULL;
    wapi_result_t sub = wapi_role_request(io, &req, 1, tag);
    if (WAPI_FAILED(sub)) return sub;

    wapi_event_t ev;
    for (int tries = 0; tries < 1024; tries++) {
        if (io->poll(io->impl, &ev)
            && ev.type == WAPI_EVENT_IO_COMPLETION
            && ev.io.user_data == tag) {
            return role_result;
        }
    }
    return WAPI_ERR_TIMEDOUT;
}

/* ============================================================
 * Application
 * ============================================================ */

#define SAMPLE_RATE 44100
#define CHANNELS    1
#define FREQUENCY   440.0f
#define AMPLITUDE   0.3f
#define DURATION_S  2

WAPI_EXPORT(wapi_main)
wapi_result_t wapi_main(void) {
    const wapi_io_t* io = wapi_io_get();
    if (!io || !wapi_cap_supported(io, WAPI_STR(WAPI_CAP_AUDIO))) {
        const char msg[] = "Audio capability not available\n";
        wapi_size_t written;
        wapi_fs_write(WAPI_STDERR, msg, sizeof(msg) - 1, &written);
        return WAPI_ERR_NOTCAPABLE;
    }

    /* Role-request the default playback endpoint. */
    wapi_audio_spec_t spec = {
        .format   = WAPI_AUDIO_F32,
        .channels = CHANNELS,
        .freq     = SAMPLE_RATE,
    };
    wapi_handle_t device;
    wapi_result_t res = request_endpoint(
        io, WAPI_ROLE_AUDIO_PLAYBACK, WAPI_ROLE_FOLLOW_DEFAULT,
        &spec, sizeof(spec), &device);
    if (WAPI_FAILED(res)) return res;

    /* Attach a stream that converts guest-format samples to the device's
     * native format. dst = NULL means "use the endpoint's native spec". */
    wapi_handle_t stream;
    res = wapi_audio_create_stream(&spec, NULL, &stream);
    if (WAPI_FAILED(res)) { wapi_audio_close(device); return res; }
    res = wapi_audio_bind_stream(device, stream);
    if (WAPI_FAILED(res)) {
        wapi_audio_destroy_stream(stream);
        wapi_audio_close(device);
        return res;
    }

    wapi_audio_resume(device);

    const int total_samples = SAMPLE_RATE * DURATION_S;
    const int chunk_size = 1024;
    float samples[1024];
    float phase = 0.0f;
    const float phase_inc = FREQUENCY * 6.28318530f / (float)SAMPLE_RATE;

    int samples_written = 0;
    while (samples_written < total_samples) {
        int count = chunk_size;
        if (samples_written + count > total_samples) {
            count = total_samples - samples_written;
        }
        for (int i = 0; i < count; i++) {
            samples[i] = AMPLITUDE * sine_approx(phase);
            phase += phase_inc;
            if (phase >= 6.28318530f) phase -= 6.28318530f;
        }
        res = wapi_audio_put_stream_data(stream, samples, count * sizeof(float));
        if (WAPI_FAILED(res)) break;
        samples_written += count;

        int32_t queued = wapi_audio_stream_queued(stream);
        if (queued > SAMPLE_RATE / 4 * (int32_t)sizeof(float)) {
            wapi_sleep(10000000ULL); /* 10ms */
        }
    }

    wapi_sleep((uint64_t)DURATION_S * 1000000000ULL + 100000000ULL);

    wapi_audio_destroy_stream(stream);
    wapi_audio_close(device);

    const char done[] = "Audio playback complete.\n";
    wapi_size_t written;
    wapi_fs_write(WAPI_STDOUT, done, sizeof(done) - 1, &written);
    return WAPI_OK;
}
