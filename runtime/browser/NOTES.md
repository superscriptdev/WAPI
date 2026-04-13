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
| `sw.js` | Background service worker. Owns the IndexedDB module-cache metadata and exposes a message API. |
| `popup.html` + `popup.js` | Toolbar popup: module cache viewer with hit/miss table. |

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
2. App calls `new WAPI().load(url)`.
3. Shim fetches the wasm bytes, compiles, and instantiates. Streaming compile
   was intentionally dropped: we always materialize bytes so we can hash them.
   The streaming-vs-buffered cost is dominated by compile + `_start`, so it's
   in the noise.
4. Shim computes sha256 of the bytes and posts:
   `window.postMessage({source:'wapi', type:'modules.touch', hash, url, size}, '*')`.
5. `bridge.js` picks it up and `chrome.runtime.sendMessage`s it to the SW.
6. `sw.js` `touch()` handler:
   - If the hash is **new** in IDB: register with `misses=1`, `addedAt=now`.
   - If the hash **already exists**: increment `hits`, update `lastUsedAt`.
7. Popup, when opened, queries the SW (`modules.list` + `modules.stats`) and
   renders the table.

### Hit/miss semantics today

The current "have I seen this hash before in IDB" check is a **proxy** for
"would a real cache have served this." It's honest only because we don't yet
store the wasm bytes themselves. Once a real bytes-cache lands, the shim
should switch to:

- Ask SW for bytes by hash → if present, use them and call `modules.hit`
- If absent, fetch network, store bytes, call `modules.miss`

The split `bumpHit` / `bumpMiss` handlers in `sw.js` are already there waiting
for that wiring; only the `touch()` shortcut needs to be deprecated.

## SW message API (current)

All messages have shape `{type: 'modules.<verb>', ...payload}` and are sent
via `chrome.runtime.sendMessage`.

| Type | Payload | Returns |
|---|---|---|
| `modules.list` | — | `Array<Record>` |
| `modules.stats` | — | `{count, totalSize, totalHits, totalMisses, hitrate}` |
| `modules.get` | `{hash}` | `Record \| null` |
| `modules.register` | `{hash, url, size}` | `Record` (resets counters if new) |
| `modules.touch` | `{hash, url, size}` | `Record` (first sighting = miss, later = hit) |
| `modules.hit` | `{hash}` | `Record \| null` |
| `modules.miss` | `{hash, url, size}` | `Record` |
| `modules.clear` | — | `undefined` |

`Record` shape: `{hash, url, size, addedAt, lastUsedAt, hits, misses}`.
IDB: database `wapi-cache`, store `modules`, keyPath `hash`.

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

1. **Real bytes cache.** Add a `bytes` IDB store (or use the Cache API)
   keyed by hash. SW gains `modules.fetch(hash) → ArrayBuffer | null` and
   `modules.store(hash, bytes)`. Shim's `load()` checks SW first, falls back
   to network on miss. This is what makes the extension actually valuable
   across sites — same wasm, downloaded once, reused everywhere.

2. **Wire `wapi_module`.** The stub at `wapi_shim.js:4002` still returns
   `WAPI_ERR_NOTSUP` for every method. Wire `load` / `is_cached` / `prefetch`
   against the SW cache. Note that wasm imports are sync but cache lookups
   are async, so this needs an explicit prefetch model: app calls
   `prefetch(hash, url)` async, awaits readiness via a signal/poll, then
   `load(hash)` is sync against the warm cache.

3. **Popup auto-refresh.** `setInterval(refresh, 1000)` while
   `document.visibilityState === 'visible'`.

4. **Origin filter in bridge.** Only forward messages from frames whose
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
