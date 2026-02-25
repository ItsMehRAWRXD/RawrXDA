# RawrXD — Every Entry Point Map

**Purpose:** Single reference for all executable/DLL entry points (WinMain, main, DllMain, wWinMain).  
**Generated:** 2026-02-20

---

## 1. Application entry points (EXE)

| Entry | File | Purpose / Target |
|-------|------|------------------|
| **WinMain** | `src/win32app/main_win32.cpp` | **Production Win32 IDE.** Used by CMake target `RawrXD-Win32IDE`. Crash containment, DPI, headless branch, then `Win32IDE` create/run. |
| **WinMain** | `Ship/RawrXD_Win32_IDE.cpp` | Monolithic Ship build; delegates to `wWinMain`. |
| **wWinMain** | `Ship/RawrXD_Win32_IDE.cpp` | Unicode WinMain wrapper in Ship; same IDE flow. |
| **WinMain** | `src/WinMain_CircularArch.cpp` | Circular-arch demo; GlobalContext, EventBus, then exit. |
| **WinMain** | `src/stub_main.cpp` | Stub WinMain (minimal). |
| **WinMain** | `src/agentic_ide_main_simple.cpp` | Simplified agentic IDE entry. |
| **WinMain** | `src/agentic_ide_main.cpp` | Agentic IDE entry (full). |
| **WinMain** | `src/test_ide_main.cpp` | Test IDE entry. |
| **wWinMain** | `src/ide_main.cpp` | Unicode IDE entry (alternative). |
| **main** | `src/main.cpp` | Default CLI / engine entry (argc/argv). |
| **main** | `src/main-simple.cpp` | Simple model loader. |
| **main** | `src/main-minimal.cpp` | Minimal main. |
| **main** | `src/main_production.cpp` | Production CLI (argc/argv). |
| **main** | `src/main_old_cli.cpp` | Legacy CLI. |
| **main** | `src/rawrxd_cli.cpp` | RawrXD CLI (argc/argv). |
| **main** | `src/cli_shell.cpp` | CLI shell; init_runtime then shell loop. |
| **main** | `src/cli/rawrxd_cli_compiler.cpp` | CLI compiler driver. |
| **main** | `src/masm/masm_cli_compiler.cpp` | MASM CLI compiler. |
| **main** | `src/api_server_simple.cpp` | API server standalone. |
| **main** | `src/tool_server.cpp` | Tool server. |
| **main** | `src/gguf_api_server.cpp` | GGUF API server. |
| **main** | `src/gguf_diagnostic.cpp` | GGUF diagnostic. |
| **main** | `src/monaco_gen.cpp` | Monaco/React IDE generator. |
| **main** | `src/engine/react_ide_generator.cpp` | React IDE generator (codegen tool). |
| **main** | `src/ggml-vulkan/vulkan-shaders/vulkan-shaders-gen.cpp` | Vulkan shader codegen. |
| **main** | `src/agent/agent_main.cpp` | Agent main. |
| **main** | `src/bench_main.cpp` | Benchmark. |
| **main** | `src/inference/inference_standalone_main.cpp` | Inference standalone. |
| **main** | `src/digestion/digestion_cli.cpp` | Digestion CLI. |
| **main** | `src/digestion/main_gui.cpp` | Digestion GUI. |
| **main** | `src/tools/multi_model_benchmark.cpp` | Multi-model benchmark. |
| **main** | `src/tools/real_multi_model_benchmark.cpp` | Real multi-model benchmark. |
| **main** | `src/tools/simple_gpu_test.cpp` | Simple GPU test. |
| **main** | `src/tools/RawrXD_KeyGen.cpp` | Keygen tool. |
| **main** | `src/tools/DeepSectorScan.cpp` | Deep sector scan. |
| **main** | `src/direct_io/burstc_main.cpp` | Burst C main. |
| **main** | `src/direct_io/sovereign_cluster_report.cpp` | Sovereign cluster report. |
| **main** | `src/direct_io/gguf_burstzone_patcher.cpp` | GGUF burstzone patcher. |
| **main** | `src/direct_io/SovereignNVMeOracle.cpp` | NVMe oracle (2x main). |
| **main** | `src/direct_io/mmf_diagnostic.cpp` | MMF diagnostic. |
| **main** | `src/thermal/governor/GovernorMain.cpp` | Governor main. |
| **main** | `src/thermal/masm/ghost_paging_main.cpp` | Ghost paging. |
| **main** | `src/thermal/masm/nvme_oracle_host.cpp` | NVMe oracle host. |
| **main** | `src/thermal/masm/nvme_oracle_host_standalone.cpp` | NVMe oracle standalone. |
| **main** | `src/win32app/IDEAutoHealerLauncher.cpp` | IDE auto-healer launcher. |
| **main** | `src/win32app/test_runner.cpp` | Test runner. |
| **main** | `src/win32app/simple_test.cpp` | Simple test. |
| **main** | `src/paint/paint_main.cpp` | Paint app. |
| **main** | `src/paint/image_generator_example.cpp` | Image generator example. |
| **main** | `src/phase_1_2_integration_demo.cpp` | Phase 1/2 demo. |
| **main** | `src/oc_stress.cpp` | OC stress. |
| **main** | `src/model_router_cli_test.cpp` | Model router CLI test. |
| **main** | `src/ggml_masm/test_masm_ops.cpp` | MASM ops test. |
| **main** | `src/ai/test_streaming_gguf_loader.cpp` | Streaming GGUF loader test. |
| **main** | `src/ai/test_minimal_streaming.cpp` | Minimal streaming test. |
| **main** | `src/agentic_ide_test.cpp` | Agentic IDE test. |
| **main** | `src/test_agentic_executor.cpp` | Agentic executor test. |
| **main** | `src/test_harness/camellia256_test.cpp` | Camellia256 test. |
| **main** | `src/reverse_engineering/deobfuscator/test_deobf.cpp` | Deobfuscator test. |
| **main** | `src/agent/instruction_loader_test.cpp` | Instruction loader test. |
| **main** | `src/agentic/tests/test_orchestrator_modules.cpp` | Orchestrator modules test. |
| **main** | `src/agentic/tests/smoke_test.cpp` | Smoke test. |
| **main** | `src/agentic/monaco/test_monaco_verification.cpp` | Monaco verification test. |
| **main** | `src/digestion/tests/digestion_config_tests.cpp` | Digestion config tests. |
| **main** | `src/masm/test_simple.cpp` | MASM simple test. |
| **main** | `src/masm/test_integration.cpp` | MASM integration test. |
| **main** | `src/masm/test_unbraid.cpp` | MASM unbraid test. |
| **main** | `src/masm/test_bridge.cpp` | MASM bridge test. |
| **main** | `src/masm/test_sloloris.cpp` | MASM sloloris test. |
| **main** | `src/masm/test_http_server.cpp` | HTTP server test. |
| **main** | `src/masm/test_http_chat_server.cpp` | HTTP chat server test. |
| **main** | `src/tests/fuzz_gguf_loader.cpp` | Fuzz GGUF loader. |
| **main** | `src/tests/regression_suite.cpp` | Regression suite. |
| **main** | `src/net/test_net_ops.cpp` | Net ops test. |
| **main** | `src/test_chat_e2e.cpp` | Chat E2E test. |
| **main** | `src/test_self_audit.cpp` | Self audit. |
| **main** | `src/test_kv_cache.cpp` | KV cache test. |
| **main** | `src/test_titan_integration.cpp` | Titan integration test. |
| **main** | `src/test_40gb_loaders.cpp` | 40GB loaders test. |
| **main** | `src/smoke_test_standalone.cpp` | Smoke standalone. |
| **main** | `src/smoke_test.cpp` | Smoke test. |
| **main** | `src/header_test.cpp` | Header test. |
| **main** | `src/minimal_test.cpp` | Minimal test. |
| **main** | `src/simple_test.cpp` | Simple test. |
| **main** | `src/test_missing_logic.cpp` | Missing logic test. |
| **main** | `src/RawrXD_PipeTest.cpp` | Pipe test. |
| **main** | `src/RawrXD_PatternBridgeClient.cpp` | Pattern bridge client. |
| **main** | `src/main_production_test.cpp` | Production test. |
| **main** | `src/main_broken.cpp` | Broken main (dev). |
| **main** | `src/test.cpp` | Generic test. |
| **main** | `src/agentic/week1/BUILD_WEEK1.ps1` | (inline C++ in script) |

