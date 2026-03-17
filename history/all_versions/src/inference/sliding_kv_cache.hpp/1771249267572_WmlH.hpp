// ================================================================
// sliding_kv_cache.hpp — Compressed Sliding Window KV Cache
// Ring buffer with SVD dimension reduction (4096 → 64)
// ================================================================

#pragma once
#ifndef RAWRXD_SLIDING_KV_CACHE_HPP
#define RAWRXD_SLIDING_KV_CACHE_HPP

#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace rawrxd {

// ================================================================
// Configuration
// ================================================================
struct KVCacheConfig {
    uint32_t window_size       = 512;     // Sliding window: last N tokens
    uint32_t full_dim           = 4096;    // Original KV dimension (e.g., hidden_size)
    uint32_t compressed_dim     = 64;      // SVD-reduced dimension
    uint32_t num_heads          = 32;      // Number of attention heads
    uint32_t head_dim           = 128;     // Dimension per head (full_dim / num_heads)
    uint32_t num_layers         = 80;      // Model layers
    bool     enable_compression = true;    // Toggle SVD compression
};

// ================================================================
// SVDProjector — Learned projection matrices for KV compression
// ================================================================
// Reduces dimension from full_dim → compressed_dim
// U: [full_dim x compressed_dim] — encoder
// V: [compressed_dim x full_dim] — decoder (reconstruction)
// ================================================================
class SVDProjector {
public:
    SVDProjector() = default;
    SVDProjector(uint32_t full_dim, uint32_t compressed_dim)
        : full_dim_(full_dim), compressed_dim_(compressed_dim)
    {
        U_.resize(full_dim * compressed_dim);
        V_.resize(compressed_dim * full_dim);
        initializeIdentityProjection();
    }

    // Compress: [full_dim] → [compressed_dim]
    void encode(const float* input, float* output) const {
        // output = U^T * input (matrix-vector multiply)
        for (uint32_t i = 0; i < compressed_dim_; i++) {
            float acc = 0.0f;
            for (uint32_t j = 0; j < full_dim_; j++) {
                acc += U_[j * compressed_dim_ + i] * input[j];
            }
            output[i] = acc;
        }
    }

    // Decompress: [compressed_dim] → [full_dim]
    void decode(const float* input, float* output) const {
        // output = V^T * input
        for (uint32_t i = 0; i < full_dim_; i++) {
            float acc = 0.0f;
            for (uint32_t j = 0; j < compressed_dim_; j++) {
                acc += V_[j * full_dim_ + i] * input[j];
            }
            output[i] = acc;
        }
    }

    // Load pre-computed SVD projection matrices from buffer
    void loadFromBuffer(const float* u_data, const float* v_data) {
        memcpy(U_.data(), u_data, U_.size() * sizeof(float));
        memcpy(V_.data(), v_data, V_.size() * sizeof(float));
    }

    uint32_t fullDim() const { return full_dim_; }
    uint32_t compressedDim() const { return compressed_dim_; }

private:
    uint32_t full_dim_       = 0;
    uint32_t compressed_dim_ = 0;
    std::vector<float> U_;  // [full_dim x compressed_dim]
    std::vector<float> V_;  // [compressed_dim x full_dim]

    // Initialize as truncated identity (passes first compressed_dim components)
    void initializeIdentityProjection() {
        std::fill(U_.begin(), U_.end(), 0.0f);
        std::fill(V_.begin(), V_.end(), 0.0f);
        uint32_t min_dim = std::min(full_dim_, compressed_dim_);
        for (uint32_t i = 0; i < min_dim; i++) {
            U_[i * compressed_dim_ + i] = 1.0f;
            V_[i * full_dim_ + i] = 1.0f;
        }
    }
};

// ================================================================
// CompressedKVCache — Ring-buffer sliding window with SVD compression
// ================================================================
// Memory layout per layer:
//   K cache: [window_size x compressed_dim] = 512 * 64 * 4 = 128KB
//   V cache: [window_size x compressed_dim] = 128KB
//   Total per layer: 256KB
//   Total 80 layers: 20MB (vs ~5GB uncompressed)
// ================================================================
class CompressedKVCache {
public:
    CompressedKVCache() = default;

