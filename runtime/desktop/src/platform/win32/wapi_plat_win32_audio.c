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
static const IID IID_IAudioClient_local       = { 0x1CB9AD4C, 0xDBFA, 0x4c32, { 0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2 } };
static const IID IID_IAudioRenderClient_local = { 0xF294ACFC, 0x3146, 0x4483, { 0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2 } };
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

struct wapi_plat_audio_device_t {
    IMMDevice*    mm_device;
    IAudioClient* audio_client;
    WAVEFORMATEX* mix_format;      /* endpoint format */
    UINT32        buffer_frames;
    bool          started;
    bool          is_capture;
    /* Streams bound to this device (phase 2: one stream per device) */
    struct wapi_plat_audio_stream_t* stream;
};

struct wapi_plat_audio_stream_t {
    /* src spec (what the guest supplies) */
    wapi_plat_audio_spec_t src;
    /* dst spec (what the device actually wants) */
    wapi_plat_audio_spec_t dst;

    byte_ring_t ring;              /* holds dst-format bytes, ready to play */

    /* Worker thread */
    HANDLE thread;
    HANDLE wake_event;             /* WASAPI event-driven callback */
    HANDLE stop_event;             /* set to end worker */

    /* When bound, these point into the device */
    struct wapi_plat_audio_device_t* device;
    IAudioRenderClient* render_client;
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
 * Worker thread: pulls from ring, pushes into WASAPI endpoint
 * ============================================================ */

static DWORD WINAPI audio_worker(LPVOID param) {
    struct wapi_plat_audio_stream_t* s = (struct wapi_plat_audio_stream_t*)param;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    DWORD task_index = 0;
    HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);

    HANDLE waits[2] = { s->stop_event, s->wake_event };
    uint32_t bpf_dst = bytes_per_frame(&s->dst);

    for (;;) {
        DWORD rc = WaitForMultipleObjects(2, waits, FALSE, 2000);
        if (rc == WAIT_OBJECT_0) break; /* stop */
        /* rc==WAIT_OBJECT_0+1 or WAIT_TIMEOUT — either way, try to fill */

        UINT32 padding = 0;
        if (FAILED(IAudioClient_GetCurrentPadding(s->device->audio_client, &padding))) continue;
        UINT32 avail_frames = s->device->buffer_frames - padding;
        if (avail_frames == 0) continue;

        BYTE* dst = NULL;
        if (FAILED(IAudioRenderClient_GetBuffer(s->render_client, avail_frames, &dst))) continue;

        UINT32 bytes_needed = avail_frames * bpf_dst;
        UINT32 got = ring_read(&s->ring, dst, bytes_needed);
        DWORD flags = 0;
        if (got < bytes_needed) {
            /* Silence the rest to avoid audible glitches on underrun */
            memset(dst + got, 0, bytes_needed - got);
            flags = 0; /* don't set AUDCLNT_BUFFERFLAGS_SILENT, it breaks on some drivers */
        }
        IAudioRenderClient_ReleaseBuffer(s->render_client, avail_frames, flags);
    }

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

int wapi_plat_audio_playback_device_count(void) {
    IMMDeviceEnumerator* e = get_enumerator();
    if (!e) return 0;
    IMMDeviceCollection* col = NULL;
    if (FAILED(IMMDeviceEnumerator_EnumAudioEndpoints(e, eRender, DEVICE_STATE_ACTIVE, &col))) {
        IMMDeviceEnumerator_Release(e);
        return 0;
    }
    UINT n = 0;
    IMMDeviceCollection_GetCount(col, &n);
    IMMDeviceCollection_Release(col);
    IMMDeviceEnumerator_Release(e);
    return (int)n;
}

int wapi_plat_audio_recording_device_count(void) {
    IMMDeviceEnumerator* e = get_enumerator();
    if (!e) return 0;
    IMMDeviceCollection* col = NULL;
    if (FAILED(IMMDeviceEnumerator_EnumAudioEndpoints(e, eCapture, DEVICE_STATE_ACTIVE, &col))) {
        IMMDeviceEnumerator_Release(e);
        return 0;
    }
    UINT n = 0;
    IMMDeviceCollection_GetCount(col, &n);
    IMMDeviceCollection_Release(col);
    IMMDeviceEnumerator_Release(e);
    return (int)n;
}

size_t wapi_plat_audio_device_name(int device_id, char* out, size_t out_len) {
    IMMDeviceEnumerator* e = get_enumerator();
    if (!e) return 0;
    IMMDevice* dev = NULL;

    if (device_id == WAPI_PLAT_AUDIO_DEFAULT_PLAYBACK) {
        IMMDeviceEnumerator_GetDefaultAudioEndpoint(e, eRender, eConsole, &dev);
    } else if (device_id == WAPI_PLAT_AUDIO_DEFAULT_RECORDING) {
        IMMDeviceEnumerator_GetDefaultAudioEndpoint(e, eCapture, eConsole, &dev);
    } else {
        IMMDeviceCollection* col = NULL;
        IMMDeviceEnumerator_EnumAudioEndpoints(e, eRender, DEVICE_STATE_ACTIVE, &col);
        if (col) {
            IMMDeviceCollection_Item(col, (UINT)device_id, &dev);
            IMMDeviceCollection_Release(col);
        }
    }
    IMMDeviceEnumerator_Release(e);
    if (!dev) return 0;

    IPropertyStore* props = NULL;
    if (FAILED(IMMDevice_OpenPropertyStore(dev, STGM_READ, &props))) {
        IMMDevice_Release(dev);
        return 0;
    }

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
    IMMDevice_Release(dev);
    return nbytes;
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
    return d;
}

void wapi_plat_audio_close_device(wapi_plat_audio_device_t* d) {
    if (!d) return;
    if (d->stream) wapi_plat_audio_stream_unbind(d->stream);
    if (d->started) IAudioClient_Stop(d->audio_client);
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

    s->wake_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    s->stop_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!s->wake_event || !s->stop_event) {
        if (s->wake_event) CloseHandle(s->wake_event);
        if (s->stop_event) CloseHandle(s->stop_event);
        ring_free(&s->ring);
        free(s);
        return NULL;
    }

