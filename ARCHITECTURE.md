# RawrXD Architecture — v7.6.0 (Stable)

> Governed Agent Execution Platform with History, Replay, Failure Intelligence, Adaptive Policy, and Explainable LLM Routing

---

## 1. System Overview

RawrXD is a **local-first, deterministic agentic platform** that loads GGUF models directly and provides multi-agent orchestration with full governance. It runs entirely on the user's machine — no cloud dependencies, no telemetry, no external API calls required.

**Three build targets** share a common agentic core:

| Target | Type | Purpose |
|--------|------|---------|
| `RawrEngine` | Console + HTTP | CLI REPL + REST API on port 8080 |
| `rawrxd-monaco-gen` | Codegen tool | Generates Vite/Monaco/Tailwind React IDEs |
| `RawrXD-Win32IDE` | Win32 GUI | Full native IDE with Direct2D rendering |

**Build requirements:** CMake 3.20+, C++20, MinGW GCC 15+ or MSVC 2022.

---

## 2. Execution Model

```
User Request
    │
    ▼
┌──────────────┐     ┌──────────────────┐
│ AgenticEngine │◄────│ PolicyEngine     │  (Phase 7)
│ (inference)   │     │ (evaluate rules) │
└──────┬───────┘     └──────────────────┘
       │
       ▼
┌──────────────────────────────────────────────────┐
│              SubAgentManager                      │
│                                                   │
│  ┌─────────┐  ┌───────────┐  ┌────────────────┐ │
│  │ SubAgent│  │   Chain   │  │  HexMag Swarm  │ │
│  │ (single)│  │(sequential)│  │  (parallel)    │ │
│  └─────────┘  └───────────┘  └────────────────┘ │
│                                                   │
│  ┌─────────────────────────────────────────────┐ │
│  │ AgentHistoryRecorder (append-only JSONL)    │ │  (Phase 5)
│  └─────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────┘
```

### Execution Primitives

- **SubAgent** — A single inference task with lifecycle: `Pending → Running → Completed|Failed|Cancelled`
- **Chain** — Sequential pipeline where each step's output feeds the next step's input via `{{input}}` template substitution
- **HexMag Swarm** — Parallel fan-out of N tasks across up to `maxParallel` threads, with configurable merge strategies (`concatenate`, `vote`, `summarize`, custom prompt)
- **Tool Dispatch** — Automatic detection and execution of tool calls in model output (subagent spawning, todo lists, chain invocation, swarm launch)

### Threading Model

- One `std::thread` per SubAgent execution
- Swarms use a semaphore pattern (`condition_variable` + atomic counter) to cap parallelism
- All shared state protected by `std::mutex` / `std::lock_guard`
- No recursive locks. No manual unlock. No `std::function` in hot paths.

---

## 3. History & Replay (Phase 5)

**File:** `agent_history.h/cpp`

Every agentic operation emits an `AgentEvent` into an append-only log:

```
AgentEvent
├── id              (auto-incrementing int64)
├── eventType       (16 types: agent_spawn, chain_step, swarm_merge, etc.)
├── sessionId       (process run identifier)
├── timestampMs     (epoch UTC)
├── durationMs      (operation duration)
├── agentId         (SubAgent ID)
├── parentId        (parent agent/session)
├── description     (human-readable summary)
├── input/output    (truncated to configurable max bytes)
├── metadata        (JSON blob — tool name, strategy, step index)
├── success         (bool)
└── errorMessage    (non-empty on failure)
```

**Persistence:** JSONL files in `--history-dir` (default: `./history/`). One JSON object per line, append-only. Loaded into memory on startup.

**Query:** `HistoryQuery` supports filtering by session, agent, event type, parent, time range, limit/offset.

**Replay:** Re-executes a previously recorded chain or swarm from its event timeline via `SubAgentManager`. Supports dry-run mode.

**Retention:** Configurable via `setRetentionDays()`. `purgeExpired()` removes old events.

---

## 4. Adaptive Intelligence & Policy (Phase 7)

**File:** `agent_policy.h/cpp`

### Design Philosophy

> "iptables for agent behavior" — explicit, versioned, auditable, human-editable rules.
> No ML. No RL. No opaque scoring. The human is always the decision authority.

### Policy Schema

```
AgentPolicy
├── id / name / description / version
├── PolicyTrigger
│   ├── eventType          (match specific event types)
│   ├── failureReason      (substring match on errors)
│   ├── taskPattern        (substring match on description)
│   ├── toolName           (match specific tools)
│   ├── failureRateAbove   (threshold from heuristics)
│   └── minOccurrences     (data sufficiency requirement)
├── PolicyAction
│   ├── maxRetries / retryDelayMs
│   ├── preferChainOverSwarm
│   ├── reduceParallelism
│   ├── timeoutOverrideMs
│   ├── confidenceThreshold
│   ├── addValidationStep / validationPrompt
│   └── customAction
├── enabled / requiresUserApproval / priority
└── createdAt / modifiedAt / createdBy / appliedCount
```

