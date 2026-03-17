# RawrXD 100% Codebase Audit

**Scope:** `D:\rawrxd` — every file type relevant to source, config, and build (full repo scan).  
**Rules reference:** `.cursorrules` (2026 Standard).  
**Date:** 2026-03-09.

---

## 1. Repository scope and file counts

| Extension | Count (recursive, excl. .git/build) |
|-----------|-------------------------------------|
| .cpp      | 18,886 |
| .md       | 11,358 |
| .h        | 9,301  |
| .asm      | 9,261  |
| .json     | 7,462  |
| .hpp      | 4,925  |
| .ps1      | 4,260  |
| .txt      | 3,037  |
| .psm1     | 784    |
| .c        | 586    |
| .inc      | 319    |
| .cmake    | 147    |

**Core source (src + Ship + include):** ~3,729 files (`.cpp`, `.h`, `.hpp`, `.c`, `.asm`).

---

## 2. .cursorrules violation summary

### 2.1 Logging: `std::cout` / `cout <<` (forbidden)

- **Rule:** Use `Logger::info/warn/error`, never `std::cout` or `printf`.
- **Total files with `std::cout`/`cout <<`:** 280+ across repo.
- **In `src/`:** 80+ .cpp files (sample below).
- **In `Ship/`:** 13 .cpp files.

**Sample `src/` files using cout (replace with Logger):**

- `src/win32app/main_win32.cpp`
- `src/win32app/VulkanRenderer.cpp`
- `src/win32app/Win32IDE_WebView2.cpp`, `Win32IDE_VSCodeUI.cpp`, `Win32IDE_NativePipeline.cpp`
- `src/agent/agentic_copilot_bridge_new.cpp`, `advanced_autonomous_task_manager.cpp`, `quantum_*`
- `src/core/enterprise_license.cpp`, `agentic_autonomous_orchestrator.cpp`, `universal_stub.cpp`
- `src/config/IDEConfig.cpp`
- `src/main.cpp`, `rawrxd_cli.cpp`, `telemetry.cpp`, `mcp_integration.cpp`
- `src/streaming_gguf_loader.cpp`, `streaming_gguf_loader_mmap.cpp`
- `src/gguf_api_server.cpp`, `complete_server.cpp`, `standalone_web_bridge.cpp`
- Plus many under `src/testing/`, `src/tests/`, `src/test_harness/`, `src/pe_writer_production/`, etc.

**Ship/ files with cout:**  
`RawrXD_CLI_Standalone.cpp`, `RawrXD_TestRunner.cpp`, `RawrXD_Agent_Final.cpp`, `Integration.cpp`, `Example_Application.cpp`, `tools/stress_test.cpp`, `tools/cli_main.cpp`, `test_suite.cpp`, `benchmark_suite.cpp`, etc.

---

### 2.2 printf / fprintf (forbidden for logging)

- **Rule:** No `printf`/`fprintf` for logging; use Logger.
- **In `src/`:** 280+ files with `printf(` or `fprintf(`.

Heavy use in: `src/core/` (e.g. `ssot_handlers.cpp`, `missing_handler_stubs.cpp`, `auto_feature_registry.cpp`, `link_stubs_*.cpp`), `src/win32app/` (many Win32IDE_*.cpp), `src/agent/`, `src/ggml.c`, `src/core/sqlite3.c`, ggml-* backends, telemetry, reverse_engineering, security, etc.

---

### 2.3 Exceptions: throw / catch

- **Rule:** Use `std::expected`; no exceptions.
- **Files with `throw `:** 10+ in `src/`.
- **Files with `catch (`:** 350+ in `src/` (many in ggml-*, backends, and agentic/orchestration code).

Exception use is concentrated in third-party/backend code (ggml, vulkan, sycl, etc.) and some agent/orchestration modules. New RawrXD code should avoid throw/catch and use `std::expected`.

---

### 2.4 Qt (forbidden)

- **Rule:** No Qt; Win32 + `Ship/StdReplacements.hpp` only.
- **Files with Qt includes:** 180+ across repo.

**In active tree (non-archived):**  
`src/QtGUIStubs.hpp`, `src/auth/QuantumAuthUI.cpp`, `Ship/QtMigrationTracker.hpp`, `src/llm_adapter/QuantBackend.cpp`, `GGUFRunner.cpp`, plus tests (e.g. `tests/test_qt_ai_integration.cpp`, `tests/bench_deflate_qt.cpp`).  
Most other Qt references are under `.archived_orphans`, `Full Source`, `RawrXD-ModelLoader`, `history`, `reconstructed` — i.e. archived or alternate trees.

