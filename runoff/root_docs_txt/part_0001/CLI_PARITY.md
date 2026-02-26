# RawrXD CLI Parity — 101% with Win32 GUI

This document describes the **pure CLI** entry points that mirror the Win32 GUI in chat and agentic autonomous capabilities.

---

## Entry Points

| Binary | Invocation | Capabilities |
|--------|------------|--------------|
| **RawrEngine.exe** | `RawrEngine [options]` | **Full 101% parity** — chat, agent loop, subagent, chain, swarm, tool dispatch, Ollama + GGUF |
| **RawrXD-Win32IDE.exe --headless --repl** | `RawrXD-Win32IDE.exe --headless --repl` | **101% parity** — chat, agent loop + tool dispatch, subagent, chain, swarm, /ask, /api/tool |

---

## RawrEngine — Full Agentic CLI

**Build:** `cmake --build . --config Release --target RawrEngine`

**Usage:**
```
RawrEngine [options]
  --model <path>    Path to GGUF model file
  --port <port>     HTTP server port (default: 8080)
  --no-http         Disable HTTP server
  --no-repl         Disable interactive REPL
```

### REPL Commands (mirrors Win32 Agent Chat + Autonomy)

| Command | Description |
|---------|-------------|
| `/chat <message>` | Chat (GGUF or Ollama if no model loaded) |
| `/chat /model:<name> <msg>` | Chat using specific Ollama model |
| `/wish <task>` | Execute user wish (natural language task) |
| `/agent <prompt> [N]` | Agentic loop: chat + tool dispatch until done (max N cycles) |
| `/subagent <prompt>` | Spawn a sub-agent |
| `/chain <s1> \| <s2> ...` | Run sequential prompt chain |
| `/swarm <p1> \| <p2> ...` | Run HexMag parallel swarm |
| `/agents` | List all sub-agents |
| `/tools` | List agent tools (Win32 View Tools parity) |
| `/run-tool <name> [json]` | Execute tool by name (e.g. `/run-tool list_dir {}`) |
| `/smoke` | Run agentic smoke test (Win32 parity) |
| `/status` | System status |

### HTTP API

- `POST /api/chat` — Agentic chat
- `POST /api/subagent` — Spawn sub-agent
- `POST /api/chain` — Execute chain
- `POST /api/swarm` — Launch swarm
- `GET /api/agents`, `/api/agents/status` — Agent status
- `GET /api/agentic/config`, `POST /api/agentic/config` — Agentic config

---

## Headless Win32 IDE — `--headless --repl`

**Invocation:** `RawrXD-Win32IDE.exe --headless --repl`

Runs the same engine as the Win32 GUI but without any window. Uses Ollama for inference (ensure `ollama` is running).

**Options:** `--help` for usage; `--prompt <text>`, `--input <file>` for batch modes (see UNFINISHED_FEATURES.md).

### REPL Commands

| Command | Description |
|---------|-------------|
| `/chat <msg>` | Chat with the model (includes tool dispatch) |
| `/agent <prompt> [N]` | Agentic loop + tool dispatch (max N cycles) |
| `/wish <task>` | Execute user wish with tool dispatch |
| `/subagent <prompt>` | Spawn a sub-agent |
| `/chain <s1> \| <s2>` | Sequential prompt chain |
| `/swarm <p1> \| <p2>` | HexMag parallel swarm |
| `/agents` | List active sub-agents |
| `/tools` | List available agent tools (Win32 View Tools parity) |
| `/status` | Full status dump (JSON) |
| `/run-tool <name> [json]` | Execute tool by name (Win32 Agent > Run Tool parity) |
| `/smoke` | Run agentic smoke test (Win32 Agent > Run Smoke Test parity) |
| `<any text>` | Treated as chat prompt |

### HTTP API (mirrors Win32 LocalServer)

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/ask` | POST | Unified chat — `{"question":"..."}` |
| `/api/tool` | POST | Tool dispatch — `{"tool":"read_file","args":{"path":"..."}}` |
| `/api/generate` | POST | Generate completion |
| `/v1/chat/completions` | POST | OpenAI-compatible chat |
| `/health` | GET | Health check |

---

## Parity Summary

| Feature | RawrEngine | Headless --repl |
|---------|------------|-----------------|
| Chat | ✅ | ✅ |
| Agent loop + tool dispatch | ✅ | ✅ |
| Subagent / Chain / Swarm | ✅ | ✅ |
| Tool dispatch (read_file, write_file, etc.) | ✅ | ✅ (REPL + HTTP /api/tool) |
| HTTP /ask | ✅ (as /api/chat) | ✅ |
| Ollama routing | ✅ | ✅ |
| Local GGUF | ✅ | ✅ (via load command) |
| View agent tools | ✅ `/tools` | ✅ `/tools` |
| Run tool by name | ✅ `/run-tool` (REPL) | ✅ `/run-tool` (REPL) |
| Agent smoke test | ✅ `/smoke` | ✅ `/smoke` |

**Both entry points provide 101% parity** with the Win32 GUI. Use **RawrEngine.exe** for a standalone CLI binary, or **RawrXD-Win32IDE.exe --headless --repl** for the same capabilities from the IDE binary.

---

## Ship build (lightweight CLI)

A **lighter-weight pure CLI** with the same 101% parity is built from the Ship directory:

- **Binaries:** `RawrXD_CLI.exe` / `RawrXD_Agent_Console.exe` (same logic).
- **Stack:** AgentOrchestrator, ToolExecutionEngine (44+ tools), Ollama, HTTP server on port 23959.
- **Build:** From a build that includes Ship: `cmake --build . --config Release --target RawrXD_CLI` (or `RawrXD_Agent_Console`).
- **In-chat commands:** `quit`, `clear`, `models`, `/tools`, `/status`, `/run-tool <name> [json]`, `help`.

See **Ship/CLI_PARITY.md** for full options, interactive commands, and parity table.
