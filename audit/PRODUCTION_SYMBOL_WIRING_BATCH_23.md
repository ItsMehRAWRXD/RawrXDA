# Production Symbol Wiring - Batch 23

Batch 23 extends strict production filtering to cover mock/fake and ASM-stub naming variants, and blocks a remaining stub-backed auxiliary library (`RawrXD-Crypto`) from strict production strip mode.

## Batch 23 fixes applied (15)

1. Added strict shared-source filtering for singular mock suffix files (`*_mock.cpp`).
2. Added strict shared-source filtering for singular fake suffix files (`*_fake.cpp`).
3. Added strict shared-source filtering for prefix/mock-bearing names (`*mock_*` / `*fake_*`).
4. Added strict shared-source filtering for ASM stub file names (`*_stubs.asm`).
5. Added strict `RAWR_ENGINE_SOURCES` filtering for mock/fake suffix and mock_/fake_ patterns.
6. Added strict `RAWR_ENGINE_SOURCES` filtering for ASM stub file names (`*_stubs.asm`).
7. Added strict `GOLD_UNDERSCORE_SOURCES` filtering for mock/fake suffix and mock_/fake_ patterns.
8. Added strict `GOLD_UNDERSCORE_SOURCES` filtering for ASM stub file names (`*_stubs.asm`).
9. Added strict `INFERENCE_ENGINE_SOURCES` filtering for mock/fake suffix and mock_/fake_ patterns.
10. Added strict `INFERENCE_ENGINE_SOURCES` filtering for ASM stub file names (`*_stubs.asm`).
11. Added strict Win32IDE strip filtering for mock/fake suffix patterns, mock_/fake_ patterns, and ASM stub file names.
12. Added Win32IDE agentic real-lane pre-filter exclusions for mock/fake suffix/prefix naming patterns.
13. Extended Win32IDE strict pre-check forbidden regex to include mock/fake suffix/prefix variants.
14. Extended `EnforceNoStubs`, broad Win32IDE post-filter, and strict post-check forbidden regexes to include mock/fake suffix variants consistently.
15. Gated `RawrXD-Crypto` out of strict production strip mode (it depends on `uac_bypass_impl_stub.cpp`) and added explicit strict-mode skip status messaging.

## Why this batch matters

This closes additional naming loopholes where strict production strip could miss compatibility units that are not named as classic `_stubs.cpp` but are still mock/fake or ASM-stub providers.

## Remaining for next batches

- Replace `src/core/missing_handler_stubs.cpp` history-indirected ownership wrapper with canonical source ownership under `src/core`.
- Continue collapsing non-strict fallback lanes into explicit compatibility options.
- Add stronger compiled-symbol closure checks for RawrEngine/Gold/Inference targets.
