# Production Symbol Wiring - Batch 29

Batch 29 focuses on strict-lane determinism and regex parity hardening so production strip mode catches additional naming variants for dissolved/stubbed providers.

## Batch 29 fixes applied (15)

1. Hoisted `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES` option declaration before early strict-lane checks.
2. Hoisted `RAWRXD_INCLUDE_STRESS_AND_REPLAY_SOURCES` option declaration before early strict-lane checks.
3. Hoisted `RAWRXD_ENABLE_MISSING_HANDLER_STUBS` option declaration before early strict-lane checks.
4. Removed duplicate late re-declarations of those three options from the replacement-pass block to keep a single authoritative declaration site.
5. Added strict guardrail: multi-config generators are now rejected when `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON` (requires single-config `CMAKE_BUILD_TYPE=Release`).
6. Hardened global strict `SOURCES` filtering to exclude `/stubs.cpp` naming.
7. Hardened global strict `SOURCES` filtering to exclude `stub_*.cpp` and `shim_*.cpp` prefix naming.
8. Hardened global strict `SOURCES` filtering to exclude `/stubs.asm` and `shim_*.asm` naming variants.
9. Extended `RAWR_ENGINE_SOURCES` strict filtering with `/stubs.cpp`, `stub_*.cpp`, `shim_*.cpp`, `/stubs.asm`, and `shim_*.asm`.
10. Extended `GOLD_UNDERSCORE_SOURCES` strict filtering with `/stubs.cpp`, `stub_*.cpp`, `shim_*.cpp`, `/stubs.asm`, and `shim_*.asm`.
11. Extended `INFERENCE_ENGINE_SOURCES` strict filtering with `/stubs.cpp`, `stub_*.cpp`, `shim_*.cpp`, `/stubs.asm`, and `shim_*.asm`.
12. Updated `RawrXD_DynamicPromptEngine` target wiring so strict production strip mode builds it as a normal `SHARED` target (non-strict keeps `EXCLUDE_FROM_ALL` behavior).
13. Removed stale empty Win32IDE "Production lane source normalization" comment block that no longer had active removal logic.
14. Extended Win32IDE strict strip filtering with `/stubs.cpp`, `stub_*.cpp`, `shim_*.cpp`, `/stubs.asm`, and `shim_*.asm`.
15. Extended Win32IDE strict agentic reality filters/pre-checks with `/stubs.asm` and `shim_*.{cpp,asm}` detection; corrected comment wording to state that generic `link_stubs` units are disallowed.

## Why this batch matters

Strict production strip mode now has more deterministic option evaluation and tighter parity across source-list filters, closing prefix and basename naming gaps that could otherwise let compatibility/stub units slip through.

## Remaining for next batches

- Replace history-indirected `src/core/missing_handler_stubs.cpp` with canonical in-tree implementation ownership.
- Continue reducing non-strict fallback lanes into explicit compatibility presets/options.
- Add deeper compiled-symbol closure checks for RawrEngine/Gold/Inference (map/symbol-level parity gates).
