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

function renderSvcStats(s) {
    document.getElementById('svc-stats').innerHTML =
        `<span class="k">services:</span> <span class="v">${s.count}</span>   ` +
        `<span class="k">users:</span> <span class="v">${s.totalUsers}</span>   ` +
        `<span class="k">pending ops:</span> <span class="v">${s.totalPending}</span>`;
}

function renderServices(list) {
    const content = document.getElementById('svc-content');
    if (!list.length) {
        content.innerHTML =
            '<div class="empty">No live services.<br><br>' +
            'Services appear here once a WAPI app calls ' +
            '<code>wapi_module.join()</code>.</div>';
        return;
    }

    list.sort((a, b) => (b.startedAt || 0) - (a.startedAt || 0));

    const rows = list.map((s) => {
        const host = s.hostedIn === 'page' ? (s.origin || originOf(s.url) || 'page') : 'sw';
        return `<tr>
            <td class="hash" title="${escapeHtml(s.hashHex)}">${escapeHtml(fmtHash(s.hashHex))}</td>
            <td title="${escapeHtml(s.name || '')}">${escapeHtml(s.name || '-')}</td>
            <td title="${escapeHtml(s.url || '')}">${escapeHtml(originOf(s.url))}</td>
            <td title="${escapeHtml(host)}">${escapeHtml(host)}</td>
            <td class="num">${s.users || 0}</td>
            <td class="num">${s.pending || 0}</td>
            <td class="num" title="${new Date(s.startedAt || 0).toISOString()}">${fmtAgo(s.startedAt)}</td>
        </tr>`;
    }).join('');

    content.innerHTML = `
        <table>
            <colgroup>
                <col class="c-hash"><col class="c-name"><col class="c-origin">
                <col class="c-host"><col class="c-users"><col class="c-pend"><col class="c-up">
            </colgroup>
            <thead><tr>
                <th>hash</th><th>name</th><th>origin</th><th>host</th>
                <th class="num">users</th><th class="num">pend</th><th class="num">up</th>
            </tr></thead>
            <tbody>${rows}</tbody>
        </table>`;
}

async function refresh() {
    try {
        const [stats, list, svcStats, svcList] = await Promise.all([
            send({ type: 'modules.stats' }),
            send({ type: 'modules.list' }),
            send({ type: 'services.stats' }),
            send({ type: 'services.list' }),
        ]);
        renderStats(stats);
        renderList(list);
        renderSvcStats(svcStats);
        renderServices(svcList);
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

// 1 Hz auto-refresh while the popup is visible. Paused under
// visibilitychange so a popup left open in the background doesn't
// keep waking the service worker.
let pollTimer = 0;
function startPolling() {
    if (pollTimer) return;
    pollTimer = setInterval(refresh, 1000);
}
function stopPolling() {
    if (!pollTimer) return;
    clearInterval(pollTimer);
    pollTimer = 0;
}
document.addEventListener('visibilitychange', () => {
    if (document.visibilityState === 'visible') startPolling();
    else stopPolling();
});

refresh();
if (document.visibilityState === 'visible') startPolling();
