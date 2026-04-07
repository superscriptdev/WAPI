# WAPI

**One binary. One ABI. Every platform.**

A capability-based ABI that turns WebAssembly into a universal application platform. One compilation target, one binary, runs everywhere. The host provides capabilities (graphics, audio, input, networking, filesystem) and the module uses them.

## What This Is

A concrete proposal for the platform WebAssembly was supposed to enable: a thin, capability-based ABI where a single `.wasm` binary runs identically in a browser tab, on a desktop, on a phone, or on a server.

- **C-style calling convention.** Functions accept integers, floats, and pointers to caller-owned memory. No WIT. No IDL. No Canonical ABI with lifting and lowering. Every language on earth can call these functions.
- **Capabilities all the way down.** A module starts with zero access. Every resource is a handle the host explicitly granted. No ambient authority.
- **webgpu.h directly.** GPU access uses the existing `webgpu.h` header from the webgpu-native project, implemented by both Dawn (Google) and wgpu-native (Rust).
- **Submit + unified events.** Async I/O follows the io_uring pattern: submit operations via a vtable, completions arrive as events in the same queue as input and window events. One wait point for everything.

## Design Lineage

This ABI doesn't start from scratch. It unifies proven, battle-tested APIs:

| Capability | Based On | Proven By |
|---|---|---|
| Filesystem, clocks, random | WASI Preview 1 | Wasmtime, Wasmer, WasmEdge |
| Windowing, input, audio | SDL3 | Billions of devices across all platforms |
| GPU compute & rendering | webgpu.h | Dawn (Chrome), wgpu-native (Firefox) |
| Async I/O | io_uring | Linux kernel, WebGPU command submission |

## Capabilities

All capabilities use the same query mechanism -- string-based names, Vulkan-style enumeration. No "core" vs "extension" distinction.

```
wapi               Capability queries (string-based feature detection)
wapi_env           Args, environment variables, random, exit
wapi_memory        Host-provided memory allocation
wapi_io            Async I/O (submit via vtable, completions as events)
wapi_clock         Monotonic and wall clocks, performance counters
wapi_fs            Capability-based filesystem (pre-opened directories)
wapi_net           QUIC/WebTransport networking
wapi_gpu           WebGPU bridge (surface setup; uses webgpu.h directly)
wapi_surface       Render targets (on-screen and offscreen)
wapi_window        OS window management (title, fullscreen, minimize)
wapi_input         Input devices: mouse, keyboard, touch, pen, gamepad, HID
wapi_audio         Audio playback and recording streams
wapi_content       Host-rendered text, images, media
wapi_clipboard     System clipboard
wapi_font          Font system queries and enumeration
wapi_video         Video/media playback
wapi_geolocation   GPS / location services
wapi_notifications System notifications
wapi_sensors       Accelerometer, gyroscope, compass
wapi_speech        Text-to-speech, speech recognition
wapi_crypto        Hardware-accelerated cryptography
wapi_biometric     Fingerprint, face recognition, WebAuthn
wapi_share         System share sheet
wapi_kv_storage    Persistent key-value storage
wapi_payments      In-app purchases / payment processing
wapi_usb           USB device access
wapi_midi          MIDI input/output
wapi_bluetooth     Bluetooth LE / GATT
wapi_camera        Camera capture
wapi_xr            VR/AR (WebXR, OpenXR)
```

Vendor capabilities use the `vendor.<name>.*` namespace for platform-specific features.

## Presets

Presets are convenience arrays of capability name strings that give developers a stable target:

| Preset | Capabilities | Use Cases |
|---|---|---|
| **Headless** | env, memory, io, clock, fs, net | Servers, CLI tools, edge functions, IoT |
| **Compute** | Headless + gpu | ML inference, video transcoding, simulations |
| **Audio** | Headless + audio | VST plugins, audio processing, voice |
| **Graphical** | Headless + gpu, surface, input, audio, content, clipboard, font | Apps, games, creative tools |
| **Mobile** | Graphical + geolocation, sensors, notifications, biometric, camera, share | Mobile apps |

Check preset support with `wapi_preset_supported(WAPI_PRESET_GRAPHICAL)` -- iterates the string array, returns true if every capability is present.

## Project Structure

