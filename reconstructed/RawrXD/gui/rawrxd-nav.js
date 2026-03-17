// ═══════════════════════════════════════════════════════════════════════════
// RawrXD Command Bar — Unified navigation + live status for ALL GUI panes
// Injected via <script src="/gui/rawrxd-nav.js"></script> in every page.
// ═══════════════════════════════════════════════════════════════════════════
(function () {
  'use strict';

  // Don't double-inject
  if (document.getElementById('rawrxd-command-bar')) return;

  // ── Route table — must match server.js GUI_ROUTES ──
  const PANES = [
    { label: 'Launcher', route: '/launcher', icon: '\u2302' },
    { label: 'Chatbot', route: '/chatbot', icon: '\uD83D\uDCAC' },
    { label: 'Agents', route: '/agents', icon: '\uD83E\uDD16' },
    { label: 'IDE', route: '/multiwindow', icon: '\uD83D\uDDA5' },
    { label: 'Features', route: '/feature-tester', icon: '\uD83E\uDDEA' },
    { label: 'Bruteforce', route: '/bruteforce', icon: '\u2699' },
    { label: 'Jumper', route: '/test-jumper', icon: '\uD83D\uDD0D' },
    { label: 'Swarm', route: '/test-swarm', icon: '\uD83D\uDC1D' },
  ];

  // ── Detect current page ──
  const currentPath = window.location.pathname;

  // ── Base URL for API calls ──
  const API_BASE = (window.location.protocol !== 'file:' && window.location.port)
    ? window.location.origin
    : 'http://localhost:8080';

  // ── Inject CSS ──
  const style = document.createElement('style');
  style.textContent = `
    #rawrxd-command-bar {
      position: fixed;
      top: 0; left: 0; right: 0;
      z-index: 99999;
      height: 36px;
      background: #0d1117;
      border-bottom: 1px solid #30363d;
      display: flex;
      align-items: center;
      padding: 0 8px;
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
      font-size: 12px;
      color: #e6edf3;
      gap: 2px;
      box-shadow: 0 2px 8px rgba(0,0,0,.4);
      user-select: none;
    }
    #rawrxd-command-bar .rcb-brand {
      font-family: 'Cascadia Code', 'Fira Code', monospace;
      font-weight: 700;
      color: #58a6ff;
      font-size: 13px;
      padding: 0 10px 0 4px;
      letter-spacing: 1px;
      flex-shrink: 0;
      cursor: pointer;
    }
    #rawrxd-command-bar .rcb-sep {
      width: 1px;
      height: 18px;
      background: #30363d;
      margin: 0 4px;
      flex-shrink: 0;
    }
    #rawrxd-command-bar .rcb-nav {
      display: flex;
      gap: 1px;
      flex-shrink: 0;
    }
    #rawrxd-command-bar .rcb-link {
      padding: 4px 8px;
      border-radius: 5px;
      color: #8b949e;
      cursor: pointer;
      transition: all .12s;
      white-space: nowrap;
      font-size: 11px;
      display: flex;
      align-items: center;
      gap: 4px;
      text-decoration: none;
    }
    #rawrxd-command-bar .rcb-link:hover {
      background: #161b22;
      color: #e6edf3;
    }
    #rawrxd-command-bar .rcb-link.active {
      background: #1f6feb;
      color: #fff;
      font-weight: 600;
    }
    #rawrxd-command-bar .rcb-link .rcb-icon {
      font-size: 13px;
    }
    #rawrxd-command-bar .rcb-spacer {
      flex: 1;
    }
    #rawrxd-command-bar .rcb-status {
      display: flex;
      gap: 10px;
      align-items: center;
      flex-shrink: 0;
      padding-right: 4px;
    }
    #rawrxd-command-bar .rcb-indicator {
      display: flex;
      align-items: center;
      gap: 4px;
      font-size: 11px;
      color: #8b949e;
    }
    #rawrxd-command-bar .rcb-dot {
      width: 7px;
      height: 7px;
      border-radius: 50%;
      flex-shrink: 0;
    }
    #rawrxd-command-bar .rcb-dot.on  { background: #3fb950; box-shadow: 0 0 5px #3fb950; }
    #rawrxd-command-bar .rcb-dot.off { background: #f85149; }
    #rawrxd-command-bar .rcb-dot.wait { background: #d29922; animation: rcb-pulse 1s infinite; }
    @keyframes rcb-pulse { 0%,100%{opacity:1} 50%{opacity:.35} }
    #rawrxd-command-bar .rcb-metric {
      font-family: 'Cascadia Code', 'Fira Code', monospace;
      color: #e6edf3;
      font-size: 11px;
      font-weight: 600;
    }
    /* Push page content down so it's not hidden behind the bar */
    body { padding-top: 38px !important; }
  `;
  document.head.appendChild(style);

  // ── Build bar HTML ──
  const bar = document.createElement('div');
  bar.id = 'rawrxd-command-bar';

  // Brand
  const brand = document.createElement('span');
  brand.className = 'rcb-brand';
  brand.textContent = 'RawrXD';
  brand.title = 'Return to Launcher';
  brand.onclick = () => window.location.href = '/launcher';
  bar.appendChild(brand);

  // Separator
  bar.appendChild(makeSep());

  // Nav links
  const nav = document.createElement('div');
  nav.className = 'rcb-nav';
  for (const p of PANES) {
    const a = document.createElement('a');
    a.className = 'rcb-link' + (currentPath === p.route || (currentPath === '/' && p.route === '/launcher') ? ' active' : '');
    a.href = p.route;
    a.innerHTML = `<span class="rcb-icon">${p.icon}</span>${p.label}`;
    a.title = p.label + ' (' + p.route + ')';
    nav.appendChild(a);
  }
  bar.appendChild(nav);

  // Spacer
  const spacer = document.createElement('div');
  spacer.className = 'rcb-spacer';
  bar.appendChild(spacer);

  // Separator
  bar.appendChild(makeSep());

  // Status indicators (right side)
  const status = document.createElement('div');
  status.className = 'rcb-status';

  status.innerHTML = `
    <div class="rcb-indicator" title="Server (port 8080)">
      <span class="rcb-dot wait" id="rcb-dot-server"></span>
      <span>Server</span>
    </div>
    <div class="rcb-indicator" title="Ollama (port 11434)">
      <span class="rcb-dot wait" id="rcb-dot-ollama"></span>
      <span>Ollama</span>
    </div>
    <div class="rcb-indicator" title="Tokens per second">
      <span>tps:</span>
      <span class="rcb-metric" id="rcb-tps">—</span>
    </div>
    <div class="rcb-indicator" title="Total requests">
      <span>req:</span>
      <span class="rcb-metric" id="rcb-reqs">—</span>
    </div>
    <div class="rcb-indicator" title="Memory usage">
      <span>mem:</span>
      <span class="rcb-metric" id="rcb-mem">—</span>
    </div>
  `;
  bar.appendChild(status);

  // Insert at top of body
  document.body.prepend(bar);

  // ── Live status polling ──
  async function pollStatus() {
    // Server health
    try {
      const res = await fetch(API_BASE + '/api/metrics', { signal: AbortSignal.timeout(3000) });
      if (res.ok) {
        const m = await res.json();
        setDot('rcb-dot-server', 'on');
        const tps = m.tokensPerSec || 0;
        document.getElementById('rcb-tps').textContent = tps > 0 ? tps.toFixed(1) : '0';
        document.getElementById('rcb-reqs').textContent = m.requests || 0;
        document.getElementById('rcb-mem').textContent = (m.memUsedMB || 0) + 'MB';
        // Ollama from server metrics (avoids CORS issues with direct Ollama probe)
        setDot('rcb-dot-ollama', m.ollamaReachable ? 'on' : 'off');
      } else {
        setDot('rcb-dot-server', 'off');
      }
    } catch (_) {
      setDot('rcb-dot-server', 'off');
      setDot('rcb-dot-ollama', 'off');
    }
  }

  function setDot(id, state) {
    const dot = document.getElementById(id);
    if (dot) dot.className = 'rcb-dot ' + state;
  }

  function makeSep() {
    const s = document.createElement('div');
    s.className = 'rcb-sep';
    return s;
  }

  // Poll on load and every 8 seconds
  pollStatus();
  setInterval(pollStatus, 8000);
})();
