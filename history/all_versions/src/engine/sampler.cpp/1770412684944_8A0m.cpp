#include "sampler.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <cstring>
#include <immintrin.h>

// ============================================================================
// High-Performance Sampler — Zero-Alloc Hot Path
// ============================================================================
// Rules:
// - NO heap allocations in sample() — all buffers pre-allocated
// - Top-K via partial_sort (40 elements, not 32K)
// - Top-P on the K-subset only
// - PCG32 RNG (10-20x faster than mt19937)
// - AVX2 vectorized softmax inline
// ============================================================================

Sampler::Sampler() : rng(std::random_device{}()) {
    fast_rng.seed((uint64_t)std::random_device{}());
    last_tokens.reserve(1024);
}

void Sampler::ensureBuffers(int n_vocab) {
    if (n_vocab != last_vocab_size) {
        prob_buf.resize(n_vocab);
        sorted_buf.resize(n_vocab);
        last_vocab_size = n_vocab;
    }
}

// Sample from a probability distribution using PCG32
int Sampler::sampleFromProbs(float* probs, int n) {
    float r = fast_rng.nextf();
    float cumsum = 0.0f;
    for (int i = 0; i < n; i++) {
        cumsum += probs[i];
        if (cumsum >= r) return i;
    }
    return n - 1;  // Rounding safety
}

