#include "../../include/gpu_dispatch_gate.h"
#include <mutex>

namespace RawrXD {

GPUDispatchGate::GPUDispatchGate() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_ = {};
}
GPUDispatchGate::~GPUDispatchGate() {
    gpuBridge_.reset();
}

bool GPUDispatchGate::Initialize() {
    if (!gpuBridge_) {
        gpuBridge_ = std::make_unique<GGUFD3D12Bridge>();
    }
    return true;
}

bool GPUDispatchGate::MatVecQ4(const float* matrix, const float* vector, float* output,
                               uint32_t rows, uint32_t cols, bool enableParityCheck) {
    (void)enableParityCheck;
    if (matrix == nullptr || vector == nullptr || output == nullptr || rows == 0 || cols == 0) {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.parityFailures++;
        return false;
    }
    for (uint32_t r = 0; r < rows; ++r) {
        float acc = 0.0f;
        const float* rowPtr = matrix + static_cast<size_t>(r) * cols;
        for (uint32_t c = 0; c < cols; ++c) {
            acc += rowPtr[c] * vector[c];
        }
        output[r] = acc;
    }
    const bool ok = true;
    std::lock_guard<std::mutex> lock(statsMutex_);
    if (ok) {
        stats_.cpuMatVecFallbacks++;
    } else {
        stats_.parityFailures++;
    }
    return ok;
}

} // namespace RawrXD