    explicit CompressedKVCache(const KVCacheConfig& cfg)
        : config_(cfg)
    {
        uint32_t dim = config_.enable_compression
            ? config_.compressed_dim
            : config_.full_dim;

        // Allocate cache storage per layer
        size_t per_layer_size = config_.window_size * dim;
        k_cache_.resize(config_.num_layers);
        v_cache_.resize(config_.num_layers);
        write_pos_.resize(config_.num_layers, 0);
        fill_count_.resize(config_.num_layers, 0);

        for (uint32_t L = 0; L < config_.num_layers; L++) {
            k_cache_[L].resize(per_layer_size, 0.0f);
            v_cache_[L].resize(per_layer_size, 0.0f);
        }

        // Initialize SVD projectors per layer
        if (config_.enable_compression) {
            projectors_.resize(config_.num_layers,
                SVDProjector(config_.full_dim, config_.compressed_dim));
        }

        // Temporary buffers for encode/decode
        tmp_compressed_.resize(dim);
        tmp_full_.resize(config_.full_dim);
    }

    // ================================================================
    // insert — Add a new KV pair for a given layer
    // ================================================================
    // k_full: [full_dim] key vector
    // v_full: [full_dim] value vector
    // layer:  layer index
    // ================================================================
    void insert(uint32_t layer, const float* k_full, const float* v_full) {
        uint32_t dim = effectiveDim();
        uint32_t pos = write_pos_[layer] % config_.window_size;
        float* k_dst = k_cache_[layer].data() + pos * dim;
        float* v_dst = v_cache_[layer].data() + pos * dim;

        if (config_.enable_compression) {
            projectors_[layer].encode(k_full, k_dst);
            projectors_[layer].encode(v_full, v_dst);
        } else {
            memcpy(k_dst, k_full, dim * sizeof(float));
            memcpy(v_dst, v_full, dim * sizeof(float));
        }

        write_pos_[layer]++;
        if (fill_count_[layer] < config_.window_size) {
            fill_count_[layer]++;
        }

        total_insertions_.fetch_add(1, std::memory_order_relaxed);
    }

    // ================================================================
    // getAttentionContext — Retrieve cached KV for attention computation
    // ================================================================
    // layer: layer index
    // k_out: [window_used x full_dim] output key matrix
    // v_out: [window_used x full_dim] output value matrix
    // Returns: number of valid entries (window_used)
    // ================================================================
    uint32_t getAttentionContext(uint32_t layer,
                                 float* k_out, float* v_out) const {
        uint32_t dim = effectiveDim();
        uint32_t count = fill_count_[layer];

        if (count == 0) return 0;

        // Read from oldest to newest in ring buffer order
        uint32_t start_pos = 0;
        if (write_pos_[layer] > config_.window_size) {
            start_pos = write_pos_[layer] % config_.window_size;
        }

        for (uint32_t i = 0; i < count; i++) {
            uint32_t ring_idx = (start_pos + i) % config_.window_size;
            const float* k_src = k_cache_[layer].data() + ring_idx * dim;
            const float* v_src = v_cache_[layer].data() + ring_idx * dim;

            if (config_.enable_compression) {
                projectors_[layer].decode(k_src, k_out + i * config_.full_dim);
                projectors_[layer].decode(v_src, v_out + i * config_.full_dim);
            } else {
                memcpy(k_out + i * dim, k_src, dim * sizeof(float));
                memcpy(v_out + i * dim, v_src, dim * sizeof(float));
            }
        }

        return count;
    }

