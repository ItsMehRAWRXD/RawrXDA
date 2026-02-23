# Agentic Win32 IDE — Full Integration Guide

This document describes how to run and smoke-test the full Win32 GUI IDE for agentic tasks: server flow, HTTP smoke test, GUI smoke test, and extension install (GitHub Copilot / Amazon Q).

**Universal access (local IDE + external site):** For the design that allows full Cursor-like usage from the locally hosted IDE and full external use via a web site (anywhere/any OS), see [docs/UNIVERSAL_ACCESS_DESIGN.md](docs/UNIVERSAL_ACCESS_DESIGN.md).

---

## 1. Agentic flow (server + IDE)

- **RawrEngine** exposes the HTTP API: `GET /status`, `GET /v1/models`, `GET /api/models`, `POST /api/chat`, `POST /api/agent/wish`, `GET/POST /api/agentic/config`, etc.
- **Win32 IDE** can run as the GUI that talks to RawrEngine (or embeds it). Chat and Agent menus use the agentic engine; optional `model` in requests routes to Ollama when provided.
- **Operation modes** (Ask / Plan / Full) are configured via `GET/POST /api/agentic/config` and control whether tool dispatch runs (Full) or not (Ask/Plan).

---

## 2. CLI parity with Win32 IDE (RawrEngine REPL)

**RawrEngine** is the pure CLI version and has **full chat and agentic autonomous parity** with the Win32 GUI:

| Capability | Win32 IDE | CLI (RawrEngine REPL) |
|------------|-----------|------------------------|
| Chat (GGUF) | ✅ Chat panel / Agent | ✅ `/chat <message>` |
| Chat (Ollama, no GGUF) | ✅ Backend Ollama / model in request | ✅ `/chat <message>` (uses active backend) or `/chat /model:llama3.2 <message>` |
| Agent wish (natural language) | ✅ API `POST /api/agent/wish` | ✅ `/wish <natural language>` |
| Agentic loop (multi-turn + tools) | ✅ Agent menu → Start Agent Loop | ✅ `/agent <prompt> [max_iterations]` (default 10) |
| Operation mode (Ask/Plan/Agent) | ✅ Config / API | ✅ `/agentic mode <Agent\|Plan\|Debug\|Ask>` |
| Model selection (Auto/MAX/multiple) | ✅ Config / API | ✅ `/models`, `/models mode`, `/models cap`, etc. |
| Backend switch (Ollama vs local) | ✅ Backend switcher | ✅ `/backend use ollama`, `/backend list` |
| Sub-agent, chain, swarm | ✅ API + UI | ✅ `/subagent`, `/chain`, `/swarm` |
| View agent tools | ✅ Agent > View Tools | ✅ `/tools` |
| Run tool by name | ✅ Agent > Run Tool | ✅ `/run-tool <name> [json]` |
| Agent smoke test | ✅ Agent > Run Smoke Test | ✅ `/smoke` |
| History, replay, policies | ✅ API + UI | ✅ `/history`, `/replay`, `/policies`, etc. |

**CLI-only details**

- With **no model loaded**, `/chat` uses the **active backend** (e.g. Ollama). Set `OLLAMA_HOST` or use `/backend use ollama` and ensure a model is pulled (e.g. `ollama pull llama3.2`).
- **`/chat /model:<name> <message>`** forces that Ollama model for one turn (mirrors API `POST /api/chat` with `"model": "<name>"`).
- **`/agent <prompt> [N]`** runs the same loop as the GUI’s Agent loop: chat → tool dispatch → feed result back → repeat until no tool call or N cycles.
- **`/tools`** lists available agent tools (parity with Win32 Agent > View Tools).
- **`/smoke`** runs the agentic smoke test (parity with Win32 Agent > Run Smoke Test).

---

## 3. HTTP smoke test (no GUI)

Use this to verify the server-side agentic path before or without the IDE.

**Prerequisites**

- RawrEngine built and running (e.g. `build_ide\RawrEngine.exe` or `RawrEngine --port 8080`).
- Default base URL: `http://localhost:8080`.

**Run**

```powershell
cd D:\rawrxd
.\SmokeTest-AgenticIDE.ps1
```

**Options**

- `-BaseUrl "http://localhost:9090"` — use a different server URL.
- `-SkipChat` — only run status, models list, and agentic config (no POST /api/chat).
- `-TimeoutSec 30` — request timeout in seconds.
- `-WaitForPortSec 30` — wait up to N seconds for the server to respond before failing (useful right after starting RawrEngine).

**What it checks**

1. `GET /status` — server ready.
2. `GET /v1/models` and `GET /api/models` — model list.
3. `GET /api/agentic/config` — agentic config (e.g. operationMode).
4. `POST /api/chat` — agentic chat (expects a response or AGENTIC_SMOKE_OK).
5. `POST /api/chat` with `model` (e.g. llama3.2) — Ollama routing (optional; may fail if Ollama is down or model missing).

Exit code **0** = all checks passed; **1** = one or more failed (or server not reachable).

---

## 4. GUI agentic smoke test

In the Win32 IDE:

1. **Launch** the IDE (e.g. `RawrXD_Win32_IDE.exe` or your main IDE executable).
2. **Agent menu** → **Run Smoke Test** (or equivalent).
3. The smoke test creates `agent_smoke_test.txt`, runs a simple agent loop (e.g. dir/ls, read file), and reports in the output pane.
4. **Success:** output pane shows completion and `agent_smoke_test.txt` is present with expected content.

If the smoke test fails, ensure RawrEngine (or embedded server) is running and that the agentic framework and model (e.g. default in config) are available.

---

## 5. Extensions (GitHub Copilot / Amazon Q)

