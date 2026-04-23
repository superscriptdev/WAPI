/**
 * WAPI Desktop Runtime - Win32 WASAPI Audio Backend
 *
 * Shared-mode playback via WASAPI. Each stream owns a lock-free
 * SPSC ring between the guest (producer, wasm thread) and the
 * audio worker thread (consumer). Worker threads copy from the
 * ring into the WASAPI endpoint buffer on event-driven wake.
 *
 * Format support in this phase:
 *   - dst spec driven by the device's mix format (WASAPI shared
 *     mode negotiates around that; we let AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
 *     do format conversion for us)
 *   - src = dst pass-through byte copy
 *   - simple S16 <-> F32 convert when src format differs from dst
 *
 * Deferred: capture devices, high-quality resampler, exclusive mode.
 * See NEXT_STEPS.md.
 */

#include "wapi_plat.h"

#define COBJMACROS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objbase.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "ole32")
#pragma comment(lib, "avrt")

/* GUIDs we need that some MinGW toolchains don't export — declare once. */
static const IID IID_IAudioClient_local        = { 0x1CB9AD4C, 0xDBFA, 0x4c32, { 0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2 } };
static const IID IID_IAudioRenderClient_local  = { 0xF294ACFC, 0x3146, 0x4483, { 0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2 } };
static const IID IID_IAudioCaptureClient_local = { 0xC8ADBD64, 0xE71E, 0x48A0, { 0xA4, 0xDE, 0x18, 0x5C, 0x39, 0x5C, 0xD3, 0x17 } };
static const CLSID CLSID_MMDeviceEnumerator_local  = { 0xBCDE0395, 0xE52F, 0x467C, { 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E } };
static const IID   IID_IMMDeviceEnumerator_local   = { 0xA95664D2, 0x9614, 0x4F35, { 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6 } };

/* ============================================================
 * SPSC ring of bytes
 * ============================================================
 * Producer: guest (wasm) thread calling stream_put
 * Consumer: WASAPI worker thread
 * Single producer, single consumer — safe with atomic head/tail.
 */

typedef struct byte_ring_t {
    uint8_t*  data;
    uint32_t  cap;        /* power of two */
    uint32_t  mask;
    volatile LONG head;   /* write position (producer) */
    volatile LONG tail;   /* read  position (consumer) */
} byte_ring_t;

static bool ring_init(byte_ring_t* r, uint32_t want_bytes) {
    uint32_t cap = 1;
    while (cap < want_bytes) cap <<= 1;
    r->data = (uint8_t*)malloc(cap);
    if (!r->data) return false;
    r->cap  = cap;
    r->mask = cap - 1;
    r->head = 0;
    r->tail = 0;
    return true;
}
static void ring_free(byte_ring_t* r) { free(r->data); r->data = NULL; }

static uint32_t ring_count(const byte_ring_t* r) {
    /* acquire both to get a consistent snapshot */
    LONG h = r->head, t = r->tail;
    return (uint32_t)(h - t);
}
static uint32_t ring_free_space(const byte_ring_t* r) {
    return r->cap - ring_count(r);
}

static uint32_t ring_write(byte_ring_t* r, const void* src, uint32_t n) {
    uint32_t avail = ring_free_space(r);
    if (n > avail) n = avail;
    uint32_t h = (uint32_t)r->head & r->mask;
    uint32_t first = r->cap - h;
    if (first > n) first = n;
    memcpy(r->data + h, src, first);
    if (n > first) memcpy(r->data, (const uint8_t*)src + first, n - first);
    InterlockedAdd(&r->head, (LONG)n);
    return n;
}

static uint32_t ring_read(byte_ring_t* r, void* dst, uint32_t n) {
    uint32_t avail = ring_count(r);
    if (n > avail) n = avail;
    uint32_t t = (uint32_t)r->tail & r->mask;
    uint32_t first = r->cap - t;
    if (first > n) first = n;
    memcpy(dst, r->data + t, first);
    if (n > first) memcpy((uint8_t*)dst + first, r->data, n - first);
    InterlockedAdd(&r->tail, (LONG)n);
    return n;
}

/* ============================================================
 * Types
 * ============================================================ */

/* Max number of streams a single device can mix. Games + OS beep
 * generally need 2-3 (music + sfx + oneshots). 8 gives headroom. */
#define WAPI_PLAT_AUDIO_MAX_STREAMS_PER_DEVICE 8

struct wapi_plat_audio_device_t {
    IMMDevice*    mm_device;
    IAudioClient* audio_client;
    WAVEFORMATEX* mix_format;      /* endpoint format */
    UINT32        buffer_frames;
    bool          started;
    bool          is_capture;