int Sampler::sample(float* logits, int n_vocab) {
    // Ensure pre-allocated buffers are sized (only reallocates on vocab change)
    ensureBuffers(n_vocab);
    
    float* probs = prob_buf.data();
    
    // ---- Step 1: Apply temperature (in-place on logits copy) ----
    if (temp != 1.0f && temp > 0.0f) {
        float inv_temp = 1.0f / temp;
        
        // AVX2 vectorized temperature scaling
        __m256 vinv_t = _mm256_set1_ps(inv_temp);
        int i = 0;
        for (; i + 7 < n_vocab; i += 8) {
            __m256 v = _mm256_loadu_ps(logits + i);
            _mm256_storeu_ps(probs + i, _mm256_mul_ps(v, vinv_t));
        }
        for (; i < n_vocab; i++) {
            probs[i] = logits[i] * inv_temp;
        }
    } else {
        std::memcpy(probs, logits, n_vocab * sizeof(float));
    }
    
    // ---- Step 2: Apply repeat penalty (sparse — only touches history) ----
    for (int t : last_tokens) {
        if (t >= 0 && t < n_vocab) {
            if (probs[t] > 0) probs[t] /= repeat_penalty;
            else probs[t] *= repeat_penalty;
        }
    }
    
    // ---- Step 3: AVX2-vectorized Softmax (stable: subtract max first) ----
    // Find max
    __m256 vmax = _mm256_set1_ps(-1e30f);
    int i = 0;
    for (; i + 7 < n_vocab; i += 8) {
        vmax = _mm256_max_ps(vmax, _mm256_loadu_ps(probs + i));
    }
    float max_logit;
    {
        alignas(32) float tmp[8];
        _mm256_store_ps(tmp, vmax);
        max_logit = tmp[0];
        for (int j = 1; j < 8; j++) if (tmp[j] > max_logit) max_logit = tmp[j];
    }
    for (; i < n_vocab; i++) if (probs[i] > max_logit) max_logit = probs[i];
    
    // exp(x - max) and sum
    __m256 vmax_bc = _mm256_set1_ps(max_logit);
    __m256 vsum = _mm256_setzero_ps();
    
    // Fast exp constants (Schraudolph-style polynomial)
    const __m256 exp_max_val = _mm256_set1_ps(88.3762626647949f);
    const __m256 exp_min_val = _mm256_set1_ps(-88.3762626647949f);
    const __m256 log2e = _mm256_set1_ps(1.4426950408889634f);
    const __m256 half_v = _mm256_set1_ps(0.5f);
    const __m256 ln2 = _mm256_set1_ps(0.6931471805599453f);
    const __m256 c2 = _mm256_set1_ps(0.24022650695910072f);
    const __m256 c3 = _mm256_set1_ps(0.05550410866482158f);
    const __m256 one_v = _mm256_set1_ps(1.0f);
    
    i = 0;
    for (; i + 7 < n_vocab; i += 8) {
        __m256 x = _mm256_sub_ps(_mm256_loadu_ps(probs + i), vmax_bc);
        // Inline fast_exp to avoid function call overhead in tight loop
        x = _mm256_min_ps(x, exp_max_val);
        x = _mm256_max_ps(x, exp_min_val);
        __m256 t = _mm256_fmadd_ps(x, log2e, half_v);
        __m256 t_floor = _mm256_floor_ps(t);
        __m256 f = _mm256_sub_ps(_mm256_sub_ps(t, t_floor), half_v);
        __m256i ni = _mm256_cvtps_epi32(t_floor);
        ni = _mm256_slli_epi32(_mm256_add_epi32(ni, _mm256_set1_epi32(127)), 23);
        __m256 pow2n = _mm256_castsi256_ps(ni);
        __m256 poly = _mm256_fmadd_ps(c3, f, c2);
        poly = _mm256_fmadd_ps(poly, f, ln2);
        poly = _mm256_fmadd_ps(poly, f, one_v);
        __m256 e = _mm256_mul_ps(pow2n, poly);
        
        _mm256_storeu_ps(probs + i, e);
        vsum = _mm256_add_ps(vsum, e);
    }
    float sum;
    {
        alignas(32) float tmp[8];
        _mm256_store_ps(tmp, vsum);
        sum = tmp[0]+tmp[1]+tmp[2]+tmp[3]+tmp[4]+tmp[5]+tmp[6]+tmp[7];
    }
    for (; i < n_vocab; i++) {
        probs[i] = std::exp(probs[i] - max_logit);
        sum += probs[i];
    }
    
    // Normalize
    float inv_sum = 1.0f / sum;
    __m256 vinv = _mm256_set1_ps(inv_sum);
    i = 0;
    for (; i + 7 < n_vocab; i += 8) {
        _mm256_storeu_ps(probs + i, _mm256_mul_ps(_mm256_loadu_ps(probs + i), vinv));
    }
    for (; i < n_vocab; i++) probs[i] *= inv_sum;
    
    // ---- Step 4: Top-K filtering via partial sort (only sort K elements) ----
    // Build index pairs into pre-allocated buffer
    auto* sorted = sorted_buf.data();
    for (int j = 0; j < n_vocab; j++) {
        sorted[j].first = probs[j];
        sorted[j].second = j;
    }
    
    int effective_k = (top_k > 0 && top_k < n_vocab) ? top_k : n_vocab;
    
    if (effective_k < n_vocab) {
        // partial_sort: O(n + k*log(k)) instead of O(n*log(n))
        std::partial_sort(sorted, sorted + effective_k, sorted + n_vocab,
            [](const auto& a, const auto& b) { return a.first > b.first; });
        
        // Zero out everything below top-K threshold
        float kth = sorted[effective_k - 1].first;
        for (int j = 0; j < n_vocab; j++) {
            if (probs[j] < kth) probs[j] = 0.0f;
        }
        
        // Renormalize
        sum = 0.0f;
        for (int j = 0; j < n_vocab; j++) sum += probs[j];
        if (sum > 0.0f) {
            inv_sum = 1.0f / sum;
            for (int j = 0; j < n_vocab; j++) probs[j] *= inv_sum;
        }
    }
    
    // ---- Step 5: Top-P (nucleus) on sorted K-subset ----
    // Correct order: softmax → sort → accumulate → cutoff → renormalize
    if (top_p < 1.0f) {
        // Re-sort the effective_k subset (already partially sorted above)
        // If top_k wasn't applied, we need a full sort of nonzero entries
        if (effective_k == n_vocab) {
            std::partial_sort(sorted, sorted + n_vocab, sorted + n_vocab,
                [](const auto& a, const auto& b) { return a.first > b.first; });
        }
        
        // Walk sorted order, zero out once cumsum exceeds top_p
        float cumsum = 0.0f;
        bool cutting = false;
        for (int j = 0; j < effective_k; j++) {
            if (cutting) {
                probs[sorted[j].second] = 0.0f;
            } else {
                cumsum += sorted[j].first;
                if (cumsum > top_p) {
                    cutting = true;
                    // Keep this token (it pushed us over), zero the rest
                }
            }
        }
        
        // Renormalize
        sum = 0.0f;
        for (int j = 0; j < n_vocab; j++) sum += probs[j];
        if (sum > 0.0f) {
            inv_sum = 1.0f / sum;
            for (int j = 0; j < n_vocab; j++) probs[j] *= inv_sum;
        }
    }
    
    // ---- Step 6: Sample using PCG32 (fast, no std::discrete_distribution) ----
    int token = sampleFromProbs(probs, n_vocab);
    
    // ---- Step 7: Update history (ring buffer style) ----
    last_tokens.push_back(token);
    if (last_tokens.size() > 1024) {
        // Shift left by 256 instead of erasing one at a time
        size_t keep = 768;
        std::memmove(last_tokens.data(), 
                     last_tokens.data() + last_tokens.size() - keep,
                     keep * sizeof(int));
        last_tokens.resize(keep);
    }
    
    return token;
}
    
    return token;
}
