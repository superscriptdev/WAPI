# WAPI

**One binary. One ABI. Every platform.**

A capability-based ABI that turns WebAssembly into a universal application platform. One compilation target, one binary, runs everywhere. The host provides capabilities (graphics, audio, input, networking, filesystem) and the module uses them.

## What This Is

A concrete proposal for the platform WebAssembly was supposed to enable: a thin, capability-based ABI where a single `.wasm` binary runs identically in a browser tab, on a desktop, on a phone, or on a server.

- **C-style calling convention.** Functions accept integers, floats, and pointers to caller-owned memory. No WIT. No IDL. No Canonical ABI with lifting and lowering. Every language on earth can call these functions.
- **Capabilities all the way down.** A module starts with zero access. Every resource is a handle the host explicitly granted. No ambient authority.
- **webgpu.h directly.** GPU access uses the existing `webgpu.h` header from the webgpu-native project, implemented by both Dawn (Google) and wgpu-native (Rust).
- **Submit + unified events.** Async I/O follows the io_uring pattern: submit operations via the `wapi_io_t` vtable, completions arrive as events in the same queue as input and window events. One wait point for everything.

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
wapi                Capability queries (string-based feature detection)
wapi_env            Args, environment variables, random, exit
wapi_memory         Host-provided memory allocation
wapi_io             Async I/O (submit/poll/wait, completions as events)
wapi_clock          Monotonic and wall clocks, performance counters
wapi_filesystem     Capability-based filesystem (pre-opened directories)
wapi_network        QUIC / WebTransport / sockets
wapi_http           One-shot HTTP requests (GET/POST/PUT/DELETE/HEAD)
wapi_compression    gzip / deflate / zstd / lz4 (compress and decompress)
wapi_gpu            WebGPU bridge (surface setup; uses webgpu.h directly)
wapi_surface        Render targets (on-screen and offscreen)
wapi_window         OS window management (title, fullscreen, minimize)
wapi_display        Display enumeration, geometry, sub-pixels
wapi_input          Input devices: mouse, keyboard, touch, pen, gamepad, HID
wapi_audio          Audio playback and recording streams
wapi_audioplugin    Audio plugin hosting (VST-style)
wapi_content        Declare a semantic content tree (a11y, indexing, screen readers)
wapi_text           Text shaping and layout (HarfBuzz / DirectWrite / CoreText)
wapi_font           Font system queries and enumeration
wapi_clipboard      System clipboard
wapi_video          Video / media playback
wapi_geolocation    GPS / location services
wapi_notifications  System notifications
wapi_sensors        Accelerometer, gyroscope, compass
wapi_speech         Text-to-speech, speech recognition
wapi_crypto         Hardware-accelerated cryptography
wapi_biometric      Fingerprint, face recognition
wapi_authn          WebAuthn / passkeys
wapi_share          System share sheet
wapi_kvstorage      Persistent key-value storage
wapi_payments       In-app purchases / payment processing
wapi_usb            USB device access
wapi_midi           MIDI input/output
wapi_bluetooth      Bluetooth LE / GATT
wapi_camera         Camera capture
wapi_xr             VR/AR (WebXR, OpenXR)
wapi_serial         Serial port access
wapi_nfc            Near-field communication
wapi_codec          Hardware-backed audio/video codecs
wapi_screencapture  Screen / window capture
wapi_module         Runtime module linking, cross-module calls
```

Vendor capabilities use the `vendor.<name>.*` namespace for platform-specific features.

## Presets

Presets are convenience arrays of capability name strings that give developers a stable target:

| Preset | Capabilities | Use Cases |
|---|---|---|
| **Headless** | env, memory, io, clock, fs, net | Servers, CLI tools, edge functions, IoT |
| **Compute** | Headless + gpu | ML inference, video transcoding, simulations |
| **Audio** | Headless + audio | VST plugins, audio processing, voice |
| **Graphical** | Headless + gpu, surface, window, display, input, audio, content, clipboard, font | Apps, games, creative tools |
| **Mobile** | Graphical + geolocation, sensors, notifications, biometric, camera, share | Mobile apps |

Check preset support with `wapi_preset_supported(WAPI_PRESET_GRAPHICAL)` -- iterates the string array, returns true if every capability is present.

## Project Structure

```
wapi/
├── spec/
│   └── wapi-spec.md            # Formal ABI specification
├── include/
│   └── wapi/
│       ├── wapi.h              # Master header (types, errors, io, allocator,
│       │                       #   capability constants, all opcodes)
│       ├── webgpu.h            # Vendored from webgpu-native, used as-is
│       │
│       │  # Foundation
│       ├── wapi_env.h          # Args, environment, exit
│       ├── wapi_clock.h        # Clocks, timers, performance counters
│       ├── wapi_filesystem.h   # Capability-based filesystem
│       ├── wapi_network.h      # QUIC / WebTransport / sockets
│       ├── wapi_http.h         # One-shot HTTP requests
│       ├── wapi_compression.h  # gzip / deflate / zstd / lz4
│       ├── wapi_crypto.h       # Hashing, encryption, signing
│       ├── wapi_module.h       # Runtime module linking
│       │
│       │  # Graphics & windowing
│       ├── wapi_gpu.h          # GPU bridge (uses webgpu.h directly)
│       ├── wapi_surface.h      # Render targets (on-screen / offscreen)
│       ├── wapi_window.h       # OS window management
│       ├── wapi_display.h      # Display enumeration
│       ├── wapi_input.h        # Mouse, keyboard, touch, pen, gamepad
│       │
│       │  # Audio / media
│       ├── wapi_audio.h        # Playback and recording
│       ├── wapi_audioplugin.h  # VST-style plugin hosting
│       ├── wapi_video.h        # Video playback
│       ├── wapi_codec.h        # Hardware audio/video codecs
│       ├── wapi_camera.h       # Camera capture
│       ├── wapi_screencapture.h
│       ├── wapi_mediasession.h
│       │
│       │  # Content (declare vs render — see "Why a content tree?" below)
│       ├── wapi_content.h      # Semantic content tree (a11y, indexing)
│       ├── wapi_text.h         # Shaping + layout (HarfBuzz/DirectWrite)
│       ├── wapi_font.h         # Font system queries
│       ├── wapi_clipboard.h    # System clipboard
│       │
│       │  # Devices
│       ├── wapi_usb.h          # USB
│       ├── wapi_serial.h       # Serial ports
│       ├── wapi_midi.h         # MIDI
│       ├── wapi_bluetooth.h    # Bluetooth LE / GATT
│       ├── wapi_nfc.h          # NFC
│       │
│       │  # Sensors / hardware
│       ├── wapi_sensors.h      # Accelerometer, gyroscope, compass
│       ├── wapi_geolocation.h  # GPS
│       ├── wapi_orientation.h  # Screen orientation
│       ├── wapi_haptics.h      # Vibration / haptics
│       ├── wapi_power.h        # Battery, wake lock, idle, saver, thermal
│       │
│       │  # Identity & integrations
│       ├── wapi_authn.h        # WebAuthn / passkeys
│       ├── wapi_biometric.h    # Fingerprint, face
│       ├── wapi_payments.h     # In-app purchases
│       ├── wapi_share.h        # System share sheet
│       ├── wapi_notifications.h
│       ├── wapi_speech.h       # Text-to-speech, recognition
│       ├── wapi_kvstorage.h    # Persistent key-value
│       ├── wapi_contacts.h
│       ├── wapi_dialog.h       # File pickers, message dialogs
│       ├── wapi_menu.h
│       ├── wapi_tray.h
│       ├── wapi_taskbar.h
│       ├── wapi_register.h     # Default app / file association
│       ├── wapi_theme.h
│       ├── wapi_dnd.h          # Drag and drop
│       ├── wapi_xr.h           # VR/AR (WebXR / OpenXR)
│       ├── wapi_networkinfo.h
│       ├── wapi_eyedropper.h
│       ├── wapi_barcode.h
│       ├── wapi_sysinfo.h
│       ├── wapi_thread.h
│       └── wapi_process.h
├── runtime/
│   └── browser/
│       ├── wapi_shim.js        # JS host shim (~6k lines, all Web APIs)
│       ├── bridge.js           # Module loader / linker
│       ├── ready.js            # Boot sequencing
│       ├── serve.js            # Local dev server
│       ├── index.html          # HTML loader
│       └── sw.js               # Service worker
└── examples/
    ├── hello_headless.c        # Headless preset demo
    ├── hello_triangle.c        # Graphical preset demo
    └── hello_audio.c           # Audio preset demo
