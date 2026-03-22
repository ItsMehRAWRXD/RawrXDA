/**
 * Win32 Native Bridge — RXIP named-pipe client
 *
 * Reverse-engineered from rawr_ipc.asm RXIP framing protocol.
 * Connects to the RawrXD-Win32IDE native process via \\.\pipe\rawrxd-main.
 *
 * Frame layout (little-endian):
 *   [u32 magic = 0x50495852 'RXIP']
 *   [u32 type]
 *   [u32 size]
 *   [u32 crc  = xor32 over payload dwords, trailing bytes folded in]
 *   [payload[size] bytes]
 *
 * All payload bodies are UTF-8 JSON.  Request/response correlation via { id } field.
 */

'use strict';

const net  = require('net');
const path = require('path');
const fs   = require('fs');
const { spawn } = require('child_process');

// ─── RXIP constants ───────────────────────────────────────────────────────────
const RXIP_MAGIC = 0x50495852;

const FRAME_TYPE = {
  AI_INVOKE:   0x01,
  AI_RESULT:   0x02,
  STATUS_REQ:  0x03,
  STATUS_RESP: 0x04,
  PING:        0x05,
  PONG:        0x06,
  AGENT_CMD:   0x07,
  AGENT_RESP:  0x08,
};

const PIPE_NAME = '\\\\.\\pipe\\rawrxd-main';

// ─── CRC (mirrors rawr_crc32_xor in rawr_ipc.asm) ────────────────────────────
function rxipCrc32(buf) {
  let crc = 0;
  const dwords = (buf.length / 4) | 0;
  for (let i = 0; i < dwords; i++) {
    crc = (crc ^ buf.readUInt32LE(i * 4)) >>> 0;
  }
  const rem = buf.length & 3;
  for (let i = 0; i < rem; i++) {
    crc = (crc ^ buf[dwords * 4 + i]) >>> 0;
  }
  return crc;
}

// ─── Frame builder ────────────────────────────────────────────────────────────
function buildFrame(type, payloadBuf) {
  const size  = payloadBuf ? payloadBuf.length : 0;
  const frame = Buffer.allocUnsafe(16 + size);
  frame.writeUInt32LE(RXIP_MAGIC,          0);
  frame.writeUInt32LE(type,                4);
  frame.writeUInt32LE(size,                8);
  frame.writeUInt32LE(size > 0 ? rxipCrc32(payloadBuf) : 0, 12);
  if (size > 0) payloadBuf.copy(frame, 16);
  return frame;
}

// ─── Candidate exe locations ──────────────────────────────────────────────────
function nativeExePaths(appRoot) {
  const base = path.resolve(appRoot, '..');
  return [
    path.join(base, 'build_smoke_verify2', 'RawrXD-Win32IDE.exe'),
    path.join(base, 'rxdn',                'RawrXD-Win32IDE.exe'),
    path.join(base, 'build',    'Release', 'RawrXD-Win32IDE.exe'),
    path.join(base, 'build_win32ide', 'Release', 'RawrXD-Win32IDE.exe'),
    path.join(base,                    'RawrXD-Win32IDE.exe'),
  ].filter(p => { try { return fs.existsSync(p); } catch { return false; } });
}

// ─── Bridge class ─────────────────────────────────────────────────────────────
class Win32Bridge {
  constructor(options = {}) {
    this._appRoot     = options.appRoot  || path.join(__dirname, '..');
    this._pipeName    = options.pipeName || PIPE_NAME;
    this._client      = null;
    this._readBuf     = Buffer.alloc(0);
    this._pending     = new Map();   // reqId → { resolve, reject, timer }
    this._reqId       = 0;
    this._state       = 'disconnected'; // disconnected | connecting | connected | failed
    this._nativeProc  = null;
    this._connectP    = null;
  }

  // ── Public API (matches WasmInferenceRuntime shape used by providers.js) ────

  async initialize() {
    if (this._state === 'connected') return this.getStatus();
    if (this._connectP)              return this._connectP;
    this._connectP = this._doConnect();
    return this._connectP;
  }

  /**
   * Invoke the Win32 inference engine.
   * Falls back automatically if the pipe is not connected (providers.js local engine takes over).
   */
  async invoke(prompt, ctx) {
    if (this._state !== 'connected') {
      await this.initialize();
    }
    if (this._state !== 'connected') {
      throw new Error('Win32 bridge unavailable — local fallback active');
    }
    return this._sendRequest(FRAME_TYPE.AI_INVOKE, { prompt, ctx });
  }

  getStatus() {
    return {
      ready:          this._state === 'connected',
      state:          this._state,
      pending:        this._pending.size,
      nativePid:      this._nativeProc ? this._nativeProc.pid : null,
      // Legacy compat fields expected by preload wasm:status consumers
      initialized:    this._state !== 'disconnected',
      error:          this._state === 'failed' ? 'Win32 bridge failed to connect' : null,
      exports:        { hasMemory: false, hasChat: this._state === 'connected' },
      source:         this._pipeName,
    };
  }

