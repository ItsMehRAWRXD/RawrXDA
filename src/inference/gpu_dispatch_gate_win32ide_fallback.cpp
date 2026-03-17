#include "../../include/gpu_dispatch_gate.h"

namespace RawrXD {

GPUDispatchGate::GPUDispatchGate() = default;
GPUDispatchGate::~GPUDispatchGate() {
    (void)gpuBridge_.release();
}

bool GPUDispatchGate::Initialize() {
    return false;
}

bool GPUDispatchGate::MatVecQ4(const float* matrix, const float* vector, float* output,
                               uint32_t rows, uint32_t cols, bool enableParityCheck) {
    (void)matrix;
    (void)vector;
    (void)output;
    (void)rows;
    (void)cols;
    (void)enableParityCheck;
    return false;
}

} // namespace RawrXD
