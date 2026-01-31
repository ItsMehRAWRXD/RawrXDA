# GGML_CPU_AUDIT.md

## Folder: `src/ggml-cpu/`

### Summary
This folder contains CPU-specific tensor operations, quantization, threading, and platform-specific code for GGML. The code is implemented in C/C++ and relies on platform-specific threading and atomic operations, as well as external math and memory libraries.

### Key Files Audited
- `ggml-cpu.c`: Main CPU tensor logic, quantization, threading, atomic operations, and platform-specific code. Relies on external math, memory, and threading libraries.

### External Dependencies
- Platform-specific threading and atomic operations (Windows, Linux, macOS)
- Math and memory libraries (stdlib, math.h, etc.)
- OpenMP or std::future for threading (optional)

### Stub/Placeholder Routines
- Some threading and atomic routines are stubbed or platform-specific.
- Quantization routines are implemented in C, with references to CPU and GPU-specific code.

### MASM Migration Targets
- All CPU tensor ops (matmul, add, mul, etc.) to be replaced with MASM SIMD routines (`ggml_masm/tensor_ops.asm`).
- Quantization/dequantization routines to be ported to MASM (`ggml_masm/quantization.asm`).
- Threading and atomic operations to be refactored for MASM64 compatibility.
- Remove all platform-specific threading and atomic dependencies in favor of MASM64 implementations.

### Next Steps
- [ ] Remove platform-specific threading and atomic operations.
- [ ] Refactor CPU tensor ops to call MASM SIMD routines.
- [ ] Port quantization routines to MASM.
- [ ] Add regression and fuzz tests for MASM tensor ops and quantization.
