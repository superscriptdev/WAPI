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

`hello_triangle.wasm` and `examples/hello_game/` both run against wgpu-native v29 on Windows. Spec-complete host modules on the Windows runtime: `core`, `io_bridge` (10 fns), `surface`, `window`, `input` (mouse + keyboard + IME commit + IME preedit + gamepad XInput + cursor warp/image + **touch via WM_TOUCH + pen via WM_POINTER + process-global hotkeys via RegisterHotKey + tracked `WAPI_HTYPE_INPUT_DEVICE` handle pool with stable UIDs + raw HID enumeration/open/read/write/feature**), `clock`, `env`, `random`, `wgpu` (40+ trampolines covering both paths), `compression` (gzip / zlib / raw via miniz), `module` (library mode + 16 MiB shared-memory pool), and the file / transfer / log / timeout opcodes inside `io_bridge`.

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
- ✅ **Touch (WM_TOUCH)** — `RegisterTouchWindow` on every window, 10-slot finger tracker keyed on OS `dwID`; `touch_get_info` / `touch_finger_count` / `touch_get_finger` backed by `SM_DIGITIZER` / `SM_MAXIMUMTOUCHES`.
- ✅ **Pen / stylus (WM_POINTER)** — `EnableMouseInPointer(TRUE)` at init, `WM_POINTERDOWN/UP/UPDATE` filtered on `PT_PEN`, pressure/tilt/rotation/eraser decoded from `POINTER_PEN_INFO`; `pen_get_info` / `pen_get_axis` / `pen_get_position` read last-known state.
- ✅ **Real `WAPI_HTYPE_INPUT_DEVICE` pool** — all devices flow through tracked handles: aggregate mouse/keyboard/pointer/touch/pen pre-minted at init, per-XInput-slot gamepad handles alloc on ADDED / free on REMOVED with a generation byte in the UID for reconnect disambiguation.
- ✅ **Hotkeys** — `RegisterHotKey(NULL, atom, ...)` with HID→VK + `WAPI_KMOD_*`→`MOD_*` tables and `MOD_NOREPEAT`; WM_HOTKEY intercepted in `drain_messages` → `WAPI_EVENT_HOTKEY`. All live bindings released on shutdown.
- ✅ **Raw HID backend scaffold** — `platform/win32/wapi_plat_win32_hid.c` enumerates via SetupDi + `HidD_GetHidGuid`, opens with overlapped R/W fallback to read-only, exposes `hid_enumerate` / `open` / `close` / `get_info` / `read_report` / `write_report` / `send_feature` / `get_feature`. `hid_request_device` / `hid_get_info` / `hid_send_report` / `hid_send_feature_report` / `hid_receive_report` wired through the `WAPI_HTYPE_INPUT_DEVICE` pool with device-close releasing the HANDLE.
- ✅ **SDL_GameControllerDB loader** — `wapi_host_gamepaddb.c` parses `GUID,name,key:value,...` lines, honors `platform:Windows`, loads from `$WAPI_GAMEPADDB` → `<exe>/data/gamepaddb.txt` → `~/.wapi/gamepaddb.txt`. Stub DB file vendored; refresh from upstream as a separate commit.
- **HID gamepad decoder** — *remaining*: `HidP_*` report parser that consumes the DB mapping and emits `WAPI_PLAT_EV_GPAD_*` events for non-XInput pads (DualShock / DualSense / Switch Pro / Joy-Cons / flight sticks). Today such pads enumerate via `wapi_hid_*` but do not appear as `WAPI_DEVICE_GAMEPAD`.
- **Force-feedback on DirectInput** — `IDirectInputDevice8::CreateEffect`. XInput rumble + per-vendor HID covers the common case first.
- **Gamepad trigger-rumble / LED / sensors / touchpad** — lands with the HID gamepad decoder (no XInput surface for them).

### A2. Audio — close out `wapi_audio.h`
- ✅ **Recording (capture)** — WASAPI `eCapture` endpoints enumerate + open via `WAPI_PLAT_AUDIO_DEFAULT_RECORDING`; worker thread drains `IAudioCaptureClient` packets into a per-stream ring, converting device→guest format; `stream_get` pulls guest-format bytes. Silent packets are zero-filled; ring-full drops oldest to keep latency bounded.
- **Real stream format conversion** — full sample-format convert works (U8/S16/S32/F32 via float intermediate). Channel count / sample-rate conversion still relies on `AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM`; a host-side resampler is deferred.
- **Async `WAPI_IO_OP_AUDIO_WRITE`** — currently NOSYS; sync `put_stream_data` path works.