    return s;
}

void wapi_plat_audio_stream_destroy(wapi_plat_audio_stream_t* s) {
    if (!s) return;
    wapi_plat_audio_stream_unbind(s);
    if (s->wake_event) CloseHandle(s->wake_event);
    if (s->stop_event) CloseHandle(s->stop_event);
    ring_free(&s->ring);
    free(s);
}

/* ============================================================
 * Stream bind / unbind
 * ============================================================ */

bool wapi_plat_audio_stream_bind(wapi_plat_audio_device_t* d,
                                 wapi_plat_audio_stream_t* s) {
    if (!d || !s || d->stream) return false;
    if (d->is_capture) return false;

    /* Device's actual format becomes our dst */
    waveformat_to_spec(d->mix_format, &s->dst);

    /* Resize ring if bpf changed */
    uint32_t bpf = bytes_per_frame(&s->dst);
    if (bpf == 0) bpf = 8;
    uint32_t want = bpf * (uint32_t)s->dst.freq;
    if (s->ring.cap < want) {
        ring_free(&s->ring);
        if (!ring_init(&s->ring, want)) return false;
    }

    if (FAILED(IAudioClient_SetEventHandle(d->audio_client, s->wake_event))) return false;
    if (FAILED(IAudioClient_GetService(d->audio_client, &IID_IAudioRenderClient_local,
                                       (void**)&s->render_client))) return false;

    s->device = d;
    d->stream = s;

    ResetEvent(s->stop_event);
    DWORD tid = 0;
    s->thread = CreateThread(NULL, 0, audio_worker, s, 0, &tid);
    if (!s->thread) {
        IAudioRenderClient_Release(s->render_client);
        s->render_client = NULL;
        s->device = NULL;
        d->stream = NULL;
        return false;
    }

    if (!d->started) {
        IAudioClient_Start(d->audio_client);
        d->started = true;
    }
    return true;
}

void wapi_plat_audio_stream_unbind(wapi_plat_audio_stream_t* s) {
    if (!s || !s->device) return;
    SetEvent(s->stop_event);
    if (s->thread) {
        WaitForSingleObject(s->thread, 2000);
        CloseHandle(s->thread);
        s->thread = NULL;
    }
    if (s->render_client) {
        IAudioRenderClient_Release(s->render_client);
        s->render_client = NULL;
    }
    if (s->device->stream == s) s->device->stream = NULL;
    s->device = NULL;
}

bool wapi_plat_audio_open_device_stream(int device_id,
                                        const wapi_plat_audio_spec_t* spec,
                                        wapi_plat_audio_device_t** out_dev,
                                        wapi_plat_audio_stream_t** out_stream) {
    wapi_plat_audio_device_t* d = wapi_plat_audio_open_device(device_id, spec);
    if (!d) return false;

    wapi_plat_audio_spec_t dst;
    waveformat_to_spec(d->mix_format, &dst);

    wapi_plat_audio_stream_t* s = wapi_plat_audio_stream_create(spec, &dst);
    if (!s) { wapi_plat_audio_close_device(d); return false; }

    if (!wapi_plat_audio_stream_bind(d, s)) {
        wapi_plat_audio_stream_destroy(s);
        wapi_plat_audio_close_device(d);
        return false;
    }

    *out_dev = d;
    *out_stream = s;
    return true;
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
    /* Capture not implemented in phase 2 */
    (void)s; (void)data; (void)len;
    return 0;
}

int wapi_plat_audio_stream_available(wapi_plat_audio_stream_t* s) {
    if (!s) return 0;
    return (int)ring_count(&s->ring);
}

int wapi_plat_audio_stream_queued(wapi_plat_audio_stream_t* s) {
    if (!s) return 0;
    return (int)ring_count(&s->ring);
}
