/**
 * WAPI hello_game — dodge-the-reds with coins, audio chirps, gamepad,
 * compressed sprites, and a child AI module.
 *
 * Capabilities exercised:
 *   wapi.surface / wapi.window — WAPI surface + title bar.
 *   wapi.gpu + webgpu          — textured sprite pipeline.
 *   wapi.input                 — keyboard + gamepad.
 *   wapi.audio                 — square-wave beeps for coin / hit.
 *   wapi.random                — seed enemy spawn direction.
 *   wapi.compression           — inflate the embedded sprite atlas at startup.
 *   wapi.module                — load + call hello_game_ai.wasm each frame.
 *
 * Coordinate system: screen pixels, origin top-left. The shader maps
 * straight to clip space using a uniform with the surface size.
 */

#include <stddef.h>

/* Freestanding targets don't ship libc intrinsics. Clang is free to
 * generate calls to memcpy / memset; provide local implementations so
 * the wasm link has no env.memcpy imports. */
void* memcpy(void* dst, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}
void* memset(void* dst, int v, size_t n) {
    unsigned char* d = (unsigned char*)dst;
    for (size_t i = 0; i < n; i++) d[i] = (unsigned char)v;
    return dst;
}

#include <wapi/wapi.h>
#include <wapi/wapi_surface.h>
#include <wapi/wapi_window.h>
#include <wapi/wapi_input.h>
#include <wapi/wapi_gpu.h>
#include <wapi/wapi_audio.h>
#include <wapi/wapi_random.h>
#include <wapi/wapi_compression.h>
#include <wapi/wapi_module.h>
#include <webgpu/webgpu.h>

#include "assets.h"
#include "ai_hash.h"
#include "tracker_types.h"
#include "tracker_hash.h"

/* ---- game tunables ---- */
#define PLAY_W            800
#define PLAY_H            600
#define MAX_ENEMIES       32
#define COIN_LIFETIME_F   300
#define PLAYER_SPEED      5.0f
#define AUDIO_RATE        22050
#define BEEP_SAMPLES      4096  /* ~186ms per beep — one stream of S16 mono */

/* Soundtrack: 8-second loop at AUDIO_RATE, rendered once by the tracker
 * child module then streamed in small chunks each frame. */
#define MUSIC_LOOP_SECONDS   8
#define MUSIC_LOOP_SAMPLES   (AUDIO_RATE * MUSIC_LOOP_SECONDS)
#define MUSIC_CHUNK_SAMPLES  2048   /* ~93ms at 22050Hz */
#define MUSIC_QUEUE_TARGET   (MUSIC_CHUNK_SAMPLES * 4 * 2) /* keep ~370ms queued */

/* ---- shared AI state (layout mirrored in ai.c) ---- */
typedef struct enemy_t {
    float    x, y;
    float    vx, vy;
    uint32_t color;  /* unused server-side, untouched by AI */
    uint32_t alive;
} enemy_t;

/* ---- vertex buffer layout (stride 20B) ----
 *   pos: vec2f (px)
 *   uv:  vec2f (atlas px)
 *   rgba: u8x4 unorm (tint)  */
typedef struct sprite_vtx_t {
    float   x, y;
    float   u, v;
    uint8_t r, g, b, a;
} sprite_vtx_t;

static const wapi_io_t* g_io;

/* Surface / GPU */
static wapi_handle_t    g_surface;
static int32_t          g_width, g_height;
static WGPUInstance     g_instance;
static WGPUAdapter      g_adapter;
static WGPUDevice       g_device;
static WGPUQueue        g_queue;
static WGPUSurface      g_wgpu_surface;
static WGPUTextureFormat g_surface_fmt;
static WGPUShaderModule g_shader;
static WGPURenderPipeline g_pipeline;
static WGPUBindGroupLayout g_bgl;
static WGPUBindGroup    g_bg;
static WGPUPipelineLayout g_plyt;
static WGPUBuffer       g_vbuf, g_ibuf, g_ubuf;
static WGPUTexture      g_atlas_tex;
static WGPUTextureView  g_atlas_view;
static WGPUSampler      g_atlas_sampler;

/* Audio */
static wapi_handle_t    g_audio_dev, g_audio_stream;
static wapi_handle_t    g_music_stream;

/* Tracker child module + baked loop buffer. */
typedef enum {
    TRK_IDLE = 0,
    TRK_JOINING,    /* join() returning WAPI_ERR_AGAIN while async fetch runs */
    TRK_READY,      /* loop rendered, music pumping */
    TRK_FAILED,     /* gave up; game runs silent (sfx still works) */
} tracker_state_t;
static tracker_state_t  g_tracker_state;
static int              g_tracker_tries;
static wapi_handle_t    g_tracker_mod;
static wapi_handle_t    g_tracker_render_fn;
static int16_t          g_music_loop[MUSIC_LOOP_SAMPLES];
static int              g_music_ready;
static uint32_t         g_music_cursor;

/* Input device handles (opened at startup). */
static wapi_handle_t    g_kb_handle;
static wapi_handle_t    g_gpad_handle;   /* XInput slot 0 when connected */

/* Module linking */
static wapi_handle_t    g_ai_module;
static wapi_handle_t    g_ai_tick_fn;
static uint64_t         g_enemy_shared_off;  /* offset into wapi_module shared memory */

/* Game state */
static uint32_t g_rng_state;
static float    g_player_x, g_player_y;
static int      g_lives;
static int      g_game_over;  /* 1 = player dead, waiting for input to restart */
static uint64_t g_sim_accum_ns;

static void reset_round(void);
static void update_viewport(void);
static void reconfigure_surface(void);
static int      g_score;
static int      g_coin_active;
static float    g_coin_x, g_coin_y;
static int      g_coin_timer;
static int      g_frame;
static int      g_running;
static int      g_hit_audio_until;
static int      g_coin_audio_until;

/* Sprite staging — rebuilt every frame, pushed via wgpuQueueWriteBuffer. */
#define MAX_SPRITES 64
#define MAX_VERTS   (MAX_SPRITES * 4)
#define MAX_INDICES (MAX_SPRITES * 6)

static sprite_vtx_t g_verts[MAX_VERTS];
static uint16_t     g_indices[MAX_INDICES];
static int          g_vtx_count;
static int          g_idx_count;

/* ============================================================
 * WGSL — textured sprites
 * ============================================================ */

