# Production Symbol Wiring - Batch 06

Batch 06 hardens unresolved/dissolved symbol behavior by aligning hotpatch/snapshot/camellia/perf/watchdog shim signatures in `rawr_engine_link_shims.cpp` with production headers and call contracts.

## Batch 06 fixes applied (15+)

1. Aligned `asm_hotpatch_restore_prologue` to `int(uint32_t)`.
2. Aligned `asm_hotpatch_backup_prologue` to `int(void*, uint32_t)`.
3. Aligned `asm_hotpatch_flush_icache` size parameter to `size_t`.
4. Aligned `asm_hotpatch_verify_prologue` to `int(void*, uint32_t)`.
5. Aligned `asm_hotpatch_free_shadow` to `int(void*, size_t)`.
6. Aligned `asm_snapshot_capture` to `int(void*, uint32_t, size_t)`.
7. Aligned `asm_snapshot_restore` to `int(uint32_t)`.
8. Aligned `asm_snapshot_discard` to `int(uint32_t)`.
9. Aligned `asm_snapshot_verify` to `int(uint32_t, uint32_t)`.
10. Aligned `asm_camellia256_auth_encrypt_file` to two-path args `(const char*, const char*)`.
11. Aligned `asm_camellia256_auth_decrypt_file` to two-path args `(const char*, const char*)`.
12. Aligned `Enterprise_DevUnlock` fallback return type to `int64_t`.
13. Aligned `asm_perf_begin` to `uint64_t(uint32_t)`.
14. Aligned `asm_perf_end` to `uint64_t(uint32_t, uint64_t)`.
15. Aligned `asm_watchdog_get_status` to `int(void*)`.
16. Aligned `asm_watchdog_get_baseline` to `int(uint8_t*)`.
17. Aligned `asm_perf_read_slot` to `int(uint32_t, void*)`.
18. Aligned `asm_perf_reset_slot` to `int(uint32_t)`.
19. Aligned `asm_perf_get_slot_count` return type to `uint32_t`.
20. Aligned `asm_perf_get_table_base` return type to `void*`.
21. Aligned `asm_camellia256_auth_encrypt_buf` to `(uint8_t*, uint32_t, uint8_t*, uint32_t*)`.
22. Aligned `asm_camellia256_auth_decrypt_buf` to `(const uint8_t*, uint32_t, uint8_t*, uint32_t*)`.
23. Aligned `asm_apply_memory_patch` length type to `size_t`.

## Why this batch matters

These signatures are consumed by production headers (`shadow_page_detour.hpp`, `camellia256_bridge.hpp`, `perf_telemetry.hpp`, `watchdog_service.hpp`) and call sites in active targets. Prior mismatches kept symbols linked but with dissolved ABI correctness.

## Remaining for next batches

- Align remaining `asm_pyre_*` shim signatures with `pyre_compute.h`.
- Continue replacing broad no-op shim lanes with concrete subsystem providers.
- Remove `history/reconstructed` include indirection from `src/core/missing_handler_stubs.cpp`.
