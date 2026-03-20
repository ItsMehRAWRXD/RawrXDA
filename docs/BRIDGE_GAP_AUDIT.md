# RawrXD — Bridge inventory & gap audit

Codebase-oriented notes: who talks to whom, critical gaps, and a practical fix order.  
Last updated: 2026-03-20.

---

## 1. Bridge inventory

| Bridge / layer | Role | Main files |
|----------------|------|------------|
| **OrchestratorBridge** | Ollama chat/FIM, tool loop, discovery | `src/agentic/OrchestratorBridge.*` |
| **AgenticBridge** | Full agentic IDE: model, tools, autonomy, streaming | `Win32IDE_AgenticBridge.*`, `Win32IDE.cpp` |
| **Win32IDEBridge** (agentic) | Capabilities, hotpatch, router; **does not** own OrchestratorBridge (IDE does) | `src/agentic/bridge/Win32IDEBridge.cpp` |
| **Backend switcher** | Active backend, HTTP routes, status | `Win32IDE_BackendSwitcher.cpp` |
| **Unified command dispatch** | SSOT command table → handlers | `unified_command_dispatch.*`, `command_registry.hpp` |
| **win32_feature_adapter** | `routeCommandUnified()` → dispatch | `win32_feature_adapter.h` |
| **Legacy `routeCommand()`** | Huge `switch` / buckets in Win32IDE | `Win32IDE_Commands.cpp`, `Win32IDE_Core.cpp` |
| **Swarm** | MASM/C++/CLI/UI multiple entrypoints | `Win32SwarmBridge`, `src/agentic/coordination/SwarmOrchestrator.cpp`, `src/swarm*`, `src/cli/swarm_orchestrator.cpp` |
| **LSP client** | Language server ↔ diagnostics/peek | `Win32IDE_LSPClient.cpp` |
| **LSP–AI hybrid** | Declared bridge stats/API | `Win32IDE.h` (`initLSPAIBridge`, hybrid stats) |
| **MonacoCore / IEditorEngine** | Native editor vs WebView2/RichEdit | `RawrXD_MonacoCore.h`, `MonacoCoreEngine.cpp`, stubs |
| **Subagent core** | Todo, chain, swarm (portable) | `subagent_core.h` / `subagent_core.cpp` |
| **Win32 subagent UI** | Commands → `SubAgentManager` / output | `Win32IDE_SubAgent.cpp`, `Win32IDE_AgentCommands.cpp` |
| **Extension / VSIX** | Copilot, Amazon Q proxy | `Win32IDE_BackendSwitcher.cpp`, `vscode_extension_api` |
| **Link / ASM stubs** | Satisfy symbols when MASM lane off | `win32_ide_link_stubs.cpp`, `monaco_core_stubs.cpp`, `rawr_engine_link_shims.cpp`, etc. |

---

## 2. Critical gaps (P0)

### A. Dual init / dual truth for the same agent stack

- **Root cause (fixed):** `ensureOrchestratorBridgeInitialized()` had been wired only from **`Win32IDE_Window.cpp`**, which is **not** in the root `CMakeLists.txt` Win32IDE source list. The shipped binary uses **`Win32IDE_Core.cpp`** for `WindowProc` / `onCreate` / `handleMessage`, so orchestrator alignment did not run on the path you ship — only `initBackendManager()`’s push/sync could, which could miss **`m_ollamaBaseUrl`** from `rawrxd.config.json` when the Ollama backend row was still empty.
- **Live path:** `Win32IDE_Core.cpp` **`onCreate`** → `initBackendManager()` → `initLLMRouter()` → **`ensureOrchestratorBridgeInitialized()`**; **`applySettings()`** (after `m_ollamaBaseUrl` is updated); **GGUF background** completion in **`Win32IDE.cpp`** (`handleGgufBackgroundLoadMessage`).
- **Policy (`ensureOrchestratorBridgeInitialized` in `Win32IDE_BackendSwitcher.cpp`):** resolve URL from `m_ollamaBaseUrl` or Ollama backend row; working directory from **`m_currentDirectory`** when set, else CWD; **`pushOllamaBackendToOrchestratorBridge`** / **`syncOllamaBackendFromOrchestratorBridge`** only when **`m_backendManagerInitialized`**; if URL known and bridge already initialized, **`ApplyIdeOllamaSettings(url, "")`** (+ `SetWorkingDirectory`) instead of **`Initialize`** again (avoids wiping models).
- **`Win32IDEBridge::initialize()`** does **not** cold-start **`OrchestratorBridge`** on `127.0.0.1:11434` — **IDE owns lifecycle**; previously that fought persisted backends/settings.
- **`Win32IDE_Window.cpp`:** orphan TU (not linked) — do not treat its **`WM_APP+200`** path as production.
- **Remaining gap:** ensure **every** mutator of Ollama URL/model goes through one policy (grep for stray `SetModel`, direct `OllamaConfig`, duplicate `Initialize` from tools/CLI).

