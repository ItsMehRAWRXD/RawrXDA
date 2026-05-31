// ============================================================================
// gpu_warmup.hpp — GPU Anti-Lag / Prime System
// ============================================================================
// Pre-warms GPU compute pipelines after initialization to eliminate
// cold-start latency on first real inference. Runs lightweight kernels
// (small matmul, buffer upload/download) to bring GPU out of low-power
// states and prime shader caches.
//
// Usage:
//   GPUPrimeEngine::instance().warmup(backend, device, queue);
//   // or let AMDGPUAccelerator call it automatically after init
// ============================================================================

#pragma once

#include <cstdint>
#include <atomic>
#include <thread>
#include <functional>

namespace RawrXD {
namespace GPU {

// Warmup configuration
struct GPUPrimeConfig {
    uint32_t matmulDim = 256;          // Small matmul dimension (256x256)
    uint32_t bufferSizeBytes = 4 * 1024 * 1024;  // 4MB buffer for upload/download
    uint32_t iterations = 3;         // Number of warmup iterations
    uint32_t maxDurationMs = 200;    // Cap total warmup time
    bool enableMatmul = true;        // Run matmul warmup
    bool enableMemcpy = true;        // Run upload/download warmup
    bool enableShaderCache = true;   // Run dummy compute shader warmup
};

// Warmup result
struct GPUPrimeResult {
    bool success = false;
    uint32_t durationMs = 0;
    uint32_t matmulTimeUs = 0;
    uint32_t memcpyTimeUs = 0;
    uint32_t shaderCacheTimeUs = 0;
    const char* backendName = "none";
};

// ============================================================================
// GPU Prime Engine — Singleton
// ============================================================================
class GPUPrimeEngine {
public:
    static GPUPrimeEngine& instance();

    // Synchronous warmup (blocks until complete)
    GPUPrimeResult warmupSync(void* device, void* queue, const GPUPrimeConfig& config = {});

    // Asynchronous warmup (returns immediately, runs in background)
    void warmupAsync(void* device, void* queue, const GPUPrimeConfig& config = {});

    // Check if warmup is complete
    bool isWarmedUp() const { return m_warmedUp.load(std::memory_order_acquire); }

    // Get last result
    GPUPrimeResult lastResult() const { return m_lastResult; }

    // Wait for async warmup to complete
    bool waitForWarmup(uint32_t timeoutMs = 5000);

private:
    GPUPrimeEngine() = default;
    ~GPUPrimeEngine() = default;
    GPUPrimeEngine(const GPUPrimeEngine&) = delete;
    GPUPrimeEngine& operator=(const GPUPrimeEngine&) = delete;

    // Backend-specific warmup implementations
    GPUPrimeResult warmupDX12(void* device, void* queue, const GPUPrimeConfig& config);
    GPUPrimeResult warmupVulkan(void* device, void* queue, const GPUPrimeConfig& config);
    GPUPrimeResult warmupOpenCL(void* device, void* queue, const GPUPrimeConfig& config);
    GPUPrimeResult warmupGeneric(void* device, void* queue, const GPUPrimeConfig& config);

    // Common warmup kernels
    uint32_t runMatmulWarmup(void* device, void* queue, uint32_t dim);
    uint32_t runMemcpyWarmup(void* device, void* queue, uint32_t sizeBytes);
    uint32_t runShaderCacheWarmup(void* device, void* queue);

    std::atomic<bool> m_warmedUp{false};
    std::atomic<bool> m_warmingUp{false};
    GPUPrimeResult m_lastResult;
    std::thread m_warmupThread;
};

// ============================================================================
// C API for integration
// ============================================================================
extern "C" {
    // Returns true if GPU has been primed and is ready for inference
    __declspec(dllexport) bool __stdcall RawrXD_GPU_IsPrimed();

    // Trigger async warmup (safe to call multiple times)
    __declspec(dllexport) void __stdcall RawrXD_GPU_TriggerWarmup(void* device, void* queue);

    // Get last warmup result as JSON string (caller must not free)
    __declspec(dllexport) const char* __stdcall RawrXD_GPU_GetWarmupReport();
}

} // namespace GPU
} // namespace RawrXD
