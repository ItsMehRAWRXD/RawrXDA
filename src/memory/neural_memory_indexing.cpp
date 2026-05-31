#include "neural_memory_indexing.h"

#include <algorithm>
#include <cmath>

namespace RawrXD::Memory {

void NeuralMemoryIndexing::upsert(uint64_t kvId, const std::vector<float>& embedding) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries[kvId] = NMIEntry{kvId, embedding};
}

std::vector<NMISearchResult> NeuralMemoryIndexing::searchApprox(
    const std::vector<float>& query,
    size_t topK,
    float minConfidence,
    bool exactRefine) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<NMISearchResult> out;
    if (query.empty() || topK == 0) {
        return out;
    }

    out.reserve(m_entries.size());
    for (const auto& [id, e] : m_entries) {
        const float s = cosine(query, e.embedding);
        if (s < minConfidence) {
            continue;
        }
        out.push_back(NMISearchResult{id, s, false});
    }

    std::sort(out.begin(), out.end(), [](const NMISearchResult& a, const NMISearchResult& b) {
        return a.score > b.score;
    });

    if (out.size() > topK) {
        out.resize(topK);
    }

    // Fallback guarantee: if nothing passes confidence, return exact nearest by brute-force.
    if (out.empty() && exactRefine && !m_entries.empty()) {
        float best = -1.0f;
        uint64_t bestId = 0;
        for (const auto& [id, e] : m_entries) {
            const float s = cosine(query, e.embedding);
            if (s > best) {
                best = s;
                bestId = id;
            }
        }
        if (bestId != 0) {
            out.push_back(NMISearchResult{bestId, best, true});
        }
    }

    return out;
}

std::optional<NMISearchResult> NeuralMemoryIndexing::exactLookup(uint64_t kvId) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_entries.find(kvId);
    if (it == m_entries.end()) {
        return std::nullopt;
    }

    return NMISearchResult{kvId, 1.0f, true};
}

float NeuralMemoryIndexing::cosine(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.empty() || b.empty()) {
        return -1.0f;
    }

    const size_t n = std::min(a.size(), b.size());
    float dot = 0.0f;
    float na = 0.0f;
    float nb = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }

    if (na <= 0.0f || nb <= 0.0f) {
        return -1.0f;
    }

    return dot / (std::sqrt(na) * std::sqrt(nb));
}

} // namespace RawrXD::Memory