    /* Shared WASAPI client + worker thread. Multiple guest streams bind
     * into device->streams[] and the single worker thread mixes them
     * into the render endpoint (or demuxes a capture endpoint). */
    IAudioRenderClient*  render_client;
    IAudioCaptureClient* capture_client;
    HANDLE       wake_event;       /* WASAPI event-driven callback */
    HANDLE       stop_event;       /* set to end worker */
    HANDLE       worker_thread;

    /* Streams bound to this device. Protected by streams_lock for
     * insertions/removals from the guest thread against reads from the
     * audio worker. The worker runs on its own thread and only reads
     * the array; the ring inside each stream is already SPSC-safe. */
    CRITICAL_SECTION streams_lock;
    struct wapi_plat_audio_stream_t* streams[WAPI_PLAT_AUDIO_MAX_STREAMS_PER_DEVICE];
    int          num_streams;
};

struct wapi_plat_audio_stream_t {
    /* src spec (what the guest supplies) */
    wapi_plat_audio_spec_t src;
    /* dst spec (what the device actually wants) */
    wapi_plat_audio_spec_t dst;

    byte_ring_t ring;              /* holds dst-format bytes, ready to play */

    /* When bound, points at the owning device. NULL otherwise. */
    struct wapi_plat_audio_device_t* device;
};

/* ============================================================
 * Format helpers
 * ============================================================ */

static uint32_t bytes_per_frame(const wapi_plat_audio_spec_t* s) {
    uint32_t bps = 1;
    switch (s->format) {
    case WAPI_PLAT_AUDIO_U8:  bps = 1; break;
    case WAPI_PLAT_AUDIO_S16: bps = 2; break;
    case WAPI_PLAT_AUDIO_S32: bps = 4; break;
    case WAPI_PLAT_AUDIO_F32: bps = 4; break;
    }
    return bps * (uint32_t)s->channels;
}

static void waveformat_to_spec(const WAVEFORMATEX* wf, wapi_plat_audio_spec_t* out) {
    out->channels = (int32_t)wf->nChannels;
    out->freq     = (int32_t)wf->nSamplesPerSec;
    out->_pad     = 0;
    if (wf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT && wf->wBitsPerSample == 32) {
        out->format = WAPI_PLAT_AUDIO_F32;
    } else if (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const WAVEFORMATEXTENSIBLE* we = (const WAVEFORMATEXTENSIBLE*)wf;
        /* KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = 00000003-0000-0010-8000-00aa00389b71 */
        if (we->SubFormat.Data1 == 0x00000003) out->format = WAPI_PLAT_AUDIO_F32;
        else if (wf->wBitsPerSample == 16)     out->format = WAPI_PLAT_AUDIO_S16;
        else if (wf->wBitsPerSample == 32)     out->format = WAPI_PLAT_AUDIO_S32;
        else                                   out->format = WAPI_PLAT_AUDIO_S16;
    } else {
        if      (wf->wBitsPerSample == 8)  out->format = WAPI_PLAT_AUDIO_U8;
        else if (wf->wBitsPerSample == 16) out->format = WAPI_PLAT_AUDIO_S16;
        else if (wf->wBitsPerSample == 32) out->format = WAPI_PLAT_AUDIO_S32;
        else                               out->format = WAPI_PLAT_AUDIO_S16;
    }
}

/* Convert src_frames from `src` format into `dst` format buffer.
 * Returns bytes written. Channel counts and rates must match —
 * this is only a format convert. */