static const char g_wgsl[] =
"struct U { atlas_size: vec2f, scale: vec2f, bias: vec2f, _pad: vec2f };\n"
"@group(0) @binding(0) var<uniform> u: U;\n"
"@group(0) @binding(1) var atlas: texture_2d<f32>;\n"
"@group(0) @binding(2) var samp : sampler;\n"
"struct VIn {\n"
"  @location(0) pos: vec2f,\n"
"  @location(1) uv:  vec2f,\n"
"  @location(2) rgba: vec4f,\n"
"};\n"
"struct VOut {\n"
"  @builtin(position) clip: vec4f,\n"
"  @location(0) uv: vec2f,\n"
"  @location(1) rgba: vec4f,\n"
"};\n"
"@vertex fn vs(v: VIn) -> VOut {\n"
"  var o: VOut;\n"
"  let ndc = vec2f(u.bias.x + v.pos.x * u.scale.x,\n"
"                  u.bias.y - v.pos.y * u.scale.y);\n"
"  o.clip = vec4f(ndc, 0.0, 1.0);\n"
"  o.uv   = v.uv / u.atlas_size;\n"
"  o.rgba = v.rgba;\n"
"  return o;\n"
"}\n"
"@fragment fn fs(in: VOut) -> @location(0) vec4f {\n"
"  return textureSample(atlas, samp, in.uv) * in.rgba;\n"
"}\n";

/* ============================================================
 * Small helpers
 * ============================================================ */

static uint32_t xorshift32(void) {
    uint32_t x = g_rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_rng_state = x;
    return x;
}

static void push_sprite(float x, float y, int tile, uint32_t rgba, float size) {
    if (g_vtx_count + 4 > MAX_VERTS) return;
    int base = g_vtx_count;
    float atlas_x = (float)(tile * ATLAS_TILE_SIZE);
    float atlas_y = 0.0f;
    float t = (float)ATLAS_TILE_SIZE;
    uint8_t r = (uint8_t)(rgba >> 24);
    uint8_t gg = (uint8_t)(rgba >> 16);
    uint8_t b = (uint8_t)(rgba >> 8);
    uint8_t a = (uint8_t)(rgba);
    g_verts[base + 0] = (sprite_vtx_t){ x,        y,        atlas_x,     atlas_y,     r, gg, b, a };
    g_verts[base + 1] = (sprite_vtx_t){ x + size, y,        atlas_x + t, atlas_y,     r, gg, b, a };
    g_verts[base + 2] = (sprite_vtx_t){ x + size, y + size, atlas_x + t, atlas_y + t, r, gg, b, a };
    g_verts[base + 3] = (sprite_vtx_t){ x,        y + size, atlas_x,     atlas_y + t, r, gg, b, a };
    g_indices[g_idx_count + 0] = (uint16_t)(base + 0);
    g_indices[g_idx_count + 1] = (uint16_t)(base + 1);
    g_indices[g_idx_count + 2] = (uint16_t)(base + 2);
    g_indices[g_idx_count + 3] = (uint16_t)(base + 0);
    g_indices[g_idx_count + 4] = (uint16_t)(base + 2);
    g_indices[g_idx_count + 5] = (uint16_t)(base + 3);
    g_vtx_count += 4;
    g_idx_count += 6;
}

/* ============================================================
 * Audio: generate a square-wave beep into the stream.
 * ============================================================ */

static void play_beep(int freq_hz, float vol) {
    static int16_t buf[BEEP_SAMPLES];
    int period = AUDIO_RATE / freq_hz;
    if (period < 2) period = 2;
    int16_t amp = (int16_t)(32767.0f * vol);
    for (int i = 0; i < BEEP_SAMPLES; i++) {
        /* Taper the amplitude so successive beeps don't hard-clip. */
        float env = 1.0f - (float)i / (float)BEEP_SAMPLES;
        int16_t s = ((i / (period / 2)) & 1) ? amp : (int16_t)-amp;
        buf[i] = (int16_t)(s * env);
    }
    wapi_audio_put_stream_data(g_audio_stream, buf, sizeof(buf));
}

/* ============================================================
 * Sprite atlas decompression via wapi_compression_process
 * ============================================================ */

static uint8_t g_atlas_rgba[ATLAS_DECOMPRESSED_BYTES];

static wapi_result_t decompress_atlas(void) {
    int32_t sub = wapi_compression_process(g_io,
        g_atlas_deflate, sizeof(g_atlas_deflate),
        g_atlas_rgba, sizeof(g_atlas_rgba),
        WAPI_COMPRESS_DEFLATE, WAPI_COMPRESS_DECOMPRESS,
        /* user_data */ 1);
    if (sub != 1) return WAPI_ERR_UNKNOWN;

    /* Drive the io reactor until the completion arrives. */
    wapi_event_t ev;
    for (int tries = 0; tries < 64; tries++) {
        if (g_io->poll(g_io->impl, &ev)) {
            if (ev.type == WAPI_EVENT_IO_COMPLETION && ev.io.user_data == 1) {
                return (ev.io.result == (int32_t)ATLAS_DECOMPRESSED_BYTES)
                       ? WAPI_OK : WAPI_ERR_IO;
            }
        }
    }
    return WAPI_ERR_TIMEDOUT;
}

/* ============================================================
 * Adapter / device callbacks
 * ============================================================ */

typedef struct { WGPUAdapter a; int done; WGPURequestAdapterStatus st; } adapter_req_t;
typedef struct { WGPUDevice  d; int done; WGPURequestDeviceStatus  st; } device_req_t;

static void on_adapter(WGPURequestAdapterStatus st, WGPUAdapter a, WGPUStringView m,
                       void* u1, void* u2) {
    (void)m; (void)u2;
    adapter_req_t* r = (adapter_req_t*)u1;
    r->a = a; r->st = st; r->done = 1;
}
static void on_device(WGPURequestDeviceStatus st, WGPUDevice d, WGPUStringView m,
                      void* u1, void* u2) {
    (void)m; (void)u2;
    device_req_t* r = (device_req_t*)u1;
    r->d = d; r->st = st; r->done = 1;
}

/* ============================================================
 * Initialization
 * ============================================================ */

