# Production Symbol Wiring - Batch 05

Batch 05 continues dissolved/unlinked symbol hardening by aligning subsystem-mode and streaming-orchestrator fallback signatures in `rawr_engine_link_shims.cpp` with active declarations used by production code.

## Batch 05 fixes applied (15+)

1. Changed `InjectMode` shim signature to `void InjectMode(void)`.
2. Changed `DiffCovMode` shim signature to `void DiffCovMode(void)`.
3. Changed `IntelPTMode` shim signature to `void IntelPTMode(void)`.
4. Changed `AgentTraceMode` shim signature to `void AgentTraceMode(void)`.
5. Changed `DynTraceMode` shim signature to `void DynTraceMode(void)`.
6. Changed `CovFusionMode` shim signature to `void CovFusionMode(void)`.
7. Changed `SideloadMode` shim signature to `void SideloadMode(void)`.
8. Aligned `SO_CreateComputePipelines` shim signature to `(void*, uint64_t)`.
9. Changed `PersistenceMode` shim signature to `void PersistenceMode(void)`.
10. Changed `SO_PrintStatistics` shim signature to `void SO_PrintStatistics(void)`.
11. Aligned `SO_CreateMemoryArena` shim signature to `void* SO_CreateMemoryArena(uint64_t)`.
12. Aligned `SO_LoadExecFile` shim signature to `int SO_LoadExecFile(const char*)`.
13. Changed `BasicBlockCovMode` shim signature to `void BasicBlockCovMode(void)`.
14. Changed `SO_PrintMetrics` shim signature to `void SO_PrintMetrics(void)`.
15. Aligned `SO_StartDEFLATEThreads` shim signature to `int SO_StartDEFLATEThreads(uint32_t)`.
16. Changed `StubGenMode` shim signature to `void StubGenMode(void)`.
17. Changed `TraceEngineMode` shim signature to `void TraceEngineMode(void)`.
18. Changed `CompileMode` shim signature to `void CompileMode(void)`.
19. Changed `GapFuzzMode` shim signature to `void GapFuzzMode(void)`.
20. Changed `EncryptMode` shim signature to `void EncryptMode(void)`.
21. Changed `EntropyMode` shim signature to `void EntropyMode(void)`.
22. Changed `AgenticMode` shim signature to `void AgenticMode(void)`.
23. Changed `UACBypassMode` shim signature to `void UACBypassMode(void)`.
24. Changed `AVScanMode` shim signature to `void AVScanMode(void)`.

## Why this batch matters

These functions are invoked through subsystem APIs that already declare `void` or specific typed signatures. Keeping shims as `int`/wrong-arity dissolves symbol contract integrity and risks ABI mismatches where call-sites and fallback definitions disagree.

## Remaining for next batches

- Continue signature alignment for remaining `asm_pyre_*`, perf, and watchdog shims with their canonical headers.
- Replace broad shim-provider lane with per-subsystem providers in production CMake targets.
- Replace history-redirected `missing_handler_stubs.cpp` with local canonical source ownership.
