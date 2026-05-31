#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <random>

// External MASM function
extern "C" float cosine_similarity_avx512(const float* a, const float* b, size_t n);

// Standard C++ implementation for baseline
float cosine_similarity_cpp(const float* a, const float* b, size_t n) {
    float dot = 0.0f;
    float sum_a2 = 0.0f;
    float sum_b2 = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        dot += a[i] * b[i];
        sum_a2 += a[i] * a[i];
        sum_b2 += b[i] * b[i];
    }
    float denom = std::sqrt(sum_a2 * sum_b2);
    return (denom == 0.0f) ? 0.0f : dot / denom;
}

int main() {
    const size_t VECTOR_SIZE = 1536; // OpenAI embedding size
    const int ITERATIONS = 100000;    // 100k comparisons

    std::cout << "--- RawrXD AVX-512 vs C++ Latency Audit ---" << std::endl;
    std::cout << "Vector Size: " << VECTOR_SIZE << "\nIterations: " << ITERATIONS << std::endl;

    // Data alignment for AVX-512 (64-byte boundary)
    alignas(64) std::vector<float> a(VECTOR_SIZE);
    alignas(64) std::vector<float> b(VECTOR_SIZE);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
    for (size_t i = 0; i < VECTOR_SIZE; ++i) {
        a[i] = dis(gen);
        b[i] = dis(gen);
    }

    // Warmup
    for (int i = 0; i < 1000; ++i) {
        cosine_similarity_cpp(a.data(), b.data(), VECTOR_SIZE);
        cosine_similarity_avx512(a.data(), b.data(), VECTOR_SIZE);
    }

    // Benchmark C++
    auto startCpu = std::chrono::high_resolution_clock::now();
    float resultCpu = 0.0f;
    for (int i = 0; i < ITERATIONS; ++i) {
        resultCpu += cosine_similarity_cpp(a.data(), b.data(), VECTOR_SIZE);
    }
    auto endCpu = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> cpuDur = endCpu - startCpu;

    // Benchmark AVX-512
    auto startAvx = std::chrono::high_resolution_clock::now();
    float resultAvx = 0.0f;
    for (int i = 0; i < ITERATIONS; ++i) {
        resultAvx += cosine_similarity_avx512(a.data(), b.data(), VECTOR_SIZE);
    }
    auto endAvx = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> avxDur = endAvx - startAvx;

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "\n[C++ Baseline]\n  Total Time: " << cpuDur.count() << " ms\n  Avg Latency: " << (cpuDur.count() * 1000.0 / ITERATIONS) << " us" << std::endl;
    std::cout << "\n[AVX-512 MASM]\n  Total Time: " << avxDur.count() << " ms\n  Avg Latency: " << (avxDur.count() * 1000.0 / ITERATIONS) << " us" << std::endl;

    double speedup = cpuDur.count() / avxDur.count();
    std::cout << "\n[RESULT] AVX-512 Speedup: " << speedup << "x" << std::endl;

    if (std::abs(resultCpu - resultAvx) > 0.1f) {
        std::cout << "[ERROR] Precision mismatch! Accuracy check failed." << std::endl;
        return 1;
    }

    std::cout << "[PASS] Accuracy check passed." << std::endl;
    return 0;
}
