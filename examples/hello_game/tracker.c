/**
 * WAPI hello_game — tracker child module.
 *
 * Exposes `tracker_render(request_off)`. Given a tracker_request_t in
 * shared memory plus linked instrument/note arrays, synthesises S16
 * mono PCM into out_off and returns the number of contributing notes.
 *
 * Waveforms: sine, saw, square, triangle, noise. Each note runs through
 * a linear ADSR envelope and is scaled by velocity and per-instrument
 * volume. The mix is clamped to int16 range.
 *
 * Pure: no host imports beyond the reactor shim and wapi_module shared
 * memory helpers.
 */

#include <wapi/wapi.h>
#include <wapi/wapi_module.h>

#include "tracker_types.h"

/* Freestanding intrinsics — the wasi-sdk freestanding build does not
 * supply libc memcpy/memset. */
void* memcpy(void* dst, const void* src, wapi_size_t n) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    for (wapi_size_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}
void* memset(void* dst, int v, wapi_size_t n) {
    unsigned char* d = (unsigned char*)dst;
    for (wapi_size_t i = 0; i < n; i++) d[i] = (unsigned char)v;
    return dst;
}

/* ============================================================
 * Math utilities — no libm in freestanding wasm.
 * ============================================================ */

static const float PI = 3.14159265358979323846f;
static const float TWO_PI = 6.28318530717958647692f;

/* Polynomial sine approximation valid on [-PI, PI]; <0.0011 abs error.
 * Remez-style minimax; good enough for synth tones. */
static float sin_approx(float x) {
    /* Wrap to [-PI, PI]. */
    while (x >  PI) x -= TWO_PI;
    while (x < -PI) x += TWO_PI;
    float x2 = x * x;
    return x * (0.99999660f
           + x2 * (-0.16664824f
           + x2 * ( 0.00830629f
           + x2 * (-0.00018363f))));
}

/* Integer-period hash (xorshift) for deterministic noise. */
static uint32_t noise_state = 0xC0FFEEu;
static float noise_sample(void) {
    uint32_t x = noise_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    noise_state = x;
    return (float)((int32_t)x) / 2147483648.0f;
}

/* MIDI note -> frequency in Hz, via 2^((n-69)/12) * 440.
 * Use a precomputed lookup for notes 0..127 to avoid needing pow. */
static float midi_to_hz(uint32_t note) {
    /* 2^(1/12) = 1.0594630943592953 */
    static const float base = 440.0f;   /* MIDI 69 = A4 */
    /* Build via repeated multiplication; cache for idempotency. */
    if (note > 127) note = 127;
    float f = base;
    int32_t steps = (int32_t)note - 69;
    if (steps > 0) {
        for (int i = 0; i < steps; i++) f *= 1.0594630943592953f;
    } else if (steps < 0) {
        for (int i = 0; i < -steps; i++) f *= (1.0f / 1.0594630943592953f);
    }
    return f;
}

/* ============================================================
 * ADSR envelope
 * ============================================================ */

/* Linear ADSR:
 *   0 .. attack       -> ramp from 0 to 1
 *   attack .. +decay  -> ramp from 1 to sustain
 *   sustain phase     -> hold until note_off
 *   note_off .. +rel  -> ramp from current level to 0
 * Returns level in [0, 1]. t is seconds since note_on; note_on_dur is
 * the held duration, after which the release phase begins. */
static float adsr(float t,
                  float note_on_dur,
                  float attack, float decay,
                  float sustain, float release) {
    if (t < 0.0f) return 0.0f;

    /* Release phase */
    if (t >= note_on_dur) {
        float rt = t - note_on_dur;
        if (release <= 0.0f || rt >= release) return 0.0f;
        /* Level at note_off: same formula as the sustain-branch below. */
        float base = sustain;
        if (note_on_dur < attack) {
            base = note_on_dur / attack;
        } else if (note_on_dur < attack + decay && decay > 0.0f) {
            float td = (note_on_dur - attack) / decay;
            base = 1.0f - td * (1.0f - sustain);
        }
        return base * (1.0f - rt / release);
    }

    /* Attack */
    if (t < attack) {
        return attack > 0.0f ? (t / attack) : 1.0f;
    }
    /* Decay */
    if (t < attack + decay && decay > 0.0f) {
        float td = (t - attack) / decay;
        return 1.0f - td * (1.0f - sustain);
    }
    /* Sustain */
    return sustain;
}

/* ============================================================
 * Render
 * ============================================================ */

#define MAX_INSTRUMENTS 16
#define MAX_NOTES       512

static tracker_instrument_t g_inst[MAX_INSTRUMENTS];
static tracker_note_t       g_notes[MAX_NOTES];

/* Working buffers. Float32 accumulator so mixing many notes does not
 * clip before final quantisation. */
#define CHUNK_SAMPLES 4096
static float    g_mix[CHUNK_SAMPLES];
static int16_t  g_out[CHUNK_SAMPLES];

