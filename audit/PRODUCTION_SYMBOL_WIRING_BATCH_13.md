# Production Symbol Wiring - Batch 13

Batch 13 continues production hardening by removing stale no-op wiring controls, reducing contradictory SSOT/fallback ownership, and tightening strict-lane stripping across Engine, Gold, Inference, and Win32IDE targets.

## Batch 13 fixes applied (15)

1. Removed stale CMake option `RAWR_REPLACE_SWARM_LICENSE_FALLBACKS` (it had no active source ownership in `SOURCES`).
2. Removed stale `if(RAWR_REPLACE_SWARM_LICENSE_FALLBACKS)` block from `CMakeLists.txt`.
3. Removed no-op `src/core/missing_handler_stubs.cpp` removal from `RAWR_SSOT_PROVIDER=CORE` RawrEngine source partition.
4. Removed no-op `src/core/missing_handler_stubs.cpp` removal from `RAWR_SSOT_PROVIDER=EXT` RawrEngine source partition.
5. Removed no-op `src/core/missing_handler_stubs.cpp` removal from `RAWR_SSOT_PROVIDER=AUTO` RawrEngine source partition.
6. Removed no-op `src/core/missing_handler_stubs.cpp` removal from `RAWR_SSOT_PROVIDER=FEATURES` RawrEngine source partition.
7. Added strict-lane removal of `src/core/cot_fallback_system.cpp` from shared `SOURCES` when `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON`.
8. Added strict-lane removal of `src/core/dml_asm_fallback.cpp` from shared `SOURCES` when `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON`.
9. Added strict-lane removal of `src/win32app/agentic_bridge_headless.cpp` from shared `SOURCES` when `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON`.
10. Added strict-lane removal of `src/win32app/Win32IDE_logMessage.cpp` from shared `SOURCES` when `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON`.
11. Removed unconditional `src/core/rawr_engine_link_shims.cpp` from `RawrEngine` target sources; now only included when strict stripping is OFF.
12. Removed unconditional `src/core/rawr_engine_link_shims.cpp` from `RawrXD_Gold` source append; now only included when strict stripping is OFF.
13. Removed unconditional `src/core/missing_handler_stubs.cpp` from `target_sources(RawrXD_Gold ...)`, eliminating AUTO-lane contradiction against the Gold production SSOT branch.
14. Added strict-lane removal of `src/inference/inference_standalone_link_shims.cpp` from `INFERENCE_ENGINE_SOURCES`.
15. Extended Win32IDE strict strip/removal to include `gpu_dispatch_gate_win32ide_fallback.cpp`, `gguf_d3d12_bridge_link_fallback.cpp`, `v280_link_fallbacks.cpp`, and `collab_cursor_fallbacks.cpp`; also gated re-append of fallback providers behind `NOT RAWRXD_PRODUCTION_STRIP_STUB_SOURCES`.

## Why this batch matters

This batch reduces dissolved-symbol risk caused by contradictory CMake ownership and silent fallback re-introduction. It also makes strict production stripping materially stronger across all primary product targets instead of only Win32IDE-local stub units.

## Remaining for next batches

- Replace history-indirected ownership in `src/core/missing_handler_stubs.cpp` with direct canonical source ownership under `src/core`.
- Continue migrating shim/fallback-only translation units to explicit non-production options.
- Add stricter CI gating to assert no fallback/shim translation units are linked when `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON`.