  shutdown() {
    for (const [, { reject, timer }] of this._pending) {
      clearTimeout(timer);
      reject(new Error('Win32 bridge shutting down'));
    }
    this._pending.clear();

    if (this._client) {
      try { this._client.destroy(); } catch { /* ignore */ }
      this._client = null;
    }
    if (this._nativeProc) {
      try { this._nativeProc.kill(); } catch { /* ignore */ }
      this._nativeProc = null;
    }
    this._state    = 'disconnected';
    this._connectP = null;
  }

  // ── Private ─────────────────────────────────────────────────────────────────

  async _doConnect() {
    this._state = 'connecting';

    // Spawn the native exe so it stands up its pipe server (if we can find it)
    await this._maybeSpawnNative();

    const status = await this._tryConnect(6, 450);
    this._connectP = null;   // allow retry on next call
    return status;
  }

  async _maybeSpawnNative() {
    if (this._nativeProc) return;
    const exes = nativeExePaths(this._appRoot);
    if (!exes.length) return;
    try {
      this._nativeProc = spawn(exes[0], ['--pipe-server'], {
        detached:     false,
        stdio:        'ignore',
        windowsHide:  true,
      });
      this._nativeProc.unref();
      this._nativeProc.on('exit', () => { this._nativeProc = null; });
      // Give the server time to call CreateNamedPipeW + ConnectNamedPipe
      await new Promise(r => setTimeout(r, 900));
    } catch {
      this._nativeProc = null;
    }
  }

  _tryConnect(maxAttempts, delayMs) {
    return new Promise((resolve) => {
      const attempt = (n) => {
        if (n > maxAttempts) {
          this._state = 'failed';
          resolve(this.getStatus());
          return;
        }

        const sock = net.createConnection(this._pipeName);
        sock.once('connect', () => {
          this._client = sock;
          this._state  = 'connected';
          sock.on('data',  chunk => this._onData(chunk));
          sock.on('error', ()    => this._onDisconnect());
          sock.on('close', ()    => this._onDisconnect());
          resolve(this.getStatus());
        });
        sock.once('error', () => {
          sock.destroy();
          setTimeout(() => attempt(n + 1), delayMs);
        });
      };
      attempt(1);
    });
  }

  _onDisconnect() {
    this._state    = 'disconnected';
    this._client   = null;
    this._connectP = null;
    for (const [, { reject, timer }] of this._pending) {
      clearTimeout(timer);
      reject(new Error('Win32 bridge pipe disconnected'));
    }
    this._pending.clear();
  }

  _onData(chunk) {
    this._readBuf = Buffer.concat([this._readBuf, chunk]);
    // Parse as many complete RXIP frames as possible
    for (;;) {
      if (this._readBuf.length < 16) break;

      const magic = this._readBuf.readUInt32LE(0);
      if (magic !== RXIP_MAGIC) {
        // Resync: scan forward for magic
        let i = 1;
        for (; i <= this._readBuf.length - 4; i++) {
          if (this._readBuf.readUInt32LE(i) === RXIP_MAGIC) break;
        }
        this._readBuf = this._readBuf.slice(i);
        continue;
      }

      const size = this._readBuf.readUInt32LE(8);
      if (this._readBuf.length < 16 + size) break;

      const type    = this._readBuf.readUInt32LE(4);
      const payload = this._readBuf.slice(16, 16 + size);
      this._readBuf = this._readBuf.slice(16 + size);
      this._dispatch(type, payload);
    }
  }

  _dispatch(type, payload) {
    if (type === FRAME_TYPE.PONG) return;   // heartbeat — ignore
    let msg;
    try {
      msg = JSON.parse(payload.toString('utf8'));
    } catch {
      return; // malformed JSON payload
    }
    const { id } = msg;
    if (id === undefined || !this._pending.has(id)) return;
    const { resolve, timer } = this._pending.get(id);
    this._pending.delete(id);
    clearTimeout(timer);
    resolve(msg.result !== undefined ? msg.result : msg);
  }

  _sendRequest(type, body) {
    const id    = ++this._reqId;
    const frame = buildFrame(type, Buffer.from(JSON.stringify({ id, ...body }), 'utf8'));

    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this._pending.delete(id);
        reject(new Error(`Win32 bridge request ${id} timed out`));
      }, 30_000);

      this._pending.set(id, { resolve, reject, timer });
      try {
        this._client.write(frame);
      } catch (e) {
        this._pending.delete(id);
        clearTimeout(timer);
        reject(e);
      }
    });
  }
}

// ─── Factory ─────────────────────────────────────────────────────────────────
function createWin32Bridge(options) {
  return new Win32Bridge(options);
}

module.exports = { createWin32Bridge, FRAME_TYPE, RXIP_MAGIC };
