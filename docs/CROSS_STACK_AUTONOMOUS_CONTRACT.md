# Cross-stack autonomous contract (E06)

Last updated: 2026-03-21.

Shared rules for **Win32 native IDE** and **Electron shell (`bigdaddyg-ide`)** so agentic runs stay predictable, gated, and auditable.

| Concern | Win32 | Electron (`bigdaddyg-ide`) |
|--------|--------|----------------------------|
| **Plan approval** | `Win32IDE` plan dialog + `full_agentic_ide::AgenticPlanningOrchestrator` gates; `Agentic::AgenticPlanningOrchestrator` queue for JSON/tool parity | `AgenticPlanningOrchestrator` (`src/agentic/agentic_planning_orchestrator.js`) — `requirePlanApproval`, `batchApproveMutations`, rollback snapshots |
| **Tool / disk mutations** | `ExecutionPlan` steps carry JSON `actions` → `m_toolExecFn` → `Win32IDE::executeAgentPlanStepViaBridge` (agent bridge) | `orchestrator.js` / runtime `writeFile`, `deleteFile`, `readFile` under project root guard (`isPathAllowed`) |
| **Inference façade** | **`routeToLocalGGUF`** = integrated **BGzipXD** stack (WASM-parity loader+runner on Win32 — **`docs/BGZIPXD_WASM.md`**); optional **`routeToOllama`** etc. | **`ai:invoke`** + **`AIProviderManager`** (Ollama HTTP only — **not** your WASM runner); local-first toolbar when manifest OK |
| **Policy** | `config/approval_policy.json` (+ `%APPDATA%\RawrXD\approval_policy.json`) loaded in `OrchestratorIntegration::initialize` | Copilot tab settings → agent policy object on `agent:start` |
| **Audit** | SQLite `agentic_approval_audit` via `recordAgenticApprovalAudit` + `Agentic::agenticAuditEmit` | Task log + optional future IPC export |
| **Idempotency** | Plan step indices stable after parse; re-approve does not duplicate counters (`approveStep` handles prior state) | Task `taskId` map; cancelled tasks do not restart without new `startTask` |
| **IPC / remote** | Local server + command palette | Preload `electronAPI`; IRC bridge (`docs/IRC_MIRC_IDE_BRIDGE.md`) |
| **RE: symbols shell (E10)** | Native panels / future `IDisasmProvider` | Right dock **Symbols** tab + `Ctrl+Shift+Y`; persists stub rows via `knowledge:append-artifact` |

**Omega SDLC phases** (structured logs, Win32): `rawrxd::ide::OmegaSdlcPhase` — `plan` → `mutate` → `verify` → `ship` (see `Win32IDE_PlanExecutor.cpp`).
