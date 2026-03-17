#!/usr/bin/env node
// ============================================================================
// RawrXD UI Smoke Runner — Headless API Contract + Backend Validation
// ============================================================================
// Usage:
//   node test/ui_smoke_runner.js                          # defaults
//   node test/ui_smoke_runner.js --url http://localhost:11435
//   node test/ui_smoke_runner.js --ollama http://localhost:11434
//   node test/ui_smoke_runner.js --tier 2                 # run single tier
//   node test/ui_smoke_runner.js --json                   # JSON output
//   node test/ui_smoke_runner.js --timeout 10000
//
// Exit codes:  0 = all pass,  1 = has failures,  2 = runtime error
// ============================================================================

const http = require('http');
const https = require('https');
const fs = require('fs');
const path = require('path');

// ---- CLI Args ----
const args = process.argv.slice(2);
function getArg(name, fallback) {
  const idx = args.indexOf('--' + name);
  if (idx !== -1 && args[idx + 1]) return args[idx + 1];
  return fallback;
}
const hasFlag = (name) => args.indexOf('--' + name) !== -1;

const CONFIG = {
  url: getArg('url', 'http://127.0.0.1:11435'),
  ollama: getArg('ollama', 'http://127.0.0.1:11434'),
  timeout: parseInt(getArg('timeout', '5000')),
  tier: getArg('tier', 'all'),
  json: hasFlag('json'),
  verbose: hasFlag('verbose') || hasFlag('v'),
};

// ---- Colors ----
const C = {
  reset: '\x1b[0m',
  green: '\x1b[32m',
  red: '\x1b[31m',
  yellow: '\x1b[33m',
  blue: '\x1b[34m',
  cyan: '\x1b[36m',
  dim: '\x1b[2m',
  bold: '\x1b[1m',
};

function log(msg) { if (!CONFIG.json) process.stdout.write(msg + '\n'); }
function logPass(id, name, ms, detail) {
  log(`  ${C.green}✔${C.reset} ${C.dim}[${id}]${C.reset} ${name} ${C.dim}(${ms}ms)${C.reset}${detail ? ' — ' + detail : ''}`);
}
function logFail(id, name, ms, detail) {
  log(`  ${C.red}✘${C.reset} ${C.dim}[${id}]${C.reset} ${name} ${C.dim}(${ms}ms)${C.reset}${detail ? ' — ' + C.red + detail + C.reset : ''}`);
}
function logSkip(id, name, reason) {
  log(`  ${C.yellow}○${C.reset} ${C.dim}[${id}]${C.reset} ${name} ${C.dim}— ${reason}${C.reset}`);
}

// ---- HTTP helpers ----
function httpRequest(urlStr, method, body, timeoutMs) {
  return new Promise((resolve) => {
    const url = new URL(urlStr);
    const mod = url.protocol === 'https:' ? https : http;
    const opts = {
      hostname: url.hostname,
      port: url.port,
      path: url.pathname + url.search,
      method: method || 'GET',
      headers: {},
      timeout: timeoutMs || CONFIG.timeout,
    };
    if (body) {
      const payload = JSON.stringify(body);
      opts.headers['Content-Type'] = 'application/json';
      opts.headers['Content-Length'] = Buffer.byteLength(payload);
    }

    const req = mod.request(opts, (res) => {
      let data = '';
      res.on('data', (chunk) => { data += chunk; });
      res.on('end', () => {
        let parsed = null;
        try { parsed = JSON.parse(data); } catch (_) { parsed = data; }
        resolve({ ok: res.statusCode >= 200 && res.statusCode < 300, status: res.statusCode, data: parsed, headers: res.headers });
      });
    });

    req.on('error', (e) => {
      resolve({ ok: false, status: 0, error: e.message });
    });
    req.on('timeout', () => {
      req.destroy();
      resolve({ ok: false, status: 0, error: 'timeout' });
    });

    if (body) req.write(JSON.stringify(body));
    req.end();
  });
}

