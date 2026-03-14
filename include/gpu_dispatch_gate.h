#pragma once

#include "gguf_d3d12_bridge.h"
#include "cpu_inference_engine.h"
#include <memory>
#include <vector>
#include <functional>

namespace RawrXD {

class GPUDispatchGate {
public:
    GPUDispatchGate();
    ~GPUDispatchGate();

    // Initialize with external D3D12 device/queue
    bool Initialize(ID3D12Device* device, ID3D12CommandQueue* queue);
    
    // Initialize with self-created D3D12 device/queue
    bool Initialize();

    // Shutdown and cleanup
    void Shutdown();

    // GPU-accelerated MatVec Q4_0 with CPU parity check
    bool MatVecQ4(const float* matrix, const float* vector, float* output,
                  uint32_t rows, uint32_t cols, bool enableParityCheck = true);

    // GPU-accelerated Softmax with CPU parity check
    bool Softmax(float* data, uint32_t size, bool enableParityCheck = true);

    // Statistics for performance monitoring
    struct Stats {
        uint64_t gpuMatVecCalls = 0;
        uint64_t cpuMatVecFallbacks = 0;
        uint64_t gpuSoftmaxCalls = 0;
        uint64_t cpuSoftmaxFallbacks = 0;
        uint64_t parityFailures = 0;
        double avgParityCheckTimeMs = 0.0;
    };

    const Stats& GetStats() const { return stats_; }

private:
    // GPU bridge for D3D12 operations
    std::unique_ptr<GGUFD3D12Bridge> gpuBridge_;

    // CPU fallback engine
    CPUInferenceEngine cpuEngine_;

    // Parity check configuration
    static constexpr float MAX_PARITY_ERR = 1e-3f; // Maximum allowed absolute error
    static constexpr double PARITY_TIMEOUT_MS = 100.0; // Timeout for parity checks

    // Internal helpers
    bool checkParity(const float* gpuResult, const float* cpuResult,
                     uint32_t size, const char* operationName);

    // Statistics
    Stats stats_;
    mutable std::mutex statsMutex_;
};

} // namespace RawrXD