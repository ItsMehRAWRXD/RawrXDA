# Agentic Mode Switcher — Implementation Summary

**Date:** 2026-02-15  
**Scope:** Plan / Agent / Ask three-mode system in RawrXD Win32 IDE.

---

## What Was Implemented

### 1. **Agentic Mode Enum & Switcher** (`src/win32app/agentic_mode_switcher.hpp`)
- **RawrXD::AgenticMode**: `Ask` (0), `Plan` (1), `Agent` (2)
- Helpers: `AgenticModeToString()`, `AgenticModeFromInt()`, `AgenticModeToInt()`

### 2. **Plan Mode Handler** (`src/win32app/plan_mode_handler.hpp` / `.cpp`)
- **PlanModeHandler**: Optional MetaPlanner + optional subagent research
- **PlanModeHandler::run(userWish)**:
  - Optionally runs a subagent for “research” (if AgenticBridge set)
  - Calls **MetaPlanner::plan()** to get a task-graph JSON
  - Returns **PlanModeResult** with `planText` (checklist), `planJson`, `researchNote`
- **formatPlanAsChecklist(planJson)**: Renders phases/tasks as numbered checklist; “Reply /approve to run” at end

### 3. **Agent Mode** (`src/win32app/agent_mode_handler.hpp`)
- **AgentModeSystemPrompt()**: Instructs model to use `manage_todo_list` and `runSubagent`, stream progress
- **AgentModeUserPrefix()**: Prepends `[Agent mode: execute autonomously with todo list and subagents]` to user message

### 4. **Ask Mode** (`src/win32app/ask_mode_handler.hpp`)
- **AskModeSystemPrompt()**: Encourages concise answers and citations
- **AskModeUserPrefix()**: Empty (no prefix)

### 5. **Win32 IDE Integration**
- **Win32IDE.h**
  - `RawrXD::AgenticMode m_agenticMode`
  - `m_hwndAgenticModeAsk`, `m_hwndAgenticModePlan`, `m_hwndAgenticModeAgent`
  - `m_hwndAgenticModeCombo` (for VSCode UI path)
  - `std::unique_ptr<MetaPlanner> m_metaPlanner`
  - `RawrXD::PlanModeHandler m_planModeHandler`
  - `onAgenticModeChanged()`, `setAgenticMode()`, `onAgenticModeAsk/Plan/Agent()`
- **Win32IDE.cpp**
  - **createSecondarySidebarContent**: Three radio buttons “Ask” | “Plan” | “Agent” (IDs 4300/4301/4302)
  - **HandleCopilotSend**:
    - **Plan**: Lazy-create MetaPlanner, set PlanModeHandler’s planner + bridge, run `m_planModeHandler.run(userMessage)`, append `[Plan]\n` + plan text to chat
    - **Agent**: `generateResponseAsync(AgentModeUserPrefix() + userMessage, onResponse)`
    - **Ask**: `generateResponseAsync(userMessage, onResponse)` (unchanged)
  - **SidebarProcImpl**: WM_COMMAND for IDC_AGENTIC_MODE_ASK/PLAN/AGENT → `onAgenticModeChanged(mode)`
- **Win32IDE_AgentCommands.cpp**
  - **setAgenticMode(RawrXD::AgenticMode)**: Sets `m_agenticMode`, updates the three radio buttons (BM_SETCHECK), appends “Chat mode: Ask|Plan|Agent” to Output
  - **onAgenticModeChanged(mode)**: Calls `setAgenticMode(mode)`
  - **onAgenticModeAsk/Plan/Agent()**: Call `setAgenticMode` with the corresponding mode
- **Win32IDE_VSCodeUI.cpp**
  - Uses existing **m_hwndAgenticModeCombo** and **IDC_AGENTIC_MODE_COMBO** (4303); **m_hwndAgenticModeCombo** and ID added so this path compiles.

### 6. **Build**
- **CMakeLists.txt**: `src/win32app/plan_mode_handler.cpp` added to the Win32 IDE target (two places).
- Compilation verified; link failed only due to output exe being in use (LNK1104).

---

## User Flow

1. **Ask** (default): User types message → same as before (streaming Q&A).
2. **Plan**: User types goal → MetaPlanner decomposes → optional subagent research → checklist shown in chat; user can reply “/approve” or switch to Agent and send again to execute.
3. **Agent**: User types goal → prompt prefixed with agent instructions → streaming response with tool calls (manage_todo_list, runSubagent) handled by existing **DispatchModelToolCalls**.

---

## Files Touched

| File | Change |
|------|--------|
| `src/win32app/agentic_mode_switcher.hpp` | New: enum + helpers |
| `src/win32app/plan_mode_handler.hpp` | New: PlanModeHandler + PlanModeResult |
| `src/win32app/plan_mode_handler.cpp` | New: run(), formatPlanAsChecklist() |
| `src/win32app/agent_mode_handler.hpp` | New: Agent prompt/prefix |
| `src/win32app/ask_mode_handler.hpp` | New: Ask prompt/prefix |
| `src/win32app/Win32IDE.h` | Agentic mode members, MetaPlanner, PlanModeHandler, declarations |
| `src/win32app/Win32IDE.cpp` | Radios, HandleCopilotSend branch, SidebarProcImpl commands |
| `src/win32app/Win32IDE_AgentCommands.cpp` | setAgenticMode, onAgenticModeChanged, RawrXD:: qualifiers |
| `src/win32app/Win32IDE_VSCodeUI.cpp` | IDC_AGENTIC_MODE_COMBO defined |
| `CMakeLists.txt` | plan_mode_handler.cpp in IDE target |

---

## Implemented Follow-Ups (All Four Next Steps Done)

1. **Editor context** — `getActiveEditorContext()` (file, line, col, selection), `formatEditorContextForAI()`; context block prepended to user message in HandleCopilotSend for all modes.

2. **Problems panel** — `getProblemsContextForAI(maxItems)` formats `m_problems`; appended to context block so Plan/Agent/Ask see current errors/warnings and can suggest fixes.
3. **Plan approval** — `/approve` (trimmed, case-insensitive) sets effectiveMessage = `m_lastPlanGoal` and effectiveMode = Agent; last plan runs in Agent mode. `m_lastPlanGoal` / `m_lastPlanText` stored in Plan mode; cleared on HandleCopilotClear.
4. **Agent system prompt** — Agent mode prompt is `AgentModeSystemPrompt() + "\n\n" + AgentModeUserPrefix() + messageWithContext`.