static wapi_result_t init_gpu(void) {
    /* Window / WAPI surface. */
    wapi_window_desc_t wcfg = {
        .chain = { .next = 0, .sType = WAPI_STYPE_WINDOW_CONFIG },
        .title = WAPI_STR("Hello Game"),
        .window_flags = WAPI_WINDOW_FLAG_RESIZABLE,
    };
    wapi_surface_desc_t sdesc = {
        .nextInChain = (uint64_t)(uintptr_t)&wcfg,
        .width = PLAY_W, .height = PLAY_H,
        .flags = WAPI_SURFACE_FLAG_HIGH_DPI,
    };
    wapi_result_t res = wapi_surface_create(&sdesc, &g_surface);
    if (WAPI_FAILED(res)) return res;
    wapi_surface_get_size(g_surface, &g_width, &g_height);

    /* Instance / adapter / device. */
    WGPUInstanceDescriptor idesc = {0};
    g_instance = wgpuCreateInstance(&idesc);
    if (!g_instance) return WAPI_ERR_NOTCAPABLE;

    WGPURequestAdapterOptions aopts = {0};
    aopts.powerPreference = WGPUPowerPreference_HighPerformance;
    adapter_req_t areq = {0};
    WGPURequestAdapterCallbackInfo acb = {0};
    acb.mode = WGPUCallbackMode_AllowProcessEvents;
    acb.callback = on_adapter; acb.userdata1 = &areq;
    wgpuInstanceRequestAdapter(g_instance, &aopts, acb);
    while (!areq.done) wgpuInstanceProcessEvents(g_instance);
    if (areq.st != WGPURequestAdapterStatus_Success) return WAPI_ERR_NOTCAPABLE;
    g_adapter = areq.a;

    WGPUDeviceDescriptor ddesc = {0};
    ddesc.label.data = "hello_game"; ddesc.label.length = 10;
    device_req_t dreq = {0};
    WGPURequestDeviceCallbackInfo dcb = {0};
    dcb.mode = WGPUCallbackMode_AllowProcessEvents;
    dcb.callback = on_device; dcb.userdata1 = &dreq;
    wgpuAdapterRequestDevice(g_adapter, &ddesc, dcb);
    while (!dreq.done) wgpuInstanceProcessEvents(g_instance);
    if (dreq.st != WGPURequestDeviceStatus_Success) return WAPI_ERR_NOTCAPABLE;
    g_device = dreq.d;
    g_queue  = wgpuDeviceGetQueue(g_device);

    /* WGPUSurface over the WAPI surface. */
    wapi_gpu_surface_source_t src = {
        .chain = { .next = 0, .sType = WAPI_STYPE_GPU_SURFACE_SOURCE_WAPI },
        .surface = g_surface,
    };
    WGPUSurfaceDescriptor surf_desc = {0};
    surf_desc.nextInChain = (WGPUChainedStruct*)&src;
    g_wgpu_surface = wgpuInstanceCreateSurface(g_instance, &surf_desc);
    if (!g_wgpu_surface) return WAPI_ERR_UNKNOWN;

    /* Prefer a non-sRGB swapchain format so the shader's output reaches
     * the screen unchanged. caps.formats[0] is host-dependent (wgpu-native
     * on Windows lists *-Srgb first, browser WebGPU lists non-sRGB first),
     * which makes identical shader output display with different gamma
     * across runtimes. Walk caps and pick the first non-sRGB; fall back
     * to BGRA8Unorm if none qualify. */
    WGPUSurfaceCapabilities caps = {0};
    wgpuSurfaceGetCapabilities(g_wgpu_surface, g_adapter, &caps);
    g_surface_fmt = WGPUTextureFormat_BGRA8Unorm;
    for (size_t i = 0; i < caps.formatCount; i++) {
        WGPUTextureFormat f = caps.formats[i];
        if (f == WGPUTextureFormat_BGRA8Unorm || f == WGPUTextureFormat_RGBA8Unorm) {
            g_surface_fmt = f;
            break;
        }
    }

    WGPUSurfaceConfiguration sc = {0};
    sc.device = g_device;
    sc.format = g_surface_fmt;
    sc.usage = WGPUTextureUsage_RenderAttachment;
    sc.width = (uint32_t)g_width;
    sc.height = (uint32_t)g_height;
    sc.presentMode = WGPUPresentMode_Fifo;
    sc.alphaMode = WGPUCompositeAlphaMode_Auto;
    wgpuSurfaceConfigure(g_wgpu_surface, &sc);

    /* ---- Atlas texture ---- */
    WGPUTextureDescriptor td = {0};
    td.label.data = "atlas"; td.label.length = 5;
    td.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    td.dimension = WGPUTextureDimension_2D;
    td.size.width = ATLAS_WIDTH;
    td.size.height = ATLAS_HEIGHT;
    td.size.depthOrArrayLayers = 1;
    td.format = WGPUTextureFormat_RGBA8Unorm;
    td.mipLevelCount = 1;
    td.sampleCount = 1;
    g_atlas_tex = wgpuDeviceCreateTexture(g_device, &td);

    WGPUTexelCopyTextureInfo dst = {0};
    dst.texture = g_atlas_tex;
    WGPUTexelCopyBufferLayout lay = {0};
    lay.bytesPerRow = ATLAS_WIDTH * 4;
    lay.rowsPerImage = ATLAS_HEIGHT;
    WGPUExtent3D ex = { ATLAS_WIDTH, ATLAS_HEIGHT, 1 };
    wgpuQueueWriteTexture(g_queue, &dst, g_atlas_rgba, sizeof(g_atlas_rgba), &lay, &ex);

    g_atlas_view = wgpuTextureCreateView(g_atlas_tex, 0);

    WGPUSamplerDescriptor sd_samp = {0};
    sd_samp.label.data = "atlas_samp"; sd_samp.label.length = 10;
    sd_samp.addressModeU = WGPUAddressMode_ClampToEdge;
    sd_samp.addressModeV = WGPUAddressMode_ClampToEdge;
    sd_samp.addressModeW = WGPUAddressMode_ClampToEdge;
    sd_samp.magFilter = WGPUFilterMode_Nearest;
    sd_samp.minFilter = WGPUFilterMode_Nearest;
    sd_samp.mipmapFilter = WGPUMipmapFilterMode_Nearest;
    sd_samp.lodMinClamp = 0.0f;
    sd_samp.lodMaxClamp = 0.0f;
    sd_samp.maxAnisotropy = 1;
    g_atlas_sampler = wgpuDeviceCreateSampler(g_device, &sd_samp);

    /* ---- Bind group layout: (0) uniform, (1) texture, (2) sampler ---- */
    WGPUBindGroupLayoutEntry bgle[3] = {0};
    bgle[0].binding = 0;
    bgle[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    bgle[0].buffer.type = WGPUBufferBindingType_Uniform;
    bgle[0].buffer.minBindingSize = 32;
    bgle[1].binding = 1;
    bgle[1].visibility = WGPUShaderStage_Fragment;
    bgle[1].texture.sampleType = WGPUTextureSampleType_Float;
    bgle[1].texture.viewDimension = WGPUTextureViewDimension_2D;
    bgle[2].binding = 2;
    bgle[2].visibility = WGPUShaderStage_Fragment;
    bgle[2].sampler.type = WGPUSamplerBindingType_Filtering;

    WGPUBindGroupLayoutDescriptor bgld = {0};
    bgld.label.data = "sprite_bgl"; bgld.label.length = 10;
    bgld.entryCount = 3;
    bgld.entries = bgle;
    g_bgl = wgpuDeviceCreateBindGroupLayout(g_device, &bgld);

    WGPUPipelineLayoutDescriptor pld = {0};
    pld.label.data = "sprite_pll"; pld.label.length = 10;
    pld.bindGroupLayoutCount = 1;
    pld.bindGroupLayouts = &g_bgl;
    g_plyt = wgpuDeviceCreatePipelineLayout(g_device, &pld);

    /* ---- Uniform buffer (32B: atlas_size, scale, bias, pad) ---- */
    WGPUBufferDescriptor ub = {0};
    ub.label.data = "ubuf"; ub.label.length = 4;
    ub.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    ub.size = 32;
    g_ubuf = wgpuDeviceCreateBuffer(g_device, &ub);

    /* ---- Bind group ---- */
    WGPUBindGroupEntry bge[3] = {0};
    bge[0].binding = 0; bge[0].buffer = g_ubuf; bge[0].offset = 0; bge[0].size = 32;
    bge[1].binding = 1; bge[1].textureView = g_atlas_view;
    bge[2].binding = 2; bge[2].sampler = g_atlas_sampler;
    WGPUBindGroupDescriptor bgd = {0};
    bgd.label.data = "sprite_bg"; bgd.label.length = 9;
    bgd.layout = g_bgl;
    bgd.entryCount = 3;
    bgd.entries = bge;
    g_bg = wgpuDeviceCreateBindGroup(g_device, &bgd);

    /* ---- Vertex / index buffers ---- */
    WGPUBufferDescriptor vb = {0};
    vb.label.data = "vb"; vb.label.length = 2;
    vb.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
    vb.size = sizeof(g_verts);
    g_vbuf = wgpuDeviceCreateBuffer(g_device, &vb);

    WGPUBufferDescriptor ib = {0};
    ib.label.data = "ib"; ib.label.length = 2;
    ib.usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;
    ib.size = sizeof(g_indices);
    g_ibuf = wgpuDeviceCreateBuffer(g_device, &ib);

    /* ---- Shader + pipeline ---- */
    WGPUShaderSourceWGSL wgsl = {0};
    wgsl.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgsl.code.data = g_wgsl;
    wgsl.code.length = sizeof(g_wgsl) - 1;
    WGPUShaderModuleDescriptor smd = {0};
    smd.nextInChain = (WGPUChainedStruct*)&wgsl;
    smd.label.data = "sprite"; smd.label.length = 6;
    g_shader = wgpuDeviceCreateShaderModule(g_device, &smd);

    WGPUVertexAttribute va[3] = {0};
    va[0].format = WGPUVertexFormat_Float32x2; va[0].offset = 0;  va[0].shaderLocation = 0;
    va[1].format = WGPUVertexFormat_Float32x2; va[1].offset = 8;  va[1].shaderLocation = 1;
    va[2].format = WGPUVertexFormat_Unorm8x4;  va[2].offset = 16; va[2].shaderLocation = 2;
    WGPUVertexBufferLayout vbl = {0};
    vbl.arrayStride = sizeof(sprite_vtx_t);
    vbl.stepMode = WGPUVertexStepMode_Vertex;
    vbl.attributeCount = 3;
    vbl.attributes = va;

    WGPUBlendState blend = {0};
    blend.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blend.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blend.color.operation = WGPUBlendOperation_Add;
    blend.alpha.srcFactor = WGPUBlendFactor_One;
    blend.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blend.alpha.operation = WGPUBlendOperation_Add;

    WGPUColorTargetState ct = {0};
    ct.format = g_surface_fmt;
    ct.blend = &blend;
    ct.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fs = {0};
    fs.module = g_shader;
    fs.entryPoint.data = "fs"; fs.entryPoint.length = 2;
    fs.targetCount = 1;
    fs.targets = &ct;

    WGPURenderPipelineDescriptor pd = {0};
    pd.label.data = "sprite_pipe"; pd.label.length = 11;
    pd.layout = g_plyt;
    pd.vertex.module = g_shader;
    pd.vertex.entryPoint.data = "vs"; pd.vertex.entryPoint.length = 2;
    pd.vertex.bufferCount = 1;
    pd.vertex.buffers = &vbl;
    pd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pd.primitive.frontFace = WGPUFrontFace_CCW;
    pd.primitive.cullMode = WGPUCullMode_None;
    pd.multisample.count = 1;
    pd.multisample.mask = 0xFFFFFFFF;
    pd.fragment = &fs;
    g_pipeline = wgpuDeviceCreateRenderPipeline(g_device, &pd);
    if (!g_pipeline) return WAPI_ERR_UNKNOWN;

    update_viewport();
    return WAPI_OK;
}

/* Compute letterbox scale/bias for the current surface size and push the
 * uniform. Preserves the PLAY_W x PLAY_H aspect ratio; the unused margin
 * is left as the clear color. */
static void update_viewport(void) {
    float w = (float)g_width;
    float h = (float)g_height;
    if (w <= 0) w = PLAY_W;
    if (h <= 0) h = PLAY_H;

    float sx = w / (float)PLAY_W;
    float sy = h / (float)PLAY_H;
    float s  = sx < sy ? sx : sy;          /* px per play unit */
    float ox = (w - PLAY_W * s) * 0.5f;    /* letterbox left margin, px */
    float oy = (h - PLAY_H * s) * 0.5f;    /* letterbox top margin, px */

    /* ndc.x = bias.x + pos.x * scale.x  where pos is in play-space px */
    /* ndc.y = bias.y - pos.y * scale.y  (y flipped, origin top-left) */
    float scale_x = 2.0f * s / w;
    float scale_y = 2.0f * s / h;
    float bias_x  = 2.0f * ox / w - 1.0f;
    float bias_y  = 1.0f - 2.0f * oy / h;

    float ub_bytes[8] = {
        (float)ATLAS_WIDTH, (float)ATLAS_HEIGHT,
        scale_x, scale_y,
        bias_x, bias_y,
        0.0f, 0.0f,
    };
    wgpuQueueWriteBuffer(g_queue, g_ubuf, 0, ub_bytes, sizeof(ub_bytes));
}

static void reconfigure_surface(void) {
    WGPUSurfaceConfiguration sc = {0};
    sc.device = g_device;
    sc.format = g_surface_fmt;
    sc.usage = WGPUTextureUsage_RenderAttachment;
    sc.width = (uint32_t)g_width;
    sc.height = (uint32_t)g_height;
    sc.presentMode = WGPUPresentMode_Fifo;
    sc.alphaMode = WGPUCompositeAlphaMode_Auto;
    wgpuSurfaceConfigure(g_wgpu_surface, &sc);
    update_viewport();
}

/* ============================================================
 * Audio setup
 * ============================================================ */

/* Submit one role request and pump the event queue until the matching
 * completion arrives. Returns the per-role wapi_result_t. */
static wapi_result_t request_role_blocking(
    uint32_t kind, uint32_t flags,
    const void* prefs, uint32_t prefs_len,
    wapi_handle_t* out_handle,
    uint64_t tag)
{
    wapi_result_t role_result = WAPI_ERR_AGAIN;
    wapi_role_request_t req = {0};
    req.kind       = kind;
    req.flags      = flags;
    req.prefs_addr = (uint64_t)(uintptr_t)prefs;
    req.prefs_len  = prefs_len;
    req.out_handle = (uint64_t)(uintptr_t)out_handle;
    req.out_result = (uint64_t)(uintptr_t)&role_result;

    wapi_result_t sub = wapi_role_request(g_io, &req, 1, tag);
    if (WAPI_FAILED(sub)) return sub;

    wapi_event_t ev;
    for (int tries = 0; tries < 1024; tries++) {
        if (g_io->poll(g_io->impl, &ev)
            && ev.type == WAPI_EVENT_IO_COMPLETION
            && ev.io.user_data == tag) {
            return role_result;
        }
    }
    return WAPI_ERR_TIMEDOUT;
}

static wapi_result_t init_audio(void) {
    wapi_audio_spec_t spec = {0};
    spec.format   = WAPI_AUDIO_S16;
    spec.channels = 1;
    spec.freq     = AUDIO_RATE;
    wapi_result_t r = request_role_blocking(
        WAPI_ROLE_AUDIO_PLAYBACK, WAPI_ROLE_FOLLOW_DEFAULT,
        &spec, sizeof(spec), &g_audio_dev, 0xA0D10ULL);
    if (WAPI_FAILED(r)) return r;

    r = wapi_audio_create_stream(&spec, NULL, &g_audio_stream);
    if (WAPI_FAILED(r)) { wapi_audio_close(g_audio_dev); return r; }
    r = wapi_audio_bind_stream(g_audio_dev, g_audio_stream);
    if (WAPI_FAILED(r)) {
        wapi_audio_destroy_stream(g_audio_stream);
        wapi_audio_close(g_audio_dev);
        return r;
    }

    /* A second stream carries the tracker-generated music so sfx beeps
     * don't stall behind queued music samples. Both share the endpoint;
     * the host mixes them. */
    r = wapi_audio_create_stream(&spec, NULL, &g_music_stream);
    if (!WAPI_FAILED(r)) {
        if (WAPI_FAILED(wapi_audio_bind_stream(g_audio_dev, g_music_stream))) {
            wapi_audio_destroy_stream(g_music_stream);
            g_music_stream = 0;
        }
    } else {
        g_music_stream = 0;
    }

    wapi_audio_resume(g_audio_dev);
    return WAPI_OK;
}

/* ============================================================
 * Tracker child module — soundtrack generation
 * ============================================================ */

/* Build the soundtrack loop: stage data in shared memory, call the
 * tracker, pull the PCM back into private memory. Called after the join
 * and get_func succeed. */
static wapi_result_t render_soundtrack(void) {
    /* Instruments. */
    tracker_instrument_t instruments[3] = {
        /* 0: bass — saw, medium punch */
        { TRACKER_WAVE_SAW,      0.005f, 0.25f, 0.25f, 0.30f, 0.35f },
        /* 1: lead — square, bright */
        { TRACKER_WAVE_SQUARE,   0.010f, 0.15f, 0.55f, 0.18f, 0.22f },
        /* 2: pad  — sine, slow swell */
        { TRACKER_WAVE_SINE,     0.20f,  0.30f, 0.70f, 0.40f, 0.30f },
    };

    /* 120 BPM: 60/120 = 0.5s per beat = AUDIO_RATE/2 samples/beat. */
    const uint32_t beat_samples = AUDIO_RATE / 2;
    const uint32_t bar_samples  = beat_samples * 4;

    /* Chord roots (bass octave 2), held for one bar each. */
    static const uint16_t bass_notes[4]   = { 45, 41, 48, 43 }; /* A2 F2 C3 G2 */
    /* Arpeggio (octave 4/5), four notes per bar. */
    static const uint16_t melody_notes[4][4] = {
        { 69, 72, 76, 72 }, /* A minor:   A4 C5 E5 C5 */
        { 65, 69, 72, 69 }, /* F major:   F4 A4 C5 A4 */
        { 72, 76, 79, 76 }, /* C major:   C5 E5 G5 E5 */
        { 67, 71, 74, 71 }, /* G major:   G4 B4 D5 B4 */
    };
    /* Pad sustains the chord root one octave up, one note per bar. */
    static const uint16_t pad_notes[4]    = { 57, 53, 60, 55 };

    tracker_note_t notes[4 + 4*4 + 4];
    uint32_t nc = 0;
    for (uint32_t bar = 0; bar < 4; bar++) {
        uint32_t bar_start = bar * bar_samples;
        /* Bass: hold for ~3.8 beats of the bar. */
        notes[nc++] = (tracker_note_t){
            bar_start, beat_samples * 38 / 10, 0, bass_notes[bar], 100
        };
        /* Melody: 4 quarter notes. */
        for (uint32_t b = 0; b < 4; b++) {
            notes[nc++] = (tracker_note_t){
                bar_start + b * beat_samples,
                beat_samples * 9 / 10,
                1, melody_notes[bar][b], 90
            };
        }
        /* Pad: one long note per bar. */
        notes[nc++] = (tracker_note_t){
            bar_start, bar_samples, 2, pad_notes[bar], 70
        };
    }

    /* Stage everything into shared memory and invoke the tracker. */
    wapi_size_t inst_off  = wapi_module_shared_alloc(sizeof(instruments), 4);
    wapi_size_t notes_off = wapi_module_shared_alloc(sizeof(tracker_note_t) * nc, 4);
    wapi_size_t out_off   = wapi_module_shared_alloc(sizeof(int16_t) * MUSIC_LOOP_SAMPLES, 2);
    wapi_size_t req_off   = wapi_module_shared_alloc(sizeof(tracker_request_t), 8);
    if (!inst_off || !notes_off || !out_off || !req_off) return WAPI_OK;

    wapi_module_shared_write(inst_off, instruments, sizeof(instruments));
    wapi_module_shared_write(notes_off, notes, sizeof(tracker_note_t) * nc);

    tracker_request_t req = {0};
    req.sample_rate     = AUDIO_RATE;
    req.num_instruments = 3;
    req.num_notes       = nc;
    req.out_samples     = MUSIC_LOOP_SAMPLES;
    req.instruments_off = inst_off;
    req.notes_off       = notes_off;
    req.out_off         = out_off;
    wapi_module_shared_write(req_off, &req, sizeof(req));

    wapi_val_t args[1] = {
        { .kind = WAPI_VAL_I64, .of.i64 = (int64_t)req_off },
    };
    wapi_val_t result;
    wapi_result_t r = wapi_module_call(g_tracker_mod, g_tracker_render_fn,
                                       args, 1, &result, 1);
    wapi_module_shared_free(req_off);
    if (WAPI_FAILED(r)) {
        wapi_module_shared_free(out_off);
        wapi_module_shared_free(notes_off);
        wapi_module_shared_free(inst_off);
        return r;
    }

    /* Pull the rendered PCM back into private memory so we can feed it
     * to the audio stream without touching shared memory every frame. */
    wapi_module_shared_read(out_off, g_music_loop, sizeof(g_music_loop));

    wapi_module_shared_free(out_off);
    wapi_module_shared_free(notes_off);
    wapi_module_shared_free(inst_off);

    g_music_ready  = 1;
    g_music_cursor = 0;
    return WAPI_OK;
}

/* Drive the join → get_func → render pipeline. Browsers need async
 * fetching, so this is called each frame until the state leaves
 * TRK_IDLE/TRK_JOINING. */
static void tracker_tick(void) {
    if (g_tracker_state == TRK_READY || g_tracker_state == TRK_FAILED) return;
    if (!wapi_cap_supported(g_io, WAPI_STR(WAPI_CAP_MODULE))) {
        g_tracker_state = TRK_FAILED;
        return;
    }

    wapi_module_hash_t hash;
    memcpy(hash.bytes, g_tracker_hash, 32);

    wapi_result_t r = wapi_module_join(&hash,
                                       WAPI_STR("hello_game_tracker.wasm"),
                                       WAPI_STR("tracker"),
                                       &g_tracker_mod);
    if (r == WAPI_ERR_AGAIN) {
        g_tracker_state = TRK_JOINING;
        if (++g_tracker_tries > 600) {   /* ~10s at 60 Hz */
            g_tracker_state = TRK_FAILED;
        }
        return;
    }
    if (WAPI_FAILED(r)) { g_tracker_state = TRK_FAILED; return; }

    r = wapi_module_get_func(g_tracker_mod, WAPI_STR("tracker_render"),
                             &g_tracker_render_fn);
    if (WAPI_FAILED(r)) {
        wapi_module_release(g_tracker_mod);
        g_tracker_mod = 0;
        g_tracker_state = TRK_FAILED;
        return;
    }

    if (WAPI_FAILED(render_soundtrack())) {
        g_tracker_state = TRK_FAILED;
        return;
    }
    g_tracker_state = TRK_READY;
}

/* Called every frame. Keeps the music stream topped up so playback
 * loops seamlessly. Skipped if tracker setup failed. */
static void pump_music(void) {
    if (!g_music_ready || !g_music_stream) return;
    while (wapi_audio_stream_queued(g_music_stream) < MUSIC_QUEUE_TARGET) {
        uint32_t n = MUSIC_CHUNK_SAMPLES;
        if (g_music_cursor + n > MUSIC_LOOP_SAMPLES) {
            n = MUSIC_LOOP_SAMPLES - g_music_cursor;
        }
        wapi_audio_put_stream_data(g_music_stream,
                                   &g_music_loop[g_music_cursor],
                                   (wapi_size_t)n * sizeof(int16_t));
        g_music_cursor += n;
        if (g_music_cursor >= MUSIC_LOOP_SAMPLES) g_music_cursor = 0;
    }
}

/* ============================================================
 * Module loading (AI child module)
 * ============================================================ */

static wapi_result_t init_ai_module(void) {
    if (!wapi_cap_supported(g_io, WAPI_STR(WAPI_CAP_MODULE))) return WAPI_OK;

    wapi_module_hash_t hash;
    memcpy(hash.bytes, g_ai_hash, 32);
    wapi_result_t r = wapi_module_load(&hash, WAPI_STR(""), &g_ai_module);
    if (WAPI_FAILED(r)) {
        /* Not fatal — run without AI delegation. */
        g_ai_module = 0;
        return WAPI_OK;
    }
    r = wapi_module_get_func(g_ai_module, WAPI_STR("ai_tick"), &g_ai_tick_fn);
    if (WAPI_FAILED(r)) {
        wapi_module_release(g_ai_module);
        g_ai_module = 0;
        return WAPI_OK;
    }
    /* Reserve shared-memory space for MAX_ENEMIES * sizeof(enemy_t). */
    g_enemy_shared_off = wapi_module_shared_alloc(
        MAX_ENEMIES * sizeof(enemy_t), 4);
    if (g_enemy_shared_off == 0) {
        wapi_module_release(g_ai_module);
        g_ai_module = 0;
    }
    return WAPI_OK;
}

/* ============================================================
 * Game state
 * ============================================================ */

static enemy_t g_enemies[MAX_ENEMIES];

static wapi_result_t init_game(void) {
    /* Acquire keyboard + gamepad endpoints through the role system.
     * Keyboard is ambient-granted; gamepad is ambient under gaming
     * policy. Both resolve synchronously on desktop. */
    request_role_blocking(WAPI_ROLE_KEYBOARD, 0, NULL, 0,
                          &g_kb_handle,   0xA0D11ULL);
    request_role_blocking(WAPI_ROLE_GAMEPAD, WAPI_ROLE_OPTIONAL, NULL, 0,
                          &g_gpad_handle, 0xA0D12ULL);

    uint32_t seed = 0xDEADBEEFu;
    wapi_random_get(&seed, sizeof(seed));
    g_rng_state = seed ? seed : 1;

    reset_round();
    g_running = 1;
    return WAPI_OK;
}

/* Clear entity state and respawn the player at the start position.
 * Called from init_game and from the game-over restart path. */
static void reset_round(void) {
    g_player_x = PLAY_W * 0.5f - 4;
    g_player_y = PLAY_H - 40;
    g_lives = 3;
    g_score = 0;
    g_coin_active = 0;
    g_frame = 0;
    g_game_over = 0;
    g_sim_accum_ns = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) g_enemies[i].alive = 0;
}

