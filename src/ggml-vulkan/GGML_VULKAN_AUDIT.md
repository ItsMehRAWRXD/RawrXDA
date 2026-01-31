# GGML_VULKAN_AUDIT.md

## Folder: `src/ggml-vulkan/`

### Summary
This folder contains Vulkan-specific tensor operations, backend logic, and device management for GGML. The code is implemented in C++ and relies on the Vulkan SDK, device properties, and external Vulkan libraries for GPU acceleration.

### Key Files Audited
- `ggml-vulkan.cpp`: Main Vulkan backend implementation. Handles device selection, memory management, error handling, and tensor ops via Vulkan pipelines and shaders. Relies on Vulkan SDK and device properties.

### External Dependencies
- Vulkan SDK (for device management, memory, and pipeline launches)
- Platform-specific Vulkan libraries (Windows, Linux, etc.)

### Stub/Placeholder Routines
- Some device property extraction and error handling routines are stubbed or simplified for unsupported platforms.
- Many tensor ops are implemented as Vulkan pipelines, but fallback logic is present for missing features.

### MASM Migration Targets
- All Vulkan pipeline launches and device management to be replaced with MASM SIMD tensor ops and MASM64 device management (`gpu_masm/`, `ggml_masm/`).
- Remove all Vulkan SDK and platform-specific Vulkan dependencies in favor of MASM64 implementations.
- Refactor backend to call MASM routines via C interface (`gpu_masm_bridge.h`, `ggml_masm_bridge.h`).

### Next Steps
- [ ] Remove all Vulkan SDK and device property dependencies.
- [ ] Refactor Vulkan tensor ops to call MASM SIMD routines.
- [ ] Port device management and memory routines to MASM64.
- [ ] Add regression and fuzz tests for MASM tensor ops and device management.