### B. Command routing order vs SSOT narrative

- **`Win32IDE_Core.cpp`:** `routeCommand(id)` runs **before** `routeCommandUnified()`.
- **`unified_command_dispatch.hpp`** describes a no–legacy-fallback story; Win32 does the **opposite** (legacy first).
- **Risk:** IDs handled only in legacy never hit SSOT stats/registry; palette vs menu can diverge; “unknown command” only if **both** miss.
- **Fix options:** register those IDs in `COMMAND_TABLE` and stop handling them in legacy, **or** document Win32 as intentionally **legacy-first** and list legacy-only IDs.

### C. Multiple orchestration / swarm implementations

- **AgentOrchestrator**, **BoundedAgentLoop**, **SwarmOrchestrator** (agentic + swarm + CLI + UI).
- **Risk:** feature flags or menu items call **A** while HTTP/CLI call **B**; status strings don’t match runtime.
- **Gap:** one public façade (e.g. **SwarmSession**) that everything uses, **or** explicit “which build lane owns swarm” in docs + CMake.

### D. Parallel inference paths

- **Backend switcher** → `routeTo*` / `generateResponse` / **native engine**.
- **AgenticBridge** / chat panel may bypass the switcher for some flows.
- **OrchestratorBridge** is a separate path again for Ollama tools/FIM.
- **Gap:** matrix of “user action → which path” is not single; failures can show “model loaded” in one layer and not another.

---

## 3. Medium gaps (P1)

| ID | Topic | Notes |
|----|--------|--------|
| **E** | Subagent / todo truth | `TodoItem` / `SubAgentManager` in `subagent_core`; UI (`Win32IDE_SubAgent.cpp`, e.g. `onSubAgentTodoList`); tool `manage_todo_list` in agentic bridge. **Direction:** one mutex-protected store + `notifyTodoObservers()` from every API entry. |
| **F** | Stub / alternate compilation lanes | `Win32IDE_LinkCompleters.cpp` — empty/stub methods when modules excluded → **silent** loss of peek/search/extensions. Monaco/mesh/streaming `*_stubs.cpp` — same. **Direction:** feature manifest / audit dashboard marks **Stub** when link-lane shims satisfy symbols. |
| **G** | LSP ↔ hybrid bridge | `m_hybridBridgeInitialized`, `initLSPAIBridge` in `Win32IDE.h` — verify wiring depth vs real LSP peek/diagnostics per build. Hybrid stats may be inert while LSP works (or vice versa). |
| **H** | Error model split | OrchestratorBridge: retry + `InferenceResult` + some throw paths in tool/chat loops. Backend switcher: string errors + non-throwing JSON (recent hardening). **Gap:** agent loop vs HTTP layer still **feel** different to users and logs. |
| **I** | Ghost text / FIM | `RequestGhostText` → OrchestratorBridge; editor may be Monaco vs RichEdit. **Gap:** FIM context (path, prefix/suffix) must match **active** engine or behavior works in one layout only. |

---

## 4. Lower priority (P2)

