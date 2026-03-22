# Production Symbol Wiring - Batch 10

Batch 10 addresses a major Win32IDE production fallback hotspot by replacing `subsystem_mode_fallbacks.cpp`'s unresolved `void` placeholders with ABI-correct no-op providers.

## Batch 10 fixes applied (15+)

1. Reworked file to include standard type headers for explicit ABI-safe signatures.
2. Added Win32-aware `HWND` type handling for quad buffer init fallback signature.
3. Aligned `AD_ProcessGGUF` fallback to `int(const char*, const char*)`.
4. Aligned `SO_InitializeVulkan` fallback to `int(void)`.
5. Aligned `SO_InitializeStreaming` fallback to `int(void)`.
6. Aligned `SO_CreateComputePipelines` fallback to `int(void*, uint64_t)`.
7. Aligned `SO_CreateMemoryArena` fallback to `void*(uint64_t)`.
8. Aligned `SO_LoadExecFile` fallback to `int(const char*)`.
9. Aligned `SO_StartDEFLATEThreads` fallback to `int(uint32_t)`.
10. Aligned `SO_InitializePrefetchQueue` fallback to `int(void)`.
11. Aligned `SO_CreateThreadPool` fallback to `int(void)`.
12. Aligned watchdog fallbacks to `int`/buffered signatures (`asm_watchdog_get_status(void*)`, `asm_watchdog_get_baseline(uint8_t*)`).
13. Aligned all Omega fallback signatures to `omega_orchestrator.hpp` contracts (`ingest`, `plan`, `deploy`, `stats`, etc.).
14. Aligned all Mesh fallback signatures to `mesh_brain.hpp` contracts (`crdt`, `dht`, `quorum`, `stats`, etc.).
15. Aligned all Speciator fallback signatures to `speciator_engine.hpp` contracts (`int32_t`/`uint64_t` return paths included).
16. Aligned all Neural fallback signatures to `neural_bridge.hpp` contracts.
17. Aligned all Hardware Synthesizer fallback signatures to `hardware_synthesizer.hpp` contracts.
18. Aligned quadbuffer/spengine fallback signatures to `masm_bridge_cathedral.h` contracts.
19. Converted legacy placeholder `void` stubs to type-correct return defaults (`0`/`nullptr`) so callers and exported symbols share the same contract.

## Why this batch matters

`subsystem_mode_fallbacks.cpp` was one of the largest dissolved symbol sources in production CMake wiring (Win32IDE lane), with dozens of signatures incompatible with active headers/callers. This batch removes broad ABI drift while preserving fallback link closure.

## Remaining for next batches

- Continue reducing fallback-only ownership by routing to real providers where available.
- Remove `history/reconstructed` include indirection in missing-handler production path.
- Tighten remaining CMake source lanes so production defaults avoid stub providers unless explicitly requested.
