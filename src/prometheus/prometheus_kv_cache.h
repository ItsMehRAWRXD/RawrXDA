#pragma once
#include <cstdint>
#include <vector>

namespace Prometheus {

// =============================================================================
// KV CACHE — Minimal stub for compilation
// =============================================================================
struct KVCache {
    struct Entry {
        std::vector<float> key;
        std::vector<float> value;
        uint32_t position = 0;
    };

    uint32_t numLayers = 0;
    uint32_t numHeads = 0;
    uint32_t headDim = 0;
    uint32_t maxSeqLen = 0;
    std::vector<std::vector<Entry>> entries; // [layer][position]

    void resize(uint32_t layers, uint32_t heads, uint32_t dim, uint32_t maxLen) {
        numLayers = layers;
        numHeads = heads;
        headDim = dim;
        maxSeqLen = maxLen;
        entries.resize(layers);
        for (auto& layer : entries) {
            layer.reserve(maxLen);
        }
    }

    void clear() {
        for (auto& layer : entries) {
            layer.clear();
        }
    }

    void append(uint32_t layer, const std::vector<float>& k, const std::vector<float>& v, uint32_t pos) {
        if (layer < entries.size()) {
            entries[layer].push_back({k, v, pos});
        }
    }

    size_t size(uint32_t layer) const {
        return layer < entries.size() ? entries[layer].size() : 0;
    }
};

} // namespace Prometheus
