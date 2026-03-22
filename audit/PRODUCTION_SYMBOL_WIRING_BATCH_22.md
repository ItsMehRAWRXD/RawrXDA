# Production Symbol Wiring - Batch 22

Batch 22 closes a strict-policy loophole for singular `*_stub.cpp` files that were previously not matched by several filters/checks, while extending the same strict behavior consistently across shared and target-specific source lanes.

## Batch 22 fixes applied (15)

1. Added strict shared-source filtering for singular `*_stub.cpp` in the top-level `SOURCES` strict strip pass.
2. Added strict `RAWR_ENGINE_SOURCES` filtering for singular `*_stub.cpp`.
3. Added strict `GOLD_UNDERSCORE_SOURCES` filtering for singular `*_stub.cpp`.
4. Added strict `INFERENCE_ENGINE_SOURCES` filtering for singular `*_stub.cpp`.
5. Added strict Win32IDE strip filtering for singular `*_stub.cpp`.
6. Added Win32IDE agentic real-lane pre-filter exclusion for singular `*_stub.cpp`.
7. Extended strict Win32IDE pre-check forbidden regex to include singular `*_stub.cpp`.
8. Extended `EnforceNoStubs` primary detection regex to include singular `_stub.cpp` naming.
9. Extended `EnforceNoStubs` secondary naming regex to include singular `_stub.cpp` naming.
10. Extended broad Win32IDE post-filter regex to remove singular `*_stub.cpp` files.
11. Extended strict Win32IDE post-check forbidden regex to include singular `*_stub.cpp`.
12. Extended strict Win32IDE post-check secondary naming regex to include singular `_stub.cpp` naming.
13. Preserved explicit exemption behavior for `ai_agent_masm_stubs.cpp` while broadening strict detection.
14. Kept strict handling for existing plural and prefix patterns (`_stubs.cpp`, `stub_*.cpp`) and made singular-suffix matching additive, not replacing existing rules.
15. Kept strict `link_stubs_*`, `*_link_shims*`, and `*_fallback*` protections intact while harmonizing singular `_stub.cpp` coverage across all lanes.

## Why this batch matters

This closes a high-signal naming gap where singular `_stub.cpp` files could bypass some strict production gates even though `_stubs.cpp` and `stub_*` were already blocked.

## Remaining for next batches

- Replace the history-indirected `src/core/missing_handler_stubs.cpp` wrapper with canonical source ownership under `src/core`.
- Continue reducing fallback-bearing non-strict default lanes via explicit compatibility options.
- Add compiled-symbol closure checks for RawrEngine/Gold/Inference targets in addition to source-pattern gates.