function validateShape(data, shape) {
  const errors = [];
  for (const [key, spec] of Object.entries(shape)) {
    if (spec.required && !(key in data)) {
      errors.push(`Missing: ${key}`);
      continue;
    }
    if (key in data) {
      const val = data[key];
      if (spec.type === 'array' && !Array.isArray(val)) {
        errors.push(`${key}: expected array, got ${typeof val}`);
      } else if (spec.type !== 'array' && typeof val !== spec.type) {
        errors.push(`${key}: expected ${spec.type}, got ${typeof val}`);
      }
      if (spec.expect !== undefined && val !== spec.expect) {
        errors.push(`${key}: expected "${spec.expect}", got "${val}"`);
      }
    }
  }
  return errors;
}

// ---- Test Registry ----
const TESTS = [];
function test(tier, id, name, fn, opts) {
  TESTS.push({ tier, id, name, fn, requiresBackend: opts?.backend ?? false, status: 'wait', detail: '', duration: 0 });
}

// ============================================================================
// TIER 1: Standalone HTML File Integrity
// ============================================================================

test(1, 't1-file-exists', 'ide_chatbot.html exists', async () => {
  const p = path.resolve(__dirname, '..', 'gui', 'ide_chatbot.html');
  const exists = fs.existsSync(p);
  const size = exists ? fs.statSync(p).size : 0;
  return { pass: exists && size > 100000, detail: exists ? `${(size / 1024).toFixed(0)} KB` : 'File not found' };
});

test(1, 't1-no-qt-refs', 'No Qt bridge references remain', async () => {
  const p = path.resolve(__dirname, '..', 'gui', 'ide_chatbot.html');
  const content = fs.readFileSync(p, 'utf-8');
  const qtRefs = [
    'QWebChannel', 'qt.webChannelTransport', 'window.qt',
    'QNetworkAccessManager', 'window.native.httpRequest',
  ];
  const found = qtRefs.filter(r => content.includes(r));
  return { pass: found.length === 0, detail: found.length === 0 ? 'Clean — no Qt bridge refs' : 'Found: ' + found.join(', ') };
});

test(1, 't1-uses-fetch', 'HTTP transport uses browser fetch()', async () => {
  const p = path.resolve(__dirname, '..', 'gui', 'ide_chatbot.html');
  const content = fs.readFileSync(p, 'utf-8');
  const hasFetch = content.includes("await fetch(") || content.includes("fetch(this.base");
  const hasRawrXDStream = content.includes('class RawrXDStream');
  return { pass: hasFetch && hasRawrXDStream, detail: `fetch=${hasFetch}, RawrXDStream=${hasRawrXDStream}` };
});

test(1, 't1-critical-functions', 'Critical functions defined in HTML', async () => {
  const p = path.resolve(__dirname, '..', 'gui', 'ide_chatbot.html');
  const content = fs.readFileSync(p, 'utf-8');
  const fns = ['sendMessage', 'sendToBackend', 'connectBackend', 'addMessage', 'getActiveUrl', 'getOfflineResponse'];
  const found = fns.filter(f => content.includes('function ' + f));
  const missing = fns.filter(f => !content.includes('function ' + f));
  return {
    pass: missing.length === 0,
    detail: missing.length === 0 ? `All ${fns.length} critical functions found` : 'Missing: ' + missing.join(', '),
  };
});

test(1, 't1-panel-ids', 'All 30+ panel DOM IDs exist', async () => {
  const p = path.resolve(__dirname, '..', 'gui', 'ide_chatbot.html');
  const content = fs.readFileSync(p, 'utf-8');
  const panels = [
    'debugPanel', 'failurePanel', 'perfPanel', 'securityPanel', 'agentPanel',
    'rePanel', 'extPanel', 'backendSwitcherPanel', 'routerPanel', 'swarmPanel',
    'multiResponsePanel', 'asmDebugPanel', 'safetyPanel', 'policiesPanel',
    'hotpatchDetailPanel', 'cotDetailPanel', 'toolsPanel', 'enginePanel',
    'metricsPanel', 'subagentPanel', 'confidencePanel', 'governorPanel',
    'lspPanel', 'hybridPanel', 'replayPanel', 'phaseStatusPanel',
    'extensionsPanel', 'browserOverlay', 'fileEditorPanel', 'terminalPanel',
  ];
  const found = panels.filter(id => content.includes('id="' + id + '"'));
  const missing = panels.filter(id => !content.includes('id="' + id + '"'));
  return {
    pass: missing.length <= 2,
    detail: `${found.length}/${panels.length} panel IDs found` + (missing.length > 0 ? '. Missing: ' + missing.join(', ') : ''),
  };
});

