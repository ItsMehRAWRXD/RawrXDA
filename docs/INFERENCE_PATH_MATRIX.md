# Inference Path Matrix

Last updated: 2026-03-21 (E01 façade + InferenceFacade log field).

This matrix maps major Win32 IDE user actions to their inference/backend execution lane.

**Loopback roles:** **`docs/LOCALHOST_COMMUNICATION_MAP.md`** — who **hosts** vs who **connects** on `127.0.0.1` (IDE client-first; optional Win32 bridge on **11435**).

**E01 — Single façade:** All chat/completion routes must align with this table. **No silent alternate path.** Unified error-channel contract: **`docs/INFERENCE_FACADE_CONTRACT.md`**. Win32 emits **`[InferenceFacade] inference_lane=… op=routeInferenceRequest`** on each façade call (`Win32IDE_BackendSwitcher.cpp`). Electron shell: **`ai:invoke`** → `AIProviderManager` with **`preferLocalInferenceFirst`** + `localGgufManifestOk` (see `IdeFeaturesContext`, `ChatPanel.js`, `electron/main.js`).

| User action (Win32) | Primary lane | Bridge/engine path | Notes |
| --- | --- | --- | --- |
| Chat send (active backend = LocalGGUF) | BackendSwitcher | `routeInferenceRequest()` -> `routeToLocalGGUF()` -> native engine (`generateResponse`) | **Your integrated path (WASM-aligned):** **BGzipXD** stack — loader + runner + codec + RE on Win32 (`GGUFRunner` / native pipeline). Same logical stack as the **WASM** build in **`docs/BGZIPXD_WASM.md`**; **not** Electron’s Ollama HTTP. No `OrchestratorBridge` dependency for primary generation. |
| Chat send (active backend = Ollama) | BackendSwitcher | `routeInferenceRequest()` -> `routeToOllama()` -> Ollama HTTP `/api/generate` | Uses backend config endpoint/model from backend switcher state. |
| Chat send (active backend = OpenAI / Claude / Gemini) | BackendSwitcher | `routeToOpenAI()` / `routeToClaude()` / `routeToGemini()` | Routed by explicit backend selection only; no automatic backend swap. |
| Chat send (active backend = ReasoningEngine) | BackendSwitcher | `routeToReasoningEngine()` -> local reasoning integration | Local analysis lane (non-Ollama). |
| Chat send (active backend = GitHubCopilot / AmazonQ) | BackendSwitcher + VS Code bridge | `routeToGitHubCopilot()` / `routeToAmazonQ()` -> VS Code Extension API command bridge | Extension lane, not OrchestratorBridge lane. |
| Backend endpoint/model change from UI/settings/local server | BackendSwitcher config mutation | `setBackendEndpoint()` / `setBackendModel()` -> `syncOrchestratorBridgeFromBackendManager()` | Sync helper aligns `OrchestratorBridge` host/port/model with backend manager state. |
| Backend switch to Ollama | BackendSwitcher switch lane | `setActiveBackend(Ollama)` -> `syncOrchestratorBridgeFromBackendManager()` | Ensures orchestrator/FIM lane tracks active Ollama config without recursive switching. |
| Settings apply with `aiOllamaUrl` | Settings lane -> BackendSwitcher | `applySettings()` -> `setBackendEndpoint(Ollama, m_ollamaBaseUrl)` (or direct sync fallback) | Keeps settings-driven URL changes in canonical backend config + orchestrator sync path. |
| Win32 app startup backend init | BackendSwitcher init lane | `initBackendManager()` -> `loadBackendConfigs()` -> `syncOrchestratorBridgeFromBackendManager()` | Startup pushes persisted endpoint/model into orchestrator state. |
| Ghost text / FIM completion | OrchestratorBridge | `OrchestratorBridge::RequestGhostText*()` -> `AgentOllamaClient::FIM*()` | FIM uses orchestrator’s Ollama host/port and FIM model, fed by sync helper. |
| Agentic tool-calling run (where used) | OrchestratorBridge | `OrchestratorBridge::RunAgent*()` -> Ollama chat + tool handlers | Uses orchestrator chat model and guardrails; synced from backend manager for endpoint/model drift control. |
| Agent-panel bridge model change callback | AgenticBridge -> BackendSwitcher | `Win32IDE_AgenticBridge` callback -> `setBackendModel(Ollama, ...)` -> sync helper | Keeps agent-panel model changes aligned with orchestrator and backend switcher state. |

## Batch 4 verification notes (non-inference crossings)

| User action (Win32) | Primary lane | Bridge/engine path | Notes |
| --- | --- | --- | --- |
| Open file picker from router | RouterOperations | `OpenFileDialog()` -> `GetOpenFileNameA` | Verified as native Win32 dialog; not part of inference lane. |
| Save file picker from router | RouterOperations | `SaveFileDialog()` -> `GetSaveFileNameA` | Verified as native Win32 dialog; not part of inference lane. |
| Select folder picker from router | RouterOperations | `SelectFolderDialog()` -> `SHBrowseForFolderA` + `SHGetPathFromIDListA` | Verified as native Win32 folder chooser; not part of inference lane. |

## Lane definitions

- `BackendSwitcher routeTo*`: explicit per-backend user-selected inference route for chat requests.
- **BGzipXD / WASM parity (LocalGGUF):** in-repo **integrated** inference — GGUF loader + token runner + codec/transport + RE hooks; Win32 uses native I/O; **WASM** target swaps the platform layer only (**`docs/BGZIPXD_WASM.md`**). This is **not** “call Ollama from the IDE.”
- `OrchestratorBridge`: Ollama-backed orchestrator used for agentic/tool workflows and FIM/ghost text.
- `AgenticBridge`: glue layer that can emit backend mutation calls into backend switcher.

## Batch 5 consistency check

- Verified against live source:
  - `syncOrchestratorBridgeFromBackendManager()` is called from backend init/load/switch + endpoint/model setters.
  - `syncBackendHealthTruthWithOrchestrator()` is called after sync and by readiness gate.
  - Startup readiness gate in `Win32IDE_Core` runs backend/router/bridge checks then sync calls.