WAPI_EXPORT(tracker_render)
int32_t tracker_render(int64_t request_off) {
    tracker_request_t req;
    if (wapi_module_shared_read((wapi_size_t)request_off, &req, sizeof(req))
        != WAPI_OK) {
        return -1;
    }
    if (req.sample_rate == 0 || req.out_samples == 0) return -1;
    if (req.num_instruments > MAX_INSTRUMENTS) return -2;
    if (req.num_notes > MAX_NOTES)             return -2;

    if (wapi_module_shared_read((wapi_size_t)req.instruments_off, g_inst,
                                req.num_instruments * sizeof(g_inst[0]))
        != WAPI_OK) return -3;
    if (wapi_module_shared_read((wapi_size_t)req.notes_off, g_notes,
                                req.num_notes * sizeof(g_notes[0]))
        != WAPI_OK) return -3;

    const float inv_sr = 1.0f / (float)req.sample_rate;
    int32_t contributing = 0;

    /* Render in chunks so we can keep scratch buffers on static storage
     * instead of blowing the stack on long loops. */
    uint32_t remaining = req.out_samples;
    uint32_t cursor = 0;
    while (remaining > 0) {
        uint32_t chunk = remaining < CHUNK_SAMPLES ? remaining : CHUNK_SAMPLES;
        for (uint32_t i = 0; i < chunk; i++) g_mix[i] = 0.0f;

        uint32_t mix_start = cursor;
        uint32_t mix_end   = cursor + chunk;

        for (uint32_t n = 0; n < req.num_notes; n++) {
            const tracker_note_t* nt = &g_notes[n];
            if (nt->instrument >= req.num_instruments) continue;
            const tracker_instrument_t* in = &g_inst[nt->instrument];

            float rel_samples = in->release * (float)req.sample_rate;
            uint32_t note_start = nt->start_sample;
            uint32_t note_end   = nt->start_sample + nt->length_samples
                                  + (uint32_t)rel_samples;
            if (note_end <= mix_start) continue;
            if (note_start >= mix_end) continue;
            contributing++;

            float hz = midi_to_hz(nt->midi_note);
            float phase_step = hz * inv_sr * TWO_PI;
            float vel = (float)nt->velocity / 127.0f;
            float note_on_dur = (float)nt->length_samples * inv_sr;

            /* Start index relative to this chunk. */
            uint32_t s_begin = note_start > mix_start
                               ? note_start - mix_start : 0;
            uint32_t s_end   = note_end < mix_end
                               ? note_end - mix_start : chunk;
            /* Phase accumulator: initialise from the note's own origin so
             * every render produces the same waveform regardless of how
             * the loop was split into chunks. */
            uint32_t abs_i = mix_start + s_begin - note_start;
            float phase = ((float)abs_i) * phase_step;

            for (uint32_t i = s_begin; i < s_end; i++) {
                float t = (float)(mix_start + i - note_start) * inv_sr;
                float env = adsr(t, note_on_dur,
                                 in->attack, in->decay,
                                 in->sustain, in->release);
                float w = 0.0f;
                switch (in->waveform) {
                case TRACKER_WAVE_SINE:
                    w = sin_approx(phase);
                    break;
                case TRACKER_WAVE_SAW: {
                    /* Phase mod 2pi, remapped to [-1, 1]. */
                    float p = phase;
                    while (p >  TWO_PI) p -= TWO_PI;
                    while (p <  0.0f)   p += TWO_PI;
                    w = (p / PI) - 1.0f;
                    break;
                }
                case TRACKER_WAVE_SQUARE: {
                    float p = phase;
                    while (p >  TWO_PI) p -= TWO_PI;
                    while (p <  0.0f)   p += TWO_PI;
                    w = p < PI ? 1.0f : -1.0f;
                    break;
                }
                case TRACKER_WAVE_TRIANGLE: {
                    float p = phase;
                    while (p >  TWO_PI) p -= TWO_PI;
                    while (p <  0.0f)   p += TWO_PI;
                    float u = p / PI;          /* 0..2 */
                    w = u < 1.0f ? (2.0f * u - 1.0f) : (3.0f - 2.0f * u);
                    break;
                }
                case TRACKER_WAVE_NOISE:
                    w = noise_sample();
                    break;
                default: w = 0.0f;
                }
                g_mix[i] += w * env * vel * in->volume;
                phase += phase_step;
            }
        }

        /* Quantise accumulator -> int16 with soft clip. */
        for (uint32_t i = 0; i < chunk; i++) {
            float s = g_mix[i];
            if (s >  1.0f) s =  1.0f;
            if (s < -1.0f) s = -1.0f;
            g_out[i] = (int16_t)(s * 32767.0f);
        }

        if (wapi_module_shared_write((wapi_size_t)(req.out_off + (uint64_t)cursor * 2),
                                     g_out, chunk * sizeof(int16_t)) != WAPI_OK) {
            return -4;
        }
        cursor    += chunk;
        remaining -= chunk;
    }

    return contributing;
}
