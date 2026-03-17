# Agentic Orchestra — One Roof, No Spiral

**Everything agentic is under one folder and one orchestrator. No scattered prototypes.**

---

## Single entry point

| What | Where |
|------|--------|
| **Public API** | `src/full_agentic_ide/` — `FullAgenticIDE.h`, `FullAgenticIDE.cpp` |
| **Implementation** | `FullAgenticIDE` owns `AgenticBridge`; bridge lives in `src/win32app/Win32IDE_AgenticBridge.cpp` |
| **Init** | **One** definition of `Win32IDE::initializeAgenticBridge()` in `src/win32app/Win32IDE_AgentCommands.cpp` |

All agentic behavior (model load, chat, tools, workspace) goes through `FullAgenticIDE`. The IDE uses `m_fullAgenticIDE` and `m_agenticBridge = m_fullAgenticIDE->getBridge()` for existing call sites.

---

## What “complete” means here

1. **One folder** — `src/full_agentic_ide/` is the agentic surface. Assembly/C++/other code is used by this stack, not scattered as separate “orchestrator” entry points.
2. **One orchestrator** — `FullAgenticIDE` is the only class the IDE uses for agentic; it owns the bridge and delegates to the engine/tool stack.
3. **Directly usable** — No “prototype-only” paths. What’s built is the path that runs; reverse engineering is only for finding roots when needed, not for endless stubs.

---

## One code change left (when file is writable)

- **File:** `src/win32app/Win32IDE_Window.cpp`
- **Problem:** Second definition of `void Win32IDE::initializeAgenticBridge()` at the end of the file (creates `AgenticBridge` directly). If both Window and AgentCommands are linked, that’s a duplicate symbol.
- **Fix:** Delete the **entire** function body (from `void Win32IDE::initializeAgenticBridge() {` through the closing `}`). Replace with:
  ```cpp
  // Single definition in Win32IDE_AgentCommands.cpp (Full Agentic IDE). Do not redefine here.
  ```
- **Keep:** The call `initializeAgenticBridge();` in `onCreate()` — it will link to the implementation in AgentCommands.

---

## References

- `docs/FULL_AGENTIC_IDE.md` — Completion manifest and checklist  
- `src/full_agentic_ide/README.md` — Module manifest
