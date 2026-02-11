# GGML_CUDA_AUDIT.md

## Folder: `src/ggml-cuda/`

### Summary
This folder contains CUDA-specific tensor operations, backend logic, and device management for GGML. The code is implemented in CUDA C++ and relies on the CUDA Toolkit, device properties, and external CUDA libraries for GPU acceleration.

### Key Files Audited
- `ggml-cuda.cu`: Main CUDA backend implementation. Handles device selection, memory management, error handling, and tensor ops via CUDA kernels. Relies on many CUDA headers and device properties.

### External Dependencies
- CUDA Toolkit (for device management, memory, and kernel launches)
- CUDA device properties and error handling
- Platform-specific CUDA libraries (Windows, Linux, etc.)

### Stub/Placeholder Routines
- Some device property extraction and error handling routines are stubbed or simplified for unsupported platforms.
- Many tensor ops are implemented as CUDA kernels, but fallback logic is present for missing features.

### MASM Migration Targets
- All CUDA kernel launches and device management to be replaced with MASM SIMD tensor ops and MASM64 device management (`gpu_masm/`, `ggml_masm/`).
- Remove all CUDA Toolkit and platform-specific CUDA dependencies in favor of MASM64 implementations.
- Refactor backend to call MASM routines via C interface (`gpu_masm_bridge.h`, `ggml_masm_bridge.h`).

### Next Steps
- [ ] Remove all CUDA Toolkit and device property dependencies.
- [ ] Refactor CUDA tensor ops to call MASM SIMD routines.
- [ ] Port device management and memory routines to MASM64.
- [ ] Add regression and fuzz tests for MASM tensor ops and device management.
