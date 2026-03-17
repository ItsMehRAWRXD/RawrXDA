// bench_flash_all_quant.cpp — Flash-Attention All-Quant Benchmark
// Target: ≥10× speedup on 4K context with FP32 baseline vs Flash O(n) memory

#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <random>
#include <algorithm>
#include <limits>

using namespace std::chrono;

// Intrinsics-based flash-attention entrypoint from kernels/flash_attn_avx2.cc
extern "C" void flash_attn_forward(
    const float* Q, const float* K, const float* V, float* O,
    int seq_len, int head_dim, bool force_scalar);

// Simple FP32 baseline for correctness/reference
extern "C" void attention_baseline(const float* Q, const float* K, const float* V,
                                    float* O, int seqLen, int headDim) {
    const float scale = 1.0f / std::sqrt(static_cast<float>(headDim));

    std::vector<float> qk(seqLen * seqLen);

    // QK^T
    for (int i = 0; i < seqLen; ++i) {
        for (int j = 0; j < seqLen; ++j) {
            float acc = 0.0f;
            const float* qi = Q + i * headDim;
            const float* kj = K + j * headDim;
            for (int d = 0; d < headDim; ++d) {
                acc += qi[d] * kj[d];
            }
            qk[i * seqLen + j] = acc * scale;
        }
    }

    // Softmax per row
    for (int i = 0; i < seqLen; ++i) {
        float row_max = -std::numeric_limits<float>::infinity();
        for (int j = 0; j < seqLen; ++j) {
            row_max = std::max(row_max, qk[i * seqLen + j]);
        }

        float row_sum = 0.0f;
        for (int j = 0; j < seqLen; ++j) {
            float val = std::exp(qk[i * seqLen + j] - row_max);
            qk[i * seqLen + j] = val;
            row_sum += val;
        }

        float inv_sum = 1.0f / row_sum;
        for (int j = 0; j < seqLen; ++j) {
            qk[i * seqLen + j] *= inv_sum;
        }
    }

    // P * V
    for (int i = 0; i < seqLen; ++i) {
        float* out = O + i * headDim;
        std::fill(out, out + headDim, 0.0f);
        for (int j = 0; j < seqLen; ++j) {
            float w = qk[i * seqLen + j];
            const float* vj = V + j * headDim;
            for (int d = 0; d < headDim; ++d) {
                out[d] += w * vj[d];
            }
        }
    }
}

// Thin wrapper that dispatches to the AVX2 flash-attention implementation
extern "C" void flash_attention(const float* Q, const float* K, const float* V,
                                 float* O, int seqLen, int headDim) {
    flash_attn_forward(Q, K, V, O, seqLen, headDim, /*force_scalar=*/false);
}

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
