# GGML_BLAS_AUDIT.md

## Folder: `src/ggml-blas/`

### Summary
This folder provides BLAS backend integration for GGML tensor operations. It supports multiple BLAS libraries (OpenBLAS, MKL, BLIS, Accelerate, NVPL) and falls back to CBLAS if none are available. The backend is implemented in C++ and uses external BLAS libraries for matrix multiplication and other tensor ops.

### Key Files Audited
- `ggml-blas.cpp`: Main BLAS backend implementation. Selects and calls external BLAS routines for matmul and out_prod. Supports threading and async execution via OpenMP or std::future.

### External Dependencies
- OpenBLAS, MKL, BLIS, Accelerate, NVPL, or CBLAS (for tensor ops)
- OpenMP or std::future for threading

### Stub/Placeholder Routines
- Device memory reporting and some device properties are stubbed (TODOs present).
- Only matmul and out_prod are implemented; other ops abort with unsupported message.

### MASM Migration Targets
- All BLAS calls (cblas_sgemm, etc.) to be replaced with MASM SIMD tensor ops (`ggml_masm/tensor_ops.asm`).
- Remove all external BLAS library dependencies.
- Refactor backend to call MASM routines via C interface (`ggml_masm_bridge.h`).

### Next Steps
- [ ] Remove BLAS library includes and replace with MASM tensor ops.
- [ ] Refactor backend to call MASM routines for matmul and out_prod.
- [ ] Remove all external dependencies and finalize MASM-only backend.
- [ ] Add regression and fuzz tests for MASM tensor ops.
