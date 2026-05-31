#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace RawrXD::Memory {

struct PKPGConfig {
    float confidenceThreshold = 0.65f;
    size_t maxPrefetchBytesPerStep = 8ull << 20;
    size_t maxOutstandingPrefetches = 32;
};

struct PKPGPrefetchRequest {
    uint64_t kvId = 0;
    float confidence = 0.0f;
    size_t bytes = 0;
};

class PredictiveKVPrefetchGraph {
public:
    explicit PredictiveKVPrefetchGraph(PKPGConfig config = {});

    void observeTransition(uint64_t fromKvId, uint64_t toKvId, bool hit);

    std::vector<PKPGPrefetchRequest> schedule(uint64_t currentKvId,
                                              const std::unordered_map<uint64_t, size_t>& kvBytes);

    size_t cancelLowConfidence(float threshold);
    void complete(uint64_t kvId, bool wasUsed);

private:
    struct EdgeStats {
        uint32_t hits = 0;
        uint32_t misses = 0;
        float confidence() const;
    };

    mutable std::mutex m_mutex;
    PKPGConfig m_config;
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, EdgeStats>> m_edges;
    std::deque<PKPGPrefetchRequest> m_outstanding;
};

} // namespace RawrXD::Memory
