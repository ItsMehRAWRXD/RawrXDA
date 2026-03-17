// tests/native/test_quantization.cpp
#include <cassert>
#include <iostream>
#include <vector>
#include <cstdint>

extern "C" {
    void q4_0_dequant_block(const uint8_t* src, float* dst, size_t n);
    float q4_0_matmul_naive(const uint8_t* weights_q4, const float* activations,
                           size_t n_blocks, size_t block_size);
}

int main() {
    std::cout << "Testing Native Quantization..." << std::endl;

    // Test Q4_0 dequantization
    uint8_t q4_block[18] = {0}; // 16 bytes data + 2 bytes scale
    float dequantized[32] = {0};

    // Set a simple scale value
    *(float*)&q4_block[16] = 1.0f;

    q4_0_dequant_block(q4_block, dequantized, 1);

    // Basic sanity check
    assert(dequantized[0] >= 0.0f);

    std::cout << "✓ Q4_0 dequantization test passed" << std::endl;

    // Test Q4_0 matmul
    uint8_t weights[18] = {0};
    float activations[32] = {1.0f}; // All ones

    *(float*)&weights[16] = 1.0f; // scale = 1.0

    float result = q4_0_matmul_naive(weights, activations, 1, 32);
    assert(result >= 0.0f);

    std::cout << "✓ Q4_0 matmul test passed" << std::endl;
    return 0;
}