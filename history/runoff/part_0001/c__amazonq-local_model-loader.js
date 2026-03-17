// Unified model loader for IDE: supports tiinyAI, GGUF, Pyre
const { spawn } = require('child_process');
const http = require('http');
const path = require('path');
const fs = require('fs');

class ModelProcess {
  constructor(label, command, args, cwd) {
    this.label = label;
    this.command = command;
    this.args = args || [];
    this.cwd = cwd || process.cwd();
    this.proc = null;
    this.stdout = '';
    this.stderr = '';
    this.startTime = null;
  }
  start() {
    return new Promise((resolve, reject) => {
      try {
        this.startTime = Date.now();
        this.proc = spawn(this.command, this.args, { cwd: this.cwd, env: process.env });
        this.proc.stdout.on('data', (d) => { this.stdout += d.toString(); });
        this.proc.stderr.on('data', (d) => { this.stderr += d.toString(); });
        this.proc.on('error', (e) => reject(e));
        // Resolve once we get first stdout or after 1s
        let resolved = false;
        const timer = setTimeout(() => { if (!resolved) { resolved = true; resolve(); } }, 1000);
        this.proc.stdout.once('data', () => { if (!resolved) { clearTimeout(timer); resolved = true; resolve(); } });
      } catch (e) {
        reject(e);
      }
    });
  }
  stop() {
    return new Promise((resolve) => {
      if (!this.proc) return resolve();
      try {
        this.proc.kill('SIGTERM');
      } catch { }
      this.proc = null;
      resolve();
    });
  }
  status() {
    return {
      label: this.label,
      running: !!this.proc,
      pid: this.proc ? this.proc.pid : null,
      uptimeMs: this.proc ? (Date.now() - this.startTime) : 0,
      stdoutTail: this.stdout.slice(-2000),
      stderrTail: this.stderr.slice(-2000),
    };
  }
}

function httpPostJson(url, body) {
  return new Promise((resolve, reject) => {
    try {
      const u = new URL(url);
      const data = JSON.stringify(body || {});
      const options = {
        hostname: u.hostname,
        port: u.port,
        path: u.pathname,
        method: 'POST',
        headers: { 'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(data) }
      };
      const req = http.request(options, (res) => {
        let buf = '';
        res.on('data', (chunk) => buf += chunk.toString());
        res.on('end', () => {
          try { resolve(JSON.parse(buf || '{}')); } catch { resolve({ ok: true, raw: buf }); }
        });
      });
      req.on('error', reject);
      req.write(data);
      req.end();
    } catch (e) {
      reject(e);
    }
  });
}

class ModelManager {
  constructor(configPath) {
    this.configPath = configPath || path.join(process.cwd(), 'config.json');
    this.cfg = this._loadCfg();
    this.processes = { tiinyAI: null, gguf: null, pyre: null };
    this.active = null; // { source, modelPath }
  }
  _loadCfg() {
    try {
      const raw = fs.readFileSync(this.configPath, 'utf-8');
      return JSON.parse(raw);
    } catch {
      return {
        tiinyAI: { command: 'tiinyai.exe', args: ['--server', '--port', '45100'], port: 45100 },
        gguf: { command: 'RawrXD-Shell.exe', args: ['--server', '--port', '45101'], port: 45101 },
        pyre: { command: 'RawrXD-Shell.exe', args: ['--server', '--port', '45102', '--engine', 'pyre'], port: 45102 }
      };
    }
  }
  async loadModel(source, modelPath) {
    if (!['tiinyAI', 'gguf', 'pyre'].includes(source)) throw new Error('Unsupported source: ' + source);
    // stop any running process of same source
    await this.unloadModel(source);
    const cfg = this.cfg[source];
    if (!cfg) throw new Error('Missing config for ' + source);

    // append model args
    const args = Array.isArray(cfg.args) ? [...cfg.args] : [];
    if (modelPath) args.push('--model', modelPath);

    const proc = new ModelProcess(`${source}-server`, cfg.command, args, cfg.cwd || process.cwd());
    await proc.start();
    this.processes[source] = proc;
    this.active = { source, modelPath };

    // Optional: inform HTTP server if supported
    if (cfg.port) {
      try { await httpPostJson(`http://localhost:${cfg.port}/models/load`, { model: modelPath }); } catch { }
    }
    return { ok: true, source, modelPath, port: cfg.port };
  }
  async unloadModel(source) {
    const proc = this.processes[source];
    if (proc) {
      try { await proc.stop(); } catch { }
      this.processes[source] = null;
      if (this.active && this.active.source === source) this.active = null;
    }
    return { ok: true };
  }
  async switchActive(source) {
    if (!this.processes[source]) throw new Error('Source not running: ' + source);
    this.active = { source, modelPath: null };
    return { ok: true, active: this.active };
  }
  status() {
    return {
      active: this.active,
      tiinyAI: this.processes.tiinyAI ? this.processes.tiinyAI.status() : { running: false },
      gguf: this.processes.gguf ? this.processes.gguf.status() : { running: false },
      pyre: this.processes.pyre ? this.processes.pyre.status() : { running: false },
    };
  }
  async generate(prompt, params) {
    // route to active backend
    if (!this.active) throw new Error('No active model');
    const src = this.active.source;
    const port = this.cfg[src] && this.cfg[src].port;
    if (!port) throw new Error('Backend port not configured');
    // Common endpoint names; try multiple for compatibility
    const endpoints = ['/generate', '/v1/generate', '/inference/generate'];
    let lastErr;
    for (const ep of endpoints) {
      try {
        const res = await httpPostJson(`http://localhost:${port}${ep}`, { prompt, params });
        if (res) return res;
      } catch (e) { lastErr = e; }
    }
    throw lastErr || new Error('Generation failed');
  }
}

module.exports = { ModelManager };
