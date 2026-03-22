# Electron shell — WASM chat lane (no Ollama for dock chat)

**Ground truth in code**

| Piece | Role |
|--------|------|
| `bigdaddyg-ide/src/utils/rawrxdWasmInference.js` | Loads WASM in the **renderer**, calls `rawrxd_chat` + linear `memory`. |
| `bigdaddyg-ide/src/components/ChatPanel.js` | Uses WASM when **Settings › AI › Chat transport** = **Local WASM** (`settings.chatTransport !== 'ollama-http'`). |
| `bigdaddyg-ide/public/rawrxd-inference.wat` | Source for the stub module; compile with `npm run build:wasm-inference` → `rawrxd-inference.wasm`. |
| `bigdaddyg-ide/src/agentic/providers.js` | **Ollama-compatible HTTP** for main-process `ai:invoke` (agent, HTTP chat). |

## User flow

1. **Default:** Chat transport = **Local WASM** — dock chat does **not** open `127.0.0.1:11434`.
2. **Optional:** Switch to **Ollama-compatible HTTP** if you run Ollama (or any server) and want chat via `ai:invoke`.
3. **WASM URL:** Settings field `wasmChatUrl` (default `/rawrxd-inference.wasm`) — relative to the page origin, or an absolute `http(s)` URL.

## WASM export contract (stub today)

- `memory`: `WebAssembly.Memory` (shared linear buffer).
- `rawrxd_chat(inPtr, inLen, outPtr, outCap) -> i32` — copies prompt UTF-8 through memory; returns byte count written to output.

Replace `public/rawrxd-inference.wasm` with a **BGzipXD** build when the full runner is available; keep the same export names or update `rawrxdWasmInference.js` in one place.

## Related

- `docs/BGZIPXD_WASM.md` — stack identity (loader + runner + codec + RE).
- `docs/INFERENCE_PATH_MATRIX.md` — Win32 native vs Electron vs WASM lanes.
- `docs/LOCALHOST_COMMUNICATION_MAP.md` — who listens on loopback when HTTP is enabled.
