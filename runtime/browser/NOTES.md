# WAPI Browser Extension — Status & Resume Notes

This directory now serves dual duty: it's the dev web runtime (`serve.js`,
`index.html`) **and** the unpacked Chrome MV3 extension root. The extension's
job is to act as a global browser shim so WAPI apps don't have to bundle
`wapi_shim.js` themselves. Long-term goal: WAPI becomes a real browser
standard and this extension goes away.

## Files in this directory

| File | Role |
|---|---|
| `wapi_shim.js` | Canonical shim. Classic-script-safe — the `export { WAPI };` line was removed; the file self-registers via `globalThis.WAPI = WAPI` at the bottom. **Single source of truth, no copies, no symlinks.** |
| `index.html` | Dev runtime page. Loads the shim via `<script src="./wapi_shim.js">` (classic), then reads `window.WAPI` inside the module block. |
| `serve.js` | Dev HTTP server (unchanged). |
| `manifest.json` | MV3 manifest. Two content_scripts entries (see below). |
| `ready.js` | Fires a `wapi-ready` event after the shim loads, so page scripts can wait deterministically. |
| `bridge.js` | Isolated-world content script. Listens for `window.postMessage({source:'wapi', type:'modules.*', ...})` from the shim and forwards to the service worker via `chrome.runtime.sendMessage`. |
| `sw.js` | Background service worker. Owns the IndexedDB module-cache metadata and exposes a message API. Will also own the services refcount map and lifecycle once the services manager lands (in progress). |
| `sw_host.js` | Headless WAPI host for the SW. Parallel implementation of the core WAPI ABI (wire contracts only — not a refactor of `wapi_shim.js`). Loaded via `importScripts('./sw_host.js')`. Exposes `self.createSwHost(opts)` that builds an imports object for instantiating service modules inside the SW. See "Service-worker host" below. |
| `popup.html` + `popup.js` | Toolbar popup: module cache viewer with hit/miss table. Will grow a live-services table once services land. |

## How the injection works

`manifest.json` has two `content_scripts` entries, both at `document_start`,
both `<all_urls>`, both `all_frames: true`:

1. **Main-world entry**: loads `wapi_shim.js` + `ready.js` with `world: "MAIN"`.
   This puts `WAPI` on the page's real `window` so apps can use it like a
   built-in. Main-world content scripts must be classic scripts, which is
   why the shim cannot use `export` syntax.

2. **Isolated-world entry**: loads `bridge.js` (default world). The isolated
   world is the only place we have `chrome.runtime` access, so it owns the
   page → service-worker forwarding.

App-side usage pattern:

```js
if (window.WAPI) start();
else addEventListener('wapi-ready', start, { once: true });
```

## Module cache producer pipeline

End-to-end:

1. Page loads. Extension injects shim (main world) and bridge (isolated world).
2. App calls `new WAPI().load(url, { hash? })` — `hash` is optional SRI.
3. Shim chooses path:
   - **SRI mode** (hash supplied): ask SW for bytes via `modules.fetch(hash)`.
     On hit, skip the network entirely. On miss, fetch the URL and verify the
     hash of the fetched bytes matches the declared hash before compile.
   - **Legacy mode** (no hash): always fetch, hash after, store.
4. Compile + instantiate.
5. On a cache miss the shim calls `modules.store(hash, url, bytes)`
   fire-and-forget; the SW writes bytes into IDB and bumps the miss counter.
   On a hit, the SW already bumped its hit counter inside `cacheFetch`.
6. Popup queries `modules.list` + `modules.stats` on a 1 Hz timer while
   visible and renders the table.

Bytes cross the `chrome.runtime.sendMessage` boundary as base64 (that channel
uses JSON, not structured clone). IDB stores raw `Uint8Array`. The extra
encode/decode is only at the extension boundary, not in the hot path.

## Service-worker host (`sw_host.js`)

Services are shared, long-lived wasm instances that multiple tabs can join
instead of re-instantiating. They need to live somewhere persistent and
shared across tabs — the natural home is the extension service worker, which
already owns the bytes cache and is the one context where a single wasm
instance can outlive any individual page.

`sw_host.js` is the headless host that instantiates service modules inside
the SW. It is a **parallel implementation** of the core WAPI ABI — not a
refactor of `wapi_shim.js`. The shim is ~6500 lines of class state tightly
coupled to `window`/`document`/`Canvas`/`WebGPU`; dual-moding it would mean
gating every call site. Keeping them separate costs some duplication but
isolates the SW path from the main-thread path entirely.

