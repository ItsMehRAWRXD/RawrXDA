/**
 * Default chat inference WASM bytes — bundled so the renderer never uses HTTP `fetch()` to load this module.
 * Regenerate after `npm run build:wasm-inference`:
 *   node scripts/sync-chat-wasm-embed.js
 */
export const RAW_RXD_CHAT_WASM_BASE64 =
  'AGFzbQEAAAABDQJgAAF/YAR/f39/AX8DBwYAAAAAAAEFAwEAAgd9BwZtZW1vcnkCABJyYXdyeGRfYWJpX3ZlcnNpb24AABFyYXdyeGRfaW5wdXRfYmFzZQABEHJhd3J4ZF9pbnB1dF9jYXAAAhJyYXdyeGRfb3V0cHV0X2Jhc2UAAxFyYXdyeGRfb3V0cHV0X2NhcAAEC3Jhd3J4ZF9jaGF0AAUKYAYEAEECCwQAQQALBgBBgIACCwYAQYCABAsGAEGAgAQLPwECfyABIANLBH8gAwUgAQshBUEAIQQCQANAIAQgBU8NASACIARqIAAgBGotAAA6AAAgBEEBaiEEDAALCyAFCw==';

export function getEmbeddedChatWasmArrayBuffer() {
  const bin = atob(RAW_RXD_CHAT_WASM_BASE64);
  const u8 = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i += 1) u8[i] = bin.charCodeAt(i);
  return u8.buffer.slice(u8.byteOffset, u8.byteOffset + u8.byteLength);
}
