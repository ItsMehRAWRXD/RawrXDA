#!/usr/bin/env node
/**
 * Rewrites src/utils/rawrxdChatWasmEmbedded.js from public/rawrxd-inference.wasm
 * so the embedded copy stays aligned with wat2wasm output (no HTTP load in renderer).
 */
const fs = require('fs');
const path = require('path');

const root = path.join(__dirname, '..');
const wasmPath = path.join(root, 'public', 'rawrxd-inference.wasm');
const outPath = path.join(root, 'src', 'utils', 'rawrxdChatWasmEmbedded.js');

if (!fs.existsSync(wasmPath)) {
  console.error('Missing', wasmPath, '— run npm run build:wasm-inference first.');
  process.exit(1);
}

const b64 = fs.readFileSync(wasmPath).toString('base64');
const header = `/**
 * Default chat inference WASM bytes — bundled so the renderer never uses HTTP \`fetch()\` to load this module.
 * Regenerate after \`npm run build:wasm-inference\`:
 *   node scripts/sync-chat-wasm-embed.js
 */
export const RAW_RXD_CHAT_WASM_BASE64 =
  '${b64}';

export function getEmbeddedChatWasmArrayBuffer() {
  const bin = atob(RAW_RXD_CHAT_WASM_BASE64);
  const u8 = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i += 1) u8[i] = bin.charCodeAt(i);
  return u8.buffer.slice(u8.byteOffset, u8.byteOffset + u8.byteLength);
}
`;

fs.writeFileSync(outPath, header, 'utf8');
console.log('Wrote', outPath, `(${Buffer.byteLength(b64, 'utf8')} base64 chars)`);
