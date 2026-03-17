# RawrXD IDE — Feature Utilization Audit

**Date:** 2026-03-04  
**Scope:** Ensure all finished/implemented features are utilized in the IDE (no dead-end features).

---

## 1. Command dispatch and SSOT

### 1.1 Flow

- **COMMAND_TABLE** (`include/command_registry.hpp`, `src/core/command_registry.hpp`) is the single source of truth: ID, symbol, canonical name, CLI alias, exposure, category, **handler**.
- **GUI:** `WM_COMMAND` → `Win32IDE::onCommand()` (Core) → `routeCommand(id)` (legacy: View, File, Backend, Router, LSP, etc.) → else `routeCommandUnified(id)` → `dispatchByGuiId(id)` → handler from `g_commandRegistry[]`.
- **CLI:** `dispatchByCli(alias)` → same handlers.
- **Palette:** Ctrl+Shift+P → `showCommandPalette()` → `buildCommandRegistry()` → user picks → `executeCommandFromPalette()` → `routeCommand(id)` then `routeCommandUnified(id)`.

### 1.2 Backend / Router / LSP

- **Menu/palette IDs** (e.g. 5037–5047 Backend, 5048–5081 Router, 5058–5070 LSP) are handled in **Win32IDE_Commands.cpp** `routeCommand()` with IDE-specific logic (e.g. `setActiveBackend()`, `getRouterStatusString()`).
- **SSOT handlers** (e.g. `handleBackendSwitchLocal`) in `ssot_handlers_ext.cpp` **delegate to the IDE** when `ctx.isGui && ctx.idePtr`: they `PostMessage(hwnd, WM_COMMAND, id, 0)` so the same ID is processed by `routeCommand()`. So:
  - **Menu/palette** → routeCommand() runs directly.
  - **CLI or programmatic dispatch** → SSOT handler runs → PostMessage → routeCommand() runs.
- **Conclusion:** Backend/Router/LSP are not dead ends; SSOT and IDE share the same behavior via delegation.

---

## 2. Command palette vs COMMAND_TABLE

### 2.1 Before audit

- **buildCommandRegistry()** in `Win32IDE_Commands.cpp` was a **manual** list of 300+ entries. Any new command added only to COMMAND_TABLE would **not** appear in the palette → drift and “hidden” features.

### 2.2 Change made

- **Sync pass** added at the end of `buildCommandRegistry()`:
  - Iterate `g_commandRegistry[]` (COMMAND_TABLE).
  - For each entry with `id != 0` and exposure `GUI_ONLY` or `BOTH`, if that `id` is not already in `m_commandRegistry`, append a palette item with label `"Category: canonicalName"`.
- **Result:** New COMMAND_TABLE commands are automatically visible in the command palette; no second manual list to maintain.

---

## 3. Panels and feature usage

| Area | Implementation | Used from IDE | Notes |
|------|----------------|---------------|--------|
| **Backend switcher** | Win32IDE_BackendSwitcher.cpp, routeCommand(5037+) | Menu, palette, SSOT (PostMessage) | ✅ Utilized |
| **LLM Router** | Win32IDE_LLMRouter.cpp, routeCommand(5048+) | Menu, palette, SSOT | ✅ Utilized |
| **LSP client** | Win32IDE_LSPClient.cpp, routeCommand(5058+) | Menu, palette, LSP panel | ✅ Utilized |
| **ASM / Hybrid / MultiResp** | routeCommand() + ssot_handlers_ext | Menu, palette | ✅ Utilized |
| **Hotpatch** | routeCommand(9001+), HotpatchPanel | Menu, palette, panel | ✅ Utilized |
| **Agent / Autonomy / SubAgent** | routeCommand(), AgentPanel, AgentCommands | Menu, palette, chat/agent UI | ✅ Utilized |
| **VscextRegistry** | vscext_registry.cpp | main_win32, selftest, listCommands/getStatusString | ✅ Linked and used |
| **Quantum agent stack** | quantum_autonomous_todo_system, quantum_multi_model_agent_cycling, etc. | agentic_deep_thinking_engine (getQuantumTodoSystem, getMultiModelCycling) | ✅ In WIN32IDE_SOURCES |
| **Unified command dispatch** | unified_command_dispatch.cpp, SharedFeatureRegistry | Populated from COMMAND_TABLE at startup; dispatchByGuiId used by routeCommandUnified | ✅ Utilized |
| **Fuzzy command palette** | Tier1Cosmetics, initFuzzySearch() | Uses g_commandRegistry directly; showFuzzyPaletteWindow() | ✅ All COMMAND_TABLE commands available |

---

## 4. Findings and recommendations

### 4.1 No dead-end features identified

- All checked features are either:
  - Handled by `routeCommand()` (legacy IDE logic), or
  - Dispatched via `routeCommandUnified()` → COMMAND_TABLE handlers, or
  - Used by panels/agents that call the same IDs or the same backend (e.g. VscextRegistry, quantum stack).

### 4.2 Drift prevention

- **Done:** Command palette is now kept in sync with COMMAND_TABLE via the new sync pass in `buildCommandRegistry()`.
- **Recommendation:** When adding a new command, add **one** line to COMMAND_TABLE; it will appear in the palette and in fuzzy search. Only add a manual palette entry in `buildCommandRegistry()` if you want a **custom label** (e.g. “Backend: Show All Backend Status” instead of “Backend: backend.status”).

### 4.3 Menus from COMMAND_TABLE (implemented)

- **Commands menu:** Menu bars are built from separate resources or code (e.g. AppendMenu). If a command is in COMMAND_TABLE but not in any menu, it is still reachable via palette and fuzzy palette. To avoid “menu-only” drift. A top-level Commands menu is now built from COMMAND_TABLE in createMenuBar (Win32IDE.cpp). Helper buildCommandsMenuFromCommandTable(HMENU) groups g_commandRegistry by category and adds one submenu per category; every GUI-exposed COMMAND_TABLE entry is reachable from the menu as well as the palette.

---

## 5. Files touched in this audit

- **src/win32app/Win32IDE_Commands.cpp**
  - Include `../core/command_registry.hpp`.
  - At end of `buildCommandRegistry()`, add SSOT sync loop so every GUI-exposed COMMAND_TABLE entry appears in the palette.
- **src/win32app/Win32IDE.cpp**
  - Include `../core/command_registry.hpp`, `<map>`, `<vector>`.
  - Add `buildCommandsMenuFromCommandTable(HMENU)` and call it from `createMenuBar()` before `SetMenu()` so a **Commands** menu (by category) is built from COMMAND_TABLE.
- **reverse_engineering_reports/ide_feature_utilization_audit.md** (this file).

---

**Audit status:** Complete. Finished features are utilized; palette is aligned with COMMAND_TABLE to prevent dead-end or hidden commands.
