// LRU cache for packed expert down-projection weights (grouped GEMM path).
// Keyed by tensor pointer identity + dimensions; use when the same expert set repeats across tokens.
#pragma once

#include "moe_expert_accumulation.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <unordered_map>
#include <utility>
#include <vector>

namespace RawrXD
{
namespace MoEAccumRef
{

struct PackCacheKey
{
    std::size_t inDim = 0;
    std::size_t outDim = 0;
    int numExperts = 0;
    std::vector<std::uintptr_t> expertPtrs;

    [[nodiscard]] bool operator==(const PackCacheKey& o) const noexcept
    {
        return inDim == o.inDim && outDim == o.outDim && numExperts == o.numExperts && expertPtrs == o.expertPtrs;
    }
};

struct PackCacheKeyHash
{
    [[nodiscard]] std::size_t operator()(const PackCacheKey& k) const noexcept
    {
        std::size_t h = k.inDim ^ (k.outDim << 1) ^ (static_cast<std::size_t>(k.numExperts) << 2);
        for (std::uintptr_t p : k.expertPtrs)
            h ^= std::hash<std::uintptr_t>{}(p) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        return h;
    }
};

/// Fixed-capacity LRU: on hit, moves entry to MRU; on miss, packs and inserts (evicts LRU if full).
class ExpertDownPackCache
{
  public:
    explicit ExpertDownPackCache(std::size_t maxEntries = 8) : m_maxEntries(maxEntries) {}

    void clear()
    {
        clearEntriesOnly();
        m_hits = 0;
        m_misses = 0;
    }

    /// Drop packed tensors but keep hit/miss counters (for long-running benchmarks).
    void clearEntriesOnly()
    {
        m_lru.clear();
        m_index.clear();
    }

    [[nodiscard]] std::uint64_t hits() const noexcept { return m_hits; }
    [[nodiscard]] std::uint64_t misses() const noexcept { return m_misses; }

    /// Returns pointer to packed weights [inDim × (K·outDim)]; valid until next cache op that evicts this key.
    [[nodiscard]] const float* getPacked(const float* const* WDown, std::size_t inDim, std::size_t outDim,
                                         int numExperts)
    {
        PackCacheKey key;
        key.inDim = inDim;
        key.outDim = outDim;
        key.numExperts = numExperts;
        key.expertPtrs.reserve(static_cast<std::size_t>(numExperts));
        for (int e = 0; e < numExperts; ++e)
            key.expertPtrs.push_back(reinterpret_cast<std::uintptr_t>(WDown[static_cast<std::size_t>(e)]));

        const auto it = m_index.find(key);
        if (it != m_index.end())
        {
            ++m_hits;
            m_lru.splice(m_lru.begin(), m_lru, it->second);
            return it->second->second.data();
        }

        ++m_misses;
        const std::size_t Ntot = static_cast<std::size_t>(numExperts) * outDim;
        std::vector<float> packed(inDim * Ntot);
        packExpertDownWeightsF32(inDim, outDim, numExperts, WDown, packed.data());

        if (m_lru.size() >= m_maxEntries)
        {
            const PackCacheKey& victim = m_lru.back().first;
            m_index.erase(victim);
            m_lru.pop_back();
        }
        m_lru.emplace_front(std::move(key), std::move(packed));
        m_index[m_lru.front().first] = m_lru.begin();
        return m_lru.front().second.data();
    }

    /// Grouped GEMM using cached pack; increments \p workspace (same semantics as \ref moeDownProjectGroupedF32).
    void downProjectGroupedCached(const float* hidden, std::size_t inDim, std::size_t outDim, int numExperts,
                                  const float* const* WDown, const float* weights, float* acc,
                                  std::vector<float>& workspace)
    {
        const float* packed = getPacked(WDown, inDim, outDim, numExperts);
        const std::size_t Ntot = static_cast<std::size_t>(numExperts) * outDim;
        workspace.resize(Ntot);
        std::fill(workspace.begin(), workspace.end(), 0.f);
        gemmRowMajorAccumF32(1, Ntot, inDim, hidden, packed, workspace.data());
        for (int e = 0; e < numExperts; ++e)
        {
            const float w = weights[static_cast<std::size_t>(e)];
            if (w == 0.f)
                continue;
            const float* block = workspace.data() + static_cast<std::size_t>(e) * outDim;
            for (std::size_t j = 0; j < outDim; ++j)
                acc[j] += w * block[j];
        }
    }

  private:
    std::size_t m_maxEntries;
    std::uint64_t m_hits = 0;
    std::uint64_t m_misses = 0;
    using ListType = std::list<std::pair<PackCacheKey, std::vector<float>>>;
    ListType m_lru;
    std::unordered_map<PackCacheKey, ListType::iterator, PackCacheKeyHash> m_index;
};

}  // namespace MoEAccumRef
}  // namespace RawrXD