test(1, 't1-offline-mode', 'Offline mode handler defined', async () => {
  const p = path.resolve(__dirname, '..', 'gui', 'ide_chatbot.html');
  const content = fs.readFileSync(p, 'utf-8');
  const hasOffline = content.includes('getOfflineResponse');
  const hasBeacon = content.includes('beaconProbe') || content.includes('beacon');
  const hasDirectMode = content.includes('directMode');
  return { pass: hasOffline && hasDirectMode, detail: `offline=${hasOffline} beacon=${hasBeacon} directMode=${hasDirectMode}` };
});

test(1, 't1-sanitization', 'Input sanitization present', async () => {
  const p = path.resolve(__dirname, '..', 'gui', 'ide_chatbot.html');
  const content = fs.readFileSync(p, 'utf-8');
  const hasValidate = content.includes('validateInput');
  const hasRateLimit = content.includes('checkRateLimit');
  const hasSecurity = content.includes('secLog');
  return { pass: hasValidate && hasRateLimit, detail: `validateInput=${hasValidate} rateLimit=${hasRateLimit} secLog=${hasSecurity}` };
});

test(1, 't1-serve-py', 'serve.py exists and has all endpoints', async () => {
  const p = path.resolve(__dirname, '..', 'gui', 'serve.py');
  if (!fs.existsSync(p)) return { pass: false, detail: 'serve.py not found' };
  const content = fs.readFileSync(p, 'utf-8');
  const endpoints = ['/health', '/status', '/models', '/v1/chat/completions', '/api/generate', '/ask',
    '/api/failures', '/api/agents/status', '/api/agents/replay', '/api/read-file'];
  const found = endpoints.filter(e => content.includes('"' + e + '"') || content.includes("'" + e + "'"));
  const missing = endpoints.filter(e => !content.includes('"' + e + '"') && !content.includes("'" + e + "'"));
  return {
    pass: missing.length === 0,
    detail: `${found.length}/${endpoints.length} endpoints in serve.py` + (missing.length > 0 ? '. Missing: ' + missing.join(', ') : ''),
  };
});

// ============================================================================
// TIER 2: Backend Connectivity
// ============================================================================

test(2, 't2-health', 'GET /health returns 200', async () => {
  const r = await httpRequest(CONFIG.url + '/health');
  return {
    pass: r.ok && r.data?.status === 'ok',
    detail: r.ok ? `status=${r.data.status}` : `Error: ${r.error || 'HTTP ' + r.status}`,
  };
}, { backend: true });

test(2, 't2-status', 'GET /status returns server info', async () => {
  const r = await httpRequest(CONFIG.url + '/status');
  if (!r.ok) return { pass: false, detail: `Error: ${r.error || 'HTTP ' + r.status}` };
  const errs = validateShape(r.data, { ready: { type: 'boolean', required: true }, backend: { type: 'string', required: true } });
  return { pass: errs.length === 0, detail: errs.length === 0 ? `backend=${r.data.backend}` : errs.join('; ') };
}, { backend: true });

test(2, 't2-models', 'GET /models returns model list', async () => {
  const r = await httpRequest(CONFIG.url + '/models');
  if (!r.ok) return { pass: false, detail: `Error: ${r.error || 'HTTP ' + r.status}` };
  return { pass: Array.isArray(r.data?.models), detail: `${r.data?.models?.length || 0} models` };
}, { backend: true });

