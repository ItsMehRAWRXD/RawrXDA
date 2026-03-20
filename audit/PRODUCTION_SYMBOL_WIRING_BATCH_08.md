# Production Symbol Wiring - Batch 08

Batch 08 resolves the next production-grade dissolved symbol tranche by aligning remaining `rawr_engine_link_shims.cpp` bridge signatures for SP-Engine, GGUF/LSP/QuadBuffer, HardwareSynth, Speciator, NeuralBridge, and OmegaOrchestrator contracts.

## Batch 08 fixes applied (15+)

1. Aligned `asm_spengine_rollback` to `int(uint32_t)`.
2. Aligned `asm_hwsynth_est_resources` to `int(const void*, uint32_t, void*)`.
3. Aligned `asm_hwsynth_predict_perf` return/type to `uint64_t(const void*, const void*)`.
4. Aligned `asm_hwsynth_get_stats` return to `void*`.
5. Aligned `asm_hwsynth_gen_gemm_spec` to `int(const void*, uint32_t, void*)`.
6. Aligned `asm_hwsynth_gen_jtag_header` to `uint64_t(void*, uint64_t, uint32_t, const void*)`.
7. Aligned `asm_hwsynth_profile_dataflow` to `int(const void*, uint32_t, uint32_t, uint32_t, uint32_t, void*)`.
8. Aligned `asm_hwsynth_init` to `int()`.
9. Aligned `asm_speciator_mutate` to `int(uint32_t, uint32_t)`.
10. Aligned `asm_speciator_gen_variant` to `int(uint32_t, void*)`.
11. Aligned `asm_speciator_get_stats` return to `void*`.
12. Aligned `asm_speciator_create_genome` to `int32_t(uint32_t, const void*, uint32_t)`.
13. Aligned `asm_speciator_crossover` to `int32_t(uint32_t, uint32_t)`.
14. Aligned `asm_speciator_speciate` to `int(uint32_t)`.
15. Aligned `asm_speciator_evaluate` to `uint64_t(uint32_t, void*, uint64_t)`.
16. Aligned `asm_speciator_compete` to `int(uint32_t*, uint32_t)`.
17. Aligned `asm_speciator_migrate` to `int32_t(uint32_t, uint32_t)`.
18. Aligned `asm_speciator_init` to `int()`.
19. Aligned `asm_speciator_select` to `int32_t(uint32_t)`.
20. Aligned `asm_neural_classify_intent` to `int(const float*)`.
21. Aligned `asm_neural_haptic_pulse` to `int(int, int, float*)`.
22. Aligned `asm_neural_encode_command` to `int(int, uint32_t, void*)`.
23. Aligned `asm_neural_acquire_eeg` to `int(const void*, int)`.
24. Aligned `asm_neural_adapt` to `int(int, int)`.
25. Aligned `asm_neural_fft_decompose` to `int(int)`.
26. Aligned `asm_neural_init` to `int()`.
27. Aligned `asm_neural_calibrate` to `int(int, int)`.
28. Aligned `asm_neural_detect_event` to `int()`.
29. Aligned `asm_neural_gen_phosphene` to `int(int, int, void*)`.
30. Aligned `asm_neural_extract_csp` to `int(const float*, float*)`.
31. Aligned `asm_neural_get_stats` return to `void*`.
32. Aligned `asm_omega_implement_generate` to `uint64_t(int)`.
33. Aligned `asm_omega_agent_step` to `int(int)`.
34. Aligned `asm_omega_plan_decompose` to `int(uint64_t, uint32_t*, int)`.
35. Aligned `asm_omega_evolve_improve` to `int(int, int)`.
36. Aligned `asm_omega_init` to `int()`.
37. Aligned `asm_omega_get_stats` return to `void*`.
38. Aligned `asm_omega_verify_test` to `int(int)`.
39. Aligned `asm_omega_architect_select` to `int(int, int)`.
40. Aligned `asm_omega_agent_spawn` to `int(int)`.
41. Aligned `asm_omega_observe_monitor` to `int(int)`.
42. Aligned `asm_omega_deploy_distribute` to `int(int, int)`.
43. Aligned `asm_omega_execute_pipeline` to `int()`.
44. Aligned `asm_omega_ingest_requirement` to `uint64_t(const char*, int)`.
45. Aligned `asm_omega_world_model_update` to `int(int, int)`.

## Why this batch matters

These entrypoints are consumed by active production headers and subsystems (`masm_bridge_cathedral.h`, `hardware_synthesizer.hpp`, `speciator_engine.hpp`, `neural_bridge.hpp`, `omega_orchestrator.hpp`). Prior signatures linked but dissolved contract correctness across C/ASM boundaries.

## Remaining for next batches

- Finish ABI normalization for remaining bridge entries still not synchronized with canonical headers.
- Continue replacing broad no-op shim providers with concrete target-scoped providers where available.
- Eliminate history-indirected production source ownership for missing-handler provider lanes.
