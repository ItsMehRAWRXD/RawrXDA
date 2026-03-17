// Unified model loader for IDE: supports tiinyAI, GGUF, Pyre, Ollama
// All paths rooted to D:\rawrxd — no C-drive references
// Production: no hardcoded responses, real metrics, real health probes
const { spawn } = require('child_process');
const http = require('http');
const path = require('path');
const fs = require('fs');
const os = require('os');

const IDE_ROOT = path.resolve(__dirname, '..', '..');

// ────────── Real metrics collector ──────────
class Metrics {
  constructor() {
    this.counters = {};     // { key: number }
    this.timings = {};      // { key: [ms samples] }
    this.gauges = {};       // { key: number }
    this._startTs = Date.now();
  }
  inc(key, n = 1) { this.counters[key] = (this.counters[key] || 0) + n; }
  gauge(key, v) { this.gauges[key] = v; }
  startTimer(key) { return { key, t0: process.hrtime.bigint() }; }
  endTimer(handle) {
    const ms = Number(process.hrtime.bigint() - handle.t0) / 1e6;
    (this.timings[handle.key] = this.timings[handle.key] || []).push(ms);
    if (this.timings[handle.key].length > 500) this.timings[handle.key].shift();
    return ms;
  }
  snapshot() {
    const result = { uptimeMs: Date.now() - this._startTs, counters: { ...this.counters }, gauges: { ...this.gauges }, timings: {} };
    for (const [k, arr] of Object.entries(this.timings)) {
      const sorted = [...arr].sort((a, b) => a - b);
      result.timings[k] = {
        count: arr.length,
        avgMs: arr.reduce((a, b) => a + b, 0) / arr.length,
        p50Ms: sorted[Math.floor(sorted.length * 0.5)] || 0,
        p95Ms: sorted[Math.floor(sorted.length * 0.95)] || 0,
        p99Ms: sorted[Math.floor(sorted.length * 0.99)] || 0,
      };
    }
    result.gauges.memUsedMB = Math.round(process.memoryUsage().rss / 1048576);
    result.gauges.cpuCount = os.cpus().length;
    return result;
  }
}
const metrics = new Metrics();

// ────────── Model process manager ──────────
class ModelProcess {
  constructor(label, command, args, cwd) {
    this.label = label;
    this.command = command;
    this.args = args || [];
    this.cwd = cwd || IDE_ROOT;
    this.proc = null;
    this.stdout = '';
    this.stderr = '';
    this.startTime = null;
    this.exitCode = null;
    this.healthy = false;
    this._healthTimer = null;
  }
  start() {
    return new Promise((resolve, reject) => {
      try {
        // Validate binary exists before spawning
        const bin = this.command;
        if (!fs.existsSync(bin)) {
          return reject(new Error(`Binary not found: ${bin}`));
        }
        this.startTime = Date.now();
        this.exitCode = null;
        this.healthy = false;
        this.proc = spawn(this.command, this.args, {
          cwd: this.cwd,
          env: { ...process.env, RAWRXD_ROOT: IDE_ROOT },
          windowsHide: true
        });
        this.proc.stdout.on('data', (d) => {
          this.stdout += d.toString();
          // Cap buffer at 64KB
          if (this.stdout.length > 65536) this.stdout = this.stdout.slice(-32768);
        });
        this.proc.stderr.on('data', (d) => {
          this.stderr += d.toString();
          if (this.stderr.length > 65536) this.stderr = this.stderr.slice(-32768);
        });
        this.proc.on('exit', (code) => { this.exitCode = code; this.healthy = false; });
        this.proc.on('error', (e) => {
          this.exitCode = -1;
          this.healthy = false;
          reject(e);
        });
        // Resolve once we get first stdout, or after 2s timeout
        let resolved = false;
        const timer = setTimeout(() => { if (!resolved) { resolved = true; resolve(); } }, 2000);
        this.proc.stdout.once('data', () => {
          if (!resolved) { clearTimeout(timer); resolved = true; resolve(); }
        });
        metrics.inc('process.start');
      } catch (e) {
        reject(e);
      }
    });
  }
  stop() {
    return new Promise((resolve) => {
      if (this._healthTimer) { clearInterval(this._healthTimer); this._healthTimer = null; }
      if (!this.proc) return resolve();
      try { this.proc.kill('SIGTERM'); } catch { }
      // Force-kill after 3s
      const timer = setTimeout(() => { try { this.proc.kill('SIGKILL'); } catch { } }, 3000);
      this.proc.once('exit', () => { clearTimeout(timer); this.proc = null; this.healthy = false; resolve(); });
      metrics.inc('process.stop');
    });
  }
  status() {
    return {
      label: this.label,
      running: !!(this.proc && this.exitCode === null),
      healthy: this.healthy,
      pid: this.proc ? this.proc.pid : null,
      exitCode: this.exitCode,
      uptimeMs: this.proc && this.exitCode === null ? (Date.now() - this.startTime) : 0,
      stdoutTail: this.stdout.slice(-2000),
      stderrTail: this.stderr.slice(-2000),
    };
  }
  // Continuous health check via HTTP GET /health on the backend port
  startHealthCheck(port, intervalMs = 5000) {
    if (this._healthTimer) clearInterval(this._healthTimer);
    this._healthTimer = setInterval(async () => {
      try {
        await httpGet(`http://localhost:${port}/health`, 2000);
        this.healthy = true;
      } catch {
        this.healthy = false;
      }
    }, intervalMs);
  }
}