### A3. Clipboard / transfer — close out `wapi_transfer.h`
- ✅ **LATENT (clipboard) — text** `text/plain` via `CF_UNICODETEXT`.
- ✅ **LATENT (clipboard) — images** `image/bmp` via `CF_DIB`. `BITMAPFILEHEADER` prepended on read, stripped on write so the guest sees standalone BMP file bytes.
- ✅ **LATENT (clipboard) — files** `text/uri-list` (read-only) via `CF_HDROP` → `file://`-prefixed, percent-encoded, CRLF-separated URIs. Write-back deferred (needs DROPFILES struct + path concat; niche).
- ✅ **POINTED (drag & drop)** external file drops via `DragAcceptFiles` + `WM_DROPFILES` → `WAPI_EVENT_TRANSFER_DELIVER` with surface-local drop point; guest reads the URI-list payload through `wapi_transfer_read(POINTED, "text/uri-list")`. Every surface auto-registers as a drop target. Drag-enter/over/leave phases still need OLE `RegisterDragDrop` + `IDropTarget` — later upgrade.
- ✅ **ROUTED (share)** — Windows system share sheet via `ShellExecuteExW(lpVerb=L"share")`. Text/image payloads materialize into `%TEMP%\wapi-share-<qpc>.<ext>` and the share verb is invoked on that path; `text/uri-list` shares the first referenced file directly (percent-decoded). Under the hood the shell verb routes through `DataTransferManager`, so the user sees the same native share UI as Explorer's Share context-menu entry. Multi-file share and rich MIME registration via `RegisterClipboardFormatW` are later upgrades.

### A4. Async IO-dispatch integration
- ✅ `WAPI_IO_OP_CONNECT` / `SEND` / `RECV` / `CLOSE` / `NETWORK_RESOLVE` — op-ctx handlers in `wapi_host_net.c`. Quality-flag → transport resolver is exhaustive: **TCP** (Winsock), **UDP** (Winsock, connected), **TCP+TLS** (Schannel SSPI handshake + `EncryptMessage`/`DecryptMessage` record layer with `SEC_PKG_STREAM_SIZES`-correct buffering), **DTLS** (same Schannel path with `ISC_REQ_DATAGRAM`), **QUIC** (msquic v2 dynamic-loaded via `LoadLibraryW("msquic.dll")`; one bidi stream per connection, callbacks marshalled to Win32 events for blocking send/recv), **reliable multicast** (PGM deprecated — `NOSYS`). Nonsensical combos → `ERR_INVAL`.
- ✅ `wapi_net_qualities_supported(qualities) -> i32` sync import — pure capability probe. Returns 1/0/`ERR_INVAL`. No sockets opened, no DllMain runs (msquic availability via `LoadLibraryExW(LOAD_LIBRARY_AS_DATAFILE)`). Lets guests pick among transports at startup without try-and-fail.
- ✅ `WAPI_IO_OP_CRYPTO_HASH` — one-shot SHA-256/384/512/SHA-1 via BCrypt CNG.
- **Remaining crypto** — `CRYPTO_ENCRYPT`/`DECRYPT`/`SIGN`/`VERIFY`/`KEY_*`/`DERIVE_KEY`/`HASH_CREATE` still NOSYS. The sync `platform_*` helpers exist; op-ctx wrappers follow the HASH pattern.
- **Remaining network** — `ACCEPT` / `NETWORK_LISTEN` / `NETWORK_CHANNEL_OPEN` / `NETWORK_CHANNEL_ACCEPT` still NOSYS. Listen/accept add no new transport; channel ops are QUIC-only.
- **Remaining** — codec, sensor, geolocation, speech, capture, bt, usb, midi, nfc, camera — bring online per A6.

### A5. Compression — close out `wapi_compression.h`
- **zstd + lz4** — currently NOSYS. miniz already vendored for gzip/zlib/raw. Blocked on vendoring upstream single-file amalgamations (zstd `build/single_file_libs/zstd.c`, lz4 `lz4frame.c` + deps); drop those under `src/third_party/zstd` and `src/third_party/lz4` and the dispatch in `wapi_host_compression.c` becomes a 10-line change.

### A6. Remaining headers — first real Windows impl

Bring each online in priority order. Audit the header first (module name match, sync-vs-async split, stringview-ptr collapse, i64 for `wapi_size_t`) before writing code.

