// llama.runtime.js  – bare-bones loader for llama.cpp WASM (single-core, browser build)
// exports: initLLamaWasm(wasmUrl) -> { fs, createContext, generate }

const MEM_PAGES = 2048;          // 128 MB initial
const MAX_PAGES = 16384;         // 1 GB max

let wasm, fs, instance;

// ---------- WASI stubs that llama.cpp actually needs ----------
const wasi = {
  proc_exit: (code) => { throw new Error("WASI proc_exit " + code); },
  fd_write: (fd, iovs, iovsLen, nwritten) => {        // stdout/stderr -> console
    const HEAP8 = new Uint8Array(instance.exports.memory.buffer);
    let written = 0;
    for (let i = 0; i < iovsLen; i++) {
      const ptr = HEAP8[iovs + i * 8] | (HEAP8[iovs + i * 8 + 1] << 8);
      const len = HEAP8[iovs + i * 8 + 4] | (HEAP8[iovs + i * 8 + 5] << 8);
      const str = new TextDecoder().decode(HEAP8.slice(ptr, ptr + len));
      if (fd === 1) console.log("[llama] " + str);
      else console.warn("[llama] " + str);
      written += len;
    }
    (new Uint32Array(instance.exports.memory.buffer))[nwritten >> 2] = written;
    return 0;
  },
  fd_close: () => 0,
  fd_fdstat_get: () => 0,
  random_get: (buf, bufLen) => {
    crypto.getRandomValues(new Uint8Array(instance.exports.memory.buffer, buf, bufLen));
    return 0;
  }
};

// ---------- Minimal WASMFS (enough for llama) ----------
class WASMFS {
  constructor(mem) {
    this.mem = new Uint8Array(mem);
    this.files = new Map();
    this.dirs = new Set(["/", "/models", "/tmp", "/home"]);
  }
  writeFile(path, data) {
    this.files.set(path, new Uint8Array(data));
  }
  readFile(path) {
    return this.files.get(path);
  }
  mkdir(path) {
    this.dirs.add(path);
  }
}

// ---------- Async generator that yields tokens ----------
async function* generate(ctx, prompt, opts = {}) {
  const { max_tokens = 256, temperature = 0.7, top_p = 0.9, stop = [] } = opts;
  const stopSet = new Set(stop);
  const promptBuf = new TextEncoder().encode(prompt);
  const promptPtr = instance.exports.malloc(promptBuf.length);
  new Uint8Array(instance.exports.memory.buffer).set(promptBuf, promptPtr);

  const evalFn = instance.exports.llama_eval;
  const logits = instance.exports.llama_get_logits(ctx);
  const vocab = instance.exports.llama_n_vocab(ctx);
  const nctx = instance.exports.llama_n_ctx(ctx);

  let n_past = 0;
  let tokens = [];
  for (let i = 0; i < promptBuf.length; i++) tokens.push(promptBuf[i] & 0xFF);

  // feed prompt
  for (let i = 0; i < tokens.length; i += 32) {
    const batch = tokens.slice(i, i + 32);
    const ptr = instance.exports.malloc(batch.length * 4);
    new Uint32Array(instance.exports.memory.buffer, ptr, batch.length).set(batch);
    evalFn(ctx, ptr, Math.min(32, batch.length), n_past);
    n_past += batch.length;
    instance.exports.free(ptr);
  }

  // generate
  for (let i = 0; i < max_tokens; i++) {
    const ptr = instance.exports.malloc(4);
    const id = instance.exports.llama_sample_top_p_top_k(ctx, logits, vocab, temperature, top_p);
    new Uint32Array(instance.exports.memory.buffer, ptr, 1)[0] = id;
    evalFn(ctx, ptr, 1, n_past);
    n_past++;
    const piece = new TextDecoder().decode(new Uint8Array([id])); // naive – use llama_token_to_piece in prod
    yield { id, text: piece };
    if (stopSet.has(piece)) break;
    instance.exports.free(ptr);
  }
}

// ---------- Public API ----------
export async function initLLamaWasm(wasmUrl) {
  const { instance: inst } = await WebAssembly.instantiateStreaming(fetch(wasmUrl), {
    wasi_snapshot_preview1: wasi,
    env: {
      memory: new WebAssembly.Memory({ initial: MEM_PAGES, maximum: MAX_PAGES, shared: false }),
      emscripten_memcpy_big: (d, s, n) => {
        const mem = new Uint8Array(inst.exports.memory.buffer);
        mem.set(mem.slice(s, s + n), d);
      }
    }
  });
  instance = inst;
  fs = new WASMFS(inst.exports.memory.buffer);
  return {
    fs,
    createContext: (opts) => {
      const { model, n_ctx = 512, seed = 42 } = opts;
      const mPath = new TextEncoder().encode(model + '\0');
      const ptr = inst.exports.malloc(mPath.length);
      new Uint8Array(inst.exports.memory.buffer).set(mPath, ptr);
      const ctx = inst.exports.llama_init_from_file(ptr, n_ctx, seed);
      inst.exports.free(ptr);
      if (!ctx) throw new Error("llama_init_from_file failed");
      return ctx;
    },
    generate
  };
}
