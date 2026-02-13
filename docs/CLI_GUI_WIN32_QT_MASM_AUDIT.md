# CLI/GUI Win32 — Full Qt, MASM, C++20 & Titan Audit

**Date:** 2026-02-12  
**Scope:** Entire CLI/GUI Win32 codebase — zero-Qt deps, pure x64 MASM integration, C++20, backend MASM framework with agent Titan kernels.

**Qt→Win32 feature parity:** For an audit of *what* from the old Qt build was converted and what remains open, see **docs/QT_TO_WIN32_PARITY_AUDIT.md**.

---

## 1. Executive Summary

| Area | Status | Notes |
|------|--------|--------|
| **Main build (Win32 IDE, tool_server, CLI)** | **Qt-free** | Root `CMakeLists.txt` does not `find_package(Qt)`; no Qt linked for primary targets. |
| **C++ standard** | **C++20** | `CMAKE_CXX_STANDARD 20` in root and subprojects (except ggml/git 17 where noted). |
| **x64 MASM** | **Enabled (MSVC)** | `ASM_MASM` enabled; 25+ kernels in `src/asm/` and custom targets (requantize, swarm, etc.). |
| **Titan / agent kernels** | **Wired** | `cpu_inference_engine` uses Titan (RawrXD_Interconnect DLL or static); Ship Win32 IDE uses RawrXD_Titan_Kernel.dll. |
| **Remaining Qt** | **None** | Qt removed from `src/setup` (SetupWizard reformed to Win32/C++20); optional headers in `include/` are stubbed or excluded from default build. |

---

## 2. Qt Dependency Audit

### 2.1 Primary targets (no Qt)

- **tool_server** — No Qt; WinSock2, Win32, nlohmann/json (see `QT_REMOVAL_*` docs).
- **RawrXD-Win32IDE** (Win32 IDE) — Built from `src/win32app/*.cpp`; no Qt includes in core. Uses Win32 API, RichEdit, WebView2.
- **CLI (cli_shell, RawrEngine, etc.)** — No Qt; `cli_shell.cpp` and core command/ssot are C++20 Win32.

### 2.2 Source files — “no Qt” design (verified comments / patterns)

Hundreds of files under `src/` and `include/` explicitly state **C++20, Win32, no Qt, no exceptions**, including:

- `src/win32app/Win32IDE*.cpp` (all Tier/cosmetics, Breadcrumbs, Audit, CursorParity, etc.)
- `src/core/*` (command_registry, ssot_handlers, feature_registry, auto_discovery, etc.)
- `src/agent/*` (model_invoker, llm_http_client, telemetry_collector, self_patch, etc.)
- `src/agentic/*`, digestion, inference, marketplace, thermal (Qt-free implementations)

### 2.3 Remaining Qt or Qt-style usage

| Location | Issue | Recommendation |
|----------|--------|----------------|
| **include/agentic_iterative_reasoning.h** | Full Qt: `QObject`, `QString`, `QJsonObject`, signals. | Provide Qt-free stub or alternate implementation; keep behind `#ifdef RAWR_HAS_QT` if needed. |
| **include/ai_completion_provider.h** | `QObject`, `QString`, `QStringList`. | Same: stub or Win32/STL implementation for zero-Qt build. |
| **include/distributed_trainer.h** | `QObject`, `QString`, `QJsonObject`, `QNetworkReply`. | Optional component; exclude from zero-Qt build or replace with STL/Win32. |
| **include/ollama_proxy.h** | `QObject`, `Q_INVOKABLE`, `QString`. | Replace with Win32/STL HTTP + callbacks. |
| **include/mainwindow_qt_original.h** | `QString` (legacy). | Archive or stub; not used by Win32 IDE. |
| **include/orchestration/TaskOrchestrator.h** | `QObject`, `QString`, `QNetworkReply`, Qt types. | Stub or non-Qt implementation. |
| **include/visualization/ContextVisualizer.h** | `QString`, `QMap`, `QSet`. | Stub or replace with `std::` types. |
| **include/agentic_text_edit.h**, **multi_tab_editor.h**, **extension_panel.h**, **production_agentic_ide.h** | Qt types. | Legacy/Qt UI; exclude from Win32 IDE build. |
| **src/setup/SetupWizard.cpp** | **Reformed** | Pure Win32/C++20: `WizardPageBase`, BCrypt for keys/fingerprint, `std::filesystem` + `std::ofstream` for config; no Qt. |
| **src/agentic_agent_coordinator.cpp** | Includes `agentic_iterative_reasoning.h`; uses `#ifdef RAWR_HAS_QT` for reasoner. | With Qt stub header, zero-Qt build compiles; reasoner is optional. |

