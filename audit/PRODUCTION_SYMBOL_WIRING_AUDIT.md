# Production Symbol Wiring Audit

Scope: active production CMake graph rooted at `/workspace/CMakeLists.txt` (`RawrEngine`, `RawrXD_Gold`, `src/reverse_engineering`, and Win32IDE block when `WIN32`).

## Fixed in this branch

1. **Duplicate quantum time manager translation units in Gold**
   - **Issue:** `src/agent/quantum_dynamic_time_manager.cpp` and `src/agent/quantum_dynamic_time_manager_impl.cpp` both provide overlapping `QuantumDynamicTimeManager` symbols.
   - **Fix:** mirrored RawrEngine behavior in Gold and removed `_impl.cpp` from `GOLD_UNDERSCORE_SOURCES` when `quantum_missing_impl.cpp` exists.

2. **Miswired MASM-only source in generic C++ source list**
   - **Issue:** `src/asm/quantum_beaconism_backend.asm` was in `SOURCES` despite being excluded from MASM kernel source wiring.
   - **Fix:** removed it from generic `SOURCES` so it cannot be treated as a regular C/C++ translation unit.

3. **Duplicate resource file on Win32IDE executable**
   - **Issue:** `src/win32app/RawrXD-Win32IDE.rc` was passed both via `WIN32IDE_SOURCES` and again in `add_executable(...)`.
   - **Fix:** removed the duplicate explicit argument.

4. **Duplicate inference source entry**
   - **Issue:** `src/core/ignite_800b.cpp` appeared twice in `INFERENCE_ENGINE_SOURCES`.
   - **Fix:** removed the duplicate entry.

## Remaining dissolved/stubbed/unwired hotspots

These are still present and should be treated as non-final production wiring:

1. **`src/core/rawr_engine_link_shims.cpp`**
   - Contains many link-shim placeholders returning `0` / `nullptr`.
   - These satisfy linkage but represent dissolved behavior for any path that reaches them.

2. **`src/core/auto_feature_stub_impl.cpp`**
   - Auto-generated command handlers returning `CommandResult::ok("stub")`.
   - Large placeholder lane still linked into core targets.

3. **`src/core/missing_handler_stubs.cpp`**
   - Currently a one-line include forwarding to:
     - `../../history/reconstructed/src/core/missing_handler_stubs.cpp`
   - Production source depends on `history/reconstructed` indirection instead of local canonical implementation.

4. **Fallback replacement option points to missing source files**
   - `RAWR_REPLACE_SWARM_LICENSE_FALLBACKS` removes:
     - `src/core/swarm_network_stubs.cpp`
     - `src/core/enterprise_license_stubs.cpp`
   - Those paths are absent in the active `src/core` tree in this workspace snapshot.

## Recommendation for full "no stubs/unwired" production policy

1. Make a strict `RAWR_PRODUCTION_NO_STUBS=ON` profile that:
   - rejects `rawr_engine_link_shims.cpp`,
   - rejects `auto_feature_stub_impl.cpp`,
   - rejects history-based include indirection for `missing_handler_stubs.cpp`.
2. Move/restore canonical `missing_handler_stubs.cpp` implementation under `src/core/`.
3. Replace each shimmed symbol with a real provider TU or hard fail configure if provider is missing.
4. Keep duplicate-source and duplicate-resource checks as CI gates.