static void spawn_enemy(void) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (g_enemies[i].alive) continue;
        uint32_t r = xorshift32();
        g_enemies[i].x     = (float)(r % (PLAY_W - 16));
        g_enemies[i].y     = -16.0f;
        g_enemies[i].vx    = ((float)((r >> 16) % 200) / 100.0f) - 1.0f;
        g_enemies[i].vy    = 2.0f + ((float)((r >> 8) % 200) / 100.0f);
        g_enemies[i].color = 0xFFFFFFFFu;
        g_enemies[i].alive = 1;
        return;
    }
}

static void spawn_coin(void) {
    uint32_t r = xorshift32();
    g_coin_x = (float)(r % (PLAY_W - 16));
    g_coin_y = 40.0f + (float)((r >> 16) % (PLAY_H - 120));
    g_coin_active = 1;
    g_coin_timer = COIN_LIFETIME_F;
}

/* ============================================================
 * Input
 * ============================================================ */

static void read_input(float* out_dx, float* out_dy) {
    float dx = 0, dy = 0;
    if (wapi_keyboard_key_pressed(g_kb_handle, WAPI_SCANCODE_A) ||
        wapi_keyboard_key_pressed(g_kb_handle, WAPI_SCANCODE_LEFT))  dx -= 1;
    if (wapi_keyboard_key_pressed(g_kb_handle, WAPI_SCANCODE_D) ||
        wapi_keyboard_key_pressed(g_kb_handle, WAPI_SCANCODE_RIGHT)) dx += 1;
    if (wapi_keyboard_key_pressed(g_kb_handle, WAPI_SCANCODE_W) ||
        wapi_keyboard_key_pressed(g_kb_handle, WAPI_SCANCODE_UP))    dy -= 1;
    if (wapi_keyboard_key_pressed(g_kb_handle, WAPI_SCANCODE_S) ||
        wapi_keyboard_key_pressed(g_kb_handle, WAPI_SCANCODE_DOWN))  dy += 1;

    /* Gamepad 0 stick — scale -32768..32767 → -1..1. */
    int16_t ax, ay;
    if (wapi_gamepad_get_axis(g_gpad_handle, WAPI_GAMEPAD_AXIS_STICK_LEFT_X, &ax) == WAPI_OK &&
        wapi_gamepad_get_axis(g_gpad_handle, WAPI_GAMEPAD_AXIS_STICK_LEFT_Y, &ay) == WAPI_OK) {
        float fx = ax / 32767.0f;
        float fy = ay / 32767.0f;
        if (fx*fx + fy*fy > 0.04f) { /* deadzone */
            dx += fx;
            dy += fy;
        }
    }

    /* Clamp diagonals to unit length. Use the wasm f32.sqrt intrinsic
     * so we don't pull in libm. */
    float len = dx*dx + dy*dy;
    if (len > 1.0f) {
        float s = 1.0f / __builtin_sqrtf(len);
        dx *= s; dy *= s;
    }
    *out_dx = dx; *out_dy = dy;
}