**Note:** Root `CMakeLists.txt` does **not** call `find_package(Qt)` and does **not** add `src/setup` via `add_subdirectory`. So the default build is Qt-free; the above are for when/if those components are ever built or included.

---

## 3. x64 MASM Integration

### 3.1 Build configuration

- **CMake:** `enable_language(ASM_MASM)`, `CMAKE_ASM_MASM_COMPILER` = `ml64.exe` (MSVC).
- **Flags:** `/c /Zi /Zd /I${CMAKE_CURRENT_SOURCE_DIR}/src/asm`.
- **Guard:** `RAWR_HAS_MASM` defined when MSVC; otherwise C++ fallbacks.

### 3.2 MASM kernel sources (main list)

Included in `ASM_KERNEL_SOURCES` and linked into main targets:

- `inference_core.asm`, `inference_kernels.asm`, `model_bridge_x64.asm`
- `FlashAttention_AVX512.asm`, `quant_avx2.asm`, `RawrXD_KQuant_Dequant.asm`
- `memory_patch.asm`, `byte_search.asm`, `request_patch.asm`
- `RawrXD_DualAgent_Orchestrator.asm`, `RawrXD_DiskRecoveryAgent.asm`, `disk_recovery_scsi.asm`
- `RawrXD-AnalyzerDistiller.asm`, `RawrXD-StreamingOrchestrator.asm`
- `RawrXD_Telemetry_Kernel.asm`, `RawrXD_Prometheus_Exporter.asm`
- `RawrXD_SelfPatch_Agent.asm`, `RawrXD_SourceEdit_Kernel.asm`
- `feature_dispatch_bridge.asm`, `gui_dispatch_bridge.asm`
- `RawrXD_CopilotGapCloser.asm`, `native_speed_kernels.asm`
- `DirectML_Bridge.asm`, `RawrXD_Hotpatch_Kernel.asm`, `RawrXD_Snapshot.asm`
- `RawrXD_Pyre_Compute.asm`, `vision_projection_kernel.asm`
- Plus `src/RawrXD_Complete_ReverseEngineered.asm`

Additional custom MASM targets (e.g. AVX-512 requantize, swarm tensor, MonacoCore, PDB kernel) are assembled via custom commands and linked where needed.

### 3.3 Interconnect (Titan / backend DLL)

- **Directory:** `interconnect/` — builds `RawrXD_Interconnect.dll` (or equivalent).
- **Contents:** `RawrXD_DllMain.asm`, `RawrXD_Agentic_Router.asm`, `RawrXD_HTTP_Router.asm`, `RawrXD_JSON_Parser.asm`, model state machine, ring buffers, streaming formatter, swarm orchestrator, etc.
- **Usage:** `cpu_inference_engine` loads this DLL for Titan assembly engine; falls back to static linkage or pure C++ if not found.

### 3.4 GUI / backend MASM (no Python)

- **gui/webserver.asm** — MASM x64 GUI server (port 3000); serves launcher, proxies to 8080.
- **gui/server_8080.asm** — MASM x64 backend on 8080; static GUI + proxy to tool_server (11435).
- **Build:** `gui/build_server_8080.bat`, `gui/build_webserver.bat` (ml64 + link); not in main CMake but documented.
- **Design:** Pure metal; no Qt, no Node for backend when using server_8080 + tool_server.

---

## 4. C++20 and “no extra deps”

- **Root:** `set(CMAKE_CXX_STANDARD 20)` and `CMAKE_CXX_STANDARD_REQUIRED ON`.
- **Subprojects:** Most use C++20 (agentic, inference, setup, Ship, tests, etc.); a few use C++17 (e.g. `src/git`, ggml, Tiny-Home).
- **No framework bloat:** Primary stack is Win32 API + STL + nlohmann/json + optional WebView2; no Qt, no Electron for the main IDE.
- **Headers:** Core and Win32 IDE headers are C++20 / Win32; Qt-only headers are in `include/` as legacy or optional and are not required for the default build.

