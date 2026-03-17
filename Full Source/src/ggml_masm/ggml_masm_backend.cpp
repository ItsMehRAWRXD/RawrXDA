// ggml_masm_backend.cpp
// C++ backend integration for MASM tensor operations
// Bridges MASM routines with C++ GGML backend

#include "ggml_masm_bridge.h"
#include <cstring>
#include <stdexcept>
#include <vector>

// Dispatcher for tensor operations
int GGML_BackendDispatch(int op, void* A, void* B, void* C, int64_t M, int64_t N, int64_t K) {
    try {
        switch (op) {
            case 0: // GGML_OP_MUL_MAT
                ggml_masm_mul_mat((const float*)A, (const float*)B, (float*)C, M, N, K);
                return 0;
            case 1: // GGML_OP_ADD
                ggml_masm_add((const float*)A, (const float*)B, (float*)C, M * N);
                return 0;
            case 2: // GGML_OP_MUL
                ggml_masm_mul((const float*)A, (const float*)B, (float*)C, M * N);
                return 0;
            case 3: // GGML_OP_SOFTMAX
                ggml_masm_softmax((const float*)A, (float*)C, M * N);
                return 0;
            default:
                return -1;
        }
    } catch (...) {
        return -1;
    }
}

// Wrapper for quantization
void ggml_masm_quantize(GgmlQuantType type, const float* src, void* dst, int64_t n) {
    switch (type) {
        case GGML_TYPE_Q4_0:
            quantize_q4_0(src, dst, n);
            break;
        case GGML_TYPE_Q8_0:
            quantize_q8_0(src, dst, n);
            break;
        default:
            throw std::runtime_error("Unsupported quantization type");
    }
}

void ggml_masm_dequantize(GgmlQuantType type, const void* src, float* dst, int64_t n) {
    switch (type) {
        case GGML_TYPE_Q4_0:
            dequantize_q4_0(src, dst, n);
            break;
        case GGML_TYPE_Q8_0:
            dequantize_q8_0(src, dst, n);
            break;
        default:
            throw std::runtime_error("Unsupported dequantization type");
    }
}