### Design principle

**The WAPI ABI is designed for a first-class browser API future. The
extension is a reference implementation where hacks are acceptable.** That
means:

- The ABI exposes the full capability surface. A native-browser WAPI would
  host GPU services properly; the extension's `sw_host.js` doesn't, because
  service workers can't touch WebGPU today. That's a reference-impl gap, not
  an ABI downgrade.
- A module that imports a capability `sw_host.js` doesn't provide will fail
  to link as a service. That's the correct "this isn't a headless service"
  signal — preferable to a STUB that links and then crashes at first use.
- Shared-worker or cross-origin hosting strategies are *implementation*
  decisions the extension can revisit. They do not need to bend the ABI.

### v1 capability surface

Wire-contract parity with the shim for the following import modules:

| Module | Status | Notes |
|---|---|---|
| `wapi` | ✅ real | capability/abi query, panic, io_get, allocator_get. Indirect-vtable path builds the same 8-entry io table and 3-entry allocator table as the shim, using a local `makeWasmFunc` (tiny wasm wrapper module compiled on the fly, no DOM). |
| `wapi_env` | ✅ real | empty args/env (services start clean), `random` via `crypto.getRandomValues`, `exit` sets `_exitCode` and fires `onExit` — cannot actually halt wasm execution from an import, so sw.js is expected to watch `exitCode()` and tear the instance down. |
| `wapi_memory` | ✅ real | bump + free-list host allocator, same shape as shim (`_allocBase` from `__heap_base` or 1 MB default). Free is effectively a no-op since bump can't reclaim. |
| `wapi_io` | ✅ real (partial ops) | opcode dispatch. Supported ops: `NOP`, `LOG` (routes to SW console via `opts.onLog`), `TIMEOUT`, `HTTP_FETCH` (global `fetch`), `CONNECT`/`SEND`/`RECV` (WebSocket), `COMPRESS_PROCESS` (CompressionStream). All others → `WAPI_ERR_NOTSUP`. `wait()` does not block — just peeks — because the SW event loop can't be stalled. |
| `wapi_clock` | ✅ real | `time_get` via `performance.now()`/`Date.now()`, `perf_counter` relative to a host-captured origin, `yield`/`sleep` are no-ops. |
| `wapi_module` | ⚠️ stub | `load`/`prefetch`/`is_cached`/`release`/`get_hash`/`join` all return `NOTSUP` for v1. Nested module loading inside a service needs the services manager (in `sw.js`) to supply bytes lookup + refcount routing. Deferred until there's a concrete service that loads another module. |

### v1.1 deferrals (modules that import these will fail to link)

Intentionally NOT exposed in v1 so that a misclassified "service" fails
loudly instead of half-running:

- `wapi_crypto` — already NOTSUP in the shim because `crypto.subtle` is
  async-only and the WAPI C ABI is sync. Solvable via `wapi_io` opcodes
  (submit hash/encrypt as an io op, completion lands in the queue), but
  requires picking opcodes and extending the op dispatch table.
- `wapi_thread` sync primitives (mutex, rwlock, semaphore, cond, barrier)
  — host-managed handles. Simple JS impls, but parked until a service
  needs them. Thread creation itself lands via `Worker` + SharedArrayBuffer.
- `wapi_filesystem` — each service should get a sandboxed OPFS root keyed
  by `(hash, name)`. Needs the per-service scope decision codified first.
- `wapi_kvstorage` — same story as filesystem: per-service IDB namespace.
- `wapi_sysinfo`, `wapi_notifications`, `wapi_codec`, `wapi_mediacaps`,
  `wapi_networkinfo`, and every UI/hardware/user-gesture capability —
  either trivially stubbable or intentionally unavailable in a headless
  host. Wire on demand.

### SKIPped outright (don't make sense in a SW)

