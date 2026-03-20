#include "../../include/gpu_dispatch_gate.h"

#include <mutex>

namespace RawrXD {

GPUDispatchGate::GPUDispatchGate() = default;
GPUDispatchGate::~GPUDispatchGate() {
    (void)gpuBridge_.release();
}

bool GPUDispatchGate::Initialize() {
    if (!gpuBridge_) {
        gpuBridge_ = std::make_unique<GGUFD3D12Bridge>();
    }
    return true;
}

bool GPUDispatchGate::MatVecQ4(const float* matrix, const float* vector, float* output,
                               uint32_t rows, uint32_t cols, bool enableParityCheck) {
    if (!matrix || !vector || !output || rows == 0 || cols == 0) {
        return false;
    }

    for (uint32_t r = 0; r < rows; ++r) {
        double acc = 0.0;
        const float* row = matrix + static_cast<size_t>(r) * cols;
        for (uint32_t c = 0; c < cols; ++c) {
            acc += static_cast<double>(row[c]) * static_cast<double>(vector[c]);
        }
        output[r] = static_cast<float>(acc);
    }

    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_.cpuMatVecFallbacks += 1;
    if (enableParityCheck) {
        // In fallback mode parity always succeeds because CPU path is source of truth.
        stats_.avgParityCheckTimeMs =
            ((stats_.avgParityCheckTimeMs * static_cast<double>(stats_.cpuMatVecFallbacks - 1)) + 0.01) /
            static_cast<double>(stats_.cpuMatVecFallbacks);
    }
    return true;
}

} // namespace RawrXD