### How Policies Affect Execution

1. **Before `spawnSubAgent()`** — PolicyEngine evaluates trigger conditions. Matched actions logged.
2. **Before `executeChain()`** — Policy evaluation informs chain configuration.
3. **Before `executeSwarm()`** — Policies can:
   - Reduce `maxParallel` (e.g., resource contention detected)
   - Override `timeoutMs` (e.g., slow operations observed)
   - **Redirect swarm → chain** (if `preferChainOverSwarm` is set)

### Heuristic Computation

`PolicyEngine::computeHeuristics()` scans the full event history and produces per-event-type and per-tool statistics:

- **Success rate** (success / total)
- **Avg / P95 duration** (from all events with duration > 0)
- **Top failure reasons** (grouped and ranked by count)

### Suggestion Generation

Four deterministic algorithms, each guarded against duplicates:

| # | Trigger | Suggested Action | Threshold |
|---|---------|-----------------|-----------|
| 1 | Success rate < 70% | Add retry (max 2, 1s delay) | ≥5 events |
| 2 | P95 duration > 30s | Extend timeout to P95 × 1.5 | ≥5 events |
| 3 | Swarm success < 80% | Prefer chain over swarm | ≥3 events |
| 4 | Agent success 50–85% | Add validation sub-agent | ≥10 events |

Every suggestion includes: rationale (human-readable), estimated improvement (0–1.0), supporting event count, affected event types.

**Critical:** Suggestions are **never auto-applied**. The user must explicitly `/policy accept <id>` or click Accept in the UI.

### Export / Import

Policies serialize to portable JSON. Teams can share policies via:
- CLI: `/policy export <file>` / `/policy import <file>`
- HTTP: `GET /api/policies/export` / `POST /api/policies/import`
- React: Download/Upload buttons in PolicyPanel

Imported policies get fresh UUIDs and `createdBy: "import"` attribution.

---

## 5. Inference Stack (Phases 8B–9B)

### Data Flow

```
UI / CLI / HTTP Request
       │
       ▼
┌──────────────────────────────┐
│  Explainability (Phase 8A)   │  Observability hooks, decision audit
└─────────────┬────────────────┘
              │
              ▼
┌──────────────────────────────┐
│  LLM Router (Phase 8C)       │  Task classification → capability scoring
│  routeWithIntelligence()     │  → failure demotion → fallback
└─────────────┬────────────────┘
              │
              ▼
┌──────────────────────────────┐
│  Backend Switcher (Phase 8B) │  Active backend dispatch
│  routeInferenceRequest()     │  5 backends: LocalGGUF, Ollama, OpenAI, Claude, Gemini
└─────────────┬────────────────┘
              │
              ▼
┌──────────────────────────────┐
│  Execution Engine            │
│  ├── Local GGUF (CPU)        │
│  ├── GPU-DX12-Compute (9B)   │  ← DX12 dispatch, VRAM, fence sync
│  ├── Ollama (local GPU)      │
│  ├── OpenAI (remote API)     │
│  ├── Claude (remote API)     │
│  └── Gemini (remote API)     │
└──────────────────────────────┘
```

### Backend Switcher (Phase 8B)

**Files:** `ai_backend.h` (RawrEngine), `Win32IDE_BackendSwitcher.cpp` (Win32IDE)

Runtime-selectable AI backends without touching inference logic:

- **5 backends** — LocalGGUF (default), Ollama, OpenAI, Claude, Gemini
- **Config per backend** — endpoint, model, API key, timeout, max tokens, temperature
- **Health probing** — async HTTP health checks with latency measurement
- **Persistence** — `backends.json` (RawrEngine) / session-relative config (Win32IDE)
- **Thread-safe** — `std::mutex` + `std::lock_guard` on all state

### LLM Router (Phase 8C)

**File:** `Win32IDE_LLMRouter.cpp` (1,040 lines)

Task-based intelligent routing that sits above the Backend Switcher:

- **8 task types** — Chat, CodeGeneration, CodeReview, CodeEdit, Planning, ToolExecution, Research, General
- **Capability scoring** — per-backend profiles (context window, tool support, cost tier, quality)
- **Failure demotion** — consecutive failures trigger automatic backend demotion
- **Explicit fallback** — auditable, logged, never silent; original active backend always restored
- **Passthrough mode** — when disabled, zero overhead
- **Persistence** — `router.json` for task preferences and capability overrides

