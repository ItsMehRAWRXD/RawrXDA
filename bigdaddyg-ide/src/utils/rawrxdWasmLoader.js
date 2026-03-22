/**
 * Chat inference WASM — in-renderer lifecycle only (no HTTP and no host API fallback).
 *
 * Load policy:
 * 1. Remote `http(s)://` URLs are rejected.
 * 2. The default module name resolves to bundled embedded bytes.
 * 3. Non-default paths are rejected in this build mode.
 *
 * @module rawrxdWasmLoader
 */

import { getEmbeddedChatWasmArrayBuffer } from './rawrxdChatWasmEmbedded';

/** @type {Map<string, WebAssembly.Instance>} */
const instanceCache = new Map();
/** @type {Map<string, Promise<WebAssembly.Instance>>} */
const inflight = new Map();

function isRemoteWasmPath(pathOrUrl) {
  return /^https?:\/\//i.test(String(pathOrUrl || '').trim());
}

function normalizedWasmPath(wasmUrlOption) {
  return String(wasmUrlOption || 'rawrxd-inference.wasm').trim() || 'rawrxd-inference.wasm';
}

function isDefaultChatWasmPath(pathOrUrl) {
  const s = normalizedWasmPath(pathOrUrl);
  if (isRemoteWasmPath(s)) return false;
  const base = s.split('?')[0].replace(/\\/g, '/');
  return base === 'rawrxd-inference.wasm' || base.endsWith('/rawrxd-inference.wasm');
}

/**
 * Stable cache key (no fetch URL — avoids implying HTTP).
 * @param {string | undefined} wasmUrlOption
 * @returns {string}
 */
export function wasmInstanceCacheKey(wasmUrlOption) {
  const raw = normalizedWasmPath(wasmUrlOption);
  if (isRemoteWasmPath(raw)) return `rejected:${raw}`;
  if (isDefaultChatWasmPath(raw)) return 'embedded:rawrxd-inference';
  return `rejected-nondefault:${raw}`;
}

/**
 * @deprecated Use wasmInstanceCacheKey — kept for any external imports.
 */
export function resolveWasmFetchUrl(explicit) {
  return wasmInstanceCacheKey(explicit);
}

/**
 * @param {string | undefined} wasmUrlOption
 * @returns {Promise<ArrayBuffer>}
 */
async function loadWasmArrayBuffer(wasmUrlOption) {
  const raw = normalizedWasmPath(wasmUrlOption);

  if (isRemoteWasmPath(raw)) {
    throw new Error(
      'Chat WASM does not load over HTTP. Use the embedded default module name only.'
    );
  }

  if (!isDefaultChatWasmPath(raw)) {
    throw new Error(
      `Chat WASM path "${raw}" is not allowed in this build mode. Use the embedded rawrxd-inference module name.`
    );
  }

  return getEmbeddedChatWasmArrayBuffer();
}

export function clearRawrxdWasmInferenceCache() {
  instanceCache.clear();
  inflight.clear();
}

/**
 * @param {string | undefined} wasmUrlOption
 * @returns {Promise<WebAssembly.Instance>}
 */
export async function getOrInstantiateChatWasm(wasmUrlOption) {
  const cacheKey = wasmInstanceCacheKey(wasmUrlOption);

  const hit = instanceCache.get(cacheKey);
  if (hit) return hit;

  const existing = inflight.get(cacheKey);
  if (existing) return existing;

  const pending = (async () => {
    try {
      const buf = await loadWasmArrayBuffer(wasmUrlOption);
      let compiled;
      try {
        compiled = await WebAssembly.compile(buf);
      } catch (e) {
        throw new Error(`WASM compile failed: ${e.message}`);
      }
      const instance = await WebAssembly.instantiate(compiled, {});
      instanceCache.set(cacheKey, instance);
      return instance;
    } finally {
      inflight.delete(cacheKey);
    }
  })();

  inflight.set(cacheKey, pending);
  return pending;
}
