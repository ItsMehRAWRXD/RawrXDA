# TRANSFORMER_AUDIT.md

## Folder: `src/` (transformer logic)

### Summary
Transformer logic is distributed across several files (e.g., `transformer_block_scalar.cpp`, `transformer_block_scalar.h`, `MultiModalModelRouter.cpp`, `multi_modal_model_router.cpp`). These files provide transformer block implementations, model routing, and integration for the IDE/CLI. Some routines are stubs or placeholders for future MASM migration.

### Key Files Audited
- `transformer_block_scalar.cpp`, `transformer_block_scalar.h`: Scalar transformer block implementation, CPU-based, uses C++ and external tensor ops.
- `MultiModalModelRouter.cpp`, `multi_modal_model_router.cpp`: Model routing and integration logic for multimodal transformers.

### External Dependencies
- C++ standard library (memory, threading, chrono)
- Qt (optional, for logging and signals)
- GGML tensor ops (to be replaced)
- Model loader and backend logic

### Stub/Placeholder Routines
- Some transformer block routines are stubbed or simplified for unsupported features.
- Model routing logic relies on external tensor ops and backend selection.

### MASM Migration Targets
- All transformer block routines to be ported to MASM (`ggml_masm/tensor_ops.asm`, `gpu_masm/gpu_backend.asm`).
- Remove all external tensor op dependencies in favor of MASM64 implementations.
- Refactor model routing and integration logic for MASM64 compatibility.
- Add MASM64 support for multimodal transformer blocks and routing.

### Next Steps
- [ ] Implement MASM transformer block routines and wire into model router logic.
- [ ] Refactor transformer logic to call MASM tensor ops and backend routines.
- [ ] Remove all external tensor op dependencies.
- [ ] Add regression and fuzz tests for MASM transformer blocks and routing.
- [ ] Document all MASM entry points and test coverage.
