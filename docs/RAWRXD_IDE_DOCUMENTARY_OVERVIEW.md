# RawrXD IDE — Documentary Overview (Start to Now)

> **Purpose:** A single, fully readable documentary of the RawrXD IDE from its origins through the current Qt-free, agentic, Win32-native platform. This document exists because the project has lacked a single narrative; it is intended to be the canonical “story” of the project.

---

## 1. Origins and Evolution

### 1.1 What RawrXD Is

**RawrXD** is a **local-first, zero-telemetry agentic platform** that:

- Loads **GGUF** (and experimental RawrBlob) models directly.
- Provides **multi-agent orchestration** with governance (policy, failure detection, self-correction).
- Offers **three main deliverables:**
  - **RawrEngine** — CLI REPL + REST API (e.g. port 8080).
  - **RawrXD-Win32IDE** — Full native Windows IDE (Direct2D, hotpatching, 170+ commands).
  - **rawrxd-monaco-gen** — Codegen for Vite/Monaco/Tailwind React IDEs.

Everything runs on the user’s machine; no cloud dependency is required for core functionality.

### 1.2 From Qt to Qt-Free

- **Earlier phase:** The project included Qt-based UI and Qt WebEngine for embedded browser/chat experiences.
- **Transition:** A deliberate move was made to remove Qt from the core IDE and engine to reduce dependencies, improve portability, and simplify builds.
- **Current state (v3.0 / v14.2):** The **Win32 IDE** is **Qt-free**, built with:
  - **C++20** and **Win32 APIs**
  - **Direct2D / DirectWrite** for text and syntax
  - **D3D11 / DirectComposition** for overlays and smooth UI
  - **MASM64** (optional) for ASM kernel acceleration (inference, hotpatch, reverse engineering)

All `qtapp/` references are deprecated; the core engine is native (`src/agentic_engine.cpp`, `src/win32app/`).

### 1.3 Tier and Phase History

The old “Phase 0–46” numbering was superseded by a **tier-based maturity model**. Many phases were **merged into core**, not abandoned:

| Old Phase | Status | Where It Went |
|-----------|--------|----------------|
| Phase 7 (Security/Policy) | Merged | `agent_policy.h/cpp` — T3-C Hotpatch Safety |
| Phase 8 (Explainability) | Merged | `agent_explainability.cpp` |
| Phase 9 (Swarm I/O) | Merged | ASM init + `swarm_coordinator.cpp` |
| Phase 10 (Orchestration) | Merged | SafetyContract, ConfidenceGate, ExecutionGovernor |
| Phase 11 (Swarm Coordinator) | Merged | RawrXD_Swarm_Network.asm + Win32IDE_SwarmPanel.cpp |
| Phase 12 (Native Debugger) | Merged | RawrXD_Debug_Engine.asm + Win32IDE_NativeDebugPanel.cpp |
| Phase 14 (Hotpatch UI) | Merged | Win32IDE_HotpatchPanel.cpp + T3-C |
| Phases 15–17 (CFG/SSA/Type Recovery) | Merged | RawrCodex.asm + enterprise_license.cpp |
| **14.2** | **Current** | Hotpatch UI integration, docs consolidation |
| **18** | **v15.0.0** | Distributed inference scheduler, Swarm Coalescence Protocol (SCP) |

---

## 2. Architecture in Brief

- **Build:** CMake 3.20+, C++20, MinGW GCC 15+ or MSVC 2022, Windows SDK. MASM64 optional.
- **Core layout:** See root **ARCHITECTURE.md** for the full directory tree. Key areas:
  - `src/` — Main C++ (engine, agentic, codec, core, server, agent, asm, reverse_engineering, config, win32app).
  - `src/win32app/` — Win32 IDE (44+ files: Win32IDE*.cpp, main_win32.cpp, etc.).
  - `src/asm/` — MASM64 kernels (inference, FlashAttention, quant, memory_patch, byte_search, request_patch, RawrCodex).
  - `src/reverse_engineering/` — Reverse engineering suite (RawrCodex, Omega suite, deobfuscator, reverser compiler, pe_tools, etc.).
- **Three-layer hotpatch:** Memory (VirtualProtect), Byte-Level (mmap GGUF), Server (request/response). Coordinated by `UnifiedHotpatchManager`; IDE commands 9001–9017.
- **Execution model:** AgenticEngine → PolicyEngine → SubAgentManager (subagents, chain, swarm) → Failure Recovery (FailureDetector, AgenticPuppeteer). Append-only agent history (JSONL).

---

## 3. Important Note: GitHub Actions and Your Local Machine

**GitHub Actions (including the “Self-Release” nightly) do not run on your computer.**

- All workflows (e.g. `.github/workflows/self_release.yml`, `main.yml`, `build.yml`) execute on **GitHub’s servers** (e.g. `windows-latest` runners).
- They **cannot** access, modify, or delete files on your local disk.
- **Self-Release** (triggered on a schedule and/or `workflow_dispatch`):
  - Checks out the repo on a GitHub runner.
  - Builds the agent (e.g. `RawrXD-Agent`).
  - Runs the agent with a fixed “wish” (e.g. improve inference speed).
  - Does **not** touch your PC.

