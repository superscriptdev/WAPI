// Popup UI for the WAPI extension. Lists every wasm module the
// service worker has seen, with hit/miss counters and a global
// hitrate. Talks to sw.js via chrome.runtime.sendMessage.

function send(msg) {
    return new Promise((resolve, reject) => {
        chrome.runtime.sendMessage(msg, (resp) => {
            const e = chrome.runtime.lastError;
            if (e) return reject(new Error(e.message));
            if (resp && resp.error) return reject(new Error(resp.error));
            resolve(resp);
        });
    });
}

function fmtBytes(n) {
    if (!n) return '0 B';
    if (n < 1024) return n + ' B';
    if (n < 1024 * 1024) return (n / 1024).toFixed(1) + ' KB';
    return (n / 1048576).toFixed(2) + ' MB';
}

function fmtAgo(ts) {
    if (!ts) return '-';
    const dt = (Date.now() - ts) / 1000;
    if (dt < 1)     return 'now';
    if (dt < 60)    return Math.floor(dt) + 's';
    if (dt < 3600)  return Math.floor(dt / 60) + 'm';
    if (dt < 86400) return Math.floor(dt / 3600) + 'h';
    return Math.floor(dt / 86400) + 'd';
}

function fmtHash(h) {
    if (!h) return '-';
    return h.length > 16 ? h.slice(0, 10) + '…' + h.slice(-4) : h;
}

function originOf(url) {
    if (!url) return '-';
    try { return new URL(url).host; } catch { return url; }
}

function escapeHtml(s) {
    return String(s).replace(/[&<>"']/g, (c) => ({
        '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;',
    }[c]));
}

function renderStats(s) {
    const pct = (s.hitrate * 100).toFixed(1);
    document.getElementById('stats').innerHTML =
        `<span class="k">modules:</span> <span class="v">${s.count}</span>   ` +
        `<span class="k">size:</span> <span class="v">${fmtBytes(s.totalSize)}</span>   ` +
        `<span class="k">hits:</span> <span class="v">${s.totalHits}</span>   ` +
        `<span class="k">miss:</span> <span class="v">${s.totalMisses}</span>   ` +
        `<span class="k">hitrate:</span> <span class="v">${pct}%</span>`;
}

function renderList(list) {
    const content = document.getElementById('content');
    if (!list.length) {
        content.innerHTML =
            '<div class="empty">No modules cached yet.<br><br>' +
            'Modules appear here once a WAPI app loads wasm via ' +
            '<code>wapi_module.load()</code>.</div>';
        return;
    }

    list.sort((a, b) => (b.lastUsedAt || 0) - (a.lastUsedAt || 0));

    const rows = list.map((m) => {
        const total = (m.hits || 0) + (m.misses || 0);
        const rate = total ? Math.round((m.hits / total) * 100) + '%' : '-';
        return `<tr>
            <td class="hash" title="${escapeHtml(m.hash)}">${escapeHtml(fmtHash(m.hash))}</td>
            <td title="${escapeHtml(m.url || '')}">${escapeHtml(originOf(m.url))}</td>
            <td class="num">${fmtBytes(m.size)}</td>
            <td class="hit">${m.hits || 0}</td>
            <td class="miss">${m.misses || 0}</td>
            <td class="num">${rate}</td>
            <td class="num" title="${new Date(m.lastUsedAt || m.addedAt || 0).toISOString()}">${fmtAgo(m.lastUsedAt || m.addedAt)}</td>
        </tr>`;
    }).join('');

    content.innerHTML = `
        <table>
            <colgroup>
                <col class="c-hash"><col class="c-origin"><col class="c-size">
                <col class="c-hits"><col class="c-miss"><col class="c-rate"><col class="c-used">
            </colgroup>
            <thead><tr>
                <th>hash</th><th>origin</th><th>size</th>
                <th class="num">hits</th><th class="num">miss</th>
                <th class="num">rate</th><th class="num">used</th>
            </tr></thead>
            <tbody>${rows}</tbody>
        </table>`;
}

async function refresh() {
    try {
        const [stats, list] = await Promise.all([
            send({ type: 'modules.stats' }),
            send({ type: 'modules.list' }),
        ]);
        renderStats(stats);
        renderList(list);
    } catch (e) {
        document.getElementById('content').innerHTML =
            `<div class="err">Error: ${escapeHtml(e.message)}</div>`;
    }
}

document.getElementById('refresh').addEventListener('click', refresh);
document.getElementById('clear').addEventListener('click', async () => {
    if (!confirm('Wipe all cached module records?')) return;
    try {
        await send({ type: 'modules.clear' });
        await refresh();
    } catch (e) {
        alert('Clear failed: ' + e.message);
    }
});

refresh();