```

## Quick Start

```c
#include <wapi/wapi.h>

WAPI_EXPORT(wapi_main)
wapi_result_t wapi_main(void) {
    // Obtain module-owned vtables from the host
    const wapi_io_t* io = wapi_io_get();
    const wapi_allocator_t* alloc = wapi_allocator_get();

    // Check what the host provides (string-based capability query)
    if (!wapi_preset_supported(io, WAPI_PRESET_GRAPHICAL)) {
        return WAPI_ERR_NOTCAPABLE;
    }

    // Create a windowed surface
    wapi_window_config_t win = {
        .chain = { .next = NULL, .sType = WAPI_STYPE_WINDOW_CONFIG },
        .title = "My App", .title_len = 6,
        .window_flags = WAPI_WINDOW_FLAG_RESIZABLE,
    };
    wapi_surface_desc_t desc = {
        .nextInChain = (uintptr_t)&win,
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

    // Event loop: all events come through the I/O vtable
    wapi_event_t ev;
    while (io->wait(io->impl, &ev, -1)) {
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

**Names read general to specific, category before instance.** Every identifier at the ABI surface walks a hierarchy from left to right: module → subsystem → kind → instance. Underscores separate levels of that hierarchy, not English words — `WAPI_CURSOR_NOTALLOWED` is one cursor kind, not two levels. When several instances share a category, the category comes first — `WAPI_GAMEPAD_AXIS_TRIGGER_LEFT`, not `..._LEFT_TRIGGER` — so sorted output and `grep` both group related names together. See [spec §2.8](spec/wapi-spec.md#28-naming-conventions).

**Capabilities are self-contained. No cross-capability headers.** Each capability header stands on its own. When two capabilities need the same vocabulary (font weight, font style, pixel format, …), each redeclares its own namespaced enum with identical wire values. Headers never `#include` each other, only the universal `wapi.h`. The duplication is deliberate: it eliminates the cross-capability dependency graph entirely. See [spec §6.1](spec/wapi-spec.md#61-namespace-isolation).

## Key Design Decisions

### Why duplicate enums across capability headers?

The obvious first instinct for shared vocabulary (font weight, font style, pixel format) is to put the enum in one canonical header and `#include` it from every consumer. We tried that. The problem is it creates a cross-capability compile-time dependency graph: `wapi_text.h` now depends on `wapi_font.h`, `wapi_dialog.h` on both, and so on. Once that graph exists, you need a way to describe it (so hosts and presets stay coherent), a way to validate it (so `wapi.dialog` without `wapi.font` gets caught), and a way to version it (so a v2 of `wapi_font_style_t` doesn't silently break downstream capabilities). That's infrastructure nobody wants for five-line enums.

Instead, each capability owns its own copy. `wapi_font_style_t`, `wapi_text_font_style_t`, and `wapi_dialog_font_style_t` are three different C types with the same wire values (`NORMAL=0`, `ITALIC=1`, `OBLIQUE=2`). A module reading a style from the font picker and passing it into text shaping does so through a plain `uint32_t` round-trip. No shared header, no transitive dependency, no validator. Consistency is maintained by convention and review, the same way every other wire-level invariant in the ABI is.

The cost is a handful of duplicated enums. The benefit is that every capability header compiles in isolation and there is no cross-capability build graph.

### Why string-based capabilities?

Bitmasks are fast but limited to 32 or 64 capabilities before requiring a breaking change. String-based capability names (like Vulkan's extension model) scale indefinitely -- any vendor can add `vendor.mycompany.myfeature` without coordination. Every capability uses the same query path: `wapi_cap_supported(io, WAPI_STR("wapi.gpu"))`. There is no "core" vs "extension" distinction. Presets are just convenience arrays of these strings. The cost of a few string comparisons at startup is negligible compared to the architectural flexibility this provides.

### Why not the Component Model?

The Component Model solves plugin composition. This ABI solves application portability. Different goals, different architectures. The Component Model adds per-call overhead (lifting, lowering, string transcoding, mandatory memory isolation) that is unnecessary when the module and host agree on struct layouts. It has no path to browser implementation. A C-style ABI can be shimmed in a browser today with ~5,000-10,000 lines of JS, the way Emscripten already does.

### Why handles, not opaque pointers?

Opaque pointers (`WGPUDevice*`) work great in native code where the caller and callee share an address space. In Wasm, the module has its own linear memory. An opaque pointer from the host would point into host memory that the module can't access. Handles (i32 indices into a host-side table) are the natural fit for the Wasm sandbox model. The host validates every handle on every call.

### Why submit + unified event queue, not async/await?

async/await is a language-level concern. The ABI is a calling convention. I/O operations are submitted through the `wapi_io_t` vtable and completions arrive as events in the same queue as input, window, and lifecycle events. One wait point for everything -- a game loop processes key presses and file load completions in the same `switch`. This is how io_uring works (kernel I/O), how webgpu.h works (GPU commands), and how every game loop works (process events, update, render). The module can wrap this in async/await, green threads, or whatever its language prefers.

### How does module composition work?

Each module has private memory (memory 0) for isolation and access to shared memory (memory 1) for zero-copy cross-module data exchange.

**Build-time** (one binary): Libraries share memory 0 naturally. Pass pointers, call functions -- just C linking. Allocators and I/O vtables can be passed as explicit function parameters (Zig-style).

**Runtime** (separate modules): Each module has its own memory 0. Data exchange uses shared memory (memory 1) with a borrow system (`wapi_module_lend` / `wapi_module_reclaim`) for zero-copy access, or explicit copies (`wapi_module_copy_in`) for simple cases. Every module gets host-controlled, sandbox-only vtables from `wapi_io_get()` / `wapi_allocator_get()`. For capabilities beyond the sandbox, callers pass vtables explicitly.

A library can work in both modes by accepting a `wapi_allocator_t` and `wapi_io_t` vtable as explicit parameters.

There is no context struct. Every module obtains its I/O and allocator vtables from the host via `wapi_io_get()` / `wapi_allocator_get()`. GPU devices come from `wapi_gpu_request_device()`. A module starts with zero state and queries capabilities through its `wapi_io_t` vtable. This keeps the ABI clean and enables safe shared module instances.

### Why errors as values and panics as a host import?

Errors are return values (`wapi_result_t`). Every function that can fail returns a negative error code. No exceptions, no traps for recoverable conditions. This is the only error model that works across every language targeting Wasm.

Panics are different -- they represent unrecoverable bugs (assertion failures, unreachable code). When a module panics, it calls `wapi_panic_report` (a host import) to record a message, then traps. The host knows which module is calling and routes the message to the appropriate handler (stderr, console, parent module). The runtime catches the Wasm trap and converts it to an error return for the parent. Panics never cross module boundaries as traps.

### Why is logging an I/O opcode?

Logging is I/O -- a message going out to the world. Rather than adding a dedicated logging API, log messages are submitted through the same `wapi_io_t` vtable as everything else (`WAPI_IO_OP_LOG`). A child module's sandbox-only vtable from `wapi_io_get()` supports logging without any permissions. For build-time libraries, wrapping the I/O vtable controls logging too. The host maps it to stderr, `console.log`, or whatever is appropriate for the platform. Fire-and-forget: no completion event, no blocking.

### Why a content tree separate from rendering?

A typical Wasm app draws everything itself: it owns the GPU and pushes pixels. That's fast and portable, but it loses everything the platform normally gives you for free: accessibility trees, screen reader navigation, search-engine indexing, find-in-page, OS-level translation, password manager autofill, and correct shaping of complex scripts (Arabic, CJK, bidi). Once the UI is just pixels, the operating system has nothing left to introspect.

WAPI splits the problem in two so the app keeps full rendering control while the host gets back everything it needs:

**`wapi_content` — declare *what* the UI is.**
The app builds a semantic tree (buttons, text, headings, lists, landmarks, form fields, …) in its own linear memory and bumps a version counter when it changes. The host reads the tree for accessibility APIs, screen readers, indexing, and keyboard navigation. This is a pure *declaration* — the host never writes back, and the tree imposes nothing on how the app draws.

**`wapi_text` — render *how* text should look.**
Text shaping and line layout are a 50,000-line problem that no app should re-solve. `wapi_text` exposes the host's text engine (HarfBuzz, DirectWrite, CoreText) so the app gets correct shaping, kerning, bidi, line breaking, and hit testing for any script — and gets back glyph runs with positions it can render through its own GPU pipeline.

**The app still draws.**
Both modules are *advisory*. The app remains in charge of pixels: it asks `wapi_text` for glyph positions and rasterizes them through `wapi_gpu` like any other geometry. `wapi_content` runs in parallel as metadata for the host. This is the opposite of the browser model (where the host owns layout and the app gives up control); WAPI keeps the GPU-driven model and adds the missing introspection layer on the side.

## Status

This is an in-progress specification with one working host. The headers define the ABI surface; a conforming host implements these imported functions.

- **Browser shim** — `runtime/browser/wapi_shim.js` (~6k lines) maps the imports to Web APIs (WebGPU, Web Audio, `fetch`, `DecompressionStream`, Pointer Events, etc.). It is the host used by the PanGui WAPI port and is the reference implementation that exercises the spec end-to-end.
- **Native host** — not yet written. A minimal graphical host should be a weekend's work on top of Wasmtime + SDL3 + Dawn; the ABI is deliberately small enough that the missing pieces are integration glue, not design work.

The spec, headers, and shim move together: when something is unclear in the header, the shim is the tiebreaker until the spec catches up.

## License

Public domain / CC0. This is infrastructure intended for universal adoption.
