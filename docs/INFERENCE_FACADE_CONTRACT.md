# Inference façade contract (E01)

**Goal:** One mental model for “user action → inference backend” with **no silent bypass** of the primary router and a **single failure channel** per surface.

## Canonical Win32 entry

| Concern | Owner | Contract |
|--------|--------|----------|
| Chat / completion routing | **BackendSwitcher** | `routeInferenceRequest()` → `routeToLocalGGUF` \| `routeToOllama` \| `routeToOpenAI` \| … per **explicit** active backend. **`routeToLocalGGUF`** is the **integrated** path: **BGzipXD** loader+runner (WASM-parity stack on Win32), **not** a thin wrapper around Ollama. |
| Orchestrator / FIM / ghost | **OrchestratorBridge** | Ollama-shaped host; **must** stay in sync via `syncOrchestratorBridgeFromBackendManager()`. |
| Agent-panel model drift | **Win32IDE_AgenticBridge** | Mutations go through backend switcher + sync helper. |

Failures **must** surface through:

- **IDELogger** (or equivalent) with a **stable tag** (e.g. `[INFERENCE]`, `[BACKEND]`), and  
- **User-visible** string on the same code path (status bar / chat error), not a swallowed `catch`.

## Matrix source of truth

Implementation details and row-level mapping: **`docs/INFERENCE_PATH_MATRIX.md`**.  
If code adds a new lane, **update the matrix** in the same PR (E01 acceptance).

## Error codes (recommended)

Use a small enum or string prefix so support can grep logs:

- `INF_ROUTE_NO_BACKEND` — no active backend selected  
- `INF_GGUF_NOT_LOADED` — local GGUF path empty or load failed  
- `INF_OLLAMA_HTTP` — HTTP error from Ollama-compatible endpoint  
- `INF_ORCH_SYNC` — orchestrator config out of sync with backend manager  

(Win32: wire gradually; this doc is the contract target.)