### Qt / GUI (optional)

| Entry | File | Purpose |
|-------|------|--------|
| **main** | `src/qtapp/main_qt.cpp` | Qt IDE main. |
| **main** | `src/qtapp/main_v5.cpp` | Qt v5 main. |
| **main** | `src/qtapp/main_simple.cpp` | Qt simple. |
| **main** | `src/qtapp/minimal_main.cpp` | Qt minimal. |
| **main** | `src/qtapp/minimal_ide_main.cpp` | Qt minimal IDE. |
| **main** | `src/qtapp/minimal_test.cpp` | Qt minimal test. |
| **main** | `src/qtapp/model_benchmark_console.cpp` | Model benchmark. |
| **main** | `src/qtapp/production_integration_test.cpp` | Production integration. |
| **main** | `src/qtapp/production_integration_example.cpp` | Production example. |
| **main** | `src/qtapp/production_feature_test.cpp` | Production feature test. |
| **main** | `src/qtapp/test_qt.cpp` | Qt test. |
| **main** | `src/qtapp/test_chat_console.cpp` | Chat console test. |
| **main** | `src/qtapp/test_chat_streaming.cpp` | Chat streaming test. |
| **main** | `src/qtapp/digest_cli.cpp` | Digest CLI. |
| **main** | `src/qtapp/simple_gpu_test.cpp` | Simple GPU test. |
| **main** | `src/qtapp/gguf_hotpatch_tester.cpp` | GGUF hotpatch tester. |
| **main** | `src/qtapp/gpu_inference_benchmark.cpp` | GPU inference benchmark. |
| **main** | `src/minimal_qt_test.cpp` | Minimal Qt test. |
| **main** | `src/qtapp/minimal_qt_test.cpp` | Minimal Qt test (qtapp). |

