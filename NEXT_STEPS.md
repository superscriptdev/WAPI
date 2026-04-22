# WAPI — Next Steps

What's left across the project, framed against [VISION.md](VISION.md). Repo-level tracker; compact, header-driven.

**Order of operations (do not skip ahead):**

1. **Phase A — 100% Windows coverage.** Every header has a real Windows host impl; every `WAPI_IO_OP_*` the Windows platform can support returns real data.
2. **Phase B — 100% Web coverage.** Browser shim reaches parity with the Windows runtime at the ABI level.
3. **Phase C — Windows + Web interop + install-to-system.** Native-messaging bridge, shared module cache, `wapi_module_join` across host types, `.wapp` install flow.
4. **Phase D — macOS + Linux.** Only starts when A–C are closed.

---

## Where we are

**Stage 1 (Prove the ABI) — Windows: hello_triangle + hello_game paths complete.**

`hello_triangle.wasm` and `examples/hello_game/` both run against wgpu-native v29 on Windows. Spec-complete host modules on the Windows runtime: `core`, `io_bridge` (10 fns), `surface`, `window`, `input` (mouse + keyboard + IME commit + IME preedit + gamepad XInput + cursor warp/image), `clock`, `env`, `random`, `wgpu` (40+ trampolines covering both paths), `compression` (gzip / zlib / raw via miniz), `module` (library mode + 16 MiB shared-memory pool), and the file / transfer / log / timeout opcodes inside `io_bridge`.

Browser shim: opcode unification landed, service-worker host drafted, module cache with SRI-keyed dedup working — but full header coverage lags Windows.

---

## ABI baseline (load-bearing — verified against headers, 2026-04)

- Top-level host imports under module `"wapi"`: `panic_report(ptr,len)`, `exit()`, `allocator_get()`.
- Async I/O lives entirely under module `"wapi_io_bridge"` with **10 functions**: `submit`, `cancel`, `poll`, `wait`, `flush`, `cap_supported`, `cap_version`, `cap_query`, `namespace_register`, `namespace_name`. Guests build a `wapi_io_t` vtable on top via the reactor shim.
- `wapi_io_op_t` is **80 bytes**; `addr`/`len`/`addr2`/`len2`/`user_data`/`result_ptr` are all `u64`.
- `wapi_stringview_t` is 16B; clang's wasm32 ABI passes it **by pointer** when taken by value, so host imports register as `(i32 sv_ptr)` not `(i64 data, i64 length)`.
- WebGPU's `WGPUFlags = uint64_t` — every usage / mask field is u64 with 8-byte alignment. `WGPUFuture` returns as `i64`.
- Events are `WAPI_EVENT_*`; surface lifecycle uses **window** events (`WINDOW_CLOSE = 0x0210` …); only `SURFACE_RESIZED = 0x0200` and `SURFACE_DPI_CHANGED = 0x020A` are surface events.
- IME: `IME_START = 0x320`, `IME_UPDATE = 0x321`, `IME_COMMIT = 0x322`, `IME_CANCEL = 0x323`. `wapi_ime_event_t` is 40B; preedit text + segments read via accessors keyed on `sequence`.
- Keyboard: `keyboard_handle` at +16, all other fields shift +4. Touch: `i32 touch_handle + i32 finger_index` (not u64/u64).
- `wapi_surface_desc_t` is 24B `{u64 nextInChain, i32 width, i32 height, u64 flags}`; title via chained `wapi_window_desc_t` (sType `WAPI_STYPE_WINDOW_CONFIG = 0x0001`).
- `wapi_gpu.h` only defines `wapi_gpu_surface_source_t` (sType `0x0101`) — guests use vanilla webgpu.h; the host rebuilds the platform-native surface source from the WAPI surface handle before `wgpuInstanceCreateSurface`.
- Capabilities are NOT an import module. Queries go through `wapi_io_bridge.cap_*`; grants submit `WAPI_IO_OP_CAP_REQUEST`.
- `wapi.random` is its own capability with import module `wapi_random` (`get`, `get_nonblock`, `fill_seed`).

**Headers are authoritative — when something doesn't fit, fix the host, not the header.**

---

## Phase A — 100% Windows coverage (current focus)

