// Isolated-world bridge between the WAPI shim and the extension
// service worker.
//
// The shim runs in the page MAIN world (so it can touch WebGPU,
// canvases, etc.) and therefore has no chrome.runtime access. It
// emits cache events via window.postMessage; we listen here in the
// isolated world (which DOES have chrome.runtime) and forward
// whitelisted messages to sw.js.
//
// Message shape from the shim:
//   { source: 'wapi', type: 'modules.<verb>', ...payload }
//
// We forward any message in the `modules.*` namespace and drop the
// rest. Replies are ignored — page-side calls are fire-and-forget.

window.addEventListener('message', (ev) => {
    if (ev.source !== window) return;
    const m = ev.data;
    if (!m || m.source !== 'wapi') return;
    if (typeof m.type !== 'string' || !m.type.startsWith('modules.')) return;
    try {
        chrome.runtime.sendMessage(m, () => {
            // Swallow lastError so it doesn't surface as an
            // unchecked-runtime-error in DevTools when the SW is
            // momentarily unavailable.
            void chrome.runtime.lastError;
        });
    } catch {
        // Extension context may be invalidated during reload.
    }
});
