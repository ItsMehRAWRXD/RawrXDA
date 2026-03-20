# Production Symbol Wiring - Batch 04

Batch 04 targets unresolved/dissolved symbol quality by aligning fallback shim signatures with canonical interfaces and fixing production lane configuration contradictions.

## Batch 04 fixes applied (15+)

1. Removed unused `RobustOllamaParser.h` include from `src/core/rawr_engine_link_shims.cpp`.
2. Added `<cstddef>` include for correct `size_t` usage in shim signatures.
3. Aligned `Scheduler_SubmitTask` shim signature to `asm_bindings.h`.
4. Aligned `AllocateDMABuffer` shim signature to `asm_bindings.h`.
5. Aligned `CalculateCRC32` shim signature to `asm_bindings.h`.
6. Aligned `Heartbeat_AddNode` shim signature to `asm_bindings.h`.
7. Aligned `Tensor_QuantizedMatMul` shim signature to `asm_bindings.h`.
8. Aligned `ConflictDetector_LockResource` shim signature to `asm_bindings.h`.
9. Aligned `GPU_WaitForDMA` shim signature to `asm_bindings.h`.
10. Aligned `ConflictDetector_RegisterResource` shim signature to `asm_bindings.h`.
11. Aligned `ConflictDetector_UnlockResource` shim signature to `asm_bindings.h`.
12. Aligned `GPU_SubmitDMATransfer` shim signature to `asm_bindings.h`.
13. Aligned `Scheduler_WaitForTask` shim signature to `asm_bindings.h`.
14. Aligned `find_pattern_asm` shim signature/return type with `byte_level_hotpatcher.cpp` contract.
15. Aligned `asm_mesh_init` shim signature with `mesh_brain.hpp`.
16. Aligned `asm_mesh_zkp_verify` shim signature with `mesh_brain.hpp`.
17. Added explicit cache option `RAWRXD_ENABLE_MISSING_HANDLER_STUBS` (default OFF).
18. Added explicit cache option `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES` (default OFF).
19. Corrected Gold lane compile definition to `RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS=0` to match active stub-backed source wiring.

## Remaining for next batches

- Replace fallback shim providers with real providers where symbol ownership is available in linked production lanes.
- Move `src/core/missing_handler_stubs.cpp` off history include indirection into local canonical ownership.
- Decompose `rawr_engine_link_shims.cpp` into target-scoped fallback providers to avoid broad dissolved behavior surfaces.