static uint32_t convert_frames(const wapi_plat_audio_spec_t* src,
                               const wapi_plat_audio_spec_t* dst,
                               const void* in_buf, uint32_t in_frames,
                               void* out_buf) {
    if (src->format == dst->format) {
        uint32_t bpf = bytes_per_frame(src);
        memcpy(out_buf, in_buf, (size_t)bpf * in_frames);
        return bpf * in_frames;
    }

    uint32_t samples = in_frames * (uint32_t)src->channels;

    /* Promote to float internally, then write out in dst format */
    float tmp_stack[1024];
    float* tmp = tmp_stack;
    float* tmp_heap = NULL;
    if (samples > (uint32_t)(sizeof(tmp_stack) / sizeof(tmp_stack[0]))) {
        tmp_heap = (float*)malloc(samples * sizeof(float));
        if (!tmp_heap) return 0;
        tmp = tmp_heap;
    }

    const uint8_t* ib = (const uint8_t*)in_buf;
    switch (src->format) {
    case WAPI_PLAT_AUDIO_U8:
        for (uint32_t i = 0; i < samples; i++)
            tmp[i] = ((float)ib[i] - 128.0f) / 128.0f;
        break;
    case WAPI_PLAT_AUDIO_S16: {
        const int16_t* p = (const int16_t*)in_buf;
        for (uint32_t i = 0; i < samples; i++) tmp[i] = (float)p[i] / 32768.0f;
        break;
    }
    case WAPI_PLAT_AUDIO_S32: {
        const int32_t* p = (const int32_t*)in_buf;
        for (uint32_t i = 0; i < samples; i++) tmp[i] = (float)p[i] / 2147483648.0f;
        break;
    }
    case WAPI_PLAT_AUDIO_F32:
        memcpy(tmp, in_buf, samples * sizeof(float));
        break;
    }

    uint32_t out_bytes = 0;
    switch (dst->format) {
    case WAPI_PLAT_AUDIO_U8: {
        uint8_t* o = (uint8_t*)out_buf;
        for (uint32_t i = 0; i < samples; i++) {
            float v = tmp[i] * 128.0f + 128.0f;
            if (v < 0.0f) v = 0.0f; else if (v > 255.0f) v = 255.0f;
            o[i] = (uint8_t)v;
        }
        out_bytes = samples;
        break;
    }
    case WAPI_PLAT_AUDIO_S16: {
        int16_t* o = (int16_t*)out_buf;
        for (uint32_t i = 0; i < samples; i++) {
            float v = tmp[i] * 32767.0f;
            if (v >  32767.0f) v =  32767.0f;
            if (v < -32768.0f) v = -32768.0f;
            o[i] = (int16_t)v;
        }
        out_bytes = samples * 2;
        break;
    }
    case WAPI_PLAT_AUDIO_S32: {
        int32_t* o = (int32_t*)out_buf;
        for (uint32_t i = 0; i < samples; i++) {
            double v = (double)tmp[i] * 2147483647.0;
            if (v >  2147483647.0) v =  2147483647.0;
            if (v < -2147483648.0) v = -2147483648.0;
            o[i] = (int32_t)v;
        }
        out_bytes = samples * 4;
        break;
    }
    case WAPI_PLAT_AUDIO_F32:
        memcpy(out_buf, tmp, samples * sizeof(float));
        out_bytes = samples * 4;
        break;
    }

    if (tmp_heap) free(tmp_heap);
    return out_bytes;
}

/* ============================================================
 * COM init (per-thread, idempotent)
 * ============================================================ */

static DWORD s_com_tls = TLS_OUT_OF_INDEXES;

static bool ensure_com(void) {
    if (s_com_tls == TLS_OUT_OF_INDEXES) s_com_tls = TlsAlloc();
    if (TlsGetValue(s_com_tls)) return true;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (hr == RPC_E_CHANGED_MODE) {
        hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    }
    if (SUCCEEDED(hr) || hr == S_FALSE) {
        TlsSetValue(s_com_tls, (void*)1);
        return true;
    }
    return false;
}

/* ============================================================
 * Per-dst-format sample convert to/from float accumulator
 * ============================================================
 * Mixing is always done in F32 space. read_as_float pulls a
 * stream's ring bytes and promotes to float; write_from_float
 * writes the mixed accumulator back in the device's dst format.
 * dst bpf mismatch between streams and the endpoint cannot happen
 * — every bound stream shares the device's dst format. */

static void promote_to_float(uint32_t fmt,
                             const void* in, uint32_t samples, float* out) {
    switch (fmt) {
    case WAPI_PLAT_AUDIO_U8: {
        const uint8_t* p = (const uint8_t*)in;
        for (uint32_t i = 0; i < samples; i++) out[i] = ((float)p[i] - 128.0f) / 128.0f;
        break;
    }
    case WAPI_PLAT_AUDIO_S16: {
        const int16_t* p = (const int16_t*)in;
        for (uint32_t i = 0; i < samples; i++) out[i] = (float)p[i] / 32768.0f;
        break;
    }
    case WAPI_PLAT_AUDIO_S32: {
        const int32_t* p = (const int32_t*)in;
        for (uint32_t i = 0; i < samples; i++) out[i] = (float)p[i] / 2147483648.0f;
        break;
    }
    case WAPI_PLAT_AUDIO_F32:
        memcpy(out, in, samples * sizeof(float));
        break;
    }
}

static void demote_from_float(uint32_t fmt,
                              const float* in, uint32_t samples, void* out) {
    switch (fmt) {
    case WAPI_PLAT_AUDIO_U8: {
        uint8_t* o = (uint8_t*)out;
        for (uint32_t i = 0; i < samples; i++) {
            float v = in[i] * 128.0f + 128.0f;
            if (v < 0.0f) v = 0.0f; else if (v > 255.0f) v = 255.0f;
            o[i] = (uint8_t)v;
        }
        break;
    }
    case WAPI_PLAT_AUDIO_S16: {
        int16_t* o = (int16_t*)out;
        for (uint32_t i = 0; i < samples; i++) {
            float v = in[i] * 32767.0f;
            if (v >  32767.0f) v =  32767.0f;
            if (v < -32768.0f) v = -32768.0f;
            o[i] = (int16_t)v;
        }
        break;
    }
    case WAPI_PLAT_AUDIO_S32: {
        int32_t* o = (int32_t*)out;
        for (uint32_t i = 0; i < samples; i++) {
            double v = (double)in[i] * 2147483647.0;
            if (v >  2147483647.0) v =  2147483647.0;
            if (v < -2147483648.0) v = -2147483648.0;
            o[i] = (int32_t)v;
        }
        break;
    }
    case WAPI_PLAT_AUDIO_F32:
        memcpy(out, in, samples * sizeof(float));
        break;
    }
}