Full reference: [`LLM_ROUTER.md`](LLM_ROUTER.md)

### GPU Backend Bridge (Phase 9B)

**Files:** `src/core/gpu_backend_bridge.h` (180 lines), `src/core/gpu_backend_bridge.cpp` (860 lines)

DirectX 12 compute bridge connecting the streaming engine registry to actual GPU hardware:

- **Dynamic DX12 loading** — `LoadLibrary("d3d12.dll")` / `LoadLibrary("dxgi.dll")`, no compile-time DX12 headers needed (MinGW-safe)
- **Best adapter selection** — DXGI factory enumeration, picks GPU with most dedicated VRAM
- **Compute command queue** — `D3D12_COMMAND_LIST_TYPE_COMPUTE`, fence synchronization, async dispatch
- **COM vtable wrappers** — all DX12 API calls go through vtable index (no `d3d12.h` include)
- **GPU capability detection** — vendor ID, shader model, FP16/INT8/FP64 support, wavefront size
- **VRAM tracking** — logical allocation tracking, quota enforcement
- **Registry integration** — registered as "GPU-DX12-Compute" engine with live function pointers
- **Graceful fallback** — if DX12 unavailable, falls back to CPU AVX-512 without crash
- **Thread-safe** — `std::mutex` + `std::atomic` on all state

Namespace: `RawrXD::GPU`, singleton: `getGPUBackendBridge()`

---

## 6. Surfaces

### CLI REPL (RawrEngine)

Full control plane with structured output:

| Command | Phase | Purpose |
|---------|-------|---------|
| `/chat <msg>` | Core | Chat with model |
| `/subagent <prompt>` | P4 | Spawn sub-agent |
| `/chain <s1> \| <s2>` | P4 | Sequential pipeline |
| `/swarm <p1> \| <p2>` | P4 | Parallel fan-out |
| `/agents` | P4 | List all sub-agents |
| `/history [agent_id]` | P5 | Query event log |
| `/replay <agent_id>` | P5 | Re-execute from history |
| `/stats` | P5 | History statistics |
| `/policies` | P7 | List active policies |
| `/suggest` | P7 | Generate suggestions from heuristics |
| `/policy accept <id>` | P7 | Accept a suggestion |
| `/policy reject <id>` | P7 | Reject a suggestion |
| `/policy export <file>` | P7 | Export to JSON |
| `/policy import <file>` | P7 | Import from JSON |
| `/heuristics` | P7 | Compute & display heuristics |
| `/backend list` | P8B | List registered backends |
| `/backend use <id>` | P8B | Switch active backend |
| `/backend status` | P8B | Show active backend info |

### HTTP API (port 8080)

| Method | Path | Phase |
|--------|------|-------|
| `GET` | `/status` | Core |
| `POST` | `/complete` | Core |
| `POST` | `/complete/stream` | Core |
| `POST` | `/api/chat` | Core |
| `POST` | `/api/subagent` | P4 |
| `POST` | `/api/chain` | P4 |
| `POST` | `/api/swarm` | P4 |
| `GET` | `/api/agents` | P4 |
| `GET` | `/api/agents/status` | P4 |
| `GET/POST` | `/api/agents/history` | P5 |
| `POST` | `/api/agents/replay` | P5 |
| `GET/POST` | `/api/policies` | P7 |
| `GET` | `/api/policies/suggestions` | P7 |
| `POST` | `/api/policies/apply` | P7 |
| `POST` | `/api/policies/reject` | P7 |
| `GET` | `/api/policies/export` | P7 |
| `POST` | `/api/policies/import` | P7 |
| `GET` | `/api/policies/heuristics` | P7 |
| `GET` | `/api/policies/stats` | P7 |
| `GET` | `/api/backends` | P8B |
| `GET` | `/api/backends/status` | P8B |
| `POST` | `/api/backends/use` | P8B |
| `GET` | `/api/router/status` | P8C |
| `GET` | `/api/router/decision` | P8C |
| `GET` | `/api/router/capabilities` | P8C |
| `POST` | `/api/router/route` | P8C |

### React IDE (rawrxd-monaco-gen)

Generates a complete Vite + Monaco + Tailwind project with:
- Code editor with AI completions
- SubAgent control panel
- History & Replay panel
- **Policy panel** with Suggestions / Policies / Heuristics tabs
- **Backend panel** with backend list, switching, health status
- **Router panel** with stats, capabilities, dry-run test routing

### Win32 GUI IDE (RawrXD-Win32IDE)

Native Win32 application with Direct2D/DirectWrite rendering:
- Full editor with syntax highlighting, ghost text, annotations
- Agent bridge to SubAgentManager
- Failure intelligence and detection
- Autonomy system with plan execution
- Agent history integration
- Backend Switcher with 5-backend support + health probing
- LLM Router with task-based intelligent routing + explicit fallback

