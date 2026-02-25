# TODO Manifest — Actionable Items for Subagents

**IDE layout:** The Win32 IDE has exactly **four main panes**; everything else is a pop up. Canonical spec: **`ARCHITECTURE.md` §6.5** — File Explorer, Terminal/Debug, Editor, AI Chat.

---

## 1. Partially complete (priority)

| Task | File | Remaining | Action |
|------|------|-----------|--------|
| cout/cerr → Logger | `src/cli_shell.cpp` | **0** ✅ | **DONE** — subagents completed profile, subagent, chain, swarm, cot, search, analyze, status, help, route_command, main |
| cout/cerr → Logger | `src/ggml-vulkan/ggml-vulkan.cpp` | ~130 | Replace long tensor-debug cerr/fprintf with s_ggmlVkLog |

## 2. RawrXD-owned code TODOs (in-project)

| File | Line | TODO |
|------|------|------|
| `src/asm/RawrXD_Sidebar_x64.h` | 62 | Provide C++ fallback implementations if building without MASM |
| `src/agent/agentic_deep_thinking_engine.cpp` | 2646 | In production, configure per-agent LLM client with specific model |
| `src/model_loader/enhanced_model_loader.cpp` | 353 | Call m_engine->Initialize(modelPath) when CPUInferenceEngine has that method |
| `src/compiler/toolchain_bridge.cpp` | 569 | Wire to x64_encoder API when parser is complete |
| `src/compiler/toolchain_bridge.cpp` | 619 | Wire merge_context in pe_builder_from_merge |
| `src/compiler/toolchain_bridge_session.cpp` | 954 | Map instruction offset to source line |
| `src/modules/unity_engine_integration.cpp` | 509 | Implement generated class logic |
| `src/digestion/digestion_reverse_engineering.cpp` | 796 | Add logic to void implementation |
| `src/ai_completion_real.cpp` | 244, 247 | Implement condition/loop stubs |
| `src/engine/core_generator.cpp` | 799 | Add language-specific Docker build steps |
| `src/core/auto_feature_registry.cpp` | (IDs) | SubAgent TODO_LIST/CLEAR — verify wiring |

## 3. GGML backend TODOs (upstream-style; lower priority)

- **ggml-vulkan**: async compiles, pointer copy, mul_mat_id prec, staging_offset, exit_node flag, fusion checks, async sync
- **ggml-opencl**: SMALL_PATH init, add support, offset FIXME, preallocated images, optimal values, duplicate defs
- **ggml-metal**: allocator filter, cpy_bytes kernel, constraint relax, helper function, grid params
- **ggml-cpu**: deep copy FIXME, threadpool move
- **ggml-cann**: device info, quantized, paddings, stream, P2P, event sync, BF16, attention sinks
- **ggml-sycl**: MAX_NODES, XMX, buffer alloc FIXME, thread safety, split buffer, MMQ accuracy, mul mat dispatch
- **ggml-webgpu**: error handling, init_tensor, cpy_tensor, reset, buffer size, MOE, thread safety
- **ggml-hexagon**: HTP bailout, errors, profiling, broadcast, non-cont tensors, sinks, F16
- **ggml-impl, ggml-backend, ggml-rpc, ggml-zdnn, ggml-blas**: various

## 4. Reference for subagents

- Logger API: `s_log.info("format {}", arg)` — see `src/include/logging/logger.h`
- cli_shell pattern: `std::cout << "text " << var << "\n";` → `s_log.info("text {}", var);`
- Already done in cli_shell: file, editor, agent, autonomy, debug, hotpatch, decision-tree, autopatch, search
- Remaining sections: profile, subagent, chain, swarm, cot, misc commands