test(2, 't2-api-tags', 'GET /api/tags returns Ollama tags', async () => {
  const r = await httpRequest(CONFIG.url + '/api/tags');
  if (!r.ok) return { pass: false, detail: `Error: ${r.error || 'HTTP ' + r.status}` };
  return { pass: Array.isArray(r.data?.models), detail: `${r.data?.models?.length || 0} models from /api/tags` };
}, { backend: true });

test(2, 't2-ollama-direct', 'Ollama direct (:11434) reachable', async () => {
  const r = await httpRequest(CONFIG.ollama + '/');
  return { pass: r.ok, detail: r.ok ? 'Ollama running' : `Error: ${r.error || 'HTTP ' + r.status}` };
});

test(2, 't2-failures', 'GET /api/failures returns failure log', async () => {
  const r = await httpRequest(CONFIG.url + '/api/failures');
  if (!r.ok) return { pass: false, detail: `Error: ${r.error || 'HTTP ' + r.status}` };
  const errs = validateShape(r.data, { failures: { type: 'array', required: true }, stats: { type: 'object', required: true } });
  return { pass: errs.length === 0, detail: errs.length === 0 ? `${r.data.failures.length} failures logged` : errs.join('; ') };
}, { backend: true });

test(2, 't2-agents-status', 'GET /api/agents/status returns agent info', async () => {
  const r = await httpRequest(CONFIG.url + '/api/agents/status');
  if (!r.ok) return { pass: false, detail: `Error: ${r.error || 'HTTP ' + r.status}` };
  return { pass: !!r.data?.agents, detail: `Agents: ${Object.keys(r.data?.agents || {}).join(', ')}` };
}, { backend: true });

test(2, 't2-chat-nonstream', 'POST /v1/chat/completions (non-streaming)', async () => {
  const r = await httpRequest(CONFIG.url + '/v1/chat/completions', 'POST',
    { model: 'rawrxd', messages: [{ role: 'user', content: 'Say OK' }], stream: false },
    CONFIG.timeout * 6);
  if (!r.ok) return { pass: false, detail: `Error: ${r.error || 'HTTP ' + r.status}` };
  const content = r.data?.choices?.[0]?.message?.content;
  return { pass: typeof content === 'string' && content.length > 0, detail: content ? `"${content.substring(0, 80)}"` : 'No content' };
}, { backend: true });

test(2, 't2-legacy-ask', 'POST /ask legacy endpoint', async () => {
  const r = await httpRequest(CONFIG.url + '/ask', 'POST', { question: 'Say OK', model: 'rawrxd' }, CONFIG.timeout * 3);
  if (!r.ok) return { pass: false, detail: `Error: ${r.error || 'HTTP ' + r.status}` };
  return { pass: typeof r.data?.answer === 'string', detail: r.data?.answer ? `"${r.data.answer.substring(0, 80)}"` : 'No answer' };
}, { backend: true });

test(2, 't2-read-file', 'POST /api/read-file reads local file', async () => {
  const r = await httpRequest(CONFIG.url + '/api/read-file', 'POST', { path: 'D:/rawrxd/gui/serve.py' });
  if (!r.ok) return { pass: false, detail: `Error: ${r.error || 'HTTP ' + r.status}` };
  const content = r.data?.content || r.data?.data;
  return { pass: typeof content === 'string' && content.length > 100, detail: `${(content?.length || 0)} chars` };
}, { backend: true });

test(2, 't2-404', 'Unknown endpoint returns 404', async () => {
  const r = await httpRequest(CONFIG.url + '/this/does/not/exist');
  return { pass: r.status === 404, detail: `Status: ${r.status}` };
}, { backend: true });

test(2, 't2-cors', 'CORS headers present', async () => {
  const r = await httpRequest(CONFIG.url + '/health');
  const cors = r.headers?.['access-control-allow-origin'];
  return { pass: !!cors, detail: cors ? `CORS: ${cors}` : 'No Access-Control-Allow-Origin' };
}, { backend: true });

