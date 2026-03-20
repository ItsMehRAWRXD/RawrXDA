# Production Symbol Wiring - Batch 26

Batch 26 reduces default Win32IDE fallback surface by moving fallback providers out of the baseline source list and into an explicit non-strict/MSVC append lane, while removing now-stale cleanup/removal entries.

## Batch 26 fixes applied (15)

1. Removed `src/inference/gpu_dispatch_gate_win32ide_fallback.cpp` from the default `WIN32IDE_SOURCES` seed list.
2. Removed `src/inference/gguf_d3d12_bridge_link_fallback.cpp` from the default `WIN32IDE_SOURCES` seed list.
3. Removed `src/win32app/v280_link_fallbacks.cpp` from the default `WIN32IDE_SOURCES` seed list.
4. Removed `src/win32app/rtp_protocol_fallback.cpp` from the default `WIN32IDE_SOURCES` seed list.
5. Removed `src/core/win32ide_asm_fallback.cpp` from the default `WIN32IDE_SOURCES` seed list.
6. Removed `src/core/cot_fallback_system.cpp` from the default `WIN32IDE_SOURCES` seed list.
7. Removed stale non-MSVC removal entry for `src/core/model_loader_fallbacks.cpp` (no longer pre-seeded).
8. Removed stale non-MSVC removal entry for `src/core/subsystem_mode_fallbacks.cpp`.
9. Removed stale non-MSVC removal entry for `src/core/rawrxd_native_log_fallback.cpp`.
10. Removed stale non-MSVC removal entry for `src/llm_adapter/ggufrunner_link_fallbacks.cpp`.
11. Removed stale non-MSVC removal entries for Win32IDE fallback transport/compute units (`gpu_dispatch_gate_win32ide_fallback.cpp`, `gguf_d3d12_bridge_link_fallback.cpp`, `v280_link_fallbacks.cpp`, `collab_cursor_fallbacks.cpp`, `rtp_protocol_fallback.cpp`, `cot_fallback_system.cpp`), since these are now appended only in the non-strict MSVC lane.
12. Removed stale strict-strip `list(REMOVE_ITEM ...)` entries for fallback units that are no longer seeded by default (`win32ide_asm_fallback.cpp`, `cot_fallback_system.cpp`, `gpu_dispatch_gate_win32ide_fallback.cpp`, `gguf_d3d12_bridge_link_fallback.cpp`, `v280_link_fallbacks.cpp`, `rtp_protocol_fallback.cpp`).
13. Rewired Win32IDE fallback provider append so fallback units are added only when `NOT RAWRXD_PRODUCTION_STRIP_STUB_SOURCES`.
14. Added MSVC guard around fallback append lane so non-MSVC builds do not auto-introduce these fallback units after non-MSVC pruning.
15. Added explicit status message for non-MSVC lane: `"[Win32IDE] Non-MSVC fallback append skipped"`.

## Why this batch matters

This batch changes fallback ownership from “always present then removed later” to “only appended when explicitly in non-strict compatibility mode,” which reduces dissolved-symbol risk and makes strict-lane behavior easier to reason about.

## Remaining for next batches

- Replace history-indirected `src/core/missing_handler_stubs.cpp` wrapper with canonical in-tree implementation ownership.
- Continue collapsing non-strict compatibility lanes into explicit options.
- Add deeper compiled-symbol closure verification for RawrEngine/Gold/Inference targets.