The bar: every `include/wapi/*.h` has a real Windows host impl; every deferred `WAPI_IO_OP_*` the Windows platform can support returns real data (NOSYS only where Windows genuinely can't do it). Nothing in Phase B–D starts until this list is zero.

### A1. Input — close out `wapi_input.h`
- **Touch (WM_TOUCH)** — `RegisterTouchWindow` on window create when flag set; emit `WAPI_PLAT_EV_FINGER_*`. Wire `touch_get_info` / `touch_finger_count` / `touch_get_finger`.
- **Pen / stylus (WM_POINTER)** — `EnableMouseInPointer(TRUE)` + `IS_POINTER_PEN_WPARAM`. Wire `pen_get_info` / `pen_get_axis` / `pen_get_position`.
- **Real `WAPI_HTYPE_INPUT_DEVICE` pool** — retire synthetic mouse=1/keyboard=2/pointer=3 + slot-as-handle gamepads for tracked handles with stable uids. Required before hotplug.
- **Hotkeys** — `RegisterHotKey` + WM_HOTKEY → `WAPI_EVENT_HOTKEY`. Gamepad / HID variants polled while device is visible.
- **Generic HID + community mapping DB** — DualShock 4 / DualSense / Switch Pro / Joy-Cons / flight sticks via raw HID. `platform/win32/wapi_plat_win32_hid.c` (SetupDi + HidD/HidP); `platform/hidapi/` per-vendor decoders; `runtime/desktop/data/gamepaddb.txt` (SDL_GameControllerDB format).
- **Force-feedback on DirectInput** — `IDirectInputDevice8::CreateEffect`. XInput rumble + per-vendor HID covers the common case first.
- **Gamepad trigger-rumble / LED / sensors / touchpad** — lands with the HID backend (no XInput surface for them).

### A2. Audio — close out `wapi_audio.h`
- **Recording (capture)** — `WAPI_PLAT_AUDIO_DEFAULT_RECORDING` + capture-device enumeration; WASAPI `eCapture` vs `eRender`.
- **Real stream format conversion** — full sample-format + channel + sample-rate conversion when `src != dst`. Currently pass-through + S16/F32 convert only. Prefer `AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM` when shared-mode engine handles it.
- **Async `WAPI_IO_OP_AUDIO_WRITE`** — currently NOSYS; sync `put_stream_data` path works.

### A3. Clipboard / transfer — close out `wapi_transfer.h`
- **Non-text clipboard** — images (CF_DIB), files (CF_HDROP), custom MIME via `RegisterClipboardFormatW`.
- **Drag & drop** — OLE `RegisterDragDrop` + `IDropTarget` on the HWND. Add `wapi_plat_window_register_drop_target` + `WAPI_PLAT_EV_DROP_*`.

### A4. Async IO-dispatch integration
Templates exist (`wapi_host_transfer_offer_op` / `read_op`, `wapi_host_compress_process_op`). Remaining handlers need the `op_ctx_t` signature and `wapi_host_io.c` wiring:
- `WAPI_IO_OP_CRYPTO_*` ← `cb_hash` / `cb_encrypt` / `cb_sign` etc.
- `WAPI_IO_OP_CONNECT` / `ACCEPT` / `SEND` / `RECV` / `NETWORK_*` ← `host_net_*`.
- `NOTIFY_SHOW`, codec, sensor, geolocation, dialog, http, speech, capture, bt, usb, midi, nfc, camera — bring online per A6.

### A5. Compression — close out `wapi_compression.h`
- **zstd + lz4** — currently NOSYS. miniz already vendored for gzip/zlib/raw; vendor zstd + lz4 and wire.

### A6. Remaining headers — first real Windows impl

Bring each online in priority order. Audit the header first (module name match, sync-vs-async split, stringview-ptr collapse, i64 for `wapi_size_t`) before writing code.

| Priority | Header | Windows surface |
|---|---|---|
| **P0** | `wapi_dialog`  | Win32 common dialogs (open / save, message box, color). |
| **P0** | `wapi_menu`    | WM_COMMAND + native menu handles. |
| **P0** | `wapi_tray`    | `Shell_NotifyIconW`. |
| **P0** | `wapi_taskbar` | `ITaskbarList3` (progress, thumb-buttons, overlay icon). |
| **P0** | `wapi_notifications` (async NOTIFY_SHOW) | Toast via `Windows.UI.Notifications` or `Shell_NotifyIconW` balloon. |
| **P1** | `wapi_http`    | `WinHTTP` or `Windows.Web.Http`. |
| **P1** | `wapi_process` | `CreateProcess` + pipes. |
| **P1** | `wapi_thread`  | `CreateThread` / TLS / condvar. |
| **P1** | `wapi_display` | `EnumDisplayMonitors` + `GetDpiForMonitor`. |
| **P1** | `wapi_sysinfo` | `GetNativeSystemInfo`, `GetVersionExW`, CPUID. |
| **P1** | `wapi_theme`   | DWM dark-mode, high-contrast detection. |
| **P2** | `wapi_screencapture`, `wapi_eyedropper` | Desktop Duplication API / GDI color pick. |
| **P2** | `wapi_codec`, `wapi_video` enrichment   | Media Foundation decoders. |
| **P2** | `wapi_authn`, `wapi_biometric`, `wapi_user`, `wapi_payments` | Windows Hello, OS account APIs. |
| **P3** | `wapi_bluetooth`, `wapi_usb`, `wapi_serial`, `wapi_midi`, `wapi_nfc`, `wapi_hid` | Peripheral stack; lands with HID backend. |
| **P3** | `wapi_camera`, `wapi_speech`, `wapi_mediasession`, `wapi_audioplugin`, `wapi_xr` | Media Foundation / WinRT. |
| **P3** | `wapi_haptics`, `wapi_orientation`, `wapi_sensors`, `wapi_geolocation`, `wapi_networkinfo`, `wapi_power` | Windows Sensor API; mostly useful on laptops / tablets. |
| **P3** | `wapi_barcode`, `wapi_register`, `wapi_contacts`, `wapi_encoding` | Niche / OS-bound; on demand. |

### A7. Module linking — close out `wapi_module.h`
Library mode + 16 MiB host-owned shared-memory pool are spec-complete. Remaining on Windows:
- **`wapi_module_join` (service mode)** — refcounted shared instance keyed on `(hash, name)`.
- **`wapi_module_lend` / `reclaim`** — controlled cross-module memory borrowing.
- **`wapi_module_io_proxy` / `allocator_proxy`** — per-call vtables so a child module's I/O routes through the parent (spec §10.7). Ships with A8.
- **Guest-mapped memory 1** — zero-copy multi-memory alternative to the host-owned pool (spec §10.1).
- **`wapi_module_copy_in` / `prefetch` / `get_desc` (with metadata)** — currently zero-init or `NOTSUP`.

### A8. Allocator vtable (`wapi.allocator_get`)
Returns 0 (NULL) today; guest reactor shim provides its own bump allocator at `__heap_base`. Real wiring writes a `wapi_allocator_t` into guest memory whose function pointers are table-set'd wasmtime trampolines — same machinery `wapi_module_io_proxy` will need.

### A9. Windows render polish
- **HDR / wide-gamut** — Dawn surface config accepts `HDR10`/`scRGB`; `DXGI_OUTPUT_DESC1` for capability probing.
- **Multi-window wake fairness** — `wapi_plat_wait_events` wakes on any HWND; works correctly today, tune only if needed.

---

## Phase B — 100% Web coverage (starts when Phase A = 0)

Bring the browser shim in [runtime/browser/](runtime/browser/) to the same header-coverage bar Windows hit in Phase A. Every capability that maps to a Web API works in Mode 1 (pure shim, no system runtime). NOSYS only where the browser genuinely can't express it (raw HID without permission, direct USB without WebUSB, etc.).

### B1. GPU + surface parity
- `wapi_host_wgpu.c` equivalent in JS against `navigator.gpu`. Same 40+ entry points the Windows runtime already covers.
- Surface via `<canvas>` + `wgpu_canvas.getContext("webgpu")`. `wapi_gpu_surface_source_t` branch for the browser path (canvas element id).

### B2. Input parity
- Keyboard / mouse / wheel → `KeyboardEvent` / `PointerEvent` / `WheelEvent`.
- IME via `CompositionEvent` + IME composition events (same 0x320/0x321/0x322/0x323).
- Touch / pen via Pointer Events (`pointerType === "pen"|"touch"`).
- Gamepad via the Gamepad API + `gamepadconnected`/`gamepaddisconnected`.
- Hotkeys: best-effort at the page scope (no global equivalent); return NOSYS with a clear reason.

### B3. Audio parity
- WebAudio `AudioWorklet` for stream playback; `getUserMedia({audio: true})` for capture.
- Full `wapi_plat_audio_*` surface the Windows runtime ships.

### B4. I/O + networking
- Filesystem → OPFS (`navigator.storage.getDirectory`).
- HTTP → `fetch`. WebTransport → `WebTransport`. WebSocket → `WebSocket`.
- Raw TCP/UDP: NOSYS in the browser (handed to the system runtime in Phase C).

### B5. Async opcodes
Mirror the Windows async-dispatch push: every `WAPI_IO_OP_*` the browser can implement returns real completions. Remaining route through the native runtime once the bridge lands in Phase C.

### B6. Remaining headers
Same matrix as Phase A, scored for Web availability: `dialog` ≈ `window.showOpenFilePicker`, `notifications` = `Notification`, `geolocation` = `navigator.geolocation`, `screencapture` = `getDisplayMedia`, `bluetooth` = Web Bluetooth (permission-gated), `usb` = WebUSB, `serial` = Web Serial, `hid` = WebHID, `nfc` = Web NFC, `speech` = Web Speech API, `xr` = WebXR, `clipboard` = async Clipboard API + file/MIME, `payments` = Payment Request API, `midi` = Web MIDI.

### B7. Extension / service-worker host production-ready
- Module cache with SRI-keyed dedup: already working, but audit against Windows behaviour.
- Permission prompts consistent with Windows runtime's eventual broker UI.
- Extension shipped to Chromium / Firefox / Safari stores.

---

## Phase C — Windows + Web interop + install-to-system (starts when Phase B = 0)

Close VISION §3/§4: one logical runtime, three deployment surfaces.

- **Native-messaging bridge** — JSON-over-stdio endpoint so the browser extension can offload to the Windows runtime (VISION Mode 2).
- **Shared module cache** — extension delegates cache lookups to the Windows runtime when installed; per-browser IndexedDB becomes a hot tier in front of the system cache.
- **`wapi_module_join` across host types** — same `(hash, name)` service joined from a browser tab and from a desktop app resolves to one running instance. Requires A7 `join` + bridge.
- **Install-to-system (VISION Mode 3)** — browser hands `.wapp` to Windows runtime, which registers it as a real OS app (Start Menu, persistent permissions, own window). Requires `.wapp` + `wapi://` registration + permission broker UI.
- **Permission broker with OS-native UI** — Windows modal dialogs for `WAPI_IO_OP_CAP_REQUEST`. Currently `cap_request` auto-grants from a static table in `wapi_host_io.c`.
- **System installer** — Windows MSI that drops the runtime + registers `.wapp` + `wapi://` + native-messaging host manifest.

Done when: the same `.wapp` runs unchanged via URL (no runtime), via URL with extension offloading to native, and as an installed system app — and a shared service joined from a tab *is* the same instance a desktop app joins.

---

## Phase D — macOS + Linux (starts when Phase C = 0)

Only now does Stage 1's "same `.wasm` on Win/Mac/Linux" matter in earnest. Both backends already have CMake slots wired and stubbed file paths.

### D1. macOS / Cocoa
Full `wapi_plat.h` on Cocoa: `NSWindow`, `NSEvent`, `CAMetalLayer`, `NSPasteboard`, drag&drop via `NSDraggingDestination`, per-monitor retina scaling. Event pump: `[NSApp nextEventMatchingMask:…untilDate:]`; `wapi_plat_wake` posts a custom NSEvent. GPU native handle: kind=`WAPI_PLAT_NATIVE_COCOA`. `wapi_host_wgpu.c`'s surface-source switch already has the MetalLayer branch. CoreAudio (AUv3) for `wapi_plat_audio_*`. Pkg installer + `.wapp` / `wapi://` registration via LaunchServices.

### D2. Linux / Wayland + PipeWire
Full `wapi_plat.h` on Wayland: `wl_compositor`, `xdg_shell`, `wl_seat`, `wl_keyboard`/`pointer`/`touch`, `wl_data_device`, `xdg_decoration`, xkbcommon. Event pump: `poll(2)` on `wl_display_get_fd()` + pipe wake fd. CMake already wires `wayland-scanner`. GPU native handle: kind=`WAPI_PLAT_NATIVE_WAYLAND`. Audio over `libpipewire-0.3`. **No X11 fallback.** deb/rpm/flatpak installer + desktop-entry MIME registration.

---

## Rough edges (small, high-leverage cleanups)

- `wapi_module_get_desc` returns zero-init (header doesn't mandate metadata; future descriptor section is plausible).
- `mouse_set_cursor_image` is per-window in the backend, exposed as aggregate; one HCURSOR per surface freed on destroy / replace. Revisit if a use case wants per-mouse-handle scoping.
- IME segment ring: 8 slots × 1 KiB. Spec contract is "valid until next io.poll" — current capacity is well over that, but document at `wapi_input.h` if it ever stings.
