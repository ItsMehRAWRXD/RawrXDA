# Unlinked symbol resolution — batch 01 (15 symbols)

**Target:** Phase Ω Omega orchestrator `extern "C"` entry points (MASM ABI).

**Implementation:** `src/core/omega_asm_native_kernel.cpp` — mutex-backed kernel state, FNV-1a requirement hashing, task ID assignment, pipeline stats, agent spawn/step, world-model updates. Behaviour aligned with the `#else` software paths in `omega_orchestrator.cpp`.

**Symbols (15):**

1. `asm_omega_init`
2. `asm_omega_shutdown`
3. `asm_omega_ingest_requirement`
4. `asm_omega_plan_decompose`
5. `asm_omega_architect_select`
6. `asm_omega_implement_generate`
7. `asm_omega_verify_test`
8. `asm_omega_deploy_distribute`
9. `asm_omega_observe_monitor`
10. `asm_omega_evolve_improve`
11. `asm_omega_execute_pipeline`
12. `asm_omega_get_stats`
13. `asm_omega_agent_spawn`
14. `asm_omega_agent_step`
15. `asm_omega_world_model_update`

**Also:**

- `src/core/omega_orchestrator_types.hpp` — shared enums/`OmegaStats`/`WorldModel` without pulling Win32 (keeps the kernel TU clean).
- Removed duplicate Omega symbols from `subsystem_mode_fallbacks.cpp` and incorrect-ABI duplicates from `rawr_engine_link_shims.cpp`.
- CMake: `omega_asm_native_kernel.cpp` listed next to `omega_orchestrator.cpp` for `RawrXD-Win32IDE` and `RawrEngine`.

**Next batch (suggested):** first 15 `asm_mesh_*` from `mesh_brain.cpp` (or next linker group).
