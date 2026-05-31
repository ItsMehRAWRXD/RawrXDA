#include "predictive_kv_prefetch_graph.h"

#include <algorithm>

namespace RawrXD::Memory {

PredictiveKVPrefetchGraph::PredictiveKVPrefetchGraph(PKPGConfig config)
    : m_config(config) {}

void PredictiveKVPrefetchGraph::observeTransition(uint64_t fromKvId, uint64_t toKvId, bool hit) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Aging Mechanism: Decay old stats to adapt to changing context
    for (auto& [from, targets] : m_edges) {
        for (auto& [to, stats] : targets) {
            if (stats.hits + stats.misses > 100) {
                stats.hits = (uint32_t)(stats.hits * 0.9f);
                stats.misses = (uint32_t)(stats.misses * 0.9f);
            }
        }
    }

    EdgeStats& stats = m_edges[fromKvId][toKvId];
    if (hit) {
        ++stats.hits;
    } else {
        ++stats.misses;
    }
}

std::vector<PKPGPrefetchRequest> PredictiveKVPrefetchGraph::schedule(
    uint64_t currentKvId,
    const std::unordered_map<uint64_t, size_t>& kvBytes) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<PKPGPrefetchRequest> out;
    auto eIt = m_edges.find(currentKvId);
    if (eIt == m_edges.end()) {
        return out;
    }

    std::vector<PKPGPrefetchRequest> candidates;
    for (const auto& [toKv, stats] : eIt->second) {
        const float conf = stats.confidence();
        
        // Multi-tier thresholding
        float threshold = m_config.confidenceThreshold;
        
        // Boost priority for small blocks (likely meta-data or critical KV)
        auto bIt = kvBytes.find(toKv);
        if (bIt != kvBytes.end() && bIt->second < (512ull << 10)) {
            threshold *= 0.75f; // More aggressive for small blocks
        }

        if (conf < threshold) {
            continue;
        }

        if (bIt == kvBytes.end()) {
            continue;
        }

        candidates.push_back(PKPGPrefetchRequest{toKv, conf, bIt->second});
    }

    std::sort(candidates.begin(), candidates.end(), [](const PKPGPrefetchRequest& a, const PKPGPrefetchRequest& b) {
        return a.confidence > b.confidence;
    });

    size_t bytesBudget = m_config.maxPrefetchBytesPerStep;
    for (const auto& c : candidates) {
        if (m_outstanding.size() >= m_config.maxOutstandingPrefetches) {
            break;
        }
        if (c.bytes > bytesBudget) {
            continue;
        }
        bytesBudget -= c.bytes;
        out.push_back(c);
        m_outstanding.push_back(c);
    }

    return out;
}

size_t PredictiveKVPrefetchGraph::cancelLowConfidence(float threshold) {
    std::lock_guard<std::mutex> lock(m_mutex);

    const size_t before = m_outstanding.size();
    const float t = std::clamp(threshold, 0.0f, 1.0f);
    m_outstanding.erase(
        std::remove_if(m_outstanding.begin(), m_outstanding.end(), [t](const PKPGPrefetchRequest& r) {
            return r.confidence < t;
        }),
        m_outstanding.end());

    return before - m_outstanding.size();
}

void PredictiveKVPrefetchGraph::complete(uint64_t kvId, bool wasUsed) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = std::find_if(m_outstanding.begin(), m_outstanding.end(), [kvId](const PKPGPrefetchRequest& r) {
        return r.kvId == kvId;
    });
    if (it != m_outstanding.end()) {
        m_outstanding.erase(it);
    }

    // Track self-edge as waste/success signal for local confidence tuning.
    EdgeStats& s = m_edges[kvId][kvId];
    if (wasUsed) {
        ++s.hits;
    } else {
        ++s.misses;
    }
}

float PredictiveKVPrefetchGraph::EdgeStats::confidence() const {
    const uint32_t total = hits + misses;
    if (total == 0) {
        return 0.0f;
    }
    return static_cast<float>(hits) / static_cast<float>(total);
}

} // namespace RawrXD::Memory
