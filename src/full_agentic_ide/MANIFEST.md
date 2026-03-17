# Full Agentic IDE — Complete Stack Manifest

**One folder. One orchestrator. Everything listed here is the full agentic stack—directly usable, no prototype-only paths.**

---

## This folder (entry point)

| File | Role |
|------|------|
| `FullAgenticIDE.h` | Public API: init, loadModel, chat, setWorkspaceRoot, getStatus, getAvailableTools, getBridge |
| `FullAgenticIDE.cpp` | Implementation: owns AgenticBridge, forwards all operations |
| `README.md` | Module overview |
| `MANIFEST.md` | This file — full list of stack components |

---

## Implementation (used by this module; do not duplicate)

### Bridge and init (single path)

| Component | Path | Role |
|-----------|------|------|
| AgenticBridge | `src/win32app/Win32IDE_AgenticBridge.cpp` (.h in same dir) | Win32IDE ↔ engine; ExecuteAgentCommand, LoadModel, SetWorkspaceRoot, callbacks |
| initializeAgenticBridge | `src/win32app/Win32IDE_AgentCommands.cpp` | **Single definition**: creates FullAgenticIDE, init, then m_agenticBridge = getBridge() |

### Engine and inference

| Component | Path |
|-----------|------|
| AgenticEngine | `src/agentic_engine.cpp` (+ include) |
| CPUInferenceEngine | `src/cpu_inference_engine.cpp` (.h) |

### Agentic core (tools, orchestrator, loop)

| Component | Path |
|-----------|------|
| ToolRegistry | `src/agentic/ToolRegistry.cpp`, `src/agentic/ToolRegistry.h` |
| AgentOrchestrator | `src/agentic/AgentOrchestrator.cpp`, `.h` |
| OrchestratorBridge | `src/agentic/OrchestratorBridge.cpp`, `.h` |
| BoundedAgentLoop | `src/agentic/BoundedAgentLoop.cpp`, `.h` |
| AgentToolHandlers | `src/agentic/AgentToolHandlers.cpp`, `.h` |
| agentic_executor | `src/agentic/agentic_executor.cpp` |
| FIMPromptBuilder | `src/agentic/FIMPromptBuilder.cpp`, `.h` |
| RawrXD_AgentLoop | `src/agentic/RawrXD_AgentLoop.cpp`, `.h` |
| OllamaProvider | `src/agentic/OllamaProvider.cpp`, `.h` |
| DiffEngine | `src/agentic/DiffEngine.cpp`, `.h` |

### Optional (recovery, observability, hotpatch)

| Component | Path |
|-----------|------|
| autonomous_recovery_orchestrator | `src/agentic/autonomous_recovery_orchestrator.cpp` |
| agentic_observability | `src/agentic_observability.cpp` |
| agentic_hotpatch_orchestrator | `src/agent/agentic_hotpatch_orchestrator.cpp`, `.hpp` |

---

## Assembly / native (when needed)

Any ASM used by the agentic stack is invoked via the C++ bridge and engine above. No separate “orchestrator” entry points; reverse engineering only to bind assembly roots when required.

---

## One init path (no duplicate)

- **Call:** `initializeAgenticBridge()` from Win32IDE (e.g. from onCreate or first agent use).
- **Definition:** Only in `Win32IDE_AgentCommands.cpp`. It creates `FullAgenticIDE`, initializes it, sets `m_agenticBridge = m_fullAgenticIDE->getBridge()`.
- **Do not** define `initializeAgenticBridge()` in `Win32IDE_Window.cpp` (or anywhere else).

**If `Win32IDE_Window.cpp` still has a second definition:** Delete the entire function body (from `void Win32IDE::initializeAgenticBridge() {` through the closing `}`). Replace with this single line:
```cpp
// Single definition in Win32IDE_AgentCommands.cpp (Full Agentic IDE). Do not redefine here.
```
A patch is in `patches/remove_duplicate_initializeAgenticBridge.patch` (apply with `git apply` after closing the file in the IDE if the apply fails due to line endings).

---

## “Complete” means

1. **One folder** — This folder (`src/full_agentic_ide/`) is the agentic surface; this manifest lists every component of the stack.
2. **One orchestrator** — `FullAgenticIDE` is the only class the IDE uses for agentic; it owns the bridge and delegates to the engine/tool stack above.
3. **Directly usable** — The path that runs is this path; no prototype-only or stub-only branches. What’s built is what runs.