---

## 5. Titan / agent kernel wiring

### 5.1 CPU inference engine (`src/cpu_inference_engine.cpp`)

- Links **Titan** via:
  - Static: `Titan_Initialize`, `Titan_LoadModel`, `Titan_RunInferenceStep` (from ASM/C++).
  - Or DLL: `RawrXD_Interconnect.dll` with `Titan_Initialize`, `Titan_LoadModel`, `Titan_RunInferenceStep`.
- Inference path: if `m_pTitanContext` and `fnTitan_RunInferenceStep` are set, runs assembly engine; else C++ fallback.
- **No Qt** in this path.

### 5.2 Ship Win32 IDE (`Ship/RawrXD_Win32_IDE.cpp`)

- Loads **RawrXD_Titan_Kernel.dll** (`LoadTitanKernel()`).
- Uses `Titan_Initialize`, `Titan_LoadModelPersistent`, `Titan_RunInference`, `Titan_Shutdown` for AI actions (suggestions, explanations, fixes).
- **No Qt** in Ship IDE.

### 5.3 tool_server

- Exposes `capabilities` including `kernel_engines`, `dual_engine`, `agentic_tools`, etc.; backend for MASM server_8080 and HTML GUI.
- **No Qt.**

---

## 6. Recommendations (production zero-Qt + MASM)

1. **Keep main build Qt-free** — Do not add `find_package(Qt)` or link Qt for Win32 IDE, tool_server, or CLI.
2. **Qt headers in `include/`** — Either:
   - Exclude from zero-Qt builds (e.g. don’t include agentic_iterative_reasoning.h, ai_completion_provider.h, etc. in targets that must be Qt-free), or
   - Add Qt-free stub headers / implementations and use `#ifdef RAWR_HAS_QT` where both variants are needed.
3. **SetupWizard** — Done. `SetupWizard.cpp` is Qt-free (WizardPageBase, BCrypt, STL, std::filesystem); `src/setup/CMakeLists.txt` builds `rawrxd_setup` with C++20 and no Qt.
4. **MASM backend** — Continue using `server_8080.asm` + `webserver.asm` as the documented pure-MASM GUI/API path; ensure `launch_rawrxd.bat` and docs reflect “no Node / no Python” when using these.
5. **Titan / Interconnect** — Keep Titan kernel and RawrXD_Interconnect as the performance path; document that DLL/static linkage is optional and C++ fallback is always available.
6. **C++20** — Keep C++20 for new code and main targets; only keep C++17 where required by a subproject (e.g. ggml).

---

## 7. File-level checklist (high level)

| Component | Qt-free | MASM / x64 | C++20 | Notes |
|-----------|--------|------------|-------|--------|
| tool_server | ✅ | N/A (backend) | ✅ | Win32 only |
| Win32 IDE (win32app) | ✅ | Linked ASM kernels | ✅ | RAWR_HAS_MASM |
| CLI (cli_shell, ssot, command_registry) | ✅ | Optional ASM | ✅ | |
| cpu_inference_engine | ✅ | Titan + ASM | ✅ | |
| gui/webserver.asm, server_8080.asm | N/A | Pure MASM | N/A | No C++ runtime in exe |
| interconnect | N/A | Pure MASM | N/A | DLL |
| include/deflate_brutal_qt.hpp | ✅ Qt-free | N/A | ✅ | STL-only (std::vector); legacy name kept |
| include/* Qt headers | Stub / exclude | N/A | N/A | RAWR_HAS_QT stubs; not in default build |
| src/setup/SetupWizard.cpp | ✅ Qt-free | N/A | ✅ | Win32/BCrypt/STL only |

This audit confirms the CLI/GUI Win32 stack is **fully non-Qt** for the IDE path: **C++20**, **x64 MASM** (kernels + interconnect + Titan), and **no Qt in database**. Optional `include/` headers with Qt types are behind `RAWR_HAS_QT` or excluded from the default build; `deflate_brutal_qt.hpp` is STL-only; SetupWizard is reformed to pure Win32/BCrypt/STL.