If you are seeing **local files being deleted or changed**, the cause is **not** GitHub Actions. Possible causes include:

- Another sync tool (OneDrive, Dropbox, Google Drive, etc.) syncing or overwriting the folder.
- A script or tool running locally (e.g. cleanup, “reset” script, or another automation).
- Accidental deletion or overwrite (e.g. restore from backup that didn’t include recent files).
- Malware (worth checking with a security scan if the behavior is unexplained).

If you prefer to avoid the nightly self-release runs, you can disable the workflow:

- In the repo: **Actions** → **Self-Release** → **Disable workflow**, or remove/rename `.github/workflows/self_release.yml` and commit.

---

## 4. RawrXD Standalone Web Bridge (Qt-Free Browser TCP Bypass)

This section documents the **reverse-engineered, Qt-free** web bridge that bypasses browser TCP limitations without any Qt dependencies.

### 4.1 Problem Solved

- **Browser limitation:** HTML/JavaScript cannot open TCP listening sockets or accept inbound connections.
- **Qt-free solution:** A **pure C++ HTTP/WebSocket server** that serves HTML UIs and talks to a GGUF server over TCP, using standard web protocols for browser ↔ app IPC.

### 4.2 Architecture Comparison

**Qt version (original):**

- HTML/JS ↔ Qt WebChannel ↔ Qt Application ↔ TCP ↔ GGUF Server  
- Requires Qt WebEngine and Qt framework.

**Standalone version (Qt-free):**

- HTML/JS ↔ HTTP/WebSocket ↔ Standalone Server (pure C++) ↔ TCP ↔ GGUF Server  
- Any modern browser; no Qt.

### 4.3 Files (Typical Layout)

- **Core:** `src/standalone_web_bridge.hpp` / `.cpp` — main server.
- **Entry:** `src/standalone_main.cpp` — console entry.
- **UI:** `standalone_interface.html` — browser UI.
- **Build:** `CMakeLists-Standalone.txt` — Qt-free CMake.

**Key types:** StandaloneWebAPI, SimpleHttpServer, SimpleWebSocketServer, StandaloneWebBridgeServer.

### 4.4 Build and Run

- **Prereqs:** C++17, CMake 3.16+, no Qt.
- **Build:**  
  `mkdir build-standalone && cd build-standalone`  
  `cmake -C ../CMakeLists-Standalone.txt ..`  
  `cmake --build . --config Release`
- **Run:**  
  `./rawrxd-standalone [http_port] [ws_port] [gguf_port] [gguf_endpoint] [web_root]`  
  Defaults: HTTP 8080, WebSocket 8081, GGUF 11434, web root = current directory.

### 4.5 Web Interface and API

- Open `http://localhost:8080` in a browser.
- **HTTP:** `GET /api/status`, `GET /api/stats`, `POST /api/sendToModel`.
- **WebSocket:** JSON-RPC 2.0, e.g. `method: "sendToModel"`, `params: { prompt, options }`.

### 4.6 When to Use Which

- **Qt version:** When the app is already Qt-based and uses Qt WebEngine.
- **Standalone version:** Web deployment, no Qt, minimal deps, containers, headless, or when you want “any browser” instead of an embedded engine.

---

## 5. Where to Find More

| Topic | Location |
|-------|----------|
| Full architecture, dir tree, hotpatch, ASM, RE suite | **ARCHITECTURE.md** (repo root) |
| Quick start, build, CLI, tiers | **README.md** (repo root) |
| Local-only universal access | **docs/LOCAL_UNIVERSAL_ACCESS.md** |
| Reverse engineering | **docs/REVERSE_ENGINEERING_GUIDE.md**, **src/reverse_engineering/RE_ARCHITECTURE.md** |
| Qt → Win32 parity | **docs/QT_TO_WIN32_PARITY_AUDIT.md** |
| Phases 13–17 | **docs/PHASE_13_17_SUMMARY.md** |
| Changelog | **docs/CHANGELOG.md** |

---

## 6. Recovered Script: minimal_pe_assembler_enhanced.ps1

The script that lived at `D:\minimal_pe_assembler_enhanced.ps1` (referenced in Cursor history as `file:///d%3A/minimal_pe_assembler_enhanced.ps1`) was recovered from Cursor’s local history and restored into the repo so it is not lost:

- **Location:** `scripts/minimal_pe_assembler_enhanced.ps1`
- **Purpose:** Enhanced x86-64 assembler with memory operands, GS/FS prefixes, and PE64 output (x64 compatible).
- **Usage:** `.\scripts\minimal_pe_assembler_enhanced.ps1 -AsmFile payload.asm [-OutExe out.exe]`

---

*This documentary overview is the canonical “start to now” narrative for the RawrXD IDE. For up-to-date build steps and options, always refer to README.md and ARCHITECTURE.md.*
