// llama-worker.js — real llama.cpp WASM runtime worker (single-core, no BLAS)
// Expects a WASM bundle "llama.wasm" and minimal runtime "llama.runtime.js" (initLLamaWasm).
// Public API via postMessage: {cmd:"init"}, {cmd:"load", fileHandle}, {cmd:"prompt", text, opts}

let L = null;            // llama runtime bindings
let ctx = null;          // llama context
let sabOut = null;       // SharedArrayBuffer ring for tokens
let sabCtrl = null;      // SharedArrayBuffer for control (head/tail/codes)
let ringView = null;     // Uint32Array view over sabOut
let ctrlView = null;     // Int32Array view over sabCtrl
let tokenBufSize = 8192; // token ring entries
let modelBytes = null;   // GGUF buffer in memory
let modelPath = "/models/model.gguf"; // mounted path in WASMFS
let nativeCache = new Map(); // PR8-style native module cache (optional)

function post(e){ self.postMessage(e); }
function ok(msg){ post({type:"ok", msg}); }
function err(msg){ post({type:"err", msg}); }

async function mountFS() {
  // wasmfs is provided by llama.runtime.js after init
  // ensure dirs exist
  L.fs.mkdir("/models"); L.fs.mkdir("/tmp"); L.fs.mkdir("/home");
}

async function readFileHandle(handle){
  const file = await handle.getFile();
  const buf = await file.arrayBuffer();
  return new Uint8Array(buf);
}

function setupSAB(){
  try {
    sabOut = new SharedArrayBuffer(tokenBufSize * 4); // u32 tokens
    sabCtrl = new SharedArrayBuffer(16);              // head(0)/tail(1)/status(2)/done(3)
    ringView = new Uint32Array(sabOut);
    ctrlView = new Int32Array(sabCtrl);
    post({ type:"sab-ready", sabOut, sabCtrl, tokenBufSize });
    ok("SAB streaming enabled");
    return true;
  } catch {
    ok("SAB unavailable; falling back to postMessage");
    return false;
  }
}

async function streamToken(tokenId, textPiece){
  // SAB first
  if (ringView){
    const head = Atomics.load(ctrlView, 0);
    const next = (head + 1) % tokenBufSize;
    Atomics.store(ringView, head, tokenId >>> 0);
    Atomics.store(ctrlView, 0, next);                 // head
    Atomics.notify(ctrlView, 0, 1);
  } else {
    post({ type:"token", id: tokenId, text: textPiece });
  }
}

async function runPrompt(text, opts={}){
  if (!ctx) return err("No context; load a model first");
  const {
    max_tokens = 256,
    temperature = 0.7,
    top_p = 0.9,
    stop = []
  } = opts;

  Atomics.store(ctrlView||new Int32Array(4), 2, 1); // status: running
  const it = L.generate(ctx, text, { max_tokens, temperature, top_p, stop });

  for await (const t of it) {
    await streamToken(t.id, t.text||"");
    if (ringView){ /* hint consumers */ }
  }

  Atomics.store(ctrlView||new Int32Array(4), 3, 1); // done
  post({ type:"done" });
}