/* ============================================================
 * Simulation
 * ============================================================ */

static void sim_step(void) {
    if (g_game_over) return;
    g_frame++;

    /* Player. */
    float dx, dy;
    read_input(&dx, &dy);
    g_player_x += dx * PLAYER_SPEED;
    g_player_y += dy * PLAYER_SPEED;
    if (g_player_x < 0) g_player_x = 0;
    if (g_player_x > PLAY_W - 8) g_player_x = PLAY_W - 8;
    if (g_player_y < 0) g_player_y = 0;
    if (g_player_y > PLAY_H - 8) g_player_y = PLAY_H - 8;

    /* Spawn. */
    if ((g_frame % 40) == 0) spawn_enemy();
    if (!g_coin_active && (g_frame % 120) == 0) spawn_coin();
    if (g_coin_active && --g_coin_timer <= 0) g_coin_active = 0;

    /* AI tick: hand enemies to the child module if available. */
    if (g_ai_module && g_ai_tick_fn) {
        wapi_module_shared_write(g_enemy_shared_off, g_enemies, sizeof(g_enemies));
        wapi_val_t args[2] = {
            { .kind = WAPI_VAL_I64, .of.i64 = (int64_t)g_enemy_shared_off },
            { .kind = WAPI_VAL_I64, .of.i64 = MAX_ENEMIES },
        };
        wapi_val_t result;
        wapi_module_call(g_ai_module, g_ai_tick_fn, args, 2, &result, 1);
        wapi_module_shared_read(g_enemy_shared_off, g_enemies, sizeof(g_enemies));
    } else {
        /* Fallback: advance locally. */
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (!g_enemies[i].alive) continue;
            g_enemies[i].x += g_enemies[i].vx;
            g_enemies[i].y += g_enemies[i].vy;
            if (g_enemies[i].y > PLAY_H) g_enemies[i].alive = 0;
        }
    }

    /* Collisions. */
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!g_enemies[i].alive) continue;
        float dxp = g_enemies[i].x - g_player_x;
        float dyp = g_enemies[i].y - g_player_y;
        if (dxp*dxp + dyp*dyp < 8.0f * 8.0f) {
            g_enemies[i].alive = 0;
            g_lives--;
            g_hit_audio_until = g_frame + 6;
            play_beep(200, 0.4f);
            if (g_lives <= 0) g_game_over = 1;
        }
    }
    if (g_coin_active) {
        float dxp = g_coin_x - g_player_x;
        float dyp = g_coin_y - g_player_y;
        if (dxp*dxp + dyp*dyp < 10.0f * 10.0f) {
            g_coin_active = 0;
            g_score++;
            g_coin_audio_until = g_frame + 4;
            play_beep(880, 0.3f);
        }
    }
}

