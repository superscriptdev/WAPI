// Runs in page MAIN world right after wapi_shim.js. The shim's
// trailing fallback block has already assigned globalThis.WAPI; we
// just announce readiness so apps can boot deterministically:
//
//     if (window.WAPI) start();
//     else addEventListener('wapi-ready', start, { once: true });
window.dispatchEvent(new Event('wapi-ready'));