| Priority | Header | Windows surface |
|---|---|---|
| ✅ **P0** | `wapi_dialog`  | Win32 IFileOpenDialog / IFileSaveDialog / MessageBoxW / ChooseColorW / ChooseFontW + folder picker. Async via `WAPI_IO_OP_DIALOG_*`; dialogs run modal on the dispatch thread until the Phase C broker lands. |
| ✅ **P0** | `wapi_menu`    | Native HMENU popups + menubars, submenus via MF_POPUP. WM_COMMAND routes through a (token,slot) id encoding; TPM_LEFTALIGN context popup, SetMenu for bars. Emits `WAPI_EVENT_MENU_SELECT`. |
| ✅ **P0** | `wapi_tray`    | `Shell_NotifyIconW` with a hidden message-only window; right-click pops a context menu built from the `wapi_tray_menu_item_t` array; clicks emit `WAPI_EVENT_TRAY_CLICK` / `WAPI_EVENT_TRAY_MENU`. Icon accepts ICO bytes; PNG falls back to default app icon pending WIC decode. |
| ✅ **P0** | `wapi_taskbar` | `ITaskbarList3` — `SetProgressState` + `SetProgressValue`, `SetOverlayIcon` / clear, `FlashWindowEx` for attention. `set_badge` remains `NOTSUP` on Win32 (emulate via overlay icon). Thumb-buttons deferred — low-value next to overlay/progress. |
| ✅ **P0** | `wapi_notifications` (async NOTIFY_SHOW) | `Shell_NotifyIconW` balloon through a shared hidden notify-icon; async via `WAPI_IO_OP_NOTIFY_SHOW`, returns a minted id inlined in the CQE payload. Balloon click fires a `WAPI_EVENT_TRAY_CLICK` carrying the id so guests can match it to their `user_data`. Toast via `Windows.UI.Notifications` is a later upgrade path. |
| ✅ **P1** | `wapi_http`    | `WinHTTP` through the async `WAPI_IO_OP_HTTP_FETCH` opcode. `WinHttpCrackUrl` → `WinHttpOpen`/`Connect`/`OpenRequest` with `WINHTTP_FLAG_SECURE` for HTTPS; body drained via `WinHttpQueryDataAvailable` + `WinHttpReadData` into the caller's buffer. Completion's `result` = bytes written, `cqe_flags` = HTTP status code. Overflow drains the socket then completes with `ERR_OVERFLOW`. |
| ✅ **P1** | `wapi_process` | `CreateProcessW` + `CreatePipe`. Stdin/stdout/stderr support `INHERITED` / `PIPE` / `NULL` modes independently; parent-side pipe ends are duplicated non-inheritable so child EOF is detectable. `pipe_read` returns 0 on `ERROR_BROKEN_PIPE` (child closed its write end). Pipes are `WAPI_HTYPE_PIPE` handles, process is `WAPI_HTYPE_PROCESS`; `destroy` detaches the child and releases both. |
| **P1** | `wapi_thread`  | `CreateThread` / TLS / condvar. Blocked on wasmtime multi-store threading: `thread_create`'s guest entry-point needs per-thread stores with shared memory. Sync primitives alone are shippable but useless without threads to use them. |
| ✅ **P1** | `wapi_display` | `EnumDisplayMonitors` + `MONITORINFOEXW` + `GetDpiForMonitor` + `EnumDisplaySettingsW` for Hz. Work-area usable bounds. EDID fetched from `HKLM\SYSTEM\CurrentControlSet\Enum\DISPLAY\...` via the monitor's device-interface path; bytes 8-11 unpack to the 7-char `<PnP><Product>` token passed to [devicedb](runtime/desktop/src/wapi_host_devicedb.c) as `(KIND_EDID, key, 7)` (see §A10). When the DB has an entry: `display_get_subpixels` returns the raw `{color, x, y}` array verbatim, `subpixel_count` populates `display_get_info`, `display_get_panel_info` returns the 72B primary-measurement struct (panel class, width_mm / height_mm / diagonal_mm, peak SDR + HDR nits, min millinits, CIE primaries + white point, sRGB/P3/Rec2020/AdobeRGB coverage %, refresh-range min/max mHz, update class, touch/stylus capabilities), and `display_get_geometry` + `display_get_cutouts` return envelope (full / rounded_rect / circle) + corner radii + hardware cutouts. |
| ✅ **P1** | `wapi_sysinfo` | `GetNativeSystemInfo` + `RtlGetVersion` (avoids compat shim) + CPUID (SSE/AVX/AVX2/AVX-512F/FMA/BMI1/2/POPCNT/LZCNT/F16C/PCLMUL/AES/SHA/CRC32) + `GlobalMemoryStatusEx` + `GetLogicalProcessorInformation` for physical cores. Dark-mode + DWM accent populate `sysinfo_t`; `host_get` answers `os.family` / `os.version` / `runtime.{name,version}` / `device.form`. |
| ✅ **P1** | `wapi_theme`   | Personalize-registry dark-mode + `DwmGetColorizationColor` accent. High-contrast via `SPI_GETHIGHCONTRAST`; reduced-motion via `SPI_GETCLIENTAREAANIMATION`; font-scale approximated from system DPI. |
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

