# RawrXD-Win32IDE build summary

Summary of what was done to get **RawrXD-Win32IDE** building.

## 1. CMake – new sources

- **src/project_context.cpp** – `RawrXD::ProjectContext`
- **src/universal_model_router.cpp** – `UniversalModelRouter`
- **src/ui/monaco_settings_dialog.cpp** – `RawrXD::UI::MonacoSettingsDialog`
- **src/thermal/RAWRXD_ThermalDashboard.cpp** – `rawrxd::thermal::ThermalDashboard`
- **src/win32app/multi_file_search_stub.cpp** – `MultiFileSearchWidget` (destructor and other non-inline methods)
- **src/win32app/benchmark_runner_stub.cpp** – `BenchmarkRunner` destructor and method stubs
- **src/win32app/benchmark_menu_stub.cpp** – `BenchmarkMenu` ctor/dtor and method stubs

Also added **src/thermal** to the IDE target’s include path.

## 2. Header fix

- **include/benchmark_menu_widget.hpp** – enum value `ERROR` renamed to `LOG_ERROR` to avoid conflict with the Windows `ERROR` macro.

## 3. Stub implementations

- **multi_file_search_stub.cpp** – Implements `~MultiFileSearchWidget` and the other declared, non-inline methods of `MultiFileSearchWidget` as no-op stubs.
- **benchmark_menu_stub.cpp** – Implements `BenchmarkMenu` ctor/dtor and all used methods; includes the same “complete type” setup as the runner stub so `unique_ptr<BenchmarkRunner>` can be destroyed.
- **benchmark_runner_stub.cpp** – Includes `gguf_core.h`, `transformer_block_scalar.h`, and `inference_engine_stub.hpp` so `InferenceEngine` and `BenchmarkRunner` have complete types for destructors; defines a minimal `RealTimeCompletionEngine` for `unique_ptr` deletion; defines `InferenceEngine::~InferenceEngine()`, `TransformerBlockScalar::~TransformerBlockScalar()`, and `BenchmarkRunner::~BenchmarkRunner()`; stubs `runBenchmarks`, `stop`, `getResults`.

## Result

- `ninja RawrXD-Win32IDE` in **build_ide** completes with exit code 0.
- Executable: **build_ide/bin/RawrXD-Win32IDE.exe**

---

## To build again

(e.g. after closing the exe if it was locked):

```powershell
cd D:\rawrxd\build_ide
ninja RawrXD-Win32IDE
```

Or from repo root:

```powershell
cd D:\rawrxd
ninja -C build_ide RawrXD-Win32IDE
```

Run the IDE:

```powershell
.\build_ide\bin\RawrXD-Win32IDE.exe
```

**If the IDE showed “8/10” (first-run audit) then exited silently:** The first-run gauntlet is now skipped by default so the IDE always starts. To run the gauntlet on first launch again, set `RAWRXD_RUN_FIRST_RUN_GAUNTLET=1` before starting. You can still run **Gauntlet: Run All Tests** from the menu (Tools / Gauntlet) anytime.

If you need a full reconfigure (e.g. after adding sources in CMake):

```powershell
cd D:\rawrxd\build_ide
cmake ..
ninja RawrXD-Win32IDE
```

---

## Where to find features in the Win32 GUI

Everything below is available directly from the menu bar or shortcuts unless noted.

| Area | Menu / Shortcut | What you get |
|------|------------------|--------------|
| **Model loading** | **File → Load Model** (submenu) | Load GGUF…, From HuggingFace…, From Ollama…, From URL…, **Smart Load (Auto-Detect)** Ctrl+Shift+M, **Quick Load GGUF** Ctrl+M. Uses `UniversalModelRouter` and streaming UX when loading. |
| **Streaming / low memory** | **View → Use Streaming Loader (Low Memory)** | Toggle streaming GGUF loader for large models. |
| **Agent loop** | **Agent → Start Agent Loop** | Multi-turn agentic loop (cycle count from **Agent → Set Cycle Count…**, default 10). |
| **Bounded agent (FIM tools)** | **Agent → Bounded Agent (FIM Tools)** Ctrl+Shift+I | Bounded agent with file-edit tools; diff panel for accept/reject. |
| **Agent tools** | **Agent → View Tools** | List of available agent tools (read_file, write_file, exec_cmd, runSubagent, chain, hexmag_swarm, etc.). |
| **Autonomous behavior** | **Agent → Autonomy** (submenu) | **Toggle Autonomy**, Start/Stop, **Set Goal…**, Status, Memory. Runs background loop with rate limit. |
| **AI options** | **Agent → AI Options** | Max Mode, Deep Thinking (CoT), Deep Research, No Refusal. |
| **Context size** | **Agent → Context Window Size** | 4K–1M tokens. |
| **Backends** | Command Palette (Ctrl+Shift+P) → “Backend” / “Router” | Switch Local GGUF / Ollama / OpenAI / Claude / Gemini; router status, policy, capabilities. |
| **LSP** | Command Palette → “LSP” | Start/stop servers, Go to Definition, Find References, Rename, Hover, Diagnostics. |
| **Hotpatch** | **Hotpatch** menu | 3-layer hotpatch (Memory, Byte, Server), proxy rules, presets. |
| **Benchmark** | **AI Extensions → Benchmark Suite…** | Benchmark runner (stub UI; engine present). |
| **Voice** | **Tools → Voice Chat** / **Voice Automation** | Panel, PTT, TTS, devices, metrics. |
| **Audit** | **Audit** menu | Dashboard, Run Full Audit, Detect Stubs, Check Menu Wiring, Export Report. |
| **Git** | **Git** menu | Status, Commit, Push, Pull, Git Panel. |

**Command Palette** (Ctrl+Shift+P): All commands are searchable by name (e.g. “Load Model”, “Autonomy”, “Streaming”).
