# INFERENCE_ENGINE_AUDIT.md

## Folder: `src/` (inference engine logic)

### Summary
Inference engine logic is distributed across several files (e.g., `inference_engine_stub.cpp`, `cpu_inference_engine.cpp`, `real_time_completion_engine.cpp`, `streaming_engine.cpp`). These files provide model inference, streaming, and completion logic for the IDE/CLI. Some routines are stubs or placeholders for future MASM migration.

### Key Files Audited
- `inference_engine_stub.cpp`: Stubbed inference engine, placeholder for MASM integration.
- `cpu_inference_engine.cpp`: CPU-based inference logic, uses C++ and external libraries for tensor ops.
- `real_time_completion_engine.cpp`, `real_time_completion_engine_v2.cpp`: Real-time completion logic, streaming, and batching.
- `streaming_engine.cpp`, `streaming_enhancements.cpp`: Streaming and speculative decoding logic.

### External Dependencies
- C++ standard library (memory, threading, chrono)
- Qt (optional, for logging and signals)
- GGML tensor ops (to be replaced)
- Model loader and backend logic

### Stub/Placeholder Routines
- `inference_engine_stub.cpp` is a placeholder for MASM integration.
- Some streaming and completion routines are stubbed or simplified for unsupported features.
- CPU inference logic relies on external tensor ops and backend selection.

### MASM Migration Targets
- All inference routines to be ported to MASM (`ggml_masm/tensor_ops.asm`, `gpu_masm/gpu_backend.asm`).
- Remove all external tensor op dependencies in favor of MASM64 implementations.
- Refactor streaming and completion logic for MASM64 compatibility.
- Add MASM64 support for speculative decoding and batching.

### Next Steps
- [ ] Implement MASM inference routines and wire into engine logic.
- [ ] Refactor engine logic to call MASM tensor ops and backend routines.
- [ ] Remove all external tensor op dependencies.
- [ ] Add regression and fuzz tests for MASM inference and streaming.
- [ ] Document all MASM entry points and test coverage.
