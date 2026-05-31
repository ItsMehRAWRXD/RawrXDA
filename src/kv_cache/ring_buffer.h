#pragma once
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace RawrXD {

// Lock-free ring buffer for KV cache.
// Provides O(1) append, efficient sliding-window attention and zero reallocation.
class KVCacheRingBuffer {
public:
    struct Config {
        size_t n_layers;
        size_t n_heads;
        size_t head_dim;
        size_t max_seq_len;
        size_t sliding_window = 0;  // 0 = disabled
    };

    explicit KVCacheRingBuffer(const Config& cfg)
        : m_cfg(cfg)
        , m_seqLen(0)
        , m_startPos(0)
    {
        const size_t floats_per_layer =
            cfg.n_heads * cfg.head_dim * cfg.max_seq_len;
        const size_t bytes_per_layer = floats_per_layer * sizeof(float);

        m_kCache.resize(cfg.n_layers * floats_per_layer, 0.0f);
        m_vCache.resize(cfg.n_layers * floats_per_layer, 0.0f);

        m_layerKOffset.resize(cfg.n_layers);
        m_layerVOffset.resize(cfg.n_layers);
        for (size_t l = 0; l < cfg.n_layers; ++l) {
            m_layerKOffset[l] = l * floats_per_layer;
            m_layerVOffset[l] = l * floats_per_layer;
        }
        (void)bytes_per_layer; // kept for readability reference
    }

    // Append a single token's K and V vectors for one layer
    void append(size_t layer, const float* k, const float* v) {
        const size_t pos = (m_startPos + m_seqLen) % m_cfg.max_seq_len;

        float* k_dst = getK(layer, pos);
        float* v_dst = getV(layer, pos);

        const size_t vec_floats = m_cfg.n_heads * m_cfg.head_dim;
        std::memcpy(k_dst, k, vec_floats * sizeof(float));
        std::memcpy(v_dst, v, vec_floats * sizeof(float));

        if (m_seqLen < m_cfg.max_seq_len) {
            ++m_seqLen;
        } else {
            // Oldest token evicted; advance ring start
            m_startPos = (m_startPos + 1) % m_cfg.max_seq_len;
        }
    }

    // Batch append for prefill
    void appendBatch(size_t layer, const float* k, const float* v, size_t n_tokens) {
        const size_t stride = m_cfg.n_heads * m_cfg.head_dim;
        for (size_t t = 0; t < n_tokens; ++t) {
            append(layer, k + t * stride, v + t * stride);
        }
    }

    // Get KV pointers for a given logical sequence position
    std::pair<const float*, const float*> getKV(size_t layer, size_t pos) const {
        const size_t actual = (m_startPos + pos) % m_cfg.max_seq_len;
        return { getK(layer, actual), getV(layer, actual) };
    }

    // Copy a contiguous logical range [start, start+len) into caller-provided buffers.
    // Handles ring wraparound transparently.
    void getKVRange(size_t layer,
                    size_t start, size_t len,
                    float* k_out, float* v_out) const
    {
        const size_t stride     = m_cfg.n_heads * m_cfg.head_dim;
        const size_t phys_start = (m_startPos + start) % m_cfg.max_seq_len;
        const size_t phys_end   = phys_start + len;

        if (phys_end <= m_cfg.max_seq_len) {
            // No wraparound: single contiguous copy
            std::memcpy(k_out, getK(layer, phys_start), len * stride * sizeof(float));
            std::memcpy(v_out, getV(layer, phys_start), len * stride * sizeof(float));
        } else {
            // Wraparound: two copies
            const size_t first  = m_cfg.max_seq_len - phys_start;
            const size_t second = len - first;

            std::memcpy(k_out,                    getK(layer, phys_start), first  * stride * sizeof(float));
            std::memcpy(k_out + first * stride,   getK(layer, 0),          second * stride * sizeof(float));

            std::memcpy(v_out,                    getV(layer, phys_start), first  * stride * sizeof(float));
            std::memcpy(v_out + first * stride,   getV(layer, 0),          second * stride * sizeof(float));
        }
    }

    size_t seqLen()       const { return m_seqLen; }
    size_t maxSeqLen()    const { return m_cfg.max_seq_len; }

    void reset() {
        m_seqLen   = 0;
        m_startPos = 0;
    }

    // Total bytes allocated for K + V caches
    size_t memoryBytes() const {
        return (m_kCache.size() + m_vCache.size()) * sizeof(float);
    }

private:
    float* getK(size_t layer, size_t pos) {
        return m_kCache.data() + m_layerKOffset[layer] + pos * m_cfg.n_heads * m_cfg.head_dim;
    }
    float* getV(size_t layer, size_t pos) {
        return m_vCache.data() + m_layerVOffset[layer] + pos * m_cfg.n_heads * m_cfg.head_dim;
    }
    const float* getK(size_t layer, size_t pos) const {
        return m_kCache.data() + m_layerKOffset[layer] + pos * m_cfg.n_heads * m_cfg.head_dim;
    }
    const float* getV(size_t layer, size_t pos) const {
        return m_vCache.data() + m_layerVOffset[layer] + pos * m_cfg.n_heads * m_cfg.head_dim;
    }

    Config              m_cfg;
    std::vector<float>  m_kCache;
    std::vector<float>  m_vCache;
    std::vector<size_t> m_layerKOffset;
    std::vector<size_t> m_layerVOffset;
    size_t              m_seqLen;
    size_t              m_startPos;
};

} // namespace RawrXD
