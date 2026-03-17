// native_core/compute_backend.hpp
// Unified GPU/CPU compute abstraction with Vulkan, CUDA, and AVX-512 backends
// Zero external include deps at compile time — backends are runtime-loaded via LoadLibrary
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>

// Forward declare MASM kernels
extern "C" {
    void Q4_0_DequantBlock_AVX2(const void* input, float* output);
    void Q4_0_DequantBatch_AVX2(const void* input, float* output, uint64_t blockCount);

    struct MatMulArgs {
        float*   A;
        float*   B;
        float*   C;
        uint64_t M;
        uint64_t N;
        uint64_t K;
        float    alpha;
        float    beta;
    };
    void MatMul_AVX512(MatMulArgs* args);
    void MatMul_AVX2(MatMulArgs* args);
    void MatVec_AVX512(float* A, float* x, float* y, uint64_t M, uint64_t K);
    float DotProduct_AVX512(float* a, float* b, uint64_t N);
}

namespace RawrXD::Native {

// ============================================================================
// Compute Backend Type
// ============================================================================
enum class ComputeBackend : uint32_t {
    CPU_AVX2    = 0,    // Always available fallback
    CPU_AVX512  = 1,    // Requires AVX-512F
    VULKAN      = 2,    // Runtime-loaded vulkan-1.dll
    CUDA        = 3,    // Runtime-loaded nvcuda.dll
    DIRECTX12   = 4,    // Runtime-loaded d3d12.dll (future)
    AUTO        = 0xFF  // Auto-detect best available
};

inline const char* BackendName(ComputeBackend b) {
    switch (b) {
        case ComputeBackend::CPU_AVX2:   return "CPU/AVX2";
        case ComputeBackend::CPU_AVX512: return "CPU/AVX-512";
        case ComputeBackend::VULKAN:     return "Vulkan";
        case ComputeBackend::CUDA:       return "CUDA";
        case ComputeBackend::DIRECTX12:  return "DirectX 12";
        default:                         return "Auto";
    }
}

// ============================================================================
// CPU Feature Detection (CPUID-based)
// ============================================================================
struct CpuFeatures {
    bool avx2    = false;
    bool avx512f = false;
    bool fma     = false;
    bool f16c    = false;

    static CpuFeatures Detect() {
        CpuFeatures f;
        int info[4]{};

        // EAX=7, ECX=0: Extended features
        __cpuidex(info, 7, 0);
        f.avx2    = (info[1] & (1 << 5))  != 0;   // EBX bit 5
        f.avx512f = (info[1] & (1 << 16)) != 0;   // EBX bit 16

        // EAX=1: Basic features
        __cpuid(info, 1);
        f.fma  = (info[2] & (1 << 12)) != 0;      // ECX bit 12
        f.f16c = (info[2] & (1 << 29)) != 0;      // ECX bit 29

        return f;
    }
};

// ============================================================================
// GPU Buffer Abstraction
// ============================================================================
struct GpuBuffer {
    void*    devicePtr  = nullptr;
    uint64_t sizeBytes  = 0;
    uint32_t bindingIdx = 0;
    bool     isStaging  = false;
};

// ============================================================================
// Vulkan Backend (Runtime-Loaded)
// ============================================================================
class VulkanBackend {
    HMODULE hVulkan_ = nullptr;
    bool    available_ = false;

    // Vulkan function pointers (loaded at runtime)
    using PFN_vkCreateInstance = void*;  // Opaque — full Vulkan types not needed at compile
    // ... additional function pointers would be loaded via GetProcAddress

public:
    bool Initialize() {
        hVulkan_ = LoadLibraryW(L"vulkan-1.dll");
        if (!hVulkan_) return false;

        // Load core Vulkan functions
        auto vkGetInstanceProcAddr = reinterpret_cast<void*(*)(void*, const char*)>(
            GetProcAddress(hVulkan_, "vkGetInstanceProcAddr")
        );
        if (!vkGetInstanceProcAddr) {
            FreeLibrary(hVulkan_);
            hVulkan_ = nullptr;
            return false;
        }

        available_ = true;
        return true;
    }

