#include <iostream>
#include <vector>
#include <chrono>
#include <immintrin.h>
#include <random>
#include <iomanip>

// Simple C++ baseline for dot product
float cpp_dot_product(const float* a, const float* b, size_t n) {
    float sum = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

// AVX-512 intrinsic version (mirroring the ASM kernel logic)
float avx512_dot_product(const float* a, const float* b, size_t n) {
    __m512 vsum = _mm512_setzero_ps();
    for (size_t i = 0; i < n; i += 16) {
        __m512 va = _mm512_loadu_ps(&a[i]);
        __m512 vb = _mm512_loadu_ps(&b[i]);
        vsum = _mm512_fmadd_ps(va, vb, vsum);
    }
    return _mm512_reduce_add_ps(vsum);
}

int main() {
    const size_t dim = 1024;
    const size_t iterations = 1000000;
    
    std::vector<float> vecA(dim);
    std::vector<float> vecB(dim);
    
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (size_t i = 0; i < dim; ++i) {
        vecA[i] = dist(gen);
        vecB[i] = dist(gen);
    }

    std::cout << "Benchmarking Vector Similarity Search (Dim=" << dim << ", Iterations=" << iterations << ")..." << std::endl;

    // Warm up
    cpp_dot_product(vecA.data(), vecB.data(), dim);

    auto start_cpp = std::chrono::high_resolution_clock::now();
    float result_cpp = 0;
    for (size_t i = 0; i < iterations; ++i) {
        result_cpp += cpp_dot_product(vecA.data(), vecB.data(), dim);
    }
    auto end_cpp = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff_cpp = end_cpp - start_cpp;

    auto start_avx = std::chrono::high_resolution_clock::now();
    float result_avx = 0;
    for (size_t i = 0; i < iterations; ++i) {
        result_avx += avx512_dot_product(vecA.data(), vecB.data(), dim);
    }
    auto end_avx = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff_avx = end_avx - start_avx;

    std::cout << "C++ Baseline: " << std::fixed << std::setprecision(4) << diff_cpp.count() << "s" << std::endl;
    std::cout << "AVX-512 Boost: " << std::fixed << std::setprecision(4) << diff_avx.count() << "s" << std::endl;
    std::cout << "Speedup: " << diff_cpp.count() / diff_avx.count() << "x" << std::endl;

    // Ensure results match (approximately)
    if (std::abs(result_cpp - result_avx) > 1.0f) {
        std::cout << "Warning: Result mismatch!" << std::endl;
    }

    return 0;
}