```
wapi/
├── spec/
│   └── wapi-spec.md                    # Formal ABI specification
├── include/
│   └── wapi/
│       ├── wapi.h            # Master header (includes all)
│       ├── wapi_types.h                 # Core types, handles, errors
│       ├── wapi_context.h               # Context, allocator, I/O vtable, panic handler
│       ├── wapi_capability.h            # Capability enumeration (string-based)
│       ├── wapi_env.h                   # Environment and process
│       ├── wapi_memory.h                # Memory allocation
│       ├── wapi_io.h                    # Async I/O (submit/poll/wait)
│       ├── wapi_clock.h                 # Clocks and timers
│       ├── wapi_fs.h                    # Filesystem
│       ├── wapi_net.h                   # Networking
│       ├── wapi_gpu.h                   # GPU (webgpu.h bridge)
│       ├── wapi_surface.h               # Render targets
│       ├── wapi_window.h               # OS window management
│       ├── wapi_input.h                 # Input devices (unified)
│       ├── wapi_audio.h                 # Audio
│       ├── wapi_content.h               # Host-rendered content
│       ├── wapi_clipboard.h             # Clipboard
│       ├── wapi_font.h                  # Font system queries
│       ├── wapi_video.h                 # Video/media playback
│       ├── wapi_geolocation.h           # GPS / location
│       ├── wapi_notifications.h         # System notifications
│       ├── wapi_sensors.h               # Accelerometer, gyroscope, etc.
│       ├── wapi_speech.h                # Text-to-speech, speech recognition
│       ├── wapi_crypto.h                # Hashing, encryption, signing
│       ├── wapi_biometric.h             # Face ID, fingerprint, WebAuthn
│       ├── wapi_share.h                 # System share sheet
│       ├── wapi_kv_storage.h            # Persistent key-value store
│       ├── wapi_payments.h              # Apple Pay, Google Pay
│       ├── wapi_usb.h                   # USB device access
│       ├── wapi_midi.h                  # MIDI input/output
│       ├── wapi_bluetooth.h             # Bluetooth LE / GATT
│       ├── wapi_camera.h                # Camera capture
│       └── wapi_xr.h                    # VR/AR (WebXR, OpenXR)
├── runtime/
│   ├── browser/
│   │   ├── wapi_shim.js                # JS shim (Web APIs)
│   │   └── index.html                # HTML loader
│   └── desktop/
│       ├── CMakeLists.txt             # Build system
│       └── src/                       # SDL3 + wgpu-native + Wasmtime
└── examples/
    ├── hello_headless.c               # Headless preset demo
    ├── hello_triangle.c               # Graphical preset demo
    └── hello_audio.c                  # Audio preset demo
```

## Quick Start

```c
#include <wapi/wapi.h>

WAPI_EXPORT(wapi_main)
wapi_result_t wapi_main(const wapi_context_t* ctx) {
    // Host provides: ctx->allocator, ctx->io, ctx->panic, ctx->gpu_device

    // Check what the host provides (string-based capability query)
    if (!wapi_preset_supported(WAPI_PRESET_GRAPHICAL)) {
        return WAPI_ERR_NOTCAPABLE;
    }

    // Create a windowed surface
    wapi_window_config_t win = {
        .chain = { .next = NULL, .sType = WAPI_STYPE_WINDOW_CONFIG },
        .title = "My App", .title_len = 6,
        .window_flags = WAPI_WINDOW_FLAG_RESIZABLE,
    };
    wapi_surface_desc_t desc = {
        .nextInChain = (wapi_chained_struct_t*)&win,
        .width = 800, .height = 600,
        .flags = WAPI_SURFACE_FLAG_HIGH_DPI,
    };
    wapi_handle_t surface;
    wapi_surface_create(&desc, &surface);

    // Get a GPU device
    wapi_handle_t gpu;
    wapi_gpu_request_device(NULL, &gpu);

    // Configure surface for rendering
    wapi_gpu_surface_config_t config = {
        .surface = surface, .device = gpu,
        .format = WAPI_GPU_FORMAT_BGRA8_UNORM,
        .present_mode = WAPI_GPU_PRESENT_FIFO,
    };
    wapi_gpu_configure_surface(&config);

    // Event loop: input, I/O completions, and timers all come through one vtable
    wapi_event_t ev;
    while (ctx->io->wait(ctx->io->impl, &ev, -1)) {
        switch (ev.type) {
        case WAPI_EVENT_IO_COMPLETION:   /* async I/O finished */ break;
        case WAPI_EVENT_KEY_DOWN:        /* keyboard input */     break;
        case WAPI_EVENT_WINDOW_CLOSE:    return WAPI_OK;
        }
    }

    return WAPI_OK;
}
```

## ABI Design Principles

These principles emerged from iterating on the design and apply across all WAPI modules:

**Separate concerns by role, not proximity.** Surface (render target) and window (OS chrome) are related but different APIs. A surface can exist without a window (offscreen rendering). Splitting them makes both simpler and enables combinations the monolith couldn't express.

**Ownership follows the physical model.** The cursor belongs to the mouse device, not the window — because with multi-mouse, each device has its own cursor. Let the physical reality dictate which API owns which state.

