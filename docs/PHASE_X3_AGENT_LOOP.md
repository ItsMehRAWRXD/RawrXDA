# Phase X+3: Agent Loop (command path wired)

**Goal:** Typing `/agent <prompt>` or `/ask <prompt>` in the command input runs the agentic pipeline and shows the response in the output pane.

---

## What's wired

1. **Command handler** — In `executeCommand()`, when the user enters `/agent` or `/ask` plus a prompt:
   - Prompt is trimmed; empty prompt shows usage.
   - `m_agenticBridge` is lazy-initialized if null (`initializeAgenticBridge()`).
   - If the bridge is initialized: `ExecuteAgentCommand(q)` is called; the returned `AgentResponse` is shown in the output pane (error vs info).
   - If the bridge is not available but `m_agent` (NativeAgent) is set: falls back to `m_agent->Ask(q)` (local model).
   - Otherwise: user is told to load a model or initialize the Agentic Framework.

2. **AgenticBridge** — Already provides:
   - `ExecuteAgentCommand(prompt)` — single-shot execution (AgenticEngine/Ollama path).
   - `StartAgentLoop(initialPrompt, maxIterations)` — multi-turn loop (menu: Start Agent Loop).
   - Output callback set in `initializeAgenticBridge()` streams agent output to the output pane and Copilot chat.

3. **Beacon** — `triggerAgenticBeacon()` sends agentic requests over the Beacon client when needed; the command path does not require it for basic `/agent`/`/ask`.

---

## How to validate (manual)

1. Build and run the C++ IDE.
2. Ensure either:
   - **Agentic path:** Agentic Framework is initialized (e.g. via menu that triggers `initializeAgenticBridge()`), or
   - **Native path:** A model is loaded with `/load <model.gguf>` so `m_agent` is available.
3. In the command input, type: `/agent Explain what a pointer is in C++` (or any prompt).
4. Confirm: Response appears in the output pane (and optionally in Copilot chat). If the bridge is not initialized and no model is loaded, you see the "No agent available" message.

---

## Files touched (Phase X+3)

| File | Change |
|------|--------|
| `Win32IDE.cpp` | `/agent` and `/ask` branch: trim prompt, init bridge if needed, call `ExecuteAgentCommand(q)` when bridge is initialized; fallback to `m_agent->Ask(q)`; single `return` and correct braces. |

---

## Next

- **Phase X+4** — GGUF hotpatch (switch model at runtime).
- **Phase X+5** — Distributed swarm (multi-GPU inference).

Optional: Propagate `/max`, `/think`, `/research`, `/norefusal` to `m_agenticBridge` when the bridge is the primary agent path so flags stay in sync.
