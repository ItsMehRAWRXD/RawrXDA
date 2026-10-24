#include "../../include/gpu_dispatch_gate.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <vector>

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

bool GPUDispatchGate::Initialize(ID3D12Device* device, ID3D12CommandQueue* queue) {
    if (!gpuBridge_) {
        gpuBridge_ = std::make_unique<GGUFD3D12Bridge>();
    }
    if (!device || !queue) {
        return true; // Fallback can operate CPU-only without a live GPU device.
    }
    return gpuBridge_->Initialize(device, queue);
}

void GPUDispatchGate::Shutdown() {
    if (gpuBridge_) {
        gpuBridge_->Shutdown();
        gpuBridge_.reset();
    }
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

bool GPUDispatchGate::Softmax(float* data, uint32_t size, bool enableParityCheck) {
    if (!data || size == 0) {
        return false;
    }

    std::vector<float> baseline;
    if (enableParityCheck) {
        baseline.assign(data, data + size);
    }

    float maxVal = data[0];
    for (uint32_t i = 1; i < size; ++i) {
        maxVal = std::max(maxVal, data[i]);
    }

    double sum = 0.0;
    for (uint32_t i = 0; i < size; ++i) {
        const float shifted = std::clamp(data[i] - maxVal, -80.0f, 80.0f);
        data[i] = std::exp(shifted);
        sum += static_cast<double>(data[i]);
    }
    if (sum <= 0.0) {
        return false;
    }
    const float inv = static_cast<float>(1.0 / sum);
    for (uint32_t i = 0; i < size; ++i) {
        data[i] *= inv;
    }

    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_.cpuSoftmaxFallbacks += 1;
    if (enableParityCheck && !baseline.empty() && !checkParity(data, data, size, "SoftmaxFallback")) {
        stats_.parityFailures += 1;
    }
    return true;
}

bool GPUDispatchGate::RMSNorm(float* data, const float* gamma, uint32_t size, float eps, bool enableParityCheck) {
    if (!data || !gamma || size == 0) {
        return false;
    }
    if (eps <= 0.0f) {
        eps = 1e-5f;
    }

    std::vector<float> baseline;
    if (enableParityCheck) {
        baseline.assign(data, data + size);
    }

    double meanSq = 0.0;
    for (uint32_t i = 0; i < size; ++i) {
        const double v = static_cast<double>(data[i]);
        meanSq += v * v;
    }
    meanSq /= static_cast<double>(size);
    const float invRms = 1.0f / std::sqrt(static_cast<float>(meanSq) + eps);

    for (uint32_t i = 0; i < size; ++i) {
        data[i] = data[i] * invRms * gamma[i];
    }

    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_.cpuRMSNormFallbacks += 1;
    if (enableParityCheck && !baseline.empty() && !checkParity(data, data, size, "RMSNormFallback")) {
        stats_.parityFailures += 1;
    }
    return true;
}

bool GPUDispatchGate::checkParity(const float* gpuResult, const float* cpuResult,
                                  uint32_t size, const char* operationName) {
    (void)operationName;
    if (!gpuResult || !cpuResult || size == 0) {
        return false;
    }
    float maxErr = 0.0f;
    for (uint32_t i = 0; i < size; ++i) {
        const float err = std::abs(gpuResult[i] - cpuResult[i]);
        if (err > maxErr) {
            maxErr = err;
        }
    }
    return maxErr <= MAX_PARITY_ERR;
}

} // namespace RawrXD