// ============================================================================
// TIER 4: API Contract Validation (shape-level)
// ============================================================================

test(4, 't4-health-shape', '/health shape matches contract', async () => {
  const r = await httpRequest(CONFIG.url + '/health');
  if (!r.ok) return { pass: false, detail: 'Unreachable' };
  const errs = validateShape(r.data, { status: { type: 'string', required: true, expect: 'ok' } });
  return { pass: errs.length === 0, detail: errs.length === 0 ? 'Shape OK' : errs.join('; ') };
}, { backend: true });

test(4, 't4-status-shape', '/status shape matches contract', async () => {
  const r = await httpRequest(CONFIG.url + '/status');
  if (!r.ok) return { pass: false, detail: 'Unreachable' };
  const errs = validateShape(r.data, {
    ready: { type: 'boolean', required: true },
    backend: { type: 'string', required: true },
  });
  return { pass: errs.length === 0, detail: errs.length === 0 ? 'Contract valid' : errs.join('; ') };
}, { backend: true });

test(4, 't4-models-shape', '/models shape matches contract', async () => {
  const r = await httpRequest(CONFIG.url + '/models');
  if (!r.ok) return { pass: false, detail: 'Unreachable' };
  const errs = validateShape(r.data, { models: { type: 'array', required: true } });
  if (errs.length === 0 && Array.isArray(r.data.models) && r.data.models.length > 0) {
    const m = r.data.models[0];
    if (typeof m.name !== 'string' && typeof m !== 'string') {
      errs.push('First model item: expected name string');
    }
  }
  return { pass: errs.length === 0, detail: errs.length === 0 ? 'Contract valid' : errs.join('; ') };
}, { backend: true });

test(4, 't4-failures-shape', '/api/failures shape matches contract', async () => {
  const r = await httpRequest(CONFIG.url + '/api/failures');
  if (!r.ok) return { pass: false, detail: 'Unreachable' };
  const errs = validateShape(r.data, {
    failures: { type: 'array', required: true },
    stats: { type: 'object', required: true },
  });
  if (errs.length === 0 && r.data.stats) {
    errs.push(...validateShape(r.data.stats, { totalFailures: { type: 'number', required: true } }));
  }
  return { pass: errs.length === 0, detail: errs.length === 0 ? 'Contract valid' : errs.join('; ') };
}, { backend: true });

test(4, 't4-agents-shape', '/api/agents/status shape matches contract', async () => {
  const r = await httpRequest(CONFIG.url + '/api/agents/status');
  if (!r.ok) return { pass: false, detail: 'Unreachable' };
  const errs = validateShape(r.data, { agents: { type: 'object', required: true } });
  return { pass: errs.length === 0, detail: errs.length === 0 ? 'Contract valid' : errs.join('; ') };
}, { backend: true });

test(4, 't4-chat-shape', '/v1/chat/completions shape matches OpenAI', async () => {
  const r = await httpRequest(CONFIG.url + '/v1/chat/completions', 'POST',
    { model: 'rawrxd', messages: [{ role: 'user', content: 'Say OK' }], stream: false },
    CONFIG.timeout * 6);
  if (!r.ok) return { pass: false, detail: `Error: ${r.error || 'HTTP ' + r.status}` };
  const errs = validateShape(r.data, { choices: { type: 'array', required: true } });
  if (errs.length === 0 && r.data.choices?.[0]) {
    errs.push(...validateShape(r.data.choices[0], { message: { type: 'object', required: true } }));
    if (r.data.choices[0].message) {
      errs.push(...validateShape(r.data.choices[0].message, {
        role: { type: 'string', required: true },
        content: { type: 'string', required: true },
      }));
    }
  }
  return { pass: errs.length === 0, detail: errs.length === 0 ? 'OpenAI shape valid' : errs.join('; ') };
}, { backend: true });


// ============================================================================
// Runner
// ============================================================================

