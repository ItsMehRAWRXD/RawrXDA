# Full Agentic IDE — Completion Manifest

**One folder. One orchestrator. No spiral.**

---

## What Was Done

### 1. Single module: `src/full_agentic_ide/`

| File | Role |
|------|------|
| **FullAgenticIDE.h** | Public API: `initialize()`, `loadModel()`, `chat()`, `setWorkspaceRoot()`, `getStatus()`, `getAvailableTools()`, `getBridge()` |
| **FullAgenticIDE.cpp** | Implementation: owns `AgenticBridge`, forwards all operations |
| **README.md** | Module manifest and usage |

All agentic entry points for the IDE go through this module. The rest of the codebase (AgenticBridge, AgenticEngine, tools) are implementation details used by FullAgenticIDE.

### 2. Win32IDE wiring

- **Win32IDE** now owns `m_fullAgenticIDE` (unique_ptr) and sets `m_agenticBridge = m_fullAgenticIDE->getBridge()` after init.
- **initializeAgenticBridge()** (in Win32IDE_AgentCommands.cpp) creates `FullAgenticIDE`, initializes it, then assigns `m_agenticBridge` from `getBridge()` so all existing `m_agenticBridge->...` call sites keep working.
- No behavioral change for callers: they still use the bridge; ownership and the single entry point live in FullAgenticIDE.

### 3. Build

- **CMakeLists.txt**: `src/full_agentic_ide/FullAgenticIDE.cpp` added to the IDE target.

### 4. Duplicate init (Window vs AgentCommands) — FIX REQUIRED

- **Single definition:** `Win32IDE::initializeAgenticBridge()` is implemented only in **Win32IDE_AgentCommands.cpp** (Full Agentic IDE path: creates `FullAgenticIDE`, init, then `m_agenticBridge = m_fullAgenticIDE->getBridge()`).
- **Win32IDE_Window.cpp** must **not** define `initializeAgenticBridge()` again. If that file is in the build, remove the function body (the whole `void Win32IDE::initializeAgenticBridge() { ... }` block) and replace with a one-line comment:  
  `// Single definition in Win32IDE_AgentCommands.cpp (Full Agentic IDE). Do not redefine here.`  
  The call site `initializeAgenticBridge();` in `onCreate()` stays; it will link to the implementation in AgentCommands.

---

## What “complete” means

1. **One folder** — `src/full_agentic_ide/` is the place for the full agentic IDE surface. New agentic work extends this module or the components it uses; it does not add new top-level entry points elsewhere.
2. **One orchestrator** — `FullAgenticIDE` is the single class the IDE uses for agentic behavior. It owns the bridge and delegates to the existing engine/tool stack.
3. **No spiral** — Prototypes and one-off agentic paths are consolidated behind this API. The spiral stops here.

---

## Checklist for “full agentic IDE” E2E

- [x] Single module and API for agentic (FullAgenticIDE).
- [x] Win32IDE uses FullAgenticIDE; bridge obtained via getBridge().
- [x] Chat path uses agentic bridge (sendMessageToModel → ExecuteAgentCommand; already fixed earlier).
- [x] Open folder sets workspace root and passes it to the agent (handleWelcomeOpenFolder already sets m_explorerRootPath, m_projectRoot, and bridge SetWorkspaceRoot).
- [x] Backend manager inited at startup (Win32IDE_Core.cpp initBackendManager).
- [ ] **You:** Remove duplicate `initializeAgenticBridge()` from `src/win32app/Win32IDE_Window.cpp`: delete the whole function body at end of file; replace with `// Single definition in Win32IDE_AgentCommands.cpp (Full Agentic IDE). Do not redefine here.` See `src/full_agentic_ide/MANIFEST.md` and `patches/remove_duplicate_initializeAgenticBridge.patch`.
- [x] CMake already includes `src/full_agentic_ide/FullAgenticIDE.cpp` and AgentCommands; include path is via `Win32IDE.h` which includes `../full_agentic_ide/FullAgenticIDE.h`.

---

## References

- Module: `src/full_agentic_ide/README.md`
- Chat + model + agentic flow: `docs/LOCAL_MODEL_CHAT_AGENTIC_CHECKLIST.md`
- Audit (what was not capable): `docs/IDE_NOT_CAPABLE_AUDIT.md`
- Agentic + model loading: `docs/AGENTIC_AND_MODEL_LOADING_AUDIT.md`