---

## 7. File Map (Active Build Targets)

### Core Engine (shared by all targets)
```
src/agentic_engine.{h,cpp}       — Model inference wrapper
src/subagent_core.{h,cpp}        — SubAgent, Chain, Swarm orchestration
src/agent_history.{h,cpp}        — Event log, query, replay (Phase 5)
src/agent_policy.{h,cpp}         — Policy engine, heuristics, suggestions (Phase 7)
src/ai_backend.h                  — Backend registry & switcher (Phase 8B, header-only)
src/complete_server.{h,cpp}      — HTTP server with all API routes
src/cpu_inference_engine.{h,cpp}  — GGUF model loading and token generation
src/main.cpp                      — RawrEngine entry point (REPL + HTTP)
```

### React Codegen
```
src/monaco_gen.cpp                — CLI entry point for codegen
src/engine/react_ide_generator.{h,cpp} — Generates full React IDE projects
```

### Win32 IDE
```
src/win32app/Win32IDE.{h,cpp}     — Main window and message loop
src/win32app/Win32IDE_Core.cpp    — Core editor functionality
src/win32app/Win32IDE_SubAgent.{h,cpp} — SubAgent bridge
src/win32app/Win32IDE_AgentHistory.cpp — History integration
src/win32app/Win32IDE_FailureIntelligence.cpp — Failure detection
src/win32app/Win32IDE_Autonomy.{h,cpp} — Autonomous plan execution
src/win32app/Win32IDE_PlanExecutor.cpp — Plan step execution
src/win32app/Win32IDE_BackendSwitcher.cpp — 5-backend switcher (Phase 8B)
src/win32app/Win32IDE_LLMRouter.cpp — Task-based routing (Phase 8C)
+ 18 more Win32IDE_*.cpp files
```

---

## 8. Design Invariants

These are **non-negotiable** properties of the system:

1. **No exceptions in core paths** — Structured results (`PatchResult`, `PolicyEvalResult`) everywhere
2. **No global state** — All engines are injected via pointers (`setPolicyEngine()`, `setHistoryRecorder()`)
3. **No autonomy creep** — Policies suggest, humans decide. `requiresUserApproval` defaults to `true`
4. **No circular includes** — Strict header dependency ordering: `policy.h` → `history.h` → `subagent.h`
5. **Thread-safe by construction** — `std::mutex` + `std::lock_guard` on all shared containers
6. **Append-only history** — Events are never modified or deleted (only purged by retention policy)
7. **Platform-independent core** — `subagent_core`, `agent_history`, `agent_policy` have zero platform dependencies

---

## 9. Build

```bash
# Configure
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -B build

# Build all targets
cmake --build build --config Release -j12

# Artifacts
build/RawrEngine.exe              # Console + HTTP
build/bin/rawrxd-monaco-gen.exe   # React IDE generator
build/bin/RawrXD-Win32IDE.exe     # Win32 GUI IDE
```

### Runtime Directories

| Flag | Default | Purpose |
|------|---------|---------|
| `--history-dir` | `./history/` | JSONL event logs |
| `--policy-dir` | `./policies/` | Policy + suggestion JSON |
| `--model` | (none) | Path to GGUF model file |
| `--port` | `8080` | HTTP server port |

---

## 10. Phase History

| Phase | Milestone | Key Deliverable |
|-------|-----------|------------------|
| 1–3 | Core inference + surfaces | GGUF loading, tokenization, HTTP, CLI, Win32 IDE |
| 4 | Multi-agent orchestration | SubAgent, Chain, Swarm, tool dispatch, portable core |
| 5 | Memory | Append-only event history, timeline query, session replay |
| 6 | Failure intelligence | Detection, classification, retry strategies |
| 7 | Governance | Policy engine, heuristics, suggestions, export/import |
| 8A | Command Palette polish | 159 commands, MRU ordering, category filters, HFONT leak fix |
| 8B | Backend Switcher | 5-backend abstraction, health probing, HTTP endpoints, config persistence |
| 8C | LLM Router | Task classification, capability scoring, failure demotion, explainable fallback |
| 9.1 | K-quant dequantization | Q2_K/Q3_K/Q4_K/Q5_K/Q6_K/F16 MASM AVX-512 kernels |
| 9A | LSP Client Bridge | clangd/pyright/typescript-language-server integration |
| 9B | GPU Backend Bridge | DX12 compute dispatch, VRAM management, streaming engine registry wiring |

**v7.7.0-phase9b is the GPU execution layer completion.** DX12 bridge + enterprise license stubs integrated.

---

*Last updated: v7.7.0-phase9b — February 2026*
