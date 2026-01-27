// bench_flash_asm_puppeteer.cpp — ASM Flash-Attention Performance Gate
// Phase 4 Final Form: Validate ≥10× vs FP32 baseline OR ≥1.2× vs C+intrinsics

#include <cstdint>
#include <cstdio>
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>
#include <cmath>
#include <cstring>

// BlockQ8_0 structure: scale (float) + 32×int8
struct BlockQ8_0 {
    float scale;
    int8_t qs[32];
};

// External kernels
extern "C" void flash_attn_forward(
    const float* Q, const float* K, const float* V, float* O,
    int seq_len, int head_dim, bool force_scalar
);

// ASM kernel from flash_attn_asm_avx2.asm (NASM)
extern "C" void flash_attn_asm_avx2(
    const float* Q, const void* K, const float* V, float* O,
    int seqLen, int headDim, int quantType
);

// Baseline FP32 attention for reference
static void standard_attention(
    const float* Q, const float* K, const float* V, float* O,
    int seq_len, int head_dim, float scale
) {
    std::vector<float> QK(seq_len * seq_len);
    
    // Q * K^T
    for (int i = 0; i < seq_len; ++i) {
        for (int j = 0; j < seq_len; ++j) {
            float sum = 0.0f;
            for (int d = 0; d < head_dim; ++d) {
                sum += Q[i * head_dim + d] * K[j * head_dim + d];
            }
            QK[i * seq_len + j] = sum * scale;
        }
    }
    
    // Softmax
    for (int i = 0; i < seq_len; ++i) {
        float max_val = -INFINITY;
        for (int j = 0; j < seq_len; ++j) {
            max_val = std::max(max_val, QK[i * seq_len + j]);
        }
        float sum_exp = 0.0f;
        for (int j = 0; j < seq_len; ++j) {
            QK[i * seq_len + j] = std::exp(QK[i * seq_len + j] - max_val);
            sum_exp += QK[i * seq_len + j];
        }
        for (int j = 0; j < seq_len; ++j) {
            QK[i * seq_len + j] /= sum_exp;
        }
    }
    
    // P * V
    for (int i = 0; i < seq_len; ++i) {
        for (int d = 0; d < head_dim; ++d) {
            float sum = 0.0f;
            for (int j = 0; j < seq_len; ++j) {
                sum += QK[i * seq_len + j] * V[j * head_dim + d];
            }
            O[i * head_dim + d] = sum;
        }
    }
}

// Quantize FP32 K matrix to Q8_0 format
static void quantize_q8_0(const float* K, BlockQ8_0* Kq8, int seq_len, int head_dim) {
    constexpr int BLOCK_SIZE = 32;
    int num_blocks = (seq_len * head_dim) / BLOCK_SIZE;
    
    for (int b = 0; b < num_blocks; ++b) {
        const float* src = K + b * BLOCK_SIZE;
        
        // Find max absolute value for this block
        float max_abs = 0.0f;
        for (int i = 0; i < BLOCK_SIZE; ++i) {
            max_abs = std::max(max_abs, std::abs(src[i]));
        }
        
        // Compute scale
        float scale = max_abs / 127.0f;
        Kq8[b].scale = scale;
        
        // Quantize to int8
        float inv_scale = (scale != 0.0f) ? (1.0f / scale) : 0.0f;
        for (int i = 0; i < BLOCK_SIZE; ++i) {
            float val = src[i] * inv_scale;
            Kq8[b].qs[i] = static_cast<int8_t>(std::round(std::clamp(val, -127.0f, 127.0f)));
        }
    }
}

