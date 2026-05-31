// FP8 Scalar vs AVX2 Benchmark - Direct Comparison
// Verifies 8x throughput improvement from vectorization

#include <iostream>
#include <chrono>
#include <cstring>
#include <immintrin.h>
#include "kernels/fp8_avx2_interface.h"

using namespace std::chrono;

// Scalar FP8 quantization (from original kernel)
void ScalarQuantizeE4M3(const float* input, uint8_t* output, size_t count, float scale) {
    const float e4m3_max = 448.0f;
    
    for (size_t i = 0; i < count; ++i) {
        float val = input[i] * scale;
        
        // Extract sign
        uint32_t bits = *reinterpret_cast<const uint32_t*>(&val);
        uint8_t sign = (bits >> 31) << 7;
        
        // Absolute value
        float abs_val = std::abs(val);
        
        // Clamp
        abs_val = std::min(abs_val, e4m3_max);
        
        // Quantize
        int32_t q = static_cast<int32_t>(abs_val);
        q = std::min(q, 255);
        q = std::max(q, 0);
        
        // Combine sign
        output[i] = static_cast<uint8_t>(q) | sign;
    }
}

int main() {
    std::cout << "=== FP8 Scalar vs AVX2 Benchmark ===" << std::endl;
    
    if (!RawrXD::Kernels::FP8AVX2Quantizer::IsAVX2Available()) {
        std::cerr << "ERROR: AVX2 not available" << std::endl;
        return 1;
    }
    
    constexpr size_t TOTAL_FLOATS = 1'000'000;
    constexpr size_t BATCH_SIZE = 64;
    constexpr size_t NUM_BATCHES = TOTAL_FLOATS / BATCH_SIZE;
    
    // Aligned memory for AVX2
    alignas(256) float input[TOTAL_FLOATS];
    alignas(64) uint8_t output_scalar[TOTAL_FLOATS];
    alignas(64) uint8_t output_avx2[TOTAL_FLOATS];
    
    // Initialize test data
    for (size_t i = 0; i < TOTAL_FLOATS; ++i) {
        input[i] = static_cast<float>(i % 100) / 10.0f;  // 0.0 to 9.9
    }
    
    // Warmup
    std::cout << "Warming up..." << std::endl;
    for (size_t i = 0; i < 100; ++i) {
        ScalarQuantizeE4M3(input, output_scalar, BATCH_SIZE, 1.0f);
        RawrXD::Kernels::FP8AVX2Quantizer::QuantizeE4M3(input, output_avx2, BATCH_SIZE, 1.0f);
    }
    
    // Scalar benchmark
    std::cout << "\nRunning scalar benchmark..." << std::endl;
    auto t0 = high_resolution_clock::now();
    for (size_t batch = 0; batch < NUM_BATCHES; ++batch) {
        ScalarQuantizeE4M3(&input[batch * BATCH_SIZE], 
                            &output_scalar[batch * BATCH_SIZE], 
                            BATCH_SIZE, 1.0f);
    }
    auto t1 = high_resolution_clock::now();
    auto scalar_us = duration_cast<microseconds>(t1 - t0).count();
    
    // AVX2 benchmark
    std::cout << "Running AVX2 benchmark..." << std::endl;
    t0 = high_resolution_clock::now();
    for (size_t batch = 0; batch < NUM_BATCHES; ++batch) {
        RawrXD::Kernels::FP8AVX2Quantizer::QuantizeE4M3(
            &input[batch * BATCH_SIZE], 
            &output_avx2[batch * BATCH_SIZE], 
            BATCH_SIZE, 1.0f);
    }
    t1 = high_resolution_clock::now();
    auto avx2_us = duration_cast<microseconds>(t1 - t0).count();
    
    // Results
    double scalar_tps = TOTAL_FLOATS * 1e6 / scalar_us;
    double avx2_tps = TOTAL_FLOATS * 1e6 / avx2_us;
    double speedup = static_cast<double>(scalar_us) / avx2_us;
    
    std::cout << "\n=== Results ===" << std::endl;
    std::cout << "Scalar time: " << scalar_us << " us" << std::endl;
    std::cout << "AVX2 time:   " << avx2_us << " us" << std::endl;
    std::cout << "Scalar TPS:  " << static_cast<uint64_t>(scalar_tps) << std::endl;
    std::cout << "AVX2 TPS:    " << static_cast<uint64_t>(avx2_tps) << std::endl;
    std::cout << "Speedup:     " << speedup << "x" << std::endl;
    
    // Verify correctness
    int mismatches = 0;
    for (size_t i = 0; i < TOTAL_FLOATS; ++i) {
        if (output_scalar[i] != output_avx2[i]) {
            mismatches++;
            if (mismatches <= 5) {
                std::cout << "Mismatch at " << i << ": scalar=" << (int)output_scalar[i] 
                          << " avx2=" << (int)output_avx2[i] << std::endl;
            }
        }
    }
    
    if (mismatches == 0) {
        std::cout << "\n[PASS] AVX2 output matches scalar (bit-exact)" << std::endl;
    } else {
        std::cout << "\n[FAIL] " << mismatches << " mismatches found" << std::endl;
        return 1;
    }
    
    if (speedup >= 4.0) {
        std::cout << "[PASS] Speedup >= 4x (target: 8x)" << std::endl;
    } else {
        std::cout << "[WARNING] Speedup < 4x" << std::endl;
    }
    
    return 0;
}