Everything that touches the DOM, GPU, audio output, user gestures,
hardware devices, or per-window state: `wapi_window`, `wapi_surface`,
`wapi_display`, `wapi_input`, `wapi_text`, `wapi_font`, `wapi_audio`,
`wapi_audioplugin`, `wapi_gpu`, `wapi_video`, `wapi_camera`, `wapi_midi`,
`wapi_usb`, `wapi_serial`, `wapi_bluetooth`, `wapi_nfc`, `wapi_hid`,
`wapi_sensors`, `wapi_geolocation`, `wapi_orientation`, `wapi_power`,
`wapi_haptics`, `wapi_xr`, `wapi_authn`,
`wapi_biometric`, `wapi_payments`, `wapi_contacts`, `wapi_clipboard`,
`wapi_dialog`, `wapi_menu`, `wapi_tray`, `wapi_taskbar`, `wapi_share`,
`wapi_dnd`, `wapi_eyedropper`, `wapi_screencapture`, `wapi_speech`,
`wapi_barcode`, `wapi_filewatcher`, `wapi_mediasession`, `wapi_theme`,
`wapi_thread` (wasm threads + SW is a cross-origin-isolation minefield;
revisit when a service asks for it), `wapi_process`, `wapi_register`.

### Shim vs sw_host: what is shared

Nothing at the code level. What is shared is the **wire format**: struct
layouts (`wapi_io_op_t` 80 bytes, `wapi_io_event_t` 128 bytes, allocator
vtable 16 bytes, io vtable 36 bytes), error constants, event type
numbers, opcode numbers. Both files derive these from the same C headers
under `include/wapi/`. If a header changes, both must be updated — but
that's a property of the ABI surface, not of the runtime implementation.

## Runtime module linking (wapi_module)

Content-addressed at the JS layer by a page-local `_wapiModuleCache` map.
Wasm imports are sync but cache lookups are async, so the ABI is an explicit
prefetch-then-load dance:

1. App calls `wapi_module_prefetch(&hash, url)` — non-blocking. The shim
   kicks off `SW.fetch → (network fallback) → verify → compile` and marks
   the entry `pending`. Returns `WAPI_OK` immediately.
2. App polls `wapi_module_is_cached(&hash)` (returns 1 once `ready`).
3. App calls `wapi_module_load(&hash, url, &module)` — sync. Instantiates
   the cached `WebAssembly.Module` against the current WAPI instance's
   imports, returns an `i32` handle. `WAPI_ERR_NOENT` if not yet ready.

Scope today: `load`, `prefetch`, `is_cached`, `release`, `get_hash`. Other
methods (`get_func`, `call`, `shared_*`, `lend`/`reclaim`, `copy_in`) remain
`WAPI_ERR_NOTSUP` — runtime linking of separate wasm modules isn't wired yet
and needs the borrow system spec'd out first.

## SW message API

| Type | Payload | Returns |
|---|---|---|
| `modules.list` | — | `Array<MetaRecord>` |
| `modules.stats` | — | `{count, totalSize, totalHits, totalMisses, hitrate}` |
| `modules.get` | `{hash}` | `MetaRecord \| null` |
| `modules.fetch` | `{hash}` | `{bytesB64, size} \| null` (bumps hits on hit) |
| `modules.store` | `{hash, url, bytesB64}` | `MetaRecord` (bumps misses, stores bytes) |
| `modules.clear` | — | `undefined` |

`MetaRecord` shape: `{hash, url, size, addedAt, lastUsedAt, hits, misses}`.
IDB: database `wapi-cache` (v2), two stores — `modules` (metadata, keyPath
`hash`) and `bytes` (`{hash, bytes: Uint8Array}`, keyPath `hash`). The split
lets the popup list metadata cheaply without dragging megabytes of wasm into
memory.

## How to run / test

1. **Load the extension**: `chrome://extensions` → enable Developer mode →
   Load unpacked → pick `runtime/browser/`. After any change to
   `manifest.json` or `sw.js`, click the reload button on the extension card.
2. **Run the dev server**: from `runtime/browser/`, `node serve.js`.
3. Visit `http://localhost:8080?app=hello_triangle.wasm` (or `audiodemo.wasm`).
4. Click the extension toolbar icon. You should see one module entry with
   `miss=1`.
5. Refresh the page. The same hash now shows `hits=1, miss=1, rate=50%`.
6. Refresh again: `hits=2, rate=66%`. Hash, origin, size, and last-used
   should all update.

To wipe state: click **Clear cache** in the popup (confirms first).

## Known caveats / things to clean up

- **Always-on hashing**. The shim hashes bytes on every load even when the
  extension isn't installed. About 10 ms on a 1 MB wasm. If this becomes
  noticeable, the bridge could set a `window.__wapiExtensionPresent = true`
  flag on install and the shim could gate on that.
- **No bridge sender filter**. Any script on the page can post a fake
  `{source:'wapi', type:'modules.touch'}` event and pollute the view.
  Acceptable for dev; tighten before publishing.