int main() {
    const int seq_len = 4096;
    const int head_dim = 64;
    const float scale = 1.0f / std::sqrt(static_cast<float>(head_dim));
    
    // Allocate tensors
    std::vector<float> Q(seq_len * head_dim);
    std::vector<float> K_fp32(seq_len * head_dim);
    std::vector<BlockQ8_0> K_q8((seq_len * head_dim) / 32);
    std::vector<float> V(seq_len * head_dim);
    std::vector<float> O_baseline(seq_len * head_dim);
    std::vector<float> O_intrinsics(seq_len * head_dim);
    std::vector<float> O_asm(seq_len * head_dim);
    
    // Initialize with random data
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    
    for (auto& v : Q) v = dist(rng);
    for (auto& v : K_fp32) v = dist(rng);
    for (auto& v : V) v = dist(rng);
    
    // Quantize K to Q8_0
    quantize_q8_0(K_fp32.data(), K_q8.data(), seq_len, head_dim);
    
    std::puts("=================================================================");
    std::puts("Flash-Attention ASM Puppeteer Benchmark (Phase 4 Final Form)");
    std::puts("=================================================================");
    std::printf("Configuration: seq_len=%d, head_dim=%d\n", seq_len, head_dim);
    std::printf("K-matrix format: Q8_0 (%zu blocks)\n", K_q8.size());
    std::puts("");
    
    // ─────────────────────────────────────────────────────────────────────────
    // Gate 1: Baseline FP32 attention (reference)
    // ─────────────────────────────────────────────────────────────────────────
    const int iters_baseline = 3;
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int it = 0; it < iters_baseline; ++it) {
        standard_attention(Q.data(), K_fp32.data(), V.data(), O_baseline.data(), 
                          seq_len, head_dim, scale);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms_baseline = std::chrono::duration<double, std::milli>(t1 - t0).count() / iters_baseline;
    
    std::printf("Baseline FP32: %.2f ms/iter\n", ms_baseline);
    
    // ─────────────────────────────────────────────────────────────────────────
    // Gate 2: C+intrinsics flash-attention (from flash_attn_avx2.cc)
    // ─────────────────────────────────────────────────────────────────────────
    const int iters_fast = 10;
    
    t0 = std::chrono::high_resolution_clock::now();
    for (int it = 0; it < iters_fast; ++it) {
        flash_attn_forward(Q.data(), K_fp32.data(), V.data(), O_intrinsics.data(),
                          seq_len, head_dim, false);
    }
    t1 = std::chrono::high_resolution_clock::now();
    double ms_intrinsics = std::chrono::duration<double, std::milli>(t1 - t0).count() / iters_fast;
    
    double speedup_intrinsics = ms_baseline / ms_intrinsics;
    std::printf("C+Intrinsics Flash: %.2f ms/iter (%.2fx vs baseline)\n", 
                ms_intrinsics, speedup_intrinsics);
    
    // ─────────────────────────────────────────────────────────────────────────
    // Gate 3: Hand-rolled ASM flash-attention with Q8_0 K-matrix
    // ─────────────────────────────────────────────────────────────────────────
    t0 = std::chrono::high_resolution_clock::now();
    for (int it = 0; it < iters_fast; ++it) {
        flash_attn_asm_avx2(Q.data(), K_q8.data(), V.data(), O_asm.data(),
                           seq_len, head_dim, 2);  // quantType=2 (Q8_0)
    }
    t1 = std::chrono::high_resolution_clock::now();
    double ms_asm = std::chrono::duration<double, std::milli>(t1 - t0).count() / iters_fast;
    
    double speedup_asm_vs_baseline = ms_baseline / ms_asm;
    double speedup_asm_vs_intrinsics = ms_intrinsics / ms_asm;
    
    std::puts("");
    std::puts("─────────────────────────────────────────────────────────────────");
    std::printf("Puppeteer-ASM: %.2f ms  Speedup: %.2fx vs baseline, %.2fx vs intrinsics\n",
                ms_asm, speedup_asm_vs_baseline, speedup_asm_vs_intrinsics);
    std::puts("─────────────────────────────────────────────────────────────────");
    std::puts("");
    
    // ─────────────────────────────────────────────────────────────────────────
    // Production gates
    // ─────────────────────────────────────────────────────────────────────────
    bool gate_10x = speedup_asm_vs_baseline >= 10.0;
    bool gate_bonus = speedup_asm_vs_intrinsics >= 1.2;
    
    if (gate_10x) {
        std::printf("✅ GATE PASS: ≥10× vs FP32 baseline (%.2fx)\n", speedup_asm_vs_baseline);
    } else {
        std::printf("⚠️  Gate miss: <10× vs baseline (%.2fx, target: ≥10×)\n", speedup_asm_vs_baseline);
    }
    
    if (gate_bonus) {
        std::printf("✅ BONUS PASS: ≥1.2× vs C+intrinsics (%.2fx)\n", speedup_asm_vs_intrinsics);
    } else {
        std::printf("⚠️  Bonus miss: <1.2× vs intrinsics (%.2fx, target: ≥1.2×)\n", speedup_asm_vs_intrinsics);
    }
    
    std::puts("");
    if (gate_10x || gate_bonus) {
        std::puts("🎯 Phase 4 COMPLETE — Ready to tag v0.7.0-flash-avx2-production");
        return 0;
    } else {
        std::puts("⚠️  Phase 4 gates not met — optimization needed");
        return 1;
    }
}