    void Shutdown() {
        if (hVulkan_) {
            FreeLibrary(hVulkan_);
            hVulkan_ = nullptr;
        }
        available_ = false;
    }

    bool IsAvailable() const { return available_; }

    // Dispatch compute shader for matrix multiply
    bool DispatchMatMul(const float* A, const float* B, float* C,
                        uint64_t M, uint64_t N, uint64_t K) {
        if (!available_) return false;
        // Full Vulkan compute pipeline would be here:
        // 1. Create buffers
        // 2. Upload A, B
        // 3. Bind compute shader
        // 4. Dispatch workgroups
        // 5. Read back C
        // For now, fall back to CPU
        return false;
    }

    ~VulkanBackend() { Shutdown(); }
};

// ============================================================================
// CUDA Backend (Runtime-Loaded)
// ============================================================================
class CudaBackend {
    HMODULE hCuda_    = nullptr;
    HMODULE hCublas_  = nullptr;
    bool    available_ = false;

    // CUDA driver API function pointers
    using PFN_cuInit        = int(*)(unsigned int);
    using PFN_cuDeviceGet   = int(*)(int*, int);
    using PFN_cuCtxCreate   = int(*)(void**, unsigned int, int);
    using PFN_cuMemAlloc    = int(*)(void**, uint64_t);
    using PFN_cuMemFree     = int(*)(void*);
    using PFN_cuMemcpyHtoD  = int(*)(void*, const void*, uint64_t);
    using PFN_cuMemcpyDtoH  = int(*)(void*, void*, uint64_t);

    PFN_cuInit       cuInit_       = nullptr;
    PFN_cuDeviceGet  cuDeviceGet_  = nullptr;
    PFN_cuCtxCreate  cuCtxCreate_  = nullptr;
    PFN_cuMemAlloc   cuMemAlloc_   = nullptr;

    void* context_ = nullptr;

public:
    bool Initialize() {
        hCuda_ = LoadLibraryW(L"nvcuda.dll");
        if (!hCuda_) return false;

        cuInit_ = reinterpret_cast<PFN_cuInit>(GetProcAddress(hCuda_, "cuInit"));
        if (!cuInit_ || cuInit_(0) != 0) {
            FreeLibrary(hCuda_);
            hCuda_ = nullptr;
            return false;
        }

        cuDeviceGet_ = reinterpret_cast<PFN_cuDeviceGet>(GetProcAddress(hCuda_, "cuDeviceGet"));
        cuCtxCreate_ = reinterpret_cast<PFN_cuCtxCreate>(GetProcAddress(hCuda_, "cuCtxCreate_v2"));
        cuMemAlloc_  = reinterpret_cast<PFN_cuMemAlloc>(GetProcAddress(hCuda_, "cuMemAlloc_v2"));

        // Create context on device 0
        int device = 0;
        if (cuDeviceGet_ && cuDeviceGet_(&device, 0) == 0) {
            if (cuCtxCreate_ && cuCtxCreate_(&context_, 0, device) == 0) {
                available_ = true;
            }
        }

        if (!available_) {
            FreeLibrary(hCuda_);
            hCuda_ = nullptr;
        }

        return available_;
    }

    void Shutdown() {
        if (hCuda_) {
            FreeLibrary(hCuda_);
            hCuda_ = nullptr;
        }
        if (hCublas_) {
            FreeLibrary(hCublas_);
            hCublas_ = nullptr;
        }
        available_ = false;
    }

    bool IsAvailable() const { return available_; }

    bool DispatchMatMul(const float* A, const float* B, float* C,
                        uint64_t M, uint64_t N, uint64_t K) {
        if (!available_) return false;
        // cuBLAS SGEMM dispatch would go here
        return false;
    }

