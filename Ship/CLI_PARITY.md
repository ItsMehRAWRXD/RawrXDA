# RawrXD CLI — 101% Parity with Win32 GUI

The **RawrXD CLI** provides a pure command-line interface with **full chat and agentic autonomous capabilities**, mirroring the Win32 GUI version at 101% parity.

## Headless IDE (Same Binary, No GUI)

**RawrXD-Win32IDE.exe --headless --repl** — Full 101% parity from the IDE binary: `/chat`, `/agent`, `/wish`, `/subagent`, `/chain`, `/swarm`, tool dispatch, HTTP `/ask` and `/api/tool`. See root `CLI_PARITY.md` for full HeadlessIDE documentation.

## Canonical Pure CLI (Main Build)

**Primary build:** From root `CMakeLists.txt`, target `RawrXD_CLI` (RAWRXD_PURE_CLI=1) or `RawrEngine`. Same binary logic; RawrEngine default port 8080, RawrXD_CLI default 23959 for Win32 IDE connection.

- **RawrXD_CLI.exe** — Full REPL: `/chat`, `/agent`, `/smoke`, `/tools`, `/subagent`, `/chain`, `/swarm`, `/autonomy`, HTTP API
- Built from `src/main.cpp` with full agentic stack (AgenticEngine, SubAgentManager, complete_server)
- **101% Win32 parity** — See `AGENTIC_IDE_INTEGRATION.md` for full capability table

## Gold Headless CLI (RawrXD_Headless_CLI)

- **RawrXD_Headless_CLI.exe** — Built from HeadlessIDE (Gold stack), console subsystem
- Same engine as RawrXD_Gold; uses `cli_main_headless.cpp` entry point
- Full parity: `/chat`, `/agent`, `/wish`, `/tools`, `/smoke`, `/subagent`, `/chain`, `/swarm`, `/agents`
- Default: interactive REPL. `--list`, `--dir`, `--ollama-host`, `--ollama-port`
- Output: `build_ide/bin/RawrXD_Headless_CLI.exe`

## Ship Build (Alternative)

- **RawrXD_CLI.exe** / **RawrXD_Agent_Console.exe** — Built from `Ship/Integration.cpp`
- Uses AgentOrchestrator + ToolExecutionEngine (44+ tools) + Ollama
- Full parity: `/tools`, `/status`, `/smoke`, `/run-tool`, HTTP server on port 23959
- Lighter weight; same agent loop, different tool integration

## Feature Parity

| Feature | Win32 GUI | RawrXD_CLI | Status |
|---------|-----------|------------|--------|
| Interactive chat | ✓ (AI Chat Panel) | ✓ (stdin/stdout) | 100% |
| Streaming responses | ✓ | ✓ | 100% |
| Agentic autonomous mode | ✓ (Agent > Start Agentic) | ✓ (default) | 100% |
| Tool calling (44+ tools) | ✓ | ✓ | 100% |
| read_file, write_file, run_command | ✓ | ✓ | 100% |
| search_files, grep, list_directory | ✓ | ✓ | 100% |
| Conversation history | ✓ | ✓ | 100% |
| Model selection (Ollama) | ✓ | ✓ (--model) | 100% |
| Working directory | ✓ | ✓ (--dir) | 100% |
| Auto-approve tools | ✓ | ✓ (--auto-approve) | 100% |
| HTTP server (IDE integration) | ✓ | ✓ (starts on launch) | 100% |
| View agent tools | ✓ (Agent > View Tools) | ✓ `/tools` (main CLI) | 100% |
| Agent smoke test | ✓ (Agent > Run Smoke Test) | ✓ `/smoke` (main CLI) | 100% |
| Autonomous loop | ✓ (Autonomy > Start) | ✓ `/autonomy start`, `stop`, `pause`, `resume`, `status` | 100% |

## Usage

```powershell
# Interactive chat + agentic mode
RawrXD_CLI.exe

# With options
RawrXD_CLI.exe --model qwen2.5-coder:14b --dir C:\Projects

# List available models and exit
RawrXD_CLI.exe --list

# Help
RawrXD_CLI.exe --help
```

