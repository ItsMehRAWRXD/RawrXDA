#include "../../include/gpu_dispatch_gate.h"

namespace RawrXD {

GPUDispatchGate::GPUDispatchGate() = default;
GPUDispatchGate::~GPUDispatchGate() {
    (void)gpuBridge_.release();
}

bool GPUDispatchGate::Initialize() {
    // Fallback lane: run with CPU execution path only.
    gpuBridge_.reset();
    return true;
}

bool GPUDispatchGate::MatVecQ4(const float* matrix, const float* vector, float* output,
                               uint32_t rows, uint32_t cols, bool enableParityCheck) {
    (void)enableParityCheck;
    if (!matrix || !vector || !output || rows == 0 || cols == 0) {
        return false;
    }

    if (cpuEngine_.MatVecQ4(matrix, vector, output, rows, cols)) {
        return true;
    }

    // Deterministic scalar fallback if the CPU engine rejects the request.
    for (uint32_t r = 0; r < rows; ++r) {
        float sum = 0.0f;
        const uint32_t rowOffset = r * cols;
        for (uint32_t c = 0; c < cols; ++c) {
            sum += matrix[rowOffset + c] * vector[c];
        }
        output[r] = sum;
    }
    return true;
}

} // namespace RawrXD
