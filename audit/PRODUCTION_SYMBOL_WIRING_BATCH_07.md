# Production Symbol Wiring - Batch 07

Batch 07 continues unlinked/dissolved symbol cleanup by aligning remaining `rawr_engine_link_shims.cpp` kernel entry signatures with active production headers for Pyre and Mesh subsystems.

## Batch 07 fixes applied (15+)

1. Aligned `asm_pyre_gemm_fp32` to `(const float*, const float*, float*, uint32_t, uint32_t, uint32_t)`.
2. Aligned `asm_pyre_rmsnorm` to `(const float*, const float*, float*, uint32_t, float)`.
3. Aligned `asm_pyre_silu` to `(float*, uint32_t)`.
4. Aligned `asm_pyre_rope` to `(float*, uint32_t, uint32_t, uint32_t, float)`.
5. Aligned `asm_pyre_embedding_lookup` to `(const float*, const uint32_t*, float*, uint32_t, uint32_t)`.
6. Aligned `asm_pyre_gemv_fp32` to `(const float*, const float*, float*, uint32_t, uint32_t)`.
7. Aligned `asm_pyre_add_fp32` to `(const float*, const float*, float*, uint32_t)`.
8. Aligned `asm_pyre_mul_fp32` to `(const float*, const float*, float*, uint32_t)`.
9. Aligned `asm_pyre_softmax` to `(float*, uint32_t)`.
10. Aligned `asm_mesh_crdt_delta` to `uint64_t(uint64_t, void*, uint32_t)`.
11. Aligned `asm_mesh_get_stats` return to `void*`.
12. Aligned `asm_mesh_dht_find_closest` to `uint32_t(const void*, void*, uint32_t)`.
13. Aligned `asm_mesh_fedavg_aggregate` to `int(const void*, uint32_t, void*, uint32_t)`.
14. Aligned `asm_mesh_crdt_merge` to `uint64_t(const void*, uint32_t)`.
15. Aligned `asm_mesh_dht_xor_distance` to `uint32_t(const void*, const void*)`.
16. Aligned `asm_mesh_shard_hash` to `int(const void*, uint64_t, void*)`.
17. Aligned `asm_mesh_quorum_vote` to `int(const uint8_t*, uint32_t, uint32_t)`.
18. Aligned `asm_mesh_topology_active_count` return type to `uint32_t`.
19. Aligned `asm_mesh_shard_bitfield` to `int(uint32_t, uint32_t)`.
20. Aligned `asm_mesh_gossip_disseminate` to `uint32_t(const void*, uint64_t, void*)`.

## Why this batch matters

Before this batch, these shims linked successfully but carried ABI/arity drift versus `pyre_compute.h` and `mesh_brain.hpp`, creating dissolved symbol correctness. This batch restores contract-level alignment while preserving fallback link closure.

## Remaining for next batches

- Finish ABI alignment for remaining shim surfaces (spengine/lsp/gguf/hwsynth/speciator/neural/omega where canonical declarations exist).
- Replace fallback shims with concrete providers in production targets as each subsystem becomes link-complete.
- Localize `missing_handler_stubs.cpp` ownership and reduce history-indirected production wiring.