The IDE can route chat to **GitHub Copilot** or **Amazon Q** via VS Code–style commands: `github.copilot.chat.proxy` and `amazon.q.chat.proxy`. These require the corresponding extensions to be installed.

**If the extension is not installed**

- Choosing GitHub Copilot or Amazon Q backend and sending a prompt will show a friendly message:  
  *"GitHub Copilot extension not installed. Use AI menu > Install from VSIX… or switch to Ollama/Local backend."*  
  (Similarly for Amazon Q.)

**Installing extensions (VSIX)**

- **AI menu** → **Install from VSIX** (or equivalent): pick a `.vsix` file. The IDE installs to `%APPDATA%\RawrXD\extensions\<vsix_stem>`.
- **Development:** set `RAWRXD_ALLOW_UNSIGNED_EXTENSIONS=1` to allow unsigned VSIX.
- **GitHub Copilot / Amazon Q:** Official VSIX are not publicly downloadable like typical marketplace extensions; they are tied to vendor accounts. To use them:
  - Install the extension in VS Code first, then copy the extension folder from your VS Code extensions directory to `%APPDATA%\RawrXD\extensions\`, or
  - Use a script that copies from a user-provided path (e.g. from VS Code’s extension dir) into RawrXD’s extension dir.

**Backend summary**

| Backend        | Requirement                          |
|----------------|--------------------------------------|
| Ollama / Local | RawrEngine + models (or Ollama)      |
| GitHub Copilot | VSIX/extension installed             |
| Amazon Q       | VSIX/extension installed             |

---

## 6. Launch and test in one go

From the repo root (e.g. `D:\rawrxd` or your worktree):

```powershell
.\Launch-And-Test.ps1
```

This script:

1. Starts **RawrEngine** from `build_ide\RawrEngine.exe` or `build\RawrEngine.exe` in a new window.
2. Waits for the HTTP server to respond on port 8080 (default 12 seconds).
3. Runs **Install-Extensions-And-SmokeTest.ps1**: copies GitHub Copilot / Amazon Q from `.cursor\extensions` or `.vscode\extensions` into `%APPDATA%\RawrXD\extensions`, then runs the HTTP smoke test.

Options: `-WaitSeconds 20`, `-Port 8080`. If the smoke test fails because nothing is listening on 8080, see **Troubleshooting** below.

---

## 7. Troubleshooting: HTTP server not on 8080

If the smoke test reports that nothing is listening on port 8080:

1. **Check the RawrEngine window** — You should see: `[CompletionServer] Listening on port 8080...` after startup. If you see `Failed to bind port 8080`, another process is using the port.
2. **Use the correct build** — Prefer `build_ide\RawrEngine.exe`; the same binary is often also built as `build\RawrEngine.exe`. Build with:  
   `cmake -B build_ide -G Ninja && cmake --build build_ide --config Release --target RawrEngine`
3. **Run RawrEngine manually first** — In a terminal: `.\build_ide\RawrEngine.exe --port 8080`, wait until you see the listening message, then in another terminal run `.\SmokeTest-AgenticIDE.ps1`.
4. **Optional longer wait** — Use `.\SmokeTest-AgenticIDE.ps1 -WaitForPortSec 30` to give the server more time to start.

---

## 8. Extension install without runtime changes (next phases)

All extension install paths are unified so that **extensions install once and work without modifying runtime or install location**:

- **VSIX install (IDE)** → `%APPDATA%\RawrXD\extensions\<extension_id>` (see **VSIXInstaller.hpp**).
- **Sidebar / extension list** → Scans the same `%APPDATA%\RawrXD\extensions` (see **Win32IDE_Sidebar.cpp**).
- **ExtensionLoader** (native/VSIX loader) → Uses `%APPDATA%\RawrXD\extensions` (see **ExtensionLoader.hpp**).
- **Ship ExtensionManager** → Uses `%APPDATA%\RawrXD` for extensions and registry (see **RawrXD_ExtensionMgr.hpp**).

Scripts **Install-Extensions-And-SmokeTest.ps1** and **Launch-And-Test.ps1** copy only Copilot/Amazon Q–style extensions into that dir by default; use `-CopyAll` on the install script to copy every extension from Cursor/VS Code. No changes to the RawrEngine or IDE binaries are required for new extensions: install (or copy) into `%APPDATA%\RawrXD\extensions` and restart or rescan.

**Next logical phases**

- Launch Win32 IDE → **Agent** menu → **Run Smoke Test** (in-app agentic smoke).
- **AI** menu → switch to GitHub Copilot or Amazon Q and send a message to smoke-test the extension.
- Run `ollama pull llama3.2` if you use the default model for chat.

---

## 9. Quick reference

| Task                    | Command or action                                      |
|-------------------------|--------------------------------------------------------|
| Launch + extensions + smoke | `.\Launch-And-Test.ps1`                             |
| HTTP smoke test         | `.\SmokeTest-AgenticIDE.ps1`                          |
| GUI smoke test          | IDE → Agent menu → Run Smoke Test                     |
| Start RawrEngine        | `build_ide\RawrEngine.exe` or `build\RawrEngine.exe` (default port 8080) |
| Extension dir           | `%APPDATA%\RawrXD\extensions`                         |
| Agentic config          | `GET/POST http://localhost:8080/api/agentic/config`   |
| Ollama models test      | `.\Test-Ollama-Models-Full.ps1` (see REQUIREMENTS-OLLAMA-TEST.md) |

---

## 10. Related docs

- **REQUIREMENTS-OLLAMA-TEST.md** — Ollama model list and generate test.
- **LOGIC_PHASES.md** — Phase table and HTTP/REPL routes.
- **Ship/QUICK_START.md** — Build and run overview.
