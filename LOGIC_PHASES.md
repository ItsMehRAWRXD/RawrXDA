# Logic Phases — Forward Order

RawrXD development is organized in **phases**. This document lists the **logic phases** (feature/subsystem) in forward order and how they are wired in the HTTP API and REPL.

## Phase order (API & REPL)

| Phase | Name | HTTP API (complete_server) | REPL / ! commands |
|-------|------|----------------------------|--------------------|
| **5** | History & Replay | GET/POST /api/agents/history, POST /api/agents/replay | /history, /replay |
| **7** | Policy Engine | GET/POST /api/policies, suggestions, apply, reject, export, import, heuristics, stats | /policies, /suggest, /policy accept\|reject\|export\|import, /heuristics |
| **8A** | Explainability | GET /api/agents/explain, /api/agents/explain/stats | /explain |
| **8B** | Backend Switcher | GET /api/backends, /api/backends/status, POST /api/backends/use | /backend list\|use\|status |
| **10** | Speculative Decoding | GET/POST /api/speculative/* | — |
| **11** | Flash Attention v2 | GET /api/flash-attention/status\|config, POST /api/flash-attention/benchmark | — |
| **12** | Extreme Compression | GET/POST /api/compression/* | — |
| **13** | Distributed Pipeline | GET/POST /api/pipeline/* | — |
| **14** | Hotpatch Control Plane | GET/POST /api/hotpatch-cp/* | — |
| **15** | Static Analysis | GET/POST /api/analysis/* | — |
| **16** | Semantic Code Intelligence | GET/POST /api/semantic/* | — |
| **17** | Enterprise Telemetry | GET/POST /api/telemetry/* | — |
| **20** | WebRTC P2P Signaling | GET /api/webrtc/status | /webrtc |
| **21** | Swarm Bridge + Model Hotpatcher | GET /api/swarm/bridge, GET /api/hotpatch/model | /swarm bridge, /hotpatch status |
| **22** | Production Release | GET /api/release/status | /release |
| **23** | GPU Kernel Auto-Tuner | GET /api/tuner/status, POST /api/tuner/run | /tune |
| **24** | Windows Sandbox | GET /api/sandbox/list, POST /api/sandbox/create | /sandbox list\|create |
| **25** | AMD GPU Acceleration | GET /api/gpu/status, POST /api/gpu/toggle, GET /api/gpu/features, GET /api/gpu/memory | /gpu, /gpu on\|off\|toggle, /gpu features\|memory |
| **26** | ReverseEngineered Kernel | GET/POST /api/scheduler/*, /api/conflict/*, /api/heartbeat/*, /api/gpu/dma/status, /api/tensor/bench, /api/timer, /api/crc32 | — |
| **51** | Security (Dork Scanner + Universal Dorker) | GET /api/security/dork/status, POST /api/security/dork/scan, POST /api/security/dork/universal, GET /api/security/dashboard | /security, /dork status |

## Continuing logic phases forward

- **Phase 20–25** are wired in `complete_server.cpp`: each documented endpoint above is implemented and delegates to the corresponding singleton (WebRTCSignaling, SwarmDecisionBridge, UniversalModelHotpatcher, ProductionReleaseEngine, GPUKernelAutoTuner, SandboxManager, AMDGPUAccelerator).
- **Phase 51 (Security)** — Implemented: HTTP handlers in `complete_server.cpp` (HandleSecurityDorkStatusRequest, HandleSecurityDorkScanRequest, HandleSecurityDorkUniversalRequest, HandleSecurityDashboardRequest); REPL `/security` and `/dork status` in `main.cpp`. IDE menu/dashboard: [docs/security/PHASES.md](docs/security/PHASES.md) Phase B.
- **Full phase list (start to finish):** [PHASES_INDEX.md](PHASES_INDEX.md) — all phases in order with implementation references; no scaffolding-only entries.
- **Agentic/config** (operation mode, model selection, modelInstanceOverrides) is exposed via GET/POST /api/agentic/config and used in the chat path (Ask = no tools, Plan = plan-only).
- **Models** (Ollama + local): GET /v1/models and GET /api/models merge local GGUF and live Ollama /api/tags; optional `model` in POST /api/chat and POST /api/agent/wish routes to Ollama when provided.

To **continue logic phases forward**:

1. Add the next phase number (e.g. 27, 28) to this table with its API routes and REPL commands.
2. Implement the route block and handlers in `complete_server.cpp` (and declare in `complete_server.h`).
3. Update `main.cpp` help text so the new endpoints are listed.
4. Optionally add init/shutdown in `main.cpp` or Win32IDE_Core if the phase has a singleton or subsystem.
