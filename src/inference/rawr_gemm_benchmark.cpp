/*
====================================================================
 RAWR GEMM BENCHMARK & VALIDATION HARNESS
====================================================================
 
 Validates:
 1. Numerical correctness vs reference scalar implementation
 2. Throughput benchmark (GFLOPS)
 3. Memory behavior (page faults, working set)
 
 Compile:
   MSVC: cl /std:c++17 /O2 /arch:AVX512 /Fe:gemm_bench.exe rawr_gemm_benchmark.cpp
   GCC:  g++ -std=c++17 -O3 -mavx512f -mavx512vl -mfma -o gemm_bench rawr_gemm_benchmark.cpp
 
====================================================================
*/

#include "rawr_gemm_avx512.h"
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

using namespace rawrxd::gemm;

// =================== REFERENCE SCALAR GEMM ====================
void reference_gemm(
    const float* A, int lda,
    const float* B, int ldb,
    float* C, int ldc,
    int M, int N, int K
) {
    for (int m = 0; m < M; m++) {
        for (int n = 0; n < N; n++) {
            float sum = 0.0f;
            for (int k = 0; k < K; k++) {
                sum += A[m * lda + k] * B[k * ldb + n];
            }
            C[m * ldc + n] = sum;
        }
    }
}

// =================== NUMERICAL CORRECTNESS TEST ====================
bool test_correctness(int M, int N, int K, float tolerance = 1e-4f) {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    
    std::vector<float> A(M * K);
    std::vector<float> B(K * N);
    std::vector<float> C_avx(M * N, 0.0f);
    std::vector<float> C_ref(M * N, 0.0f);
    
    for (auto& v : A) v = dist(rng);
    for (auto& v : B) v = dist(rng);
    
    // Reference implementation
    reference_gemm(A.data(), K, B.data(), N, C_ref.data(), N, M, N, K);
    
    // AVX-512 implementation
    blocked_gemm(A.data(), K, B.data(), N, C_avx.data(), N, M, N, K);
    
    // Compare results
    float max_error = 0.0f;
    float max_rel_error = 0.0f;
    int error_count = 0;
    
    for (int i = 0; i < M * N; i++) {
        float abs_error = std::abs(C_avx[i] - C_ref[i]);
        float rel_error = (C_ref[i] != 0.0f) ? 
            abs_error / std::abs(C_ref[i]) : abs_error;
        
        if (abs_error > tolerance && rel_error > tolerance) {
            error_count++;
            max_error = std::max(max_error, abs_error);
            max_rel_error = std::max(max_rel_error, rel_error);
        }
    }
    
    if (error_count > 0) {
        std::cout << "CORRECTNESS FAIL: " << M << "x" << N << "x" << K << std::endl;
        std::cout << "  Errors: " << error_count << "/" << (M * N) << std::endl;
        std::cout << "  Max abs error: " << max_error << std::endl;
        std::cout << "  Max rel error: " << max_rel_error << std::endl;
        return false;
    }
    
    std::cout << "CORRECTNESS PASS: " << M << "x" << N << "x" << K 
              << " (max error: " << max_error << ")" << std::endl;
    return true;
}

