# Reverse-engineer stubs → closure (shell)

**Intent:** Stub-like behavior (echo wasm, IPC shims, caches) should keep **private state** inside a **lexical closure**, not module-level `let` that anything could confuse with public API.

## Implemented

| Surface | Pattern |
|---------|---------|
| `src/utils/rawrxdWasmInference.js` | `createRawrxdWasmChatRuntime()` returns `{ invokeRawrxdWasmChat, clearRawrxdWasmInferenceCache }`; module exports a **singleton** `runtime` so imports stay stable. |
| `electron/preload.js` | IIFE builds `electronAPI` with inner `invoke(channel, …)` closing over `ipcRenderer`; `contextBridge.exposeInMainWorld` runs once at the end. |

## When to use

- **Caches** (wasm instance, URL key) → closure + explicit `clear*`.
- **IPC facades** → single `invoke` helper in closure reduces typo drift vs scattered `ipcRenderer.invoke`.
- **Tests (future):** `createRawrxdWasmChatRuntime()` can be called twice for isolation; production uses one singleton.

## Related

- `docs/ELECTRON_WASM_CHAT.md`
- `docs/UNSTUB_BATCHES_SHELL.md`
