/**
 * WAPI - Hello Audio Example
 *
 * A minimal audio application that demonstrates the Audio preset:
 * generates a sine wave and plays it through the default audio device.
 *
 * Compile with:
 *   clang --target=wasm32 -O2 -nostdlib \
 *     -I../include -o hello_audio.wasm hello_audio.c
 */

#include <wapi/wapi.h>

/* ============================================================
 * Simple fixed-point sine approximation (no libm needed)
 * ============================================================ */

static float sine_approx(float x) {
    /* Normalize x to [-PI, PI] range */
    const float PI = 3.14159265f;
    const float TWO_PI = 6.28318530f;

    /* Wrap to [0, 2*PI] */
    while (x < 0.0f) x += TWO_PI;
    while (x >= TWO_PI) x -= TWO_PI;

    /* Shift to [-PI, PI] */
    x -= PI;

    /* Bhaskara I's sine approximation */
    float x2 = x * x;
    return (16.0f * x * (PI - x)) /
           (5.0f * PI * PI - 4.0f * x * (PI - x));
}

/* ============================================================
 * Application State
 * ============================================================ */

#define SAMPLE_RATE 44100
#define CHANNELS    1
#define FREQUENCY   440.0f  /* A4 note */
#define AMPLITUDE   0.3f
#define DURATION_S  2       /* Play for 2 seconds */

WAPI_EXPORT(wapi_main)
wapi_result_t wapi_main(const wapi_context_t* ctx) {
    (void)ctx;
    wapi_result_t res;

    /* Check capabilities */
    if (!wapi_capability_supported(WAPI_CAP_AUDIO, 8)) {
        const char msg[] = "Audio capability not available\n";
        wapi_size_t written;
        wapi_fs_write(WAPI_STDERR, msg, sizeof(msg) - 1, &written);
        return WAPI_ERR_NOTCAPABLE;
    }

    /* Open audio device and stream */
    wapi_audio_spec_t spec = {
        .format   = WAPI_AUDIO_F32,
        .channels = CHANNELS,
        .freq     = SAMPLE_RATE,
    };

    wapi_handle_t device, stream;
    res = wapi_audio_open_device_stream(WAPI_AUDIO_DEFAULT_PLAYBACK, &spec,
                                       &device, &stream);
    if (WAPI_FAILED(res)) return res;

    /* Generate and push audio samples */
    const int total_samples = SAMPLE_RATE * DURATION_S;
    const int chunk_size = 1024;
    float samples[1024];
    float phase = 0.0f;
    const float phase_inc = FREQUENCY * 6.28318530f / (float)SAMPLE_RATE;

    /* Start playback */
    wapi_audio_resume_device(device);

    int samples_written = 0;
    while (samples_written < total_samples) {
        int count = chunk_size;
        if (samples_written + count > total_samples) {
            count = total_samples - samples_written;
        }

        /* Generate sine wave samples */
        for (int i = 0; i < count; i++) {
            samples[i] = AMPLITUDE * sine_approx(phase);
            phase += phase_inc;
            if (phase >= 6.28318530f) phase -= 6.28318530f;
        }

        /* Push to audio stream */
        res = wapi_audio_put_stream_data(stream, samples, count * sizeof(float));
        if (WAPI_FAILED(res)) break;

        samples_written += count;

        /* Wait a bit if the stream buffer is getting full */
        int32_t queued = wapi_audio_stream_queued(stream);
        if (queued > SAMPLE_RATE / 4 * (int32_t)sizeof(float)) {
            wapi_sleep(10000000ULL); /* 10ms */
        }
    }

    /* Wait for playback to finish */
    wapi_sleep((uint64_t)DURATION_S * 1000000000ULL + 100000000ULL);

    /* Cleanup */
    wapi_audio_destroy_stream(stream);
    wapi_audio_close_device(device);

    const char done[] = "Audio playback complete.\n";
    wapi_size_t written;
    wapi_fs_write(WAPI_STDOUT, done, sizeof(done) - 1, &written);

    return WAPI_OK;
}
