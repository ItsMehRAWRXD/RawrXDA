# GPU Backend Audit - src/gpu/

## Files in Directory
- `gpu_backend.cpp` (~120 lines) - Backend selector (stubbed)
- `kv_cache_optimizer.cpp` (~70 lines) - Sliding-window KV cache (CPU only)
- `speculative_decoder.cpp` (~70 lines) - Random-token speculative decoder (CPU only)

## External Dependencies Identified

### gpu_backend.cpp
**Current State:** Purely stubbed; `initializeVulkan()` always returns true, `initializeCuda()` always returns false, and no calls are made to ggml or MASM. Backend selection logic logs but never touches hardware.
**Real Backends Available:** MASM implementations already exist in `gpu_masm/gpu_backend.asm` (InitializeGPUBackend, AllocateGPUMemory, etc.) and `gpu_masm/gpu_detection.asm` (GPU_Detect, GPU_DeviceList, etc.). These are not yet invoked.

**Actions Needed (highest priority):**
1. Define C-callable shims for the MASM entry points and include them in the build.
2. Replace stub init functions with calls to `InitializeGPUBackend` using a preferred backend order (Vulkan->CUDA->ROCm->CPU) and propagate real success/failure.
3. Add structured logging + latency metrics around init paths.
4. Wire memory allocation/teardown in C++ to MASM alloc/free routines.

### kv_cache_optimizer.cpp
**Current State:** CPU-only, deterministic sliding-window eviction; no SIMD/GPU usage.
**Actions Needed:**
1. Add unit tests for eviction correctness and sizing.
2. Expose hooks to MASM tensor movement (once available) for GPU-resident caches.
3. Add metrics (evictions, window hits, bytes retained) for observability.

### speculative_decoder.cpp
**Current State:** Generates random token IDs and “verifies” by returning them unchanged; no model interaction.
**Actions Needed:**
1. Replace random generation with draft-model inference (CPU first, MASM later).
2. Implement verification against target model; add acceptance/reject metrics.
3. Provide deterministic tests and latency measurements.

## MASM64 Integration Plan (updated)

### Phase 1: Bindings
- Add C prototypes in a shared header (e.g., `gpu_masm_bridge.h`) for `InitializeGPUBackend`, `AllocateGPUMemory`, `FreeGPUMemory`, `GPU_Detect`, and exported device structs.
- Update CMake to assemble/link `gpu_masm/*.asm` and expose symbols to C++.

### Phase 2: Backend Wiring
- Replace `initializeVulkan`/`initializeCuda` with calls into `InitializeGPUBackend` using backend IDs (1=Vulkan-like, 2=CUDA-like, 3=ROCm-like, 0=CPU).
- Route allocation/free from `gpu_backend.cpp` to the MASM allocators.
- Surface detection results from `GPU_DeviceList` into Qt logs/telemetry.

### Phase 3: Feature Parity
- Port KV cache movement to MASM tensor ops once bindings exist.
- Replace speculative decoding randomness with MASM kernels or CPU draft/verify.
- Add regression tests for backend selection, cache eviction, and speculative acceptance rates.

## Next Steps
1. Add MASM C interface header and link rules; expose detection/alloc/init symbols to C++.
2. Refactor `gpu_backend.cpp` to remove stubs and call MASM backends with real success/failure propagation.
3. Add observability (logs + latency timers) around init and cache paths.
4. Write regression tests for backend selection and KV eviction.

## Dependencies to Remove
- Vulkan SDK
- CUDA Toolkit
- ROCm/HIP SDK
- GGML GPU backends

## Expected Outcome
- Pure MASM64 GPU backend implementation exposed through C++
- Direct hardware access via MASM, no external GPU SDKs
- Tested backend selection, memory management, and speculative decoding hooks
