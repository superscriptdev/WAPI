// Isolated-world bridge between the WAPI shim and the extension
// service worker.
//
// The shim runs in the page MAIN world (so it can touch WebGPU,
// canvases, etc.) and therefore has no chrome.runtime access. It
// emits cache events via window.postMessage; we listen here in the
// isolated world (which DOES have chrome.runtime) and forward
// whitelisted messages to sw.js.
//
// Two message flavors, distinguished by the presence of a reqId:
//
//   Fire-and-forget (no reqId):
//     shim → { source: 'wapi', type: 'modules.<verb>', ...payload }
//     bridge forwards, ignores reply. Used by modules.store.
//
//   Request/reply (reqId present):
//     shim → { source: 'wapi', type: 'modules.<verb>', reqId, ...payload }
//     bridge forwards, awaits SW reply, posts
//     { source: 'wapi-reply', reqId, result | error } back into the
//     page via window.postMessage so the shim's Promise resolves.
//     Used by modules.fetch (bytes round-trip).
//
// We forward any message in the `modules.*` or `services.*` namespace
// and drop the rest.

window.addEventListener('message', (ev) => {
    if (ev.source !== window) return;
    const m = ev.data;
    if (!m || m.source !== 'wapi') return;
    if (typeof m.type !== 'string') return;
    if (!m.type.startsWith('modules.') && !m.type.startsWith('services.')) return;

    const reqId = m.reqId;
    try {
        chrome.runtime.sendMessage(m, (resp) => {
            const err = chrome.runtime.lastError;
            if (reqId == null) {
                // Fire-and-forget: swallow lastError so it doesn't
                // surface as an unchecked-runtime-error.
                void err;
                return;
            }
            if (err) {
                window.postMessage({
                    source: 'wapi-reply',
                    reqId,
                    error: err.message || String(err),
                }, '*');
                return;
            }
            if (resp && resp.error) {
                window.postMessage({
                    source: 'wapi-reply',
                    reqId,
                    error: resp.error,
                }, '*');
                return;
            }
            window.postMessage({
                source: 'wapi-reply',
                reqId,
                result: resp,
            }, '*');
        });
    } catch (e) {
        // Extension context may be invalidated mid-reload.
        if (reqId != null) {
            window.postMessage({
                source: 'wapi-reply',
                reqId,
                error: String(e && e.message || e),
            }, '*');
        }
    }
});