- **`WebAssembly.Module` source path skipped**. If a caller pre-compiles
  before calling `load()`, we have no bytes to hash. Rare.
- **Popup doesn't auto-refresh**. Have to click Refresh after triggering a
  load while the popup is open. A 1 Hz interval would fix it.
- **`web_accessible_resources` was removed** from the manifest because the
  shim is injected directly via content_scripts now. If we ever go back to
  per-page dynamic import, that needs to come back.
- **Hardlink experiment was reverted.** The shim used to live in two places
  with a hardlink between them; that broke under git checkout. Co-locating
  the extension files in `runtime/browser/` next to the canonical shim is
  the current single-file solution.

## Next steps when resuming

1. ✅ **Real bytes cache.** IDB `bytes` store, `modules.fetch`/`modules.store`
   SW handlers, shim checks SW first on URL loads, hash verified on insert
   (SRI when `config.hash` supplied). Done.

2. ✅ **Wire `wapi_module`.** `load` / `is_cached` / `prefetch` / `release` /
   `get_hash` now go through a page-local content-addressed
   `WebAssembly.Module` cache. Prefetch is async, load is sync against the
   warm cache. Other methods (`get_func`, `call`, `shared_*`, `lend` /
   `reclaim`, `copy_in`) remain NOTSUP — they need the borrow system and
   cross-module calling specced out first.

3. ✅ **Popup auto-refresh.** 1 Hz `setInterval` gated on
   `document.visibilityState`. Pauses when the popup is hidden.

4. **Shared service instances** — ⚙️ in progress.
   **Status**: `sw_host.js` landed with v1 capability surface (see
   "Service-worker host" above). Next: wire `services.join/release/list/stats`
   in `sw.js` with a refcount map keyed by `(hash, name)`, extend `bridge.js`
   allowlist to `services.*`, wire `wapi_module_join` in `wapi_shim.js` to
   forward through the bridge, and grow the popup table.
   **Open questions still unresolved** — these shaped the v1 scope and are
   the next design conversations:
   Rationale: the bytes cache covers "don't re-download, don't re-compile,"
   but every `load()` still instantiates a fresh linear memory and
   re-runs `_initialize`. Two costs repeat forever per-page, per-site:

   - **Startup time**: _initialize, constructor globals, any table
     construction the module does on first use.
   - **Memory footprint** (the bigger lever): every instance holds a
     private copy of every data table the module needs. For anything
     table-heavy this is a *lot*:
     - **ICU / Unicode tables**: collation, case mapping, normalization,
       bidi, line break, word break, segmentation — tens of MB per
       instance, all read-only.
     - **Font shaping**: HarfBuzz-style GPOS/GSUB tables, script
       databases, OpenType feature tables.
     - **Emoji**: the UCD emoji tables + skin-tone + ZWJ sequence data.
     - **IDN / punycode**: Unicode tables again, plus IDN mapping.
     - **Timezone**: tzdata is ~1 MB of ruleset per instance.
     - **ML weights / codecs / grammars**: already mentioned — hundreds
       of MB to multiple GB.

     With shared instances, N callers pay 1× the memory instead of N×.
     The RAM savings alone justify this.

   - **Shared function tables across libraries**: compound benefit. Many
     libraries re-vendor the same primitives (libc, math, string, sort,
     hash) because today every wasm binary bundles its own. A shared
     `libc` service joined by every other service means *one* libc in
     memory, one code pool, one set of lookup tables — every consumer
     effectively shares the implementation. The savings compound because
     services can join services: a Unicode service joins libc, a text-
     shaping service joins Unicode, an editor joins text-shaping, and
     everything upstream is paid for once at the bottom of the DAG.

   If React / Wordpress / an LLM kernel / an ICU-equivalent were WAPI
   modules, today's bytes cache still pays full instantiation + table
   construction + linear memory on every site. Service mode pays it
   once, forever, for the whole browser.

   Two deployment modes from the same binary:

   - **Library mode** (today): `wapi_module_load(&hash, url, &module)`.
     Fresh instance per caller, isolated state. Equivalent to linking
     `libfoo.a`. What `wapi_module_load` does right now.

   - **Service mode** (new): `wapi_module_join(&hash, url, name, &module)`.
     Refcounted shared instance. First caller starts it; subsequent callers
     attach. Alive until last handle releases. Equivalent to talking to a
     running daemon. `name` lets independent apps agree on which instance
     to join without coordinating hashes.

   Same binary, caller picks deployment. The module binary doesn't decide.

   Popup gains a "live services" section alongside the bytes-cache table:
   hash, name, user count, memory, uptime.

   Open questions (answer before building):

   - **Scope of "shared"**: per-tab? per-origin? per-browser? Cross-origin
     instance sharing is where the big wins live (every site joins one
     React service), but it's a real security boundary — shared instance
     state means shared memory across origins, which is exactly what
     `cookieStore` / process isolation were designed to prevent. The
     bytes cache is safe to share across origins because bytes are
     immutable and content-addressed; a running instance is not.
     Probably: per-origin by default, cross-origin opt-in with
     a capability declaration the site and the module both sign off on.

   - **Cycles**: service A joins service B which joins service A → refcounts
     never hit zero. Lean toward detecting at join time and returning
     `WAPI_ERR_LOOP`. Cycles in service graphs are almost always a bug.

   - **Lifetime heuristic**: user suggested a startup-cost-vs-memory-cost
     tradeoff — keep hot if expensive to restart, evict if cheap. Could
     layer on top of refcount: once refcount hits zero, decide whether to
     kill immediately or park for N seconds in case someone re-joins.

   - **Naming**: global strings collide. Probably `(hash, name)` pair so
     two different binaries can use the same name without stomping.

   - **Destruction**: how does a service learn it's about to be torn down?
     Optional `wapi_module_on_shutdown` export? Drop on the floor?

   Defer until there's a first concrete service to design against.
   Premature API design here is the expensive kind of mistake.