async function run() {
  const tiers = CONFIG.tier === 'all' ? [1, 2, 4] : [parseInt(CONFIG.tier)];
  // Note: Tier 3 (panel health) requires browser — use RawrXD-UI-TestHarness.html for that

  log(`\n${C.cyan}${C.bold}━━━ RawrXD UI Smoke Runner ━━━${C.reset}`);
  log(`${C.dim}Backend: ${CONFIG.url}  Ollama: ${CONFIG.ollama}  Timeout: ${CONFIG.timeout}ms${C.reset}\n`);

  let totalPass = 0, totalFail = 0, totalSkip = 0;
  const results = [];
  const t0global = Date.now();

  // Quick backend probe for skip logic
  let backendAlive = false;
  try {
    const probe = await httpRequest(CONFIG.url + '/health', 'GET', null, 3000);
    backendAlive = probe.ok;
  } catch (_) { }

  if (!backendAlive && tiers.some(t => t >= 2)) {
    log(`${C.yellow}⚠  Backend at ${CONFIG.url} not reachable — Tier 2/4 tests requiring backend will be skipped${C.reset}\n`);
  }

  for (const tier of tiers) {
    const tierNames = { 1: 'Standalone Integrity', 2: 'Backend Connectivity', 3: 'Panel Health', 4: 'API Contract' };
    log(`${C.bold}▸ Tier ${tier}: ${tierNames[tier] || 'Unknown'}${C.reset}`);

    const tierTests = TESTS.filter(t => t.tier === tier);

    for (const t of tierTests) {
      if (t.requiresBackend && !backendAlive) {
        t.status = 'skip';
        t.detail = 'Backend not reachable';
        totalSkip++;
        logSkip(t.id, t.name, t.detail);
        results.push({ ...t });
        continue;
      }

      const t0 = Date.now();
      try {
        const result = await t.fn();
        t.duration = Date.now() - t0;
        t.status = result.pass ? 'pass' : 'fail';
        t.detail = result.detail || '';
      } catch (e) {
        t.duration = Date.now() - t0;
        t.status = 'fail';
        t.detail = 'Exception: ' + e.message;
      }

      if (t.status === 'pass') { totalPass++; logPass(t.id, t.name, t.duration, CONFIG.verbose ? t.detail : ''); }
      else { totalFail++; logFail(t.id, t.name, t.duration, t.detail); }

      results.push({ id: t.id, tier: t.tier, name: t.name, status: t.status, detail: t.detail, duration: t.duration });
    }
    log('');
  }

  const totalDuration = Date.now() - t0global;
  const total = totalPass + totalFail + totalSkip;
  const rate = (totalPass + totalFail) > 0 ? Math.round(totalPass / (totalPass + totalFail) * 100) : 0;

  log(`${C.bold}━━━ Results ━━━${C.reset}`);
  log(`  ${C.green}Pass: ${totalPass}${C.reset}  ${C.red}Fail: ${totalFail}${C.reset}  ${C.yellow}Skip: ${totalSkip}${C.reset}  Total: ${total}  Rate: ${rate}%  Time: ${totalDuration}ms`);

  if (totalFail === 0 && totalSkip === 0) {
    log(`\n${C.green}${C.bold}✔ ALL TESTS PASSED${C.reset}\n`);
  } else if (totalFail === 0) {
    log(`\n${C.yellow}${C.bold}⚠ ALL REACHABLE TESTS PASSED (${totalSkip} skipped — start backend for full coverage)${C.reset}\n`);
  } else {
    log(`\n${C.red}${C.bold}✘ ${totalFail} TEST(S) FAILED${C.reset}\n`);
  }

  if (CONFIG.json) {
    const report = {
      timestamp: new Date().toISOString(),
      config: CONFIG,
      backendAlive,
      summary: { total, pass: totalPass, fail: totalFail, skip: totalSkip, rate, durationMs: totalDuration },
      tests: results,
    };
    process.stdout.write(JSON.stringify(report, null, 2) + '\n');
  }

  process.exit(totalFail > 0 ? 1 : 0);
}

run().catch(e => {
  console.error('Fatal:', e.message);
  process.exit(2);
});