/* ============================================================
 * Event loop
 * ============================================================ */

static void process_events(void) {
    wapi_event_t ev;
    while (g_io->poll(g_io->impl, &ev)) {
        switch (ev.type) {
            case WAPI_EVENT_WINDOW_CLOSE:
                g_running = 0;
                break;
            case WAPI_EVENT_SURFACE_RESIZED:
                g_width  = ev.surface.data1;
                g_height = ev.surface.data2;
                reconfigure_surface();
                break;
            case WAPI_EVENT_KEY_DOWN:
                if (ev.key.scancode == WAPI_SCANCODE_ESCAPE) g_running = 0;
                else if (g_game_over) reset_round();
                break;
            case WAPI_EVENT_MOUSE_BUTTON_DOWN:
            case WAPI_EVENT_POINTER_DOWN:
            case WAPI_EVENT_TOUCH_DOWN:
                if (g_game_over) reset_round();
                break;
            case WAPI_EVENT_GAMEPAD_BUTTON_DOWN:
                if (ev.gbutton.button == WAPI_GAMEPAD_BUTTON_START) g_running = 0;
                else if (g_game_over) reset_round();
                break;
            default: break;
        }
    }
}

/* ============================================================
 * Rendering
 * ============================================================ */

static void build_sprites(void) {
    g_vtx_count = 0;
    g_idx_count = 0;

    /* Playfield border strip (top bar). */
    push_sprite(0, 0, 0, 0x101020FFu, 0);  /* ignored: size 0 → no geometry */
    g_vtx_count = 0; g_idx_count = 0;

    /* Enemies (tile 2, red tint modulation). */
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!g_enemies[i].alive) continue;
        push_sprite(g_enemies[i].x, g_enemies[i].y, 2, 0xFFFFFFFFu, 16);
    }

    /* Coin (tile 1). */
    if (g_coin_active) {
        uint32_t tint = (g_coin_timer < 30 && (g_coin_timer & 4)) ? 0x888888FFu : 0xFFFFFFFFu;
        push_sprite(g_coin_x, g_coin_y, 1, tint, 16);
    }

    /* Life pips (tile 3). */
    for (int i = 0; i < g_lives; i++) {
        push_sprite(8.0f + i * 20.0f, 8.0f, 3, 0xFFFFFFFFu, 16);
    }

    /* Player (tile 0) — last, so it draws on top. */
    uint32_t player_tint = (g_hit_audio_until > g_frame) ? 0xFF8080FFu : 0xFFFFFFFFu;
    push_sprite(g_player_x, g_player_y, 0, player_tint, 16);
}

