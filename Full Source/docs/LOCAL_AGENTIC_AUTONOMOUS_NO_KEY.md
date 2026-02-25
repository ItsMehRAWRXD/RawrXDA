# Local, Agentic, Autonomous — No API Key

**Intent:** When we say **without an API key**, we mean the IDE runs **locally, agentically, and autonomously** in the same way Cursor and GitHub Copilot do — with **no cloud and no keys**.

---

## In one sentence

**No API key = same Cursor/Copilot-style agentic and autonomous behavior, fully on-device.**

---

## What that means

| Term | Meaning here |
|------|----------------|
| **Locally** | All inference and tooling run on your machine. No calls to OpenAI, Cursor cloud, or GitHub API with a key. Optional: Ollama on localhost or a GGUF file; no API key required. |
| **Agentically** | The agent plans, uses tools (edit, search, run, read), and iterates (BoundedAgentLoop, ToolRegistry). Same loop as Cursor agent mode; the only difference is the LLM is local (GGUF + x64 MASM kernels). |
| **Autonomously** | Multi-step workflows and decisions run without you holding the reins (autonomous_workflow_engine, agentic_task_graph). Again, the model is local; no key. |

So: completion, chat, Composer, @-mentions, refactor, semantic index, and the rest of the 8 Cursor parity modules can all run **local + agentic + autonomous** with **no API key**.

---

## How it’s implemented

- **Inference:** Local GGUF + `LocalParity_NextToken` (x64 MASM) and C++ bridge; see `docs/LOCAL_PARITY_NO_API_KEY_SPEC.md` and `include/local_parity_kernel.h`.
- **Agent loop:** `BoundedAgentLoop`, `ToolRegistry`, `AgentTranscript` — same as with a remote backend; backend is set to `local` / `local://gguf` so no key is used.
- **Autonomous pipeline:** `autonomous_workflow_engine`, `agentic_task_graph` — they call the same ModelInvoker/LLM backend; when that backend is local, everything stays on-device.

---

## Where this is documented

- **This file** — definition of “without an API key” = local + agentic + autonomous.
- **docs/LOCAL_PARITY_NO_API_KEY_SPEC.md** — full stack (MASM kernel, bridge, zero-key contract).
- **docs/LOCAL_INFERENCE_PARITY_SPEC.md** — local inference architecture and gaps.
- **include/cursor_github_parity_bridge.h** — note that local parity mode uses LocalParity_* and no API key.
