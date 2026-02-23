# Phases Index — Start to Finish

Single ordered list of all RawrXD phases with implementation status. Every row is either **Implemented** (with source references) or **Pending** (no scaffolding).

| # | Name | Implementation | HTTP API | REPL |
|---|------|----------------|----------|------|
| 2 | UI Integration (Win32 MessageBox, std::filesystem) | `src/ui/phase2_integration_example.cpp`, `include/Phase2_Foundation.h`, `src/loader/Phase2_Foundation.cpp` | — | — |
| 5 | History & Replay | `agent_history.h/cpp`, `complete_server.cpp` handlers | GET/POST /api/agents/history, POST /api/agents/replay | /history, /replay |
| 6 | Standalone Inference Engine | `src/inference/`, `RawrXD-ModelLoader`, CMake Phase 6 target | — | — |
| 7 | Policy Engine | `agent_policy.h/cpp`, `complete_server.cpp` HandlePolicies* | GET/POST /api/policies/* | /policies, /suggest, /policy accept\|reject\|export\|import, /heuristics |
| 8A | Explainability | `agent_explainability.h/cpp`, HandleExplain* | GET /api/agents/explain, /api/agents/explain/stats | /explain |
| 8B | Backend Switcher | `ai_backend.h/cpp`, HandleBackends* | GET /api/backends, /api/backends/status, POST /api/backends/use | /backend list\|use\|status |
| 9 | Streaming engine registry | CMakeLists Win32 IDE block | — | — |
| 10 | Speculative Decoding | `gpu/speculative_decoder_v2.h`, HandleSpecDec* | GET/POST /api/speculative/* | — |
| 11 | Flash Attention v2 / Distributed Swarm Compilation | `core/flash_attention.h`, HandleFlashAttn*; swarm build | GET /api/flash-attention/*, POST /api/flash-attention/benchmark | — |
| 12 | Extreme Compression / Native Debugger Engine | `activation_compressor.h`, HandleCompression*; debugger | GET/POST /api/compression/* | — |
| 13 | Distributed Pipeline Orchestrator | `core/distributed_pipeline_orchestrator.hpp/cpp`, HandlePipeline* | GET/POST /api/pipeline/* | — |
| 14 | Hotpatch Control Plane | `core/hotpatch_control_plane.hpp/cpp`, HandleHotpatchCP* | GET/POST /api/hotpatch-cp/* | — |
| 15 | Static Analysis Engine | `core/static_analysis_engine.hpp/cpp`, HandleAnalysis* | GET/POST /api/analysis/* | — |
| 15b | Code Linter | `src/core/code_linter.cpp` | — | — |
| 16 | Semantic Code Intelligence | `core/semantic_code_intelligence.hpp/cpp`, HandleSemantic* | GET/POST /api/semantic/* | — |
| 17 | Enterprise Telemetry & Compliance | `core/enterprise_telemetry_compliance.hpp/cpp`, HandleTelemetry* | GET/POST /api/telemetry/* | — |
| 18 | Agent-Driven Hotpatch Orchestration | CMakeLists Win32 IDE; design: PHASE_18_ROADMAP.md | — | — |
| 19 | Agentic Decision Tree + CLI Autonomy | `Ship/AgentOrchestrator.hpp`, CLI autonomy | — | — |
| 20 | Headless CLI / WebRTC P2P / AVX-512 Requant | `cli_headless_systems.h`, `core/webrtc_signaling.h`, MASM AVX-512 | GET /api/webrtc/status | /webrtc |
| 21 | Swarm Bridge + Universal Model Hotpatcher | `core/swarm_decision_bridge.h`, `core/universal_model_hotpatcher.h`, HandleSwarmBridge*, HandleHotpatchModel* | GET /api/swarm/bridge, GET /api/hotpatch/model | /swarm bridge, /hotpatch status |
| 22 | Production Release Engineering | `core/production_release.h`, HandleRelease* | GET /api/release/status | /release |
| 23 | GPU Kernel Auto-Tuner | `core/gpu_kernel_autotuner.h`, HandleTuner* | GET /api/tuner/status, POST /api/tuner/run | /tune |
| 24 | Windows Sandbox Integration | `core/sandbox_integration.h`, HandleSandbox* | GET /api/sandbox/list, POST /api/sandbox/create | /sandbox list\|create |
| 25 | AMD GPU Acceleration | `core/amd_gpu_accelerator.h`, HandleGpu* | GET /api/gpu/status, POST /api/gpu/toggle, GET /api/gpu/features, GET /api/gpu/memory | /gpu, /gpu on\|off\|toggle, /gpu features\|memory |
| 26 | ReverseEngineered Kernel (Scheduler, Conflict, Heartbeat, GPU DMA, Tensor, Timer, CRC32) | `include/reverse_engineered_bridge.h`, HandleScheduler*, HandleConflict*, HandleHeartbeat*, HandleGpuDma*, HandleTensorBench*, HandleTimer*, HandleCrc32* | GET/POST /api/scheduler/*, /api/conflict/*, /api/heartbeat/*, /api/gpu/dma/status, /api/tensor/bench, /api/timer, POST /api/crc32 | /scheduler status, /conflict status, /heartbeat status, /gpu dma status, /tensor bench, /timer, /crc32 |
| 27 | Embedded LSP Server (JSON-RPC 2.0) | CMakeLists Win32 IDE | — | — |
| 27b | LSP ↔ Hotpatch Bridge | CMakeLists Win32 IDE | — | — |
| 28 | MonacoCore Native Editor (Gap Buffer + Tokenizer) | MASM MonacoCore, CMakeLists | — | — |
| 29 | Native PDB Parser / Multi-Arch GPU / GSI Hash / Reference Provider | `pdb_reference_provider`, MASM GSI/reference, CMakeLists | — | — |
| 29.2 | GSI Hash Table + TPI Type Parser / Multi-Reference Provider | MASM sources, CMakeLists | — | — |
| 30 | Unified Accelerator Router | MASM Accelerator Router, CMakeLists | — | — |
| 31 | IDE Self-Audit & Stub Detection | MASM Self-Audit kernel, CMakeLists | — | — |
| 32 | The Final Gauntlet (Pre-Packaging Verification) | CMakeLists Win32 IDE | — | — |
| 32A | Chain-of-Thought Multi-Model Review | CMakeLists | — | — |
| 32B | Tunable Reasoning Pipeline | CMakeLists | — | — |
| 33 | Voice Chat Engine | `core/voice_chat.hpp`, REPL /voice * | — | /voice record\|play\|transcribe\|speak\|devices\|mode\|room\|status\|metrics |
| 34 | Instructions Context Provider / Telemetry Export | CMakeLists, tools.instructions.md | — | — |
| 36 | Cross-Process MMF / QuickJS / Flight Recorder / MCP | CMakeLists Win32 IDE | — | — |
| 37 | Chain-of-Thought Engine (MASM CoT DLL) / Iterative Partial Inference / JS Extension Host | MASM CoT phase39, CoT DLL, CMakeLists | — | — |
| 39 | CoT Enterprise Finishers (Snapshot, Telemetry, Copy Engine, Security) | `src/asm/rawrxd_cot_phase39.asm`, `include/cot_phase39_exports.h` | — | — |
| 41 | Dual-Agent Orchestrator / Build Fix | CMakeLists | — | — |
| 43 | Plugin System (Native Win32 DLL loading) | CMakeLists Win32 IDE | — | — |
| 44 | Voice Automation (TTS Response Reader) | CMakeLists | — | — |
| 45 | RawrCodex / Debug Engine / QuadBuffer / Swarm Network / License / License Shield / Game Engine | MASM RawrCodex, Debug, QuadBuffer, Swarm, License, License Shield; CMakeLists | — | — |
| 46 | VulkanKernel / Live Binary Patcher | MASM VulkanBridge, CMakeLists | — | — |
| 48 | The Final Crucible (Unified Stress-Test Harness) | CMakeLists Win32 IDE | — | — |
| 49 | SocketResponder / TridentBeacon / Copilot Gap Closer | MASM SocketResponder, TridentBeacon; CMakeLists | — | — |
| 50 | AgenticOrchestrator / GGUF Vulkan Loader / LSP-AI Bridge / Streaming QuadBuffer / SelfPatch / Enterprise Hardening | MASM Phase 50 sources; `core/update_signature.cpp`, `enterprise_stress_tests.cpp`, etc. | — | — |
| 51 | Security (Dork Scanner + Universal Dorker) | `src/security/RawrXD_GoogleDork_Scanner.{h,cpp}`, `src/security/RawrXD_Universal_Dorker.{h,cpp}`, `Ship/RawrXD_DorkScanner_MASM.asm`, `Ship/RawrXD_Universal_Dorker.asm`, `complete_server.cpp` HandleSecurity* | GET /api/security/dork/status, POST /api/security/dork/scan, POST /api/security/dork/universal, GET /api/security/dashboard | /security, /dork status |

---

## Implementation key

- **Implemented:** Source files and/or handlers listed; API/REPL wired in `complete_server.cpp` and `main.cpp` where applicable.
- **Pending:** Phase number and name are tracked; implementation not yet present (no stub responses).

## Cross-references

- **Logic phases (API & REPL):** [LOGIC_PHASES.md](LOGIC_PHASES.md)
- **Security sub-phases (IDE + dashboard):** [docs/security/PHASES.md](docs/security/PHASES.md)
- **Phase 13–17 summary:** [docs/PHASE_13_17_SUMMARY.md](docs/PHASE_13_17_SUMMARY.md)
- **Phase 18 roadmap:** [PHASE_18_ROADMAP.md](PHASE_18_ROADMAP.md)
