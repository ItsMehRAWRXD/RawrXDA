# Double Reverse-Engineering + Backwards Fine Print

**Sources reversed:** `bigdaddyg-ide/README.md`, `Win32IDE_AgenticBridge.h/.cpp`, `native_agent.hpp`  
**Purpose:** Two full reverse passes, then backwards-written fine print to raise *nativity* (contract clarity, platform fidelity) and enable **smarter time management** (predictable SLAs, fewer surprises).

---

## Pass 1 — Behavioral & Structural Reverse

### 1.1 BigDaddyG IDE (Electron/React)

| Concern | Reversed behavior |
|--------|--------------------|
| **Identity** | Full IDE: file explorer, Monaco, terminal, project root, local DB. AI (Ollama/BigDaddyG, future Copilot/Amazon Q/Cursor) is optional enhancement. |
| **Platform** | Windows-first; NSIS installer. Linux/macOS (AppImage etc.) scaffolded, secondary. |
| **Stack** | Electron (main: fs, IPC, dialogs), React (CRA), Tailwind, Monaco, **node-pty** (required), **sqlite3** (required), Ollama (optional). |
| **Prereqs** | Node 18+, npm/yarn, Windows build tools for node-pty and sqlite3 native bindings; Ollama only for AI. |
| **Scripts** | `npm start` = React :3000 + wait-on + Electron; `dev:react` / `dev:electron` split; `build` → `build/`; `build:electron` / `pack` / `dist` for packaging. |
| **Config** | `config/providers.json` (AI URLs, models, flags); `config/performance.json` (FPS, memory). Config must ship in `build.files` for packaged app. |
| **Security** | All file/dir access scoped to **project root**; main process validates paths before any `fs` op. |
| **Usage** | Open project → sidebar file click → Monaco; terminal = node-pty; AI = toolbar dropdown + “Toggle Agent” for agentic panel (plan + steps via BigDaddyG). |

### 1.2 Win32 AgenticBridge (C++)

| Concern | Reversed behavior |
|--------|--------------------|
| **Role** | Bridge between Win32 IDE and agentic stack: native CPU inference + AgenticEngine + optional PowerShell subprocess. |
| **Lifetime** | Owned by `Win32IDE*`; holds `unique_ptr<CPUInferenceEngine>`, `unique_ptr<NativeAgent>`, lazy `unique_ptr<SubAgentManager>`. EngineHolder singleton (mutex) for CPU + AgenticEngine. |
| **Init** | `Initialize(frameworkPath, modelName)`; resolves framework path (exe dir, scripts, cwd); gets EngineHolder CPU + agent; sets `m_initialized`. No model load required for init. |
| **Single command** | `ExecuteAgentCommand(prompt)`: hijacker commands (local, no model) → specials (/react-server, /install_vsix, /bugreport, /suggest, /patch) → file-wrapping → `agentEngine->chat()` → `DispatchModelToolCalls` → optional tool result append. |
| **Loop** | `StartAgentLoop(initialPrompt, maxIterations)` sets `m_agentLoopRunning`, runs one `ExecuteAgentCommand`, invokes `m_outputCallback`, then sets running false. (Single-shot, not iterative loop.) |
| **Config** | MaxMode, DeepThinking, DeepResearch, NoRefusal, AutoCorrect, context size (4k–1m), model, Ollama URL; language + file context propagated to NativeAgent. |
| **SubAgents** | `GetSubAgentManager()` lazy-creates via `createWin32SubAgentManager(agentEngine)`, wires IDE callbacks (output, openFile, sendToTerminal, workspace root, list/execute IDE commands). RunSubAgent / ExecuteChain / ExecuteSwarm delegate to manager; swarm post-merge goes through `hookSwarmMerge`. |
| **Tool dispatch** | `DispatchModelToolCalls(modelOutput, toolResult)` → SubAgentManager; then `hookToolResult(toolName, toolResult)` for failure classification. |
| **PowerShell** | SpawnPowerShellProcess (CreatePipe, CreateProcessA, CREATE_NO_WINDOW), ReadProcessOutput (PeekNamedPipe loop, timeout), KillPowerShellProcess. Used when framework path points to scripts; native path uses EngineHolder only. |
| **RE tools** | RunDumpbin, RunCodex, RunCompiler delegate to AgenticEngine. |

### 1.3 NativeAgent (RawrXD)

| Concern | Reversed behavior |
|--------|--------------------|
| **Role** | In-process agent over `CPUInferenceEngine*`: prompt build, streaming, tool-format instructions, language/file context, optional autocorrect. |
| **Prompt** | System string: identity + DeepThink/MaxMode/NoRefusal + full tool list (runSubagent, manage_todo_list, chain, hexmag_swarm, shell, read_file, write_file, …) with TOOL:name:args format; then language + file path; then "User: … Assistant:". |
| **Generation** | Tokenize → GenerateStreaming(2048); thought tags visibility; optional AutoCorrect pass on full response. |
| **High-level** | Ask, Edit, BugReport, Suggest, HotPatch, Compile (simulated), Plan; CreateReactServerPlan. Research = recursive_directory_iterator over code extensions, line search. |

