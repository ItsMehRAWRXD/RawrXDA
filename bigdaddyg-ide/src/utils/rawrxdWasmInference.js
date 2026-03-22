/**
 * Renderer-side BGzipXD WASM lane for chat — module bytes from embedded bundle or Electron disk read only (no HTTP fetch).
 * Default artifact: `public/rawrxd-inference.wasm` (rebuild: `npm run build:wasm-inference`).
 *
 * Host contract: same prompt string shape as `ai:invoke` / Ollama single user message — UTF-8 bytes
 * in the module input region; UTF-8 assistant text in the output region (bootstrap = byte echo).
 *
 * ABI v2: module exports `rawrxd_input_base`, `rawrxd_input_cap`, `rawrxd_output_base`, `rawrxd_output_cap`,
 * `rawrxd_abi_version` so the host cannot desync from the `.wat` / `.wasm` layout.
 *
 * M07 — Echo bootstrap → ok:true, lane wasm-echo-stub, empty content (ChatPanel shows finished WASM-only message; no host API).
 * Generative WASM → ok:true, lane wasm-rawrxd, content is model output only.
 */

import { clearRawrxdWasmInferenceCache as clearWasmLoaderCache, getOrInstantiateChatWasm } from './rawrxdWasmLoader';

/** Fallbacks if an older wasm lacks ABI exports */
const FALLBACK_INPUT_BASE = 0;
const FALLBACK_INPUT_CAP = 32768;
const FALLBACK_OUTPUT_BASE = 65536;
const FALLBACK_OUTPUT_CAP = 65536;

function readAbi(exports) {
  const ver =
    typeof exports.rawrxd_abi_version === 'function' ? exports.rawrxd_abi_version() | 0 : 1;
  const inputBase =
    typeof exports.rawrxd_input_base === 'function' ? exports.rawrxd_input_base() | 0 : FALLBACK_INPUT_BASE;
  const inputCap =
    typeof exports.rawrxd_input_cap === 'function'
      ? Math.max(256, exports.rawrxd_input_cap() | 0)
      : FALLBACK_INPUT_CAP;
  const outputBase =
    typeof exports.rawrxd_output_base === 'function'
      ? exports.rawrxd_output_base() | 0
      : FALLBACK_OUTPUT_BASE;
  const outputCap =
    typeof exports.rawrxd_output_cap === 'function'
      ? Math.max(256, exports.rawrxd_output_cap() | 0)
      : FALLBACK_OUTPUT_CAP;
  return { ver, inputBase, inputCap, outputBase, outputCap };
}

export function clearRawrxdWasmInferenceCache() {
  clearWasmLoaderCache();
}

/**
 * @param {string} fullPrompt — Same concatenation as HTTP lane (persona + context + User: …).
 * @param {{ wasmUrl?: string, maxTokens?: number }} [options]
 * @returns {Promise<{ ok: true, content: string, lane: string, abiVersion?: number } | { ok: false, error: string }>}
 */
export async function invokeRawrxdWasmChat(fullPrompt, options = {}) {
  const text = String(fullPrompt ?? '');
  const te = new TextEncoder();
  const td = new TextDecoder('utf-8', { fatal: false });

  let instance;
  try {
    instance = await getOrInstantiateChatWasm(options.wasmUrl);
  } catch (e) {
    return {
      ok: false,
      error: `${e.message} — Run npm run build:wasm-inference && node scripts/sync-chat-wasm-embed.js, or open via Electron for disk-loaded wasm.`
    };
  }

  const exports = instance.exports;
  const chat = exports.rawrxd_chat;
  const memory = exports.memory;
  if (typeof chat !== 'function' || !memory) {
    return {
      ok: false,
      error: 'rawrxd-inference.wasm is missing exports `memory` and `rawrxd_chat`. Rebuild from public/rawrxd-inference.wat.'
    };
  }

  const abi = readAbi(exports);
  const payload = te.encode(text);
  if (payload.length === 0) {
    return { ok: true, lane: 'wasm-echo-stub', content: '', abiVersion: abi.ver };
  }
  if (payload.length > abi.inputCap) {
    return {
      ok: false,
      error: `Prompt exceeds WASM input cap (${abi.inputCap} bytes). Shorten the message or set Settings › AI → Chat transport → Ollama HTTP.`
    };
  }
  if (abi.inputBase + payload.length > memory.buffer.byteLength) {
    return { ok: false, error: 'WASM memory too small for input region — rebuild rawrxd-inference.wasm with larger initial pages.' };
  }
  if (abi.outputBase + abi.outputCap > memory.buffer.byteLength) {
    return { ok: false, error: 'WASM memory too small for output region — rebuild rawrxd-inference.wasm with larger initial pages.' };
  }

  let mem = new Uint8Array(memory.buffer, 0, memory.buffer.byteLength);
  mem.set(payload, abi.inputBase);

  const tokenBudget = Number(options.maxTokens) || 512;
  const desiredOut = Math.max(256, Math.min(abi.outputCap, tokenBudget * 6));
  const maxOut = Math.min(abi.outputCap, Math.max(payload.length, desiredOut));

  const written = chat(abi.inputBase, payload.length, abi.outputBase, maxOut);
  if (written <= 0) {
    return { ok: false, error: 'WASM rawrxd_chat returned no bytes.' };
  }

  mem = new Uint8Array(memory.buffer, 0, memory.buffer.byteLength);
  const slice = mem.subarray(abi.outputBase, abi.outputBase + written);
  const body = td.decode(slice);

  const norm = (s) => s.replace(/\r\n/g, '\n').trimEnd();
  const outNorm = norm(body);
  const inNorm = norm(text);
  if (outNorm === inNorm) {
    return {
      ok: true,
      lane: 'wasm-echo-stub',
      content: '',
      abiVersion: abi.ver
    };
  }
  return {
    ok: true,
    lane: 'wasm-rawrxd',
    content: body,
    abiVersion: abi.ver
  };
}