## Options (Main build: RawrXD_CLI from root CMakeLists.txt)

| Option | Description |
|--------|-------------|
| `--model <path>` | Path to GGUF model file (or use Ollama when no model loaded) |
| `--port <port>` | HTTP server port (default **23959** for RawrXD_CLI so Win32 IDE can connect) |
| `--dir <path>` | Working directory for tool execution (list_directory, read_file, run_command) |
| `--list`, `-l` | List Ollama models and exit (requires Ollama running; respects OLLAMA_HOST) |
| `--no-http` | Disable HTTP server |
| `--no-repl` | Disable interactive REPL |
| `--help` | Show help |

## Interactive Commands (main.cpp REPL)

| Command | Description |
|---------|-------------|
| `/chat <msg>` | Chat (GGUF or Ollama when no model) |
| `/agent <prompt> [N]` | Agentic loop (chat + tools, max N cycles) |
| `/autonomy start` | Start autonomous loop (poll→detect→decide→act→verify) |
| `/autonomy stop` | Stop autonomous loop |
| `/autonomy pause` | Pause autonomy loop |
| `/autonomy resume` | Resume paused autonomy loop |
| `/autonomy status` | Show autonomy state and stats |
| `/subagent <prompt>` | Spawn a sub-agent |
| `/chain`, `/swarm` | Run chain or swarm |
| `quit` / `exit` | Exit the CLI |
| `clear` | Clear conversation history |
| `models` / `/models` | List available Ollama models |
| `/tools` | List all agent tools (View Tools parity) |
| `/status` | Show agent state, model, LLM connection |
| `/smoke` | Run agentic smoke test (Agent > Run Smoke Test parity) |
| `/run-tool <name> [json]` | Execute tool directly (e.g. `/run-tool list_directory {"path":"C:\\temp"}`) |

## Build

### Main Pure CLI (Full Parity)

From repo root:

```powershell
cmake -B build_ide -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build_ide --target RawrXD_CLI --config Release
```

Output: `build_ide/bin/RawrXD_CLI.exe` or `build_ide/RawrEngine.exe`. RawrXD_CLI default port **23959**; RawrEngine default **8080**. Both support `/autonomy`, `/chat`, `/agent`, and full REPL.

### Gold Headless CLI (101% parity via HeadlessIDE)

```powershell
cmake --build build_ide --target RawrXD-CLI --config Release
```

Output: `build_ide/bin/RawrXD_Headless_CLI.exe`. Same engine as RawrXD_Gold; defaults to REPL. Use `RawrXD_Gold.exe --headless` for equivalent.

**Quick verification:** `RawrXD_CLI.exe --list` (lists Ollama models and exits); `RawrXD_CLI.exe` then `/help`, `/status`, `/tools`, `/smoke`, `exit` in REPL.

### Ship Full Agentic CLI

**Quick build:** `cd Ship && build_ship_cli.bat`

**Manual CMake:**
```powershell
cd D:\rawrxd\Ship
mkdir build -Force
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target RawrXD_CLI --config Release
```

Output: `build/Release/RawrXD_CLI.exe` (or `build/RawrXD_CLI.exe` depending on generator).

The Win32 IDE looks for, in order: `RawrXD_CLI.exe`, `RawrXD_Agent_Console.exe`, `RawrXD_Agent_GUI.exe` when starting Agentic Mode.

*Note: `build_production.bat` produces `RawrXD_CLI_Lite.exe` (basic GGUF chat only). For full agentic CLI, use `build_ship_cli.bat`.*

## Architecture

RawrXD_CLI uses the same components as the Win32 IDE:

- **AgentOrchestrator** — Multi-turn agent loop with tool execution
- **ToolExecutionEngine** — 44+ tools (file ops, search, commands)
- **LLMClient** — Ollama API integration
- **ToolImplementations** — read_file, write_file, run_command, search_files, grep, etc.

When the Win32 IDE runs "Agent > Start Agentic Mode", it spawns RawrXD_Agent_Console.exe (same binary logic as RawrXD_CLI.exe). The CLI is the same agent, just driven by stdin/stdout instead of the GUI.