/* ======= PR8-enhanced init (ABI-compatible) ======= */
async function initLlamaWasm(wasmUrl) {
  // 1. Compile + instantiate with async pipeline
  const resp = await fetch(wasmUrl);
  const compiled = WebAssembly.compileStreaming
    ? await WebAssembly.compileStreaming(resp)
    : await WebAssembly.compile(await resp.arrayBuffer());

  const instance = await WebAssembly.instantiate(compiled, {
    wasi_snapshot_preview1: {},
    env: {
      memory: new WebAssembly.Memory({ initial: 256, maximum: 512 })
    }
  });

  const exports = instance.exports;

  // Helpers to move strings between JS and WASM
  function strToPtr(str) {
    if (!exports._malloc || !exports.memory) {
      throw new Error('WASM exports missing _malloc or memory');
    }
    const buf = new TextEncoder().encode(str + '\0');
    const ptr = exports._malloc(buf.length);
    new Uint8Array(exports.memory.buffer, ptr, buf.length).set(buf);
    return ptr;
  }

  function ptrToStr(ptr) {
    if (!ptr || !exports.memory) return '';
    const mem = new Uint8Array(exports.memory.buffer);
    let end = ptr;
    while (mem[end] !== 0) end++;
    return new TextDecoder().decode(mem.slice(ptr, end));
  }

  // 2. Build the exact shape llama-worker expects
  const Lwrapped = {
    fs: {
      mkdir: (path) => {
        // Optional: if your WASM exposes a POSIX-style mkdir wrapper
        if (exports._mkdir && exports.memory) {
          const p = strToPtr(path);
          const rc = exports._mkdir(p);
          return rc;
        }
        // Fallback: no-op if not provided
        return 0;
      },
      writeFile: (path, data) => {
        if (!exports._write_file || !exports._malloc || !exports._free || !exports.memory) {
          throw new Error('writeFile not supported by WASM exports');
        }
        const ptr = exports._malloc(data.byteLength);
        new Uint8Array(exports.memory.buffer, ptr, data.byteLength).set(data);
        const pathPtr = strToPtr(path);
        exports._write_file(ptr, data.byteLength, pathPtr);
        exports._free(ptr);
      }
    },
    createContext: (opts) => {
      const nCtx = opts.n_ctx ?? 4096;
      const seed = opts.seed ?? -1;
      if (!exports._llama_init_from_file) {
        throw new Error('llama_init_from_file export not found');
      }
      const modelPtr = strToPtr(opts.model);
      const ctx = exports._llama_init_from_file(modelPtr, nCtx, seed);
      if (!ctx) throw new Error('llama_init_from_file failed');
      return ctx;
    },
    // Async iterator adapter around your low-level token API
    generate: async function* (ctxHandle, prompt, opts = {}) {
      const maxTokens = opts.max_tokens ?? 256;
      if (!exports._llama_generate || !exports._llama_next_token || !exports._llama_token_to_piece) {
        throw new Error('llama generate/token exports not found');
      }
      const promptPtr = strToPtr(prompt || '');
      const it = exports._llama_generate(ctxHandle, promptPtr, maxTokens);
      try {
        while (true) {
          const tok = exports._llama_next_token(it);
          if (!tok) break;
          const piecePtr = exports._llama_token_to_piece(ctxHandle, tok);
          const text = ptrToStr(piecePtr);
          yield { id: tok, text };
          // allow cooperative scheduling
          await 0;
        }
      } finally {
        if (exports._llama_free_iterator) {
          exports._llama_free_iterator(it);
        }
      }
    }
  };

  // 3. Optional PR8 bookkeeping (can be extended later)
  if (!self.nativeCache) self.nativeCache = new Map();
  self.nativeCache.set(compiled, { weak_ptr: compiled, isolates: new Set() });

  return Lwrapped;
}

// Preserve the original initRuntime signature but use PR8 loader
async function initRuntime(urls){
  try {
    const wasmUrl = (urls && urls.wasm) || './llama.wasm';
    L = await initLlamaWasm(wasmUrl);
    await mountFS();
    ok("llama.cpp WASM initialized (PR8)");
  } catch (e) {
    err("Init failed: " + e.message);
  }
}

async function loadModelFromHandle(handle){
  try {
    modelBytes = await readFileHandle(handle);
    L.fs.writeFile(modelPath, modelBytes);
    ctx = L.createContext({ model: modelPath, n_ctx: 4096, seed: 42 });
    ok("Model loaded: " + modelPath);
  } catch (e) {
    err("Model load failed: " + e.message);
  }
}

self.onmessage = async (ev) => {
  const { cmd, urls, fileHandle, text, opts } = ev.data || {};
  try {
    if (cmd === "init"){
      const sab = setupSAB();
      await initRuntime(urls);
      post({ type:"ready", sab });
    } else if (cmd === "load"){
      await loadModelFromHandle(fileHandle);
      post({ type:"loaded" });
    } else if (cmd === "prompt"){
      await runPrompt(text, opts||{});
    } else if (cmd === "stop"){
      // optional cooperative cancellation
      Atomics.store(ctrlView||new Int32Array(4), 2, 0);
      post({ type:"stopped" });
    } else {
      err("Unknown cmd: " + cmd);
    }
  } catch (e) {
    err(e.message);
  }
};
