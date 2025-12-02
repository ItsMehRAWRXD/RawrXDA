// bench_flash_all_quant.cpp — Flash-Attention All-Quant Benchmark
// Target: ≥10× speedup on 4K context with FP32 baseline vs Flash O(n) memory

#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <random>
#include <algorithm>

using namespace std::chrono;

extern "C" void flash_attention(const float* Q, const float* K, const float* V, float* O, int seqLen, int headDim);
extern "C" void attention_baseline(const float* Q, const float* K, const float* V, float* O, int seqLen, int headDim);

int main() {
    const int seqLen = 4096;
    const int headDim = 64;
    
    std::cout << "Flash-Attention All-Quant Benchmark\n";
    std::cout << "Shape: " << seqLen << " × " << headDim << " (4K context)\n\n";
    
    // Allocate buffers
    std::vector<float> Q(seqLen * headDim);
    std::vector<float> K(seqLen * headDim);
    std::vector<float> V(seqLen * headDim);
    std::vector<float> O_baseline(seqLen * headDim);
    std::vector<float> O_flash(seqLen * headDim);
    
    // Random init
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (auto& x : Q) x = dist(rng);
    for (auto& x : K) x = dist(rng);
    for (auto& x : V) x = dist(rng);
    
    // Warmup
    std::cout << "Warming up...\n";
    attention_baseline(Q.data(), K.data(), V.data(), O_baseline.data(), seqLen, headDim);
    flash_attention(Q.data(), K.data(), V.data(), O_flash.data(), seqLen, headDim);
    
    // Benchmark FP32 baseline (full materialization)
    std::cout << "Running FP32 baseline (O(n²) memory)...\n";
    auto t0 = high_resolution_clock::now();
    attention_baseline(Q.data(), K.data(), V.data(), O_baseline.data(), seqLen, headDim);
    auto t1 = high_resolution_clock::now();
    double ms_fp32 = duration<double, std::milli>(t1 - t0).count();
    
    // Benchmark Flash-Attention (tiled online-softmax)
    std::cout << "Running Flash-Attention (O(n) memory)...\n";
    t0 = high_resolution_clock::now();
    flash_attention(Q.data(), K.data(), V.data(), O_flash.data(), seqLen, headDim);
    t1 = high_resolution_clock::now();
    double ms_flash = duration<double, std::milli>(t1 - t0).count();
    
    // Verify correctness
    float maxDiff = 0.0f;
    for (size_t i = 0; i < O_baseline.size(); ++i) {
        maxDiff = std::max(maxDiff, std::abs(O_baseline[i] - O_flash[i]));
    }
    
    // Report
    double speedup = ms_fp32 / ms_flash;
    std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    std::cout << "Max abs diff: " << maxDiff << "\n";
    std::cout << "FP32 baseline: " << ms_fp32 << " ms\n";
    std::cout << "Flash (O(n)):  " << ms_flash << " ms\n";
    std::cout << "Speedup: " << speedup << "x\n";
    
    if (speedup >= 10.0) {
        std::cout << "✅ END-TO-END: >= 10× speedup achieved\n";
        return 0;
    } else {
        std::cout << "❌ END-TO-END: < 10× speedup (got " << speedup << "x)\n";
        return 1;
    }
}
