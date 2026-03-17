# RawrXD Model Loader - AI Coding Agent Instructions

## Project Overview

RawrXD is a multi-target AI/LLM development environment with three main executables:
- **RawrXD-QtShell**: Qt-based IDE with integrated GGUF model loading, inference, and autonomous agent system
- **RawrXD-Win32IDE**: Native Win32 IDE with PowerShell integration and transparent rendering
- **RawrXD-Agent**: Standalone autonomous coding agent for self-modification and releases

The project combines custom GGUF parsing, Vulkan GPU acceleration (AMD RDNA3 optimized), and a complete IDE environment.

## Build System

### Quick Build
```powershell
# Primary build command (from RawrXD-ModelLoader directory)
.\scripts\build.ps1              # Defaults to Release/x64
.\scripts\build.ps1 -Config Debug

# Manual CMake workflow
cmake -S . -B build -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Critical Build Targets
- `RawrXD-QtShell` - Main Qt IDE application (requires Qt6)
- `RawrXD-Win32IDE` - Win32-based IDE (MSVC only)
- `RawrXD-Agent` - Autonomous agent executable
- `model_loader_bench` - Benchmarking harness with JSON output
- `self_test_gate` - Static library for agent self-testing/rollback

### Environment Requirements
- **MSVC required** for Qt targets (MinGW causes ABI mismatch)
- **Qt6_DIR** must point to Qt 6.7.3+ MSVC build (e.g., `C:/Qt/6.7.3/msvc2022_64/lib/cmake/Qt6`)
- **VULKAN_SDK** environment variable for shader compilation
- **ggml submodule** at `3rdparty/ggml` (optional but enables quantization)

## Architecture

### Component Locations
```
src/
├── qtapp/              # Qt-based IDE (MainWindow, AI panels, terminals)
│   ├── MainWindow.*    # "One IDE to rule them all" - 510+ line header
│   ├── gguf_loader.*   # Qt-based GGUF parser with QSharedMemory
│   ├── inference_engine.* # Transformer inference implementation
│   ├── bpe_tokenizer.* # BPE tokenization
│   └── ai_chat_panel.* # AI assistant interface
├── win32app/           # Win32 native IDE
│   ├── Win32IDE.*      # Main window with sidebar, PowerShell panel
│   └── Win32IDE_Autonomy.* # Self-modification integration
├── agent/              # Autonomous agent system (Stage 3 autonomy)
│   ├── planner.*       # Natural language → JSON task list
│   ├── self_patch.*    # Code generation + hot-reload
│   ├── release_agent.* # Version bumping, Git tagging, deployment
│   └── meta_learn.*    # Performance database (perf_db.json)
├── llm_adapter/        # Quantization backend abstraction
│   └── QuantBackend.*  # Runtime switching: F32/Q4_0/Q8_0/Fallback
└── backend/            # Core services and APIs
```

### Key Architectural Patterns

**GGUF Loading**: Two implementations exist:
- `src/qtapp/gguf_loader.*` - Qt-based with QSharedMemory and QHash
- `src/gguf_loader.cpp` - Original implementation
Both parse GGUF v3 binary format, zone-based tensor streaming, memory-mapped access.

**QuantBackend Runtime Switching**: The `QuantBackend` singleton (`src/llm_adapter/QuantBackend.h`) provides runtime mode switching:
```cpp
QuantBackend::instance().setMode(QuantMode::Q8_0);  // 13GB → 7GB RAM
QuantBackend::instance().matmul(A, B, C, N, M, K);
```
Modes: `FALLBACK` (pure C++), `Q4_0` (4-bit), `Q8_0` (8-bit), `F32` (full precision).

**Autonomous Agent System** (Ctrl+Shift+A in IDE):
- Input: Natural language ("Add Q8_K Vulkan kernel")
- Planner generates JSON task list with dependencies
- SelfPatch generates code + hot-reloads without restart
- ReleaseAgent handles version bumping, Git tagging, GitHub releases
- MetaLearn tracks performance in `perf_db.json`

**MainWindow Subsystems**: `src/qtapp/MainWindow.h` contains 20+ dockable panels:
- Project explorer, build integration, VCS (Git/SVN/Mercurial/Perforce)
- Debugger, profiler, Docker/cloud management
- AI chat panel, command palette, activity bar
All subsystems integrate with `StreamerClient` and `AgentOrchestrator` for AI assistance.

## Development Workflows

### Adding New Compute Kernels
1. Create shader in `kernels/` (AVX2 .cc or assembly .asm)
2. Update CMakeLists.txt to include new kernel source
3. Integrate into `QuantBackend` or `VulkanCompute` abstraction
4. Add benchmark in `tests/bench_*.cpp`

Example kernel naming: `q8_0_avx2.cc`, `flash_attn_asm_avx2.asm`

### Testing
```powershell
# Run benchmarks (outputs JSON to stdout + bench/bench_results.json)
.\build\bin\Release\model_loader_bench.exe path\to\model.gguf
.\build\bin\Release\model_loader_bench.exe path\to\model.gguf --no-gpu

