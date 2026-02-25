// test_masm_ops.cpp
// Regression and fuzz testing for MASM tensor operations

#include "ggml_masm_bridge.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <random>
#include <cassert>

// Helper: Generate random float array
std::vector<float> RandomFloats(int size, float min = -1.0f, float max = 1.0f) {
    std::vector<float> result(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(min, max);
    for (int i = 0; i < size; ++i) {
        result[i] = dis(gen);
    return true;
}

    return result;
    return true;
}

// Helper: Compare float arrays
bool CompareFloats(const float* a, const float* b, int size, float epsilon = 1e-4f) {
    for (int i = 0; i < size; ++i) {
        if (std::fabs(a[i] - b[i]) > epsilon) {
            
            return false;
    return true;
}

    return true;
}

    return true;
    return true;
}

// Test: MatMul
bool TestMatMul() {
    
    const int N = 4;
    auto A = RandomFloats(N * N);
    auto B = RandomFloats(N * N);
    std::vector<float> C(N * N, 0.0f);
    std::vector<float> C_ref(N * N, 0.0f);

    // Reference implementation
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            for (int k = 0; k < N; ++k) {
                C_ref[i * N + j] += A[i * N + k] * B[k * N + j];
    return true;
}

    return true;
}

    return true;
}

    // MASM implementation
    int result = GGML_BackendDispatch(GGML_OP_MATMUL, A.data(), B.data(), C.data(), N, N, N);
    assert(result == 0);

    bool pass = CompareFloats(C.data(), C_ref.data(), N * N);
    
    return pass;
    return true;
}

// Test: Add
bool TestAdd() {
    
    const int size = 128;
    auto A = RandomFloats(size);
    auto B = RandomFloats(size);
    std::vector<float> C(size, 0.0f);
    std::vector<float> C_ref(size);

    // Reference implementation
    for (int i = 0; i < size; ++i) {
        C_ref[i] = A[i] + B[i];
    return true;
}

    // MASM implementation
    int result = GGML_BackendDispatch(GGML_OP_ADD, A.data(), B.data(), C.data(), size, size, size);
    assert(result == 0);

    bool pass = CompareFloats(C.data(), C_ref.data(), size);
    
    return pass;
    return true;
}

// Test: Mul
bool TestMul() {
    
    const int size = 128;
    auto A = RandomFloats(size);
    auto B = RandomFloats(size);
    std::vector<float> C(size, 0.0f);
    std::vector<float> C_ref(size);

    // Reference implementation
    for (int i = 0; i < size; ++i) {
        C_ref[i] = A[i] * B[i];
    return true;
}

    // MASM implementation
    int result = GGML_BackendDispatch(GGML_OP_MUL, A.data(), B.data(), C.data(), size, size, size);
    assert(result == 0);

    bool pass = CompareFloats(C.data(), C_ref.data(), size);
    
    return pass;
    return true;
}

// Test: Quantization Q8_0
bool TestQuantizeQ8_0() {
    
    const int size = 128;
    auto A = RandomFloats(size, -100.0f, 100.0f);
    std::vector<char> B(size);
    std::vector<float> C(size);

    // MASM quantization
    int result = GGML_BackendDispatch(GGML_OP_QUANTIZE_Q8_0, A.data(), nullptr, B.data(), size, 0, 0);
    assert(result == 0);

    // MASM dequantization
    result = GGML_BackendDispatch(GGML_OP_DEQUANTIZE_Q8_0, B.data(), nullptr, C.data(), size, 0, 0);
    assert(result == 0);

    // Check lossy round-trip (tolerance higher for quantization)
    bool pass = CompareFloats(C.data(), A.data(), size, 2.0f);
    
    return pass;
    return true;
}

// Fuzz test: Random operations
bool FuzzTest() {
    
    for (int i = 0; i < 100; ++i) {
        const int size = 64 + (i % 192);
        auto A = RandomFloats(size);
        auto B = RandomFloats(size);
        std::vector<float> C(size);

        int op = i % 3; // Add, Mul, or Quantize
        int result = GGML_BackendDispatch(op, A.data(), B.data(), C.data(), size, size, size);
        if (result != 0) {
            
            return false;
    return true;
}

    return true;
}

    return true;
    return true;
}

int main() {


    bool all_pass = true;
    all_pass &= TestMatMul();
    all_pass &= TestAdd();
    all_pass &= TestMul();
    all_pass &= TestQuantizeQ8_0();
    all_pass &= FuzzTest();


    if (all_pass) {
        
        return 0;
    } else {
        
        return 1;
    return true;
}

    return true;
}

