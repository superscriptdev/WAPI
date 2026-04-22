# WAPI Vision

**Where this is going, and why it matters.**

`README.md` is the "what is this?" entry point. This document is the strategic one: what WAPI is trying to replace, how the runtime / browser extension / OS-integration story ties together, and the staged path to get there. It is a living document — sections evolve as milestones land and questions resolve.

---

## 1. The Vision

The line between web, desktop, and mobile is dissolving. Every application is becoming a sandboxed capability consumer running on hardware it doesn't directly touch. WAPI is the runtime that makes that explicit: one binary, one ABI, every platform. Install it once; everything that used to require a browser, Electron, .NET, the JVM, Flash, or a per-OS build targets WAPI instead. Endgame: browsers and operating systems ship WAPI natively and the browser-as-application-platform disappears.

## 2. What WAPI Replaces (and what it doesn't)

- **Browsers** — subsumes the app runtime (Wasm + GPU + I/O + sandbox). Does not subsume the document web. HTML/CSS/JS pages keep working; WAPI is additive, served from the same URL space.
- **Electron / Tauri** — fully replaces. No bundled browser engine, no duplicate Chromium per app.
- **.NET / JVM** — replaces the runtime layer. Managed languages compile to Wasm (WasmGC) and target WAPI for I/O instead of platform-specific BCLs.
- **Flash** — the "portable applet" role, with a modern capability-based security model instead of a plugin sandbox.
- **Native SDKs (Win32, Cocoa, UIKit, Android)** — replaces for the portable-application use case. Not replacing OS-internal system programming.
- **SDL / GLFW + Vulkan/Metal/DX12 directly** — replaces for portable apps. Direct native access stays available for engines and drivers.

## 3. The Runtime Architecture

*One installed runtime, one browser extension, apps run anywhere.* The architectural leverage is that these are not three parallel hosts competing — they are **one logical runtime with three deployment surfaces**, tied together by content-addressed modules and a shared capability broker.

### 3a. The System Runtime

A single installable binary per OS (Windows MSI, macOS pkg, Linux deb/rpm/flatpak, Android APK, iOS regulatory-pending). It provides:

- **Wasm execution engine.** Wasmtime today, swappable.
- **Host implementations** of every WAPI capability — especially the ones a browser cannot do cleanly: direct GPU, low-level audio, full filesystem, background services, HID, USB, MIDI.
- **Module cache** keyed by SHA-256 content hash. Already spec'd in [spec/wapi-spec.md](spec/wapi-spec.md) §10: the same `.wasm` fetched from any URL or loaded from any disk path produces the same cache entry. This cache is the backbone of deep integration — every other feature builds on it.
- **Shared service registry.** `wapi_module_join(hash, url, name, module)` is already declared in [include/wapi/wapi_module.h](include/wapi/wapi_module.h): multiple callers with the same `(hash, name)` attach to one refcounted running instance with shared memory. A UI framework, physics engine, or protocol stack loads once and is reused by every app that imports it.
- **Permission broker** with OS-native UI. Prompts render as native Windows / macOS / Linux dialogs, not browser chrome.
- **Protocol + file association.** `wapi://` URLs, `.wapp` file open handler, deep links.
- **Native messaging host** — the standard WebExtension JSON-over-stdio bridge (Chromium, Firefox, Safari all support it). This is how the extension reaches the system runtime.

### 3b. The Browser Extension

A WebExtension (Chromium, Firefox, Safari) that treats a WAPI module served from a URL as a first-class content type and progressively enhances based on what is available locally. Three operating modes, chosen per page, transparent to the module:

- **Mode 1 — Pure shim.** No system runtime installed. Extension runs the module entirely in-browser using [runtime/browser/wapi_shim.js](runtime/browser/wapi_shim.js) against Web APIs (WebGPU, canvas, Web Audio, OPFS, fetch, WebTransport). Functional ABI parity for capabilities the browser supports. This is roughly what exists today — the fallback baseline.
- **Mode 2 — Native-offloaded.** System runtime installed. Extension connects via native messaging. Compute-heavy or feature-gated work delegates to the local runtime: capabilities the browser does not expose (HID, raw sockets, background services), cross-origin shared service joins, large module compile, modules already in the local cache. The module still runs *conceptually* in the tab, but expensive operations short-circuit to native code. The cache is the payoff: if the user has ever run any app — any tab, any site, any desktop app — that used `ui-framework@v2`, the bytes are already cached and the running service may already be available to join.
- **Mode 3 — Install-to-system.** User clicks "Install" on a `.wapp` served from a URL (PWA-style prompt, initiated by page or extension). Extension hands the `.wapp` to the system runtime, which registers it as a real OS app (Start Menu / Dock / Launcher), persists granted permissions, and from then on the app runs outside the browser with its own window. The origin stays linked so updates can flow, but the app is no longer tab-bound.