static wapi_result_t render_frame(void) {
    build_sprites();
    if (g_vtx_count > 0) {
        wgpuQueueWriteBuffer(g_queue, g_vbuf, 0, g_verts,
                             sizeof(sprite_vtx_t) * g_vtx_count);
        wgpuQueueWriteBuffer(g_queue, g_ibuf, 0, g_indices,
                             sizeof(uint16_t) * g_idx_count);
    }

    WGPUSurfaceTexture st;
    wgpuSurfaceGetCurrentTexture(g_wgpu_surface, &st);
    if (st.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        st.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        return WAPI_ERR_UNKNOWN;
    }
    WGPUTextureView view = wgpuTextureCreateView(st.texture, 0);

    WGPUCommandEncoderDescriptor ced = {0};
    ced.label.data = "frame"; ced.label.length = 5;
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(g_device, &ced);

    WGPURenderPassColorAttachment ca = {0};
    ca.view = view;
    ca.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    ca.loadOp = WGPULoadOp_Clear;
    ca.storeOp = WGPUStoreOp_Store;
    ca.clearValue.r = 0.04; ca.clearValue.g = 0.05; ca.clearValue.b = 0.10; ca.clearValue.a = 1;
    WGPURenderPassDescriptor rp = {0};
    rp.label.data = "main"; rp.label.length = 4;
    rp.colorAttachmentCount = 1;
    rp.colorAttachments = &ca;
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(enc, &rp);

    if (g_idx_count > 0) {
        wgpuRenderPassEncoderSetPipeline(pass, g_pipeline);
        wgpuRenderPassEncoderSetBindGroup(pass, 0, g_bg, 0, 0);
        wgpuRenderPassEncoderSetVertexBuffer(pass, 0, g_vbuf, 0,
                                             sizeof(sprite_vtx_t) * g_vtx_count);
        wgpuRenderPassEncoderSetIndexBuffer(pass, g_ibuf, WGPUIndexFormat_Uint16, 0,
                                            sizeof(uint16_t) * g_idx_count);
        wgpuRenderPassEncoderDrawIndexed(pass, g_idx_count, 1, 0, 0, 0);
    }
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    WGPUCommandBufferDescriptor cbd = {0};
    WGPUCommandBuffer cb = wgpuCommandEncoderFinish(enc, &cbd);
    wgpuQueueSubmit(g_queue, 1, &cb);
    wgpuCommandBufferRelease(cb);
    wgpuCommandEncoderRelease(enc);

    wgpuSurfacePresent(g_wgpu_surface);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(st.texture);

    /* Drive wgpu-native's async callbacks (none expected during frame). */
    wgpuInstanceProcessEvents(g_instance);
    return WAPI_OK;
}