/* ============================================================
 * Worker thread: one per device. Mixes all bound streams into the
 * single shared render endpoint, or fans capture out to each.
 * ============================================================ */

static DWORD WINAPI audio_worker(LPVOID param) {
    struct wapi_plat_audio_device_t* d = (struct wapi_plat_audio_device_t*)param;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    DWORD task_index = 0;
    HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);

    HANDLE waits[2] = { d->stop_event, d->wake_event };

    /* Float accumulator for mixing. Sized to one WASAPI buffer's worth
     * of samples (frames * channels). Grows as needed. */
    float*   mix_buf      = NULL;
    uint8_t* stream_bytes = NULL;
    uint32_t mix_cap      = 0;

    for (;;) {
        DWORD rc = WaitForMultipleObjects(2, waits, FALSE, 2000);
        if (rc == WAIT_OBJECT_0) break; /* stop */

        wapi_plat_audio_spec_t dst_spec;
        waveformat_to_spec(d->mix_format, &dst_spec);
        uint32_t bpf_dst = bytes_per_frame(&dst_spec);

        if (!d->is_capture) {
            UINT32 padding = 0;
            if (FAILED(IAudioClient_GetCurrentPadding(d->audio_client, &padding))) continue;
            UINT32 avail_frames = d->buffer_frames - padding;
            if (avail_frames == 0) continue;

            uint32_t samples   = avail_frames * (uint32_t)dst_spec.channels;
            uint32_t need_cap  = samples;
            if (need_cap > mix_cap) {
                free(mix_buf); free(stream_bytes);
                mix_buf      = (float*)malloc(need_cap * sizeof(float));
                stream_bytes = (uint8_t*)malloc(avail_frames * bpf_dst);
                if (!mix_buf || !stream_bytes) {
                    mix_cap = 0;
                    free(mix_buf); free(stream_bytes);
                    mix_buf = NULL; stream_bytes = NULL;
                    continue;
                }
                mix_cap = need_cap;
            }
            memset(mix_buf, 0, samples * sizeof(float));

            /* Pull from each stream's ring and sum into mix_buf. */
            EnterCriticalSection(&d->streams_lock);
            int n = d->num_streams;
            struct wapi_plat_audio_stream_t* streams[WAPI_PLAT_AUDIO_MAX_STREAMS_PER_DEVICE];
            for (int i = 0; i < n; i++) streams[i] = d->streams[i];
            LeaveCriticalSection(&d->streams_lock);

            uint32_t bytes_needed = avail_frames * bpf_dst;
            float    scratch[2048];
            for (int i = 0; i < n; i++) {
                struct wapi_plat_audio_stream_t* s = streams[i];
                memset(stream_bytes, 0, bytes_needed);
                ring_read(&s->ring, stream_bytes, bytes_needed);
                /* Promote stream's dst-format bytes -> float scratch and
                 * sum. Chunked to avoid allocating for large buffers. */
                uint32_t remaining = samples;
                uint32_t cursor    = 0;
                while (remaining > 0) {
                    uint32_t chunk = remaining;
                    if (chunk > sizeof(scratch) / sizeof(scratch[0]))
                        chunk = sizeof(scratch) / sizeof(scratch[0]);
                    uint32_t sample_bytes;
                    switch (s->dst.format) {
                    case WAPI_PLAT_AUDIO_U8:  sample_bytes = 1; break;
                    case WAPI_PLAT_AUDIO_S16: sample_bytes = 2; break;
                    case WAPI_PLAT_AUDIO_S32: sample_bytes = 4; break;
                    case WAPI_PLAT_AUDIO_F32: sample_bytes = 4; break;
                    default:                  sample_bytes = 0; break;
                    }
                    if (sample_bytes == 0) break;
                    promote_to_float(s->dst.format,
                                     stream_bytes + cursor * sample_bytes,
                                     chunk, scratch);
                    for (uint32_t k = 0; k < chunk; k++) mix_buf[cursor + k] += scratch[k];
                    cursor    += chunk;
                    remaining -= chunk;
                }
            }

            /* Soft-clip the sum before quantizing back to device format. */
            for (uint32_t i = 0; i < samples; i++) {
                float v = mix_buf[i];
                if (v >  1.0f) v =  1.0f;
                if (v < -1.0f) v = -1.0f;
                mix_buf[i] = v;
            }

            BYTE* dst = NULL;
            if (FAILED(IAudioRenderClient_GetBuffer(d->render_client, avail_frames, &dst))) continue;
            demote_from_float(dst_spec.format, mix_buf, samples, dst);
            IAudioRenderClient_ReleaseBuffer(d->render_client, avail_frames, 0);
        } else {
            /* Capture: fan the endpoint's samples out to each bound stream
             * (convert to each stream's src format). Preserves current
             * single-subscriber semantics if only one stream is bound. */
            for (;;) {
                UINT32 next = 0;
                if (FAILED(IAudioCaptureClient_GetNextPacketSize(d->capture_client, &next))) break;
                if (next == 0) break;

                BYTE* src = NULL;
                UINT32 frames = 0;
                DWORD  flags  = 0;
                if (FAILED(IAudioCaptureClient_GetBuffer(d->capture_client, &src, &frames,
                                                         &flags, NULL, NULL))) break;
                bool silent = (flags & 0x1 /* AUDCLNT_BUFFERFLAGS_SILENT */) != 0;

                EnterCriticalSection(&d->streams_lock);
                int n = d->num_streams;
                struct wapi_plat_audio_stream_t* streams[WAPI_PLAT_AUDIO_MAX_STREAMS_PER_DEVICE];
                for (int i = 0; i < n; i++) streams[i] = d->streams[i];
                LeaveCriticalSection(&d->streams_lock);

                for (int i = 0; i < n; i++) {
                    struct wapi_plat_audio_stream_t* s = streams[i];
                    uint32_t out_guest_bytes = frames * bytes_per_frame(&s->src);
                    uint8_t  stk[4096];
                    uint8_t* buf = stk;
                    bool     heap = false;
                    if (out_guest_bytes > sizeof(stk)) {
                        buf = (uint8_t*)malloc(out_guest_bytes);
                        heap = (buf != NULL);
                        if (!buf) continue;
                    }
                    if (silent) memset(buf, 0, out_guest_bytes);
                    else        convert_frames(&s->dst, &s->src, src, frames, buf);

                    uint32_t free_bytes = ring_free_space(&s->ring);
                    if (free_bytes < out_guest_bytes) {
                        uint32_t drop = out_guest_bytes - free_bytes;
                        uint8_t sink[1024];
                        while (drop > 0) {
                            uint32_t take = drop > sizeof(sink) ? (uint32_t)sizeof(sink) : drop;
                            uint32_t got = ring_read(&s->ring, sink, take);
                            if (got == 0) break;
                            drop -= got;
                        }
                    }
                    ring_write(&s->ring, buf, out_guest_bytes);
                    if (heap) free(buf);
                }

                IAudioCaptureClient_ReleaseBuffer(d->capture_client, frames);
            }
        }
    }

    free(mix_buf); free(stream_bytes);
    if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
    CoUninitialize();
    return 0;
}