# Quantization correctness tests
.\build\bin\Release\quant_correctness_tests.exe
```

### Autonomous Agent Triggers
1. **IDE Shortcut**: Ctrl+Shift+A (select text first)
2. **Environment Variable**: `$env:RAWRXD_WISH = "Add feature X"`
3. **Clipboard**: Voice recognition → clipboard monitoring

### Common Build Issues

**Qt6 not found**: Set `Qt6_DIR` explicitly in CMakeLists.txt or environment:
```powershell
$env:Qt6_DIR = "C:/Qt/6.7.3/msvc2022_64/lib/cmake/Qt6"
```

**MSVC/MinGW mismatch**: Qt targets require MSVC. Don't mix compilers.

**Missing Windows SDK headers**: `include/` directory provides polyfills for `SetProcessDpiAwarenessContext` etc.

**ggml conflicts**: `kernels/quant_ladder_avx2.cpp` disabled in CMakeLists.txt to avoid symbol conflicts with ggml.

## Coding Conventions

### File Naming
- Qt classes: `MainWindow.h/cpp`, `ActivityBar.h/cpp` (PascalCase)
- Services: `gguf_loader.hpp/cpp`, `inference_engine.hpp/cpp` (snake_case headers)
- Agent modules: `planner.hpp/cpp`, `self_patch.hpp/cpp`

### Code Style
- **Headers**: Use `#pragma once` (all files follow this)
- **Qt integration**: Forward-declare Qt classes, include in .cpp
- **Quantization**: Abstract through `QuantBackend` rather than direct ggml calls
- **Error handling**: Check `isOpen()`, `isValid()` before operations
- **Memory**: Qt uses QSharedMemory for GGUF weights, RAII for resources

### Documentation Locations
- `README.md` - Main quickstart (build, usage, API endpoints)
- `ARCHITECTURE-EDITOR.md` - Editor subsystem design (gap buffer, syntax highlighting)
- `AUTONOMOUS-AGENT-GUIDE.md` - Agent system documentation (594 lines)
- `QUICK-REFERENCE.md` - Developer quickstart (335 lines)
- `IDE-QUICK-REFERENCE.md` - Win32IDE keyboard shortcuts and menus
- `BENCHMARKS.md` - Benchmark harness usage and JSON output format
- `SETUP.md` - Detailed prerequisite installation (Visual Studio, CMake, Vulkan SDK)

## API & Integration

### Ollama-Compatible Endpoints
```bash
# Local server runs on port 11434
curl http://localhost:11434/api/tags
curl -X POST http://localhost:11434/api/generate \
  -d '{"model":"bigdaddyg-q2k","prompt":"Hello"}'
```

### OpenAI-Compatible Endpoint
```bash
curl -X POST http://localhost:11434/v1/chat/completions \
  -d '{"model":"bigdaddyg-q2k","messages":[{"role":"user","content":"Hello"}]}'
```

### Model Storage
- Default: `%USERPROFILE%\RawrXD\models\`
- Config: `%USERPROFILE%\RawrXD\config.json` (DPAPI encrypted)

## Performance Characteristics

### GPU Acceleration
- Optimized for AMD RDNA3 (7800XT)
- Vulkan compute backend with 16x16 tiled MatMul
- Zone-based streaming for 15GB+ models

### Benchmark Metrics
JSON output includes: `parse_ms`, `gpu_init_ms`, `matmul_avg_ms`, `rmsnorm_avg_ms`, `silu_avg_ms`, `attention_avg_ms`

Track regressions by comparing `bench/bench_results.json` snapshots.

### Memory Management
- Q2_K through Q8_0 quantization support
- Compression ratios: Q4_0 (~3.7x), Q8_0 (~1.86x)
- Gap buffer for text editing (<10MB files), rope structure for >10MB

## Special Considerations

### Hot-Reload System
The agent can modify its own code and hot-reload modules without restart (`src/agent/hot_reload.*`). Be cautious when editing agent source files directly.

### Session Persistence
MainWindow automatically saves layout + buffer metadata to restore after crashes (`SessionStore` component).

### Shader Compilation
If `glslc` is available, build.ps1 auto-compiles .glsl shaders to .spv. Missing shaders cause runtime failures.

### Swarm Collaboration
Qt WebSockets integration (optional) enables multi-user editing. Check `HAVE_QT_WEBSOCKETS` definition.
