# GGML_AUDIT.md

## Folder: `src/ggml/`

### Summary
This folder contains the core tensor logic, quantization, and backend abstraction for GGML. The code is currently implemented in C/C++ and relies on external libraries for BLAS, CUDA, Vulkan, and other GPU backends. Quantization routines are implemented in C, with references to CPU and GPU-specific code.

### Key Files Audited
- `ggml.c`: Main tensor logic, memory management, logging, and platform-specific code. Relies on external allocators and threading libraries. Quantization via `ggml-quants.h`.
- `ggml.cpp`: C++ glue for exception handling and backtrace printing.
- `ggml-quants.c`: Quantization and dequantization routines for multiple formats (Q4_0, Q4_1, Q5_0, Q5_1, Q8_0, Q8_1, MXFP4, etc.). Uses C math and memory functions.
- `ggml-backend.cpp`: Backend abstraction, buffer management, and device selection. Supports multiple backends (CPU, BLAS, CUDA, Vulkan, etc.).

### External Dependencies
- BLAS libraries (for tensor ops)
- CUDA Toolkit (for GPU backend)
- Vulkan SDK (for GPU backend)
- Platform-specific allocators (Windows, Linux, macOS)
- Zlib/Zstd (for compression)

### Stub/Placeholder Routines
- Many backend selection and device routines are stubbed for unsupported platforms or missing libraries.
- Quantization routines are implemented in C, but GPU-accelerated versions are only available if compiled with CUDA/Vulkan.
- Backend buffer and device selection logic is generic, with platform-specific code guarded by macros.

### MASM Migration Targets
- All tensor ops (matmul, add, mul, etc.) to be replaced with MASM SIMD routines (`ggml_masm/tensor_ops.asm`).
- Quantization/dequantization routines to be ported to MASM (`ggml_masm/quantization.asm`).
- Backend buffer/device selection to be refactored to call MASM routines via C interface (`ggml_masm_bridge.h`).
- Remove all BLAS, CUDA, Vulkan, and platform-specific allocators in favor of MASM64 implementations.
- Compression routines to be ported to MASM (`ggml_masm/compression.asm`).

### Next Steps
- [ ] Audit all backend subfolders (`ggml-blas`, `ggml-cpu`, `ggml-cuda`, `ggml-vulkan`, etc.) for external calls and stub routines.
- [ ] Write MASM64 replacements for all tensor ops and quantization routines.
- [ ] Expose MASM tensor ops via C interface and update backend selection logic.
- [ ] Remove all external dependencies and finalize MASM-only backend.
- [ ] Add regression and fuzz tests for MASM tensor ops and quantization.