    // ================================================================
    // computeAttentionScores — Dot-product attention in compressed space
    // ================================================================
    // Performs attention directly on compressed KV to avoid decompression
    // query: [full_dim] query vector (will be compressed internally)
    // scores_out: [window_size] attention score buffer
    // Returns: number of valid scores
    // ================================================================
    uint32_t computeCompressedAttention(uint32_t layer,
                                         const float* query,
                                         float* scores_out) {
        uint32_t dim = effectiveDim();
        uint32_t count = fill_count_[layer];
        if (count == 0) return 0;

        // Compress query
        const float* q_vec = query;
        if (config_.enable_compression) {
            projectors_[layer].encode(query, tmp_compressed_.data());
            q_vec = tmp_compressed_.data();
        }

        // Compute dot-product scores against all cached keys
        uint32_t start_pos = 0;
        if (write_pos_[layer] > config_.window_size) {
            start_pos = write_pos_[layer] % config_.window_size;
        }

        float scale = 1.0f / sqrtf(static_cast<float>(dim));

        for (uint32_t i = 0; i < count; i++) {
            uint32_t ring_idx = (start_pos + i) % config_.window_size;
            const float* k_ptr = k_cache_[layer].data() + ring_idx * dim;

            float dot = 0.0f;
            for (uint32_t d = 0; d < dim; d++) {
                dot += q_vec[d] * k_ptr[d];
            }
            scores_out[i] = dot * scale;
        }

        return count;
    }

    // ================================================================
    // Eviction and management
    // ================================================================
    void clearLayer(uint32_t layer) {
        uint32_t dim = effectiveDim();
        std::fill(k_cache_[layer].begin(), k_cache_[layer].end(), 0.0f);
        std::fill(v_cache_[layer].begin(), v_cache_[layer].end(), 0.0f);
        write_pos_[layer] = 0;
        fill_count_[layer] = 0;
    }

    void clearAll() {
        for (uint32_t L = 0; L < config_.num_layers; L++) {
            clearLayer(L);
        }
        total_insertions_.store(0, std::memory_order_relaxed);
    }

    // Load SVD projector weights for a specific layer
    void loadProjector(uint32_t layer, const float* u_data, const float* v_data) {
        if (config_.enable_compression && layer < config_.num_layers) {
            projectors_[layer].loadFromBuffer(u_data, v_data);
        }
    }

    // ================================================================
    // Statistics
    // ================================================================
    size_t memoryUsageBytes() const {
        uint32_t dim = effectiveDim();
        size_t cache_bytes = config_.num_layers * config_.window_size * dim * sizeof(float) * 2;
        size_t proj_bytes = 0;
        if (config_.enable_compression) {
            proj_bytes = config_.num_layers *
                (config_.full_dim * config_.compressed_dim +
                 config_.compressed_dim * config_.full_dim) * sizeof(float);
        }
        return cache_bytes + proj_bytes;
    }

    size_t uncompressedEquivalentBytes() const {
        return config_.num_layers * config_.window_size *
               config_.full_dim * sizeof(float) * 2;
    }

    double compressionRatio() const {
        size_t actual = memoryUsageBytes();
        return actual > 0
            ? static_cast<double>(uncompressedEquivalentBytes()) / actual
            : 1.0;
    }

    uint64_t totalInsertions() const {
        return total_insertions_.load(std::memory_order_relaxed);
    }

    uint32_t windowSize() const { return config_.window_size; }
    uint32_t numLayers()  const { return config_.num_layers; }

private:
    KVCacheConfig config_;

    // Per-layer ring buffers (compressed dimension)
    std::vector<std::vector<float>> k_cache_;
    std::vector<std::vector<float>> v_cache_;
    std::vector<uint32_t>           write_pos_;
    std::vector<uint32_t>           fill_count_;

    // SVD projection matrices per layer
    std::vector<SVDProjector> projectors_;

    // Temporary buffers (reused to avoid allocation)
    mutable std::vector<float> tmp_compressed_;
    mutable std::vector<float> tmp_full_;

    std::atomic<uint64_t> total_insertions_{0};

    uint32_t effectiveDim() const {
        return config_.enable_compression
            ? config_.compressed_dim
            : config_.full_dim;
    }
};

} // namespace rawrxd

#endif // RAWRXD_SLIDING_KV_CACHE_HPP