---

### 2.5 Raw new/delete (discouraged)

- **Rule:** Prefer smart pointers; no raw `new`/`delete` without clear ownership.
- **In `src/` .cpp:** 200+ files with `new Type(` or `delete `.

Not all are violations (e.g. placement new, or narrow use in legacy/backend code). Files with many raw new/delete that are good candidates for review: e.g. `src/core/extension_polyfill_engine.cpp`, `src/core/model_training_pipeline.cpp`, `src/qtapp/MainWindow.cpp`, `src/training_dialog.cpp`, `src/tokenizer_selector.cpp`, `src/observability_dashboard.cpp`, `src/hardware_backend_selector.cpp`, `src/metrics_dashboard.cpp`, various ggml-* and agentic files.

---

## 3. Positive patterns (compliance)

### 3.1 Logger / LOG_* usage

- **`LOG_INFO` / `LOG_WARN` / `LOG_ERROR` / `LOG_DEBUG`:** Used in 90+ files under `src/`, especially:
  - `src/win32app/Win32IDE_*.cpp` (AgentCommands, Core, CursorParity, Autonomy, Tier*, Themes, Session, LocalServer, NativePipeline, etc.)
  - `src/agentic/` (e.g. phase_integration_real.cpp)
  - `src/gguf.cpp`, ggml-* backends
- **`Logger::info/warn/error`:** Present in at least `src/agentic/observability/Telemetry.cpp`; LOG_* is the dominant pattern in Win32 IDE code.

### 3.2 std::expected

- **Files using `std::expected` or `std::unexpected`:** 15 in `src/` (e.g. `stubs.cpp`, `swarm_orchestrator.cpp`, `enhanced_cli.cpp`, `ide_orchestrator.cpp`, `gui_main.cpp`, `backend/vulkan_compute.cpp`, `ai/universal_model_router.cpp`, `ai/token_generator.cpp`, `ai/gguf_parser.cpp`, `agentic_ide_new.cpp`, `agentic/swarm_orchestrator.cpp`, `agentic/coordination/SwarmOrchestrator.cpp`, `agentic/chain_of_thought.cpp`, `agent/eval_framework.hpp`).

### 3.3 StdReplacements / Win32-only types

- **StdReplacements.hpp / WideString / StdMap / StdFile:** Used in a small set under `src/` (e.g. `model_source_resolver.cpp`, `reverse_engineering/binary_patcher.*`, `pe_writer_production/structures/resource_manager.cpp`). Broader adoption of StdReplacements in new code would align with .cursorrules.

---

## 4. Architecture and layout (high level)

- **Entry points:** `src/win32app/main_win32.cpp`, `Win32IDE.cpp`, `src/main.cpp`, plus Ship/ and various test/bench entry points.
- **IDE core:** `src/win32app/Win32IDE*.cpp`, `Ship/RawrXD_AgentCoordinator.cpp`, `Ship/RawrXD_AutonomousAgenticPipeline.cpp`.
- **Build:** `CMakeLists.txt`; Win32 IDE target `RawrXD-Win32IDE`; builds: `build_real_lane`, `build_real`, `build_ninja`, etc.
- **Layers:** UI (Win32/HWND), business logic (C++20), data (SQLite/native), PowerShell bridge (CreateProcess). Rule: no Qt; Win32 + StdReplacements.

---

## 5. Recommendations (priority order)

1. **Logging (high):**  
   Replace `std::cout`/`cout <<` and logging use of `printf`/`fprintf` with `Logger::info/warn/error` (or existing `LOG_*` where that’s the project standard) in:
   - `src/win32app/` (main_win32, VulkanRenderer, Win32IDE_WebView2, Win32IDE_VSCodeUI, Win32IDE_NativePipeline, etc.)
   - `src/agent/`, `src/core/` (enterprise_license, agentic_autonomous_orchestrator, universal_stub, IDEConfig, etc.)
   - `src/main.cpp`, `rawrxd_cli.cpp`, `telemetry.cpp`, `mcp_integration.cpp`
   - Key Ship/ CLI and test apps (RawrXD_CLI_Standalone, Integration, Example_Application, test runners).

2. **Error handling (medium):**  
   Prefer `std::expected` in new code and in refactors of agentic/orchestration and core logic; reduce reliance on exceptions in code that is clearly RawrXD (non-3rd-party).

