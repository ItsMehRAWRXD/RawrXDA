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
    const bool ok = cpuEngine_.MatVecQ4(matrix, vector, output, rows, cols);
    std::lock_guard<std::mutex> lock(statsMutex_);
    if (ok) {
        stats_.cpuMatVecFallbacks++;
    } else {
        stats_.parityFailures++;
    }
    return ok;
}

} // namespace RawrXD