- **J** — CLI/headless: `cli_feature_bridge.h`, `HeadlessIDE`, `OrchestratorBridge` shims in `rawrxd_cli_link_shims.cpp` — parity with Win32 menus not guaranteed.
- **K** — HTTP/extension backends (Copilot, Amazon Q): “triggered, await stream” style; **no shared streaming contract** with chat panel unless explicitly wired.
- **L** — Enterprise/sovereign: `enterprise_devunlock_bridge`, telemetry hooks — often off or stubbed; document as **optional lane**.

---

## 5. Bridges in good shape (relative)

- **Backend switcher ↔ OrchestratorBridge** (Ollama model/endpoint): bidirectional sync reduces drift when all mutators use it.
- **Win32IDE ↔ Win32IDEBridge:** **`OrchestratorBridge` is not owned here** — IDE aligns via **`ensureOrchestratorBridgeInitialized()`** (above). **`Win32IDEBridge::initialize()`** is **not** currently invoked from **`Win32IDE_Core`** in this tree (no `#include` / call); capability router / hotpatch / observability inside that class only spin up after **`initialize()`**. If you need **`preprocessMessage`** or capability registration in production, wire an explicit init from the live startup path (and avoid duplicating Orchestrator init). **`CapabilityRouter::initializeAll()`** returns success when there are **zero** registered capabilities so empty manifest scans do not brick the bridge.
- **LSP → peek overlay:** LSP commands can drive `showPeekOverlay` with real locations when wired.
- **Unified dispatch machinery:** solid for anything **fully** registered in `COMMAND_TABLE`; Win32 **ordering** vs legacy is the main inconsistency.

---

## 6. Suggested fix order (status)

1. **Done — OrchestratorBridge init (production path)** — `ensureOrchestratorBridgeInitialized()` from **Core `onCreate`**, **`applySettings`**, and **GGUF background** path; `push`/`sync` when backend manager is up; `Win32IDEBridge` does **not** init Ollama. Orphan `Win32IDE_Window.cpp` carries a duplicate bootstrap for reference only.
2. **Done — Command path** — Documented **legacy-first** policy and re-entry risk: `docs/COMMAND_DISPATCH_BRIDGE.md` (unified-first remains unsafe until SSOT handlers stop self-`PostMessage` same ID).
3. **Done — Todo SSOT** — `manage_todo_list` mutates `SubAgentManager` via `SubAgentManagerRegisterToolTodoTarget`; `setTodoList` / `updateTodoStatus` fire `setTodoListChangedCallback` → `Win32IDE::notifySubAgentTodoListChanged` → `WM_SUBAGENT_TODO_CHANGED`.
4. **Done — Swarm façade** — `src/bridge/RawrXD_SwarmFacade.h` (`RawrXD::SwarmProduct::*`) for IAT lane; hexmag / `executeSwarm` remains in `subagent_core` (documented split).
5. **Done — Build-lane manifest** — `Win32IDE_FeatureManifest.cpp` entries: `bridge.linkCompletersLane` (STUB), `bridge.orchestratorInit`, `bridge.unifiedDispatchDoc`, `bridge.swarmIatFacade`; `subagent.todoList` description updated.

**Sixth item (from rollout):** inference/matrix doc — still optional follow-up (see §7).

---

## 7. Related docs / follow-ups

- **Init graph:** `ensureOrchestratorBridgeInitialized` → `OrchestratorBridge::Initialize` / `ApplyIdeOllamaSettings` → `pushOllamaBackendToOrchestratorBridge` / `syncOllamaBackendFromOrchestratorBridge`.
- **Commands:** `docs/COMMAND_DISPATCH_BRIDGE.md`.
- **Progress / re-audit table:** `docs/IDE_MASTER_PROGRESS.md` (sovereign batch counts + 63/200 reconciliation).
- **Win32IDEBridge (agentic, not Ollama):** optional follow-up — call `Win32IDEBridge::initialize()` from the **live** Win32 startup path if capability router / hotpatch / `preprocessMessage` should be active in production. This is **independent** of Orchestrator wiring (§2 A); do not conflate the two.
- Optional: add **inference path matrix** (user action → backend switcher vs agentic vs OrchestratorBridge).