/* ============================================================
 * Device enumeration
 * ============================================================ */

static IMMDeviceEnumerator* get_enumerator(void) {
    if (!ensure_com()) return NULL;
    IMMDeviceEnumerator* e = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_MMDeviceEnumerator_local, NULL,
                                  CLSCTX_ALL, &IID_IMMDeviceEnumerator_local,
                                  (void**)&e);
    return SUCCEEDED(hr) ? e : NULL;
}

/* FriendlyName + FormFactor lookup used by wapi_plat_audio_device_describe. */
static size_t immdevice_friendly_name(IMMDevice* dev, char* out, size_t out_len) {
    if (!dev) return 0;
    IPropertyStore* props = NULL;
    if (FAILED(IMMDevice_OpenPropertyStore(dev, STGM_READ, &props))) return 0;
    /* PKEY_Device_FriendlyName */
    const PROPERTYKEY pk = { {0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14 };
    PROPVARIANT v; PropVariantInit(&v);
    size_t nbytes = 0;
    if (SUCCEEDED(IPropertyStore_GetValue(props, &pk, &v)) && v.vt == VT_LPWSTR) {
        int u8len = WideCharToMultiByte(CP_UTF8, 0, v.pwszVal, -1, NULL, 0, NULL, NULL);
        nbytes = u8len > 0 ? (size_t)(u8len - 1) : 0;
        if (out && out_len > 0 && nbytes > 0) {
            size_t copy = nbytes < out_len ? nbytes : out_len;
            WideCharToMultiByte(CP_UTF8, 0, v.pwszVal, -1, out, (int)copy, NULL, NULL);
        }
    }
    PropVariantClear(&v);
    IPropertyStore_Release(props);
    return nbytes;
}

/* WASAPI EndpointFormFactor → wapi_audio_form_t wire values.
 * Source: mmdeviceapi.h EndpointFormFactor enum. */
static uint32_t immdevice_form_factor(IMMDevice* dev) {
    if (!dev) return 0; /* UNKNOWN */
    IPropertyStore* props = NULL;
    if (FAILED(IMMDevice_OpenPropertyStore(dev, STGM_READ, &props))) return 0;
    /* PKEY_AudioEndpoint_FormFactor */
    const PROPERTYKEY pk = { {0x1da5d803, 0xd492, 0x4edd, {0x8c, 0x23, 0xe0, 0xc0, 0xff, 0xee, 0x7f, 0x0e}}, 0 };
    PROPVARIANT v; PropVariantInit(&v);
    uint32_t form = 0;
    if (SUCCEEDED(IPropertyStore_GetValue(props, &pk, &v)) && v.vt == VT_UI4) {
        /* WASAPI: 0=RemoteNetworkDevice, 1=Speakers, 2=LineLevel, 3=Headphones,
         * 4=Microphone, 5=Headset, 6=Handset, 7=UnknownDigitalPassthrough,
         * 8=SPDIF, 9=DigitalAudioDisplayDevice, 10=UnknownFormFactor. */
        switch (v.ulVal) {
        case 1:  form = 1; break; /* Speakers */
        case 2:  form = 4; break; /* LineOut */
        case 3:  form = 2; break; /* Headphones */
        case 5:  form = 3; break; /* Headset */
        case 4:  form = 3; break; /* Microphone → headset mic bucket */
        default: form = 0; break;
        }
    }
    PropVariantClear(&v);
    IPropertyStore_Release(props);
    return form;
}

/* ============================================================
 * Device open / close
 * ============================================================ */

wapi_plat_audio_device_t* wapi_plat_audio_open_device(int device_id,
                                                      const wapi_plat_audio_spec_t* spec_hint) {
    (void)spec_hint; /* shared-mode uses the endpoint mix format */

    IMMDeviceEnumerator* e = get_enumerator();
    if (!e) return NULL;

    struct wapi_plat_audio_device_t* d = (struct wapi_plat_audio_device_t*)calloc(1, sizeof(*d));
    if (!d) { IMMDeviceEnumerator_Release(e); return NULL; }

    EDataFlow flow = (device_id == WAPI_PLAT_AUDIO_DEFAULT_RECORDING) ? eCapture : eRender;
    d->is_capture = (flow == eCapture);

    if (device_id < 0) {
        IMMDeviceEnumerator_GetDefaultAudioEndpoint(e, flow, eConsole, &d->mm_device);
    } else {
        IMMDeviceCollection* col = NULL;
        IMMDeviceEnumerator_EnumAudioEndpoints(e, flow, DEVICE_STATE_ACTIVE, &col);
        if (col) {
            IMMDeviceCollection_Item(col, (UINT)device_id, &d->mm_device);
            IMMDeviceCollection_Release(col);
        }
    }
    IMMDeviceEnumerator_Release(e);
    if (!d->mm_device) { free(d); return NULL; }

    if (FAILED(IMMDevice_Activate(d->mm_device, &IID_IAudioClient_local,
                                  CLSCTX_ALL, NULL, (void**)&d->audio_client))) {
        IMMDevice_Release(d->mm_device);
        free(d);
        return NULL;
    }

    if (FAILED(IAudioClient_GetMixFormat(d->audio_client, &d->mix_format))) {
        IAudioClient_Release(d->audio_client);
        IMMDevice_Release(d->mm_device);
        free(d);
        return NULL;
    }

    /* 40ms buffer (latency target; WASAPI may round up) */
    REFERENCE_TIME period = 40 * 10000;
    DWORD init_flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK
                     | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
                     | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
    HRESULT hr = IAudioClient_Initialize(d->audio_client,
                                         AUDCLNT_SHAREMODE_SHARED,
                                         init_flags,
                                         period, 0,
                                         d->mix_format, NULL);
    if (FAILED(hr)) {
        CoTaskMemFree(d->mix_format);
        IAudioClient_Release(d->audio_client);
        IMMDevice_Release(d->mm_device);
        free(d);
        return NULL;
    }

    IAudioClient_GetBufferSize(d->audio_client, &d->buffer_frames);
    InitializeCriticalSection(&d->streams_lock);
    return d;
}

void wapi_plat_audio_close_device(wapi_plat_audio_device_t* d) {
    if (!d) return;
    /* Unbind all streams, tearing down the worker via the last unbind. */
    for (;;) {
        EnterCriticalSection(&d->streams_lock);
        struct wapi_plat_audio_stream_t* s = d->num_streams ? d->streams[0] : NULL;
        LeaveCriticalSection(&d->streams_lock);
        if (!s) break;
        wapi_plat_audio_stream_unbind(s);
    }
    if (d->started) IAudioClient_Stop(d->audio_client);
    if (d->wake_event)   { CloseHandle(d->wake_event);   d->wake_event = NULL; }
    if (d->stop_event)   { CloseHandle(d->stop_event);   d->stop_event = NULL; }
    DeleteCriticalSection(&d->streams_lock);
    if (d->mix_format)    CoTaskMemFree(d->mix_format);
    if (d->audio_client)  IAudioClient_Release(d->audio_client);
    if (d->mm_device)     IMMDevice_Release(d->mm_device);
    free(d);
}

void wapi_plat_audio_pause_device(wapi_plat_audio_device_t* d) {
    if (d && d->started) {
        IAudioClient_Stop(d->audio_client);
        d->started = false;
    }
}

void wapi_plat_audio_resume_device(wapi_plat_audio_device_t* d) {
    if (d && !d->started) {
        IAudioClient_Start(d->audio_client);
        d->started = true;
    }
}

/* ============================================================
 * Stream create / destroy
 * ============================================================ */

wapi_plat_audio_stream_t* wapi_plat_audio_stream_create(const wapi_plat_audio_spec_t* src,
                                                        const wapi_plat_audio_spec_t* dst) {
    struct wapi_plat_audio_stream_t* s = (struct wapi_plat_audio_stream_t*)calloc(1, sizeof(*s));
    if (!s) return NULL;
    if (src) s->src = *src; else { s->src.format = WAPI_PLAT_AUDIO_F32; s->src.channels = 2; s->src.freq = 48000; }
    if (dst) s->dst = *dst; else { s->dst = s->src; }

    /* Ring sized for ~1s of dst-format audio */
    uint32_t bpf = bytes_per_frame(&s->dst);
    if (bpf == 0) bpf = 8;
    uint32_t want = bpf * (uint32_t)s->dst.freq;
    if (want < 16384) want = 16384;
    if (!ring_init(&s->ring, want)) { free(s); return NULL; }

    return s;
}

void wapi_plat_audio_stream_destroy(wapi_plat_audio_stream_t* s) {
    if (!s) return;
    wapi_plat_audio_stream_unbind(s);
    ring_free(&s->ring);
    free(s);
}

/* ============================================================
 * Stream bind / unbind
 * ============================================================ */

/* Spin up the device's shared WASAPI client + worker thread on first
 * stream bind. Subsequent binds just attach to the running worker. */
static bool device_ensure_worker(wapi_plat_audio_device_t* d) {
    if (d->worker_thread) return true;

    if (!d->wake_event)  d->wake_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!d->stop_event)  d->stop_event = CreateEventW(NULL, TRUE,  FALSE, NULL);
    if (!d->wake_event || !d->stop_event) return false;

    if (FAILED(IAudioClient_SetEventHandle(d->audio_client, d->wake_event))) return false;

    if (d->is_capture) {
        if (FAILED(IAudioClient_GetService(d->audio_client, &IID_IAudioCaptureClient_local,
                                           (void**)&d->capture_client))) return false;
    } else {
        if (FAILED(IAudioClient_GetService(d->audio_client, &IID_IAudioRenderClient_local,
                                           (void**)&d->render_client))) return false;
    }

    ResetEvent(d->stop_event);
    DWORD tid = 0;
    d->worker_thread = CreateThread(NULL, 0, audio_worker, d, 0, &tid);
    if (!d->worker_thread) {
        if (d->render_client)  { IAudioRenderClient_Release(d->render_client);  d->render_client  = NULL; }
        if (d->capture_client) { IAudioCaptureClient_Release(d->capture_client); d->capture_client = NULL; }
        return false;
    }

    if (!d->started) {
        IAudioClient_Start(d->audio_client);
        d->started = true;
    }
    return true;
}

