#include <cstdint>
#include <cstdio>
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>
#include <cmath>

extern "C" void flash_attn_forward(
    const float* Q, const float* K, const float* V, float* O,
    int seq_len, int head_dim, bool force_scalar
);

// Baseline attention for comparison
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

int main() {
    const int seq_len = 4096;  // Long context (4K tokens)
    const int head_dim = 64;   // Typical attention head dimension
    const float scale = 1.0f / std::sqrt((float)head_dim);

    std::vector<float> Q(seq_len * head_dim);
    std::vector<float> K(seq_len * head_dim);
    std::vector<float> V(seq_len * head_dim);
    std::vector<float> O_ref(seq_len * head_dim);
    std::vector<float> O_flash(seq_len * head_dim);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    for (auto& v : Q) v = dist(rng);
    for (auto& v : K) v = dist(rng);
    for (auto& v : V) v = dist(rng);

    // Run baseline (only on smaller subset for speed)
    const int test_seq = 512;  // Test correctness on smaller sequence
    std::vector<float> Q_test(test_seq * head_dim);
    std::vector<float> K_test(test_seq * head_dim);
    std::vector<float> V_test(test_seq * head_dim);
    std::vector<float> O_test_ref(test_seq * head_dim);
    std::vector<float> O_test_flash(test_seq * head_dim);
    
    std::copy_n(Q.begin(), test_seq * head_dim, Q_test.begin());
    std::copy_n(K.begin(), test_seq * head_dim, K_test.begin());
    std::copy_n(V.begin(), test_seq * head_dim, V_test.begin());

    standard_attention(Q_test.data(), K_test.data(), V_test.data(), 
                      O_test_ref.data(), test_seq, head_dim, scale);
    flash_attn_forward(Q_test.data(), K_test.data(), V_test.data(), 
                       O_test_flash.data(), test_seq, head_dim, false);

    float max_abs = 0.0f;
    for (int i = 0; i < test_seq * head_dim; ++i) {
        max_abs = std::max(max_abs, std::abs(O_test_ref[i] - O_test_flash[i]));
    }
    std::printf("Max abs diff (seq=%d): %.6f\n", test_seq, max_abs);

    // Benchmark on full 4K context
    const int iters = 5;
    
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int it = 0; it < iters; ++it) {
        standard_attention(Q.data(), K.data(), V.data(), O_ref.data(), 
                          seq_len, head_dim, scale);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms_baseline = std::chrono::duration<double, std::milli>(t1 - t0).count();

    t0 = std::chrono::high_resolution_clock::now();
    for (int it = 0; it < iters; ++it) {
        flash_attn_forward(Q.data(), K.data(), V.data(), O_flash.data(), 
                           seq_len, head_dim, false);
    }
    t1 = std::chrono::high_resolution_clock::now();
    double ms_flash = std::chrono::duration<double, std::milli>(t1 - t0).count();

    double speedup = ms_baseline / ms_flash;
    std::printf("Baseline: %.2f ms  Flash(AVX2): %.2f ms  Speedup: %.2fx\n", 
                ms_baseline, ms_flash, speedup);

    if (speedup >= 10.0) {
        std::puts("✅ FLASH-ATTENTION: >= 10× speedup achieved");
        return 0;
    } else {
        std::printf("⚠️  FLASH-ATTENTION: %.2fx (target: >=10×)\n", speedup);
        return 1;
    }
}
