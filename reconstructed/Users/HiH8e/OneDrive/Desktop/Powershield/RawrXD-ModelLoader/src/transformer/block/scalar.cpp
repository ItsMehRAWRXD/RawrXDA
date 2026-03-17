#include <cmath>
#include <cstring>
#include <vector>
#include <iostream>

// ---------- SCALAR HELPERS (NO SIMD, NO PLACEHOLDERS) ----------

static float scalar_dot(const float* a, const float* b, size_t n) {
    float sum = 0.0f;
    for (size_t i = 0; i < n; ++i) sum += a[i] * b[i];   // explicit scalar FMA
    return sum;
}

static void scalar_softmax(float* x, size_t n) {
    float max_val = x[0];
    for (size_t i = 1; i < n; ++i) if (x[i] > max_val) max_val = x[i];
    float sum = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        x[i] = std::exp(x[i] - max_val);   // explicit scalar exp
        sum += x[i];
    }
    for (size_t i = 0; i < n; ++i) x[i] /= sum;          // explicit scalar div
}

static void scalar_matmul(const float* A, const float* B, float* C,
                          size_t M, size_t N, size_t K) {
    // C[M][N] = A[M][K] * B[K][N]   — row-major, scalar only
    for (size_t m = 0; m < M; ++m) {
        for (size_t n = 0; n < N; ++n) {
            float sum = 0.0f;
            for (size_t k = 0; k < K; ++k)
                sum += A[m * K + k] * B[k * N + n];   // explicit scalar mul-add
            C[m * N + n] = sum;
        }
    }
}

// ---------- SCALAR MULTI-HEAD ATTENTION (NO SIMD) ----------

void attention_forward_scalar(const float* Q, const float* K, const float* V,
                                float* out, size_t seq_len, size_t head_dim) {
    std::vector<float> scores(seq_len * seq_len);
    // QK^T
    scalar_matmul(Q, K, scores.data(), seq_len, seq_len, head_dim);
    // scale
    float scale = 1.0f / std::sqrt(float(head_dim));
    for (size_t i = 0; i < seq_len * seq_len; ++i) scores[i] *= scale;
    // softmax row-wise
    for (size_t i = 0; i < seq_len; ++i) scalar_softmax(&scores[i * seq_len], seq_len);
    // attention * V
    scalar_matmul(scores.data(), V, out, seq_len, head_dim, seq_len);
}

// ---------- SCALAR FEED-FORWARD (NO SIMD) ----------

void feed_forward_scalar(const float* x, const float* W1, const float* W2,
                         const float* b1, const float* b2,
                         float* out, size_t seq_len, size_t d_model, size_t d_ff) {
    std::vector<float> tmp1(seq_len * d_ff);
    std::vector<float> tmp2(seq_len * d_ff);

    // Linear 1
    scalar_matmul(x, W1, tmp1.data(), seq_len, d_ff, d_model);
    for (size_t i = 0; i < seq_len * d_ff; ++i) tmp1[i] += b1[i];

    // GELU activation (explicit scalar)
    for (size_t i = 0; i < seq_len * d_ff; ++i) {
        float val = tmp1[i];
        tmp2[i] = 0.5f * val * (1.0f + std::tanh(0.7978845608f * (val + 0.044715f * val * val * val)));
    }

    // Linear 2
    scalar_matmul(tmp2.data(), W2, out, seq_len, d_model, d_ff);
    for (size_t i = 0; i < seq_len * d_model; ++i) out[i] += b2[i];
}

// ---------- SCALAR LAYER-NORM (NO SIMD) ----------

void layer_norm_scalar(float* x, size_t n, float eps = 1e-5f) {
    float mean = 0.0f;
    for (size_t i = 0; i < n; ++i) mean += x[i];
    mean /= float(n);

    float var = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        float diff = x[i] - mean;
        var += diff * diff;
    }
    var /= float(n);
    float inv_std = 1.0f / std::sqrt(var + eps);

    for (size_t i = 0; i < n; ++i) x[i] = (x[i] - mean) * inv_std;
}

// ---------- SCALAR RMS NORM (LLaMA-style) ----------

void rms_norm_scalar(float* x, size_t n, float eps = 1e-6f) {
    float rms = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        float val = x[i];
        rms += val * val;  // explicit scalar multiply
    }
    rms = std::sqrt(rms / float(n) + eps);  // explicit sqrt
    
    for (size_t i = 0; i < n; ++i) {
        x[i] /= rms;  // explicit scalar divide
    }
}

// ---------- EXPORT FUNCTIONS ----------

extern "C" {
    void transformer_attention_scalar(const float* Q, const float* K, const float* V,
                                     float* out, size_t seq_len, size_t head_dim) {
        attention_forward_scalar(Q, K, V, out, seq_len, head_dim);
    }
    
    void transformer_ffn_scalar(const float* x, const float* W1, const float* W2,
                               const float* b1, const float* b2,
                               float* out, size_t seq_len, size_t d_model, size_t d_ff) {
        feed_forward_scalar(x, W1, W2, b1, b2, out, seq_len, d_model, d_ff);
    }
    
    void transformer_layer_norm(float* x, size_t n) {
        layer_norm_scalar(x, n);
    }
    
    void transformer_rms_norm(float* x, size_t n) {
        rms_norm_scalar(x, n);
    }
}