/* Tear down the worker + client when the last stream unbinds. */
static void device_teardown_worker(wapi_plat_audio_device_t* d) {
    if (!d->worker_thread) return;
    SetEvent(d->stop_event);
    WaitForSingleObject(d->worker_thread, 2000);
    CloseHandle(d->worker_thread);
    d->worker_thread = NULL;
    if (d->render_client)  { IAudioRenderClient_Release(d->render_client);  d->render_client  = NULL; }
    if (d->capture_client) { IAudioCaptureClient_Release(d->capture_client); d->capture_client = NULL; }
}

bool wapi_plat_audio_stream_bind(wapi_plat_audio_device_t* d,
                                 wapi_plat_audio_stream_t* s) {
    if (!d || !s || s->device) return false;

    /* Device's actual format becomes our dst. Every bound stream shares
     * it so the mixer can sum samples without per-stream rate conversion. */
    waveformat_to_spec(d->mix_format, &s->dst);

    uint32_t bpf = d->is_capture ? bytes_per_frame(&s->src) : bytes_per_frame(&s->dst);
    if (bpf == 0) bpf = 8;
    uint32_t want = bpf * (uint32_t)s->dst.freq;
    if (s->ring.cap < want) {
        ring_free(&s->ring);
        if (!ring_init(&s->ring, want)) return false;
    }

    EnterCriticalSection(&d->streams_lock);
    if (d->num_streams >= WAPI_PLAT_AUDIO_MAX_STREAMS_PER_DEVICE) {
        LeaveCriticalSection(&d->streams_lock);
        return false;
    }
    d->streams[d->num_streams++] = s;
    LeaveCriticalSection(&d->streams_lock);

    s->device = d;

    if (!device_ensure_worker(d)) {
        /* Roll back the append on startup failure. */
        EnterCriticalSection(&d->streams_lock);
        for (int i = 0; i < d->num_streams; i++) {
            if (d->streams[i] == s) {
                d->streams[i] = d->streams[--d->num_streams];
                break;
            }
        }
        LeaveCriticalSection(&d->streams_lock);
        s->device = NULL;
        return false;
    }
    return true;
}