// =================== THROUGHPUT BENCHMARK ====================
double benchmark_gemm_impl(
    const std::string& name,
    int M, int N, int K,
    int iterations = 100,
    bool use_avx = true
) {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    
    std::vector<float> A(M * K);
    std::vector<float> B(K * N);
    std::vector<float> C(M * N, 0.0f);
    
    for (auto& v : A) v = dist(rng);
    for (auto& v : B) v = dist(rng);
    
    // Warmup
    for (int i = 0; i < 5; i++) {
        if (use_avx) {
            blocked_gemm(A.data(), K, B.data(), N, C.data(), N, M, N, K);
        } else {
            reference_gemm(A.data(), K, B.data(), N, C.data(), N, M, N, K);
        }
    }
    
    // Benchmark
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; i++) {
        if (use_avx) {
            blocked_gemm(A.data(), K, B.data(), N, C.data(), N, M, N, K);
        } else {
            reference_gemm(A.data(), K, B.data(), N, C.data(), N, M, N, K);
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    double seconds = std::chrono::duration<double>(end - start).count();
    
    // GFLOPS = 2 * M * N * K / time / 1e9
    double flops = 2.0 * M * N * K * iterations;
    double gflops = flops / seconds / 1e9;
    
    std::cout << std::setw(25) << std::left << name << ": ";
    std::cout << std::setw(8) << std::fixed << std::setprecision(1) << gflops << " GFLOPS";
    std::cout << " (" << std::setprecision(3) << seconds << "s for " << iterations << " iters)" << std::endl;
    
    return gflops;
}

// =================== LLM INFERENCE SIMULATION ====================
void benchmark_llm_inference() {
    std::cout << "\n=== LLM Inference Simulation ===" << std::endl;
    
    // Typical LLM dimensions
    struct LayerSize {
        int M, N, K;
        std::string name;
    };
    
    std::vector<LayerSize> layers = {
        // QKV projections (hidden -> 3 * hidden)
        {1, 1536, 4096, "QKV (7B hidden)"},
        {1, 3072, 4096, "QKV (7B large)"},
        
        // Output projection (hidden -> hidden)
        {1, 4096, 4096, "Attn Output (7B)"},
        
        // FFN layers (SwiGLU: gate + up + down)
        {1, 11008, 4096, "FFN Gate (7B)"},
        {1, 11008, 4096, "FFN Up (7B)"},
        {1, 4096, 11008, "FFN Down (7B)"},
        
        // Output layer (vocab)
        {1, 32000, 4096, "Output (7B vocab)"},
        {1, 128000, 4096, "Output (large vocab)"},
        
        // Larger model sizes
        {1, 6144, 8192, "QKV (13B)"},
        {1, 22016, 8192, "FFN Gate (13B)"},
        {1, 8192, 22016, "FFN Down (13B)"},
    };
    
    std::cout << "\n--- AVX-512 Blocked GEMM ---" << std::endl;
    double total_gflops = 0;
    int count = 0;
    for (const auto& layer : layers) {
        double gflops = benchmark_gemm_impl(layer.name, layer.M, layer.N, layer.K, 100, true);
        total_gflops += gflops;
        count++;
    }
    std::cout << "Average: " << std::fixed << std::setprecision(1) 
              << (total_gflops / count) << " GFLOPS" << std::endl;
    
    // Compare with scalar
    std::cout << "\n--- Scalar Reference (for comparison) ---" << std::endl;
    for (const auto& layer : layers) {
        benchmark_gemm_impl(layer.name + " (scalar)", layer.M, layer.N, layer.K, 10, false);
    }
}

// =================== MEMORY BEHAVIOR TEST ====================
void test_memory_behavior() {
    std::cout << "\n=== Memory Behavior Test ===" << std::endl;
    
    const auto& features = get_cpu_features();
    std::cout << "CPU Features:" << std::endl;
    std::cout << "  AVX-512F:  " << (features.has_avx512f ? "YES" : "NO") << std::endl;
    std::cout << "  AVX-512VL: " << (features.has_avx512vl ? "YES" : "NO") << std::endl;
    std::cout << "  AVX-512BW: " << (features.has_avx512bw ? "YES" : "NO") << std::endl;
    std::cout << "  FMA:       " << (features.has_fma ? "YES" : "NO") << std::endl;
    std::cout << "  Cores:     " << features.num_cores << std::endl;
    std::cout << "  L1 Cache:  " << features.l1_cache_kb << " KB" << std::endl;
    std::cout << "  L2 Cache:  " << features.l2_cache_kb << " KB" << std::endl;
    
    // Test cache-friendly blocking
    std::cout << "\n--- Cache Blocking Efficiency ---" << std::endl;
    
    // Small: fits in L1
    benchmark_gemm_impl("L1-fit (64x64x64)", 64, 64, 64, 1000);
    
    // Medium: fits in L2
    benchmark_gemm_impl("L2-fit (256x256x256)", 256, 256, 256, 100);
    
    // Large: exceeds L2
    benchmark_gemm_impl("L3-fit (1024x1024x1024)", 1024, 1024, 1024, 10);
    
    // Very large: exceeds L3
    benchmark_gemm_impl("RAM (4096x4096x4096)", 4096, 4096, 4096, 3);
}

// =================== VEC-MAT MUL BENCHMARK ====================
void benchmark_vec_mat_mul() {
    std::cout << "\n=== Vector-Matrix Multiply (LLM Output Layer) ===" << std::endl;
    
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    
    // Typical LLM output layer: hidden_dim x vocab_size
    std::vector<std::pair<int, int>> sizes = {
        {4096, 32000},   // Llama 7B
        {4096, 128000},  // Large vocab
        {8192, 128000},  // Llama 13B large
        {5120, 128256},  // GPT-4 class
    };
    
    for (const auto& [K, N] : sizes) {
        std::vector<float> x(K);
        std::vector<float> W(N * K);
        std::vector<float> out(N);
        
        for (auto& v : x) v = dist(rng);
        for (auto& v : W) v = dist(rng);
        
        int iterations = 100;
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; i++) {
            vec_mat_mul(x.data(), W.data(), out.data(), N, K);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        double seconds = std::chrono::duration<double>(end - start).count();
        double gflops = 2.0 * N * K * iterations / seconds / 1e9;
        
        std::cout << "vec_mat_mul " << K << "x" << N << ": "
                  << std::fixed << std::setprecision(1) << gflops << " GFLOPS" << std::endl;
    }
}

// =================== MAIN ====================
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " RAWR GEMM AVX-512 VALIDATION HARNESS  " << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 1. Numerical correctness
    std::cout << "\n=== Test 1: Numerical Correctness ===" << std::endl;
    bool all_correct = true;
    all_correct &= test_correctness(16, 16, 16);
    all_correct &= test_correctness(64, 64, 64);
    all_correct &= test_correctness(128, 128, 128);
    all_correct &= test_correctness(256, 256, 256);
    all_correct &= test_correctness(512, 512, 512);
    all_correct &= test_correctness(16, 64, 512);   // Non-square
    all_correct &= test_correctness(1, 32000, 4096); // LLM output layer
    
    if (all_correct) {
        std::cout << "\n✓ All correctness tests PASSED" << std::endl;
    } else {
        std::cout << "\n✗ Some correctness tests FAILED" << std::endl;
        return 1;
    }
    
    // 2. Throughput benchmark
    std::cout << "\n=== Test 2: Throughput Benchmark ===" << std::endl;
    benchmark_llm_inference();
    
    // 3. Memory behavior
    std::cout << "\n=== Test 3: Memory Behavior ===" << std::endl;
    test_memory_behavior();
    
    // 4. Vec-mat benchmark
    benchmark_vec_mat_mul();
    
    // Summary
    std::cout << "\n========================================" << std::endl;
    std::cout << " VALIDATION COMPLETE" << std::endl;
    std::cout << "========================================" << std::endl;
    
    print_benchmark();
    
    return 0;
}