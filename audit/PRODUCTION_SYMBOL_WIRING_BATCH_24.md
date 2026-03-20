# Production Symbol Wiring - Batch 24

Batch 24 closes strict-filter gaps for ASM naming variants of stub/shim/fallback/mock units, ensuring parity between C++ and MASM compatibility naming across shared/target filters and Win32IDE policy gates.

## Batch 24 fixes applied (15)

1. Added strict shared-source filtering for singular ASM stub files (`*_stub.asm`).
2. Added strict shared-source filtering for prefix ASM stub files (`stub_*.asm`).
3. Added strict shared-source filtering for ASM shim files (`*_shim.asm` / `*_shims.asm`).
4. Added strict shared-source filtering for ASM link-shim files (`*_link_shim.asm` / `*_link_shims.asm`).
5. Added strict shared-source filtering for ASM fallback files (`*_fallback*.asm`).
6. Added strict shared-source filtering for ASM mock/fake suffix and prefix variants.
7. Added strict shared-source filtering for `link_stubs_*.asm`.
8. Extended strict `RAWR_ENGINE_SOURCES` filtering with the same ASM stub/shim/fallback/mock/link_stubs coverage.
9. Extended strict `GOLD_UNDERSCORE_SOURCES` filtering with the same ASM compatibility-unit coverage.
10. Extended strict `INFERENCE_ENGINE_SOURCES` filtering with the same ASM compatibility-unit coverage.
11. Extended Win32IDE strict strip filter pass with ASM compatibility-unit coverage (`*_stub.asm`, `stub_*.asm`, `*_shims*.asm`, `*_link_shims*.asm`, `*_fallback*.asm`, mock/fake asm variants, and `link_stubs_*.asm`).
12. Extended Win32IDE agentic real-lane pre-filter exclusions to include ASM stub/shim/fallback/mock/link_stubs naming variants.
13. Extended strict Win32IDE pre-check forbidden regex to classify ASM compatibility-unit variants as violations.
14. Extended `EnforceNoStubs` detection regexes to cover both C++ and ASM naming variants (`*_stub`, `*_shims`, `*_link_shims`, `*_fallback*`, `link_stubs_*`, mock/fake suffixes).
15. Extended broad Win32IDE post-filter and strict post-check forbidden regexes to include ASM compatibility-unit variants with C++/ASM parity.

## Why this batch matters

Prior strict filters focused mostly on C++ patterns and a narrow subset of ASM stubs. This batch removes that asymmetry, preventing compatibility units from bypassing strict production rules solely due to MASM file naming.

## Remaining for next batches

- Replace history-indirected `src/core/missing_handler_stubs.cpp` wrapper with canonical in-tree implementation ownership.
- Continue converting non-strict fallback lanes into explicit compatibility options.
- Add deeper compiled-symbol closure checks for RawrEngine/Gold/Inference targets.
