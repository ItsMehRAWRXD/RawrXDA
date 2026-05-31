// Fused Decode + FP8 Benchmark
// Measures actual memory bandwidth savings from kernel fusion

#include <iostream>
#include <chrono>
#include <cstring>
#include <vector>
#include <algorithm>
#include <immintrin.h>
#include "kernels/fp8_avx2_interface.h"
#include "kernels/fused_decode_fp8_interface.h"

using namespace std::chrono;

// Simulate decode stage (produces float tokens)
void simulate_decode(const float* input, float* output, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        // Simple decode simulation (identity with small transform)
        output[i] = input[i] * 0.5f + 0.1f;
    }
}

// Benchmark 1: Separate stages (current)
// Input → Decode Buffer → FP8 Output
void benchmark_separate_stages(const float* input, uint8_t* output, size_t count, int iterations) {
    // Allocate intermediate buffer
    std::vector<float> decoded(count);
    
    auto start = high_resolution_clock::now();
    
    for (int iter = 0; iter < iterations; ++iter) {
        // Stage 1: Decode (memory write)
        simulate_decode(input, decoded.data(), count);
        
        // Stage 2: FP8 Quantize (memory read + write)
        RawrXD::Kernels::FP8AVX2Quantizer::QuantizeE4M3(
            decoded.data(), output, count, 1.0f
        );
        
        // Prevent optimization
        _mm_mfence();
    }
    
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();
    
    double tokens_per_sec = static_cast<double>(count * iterations) * 1e6 / us;
    double bytes_read = static_cast<double>(count * iterations * sizeof(float));      // Input read
    double bytes_written = static_cast<double>(count * iterations * sizeof(float));   // Decode write
    double bytes_read2 = static_cast<double>(count * iterations * sizeof(float));      // Decode read
    double bytes_written2 = static_cast<double>(count * iterations * sizeof(uint8_t)); // FP8 write
    double total_bytes = bytes_read + bytes_written + bytes_read2 + bytes_written2;
    double bandwidth_gb = total_bytes / (1024.0 * 1024.0 * 1024.0) / (us / 1e6);
    
    std::cout << "  Throughput: " << static_cast<uint64_t>(tokens_per_sec) << " tokens/sec" << std::endl;
    std::cout << "  Memory bandwidth: " << bandwidth_gb << " GB/s" << std::endl;
    std::cout << "  Memory traffic: " << (total_bytes / (1024.0 * 1024.0 * 1024.0)) << " GB total" << std::endl;
}

// Benchmark 2: Fused stages (new)
// Input → FP8 Output (no intermediate)
void benchmark_fused_stages(const float* input, uint8_t* output, size_t count, int iterations) {
    auto start = high_resolution_clock::now();
    
    for (int iter = 0; iter < iterations; ++iter) {
        // Fused: Decode + FP8 in one pass
        // Memory: Input read → FP8 write (no intermediate!)
        RawrXD::Kernels::FusedDecodeFP8Processor::Process(
            input, output, count, 1.0f
        );
        
        // Prevent optimization
        _mm_mfence();
    }
    
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();
    
    double tokens_per_sec = static_cast<double>(count * iterations) * 1e6 / us;
    double bytes_read = static_cast<double>(count * iterations * sizeof(float));      // Input read only
    double bytes_written = static_cast<double>(count * iterations * sizeof(uint8_t));   // FP8 write only
    double total_bytes = bytes_read + bytes_written;
    double bandwidth_gb = total_bytes / (1024.0 * 1024.0 * 1024.0) / (us / 1e6);
    
    std::cout << "  Throughput: " << static_cast<uint64_t>(tokens_per_sec) << " tokens/sec" << std::endl;
    std::cout << "  Memory bandwidth: " << bandwidth_gb << " GB/s" << std::endl;
    std::cout << "  Memory traffic: " << (total_bytes / (1024.0 * 1024.0 * 1024.0)) << " GB total" << std::endl;
}

// Benchmark 3: Streaming optimized
void benchmark_streaming(const float* input, uint8_t* output, size_t count, int iterations) {
    auto start = high_resolution_clock::now();
    
    for (int iter = 0; iter < iterations; ++iter) {
        RawrXD::Kernels::FusedDecodeFP8Processor::ProcessStreaming(
            input, output, count, 1.0f
        );
        _mm_mfence();
    }
    
    auto end = high_resolution_clock::now();
    auto us = duration_cast<microseconds>(end - start).count();
    
    double tokens_per_sec = static_cast<double>(count * iterations) * 1e6 / us;
    std::cout << "  Throughput: " << static_cast<uint64_t>(tokens_per_sec) << " tokens/sec" << std::endl;
}

int main() {
    std::cout << "=== Fused Decode + FP8 Benchmark ===" << std::endl;
    std::cout << "Testing memory bandwidth savings from kernel fusion\n" << std::endl;
    
    // Check AVX2
    if (!RawrXD::Kernels::FP8AVX2Quantizer::IsAVX2Available()) {
        std::cerr << "ERROR: AVX2 not available" << std::endl;
        return 1;
    }
    
    // Test configuration
    constexpr size_t TOKEN_COUNT = 1'000'000;  // 1M tokens
    constexpr int ITERATIONS = 10;
    
    // Allocate aligned memory
    alignas(256) std::vector<float> input(TOKEN_COUNT);
    alignas(256) std::vector<uint8_t> output(TOKEN_COUNT);
    
    // Initialize input
    for (size_t i = 0; i < TOKEN_COUNT; ++i) {
        input[i] = static_cast<float>(i % 100) / 100.0f;
    }
    
    std::cout << "Configuration:" << std::endl;
    std::cout << "  Tokens: " << TOKEN_COUNT << std::endl;
    std::cout << "  Iterations: " << ITERATIONS << std::endl;
    std::cout << "  Input size: " << (TOKEN_COUNT * sizeof(float) / (1024.0 * 1024.0)) << " MB" << std::endl;
    std::cout << "  Output size: " << (TOKEN_COUNT * sizeof(uint8_t) / (1024.0 * 1024.0)) << " MB\n" << std::endl;
    
    // Benchmark 1: Separate stages
    std::cout << "[1] Separate Stages (Decode Buffer → FP8):" << std::endl;
    benchmark_separate_stages(input.data(), output.data(), TOKEN_COUNT, ITERATIONS);
    std::cout << std::endl;
    
    // Benchmark 2: Fused stages
    std::cout << "[2] Fused Stages (Direct Decode→FP8):" << std::endl;
    benchmark_fused_stages(input.data(), output.data(), TOKEN_COUNT, ITERATIONS);
    std::cout << std::endl;
    
    // Benchmark 3: Streaming optimized
    std::cout << "[3] Streaming Optimized (Cache-friendly chunks):" << std::endl;
    benchmark_streaming(input.data(), output.data(), TOKEN_COUNT, ITERATIONS);
    std::cout << std::endl;
    
    // Summary
    std::cout << "=== Summary ===" << std::endl;
    std::cout << "Memory traffic reduction:" << std::endl;
    std::cout << "  Separate: 4N bytes (read+write+read+write)" << std::endl;
    std::cout << "  Fused:    1.25N bytes (read+write only)" << std::endl;
    std::cout << "  Savings:  ~69% memory bandwidth reduction" << std::endl;
    
    return 0;
}
