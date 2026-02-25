# Vulkan/GPU Integration Architecture

## Overview

RawrXD uses a **tiered GPU compute strategy**: Vulkan SDK is optional at build time.
When present, GPU-accelerated inference kernels (MatMul, FlashAttention, quantization)
run on the GPU. When absent, all operations fall back to CPU scalar or AVX512 paths.

## Build-Time Detection

CMake handles Vulkan detection automatically:

```cmake
find_package(Vulkan QUIET)
# Sets RAWR_HAS_VULKAN=1 (found) or RAWR_HAS_VULKAN=0 (not found)
```

The compile definition `RAWR_HAS_VULKAN` is set on all targets:
- `RawrEngine`
- `RawrXD_Gold`
- `RawrXD-Win32IDE`

If the Vulkan SDK is not installed, the build succeeds with CPU-only inference.

## Source File Architecture

### Headers (two-tier)

| File | Purpose |
|------|---------|
| `include/vulkan_compute.h` | Full Vulkan compute class with `#if RAWR_HAS_VULKAN` guard. Uses real Vulkan types when SDK present, dummy `void*` types when absent. Lives in `CPUInference` namespace. |
| `src/vulkan_compute.h` | Minimal stub header with inline no-op methods. Used by `vulkan_compute.cpp` (local include). No namespace. |

### Implementation Files

| File | Purpose | Guard |
|------|---------|-------|
| `src/vulkan_compute.cpp` | Full VulkanCompute class (1700+ lines): instance creation, device selection, compute pipelines, buffer management, KV cache, async command pools, SPIR-V shader loading | `#if RAWR_HAS_VULKAN` — entire file is conditional |
| `src/win32app/VulkanRenderer.cpp` | Win32 surface + swapchain renderer. Loads `vulkan-1.dll` dynamically at runtime (no SDK dependency) | Runtime `LoadLibrary` — always compiles |
| `src/vulkan_masm.asm` | MASM x64 Q5_1 quantization kernel (`QuantQ5_1`), FP32→FP16 conversion. CPU-side SIMD, not GPU compute | N/A — pure assembly, always assembled |

### MASM Assembly Shims

| File | Purpose |
|------|---------|
| `src/asm/RawrXD_VulkanBridge.asm` | GPU dispatch shim: `VkBridge_Init`, `MatMul`, `FlashAttn`, `Hotswap` entry points |
| `src/asm/RawrXD_GGUF_Vulkan_Loader.asm` | Memory-mapped GGUF parser with FNV-1a hashing and tensor staging buffer setup |

## Runtime Fallback Chain

```
1. Vulkan SDK present + compatible GPU  →  GPU compute (SPIR-V shaders)
2. Vulkan SDK present + no GPU          →  CPU scalar (VulkanCompute::Execute* methods)
3. No Vulkan SDK                        →  CPU scalar + AVX512 (vulkan_masm.asm kernels)
```

The `VulkanRenderer` (swapchain/presentation) uses dynamic loading (`LoadLibrary("vulkan-1.dll")`)
and works independently of the compile-time SDK detection.

## Key Operations

| Operation | GPU Path | CPU Fallback |
|-----------|----------|-------------|
| MatMul | `DispatchMatMul()` via SPIR-V compute shader | `ExecuteMatMul()` scalar loop |
| Attention | FlashAttention v2 compute shader | `ExecuteAttention()` |
| RMSNorm | Compute shader | `ExecuteRMSNorm()` |
| SiLU | Compute shader | `ExecuteSiLU()` |
| Softmax | Compute shader | `ExecuteSoftmax()` |
| Q5_1 Quantize | N/A | `QuantQ5_1` (MASM x64 AVX) |
| FP32→FP16 | N/A | `FP32_TO_FP16` (MASM x64) |

## Compile Definition Reference

| Define | Value | Meaning |
|--------|-------|---------|
| `RAWR_HAS_VULKAN=1` | CMake sets when `find_package(Vulkan)` succeeds or manual SDK path found | Full Vulkan compute available |
| `RAWR_HAS_VULKAN=0` | CMake sets when no Vulkan SDK detected | CPU-only mode, stub implementations used |

## Adding New GPU Kernels

1. Write SPIR-V compute shader (`.comp` → `glslangValidator` → `.spv`)
2. Add `LoadShader()` + `CreateComputePipeline()` call in `VulkanCompute`
3. Guard new code with `#if RAWR_HAS_VULKAN`
4. Add CPU scalar fallback in the `#else` branch or in `Execute*` methods
5. Register MASM entry point in `RawrXD_VulkanBridge.asm` if needed

## Verification

```bash
# Check if Vulkan is detected during CMake configure:
cmake -B build -S . 2>&1 | findstr /i "Vulkan"
# Expected: "[Vulkan] Found SDK: ..." or "[Vulkan] SDK not found — GPU compute uses CPU fallback"

# Runtime check (in IDE console):
# VulkanCompute::Initialize() returns true (GPU) or false (CPU fallback)
```
