# Non-Operational Feature Audit (RawrXD IDE)

This audit identifies features that are currently non-functional, stubbed, or partially implemented despite being "enabled" in the command registry.

## 1. High-Level Feature Stubs (C++ / Command Dispatch)
These features are wired but currently lack backing logic (captured via `ssot_missing_handlers_provider.cpp` and `unified_command_dispatch.cpp`).

| Feature Area | Commands | Implementation Status |
| :--- | :--- | :--- |
| **Game Engine Bridge** | `unreal.*`, `unity.*` | Full Stub: Empty handler logic. |
| **Marketplace** | `marketplace.*` | Redirect Stub: No backend download/install logic. |
| **Advanced Inference** | `model.finetune`, `model.quantize` | Stub: Prints "Not implemented" in terminal. |
| **Vision AI** | `vision.analyze` | Stub: Lacks actual image processing backend. |
| **LSP Infrastructure** | `lspServer.*` (start, reindex, stats) | CLI Stub: Prints fallback message; GUI relies on `WM_COMMAND`. |
| **AI Context Expansion** | `ai.context128k` through `ai.context1m` | Stub: No underlying context management logic. |

## 2. Low-Level Kernel Stubs (MASM / Assembly)
These assembly kernels are either skeletal or contain significant `TODO` sections.

| Kernel / File | Missing Functionality | Impact |
| :--- | :--- | :--- |
| `agentic_deep_thinking_kernels.asm` | Full Recovery Stub | "Deep Thinking" features are currently pass-through `xor eax, eax`. |
| `RawrXD_AgenticOrchestrator.asm` | Empty File | No orchestration logic for multi-agent loops. |
| `RawrXD_UnifiedOverclockGovernor.asm` | GPU Backend (Vulkan/CUDA/ROCm) | GPU acceleration for inference/model operations is unimplemented. |
| `swarm.asm` | P2P / Dispatch Compute | Swarm/Distributed compute functionality is a stub. |
| `vision_projection_kernel.asm` | DirectStorage / Vulkan DMA | High-performance vision pipeline paths are marked as `TODO`. |

## 3. GUI vs CLI Disparity
Several "enabled" features only function when triggered from the Win32 GUI via message loops:
- **Theming & UI:** `theme.*`, `view.transparency*`, `view.monaco*`.
- **Status:** CLI invocation reports: "[GUI-only command]".

## 4. Recommendations
- **Tier 1:** Implement `ai.chatMode` and `lspServer` as these are core developer experience loops.
- **Tier 2:** Flesh out `agentic_deep_thinking_kernels` to provide actual processing beyond recovery stubs.
- **Tier 3:** Complete the GPU backend in `UnifiedOverclockGovernor` to enable hardware acceleration.
