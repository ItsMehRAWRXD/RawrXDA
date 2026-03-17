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
    }
    return result;
}

// Helper: Compare float arrays
bool CompareFloats(const float* a, const float* b, int size, float epsilon = 1e-4f) {
    for (int i = 0; i < size; ++i) {
        if (std::fabs(a[i] - b[i]) > epsilon) {
            std::cout << "Mismatch at index " << i << ": " << a[i] << " vs " << b[i] << std::endl;
            return false;
        }
    }
    return true;
}

// Test: MatMul
bool TestMatMul() {
    std::cout << "Testing MatMul..." << std::endl;
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
            }
        }
    }

    // MASM implementation
    int result = GGML_BackendDispatch(GGML_OP_MATMUL, A.data(), B.data(), C.data(), N, N, N);
    assert(result == 0);

    bool pass = CompareFloats(C.data(), C_ref.data(), N * N);
    std::cout << "MatMul: " << (pass ? "PASS" : "FAIL") << std::endl;
    return pass;
}

// Test: Add
bool TestAdd() {
    std::cout << "Testing Add..." << std::endl;
    const int size = 128;
    auto A = RandomFloats(size);
    auto B = RandomFloats(size);
    std::vector<float> C(size, 0.0f);
    std::vector<float> C_ref(size);

    // Reference implementation
    for (int i = 0; i < size; ++i) {
        C_ref[i] = A[i] + B[i];
    }

    // MASM implementation
    int result = GGML_BackendDispatch(GGML_OP_ADD, A.data(), B.data(), C.data(), size, size, size);
    assert(result == 0);

    bool pass = CompareFloats(C.data(), C_ref.data(), size);
    std::cout << "Add: " << (pass ? "PASS" : "FAIL") << std::endl;
    return pass;
}

// Test: Mul
bool TestMul() {
    std::cout << "Testing Mul..." << std::endl;
    const int size = 128;
    auto A = RandomFloats(size);
    auto B = RandomFloats(size);
    std::vector<float> C(size, 0.0f);
    std::vector<float> C_ref(size);

    // Reference implementation
    for (int i = 0; i < size; ++i) {
        C_ref[i] = A[i] * B[i];
    }

    // MASM implementation
    int result = GGML_BackendDispatch(GGML_OP_MUL, A.data(), B.data(), C.data(), size, size, size);
    assert(result == 0);

    bool pass = CompareFloats(C.data(), C_ref.data(), size);
    std::cout << "Mul: " << (pass ? "PASS" : "FAIL") << std::endl;
    return pass;
}

// Test: Quantization Q8_0
bool TestQuantizeQ8_0() {
    std::cout << "Testing QuantizeQ8_0..." << std::endl;
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
    std::cout << "QuantizeQ8_0: " << (pass ? "PASS" : "FAIL") << std::endl;
    return pass;
}

// Fuzz test: Random operations
bool FuzzTest() {
    std::cout << "Running fuzz test..." << std::endl;
    for (int i = 0; i < 100; ++i) {
        const int size = 64 + (i % 192);
        auto A = RandomFloats(size);
        auto B = RandomFloats(size);
        std::vector<float> C(size);

        int op = i % 3; // Add, Mul, or Quantize
        int result = GGML_BackendDispatch(op, A.data(), B.data(), C.data(), size, size, size);
        if (result != 0) {
            std::cout << "Fuzz test FAILED at iteration " << i << std::endl;
            return false;
        }
    }
    std::cout << "Fuzz test: PASS" << std::endl;
    return true;
}

int main() {
    std::cout << "=== MASM Tensor Op Regression Tests ===" << std::endl;
    
    bool all_pass = true;
    all_pass &= TestMatMul();
    all_pass &= TestAdd();
    all_pass &= TestMul();
    all_pass &= TestQuantizeQ8_0();
    all_pass &= FuzzTest();

    std::cout << std::endl;
    if (all_pass) {
        std::cout << "ALL TESTS PASSED" << std::endl;
        return 0;
    } else {
        std::cout << "SOME TESTS FAILED" << std::endl;
        return 1;
    }
}