The module does not know which mode it is in. It imports the WAPI ABI, gets a vtable, and calls into it. The host decides whether the implementation is JS shim, native runtime via IPC, or native runtime directly. This is the whole point of the thin waist.

Current progress: opcode unification landed (2026-04-19), service-worker host drafted, module cache with SRI-keyed deduplication working. See [runtime/browser/NOTES.md](runtime/browser/NOTES.md). **Native-messaging bridge, cross-host cache sharing, and install-to-system are not yet built** — those are Stage 3 deliverables.

### 3c. Native OS / Browser Integration (endgame)

OS vendors bundle the runtime (the way Windows bundles .NET, Android bundles WebView, macOS bundles system frameworks). Browser vendors implement the ABI natively against their existing WebGPU / Canvas / WebTransport stacks, making the extension unnecessary. Chrome OS and LG webOS already prove "web runtime as OS primitive"; WAPI is that trajectory with Wasm + WebGPU instead of HTML/JS.

The same `.wasm` runs unchanged across every mode. That is the entire point.

## 4. The Shared Module + Service Instance Model

This is where "three deployment surfaces" becomes "one logical runtime."

**Content-addressed bytes.** Every module has one identity: the SHA-256 hash of its bytes. URLs are fetch hints, not identities. Two modules with the same hash *are* the same module — fetched once, compiled once, cached once.

**One cache across the whole system.** The system runtime's module cache is the single cache. Desktop apps read from it. Browser-extension Mode 2 reads from it via native messaging. An app installed via Mode 3 reads from it. A `.wasm` fetched by one site becomes available to every other app on the machine without re-downloading. This is not a per-browser cache and not a per-app cache — it is a *user-level* cache of the Wasm universe.

**Library mode vs. service mode.** Each call picks one ([include/wapi/wapi_module.h](include/wapi/wapi_module.h)):

- `wapi_module_load(hash, url, module)` — library mode. Fresh instance per call, private memory, isolated state. Like linking `libfoo.a` statically.
- `wapi_module_join(hash, url, name, module)` — service mode. Refcounted shared instance. First caller starts it; subsequent callers attach. One instance, one linear memory, many callers. Alive until the last handle releases.

**Services span host types.** When the system runtime is installed, a service is a runtime-level singleton keyed by `(hash, name)`. A desktop app and a browser tab both importing `ui-framework@v2` with name `"app.ui"` can share a single running instance — no second copy of ICU tables, font shaping data, or framework globals. Security and privacy trade-offs (same-origin vs. cross-origin, same-user vs. cross-user) are governed by capability manifests and explicit opt-in. Detail work is tracked under "Shared service instances" in [runtime/browser/NOTES.md](runtime/browser/NOTES.md).

**Progressive cache participation.** Without the system runtime: extension keeps its own per-browser IndexedDB cache (what exists today). With the system runtime installed: extension delegates cache lookups; the browser-local cache becomes a hot tier in front of the system cache. Same pattern as HTTP shared proxy caches, applied to Wasm modules.

## 5. Distribution

How does a user actually run a WAPI app? Three channels, all using the same `.wapp` binary:

- **URL** — visit `https://example.com/app.wapp`. Browser extension (or native browser) loads it. Same-origin policy applies. No install prompt for a single-use app.
- **Install to system** — click "Install" in the browser-extension UI, or open a `.wapp` file directly. Registered with the OS; first-class app with icon, Start Menu / Dock / Launcher entry. Permissions persist across runs.
- **Deep link** — `wapi://vendor.example.com/app` resolves via the system runtime; runs cached copy if fresh, otherwise fetches.

No app store required. App stores become optional discovery surfaces, not gatekeepers. Distribution is open by default, the way the web itself is.

## 6. The Package Format (`.wapp`)

A `.wapp` is a deterministic archive containing the `.wasm` binary, a `manifest.json` (capability requirements, version, icon, declared opcode namespaces), asset bundles, and an optional shared-module dependency list. No JavaScript required. No HTML required. No per-platform variants. Manifest schema is defined alongside the module linking spec.

## 7. Capability-Based Security