5. **Central CVE registry / runtime vulnerability deny-list.** Because
   modules are content-addressed, the hash IS the identity — there's no
   "version string" to lie about, no alternate distribution channel to
   sneak past. That makes runtime-level CVE enforcement actually tractable
   in a way it never was for npm/PyPI/crates.io.

   Model:
   - A central registry (wapi.dev or similar) publishes a signed list of
     `{hash, severity, cve_id, description, fixed_in_hash?}` records.
     Distribution is out-of-band — GitHub repo, signed JSON, whatever.
   - The extension SW periodically pulls the list and caches it in IDB.
   - On `modules.fetch` / `modules.store` / `services.join`, the SW checks
     the hash against the deny-list. Blocked hashes fail the load with
     `WAPI_ERR_BLOCKED` (new error code) and surface in the popup with
     the CVE ID.
   - User can allowlist specific hashes manually (escape hatch for a
     module they accept the risk of).

   Why this beats npm/crates-style advisories:
   - **No identity trust**: hash = identity. A malicious update can't
     re-use the blocked hash.
   - **No cooperation needed**: doesn't matter if the author acknowledges
     the CVE or cuts a patch release. The runtime refuses the bytes.
   - **Air-gapped enforcement**: the deny-list is pushed to the browser,
     not queried at runtime. Works offline, no per-load lookup cost.
   - **Aligns with user's threat model**: supply chain is the real risk,
     and this is the only mechanism that actually addresses it at runtime.
     Capability-based sandboxing (Rust component model) doesn't stop a
     malicious dependency from doing damage *within* its granted caps;
     a CVE deny-list does.

   Out of scope for v1 (cache + shared instances). Sketched here so the
   SW message API and popup layout leave room for a "blocked" state
   on each record.

6. **Origin filter in bridge.** Only forward messages from frames whose
   top-level origin matches a list, or that include a shared secret the
   shim sets via a property the isolated world can read.

5. **Firefox manifest.** Add `browser_specific_settings.gecko` once Chrome
   flow is stable. Firefox 128+ supports `world: "MAIN"` so the same
   manifest mostly works.

6. **Per-row evict in popup.** Right now Clear nukes everything. A small
   per-row trash button + a `modules.delete(hash)` SW handler is a quick win.

7. **Long-term**: push WAPI as a real browser standard so this extension
   becomes obsolete. The whole point of doing it this way is that the
   browser eventually exposes `window.WAPI` natively and apps don't need
   any shim or extension.

8. **Per-origin opt-in via response header** (deferred until standardization
   conversations start). Today the shim injects on `<all_urls>` like every
   other extension and that's fine. When WAPI is being shopped to browser
   vendors, the right opt-in mechanism is a response header (`Wapi-Enabled: 1`
   or whatever the standard picks) — same shape as COOP/COEP, Permissions-
   Policy, Origin Trials. Don't build it now: we'd rebuild against the real
   header name later, and there's no untrusted-site problem to solve at
   one-user dev scale.
