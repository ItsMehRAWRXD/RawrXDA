# Production Symbol Wiring - Batch 09

Batch 09 moves production CMake wiring away from dissolved stub-handler defaults and toward real handler ownership, while preserving fallback behavior for explicit legacy lanes.

## Batch 09 fixes applied (15+)

1. Changed default `RAWR_SSOT_PROVIDER` from `EXT` to `AUTO` for production-oriented ownership.
2. Kept provider options unchanged (`CORE/EXT/AUTO/STUBS/FEATURES`) while updating default lane.
3. Removed unconditional `auto_feature_stub_impl.cpp` injection from RawrEngine target sources.
4. Added RawrEngine conditional: include `auto_feature_stub_impl.cpp` only for `EXT` or `STUBS` lanes.
5. Gold lane no longer unconditionally removes `auto_feature_registry.cpp`.
6. Gold lane no longer unconditionally removes `ssot_handlers.cpp`.
7. Gold lane no longer unconditionally removes `ssot_handlers_ext_runtime_minimal.cpp`.
8. Gold lane no longer unconditionally removes `ssot_handlers_ext_isolated.cpp`.
9. Added Gold `AUTO` branch to remove `missing_handler_stubs.cpp` from production lane.
10. Added Gold `AUTO` branch to remove `ssot_handlers_ext_runtime_minimal.cpp`.
11. Added Gold `AUTO` branch to remove `ssot_handlers_ext_isolated.cpp`.
12. Added Gold `AUTO` branch to append `ssot_handlers_ext.cpp` as full handler provider.
13. Added Gold non-AUTO legacy branch to keep fallback/stub provider ownership behavior.
14. Added Gold provider-state variable `_RAWR_GOLD_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS`.
15. Made Gold `RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS` compile definition dynamic from provider-state variable.
16. Aligned `RawrXD_Native_Log` shim signature to `void(const char*)` to match active call sites.

## Why this batch matters

Prior wiring defaulted to an `EXT` lane where command resolution frequently depended on `auto_feature_stub_impl.cpp` no-op handlers. This batch changes production defaults and target source ownership to prioritize real provider lanes and reduce dissolved command behavior.

## Remaining for next batches

- Continue replacing remaining broad shim ownership with concrete subsystem providers where link-complete.
- Remove `history/reconstructed` indirection from `src/core/missing_handler_stubs.cpp`.
- Validate remaining fallback-only symbols and move them behind explicit non-production gates.