void wapi_plat_audio_stream_unbind(wapi_plat_audio_stream_t* s) {
    if (!s || !s->device) return;
    wapi_plat_audio_device_t* d = s->device;
    EnterCriticalSection(&d->streams_lock);
    for (int i = 0; i < d->num_streams; i++) {
        if (d->streams[i] == s) {
            d->streams[i] = d->streams[--d->num_streams];
            break;
        }
    }
    int remaining = d->num_streams;
    LeaveCriticalSection(&d->streams_lock);
    s->device = NULL;

    if (remaining == 0) device_teardown_worker(d);
}

void wapi_plat_audio_device_describe(wapi_plat_audio_device_t* d,
                                     wapi_plat_audio_spec_t* out_native,
                                     uint32_t* out_form,
                                     uint8_t out_uid[16],
                                     char* name_buf, size_t name_buf_cap,
                                     size_t* out_name_len) {
    if (out_native) {
        if (d && d->mix_format) waveformat_to_spec(d->mix_format, out_native);
        else memset(out_native, 0, sizeof(*out_native));
    }
    if (out_form) {
        *out_form = (d && d->mm_device) ? immdevice_form_factor(d->mm_device) : 0;
    }
    if (out_uid) {
        memset(out_uid, 0, 16);
        if (d && d->mm_device) {
            /* Use the IMMDevice pointer as a per-session stable id until
             * PKEY_Device_ContainerId / DeviceInterface path folding lands. */
            uintptr_t p = (uintptr_t)d->mm_device;
            memcpy(out_uid, &p, sizeof(p) < 16 ? sizeof(p) : 16);
        }
    }
    size_t written = 0;
    if (name_buf && name_buf_cap > 0 && d && d->mm_device) {
        written = immdevice_friendly_name(d->mm_device, name_buf, name_buf_cap);
    }
    if (out_name_len) *out_name_len = written;
}

