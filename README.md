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
mkdir build_native
cd build_native
cmake .. -DENABLE_QT=OFF -DUSE_AVX512=ON -DRAWRXD_BUILD_CLI=ON
cmake --build . --config Release
```

## ⚠️ Migration Notes (v2.0 → v3.0)
- **Qt Removal**: All `qtapp/` references are deprecated. The core engine is now `src/agentic_engine.cpp` (Native).
- **Simulations**: Legacy simulation stubs (`cli_extras_stubs.cpp`, `stubs.cpp`) have been removed or neutralized.
- **Config**: Settings are now stored in `settings.json` via pure JSON parsing.

---
*RawrXD v3.0 - The Future of Native AI Development*