---

## 2. DLL entry points (DllMain)

| Entry | File | Purpose |
|-------|------|--------|
| **DllMain** | `src/plugins/example_voice_plugin.cpp` | Voice plugin DLL. |
| **DllMain** | `src/agentic/Phase3_Agent_Kernel_Bridge.cpp` | Agent kernel bridge DLL. |
| **DllMain** (ASM) | `src/win32app/Win32IDE_Sidebar.asm` | Sidebar MASM64 DLL (proc frame). |
| **DllMain** (ASM) | `asm/Win32IDE_Sidebar_Core.asm` | Sidebar core ASM. |

---

## 3. Shader / compute (void main — not process entry)

| Location | Note |
|----------|------|
| `src/gpu/shader_matmul.comp` | Compute shader. |
| `src/backend/vulkan_compute.cpp` | Inline shader `main`. |
| `src/ggml-vulkan/vulkan-shaders/*.comp`, `*.glsl` | Vulkan compute shaders (`void main()`). |

These are GPU entry points, not process entry points.

---

## 4. Canonical IDE build entry

| What | Where | Notes |
|------|--------|--------|
| **IDE EXE** | `RawrXD-Win32IDE` | CMake target. |
| **Source entry** | `src/win32app/main_win32.cpp` → `WinMain` | Single WinMain for IDE; headless branch uses `HeadlessIDE`. |
| **MASM/ml64** | All `.asm` in `ASM_KERNEL_SOURCES` + custom MASM objects | Built via `masm_kernels`; `CMAKE_ASM_MASM_COMPILER` = `ml64.exe` (x64). |
| **Output** | `bin/RawrXD-Win32IDE.exe` (or build dir) | RUNTIME_OUTPUT_DIRECTORY = bin. |

---

## 5. Build with ml64 (IDE)

- **CMake:** On Windows with MSVC, `enable_language(ASM_MASM)` and `CMAKE_ASM_MASM_COMPILER` set to `ml64.exe` (root `CMakeLists.txt`). All MASM sources are assembled with ml64; `RawrXD-Win32IDE` depends on target `masm_kernels` and links `${MASM_OBJECTS}`.
- **Commands (from repo root):**
  - Configure: `cmake -B build -G "Visual Studio 17 2022" -A x64`
  - Build IDE (includes ml64 step): `cmake --build build --target RawrXD-Win32IDE --config Release`

---

**Version:** 1.0  
**Date:** 2026-02-20
