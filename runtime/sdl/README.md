# WAPI SDL Runtime

A WAPI host built on **SDL3 + Dawn + WAMR**. Windows, Linux, and macOS. Only capabilities that SDL3 or Dawn can back are registered; everything else is left unresolved so apps must gate usage behind `wapi.cap_supported()`.

## Stack

| Piece       | Library | Role                                                       |
|-------------|---------|------------------------------------------------------------|
| WASM engine | [WAMR](https://github.com/bytecodealliance/wasm-micro-runtime) | Loads and runs the `.wasm` module. Interpreter + fast-interp + optional LLVM AOT. |
| Windowing   | [SDL3](https://libsdl.org/) (no SDL_GPU) | Windows, input, audio, sensors, haptics, clipboard, DnD. |
| GPU         | [Dawn](https://dawn.googlesource.com/dawn) | WebGPU via `webgpu.h`, surface from SDL3 native window properties. |

## Supported capabilities

Registered (app can call these imports, and `cap_supported` returns `1`):

```
wapi.env      wapi.memory    wapi.io       wapi.clock     wapi.random
wapi.filesystem
wapi.gpu      wapi.surface   wapi.window   wapi.display   wapi.input
wapi.audio    wapi.haptics   wapi.sensors  wapi.power
wapi.clipboard                wapi.dnd
```

## Unsupported capabilities

Not registered. `cap_supported` returns `0`; if an app imports them anyway, instantiation fails:

```
compression, crypto, encoding, module,
net, http, content, text, font, video, kv_storage, notifications,
geolocation, speech, biometric, share, payments,
usb, midi, bluetooth, camera, xr,
register, taskbar, permissions, orientation,
codec, media_session, authn, network_info,
hid, serial, screen_capture, contacts, barcode, nfc
```

A module that wants to be portable should call `wapi_cap_supported(io, WAPI_STR(WAPI_CAP_GPU))` (etc.) on the io vtable before touching optional capabilities.

## Prerequisites

| Dep    | Version   | Notes                                                                             |
|--------|-----------|-----------------------------------------------------------------------------------|
| SDL3   | 3.2+      | Built with `-DSDL_SHARED=ON`/`OFF`, installed so `find_package(SDL3)` resolves.   |
| Dawn   | recent    | Install prefix with `include/webgpu/webgpu.h` + `lib/webgpu_dawn.{lib,so,dylib}`. |
| WAMR   | source    | Checkout of `wasm-micro-runtime` — no install step needed.                        |
| CMake  | 3.20+     |                                                                                   |
| C/C++  | C11 / C++17 | MSVC, Clang, or GCC.                                                            |

### Building Dawn

See Dawn's [CMake quickstart](https://dawn.googlesource.com/dawn/+/HEAD/docs/quickstart-cmake.md). A minimal install:

```sh
git clone https://dawn.googlesource.com/dawn && cd dawn
cmake -S . -B out -DDAWN_FETCH_DEPENDENCIES=ON \
                  -DDAWN_BUILD_SAMPLES=OFF \
                  -DCMAKE_INSTALL_PREFIX=<install-prefix>
cmake --build out --target install
```

Then pass `-DDAWN_DIR=<install-prefix>` when configuring this runtime.

### Fetching WAMR

```sh
git clone https://github.com/bytecodealliance/wasm-micro-runtime.git <wamr-src>
```

Pass `-DWAMR_ROOT_DIR=<wamr-src>` when configuring.

## Build

```sh
cd runtime/sdl
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DWAMR_ROOT_DIR=<path-to>/wasm-micro-runtime \
  -DDAWN_DIR=<path-to-dawn-install>
cmake --build build --config Release
```

Output: `build/wapi_sdl_runtime(.exe)`.

## Run

```sh
./wapi_sdl_runtime app.wasm
./wapi_sdl_runtime app.wasm --dir ./assets
./wapi_sdl_runtime app.wasm --dir assets:./host/assets -- --my-app-arg 42
```

| Flag                    | Meaning                                                       |
|-------------------------|---------------------------------------------------------------|
| `--dir <path>`          | Pre-open a host directory; guest sees it at the same path.    |
| `--dir <guest>:<host>`  | Pre-open with a guest-side path alias.                        |
| `--mapdir <g>::<h>`     | Alternative pre-open syntax.                                  |
| `--`                    | Everything after is passed to the wasm module as `argv[]`.    |

## Entry points

The loaded module must export:

- `wapi_main() -> i32` — one-shot; if only this is exported, the runtime calls it and exits.
- `wapi_frame(timestamp_ns: i64) -> i32` *(optional)* — called every loop iteration after `wapi_main` returns. Return `WAPI_ERR_CANCELED` to quit.

SDL events are polled before each `wapi_frame` call and translated into WAPI events on the shared queue read by `wapi_io.poll` / `wapi_io.wait`.

## Architecture notes

- **Role requests dispatched to SDL enumerators.** `WAPI_IO_OP_ROLE_REQUEST` / `WAPI_IO_OP_ROLE_REPICK` (spec §9.10) are the single entry point for opening any device. The handler switches on `wapi_role_kind_t` and fulfills each role from SDL's own enumeration: `SDL_GetAudioPlaybackDevices` / `SDL_GetAudioRecordingDevices` for audio endpoints, `SDL_GetCameras` for cameras, `SDL_GetMIDI*` for MIDI ports, `SDL_GetJoysticks` for gamepads / haptics, `SDL_GetHIDs` for raw HID. `WAPI_ROLE_ALL` expands to the full SDL list filtered by `prefs`; `target_uid` matches against SDL's serial/vendor tuple; `WAPI_ROLE_FOLLOW_DEFAULT` subscribes to SDL's `SDL_EVENT_AUDIO_DEVICE_ADDED` / `REMOVED` stream and reroutes silently.
- **One binary, many imports.** Each `wapi_host_*.c` file defines a `static NativeSymbol[]` and a `wapi_host_<name>_registration()` accessor. [main.c](src/main.c) collects them and makes one `wasm_runtime_register_natives()` call per module before instantiation.
- **Handle table.** All host-owned resources (SDL windows, audio streams, GPU devices, file handles, io queues, haptics, sensors…) are `int32_t` handles in `g_rt.handles[]`. Handles 1–3 are stdin/stdout/stderr.
- **Memory allocator.** `wapi_memory.alloc` delegates to `wasm_runtime_module_malloc`, with an 8-byte header stashed before each aligned allocation to remember the raw pointer and user-requested size.
- **Events.** Unified 128-byte events flow through one `wapi_event_queue_t` in `g_rt`. SDL events, I/O completions, and timeouts all funnel in. `wapi_io.wait` blocks on `SDL_WaitEvent[Timeout]` so a single call wakes on both input and I/O.
- **GPU.** One shared `WGPUInstance` (`g_rt.wgpu_instance`) — lazy-created by `wapi_gpu.request_device`, released at shutdown. Surfaces are created via `SDL_GetWindowProperties` → platform native chain struct → `wgpuInstanceCreateSurface`.
- **Rendering path.** Apps drive WebGPU directly through `wapi_gpu` imports. There is no SDL_GPU usage; all on-screen rendering is Dawn → platform.

## Relation to `runtime/desktop`

`runtime/desktop/` is evolving into a pure native host (Win32 / Cocoa / X11+Wayland, no SDL). `runtime/sdl/` is the leaner alternative: a small SDL3+Dawn host that covers the capabilities SDL can reach and exposes nothing else.

## License

Same as the rest of WAPI — public domain / CC0.