**Stable identity via UID.** Physical devices get an opaque 16-byte UID that persists across disconnect/reconnect and across sessions. This lets an app restore user-to-device mappings (player 1's controller) without requiring the user to re-bind every time.

**No embedded pointers in structs.** Info structs contain only value-type fields — never raw pointers like `name_ptr`/`name_len`. Names and variable-length data are queried through buffer-filling functions (`get_name(handle, buf, buf_len, &name_len)`). This keeps structs fixed-size, portable, and memcpy-safe.

**Chained structs for extension, not flags.** Optional configuration uses the `nextInChain` pattern (from webgpu.h), not ever-growing bitmask flags. Chained structs are additive, self-describing, and don't require reserving bits in advance.

## Key Design Decisions

### Why string-based capabilities?

Bitmasks are fast but limited to 32 or 64 capabilities before requiring a breaking change. String-based capability names (like Vulkan's extension model) scale indefinitely -- any vendor can add `vendor.mycompany.myfeature` without coordination. Every capability uses the same query path: `wapi_capability_supported("wapi.gpu", 6)`. There is no "core" vs "extension" distinction. Presets are just convenience arrays of these strings. The cost of a few string comparisons at startup is negligible compared to the architectural flexibility this provides.

### Why not the Component Model?

The Component Model solves plugin composition. This ABI solves application portability. Different goals, different architectures. The Component Model adds per-call overhead (lifting, lowering, string transcoding, mandatory memory isolation) that is unnecessary when the module and host agree on struct layouts. It has no path to browser implementation. A C-style ABI can be shimmed in a browser today with ~5,000-10,000 lines of JS, the way Emscripten already does.

### Why handles, not opaque pointers?

Opaque pointers (`WGPUDevice*`) work great in native code where the caller and callee share an address space. In Wasm, the module has its own linear memory. An opaque pointer from the host would point into host memory that the module can't access. Handles (i32 indices into a host-side table) are the natural fit for the Wasm sandbox model. The host validates every handle on every call.

### Why submit + unified event queue, not async/await?

async/await is a language-level concern. The ABI is a calling convention. I/O operations are submitted through a vtable (`ctx->io->submit`) and completions arrive as events in the same queue as input, window, and lifecycle events. One wait point for everything -- a game loop processes key presses and file load completions in the same `switch`. This is how io_uring works (kernel I/O), how webgpu.h works (GPU commands), and how every game loop works (process events, update, render). The module can wrap this in async/await, green threads, or whatever its language prefers.

### Why explicit context (allocator + I/O + panic as vtables)?

Modules receive their allocator, I/O interface, and panic handler from the host via `wapi_context_t`, not from global imports. This means a parent module controls how children allocate memory, perform I/O, and report fatal errors. You can pass a throttled I/O, an arena allocator, a mock, or a panic handler that routes to your logging system -- the child doesn't know. The same context struct flows from host to app to shared module, making the dependency graph explicit and testable.

The context contains only execution substrates -- things that differ per-caller when a module runtime is shared between applications. Memory (allocator), world interaction (I/O vtable), panic reporting, compute (GPU device handle), and flags. Application-level concerns like surfaces, locale, and preferences are function parameters, not context fields.

### Why errors as values and panics on the context?

Errors are return values (`wapi_result_t`). Every function that can fail returns a negative error code. No exceptions, no traps for recoverable conditions. This is the only error model that works across every language targeting Wasm.

Panics are different -- they represent unrecoverable bugs (assertion failures, unreachable code). When a module panics, it calls the panic handler on its context to record a message, then traps. The handler is provided by whoever created the context: the host for `wapi_main`, the parent for `wapi_module_init`. This means when module M panics while app A called it, the panic message routes to A's handler -- not to some other app sharing the same module runtime. The runtime catches the Wasm trap and converts it to an error return for the parent. Panics never cross module boundaries as traps.

### Why is logging an I/O opcode?

Logging is I/O -- a message going out to the world. Rather than adding a dedicated logging API, log messages are submitted through the same I/O vtable as everything else (`WAPI_IO_OP_LOG`). This means a parent module that wraps a child's I/O vtable automatically controls the child's logging too -- filtering, routing, prefixing, or suppressing it. No new context fields, no separate logging interface. The host maps it to stderr, `console.log`, or whatever is appropriate for the platform. Fire-and-forget: no completion event, no blocking.

### Why include content rendering?

Every OS ships mature, hardware-accelerated text shapers, image decoders, and video pipelines. Bundling your own into every Wasm module wastes binary size and misses platform capabilities. More importantly, host-rendered content enables accessibility (screen readers), indexability (search engines), and correct complex script rendering (Arabic, CJK, bidi) without the module doing any of that work.

## Status

This is a specification proposal. No runtime implementation exists yet. The headers define the complete ABI surface; a conforming host implements these imported functions.

A minimal graphical host could be built in a weekend using Wasmtime + SDL3 + Dawn. A browser shim would be ~5,000-10,000 lines of JS mapping these imports to Web APIs.

## License

Public domain / CC0. This is infrastructure intended for universal adoption.
