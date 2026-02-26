# RawrXD v3.0 - Native Agentic AI IDE

> **Win32 Native** | **No Qt Dependencies** | **Agentic Core** | **AVX512 Inference** | **Production Ready**

![Build Status](https://github.com/ItsMehRAWRXD/RawrXD/actions/workflows/build.yml/badge.svg)
![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows%20x64-lightgrey)

**RawrXD v3.0** marks the complete transition to a native Windows architecture, eliminating all legacy Qt dependencies in favor of pure **C++20/Win32 APIs**. It features a fully integrated **Agentic Engine** capable of autonomous deep thinking, file research, and self-correction.

## 🎯 v3.0 Highlights

### 🧠 Agentic Engine
- **Deep Thinking**: Integrated Chain-of-Thought (CoT) reasoning for complex problem solving without external API calls.
- **Deep Research**: Autonomous file system scanning (`FileOps`) and context gathering.
- **Self-Correction**: Automated "Code Surgery" using `AgentHotPatcher` techniques for real-time fixes.
- **Reactor Generation**: Experimental support for generating React Server Components.

### ⚡ Native Inference (AVX512)
- **Custom Inference Engine**: Built from scratch for AVX512-optimized CPU inference (`RawrXDTransformer`).
- **Universal Model Loader**: Supports standard **GGUF** and the experimental **RawrBlob** (flat float) format.
- **Direct-to-Hardware**: Hardware-aware scheduling and memory management.
- **Vulkan Types**: Compute Queue Family detection for hybrid inference.

### 💻 Interactive CLI (`rawrxd_cli.exe`)
The new Native CLI provides a powerful interactive shell for AI interaction and system control:
- `/load <path>`: Load GGUF/Blob models directly.
- `/agent <query>`: Dispatch tasks to the **Advanced Coding Agent**.
- `/patch <target>`: Apply hot-patches to code or running instances.
- `/bugreport`: Generate security and optimization audits using `CliSecurityIssue` scanners.
- **Hotkeys**:
    - `x` : Analyze File (Security/Optimization)
    - `t` : Generate Test Stubs
    - `g` : Toggle Overclock Governor
    - `p` : System Status (Thermal/Power)

## 🛠️ Architecture

### AIIntegrationHub
The **AIIntegrationHub** acts as the central nervous system, routing messages between the CLI, the Inference Engine, and the Agentic Core. It replaces the previous stub-based simulation layer.

### Native Networking
- **Winsock API Server**: Built-in REST API (Port 11434) compatible with Ollama/OpenAI clients.
- **WinHTTP**: Native HTTP client for cloud connectivity.

## 📦 Build Instructions

### Prerequisites
- Visual Studio 2022 (C++20 support)
- CMake 3.20+
- AVX512-capable CPU (Recommended)

### Building (Native Win32)
```powershell
cd RawrXD
mkdir build -Force
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Verification & finalizing
After a successful build, run the verification suite (Qt-free policy, binary checks, Win32 linkage):
```powershell
.\Verify-Build.ps1 -BuildDir ".\build"
```
All seven checks must pass. Optionally generate the source manifest (≈1450 files in `src` + `Ship`):
```powershell
.\scripts\Digest-SourceManifest.ps1 -OutDir ".\build" -Format both
# or: cmake --build build --target source_manifest
```
Open items and ship checklist: **UNFINISHED_FEATURES.md**. Which IDE exe to run: **IDE_LAUNCH.md**. Mac/Linux: **docs/MAC_LINUX_WRAPPER.md** (Wine bootable space).

## 🏗️ Build Targets

| Target | Description |
|--------|-------------|
| `RawrXD_CLI` | **Pure CLI** — full chat + agentic parity with Win32 IDE (`/chat`, `/agent`, `/smoke`, `/tools`) |
| `RawrEngine` | CLI inference engine + agentic core |
| `RawrXD-Win32IDE` | Full Win32 GUI IDE with all subsystems |
| `RawrXD-InferenceEngine` | **Standalone inference** — loads GGUF, emits tokens, no IDE |
| `rawrxd-monaco-gen` | Monaco/React IDE generator |

### Pure CLI (101% Win32 Parity)
```powershell
cmake --build build_ide --target RawrXD_CLI --config Release
# Output: build_ide/bin/RawrXD_CLI.exe — default port 23959
# Commands: /chat, /agent, /smoke, /tools, /subagent, /chain, /swarm, /autonomy
# See Ship/CLI_PARITY.md and AGENTIC_IDE_INTEGRATION.md
```

### Standalone Inference Engine (Phase 6)
```powershell
cmake --build . --config Release --target RawrXD-InferenceEngine
# Usage:
bin\RawrXD-InferenceEngine.exe model.gguf --prompt "Hello" --tokens 256
bin\RawrXD-InferenceEngine.exe model.gguf --interactive
bin\RawrXD-InferenceEngine.exe model.gguf --benchmark
```

## 🔄 Tier System & Phase Deprecation

The original numbered phase system (Phases 0–46) has been superseded by a tier-based maturity model. Phases 7–17 were **merged into core infrastructure**, not abandoned:

| Old Phase | Status | Where It Went |
|-----------|--------|---------------|
| Phase 7 (Security/Policy) | Merged | `agent_policy.h/cpp` — T3-C Hotpatch Safety |
| Phase 8 (Explainability) | Merged | `agent_explainability.cpp` — Agent Transparency |
| Phase 9 (Swarm I/O) | Merged | ASM init sequence + `swarm_coordinator.cpp` |
| Phase 10 (Orchestration) | Merged | `SafetyContract`, `ConfidenceGate`, `ExecutionGovernor` |
| Phase 11 (Swarm Coordinator) | Merged | `RawrXD_Swarm_Network.asm` + `Win32IDE_SwarmPanel.cpp` |
| Phase 12 (Native Debugger) | Merged | `RawrXD_Debug_Engine.asm` + `Win32IDE_NativeDebugPanel.cpp` |
| Phase 13 | Never defined | — |

**Mac/Linux:** Use `./scripts/rawrxd-space.sh` — see **docs/MAC_LINUX_WRAPPER.md** (Wine bootable space).
| Phase 14 (Hotpatch UI) | Merged | `Win32IDE_HotpatchPanel.cpp` + T3-C |
| Phases 15–16 (CFG/SSA) | Merged | `RawrCodex.asm` prerequisites |
| Phase 17 (Type Recovery) | Merged | `RawrCodex.asm` + `enterprise_license.cpp` |
| Phase 18 (Distributed) | Rewritten | Swarm Subsystem (Phase 21) |

### Current Tier Status
- **T3: COMPLETE** — Telemetry Kernel → Deterministic Replay → Hotpatch Safety
- **T4: COMPLETE** — Autonomous Recovery Orchestrator (divergence → symbolize → fix → verify → commit)
- **Inference Engine: Phase 6 compilation target added** — `RawrXD-InferenceEngine.exe`

## ⚠️ Migration Notes (v2.0 → v3.0)
- **Qt Removal**: All `qtapp/` references are deprecated. The core engine is now `src/agentic_engine.cpp` (Native).
- **Simulations**: Legacy simulation stubs (`cli_extras_stubs.cpp`, `stubs.cpp`) have been removed or neutralized.
- **Config**: Settings are now stored in `settings.json` via pure JSON parsing.

---

**Verification:** `Verify-Build.ps1` · **Open work:** `UNFINISHED_FEATURES.md` · **Gap vs top 50:** `docs/TOP_50_GAP_ANALYSIS.md` · *RawrXD v3.0 — Native AI Development*