3. **Qt (medium):**  
   Keep Qt out of the main Win32 IDE path. Treat `src/auth/QuantumAuthUI.cpp`, `Ship/QtMigrationTracker.hpp`, and any remaining Qt in active src/ as technical debt; replace or stub with Win32/StdReplacements when touching those areas.

4. **Memory (lower):**  
   Where ownership is unclear or code is being refactored, replace raw `new`/`delete` with `std::unique_ptr`/`std::shared_ptr` in the files called out in §2.5.

5. **StdReplacements (ongoing):**  
   Use `Ship/StdReplacements.hpp` (WideString, StdMap, StdFile, etc.) for new code and when touching string/container/UI code to keep the codebase Qt-free and consistent.

---

## 6. File-level lists (for tooling or scripts)

- **cout in src:** 80+ .cpp files (see §2.1; grep: `std::cout|cout\s*<<` in `src/**/*.cpp`).
- **cout in Ship:** 13 .cpp files (see §2.1).
- **printf/fprintf in src:** 280+ files (grep: `printf\s*\(|fprintf\s*\(` in `src/**/*.{cpp,c,h,hpp}`).
- **Qt includes:** 180+ files (grep: `#include\s*[<"]Q[A-Za-z]+|#include\s*<Qt` in `**/*.{cpp,h,hpp}`); filter by path to exclude archived/Full Source/history.
- **raw new/delete in src:** 200+ .cpp files (grep: `\bnew\s+[A-Za-z_][A-Za-z0-9_:]*\s*\(|\bdelete\s+` in `src/**/*.cpp`).

---

## 7. Summary

| Category           | Approx. scope        | Status vs .cursorrules   |
|--------------------|----------------------|---------------------------|
| std::cout          | 280+ files           | Violation — use Logger   |
| printf/fprintf     | 280+ in src          | Violation for logging    |
| throw/catch        | 10+ / 350+ in src    | Avoid in new code        |
| Qt includes        | 180+ (many archived) | Violation in active Qt   |
| raw new/delete     | 200+ in src          | Review and reduce        |
| Logger/LOG_*       | 90+ in src           | Compliant                |
| std::expected      | 15 in src            | Compliant / expand       |
| StdReplacements    | 4 in src             | Low adoption — expand    |

**Overall:** The codebase is large and mixed (Win32 IDE, agents, backends, tests, archived trees). The Win32 IDE path and newer agentic code use LOG_* and some std::expected; the main systematic violations are logging via cout/printf and remaining Qt in a few active files. Addressing cout/printf in `src/` and `Ship/` first will bring the most visible alignment with the 2026 .cursorrules.

---

*Audit produced by automated scan + spot checks. Re-run greps from §6 to refresh counts or generate file lists for a specific subfolder or target.*

---

## 8. Batch 1 (P0) Logging Standardization — Executed 2026-03-10

**Scope:** Convert `std::cout`/`std::cerr` to unified `LOG_INFO`/`LOG_WARNING`/`LOG_ERROR` (IDELogger pipeline) in high-impact `src/` files.

**Files patched:**

| File | Change |
|------|--------|
| `src/config/IDEConfig.cpp` | Added `win32app/IDELogger.h`. Replaced 2× `std::cout` and 1× `std::cerr` with `LOG_INFO`/`LOG_ERROR`. |
| `src/win32app/main_win32.cpp` | Replaced headless server listening `std::cout` with `LOG_INFO` (REPL/JSON stdout left as program output). |
| `src/win32app/Win32IDE_NativePipeline.cpp` | Added `IDELogger.h`. Macros `RAWRXD_LOG_INFO`, `RAWRXD_LOG_WARNING`, `RAWRXD_LOG_ERROR` now call `LOG_INFO`/`LOG_WARNING`/`LOG_ERROR` instead of `std::cout`/`std::cerr`; kept `OutputDebugStringA` for diagnostics. |
| `src/telemetry.cpp` | Added `win32app/IDELogger.h`. Replaced telemetry event `std::cout` with `LOG_INFO`. |
| `src/core/enterprise_license.cpp` | Added `../win32app/IDELogger.h` and `<sstream>`. Replaced all `std::cout`/`std::cerr` (21+ sites) with `LOG_INFO`/`LOG_WARNING`/`LOG_ERROR` using string concatenation or `std::ostringstream` where needed. |

**Build:** `ninja RawrXD-Win32IDE` in `build_real_lane` was run to verify; modified units compile.

**Next (Batch 2+):** Remaining P0 `std::cout`/`printf` in other `src/` files; then P1 raw `new`/`delete`, P2 exceptions, Qt purge per §5.