---

## Pass 2 — Invariants, Timing, Failure Modes

### 2.1 Invariants (must hold for “native” behavior)

- **IDE**: File/dir access only under project root; main process is sole authority for path validation.
- **Bridge**: `ExecuteAgentCommand` is reentrant only to the extent EngineHolder and SubAgentManager are thread-safe; one logical “agent loop” at a time (`m_agentLoopRunning`).
- **EngineHolder**: Single CPU engine and single AgenticEngine per process; context limit and config updated before chat.
- **SubAgentManager**: Created once per bridge; workspace root, openFile, sendToTerminal, IDE command list/execute must be set for full tool behavior.
- **NativeAgent**: Model must be loaded before Ask; prompt caps file path at 260 chars; tool format is TOOL:name:args (exact).

### 2.2 Timing / time management

- **ReadProcessOutput**: Timeout `timeoutMs` (default 5000); polling Sleep(100); process exit drains pipe then stops.
- **SubAgent**: `waitForSubAgent(agentId, 120000)` — 2 min default.
- **Swarm**: `timeoutMs = 120000` in SwarmConfig; merge then `hookSwarmMerge`.
- **StartAgentLoop**: Single shot; no internal iteration over `maxIterations` in current code — “loop” is one command + callback.
- **BigDaddyG**: No explicit timeouts in README; dev server (React :3000 + Electron) and build times drive “smarter time management” by making dev/build steps explicit.

### 2.3 Failure modes

- Bridge not initialized → ExecuteAgentCommand returns AGENT_ERROR "Engine Not Initialized".
- Framework path not found → ResolveFrameworkPath returns default path; PowerShell spawn may fail later.
- Tool dispatch fails → hookToolResult receives result; failure classification logged; caller can retry or surface.
- Swarm merge → hookSwarmMerge classifies; failFast = false so partial results still returned.
- Model not loaded (NativeAgent) → Ask prints "[Agent] No model loaded. Use /load <path> first." and returns.

---

## Backwards Fine Print (effect → cause, last clause first)

*Written backwards so obligations and guarantees are ordered from outcome to precondition. This raises *nativity*: the contract reads from “what you get” to “what you must do,” reducing ambiguity and enabling smarter time allocation (clear SLAs, fewer surprises).*

---

**.12** You manage time against explicit phases: install (npm install, native modules), config (providers.json, performance.json), dev (start / dev:react / dev:electron), build (build / build:electron / pack / dist), and runtime (open project, edit, terminal, optional AI). Smarter time management is permitted only to the extent these phases are respected.

**.11** You accept that the Win32 Bridge’s “agent loop” is a single-shot execution plus output callback, not an iterative multi-turn loop; any expectation of multiple automatic turns is outside the current contract and must be implemented above the bridge.

**.10** You accept that tool dispatch is the single funnel: all tool results pass through DispatchModelToolCalls and hookToolResult; failure classification and logging are part of the contract, and retries or user feedback are your responsibility.

**.9** You agree that SubAgent and Swarm timeouts (120000 ms) are the current ceiling for those operations; exceeding them implies cancellation or failure handling by the caller, not by the bridge alone.

**.8** You agree that file and directory access in the Electron app are restricted to the project root chosen at “Open project,” and that the main process validates paths before any fs operation; violations are outside the supported contract.

**.7** You accept that AI features (Ollama/BigDaddyG, agent panel, provider dropdown) are optional; the IDE is fully functional without them, and any time budget for “AI availability” is separate from core edit/terminal/project management.

**.6** You agree that the native stack (CPUInferenceEngine, AgenticEngine, NativeAgent) is process-global and lazy-initialized; first use may incur one-time cost (model load, context sizing), and time estimates must account for it.

**.5** You accept that configuration (MaxMode, DeepThinking, context size, model name, Ollama URL, language/file context) takes effect on the next ExecuteAgentCommand or engine call, not retroactively; ordering of Set* and Execute matters.

**.4** You agree that packaged builds must include config (e.g. config/**/* in build.files) so that providers and performance settings ship with the app; otherwise runtime behavior is undefined for packaged installs.

**.3** You accept that node-pty and sqlite3 require native bindings and Windows build tools at install time; install failures or version skew are outside the “zero-config” guarantee and must be resolved before dev or build phases.

**.2** You agree that Windows is the primary target and Linux/macOS packaging is scaffolded but secondary; parity of behavior and timing on non-Windows hosts is not guaranteed.

**.1** You accept that this fine print is derived from a double reverse-engineering of the stated sources (BigDaddyG IDE README, Win32IDE_AgenticBridge, NativeAgent) and reflects inferred contracts and behavior; it is not a substitute for source code or official documentation. Nativity (contract clarity and platform fidelity) is increased by reading clauses in reverse order (effect → cause) so that outcomes and SLAs are front of mind for smarter time management.

---

**End of reverse-engineering and backwards fine print.**
