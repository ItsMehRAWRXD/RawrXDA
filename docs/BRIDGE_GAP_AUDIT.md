# RawrXD — Bridge inventory & gap audit

Codebase-oriented notes: who talks to whom, critical gaps, and a practical fix order.  
Last updated: 2026-03-20.

---

## 1. Bridge inventory

| Bridge / layer | Role | Main files |
|----------------|------|------------|
| **OrchestratorBridge** | Ollama chat/FIM, tool loop, discovery | `src/agentic/OrchestratorBridge.*` |
| **AgenticBridge** | Full agentic IDE: model, tools, autonomy, streaming | `Win32IDE_AgenticBridge.*`, `Win32IDE.cpp` |
| **Win32IDEBridge** (agentic) | Capabilities, hotpatch, router; also calls **OrchestratorBridge::Initialize** | `src/agentic/bridge/Win32IDEBridge.cpp` |
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

- **OrchestratorBridge** is initialized from **`Win32IDE_Window.cpp`** (when `m_ollamaBaseUrl` is set) and again from **`Win32IDEBridge::initialize()`** with a different working-dir / URL story.
- **Risk:** two callers, different config timing, “initialized” in one place but not reflected in backend UI until sync runs.
- **Mitigation in tree:** `syncOllamaBackendFromOrchestratorBridge` / `pushOllamaBackendToOrchestratorBridge` (backend switcher).
- **Remaining gap:** ensure **every** code path that mutates Ollama URL/model goes through one policy (grep for stray `SetModel`, direct `OllamaConfig`, duplicate `Initialize`).

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
- **LSP → peek overlay:** LSP commands can drive `showPeekOverlay` with real locations when wired.
- **Unified dispatch machinery:** solid for anything **fully** registered in `COMMAND_TABLE`; Win32 **ordering** vs legacy is the main inconsistency.

---

## 6. Suggested fix order

1. **Single OrchestratorBridge init policy** — one function from Win32 startup: URL, workdir, then push/sync with backend manager; **Win32IDEBridge** should call that helper instead of raw `Initialize` with different args.
2. **Command path** — migrate all `routeCommand` IDs into SSOT and **invert** order (unified first, legacy only for documented exceptions), **or** generate docs + static assert / list of legacy-only IDs.
3. **Subagent todo SSOT** — central store + UI refresh on every `manage_todo_list` / menu action.
4. **Swarm façade** — pick one orchestrator for product build; others behind `RAWR_*` or namespace `internal`.
5. **Build-lane manifest** — mark features **Stub** when `LinkCompleters` / `*_stubs.cpp` satisfy symbols.

---

## 7. Related docs / follow-ups

- After (1)–(2), update this doc with the **actual** init call graph and a **legacy-only command ID** list (or link to a generated artifact).
- Optional: add a short **inference path matrix** (user action → backend switcher vs agentic vs OrchestratorBridge).
