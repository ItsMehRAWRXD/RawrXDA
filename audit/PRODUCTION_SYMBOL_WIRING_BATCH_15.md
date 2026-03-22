# Production Symbol Wiring - Batch 15

Batch 15 closes fallback pattern loopholes (`*_fallbacks.cpp`) in strict enforcement, tightens non-MSVC fallback pruning, and extends no-stub policy checks beyond Win32IDE to Engine/Gold/Inference under strict production strip mode.

## Batch 15 fixes applied (15)

1. Added strict production guard that fails configure if `src/core/missing_handler_stubs.cpp` is still history-indirected (`history/reconstructed` include path).
2. Removed stale no-op removal of `src/core/memory_patch_byte_search_stubs.cpp` from `RAWR_REPLACE_MASM_FALLBACKS`.
3. Added `src/core/subsystem_mode_fallbacks.cpp` to non-MSVC Win32IDE exclusion list.
4. Added `src/core/rawrxd_native_log_fallback.cpp` to non-MSVC Win32IDE exclusion list.
5. Added `src/llm_adapter/ggufrunner_link_fallbacks.cpp` to non-MSVC Win32IDE exclusion list.
6. Added `src/win32app/v280_link_fallbacks.cpp` to non-MSVC Win32IDE exclusion list.
7. Added `src/win32app/rtp_protocol_fallback.cpp` to non-MSVC Win32IDE exclusion list.
8. Removed stale `src/core/monaco_core_stubs.cpp` from strict production strip list (no longer seeded in default Win32IDE sources).
9. Removed stale `src/win32app/win32ide_extension_command_fallback.cpp` from strict production strip list (already removed from default Win32IDE sources).
10. Expanded Win32IDE agentic real-lane source filter from `*_fallback.cpp` to `*_fallbacks?.cpp` (singular+plural fallback names).
11. Expanded strict pre-check forbidden pattern from `*_fallback.cpp` to `*_fallbacks?.cpp`.
12. Updated `EnforceNoStubs` fallback detection to explicitly match both `*_fallback.cpp` and `*_fallbacks.cpp`.
13. Updated broad Win32IDE post-filter regex to exclude both singular/plural fallback units.
14. Updated strict post-check forbidden pattern to include both singular/plural fallback units.
15. Added strict production no-stub enforcement calls for `RawrEngine`, `RawrXD_Gold`, and `RawrXD-InferenceEngine` when `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON`.

## Why this batch matters

Before this batch, plural fallback units (`*_fallbacks.cpp`) could bypass some strict filters and policy checks, allowing dissolved behavior paths to survive “strict” builds. Batch 15 closes that gap and broadens no-stub policy coverage across all key production targets.

## Remaining for next batches

- Replace the history-indirected `src/core/missing_handler_stubs.cpp` include shim with canonical in-tree implementation ownership under `src/core`.
- Continue replacing fallback-only TUs with real providers in strict production lanes.
- Add target-level compiled-symbol audits for RawrEngine/Gold/Inference analogous to Win32IDE map-based verification.
