#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace RawrXD::Memory {

struct NMIEntry {
    uint64_t kvId = 0;
    std::vector<float> embedding;
};

struct NMISearchResult {
    uint64_t kvId = 0;
    float score = 0.0f;
    bool exactFallback = false;
};

class NeuralMemoryIndexing {
public:
    void upsert(uint64_t kvId, const std::vector<float>& embedding);

    std::vector<NMISearchResult> searchApprox(const std::vector<float>& query,
                                              size_t topK,
                                              float minConfidence,
                                              bool exactRefine) const;

    std::optional<NMISearchResult> exactLookup(uint64_t kvId) const;

private:
    static float cosine(const std::vector<float>& a, const std::vector<float>& b);

    mutable std::mutex m_mutex;
    std::unordered_map<uint64_t, NMIEntry> m_entries;
};

} // namespace RawrXD::Memory