Every resource is a handle the host explicitly granted. A module starts with zero access — it cannot read a file, open a network connection, or draw a pixel until the host provides the relevant imports. Permission prompts are *lazy* and *per-capability*: they fire when the module first calls into a gated capability, not at startup. Capabilities enumerate (`wapi_cap_supported(io, "wapi.gpu")`), not identify (no `navigator.userAgent` equivalent). Authoritative lifecycle is in [spec/wapi-spec.md](spec/wapi-spec.md) §9.

## 8. Where We Are Today

Honest snapshot — update when the reality moves.

- **ABI spec:** v1.0.0 Draft, stable at the calling-convention level. See [spec/wapi-spec.md](spec/wapi-spec.md).
- **Desktop runtime (Windows):** triangle path and hello-game path both working — sprite rendering, audio, XInput gamepad, compression (gzip/zlib/raw), runtime module linking (library mode + shared-memory pool), IME composition + commit. Next steps tracked in [NEXT_STEPS.md](NEXT_STEPS.md) at repo root.
- **Desktop runtime (macOS / Linux):** host backends stubbed, not wired.
- **Browser runtime:** opcode unification landed, service-worker host drafted, module cache with SRI-keyed dedup working, extension not yet shipped to stores. See [runtime/browser/NOTES.md](runtime/browser/NOTES.md).
- **System-runtime installer, native-messaging bridge, install-to-system flow:** not started.
- **Language bindings:** C / C++ via the headers. Rust, Zig, and C# bindings are TBD.

## 9. Staged Roadmap

Four stages, each with a done-when criterion. No calendar dates.

### Stage 1 — Prove the ABI

Desktop runtime runs `hello_triangle` and `hello_game` from a single `.wasm` on Windows, macOS, and Linux.

*Done when:* any C / Rust / Zig developer can ship an interactive graphical app to the three desktop OSes with one binary.

### Stage 2 — Universal system runtime

Production-quality installer registering `.wapp` and `wapi://` with the OS. Permission broker with native UI. Content-addressed module cache exposed over a local IPC endpoint.

*Done when:* a non-technical user can double-click a `.wapp` and the app runs sandboxed with OS-native permission prompts.

### Stage 3 — Browser extension with deep integration

Chromium / Firefox / Safari extensions that:

- Route `.wapp` content through the local runtime via native messaging.
- Share the system module cache.
- Support `wapi_module_join` across host types (tab ↔ desktop app share one instance).
- Implement the install-to-system (Mode 3) flow.
- Gracefully fall back to pure-shim (Mode 1) when the runtime is not installed.

*Done when:* the same `.wapp` runs unmodified whether served from a URL (no runtime installed), served from a URL (runtime installed, offloading to native), or installed as a system app — and a shared service joined from a tab is the same instance joined from a desktop app.

### Stage 4 — Native adoption

Pitch browser vendors and OS vendors. Reference implementation + spec mature enough that a browser team estimating native implementation sees ~5–10k LoC of shim code, not a full runtime port.

*Done when:* at least one browser vendor commits, or at least one OS vendor bundles the runtime by default.

## 10. Non-Goals

- Not replacing the HTML / CSS document web. That is a content medium; WAPI is an application runtime. Pages and apps coexist.
- Not a general-purpose OS. WAPI runs on top of an OS or browser.
- Not a competitor to the WASI Component Model for sandboxed multi-tenant plugin composition. Different audience, different trade-offs.
- Not a new programming language. Any language that compiles to Wasm + a standard GPU shader language targets WAPI.

## 11. Open Questions

Living section. These shape the next round of design conversations.

- **Governance.** When does WAPI need a formal standards process, and which one? Governance follows adoption, but Stage 4 needs a concrete answer.
- **Shared module registry.** Self-hosted, federated, or piggyback on an existing package ecosystem?
- **Mobile story.** Technically straightforward on Android; regulatorily unclear on iOS. Watch the DMA trajectory.
- **GPU surface drift.** How closely should WAPI's GPU surface track `wasi-gfx` and `webgpu.h` as those evolve independently?
- **Linked-module memory isolation.** Fully shared, staging buffer, or per-module isolated with controlled borrowing? The spec sketches borrowing (`wapi_module_lend` / `wapi_module_reclaim`) — the full strategy is open.
- **Cross-origin shared service instances.** Exact opt-in mechanism from both the site and the module. Sketched in [runtime/browser/NOTES.md](runtime/browser/NOTES.md).
- **Native messaging protocol.** Define our own JSON schema, or adopt an existing IPC format?

---

*This document is expected to evolve. Update the status snapshot and roadmap as milestones land. Grow or shrink the open-questions list as decisions are made. Split sections into their own files only when they clearly outgrow one doc.*