// ────────── HTTP helpers ──────────
function httpGet(url, timeoutMs = 5000) {
  return new Promise((resolve, reject) => {
    const u = new URL(url);
    const req = http.get({ hostname: u.hostname, port: u.port, path: u.pathname, timeout: timeoutMs }, (res) => {
      let buf = '';
      res.on('data', (c) => buf += c);
      res.on('end', () => { try { resolve(JSON.parse(buf)); } catch { resolve({ raw: buf }); } });
    });
    req.on('error', reject);
    req.on('timeout', () => { req.destroy(); reject(new Error('timeout')); });
  });
}

function httpPostJson(url, body, timeoutMs = 30000) {
  return new Promise((resolve, reject) => {
    const u = new URL(url);
    const data = JSON.stringify(body || {});
    const options = {
      hostname: u.hostname, port: u.port, path: u.pathname, method: 'POST',
      headers: { 'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(data) },
      timeout: timeoutMs
    };
    const req = http.request(options, (res) => {
      let buf = '';
      res.on('data', (chunk) => buf += chunk);
      res.on('end', () => {
        try { resolve(JSON.parse(buf || '{}')); } catch { resolve({ ok: true, raw: buf }); }
      });
    });
    req.on('error', reject);
    req.on('timeout', () => { req.destroy(); reject(new Error('timeout')); });
    req.write(data);
    req.end();
  });
}

// ────────── Model file discovery ──────────
function discoverModels(searchDirs) {
  const found = [];
  const exts = ['.gguf', '.bin', '.safetensors', '.pt', '.pth', '.onnx'];
  for (const dir of searchDirs) {
    try {
      if (!fs.existsSync(dir)) continue;
      const entries = fs.readdirSync(dir, { withFileTypes: true });
      for (const e of entries) {
        if (e.isFile() && exts.some(ext => e.name.toLowerCase().endsWith(ext))) {
          const full = path.join(dir, e.name);
          const stat = fs.statSync(full);
          found.push({
            name: e.name,
            path: full,
            sizeMB: Math.round(stat.size / 1048576),
            format: path.extname(e.name).replace('.', '').toUpperCase(),
            modified: stat.mtime.toISOString()
          });
        }
        // One level of subdirectory scan
        if (e.isDirectory()) {
          try {
            const sub = fs.readdirSync(path.join(dir, e.name), { withFileTypes: true });
            for (const se of sub) {
              if (se.isFile() && exts.some(ext => se.name.toLowerCase().endsWith(ext))) {
                const full = path.join(dir, e.name, se.name);
                const stat = fs.statSync(full);
                found.push({
                  name: se.name,
                  path: full,
                  sizeMB: Math.round(stat.size / 1048576),
                  format: path.extname(se.name).replace('.', '').toUpperCase(),
                  modified: stat.mtime.toISOString()
                });
              }
            }
          } catch { }
        }
      }
    } catch { }
  }
  return found;
}

// ────────── Ollama bridge ──────────
class OllamaBridge {
  constructor(baseUrl = 'http://localhost:11434') {
    this.baseUrl = baseUrl;
  }
  async isAvailable() {
    try { const r = await httpGet(`${this.baseUrl}/api/tags`, 3000); return !!r; }
    catch { return false; }
  }
  async listModels() {
    try {
      const r = await httpGet(`${this.baseUrl}/api/tags`, 5000);
      return (r.models || []).map(m => ({ name: m.name, size: m.size, modified: m.modified_at, format: 'OLLAMA' }));
    } catch { return []; }
  }
  async generate(model, prompt, params = {}) {
    const timer = metrics.startTimer('ollama.generate');
    try {
      const body = { model, prompt, stream: false, options: {} };
      if (params.temperature !== undefined) body.options.temperature = params.temperature;
      if (params.topP !== undefined) body.options.top_p = params.topP;
      if (params.topK !== undefined) body.options.top_k = params.topK;
      if (params.maxTokens !== undefined) body.options.num_predict = params.maxTokens;
      if (params.repeatPenalty !== undefined) body.options.repeat_penalty = params.repeatPenalty;
      const res = await httpPostJson(`${this.baseUrl}/api/generate`, body, 120000);
      const ms = metrics.endTimer(timer);
      metrics.inc('ollama.tokens', res.eval_count || 0);
      return {
        response: res.response,
        model: res.model,
        totalDurationMs: (res.total_duration || 0) / 1e6,
        loadDurationMs: (res.load_duration || 0) / 1e6,
        evalCount: res.eval_count || 0,
        evalDurationMs: (res.eval_duration || 0) / 1e6,
        tokensPerSec: res.eval_count && res.eval_duration ? (res.eval_count / (res.eval_duration / 1e9)) : 0,
        promptEvalCount: res.prompt_eval_count || 0,
        wallClockMs: ms
      };
    } catch (e) {
      metrics.endTimer(timer);
      throw e;
    }
  }
  async chat(model, messages, params = {}) {
    const timer = metrics.startTimer('ollama.chat');
    try {
      const body = { model, messages, stream: false, options: {} };
      if (params.temperature !== undefined) body.options.temperature = params.temperature;
      if (params.maxTokens !== undefined) body.options.num_predict = params.maxTokens;
      const res = await httpPostJson(`${this.baseUrl}/api/chat`, body, 120000);
      const ms = metrics.endTimer(timer);
      metrics.inc('ollama.chatTokens', res.eval_count || 0);
      return {
        message: res.message,
        model: res.model,
        totalDurationMs: (res.total_duration || 0) / 1e6,
        evalCount: res.eval_count || 0,
        tokensPerSec: res.eval_count && res.eval_duration ? (res.eval_count / (res.eval_duration / 1e9)) : 0,
        wallClockMs: ms
      };
    } catch (e) {
      metrics.endTimer(timer);
      throw e;
    }
  }
}

// ────────── ModelManager ──────────
class ModelManager {
  constructor(configPath) {
    this.configPath = configPath || path.join(__dirname, 'config.json');
    this.cfg = this._loadCfg();
    this.processes = { tiinyAI: null, gguf: null, pyre: null };
    this.active = null; // { source, modelPath, port }
    this.ollama = new OllamaBridge(this.cfg.ollama?.baseUrl || 'http://localhost:11434');
    this.modelSearchDirs = [
      IDE_ROOT,
      path.join(IDE_ROOT, 'models'),
      path.join(IDE_ROOT, 'Modelfiles'),
      path.join(IDE_ROOT, 'test_40gb_models'),
    ];
  }
  _loadCfg() {
    try {
      const raw = fs.readFileSync(this.configPath, 'utf-8');
      const parsed = JSON.parse(raw);
      // Also load main config for Ollama settings
      try {
        const mainCfg = JSON.parse(fs.readFileSync(path.join(IDE_ROOT, 'rawrxd.config.json'), 'utf-8'));
        parsed.ollama = mainCfg.ollama || {};
        parsed.inference = mainCfg.inference || {};
      } catch { }
      return parsed;
    } catch {
      return {
        tiinyAI: { command: path.join(IDE_ROOT, 'bin', 'tiinyai.exe'), args: ['--server', '--port', '45100'], port: 45100 },
        gguf: { command: path.join(IDE_ROOT, 'build_ide_output', 'CodexAIReverseEngine.exe'), args: ['--server', '--port', '45101'], port: 45101 },
        pyre: { command: path.join(IDE_ROOT, 'build_ide_output', 'CodexAIReverseEngine.exe'), args: ['--server', '--port', '45102', '--engine', 'pyre'], port: 45102 },
        ollama: { baseUrl: 'http://localhost:11434', model: 'llama2' },
        inference: { temperature: 0.7, maxTokens: 512 }
      };
    }
  }

  // ── Discover real model files on disk ──
  discoverModels() {
    const timer = metrics.startTimer('discover.models');
    const local = discoverModels(this.modelSearchDirs);
    metrics.endTimer(timer);
    metrics.gauge('models.localCount', local.length);
    return local;
  }

  // ── Discover Ollama models ──
  async discoverOllamaModels() {
    return this.ollama.listModels();
  }

  async loadModel(source, modelPath) {
    if (source === 'ollama') {
      // Ollama doesn't need a subprocess — just set active
      const available = await this.ollama.isAvailable();
      if (!available) throw new Error('Ollama not reachable at ' + (this.cfg.ollama?.baseUrl || 'http://localhost:11434'));
      this.active = { source: 'ollama', modelPath: modelPath || this.cfg.ollama?.model || 'llama2', port: null };
      metrics.inc('model.load.ollama');
      return { ok: true, source: 'ollama', modelPath: this.active.modelPath };
    }
    if (!['tiinyAI', 'gguf', 'pyre'].includes(source)) throw new Error('Unsupported source: ' + source);
    await this.unloadModel(source);
    const cfg = this.cfg[source];
    if (!cfg) throw new Error('Missing config for ' + source);

    // Validate model file exists (if a path was given)
    if (modelPath && !fs.existsSync(modelPath)) {
      throw new Error('Model file not found: ' + modelPath);
    }

    const args = Array.isArray(cfg.args) ? [...cfg.args] : [];
    if (modelPath) args.push('--model', modelPath);

    const cwd = cfg.cwd || IDE_ROOT;
    const proc = new ModelProcess(`${source}-server`, cfg.command, args, cwd);
    const timer = metrics.startTimer('model.load.' + source);
    await proc.start();
    const loadMs = metrics.endTimer(timer);
    this.processes[source] = proc;
    this.active = { source, modelPath, port: cfg.port };

    // Start background health checks
    if (cfg.port) {
      proc.startHealthCheck(cfg.port);
      // Attempt initial load notification
      try { await httpPostJson(`http://localhost:${cfg.port}/models/load`, { model: modelPath }, 10000); } catch { }
    }
    metrics.inc('model.load.' + source);
    return { ok: true, source, modelPath, port: cfg.port, loadMs };
  }

  async unloadModel(source) {
    if (source === 'ollama') {
      if (this.active && this.active.source === 'ollama') this.active = null;
      return { ok: true };
    }
    const proc = this.processes[source];
    if (proc) {
      await proc.stop();
      this.processes[source] = null;
      if (this.active && this.active.source === source) this.active = null;
      metrics.inc('model.unload.' + source);
    }
    return { ok: true };
  }

  async switchActive(source) {
    if (source === 'ollama') {
      const ok = await this.ollama.isAvailable();
      if (!ok) throw new Error('Ollama not reachable');
      this.active = { source: 'ollama', modelPath: this.cfg.ollama?.model || 'llama2', port: null };
      return { ok: true, active: this.active };
    }
    if (!this.processes[source]) throw new Error('Source not running: ' + source);
    const cfg = this.cfg[source];
    this.active = { source, modelPath: null, port: cfg?.port };
    return { ok: true, active: this.active };
  }

  status() {
    return {
      active: this.active,
      tiinyAI: this.processes.tiinyAI ? this.processes.tiinyAI.status() : { running: false, healthy: false },
      gguf: this.processes.gguf ? this.processes.gguf.status() : { running: false, healthy: false },
      pyre: this.processes.pyre ? this.processes.pyre.status() : { running: false, healthy: false },
      metrics: metrics.snapshot()
    };
  }

  // ── Generation: routes to the real active backend ──
  async generate(prompt, params = {}) {
    if (!this.active) throw new Error('No active model — call /models/load first');
    const src = this.active.source;
    const infCfg = this.cfg.inference || {};
    const mergedParams = {
      temperature: params.temperature ?? infCfg.temperature ?? 0.7,
      maxTokens: params.maxTokens ?? infCfg.maxTokens ?? 512,
      topP: params.topP ?? infCfg.topP ?? 0.9,
      topK: params.topK ?? infCfg.topK ?? 40,
      repeatPenalty: params.repeatPenalty ?? infCfg.repetitionPenalty ?? 1.1,
    };

    // Ollama path: use real Ollama API
    if (src === 'ollama') {
      const model = this.active.modelPath || this.cfg.ollama?.model || 'llama2';
      return this.ollama.generate(model, prompt, mergedParams);
    }

    // Backend subprocess path: try the real HTTP inference endpoints
    const port = this.active.port || (this.cfg[src] && this.cfg[src].port);
    if (!port) throw new Error('Backend port not configured for ' + src);

    const timer = metrics.startTimer('generate.' + src);
    const endpoints = [
      '/api/generate',
      '/v1/completions',
      '/generate',
      '/inference/generate'
    ];
    const body = {
      prompt,
      temperature: mergedParams.temperature,
      max_tokens: mergedParams.maxTokens,
      top_p: mergedParams.topP,
      top_k: mergedParams.topK,
      repeat_penalty: mergedParams.repeatPenalty
    };
    let lastErr;
    for (const ep of endpoints) {
      try {
        const res = await httpPostJson(`http://localhost:${port}${ep}`, body, 120000);
        const ms = metrics.endTimer(timer);
        metrics.inc('generate.success.' + src);
        // Normalize response shape
        return {
          response: res.response || res.content || res.text || res.choices?.[0]?.text || JSON.stringify(res),
          model: res.model || src,
          wallClockMs: ms,
          source: src,
          raw: res
        };
      } catch (e) { lastErr = e; }
    }
    metrics.endTimer(timer);
    metrics.inc('generate.fail.' + src);
    throw lastErr || new Error('All inference endpoints failed on port ' + port);
  }

  // ── Chat: routes to Ollama chat or backend ──
  async chat(messages, params = {}) {
    if (!this.active) throw new Error('No active model — call /models/load first');
    if (this.active.source === 'ollama') {
      const model = this.active.modelPath || this.cfg.ollama?.model || 'llama2';
      return this.ollama.chat(model, messages, params);
    }
    // For non-Ollama backends, convert chat to prompt and use generate
    const prompt = messages.map(m => `${m.role}: ${m.content}`).join('\n') + '\nassistant:';
    return this.generate(prompt, params);
  }

  metricsSnapshot() { return metrics.snapshot(); }
}

module.exports = { ModelManager, IDE_ROOT, Metrics, OllamaBridge, discoverModels, metrics };