/* ============================================================
 * Entry points
 * ============================================================ */

/* Fixed-timestep accumulator: simulation advances at a constant 60 Hz
 * regardless of how fast the host calls wapi_frame. 60 Hz browser: one
 * sim step per frame. 144 Hz monitor: one sim step every ~2.4 frames
 * on average (the accumulator only fires when it crosses SIM_STEP_NS).
 * Bounds max catch-up at 4 steps / frame so a long pause (tab
 * backgrounded, breakpoint) doesn't spiral after resuming. */
#define SIM_HZ        60
#define SIM_STEP_NS   (1000000000ULL / SIM_HZ)
#define SIM_MAX_STEPS 4
static uint64_t g_sim_last_ts = 0;
static uint64_t g_sim_accum_ns = 0;

WAPI_EXPORT(wapi_frame)
wapi_result_t wapi_frame(wapi_timestamp_t ts) {
    process_events();
    if (!g_running) return WAPI_ERR_CANCELED;

    if (g_sim_last_ts == 0) {
        g_sim_last_ts = ts;
    } else if (ts > g_sim_last_ts) {
        g_sim_accum_ns += ts - g_sim_last_ts;
        g_sim_last_ts = ts;
    } else {
        g_sim_last_ts = ts;
    }
    int steps = 0;
    while (g_sim_accum_ns >= SIM_STEP_NS && steps < SIM_MAX_STEPS) {
        sim_step();
        g_sim_accum_ns -= SIM_STEP_NS;
        steps++;
    }
    if (g_sim_accum_ns > SIM_STEP_NS * SIM_MAX_STEPS) {
        /* Big pause — drop remaining debt rather than catching up forever. */
        g_sim_accum_ns = 0;
    }

    tracker_tick();
    pump_music();
    render_frame();
    return WAPI_OK;
}

WAPI_EXPORT(wapi_main)
wapi_result_t wapi_main(void) {
    g_io = wapi_io_get();
    if (!g_io) return WAPI_ERR_UNKNOWN;

    if (!wapi_cap_supported(g_io, WAPI_STR(WAPI_CAP_GPU))     ||
        !wapi_cap_supported(g_io, WAPI_STR(WAPI_CAP_SURFACE)) ||
        !wapi_cap_supported(g_io, WAPI_STR(WAPI_CAP_INPUT))   ||
        !wapi_cap_supported(g_io, WAPI_STR(WAPI_CAP_AUDIO))) {
        return WAPI_ERR_NOTCAPABLE;
    }

    wapi_result_t r;
    r = decompress_atlas();  if (WAPI_FAILED(r)) return r;
    r = init_gpu();          if (WAPI_FAILED(r)) return r;
    r = init_audio();        if (WAPI_FAILED(r)) { /* non-fatal — no sound */ }
    r = init_game();         if (WAPI_FAILED(r)) return r;
    r = init_ai_module();    if (WAPI_FAILED(r)) { /* non-fatal — local sim */ }
    /* tracker_tick is driven from wapi_frame so the browser's async fetch
     * has a chance to complete between calls. */
    return WAPI_OK;
}
