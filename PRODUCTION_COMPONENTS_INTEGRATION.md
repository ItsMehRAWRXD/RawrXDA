# Production Components Integration Note

This document summarizes integration of the user-provided "production-hardened" components (ASM + C++).

## What Was Done

### 1. ASM (on disk, not in build)

- **`src/asm/ksamd64.inc`** — Added. Minimal shim that does `option casemap:none` and `INCLUDE RawrXD_Common.inc` for ASM that expects `ksamd64.inc`.
- **`src/asm/agentic_deep_thinking_kernels.asm`** — Replaced with your v2.0 content, adapted to use `INCLUDE RawrXD_Common.inc`, horizontal sum fix for `masm_cognitive_pattern_scan_avx512`, and `.data` moved to top. **Not** in `ASM_KERNEL_SOURCES` because ML64 still reports errors (e.g. `.endprolog`, REAL4 initializer, scale value).
- **`src/asm/ai_completion_provider_masm.asm`** — Replaced with your enhanced version (embedding lookup, attention, speculative decode, sampler_top_p, kv_cache_append). **Not** in build (prolog size / .ENDPROLOG / operand errors).
- **`src/asm/agentic_puppeteer_byte_ops.asm`** — Replaced with your v2 (asm_safe_replace, asm_utf8_to_utf16, asm_ring_buffer_stream, asm_memory_integrity_check, asm_secure_memmove). **Not** in build (constant out of range, invalid operands).

To enable them in the build, fix ML64-specific issues (e.g. prolog &lt; 256 bytes, `.endprolog` for every FRAME proc, REAL4 floating-point initializers, and operand sizes). CMakeLists.txt has a comment where to re-add the three files.

### 2. C++ — Not Overwritten

Existing implementations were **not** replaced, to avoid breaking the current Win32 IDE build and APIs:

- **model_registry** — `include/model_registry.h` and `src/model_registry.cpp` already implement the Qt-free API (ModelVersion, setShowCallback, show(), etc.). Your snippet used a different API (e.g. GGUFLoader, getActiveModel() returning string from a different shape).
- **checkpoint_manager** — `include/checkpoint_manager.h` and `src/core/checkpoint_manager.cpp` already implement the Qt-free API and show callback.
- **universal_model_router** — `src/universal_model_router.cpp` exists and is used; your version would duplicate or conflict.
- **byte_level_hotpatcher** — `src/core/byte_level_hotpatcher.cpp` exists and uses `find_pattern_asm` from `memory_patch_byte_search_stubs.cpp`; no replacement.
- **multi_gpu** — `src/core/multi_gpu.cpp` and `multi_gpu_manager.cpp` are already in use; your Vulkan-based snippet would require matching the existing MultiGPUManager API and build deps.
- **agentic_config, context_deterioration_hotpatch, vscode_marketplace, complete_implementations** — Not dropped in as-is to avoid duplicate symbols and API drift; can be wired in incrementally where signatures match.

### 3. CMake

- **No** `/FORCE:MULTIPLE` — That would hide duplicate-symbol errors.
- **No** extra `target_sources(RawrXD-Win32IDE ...)` for the C++ files you listed — those sources are already part of `WIN32IDE_SOURCES` or main `SOURCES` where needed.

## Build Verification

- **RawrXD-Win32IDE** builds successfully (with the three new ASM files excluded from the build).
- To re-enable the new ASM once ML64 issues are fixed: in `CMakeLists.txt`, add back to `ASM_KERNEL_SOURCES`:
  - `src/asm/agentic_deep_thinking_kernels.asm`
  - `src/asm/ai_completion_provider_masm.asm`
  - `src/asm/agentic_puppeteer_byte_ops.asm`

## ML64 Errors to Address (for ASM inclusion)

- **agentic_deep_thinking_kernels.asm**: syntax error on `.`; invalid scale value; REAL4 must use floating-point initializer; PROC FRAME prolog &lt; 256 bytes and `.endprolog` required; invalid operand size.
- **ai_completion_provider_masm.asm**: invalid instruction operands; constant out of range; prolog size and `.endprolog` for asm_attention_forward, asm_speculative_decode, asm_kv_cache_append.
- **agentic_puppeteer_byte_ops.asm**: invalid instruction operands; constant value out of range (e.g. FNV prime / hex constants for 64-bit).

Once these are fixed for ML64, uncomment the three entries in `ASM_KERNEL_SOURCES` and rebuild.