/* ============================================================
 * Stream put / get
 * ============================================================ */

bool wapi_plat_audio_stream_put(wapi_plat_audio_stream_t* s, const void* data, int len) {
    if (!s || !data || len <= 0) return true;
    uint32_t src_bpf = bytes_per_frame(&s->src);
    if (src_bpf == 0) return false;
    uint32_t in_frames = (uint32_t)len / src_bpf;
    if (in_frames == 0) return true;

    /* Convert in blocks so we don't blow the stack */
    enum { BLOCK_FRAMES = 1024 };
    uint8_t convbuf[BLOCK_FRAMES * 16]; /* up to 16 bytes/frame */

    const uint8_t* src = (const uint8_t*)data;
    uint32_t remaining = in_frames;
    while (remaining > 0) {
        uint32_t chunk = remaining > BLOCK_FRAMES ? BLOCK_FRAMES : remaining;
        uint32_t wrote = convert_frames(&s->src, &s->dst, src, chunk, convbuf);
        if (wrote == 0) return false;

        /* Block-wait on ring space so guest doesn't lose data */
        while (ring_free_space(&s->ring) < wrote) {
            Sleep(1);
        }
        ring_write(&s->ring, convbuf, wrote);

        src       += chunk * src_bpf;
        remaining -= chunk;
    }
    return true;
}

int wapi_plat_audio_stream_get(wapi_plat_audio_stream_t* s, void* data, int len) {
    if (!s || !data || len <= 0) return 0;
    /* Capture path: ring holds guest-format (src) bytes produced by
     * the worker. Playback streams have no data to return. */
    if (!s->device || !s->device->is_capture) return 0;
    return (int)ring_read(&s->ring, data, (uint32_t)len);
}

int wapi_plat_audio_stream_available(wapi_plat_audio_stream_t* s) {
    if (!s) return 0;
    return (int)ring_count(&s->ring);
}

int wapi_plat_audio_stream_queued(wapi_plat_audio_stream_t* s) {
    if (!s) return 0;
    return (int)ring_count(&s->ring);
}