    ~CudaBackend() { Shutdown(); }
};

// ============================================================================
// Unified Compute Engine — auto-selects best backend
// ============================================================================
class ComputeEngine {
    ComputeBackend activeBackend_ = ComputeBackend::CPU_AVX2;
    CpuFeatures    cpuFeatures_{};
    VulkanBackend  vulkan_;
    CudaBackend    cuda_;
    bool           initialized_ = false;

public:
    bool Initialize(ComputeBackend preferred = ComputeBackend::AUTO) {
        cpuFeatures_ = CpuFeatures::Detect();

        if (preferred == ComputeBackend::AUTO) {
            // Priority: CUDA > Vulkan > AVX-512 > AVX2
            if (cuda_.Initialize()) {
                activeBackend_ = ComputeBackend::CUDA;
            } else if (vulkan_.Initialize()) {
                activeBackend_ = ComputeBackend::VULKAN;
            } else if (cpuFeatures_.avx512f) {
                activeBackend_ = ComputeBackend::CPU_AVX512;
            } else {
                activeBackend_ = ComputeBackend::CPU_AVX2;
            }
        } else {
            switch (preferred) {
                case ComputeBackend::CUDA:
                    if (cuda_.Initialize()) { activeBackend_ = ComputeBackend::CUDA; break; }
                    [[fallthrough]];
                case ComputeBackend::VULKAN:
                    if (vulkan_.Initialize()) { activeBackend_ = ComputeBackend::VULKAN; break; }
                    [[fallthrough]];
                case ComputeBackend::CPU_AVX512:
                    if (cpuFeatures_.avx512f) { activeBackend_ = ComputeBackend::CPU_AVX512; break; }
                    [[fallthrough]];
                default:
                    activeBackend_ = ComputeBackend::CPU_AVX2;
                    break;
            }
        }

        initialized_ = true;
        return true;
    }

    void Shutdown() {
        vulkan_.Shutdown();
        cuda_.Shutdown();
        initialized_ = false;
    }

    ComputeBackend ActiveBackend() const { return activeBackend_; }
    const char* ActiveBackendName() const { return BackendName(activeBackend_); }
    const CpuFeatures& GetCpuFeatures() const { return cpuFeatures_; }

    // ================================================================
    // Matrix Multiply: C = alpha * A * B + beta * C
    // ================================================================
    void MatMul(float* A, float* B, float* C,
                uint64_t M, uint64_t N, uint64_t K,
                float alpha = 1.0f, float beta = 0.0f) {

        // Try GPU backends first
        if (activeBackend_ == ComputeBackend::CUDA) {
            if (cuda_.DispatchMatMul(A, B, C, M, N, K)) return;
        }
        if (activeBackend_ == ComputeBackend::VULKAN) {
            if (vulkan_.DispatchMatMul(A, B, C, M, N, K)) return;
        }

        // CPU fallback — use MASM kernels
        MatMulArgs args{};
        args.A = A;
        args.B = B;
        args.C = C;
        args.M = M;
        args.N = N;
        args.K = K;
        args.alpha = alpha;
        args.beta  = beta;

        MatMul_AVX512(&args);  // Works on both AVX2 and AVX-512 (uses ymm)
    }

    // ================================================================
    // Matrix-Vector: y = A * x
    // ================================================================
    void MatVec(float* A, float* x, float* y, uint64_t M, uint64_t K) {
        MatVec_AVX512(A, x, y, M, K);
    }

    // ================================================================
    // Dot Product: d = a . b
    // ================================================================
    float DotProd(float* a, float* b, uint64_t N) {
        return DotProduct_AVX512(a, b, N);
    }

    // ================================================================
    // Q4_0 Dequantization
    // ================================================================
    void DequantQ4_0(const void* quantized, float* output, uint64_t blockCount) {
        Q4_0_DequantBatch_AVX2(quantized, output, blockCount);
    }

    ~ComputeEngine() { Shutdown(); }
};

} // namespace RawrXD::Native
