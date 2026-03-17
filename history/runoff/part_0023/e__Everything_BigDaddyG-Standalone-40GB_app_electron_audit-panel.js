// BigDaddyG IDE - Audit Panel
// In-IDE auditor to check frontend wiring, backend reachability, and module errors.

(function () {
  const AUDIT_TIMEOUT = 4000;
  const ENDPOINTS = {
    models: 'http://localhost:11441/v1/models',
    chat: 'http://localhost:11441/v1/chat/completions',
    tags: 'http://localhost:11441/api/tags'
  };

  function withTimeout(promise, ms, label) {
    return new Promise((resolve) => {
      const timer = setTimeout(() => resolve({ ok: false, detail: `${label} timed out` }), ms);
      promise.then((res) => { clearTimeout(timer); resolve(res); }).catch((err) => { clearTimeout(timer); resolve({ ok: false, detail: `${label} error: ${err.message || err}` }); });
    });
  }

  async function probeFetch(url, body) {
    try {
      const res = await fetch(url, body ? { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) } : undefined);
      if (!res.ok) return { ok: false, detail: `HTTP ${res.status}` };
      const json = await res.json().catch(() => null);
      return { ok: true, detail: json ? (json.data ? `items: ${json.data.length}` : 'ok') : 'ok', data: json };
    } catch (e) {
      return { ok: false, detail: e.message || String(e) };
    }
  }

  function probeDom() {
    const sendBtn = document.getElementById('ai-send-btn');
    const input = document.getElementById('ai-input');
    const messages = document.getElementById('ai-chat-messages');
    return {
      sendButtonPresent: !!sendBtn,
      inputPresent: !!input,
      messagesPresent: !!messages,
      inputVisible: !!(input && input.offsetParent !== null),
      messagesScrollable: !!(messages && messages.style && messages.style.overflowY === 'auto')
    };
  }

  function probeMonaco() {
    const hasRequire = typeof require !== 'undefined' && typeof require.config === 'function';
    const hasMonaco = typeof monaco !== 'undefined' && monaco.editor;
    return { hasRequire, hasMonaco, base: window.monacoBasePath || 'unknown' };
  }

  function probeLayout() {
    const terminal = document.getElementById('terminal-panel');
    const right = document.getElementById('right-sidebar');
    let overlapping = false;
    if (terminal && right) {
      const tRect = terminal.getBoundingClientRect();
      const rRect = right.getBoundingClientRect();
      overlapping = !(tRect.right <= rRect.left || tRect.left >= rRect.right || tRect.bottom <= rRect.top || tRect.top >= rRect.bottom);
    }
    return { overlappingTerminal: overlapping };
  }

  function probeErrors() {
    return (window.__recentConsoleErrors || []).slice(-20);
  }

  async function runAudit() {
    const results = { frontend: {}, backend: {}, layout: {}, errors: [] };

    // Frontend probes
    results.frontend.dom = probeDom();
    results.frontend.monaco = probeMonaco();

    // Layout overlap check
    results.layout = probeLayout();

    // Backend probes with timeouts
    results.backend.models = await withTimeout(probeFetch(ENDPOINTS.models), AUDIT_TIMEOUT, 'models');
    results.backend.tags = await withTimeout(probeFetch(ENDPOINTS.tags), AUDIT_TIMEOUT, 'tags');
    results.backend.chat = await withTimeout(probeFetch(ENDPOINTS.chat, { model: 'bigdaddyg-latest', messages: [{ role: 'user', content: 'ping' }] }), AUDIT_TIMEOUT, 'chat');

    // Collect recent errors
    results.errors = probeErrors();

    return results;
  }

  function renderResults(results) {
    const container = document.getElementById('audit-body');
    if (!container) return;
    const fmt = (ok) => ok ? '✅' : '⚠️';
    const item = (label, ok, detail) => `<div style="display:flex;justify-content:space-between;gap:8px;"><span>${fmt(ok)} ${label}</span><span style="color:${ok ? '#0f0' : '#f66'};font-size:11px;">${detail || ''}</span></div>`;

    const dom = results.frontend.dom;
    const monaco = results.frontend.monaco;
    const layout = results.layout;
    const back = results.backend;

    container.innerHTML = `
            <div style="display:flex;flex-direction:column;gap:10px;">
                <div style="font-weight:bold;color:var(--cyan);">Frontend</div>
                ${item('Chat input present', dom.inputPresent, '')}
                ${item('Chat send present', dom.sendButtonPresent, '')}
                ${item('Chat messages present', dom.messagesPresent, '')}
                ${item('Chat messages scrollable', dom.messagesScrollable, '')}
                ${item('Monaco loader available', monaco.hasRequire, `base: ${monaco.base}`)}
                ${item('Monaco editor initialized', monaco.hasMonaco, '')}

                <div style="font-weight:bold;color:var(--cyan);">Layout</div>
                ${item('Terminal not overlapping chat', !layout.overlappingTerminal, layout.overlappingTerminal ? 'terminal overlaps right sidebar' : '')}

                <div style="font-weight:bold;color:var(--cyan);">Backend</div>
                ${item('/v1/models', back.models.ok, back.models.detail)}
                ${item('/api/tags', back.tags.ok, back.tags.detail)}
                ${item('/v1/chat/completions', back.chat.ok, back.chat.detail)}

                <div style="font-weight:bold;color:var(--cyan);">Recent errors</div>
                <div style="max-height:140px;overflow:auto;border:1px solid rgba(0,212,255,0.2);padding:8px;border-radius:6px;font-size:11px;color:#f88;white-space:pre-wrap;">${results.errors.length ? results.errors.map(e => `• ${e}`).join('\n') : 'None'}</div>
            </div>
        `;
  }

  function renderModal() {
    if (document.getElementById('audit-modal')) return;
    const modal = document.createElement('div');
    modal.id = 'audit-modal';
    modal.style.cssText = 'position:fixed;inset:0;background:rgba(0,0,0,0.75);z-index:12000;display:none;align-items:center;justify-content:center;';
    modal.innerHTML = `
            <div style="background:var(--void);border:2px solid var(--cyan);border-radius:10px;width:800px;max-width:90vw;max-height:90vh;display:flex;flex-direction:column;">
                <div style="display:flex;align-items:center;justify-content:space-between;padding:12px 16px;border-bottom:1px solid rgba(0,212,255,0.2);">
                    <div style="color:var(--cyan);font-weight:bold;">System Audit</div>
                    <div style="display:flex;gap:8px;">
                        <button id="audit-run" style="padding:6px 10px;border:1px solid var(--cyan);background:rgba(0,212,255,0.1);color:var(--cyan);border-radius:4px;cursor:pointer;">Run</button>
                        <button id="audit-copy" style="padding:6px 10px;border:1px solid var(--orange);background:rgba(255,107,53,0.1);color:var(--orange);border-radius:4px;cursor:pointer;">Copy JSON</button>
                        <button id="audit-close" style="padding:6px 10px;border:1px solid var(--red);background:rgba(255,71,87,0.1);color:var(--red);border-radius:4px;cursor:pointer;">×</button>
                    </div>
                </div>
                <div id="audit-body" style="padding:14px;overflow:auto;flex:1;">Run audit to see results.</div>
            </div>
        `;
    document.body.appendChild(modal);

    document.getElementById('audit-close').onclick = () => modal.style.display = 'none';
    document.getElementById('audit-run').onclick = async () => {
      document.getElementById('audit-body').textContent = 'Running...';
      const res = await runAudit();
      modal.dataset.latest = JSON.stringify(res, null, 2);
      renderResults(res);
    };
    document.getElementById('audit-copy').onclick = () => {
      const txt = modal.dataset.latest || 'No audit run yet.';
      navigator.clipboard?.writeText(txt).catch(() => { });
    };
  }

  // Capture console errors for later display
  (function hookConsole() {
    const origError = console.error;
    window.__recentConsoleErrors = window.__recentConsoleErrors || [];
    console.error = function (...args) {
      window.__recentConsoleErrors.push(args.map(String).join(' '));
      if (window.__recentConsoleErrors.length > 100) {
        window.__recentConsoleErrors.shift();
      }
      origError.apply(console, args);
    };
  })();

  window.auditPanel = {
    openAudit() {
      renderModal();
      const modal = document.getElementById('audit-modal');
      if (!modal) return;
      modal.style.display = 'flex';
    }
  };
})();
