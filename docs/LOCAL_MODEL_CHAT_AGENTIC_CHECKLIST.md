# Local Model + Chat + Agentic Duties — Checklist

**Goal:** Open a local model (or Ollama), ask a question in chat, and have the model perform agentic duties (tool calls, edits, multi-step tasks).

---

## What Was Fixed (2026-03-13)

1. **Chat input now uses the agentic path**
   - Previously: In chat mode, the command input required `m_agent` and called `m_agent->Ask()`, which bypassed the AgenticBridge and tool dispatch.
   - Now: In chat mode, when the agentic bridge is initialized, the command is sent via `sendMessageToModel()` → `ExecuteAgentCommand()` → local model or `routeInferenceRequest()` (Ollama/cloud), with `DispatchModelToolCalls()` so tool calls in the model output are executed.

2. **Chat allowed without a local GGUF**
   - Previously: `sendMessageToModel()` returned "No model loaded" unless `isModelLoaded()` was true (e.g. a GGUF path set).
   - Now: Chat is allowed when the agentic bridge is initialized, so Ollama (or the active backend) can be used without loading a GGUF first. `ExecuteAgentCommand()` routes to `routeInferenceRequest()` when no local model is loaded.

---

## User Flow

| Step | Action | Result |
|------|--------|--------|
| 1 | Start IDE | Agentic bridge and backend manager initialize (Core/CreateWindow path). |
| 2 | **Option A:** Load a local GGUF (File > Open model, or drag .gguf, or Load Model) | `loadModelForInference()` → AgenticBridge loads model into shared CPU engine. Chat and agentic use this model. |
| 2 | **Option B:** Use Ollama only | Set Backend Switcher to Ollama, ensure Ollama is running and a model is pulled. No GGUF needed. |
| 3 | Turn on Chat Mode | (e.g. command or menu: toggle chat mode). Status shows "Chat Mode ON - Model: …". |
| 4 | Type a question in the command input and press Enter | Message goes to `sendMessageToModel()` → `ExecuteAgentCommand()` → local model or `routeInferenceRequest()` → response shown; any tool calls in the response are dispatched via `DispatchModelToolCalls()`. |

---

## If It Still Doesn’t Work

- **"No model loaded" / no response**
  - **Local GGUF:** Ensure the model file loads (check Output/Errors). For `RawrXDInference`, `tokenizer.json` (and optionally `merges.txt`) next to the GGUF or in cwd may be required; some builds use embedded tokenizer.
  - **Ollama:** Ensure Ollama is running, a model is pulled (e.g. `ollama run llama3` once), and Backend Switcher is set to Ollama with the correct endpoint (e.g. `http://localhost:11434`). Backend manager is inited at startup (Core), so routing should work.

- **Chat mode does nothing when I press Enter**
  - Agentic bridge must be initialized (it is when the main window is created). If you use a different entry point, ensure `initializeAgenticBridge()` runs. Check that the command handler is the one that does “Chat mode: send to model via agentic bridge” (see `Win32IDE.cpp` around the chat-mode block).

- **Model answers but no tool execution**
  - Tool dispatch is in `DispatchModelToolCalls()` (AgenticBridge). The model output must contain tool calls in the format the parser expects. Check that the backend (local or Ollama) returns tool-use format and that the parser matches it.

- **Workspace/project context**
  - For “work in this folder” behavior, open the project folder and set the workspace root so the agent gets project context. See `docs/AGENTIC_AND_MODEL_LOADING_AUDIT.md` (workspace root and prompt injection).

---

## Code References

- Chat command path: `Win32IDE.cpp` — command handler, block “Chat mode: send to model via agentic bridge” (calls `sendMessageToModel()`).
- Send to model: `Win32IDE.cpp` — `sendMessageToModel()` (allows chat when bridge inited; uses backend router or agentic bridge).
- Agentic execution and tools: `Win32IDE_AgenticBridge.cpp` — `ExecuteAgentCommand()`, `DispatchModelToolCalls()`; when no local model, calls `m_ide->routeInferenceRequest()`.
- Backend routing: `Win32IDE_BackendSwitcher.cpp` — `routeInferenceRequest()`, `routeToOllama()`, etc.
- Audit (backend init, agentic path, workspace): `docs/AGENTIC_AND_MODEL_LOADING_AUDIT.md`.