### A10. Community databases (gamepads + devicedb)

OS APIs stop short of the metadata apps actually need for rendering: sub-pixel layout, precise panel class, exact DCI-P3 coverage, physical envelope (rounded corners / notches), touch / stylus on this particular surface, pen-tilt/rotation quirks. No platform exposes these — DB lookup is the primary path on every backend.

**`gamepaddb.txt`** — ✅ loader shipped ([wapi_host_gamepaddb.c](runtime/desktop/src/wapi_host_gamepaddb.c)). Refresh on release cadence from https://github.com/mdqinc/SDL_GameControllerDB; user override at `~/.wapi/gamepaddb.txt`. Consumed by the HID gamepad decoder when it lands.

**`devicedb.txt`** — ✅ scaffold shipped ([wapi_host_devicedb.c](runtime/desktop/src/wapi_host_devicedb.c)). Cross-platform panel reference, one row per physical panel, with multiple typed keys per row. Schema principle: **primary measurements, not labels.** Marketing tiers (HDR400, G-Sync Compatible, "is P3") are derivable from the real numbers and often lie in EDID, so they're not represented. A row carries:
- sub-pixel layout — consumed by `display_get_subpixels`,
- panel class (`ips_lcd` / `va_lcd` / `tn_lcd` / `oled` / `woled` / `qdoled` / `microled` / e-ink / plasma / CRT / DLP / LCoS / projector),
- physical `width_mm` / `height_mm` / `diagonal_mm`,
- measured luminance: `peak_sdr_nits` / `peak_hdr_nits` / `min_mnits` (millinits for OLED floor precision),
- CIE xy `primaries` of R/G/B/W and `white_point`,
- gamut coverage % against sRGB / P3 / Rec2020 / AdobeRGB,
- `refresh` range (min Hz, max Hz) — fixed or VRR span in milliHertz,
- `update` class (fast / slow / very_slow) for e-ink friendliness,
- physical geometry: envelope (`full` / `rounded_rect` / `circle`) + per-corner radii + hardware cutout rects,
- surface input: `has_touch` / `has_stylus` / pressure-levels / tilt.

Key namespaces (each entry may carry keys from multiple kinds):
- `edid` — 7-char `<PnP><Product>` (desktop monitors, laptop internals with EDID, TVs over HDMI)
- `apple` — sysctl `hw.machine` (iPhone16,1 / MacBookPro18,1 / Watch6,1 / AppleTV11,1 / RealityDevice14,1) — built-in panel of Apple devices where no EDID is exposed
- `android` — `<manufacturer>/<model>` lowercase from Build.* — Android phone / tablet / Wear OS / Android TV built-in
- `dmi` — SMBIOS chassis identifier (secondary, for SKUs whose panel varies)
- `dt` — Linux device-tree model string (embedded)
- `smarttv` — Tizen / webOS / Fire TV / Roku vendor + model
- `console` — fixed console SKUs (sony/ps5, nintendo/switch-oled)

The lookup is **per-display**, not per-device: each enumerated display asks the DB with whichever key kind the host can produce for *that* display. Example:
- Windows desktop with an external Dell monitor: `lookup(KIND_EDID, "DEL40F9", 7)`
- MacBook Air M2 built-in display: `lookup(KIND_APPLE, "Mac14,2", 7)`
- MacBook Air driving an external Studio Display: `lookup(KIND_EDID, "APP...", 7)` on the second display
- Nintendo Switch OLED docked → TV: `lookup(KIND_EDID, "<tv-PnP>", 7)` on the HDMI output

Each linuxhw/EDID, Apple's device-identifier list, Google's supported-devices list etc. seeds one key namespace. A scraper pipeline per namespace produces rows that share fields when the panel is known to be identical across keys; the multi-key-per-entry format makes that dedup natural.

File format: TSV. Entry line = `<name>\t<field>:<value>\t...`; key line = indented `<namespace>:<key>`; comments start `#`. Tab separator means Apple's comma-bearing identifiers (`iPhone16,1`) need no escaping. Search order: `$WAPI_DEVICEDB` → `<exe>/data/devicedb.txt` → `~/.wapi/devicedb.txt`. Duplicate keys across entries merge field-by-field.

Shipped: parser, loader, multi-key lookup, EDID-path wiring on Windows (`display_get_*` all consult the DB via `KIND_EDID`), `display_get_geometry` + `display_get_cutouts` return DB-sourced envelope / corners / cutouts (zero-fallback when unmatched — correct for external monitors). Stub `runtime/desktop/data/devicedb.txt` with the full format spec and key-namespace vocabulary; seed entries are a separate follow-up (scrapers per namespace).

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
