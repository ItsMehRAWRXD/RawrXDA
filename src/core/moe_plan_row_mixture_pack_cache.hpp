// LRU cache for packed MoE expert down-weight blocks (grouped GEMM layout), keyed by stable mixture string.
// Used by RawrXDTransformer when moe_down_enable_grouped_integration is on; execution path still falls back to
// per-expert SwiGLU until a shared pre-down activation allows fused grouped apply.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <list>
#include <mutex>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace RawrXD
{
namespace MoEIntegr
{

class MoEMixturePlanPackCache
{
  public:
    /// \param maxPackedBytes Total packed payload cap (sum of `vector<float>` sizes × sizeof(float)).
    ///        `0` disables the byte cap (entry limit only).
    explicit MoEMixturePlanPackCache(std::size_t maxEntries, std::size_t maxPackedBytes = 0)
        : m_maxEntries(maxEntries > 0 ? maxEntries : 1u), m_maxPackedBytes(maxPackedBytes)
    {
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(m_mu);
        clearEntriesOnlyUnlocked();
        m_hits = 0;
        m_misses = 0;
        m_evictions = 0;
        m_rowInvalidationEvictions = 0;
    }

    /// Drop all packed entries but keep hit/miss/eviction counters (e.g. swarm \p planGeneration changed).
    void clearEntriesOnly()
    {
        std::lock_guard<std::mutex> lock(m_mu);
        clearEntriesOnlyUnlocked();
    }

    [[nodiscard]] std::uint64_t hits() const noexcept
    {
        std::lock_guard<std::mutex> lock(m_mu);
        return m_hits;
    }
    [[nodiscard]] std::uint64_t misses() const noexcept
    {
        std::lock_guard<std::mutex> lock(m_mu);
        return m_misses;
    }
    [[nodiscard]] std::uint64_t evictions() const noexcept
    {
        std::lock_guard<std::mutex> lock(m_mu);
        return m_evictions;
    }
    [[nodiscard]] std::uint64_t rowInvalidationEvictions() const noexcept
    {
        std::lock_guard<std::mutex> lock(m_mu);
        return m_rowInvalidationEvictions;
    }
    [[nodiscard]] std::size_t currentPackedBytes() const noexcept
    {
        std::lock_guard<std::mutex> lock(m_mu);
        return m_currentPackedBytes;
    }
    [[nodiscard]] std::size_t maxPackedBytes() const noexcept
    {
        std::lock_guard<std::mutex> lock(m_mu);
        return m_maxPackedBytes;
    }

    /// On hit: sets \p outPtr / \p outFloatCount to internal storage (valid until next mutating call).
    [[nodiscard]] bool tryGet(const std::string& key, const float** outPtr, std::size_t* outFloatCount)
    {
        std::lock_guard<std::mutex> lock(m_mu);
        const auto it = m_index.find(key);
        if (it == m_index.end())
        {
            ++m_misses;
            return false;
        }
        ++m_hits;
        m_lru.splice(m_lru.begin(), m_lru, it->second);
        std::vector<float>& v = it->second->second;
        *outPtr = v.data();
        *outFloatCount = v.size();
        return true;
    }

    [[nodiscard]] bool contains(const std::string& key) const
    {
        std::lock_guard<std::mutex> lock(m_mu);
        return m_index.find(key) != m_index.end();
    }

    /// Insert / refresh without plan-row reverse index (tests and legacy paths).
    void put(const std::string& key, std::vector<float>&& data)
    {
        std::lock_guard<std::mutex> lock(m_mu);
        putUnlocked(key, std::move(data), {});
    }

    /// Insert / refresh and register \p planRows for \ref invalidateEntriesReferencingPlanRows.
    void put(const std::string& key, std::vector<float>&& data, std::span<const std::size_t> planRows)
    {
        std::lock_guard<std::mutex> lock(m_mu);
        putUnlocked(key, std::move(data), planRows);
    }

    /// Remove every cache entry that references any of \p rowIds (O(affected keys); uses reverse index).
    [[nodiscard]] std::uint32_t invalidateEntriesReferencingPlanRows(std::span<const std::size_t> rowIds)
    {
        std::lock_guard<std::mutex> lock(m_mu);
        std::unordered_set<std::string> keysToErase;
        for (const std::size_t row : rowIds)
        {
            const auto it = m_rowToKeys.find(row);
            if (it == m_rowToKeys.end())
                continue;
            for (const std::string& k : it->second)
                keysToErase.insert(k);
        }
        std::uint32_t n = 0;
        for (const std::string& k : keysToErase)
        {
            if (eraseKeyUnlocked(k))
            {
                ++n;
                ++m_rowInvalidationEvictions;
            }
        }
        return n;
    }

  private:
    void clearEntriesOnlyUnlocked() noexcept
    {
        m_lru.clear();
        m_index.clear();
        m_rowToKeys.clear();
        m_keyToRows.clear();
        m_currentPackedBytes = 0;
    }

    [[nodiscard]] static std::size_t packedBytesOf(const std::vector<float>& v) noexcept
    {
        return v.size() * sizeof(float);
    }

    void unlinkKeyFromRowMaps_(const std::string& key)
    {
        const auto kt = m_keyToRows.find(key);
        if (kt == m_keyToRows.end())
            return;
        for (const std::size_t row : kt->second)
        {
            const auto rt = m_rowToKeys.find(row);
            if (rt == m_rowToKeys.end())
                continue;
            rt->second.erase(key);
            if (rt->second.empty())
                m_rowToKeys.erase(rt);
        }
        m_keyToRows.erase(kt);
    }

    static std::vector<std::size_t> normalizeRowList_(std::span<const std::size_t> planRows)
    {
        std::vector<std::size_t> out(planRows.begin(), planRows.end());
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
        return out;
    }

    void linkKeyToRowMaps_(const std::string& key, const std::vector<std::size_t>& rows)
    {
        if (rows.empty())
            return;
        m_keyToRows[key] = rows;
        for (const std::size_t row : rows)
            m_rowToKeys[row].insert(key);
    }

    [[nodiscard]] bool eraseKeyUnlocked(const std::string& key)
    {
        const auto it = m_index.find(key);
        if (it == m_index.end())
            return false;
        unlinkKeyFromRowMaps_(key);
        m_currentPackedBytes -= packedBytesOf(it->second->second);
        m_lru.erase(it->second);
        m_index.erase(it);
        return true;
    }

    void evictLruBack()
    {
        if (m_lru.empty())
            return;
        const std::string& k = m_lru.back().first;
        unlinkKeyFromRowMaps_(k);
        m_currentPackedBytes -= packedBytesOf(m_lru.back().second);
        m_index.erase(k);
        m_lru.pop_back();
    }

    [[nodiscard]] bool shouldEvictBeforeInsert(std::size_t incomingBytes) const noexcept
    {
        if (m_lru.size() >= m_maxEntries)
            return true;
        if (m_maxPackedBytes == 0)
            return false;
        return m_currentPackedBytes + incomingBytes > m_maxPackedBytes;
    }

    void evictUntilWithinByteCapPreferKeepFront()
    {
        if (m_maxPackedBytes == 0)
            return;
        while (m_currentPackedBytes > m_maxPackedBytes && m_lru.size() > 1)
        {
            ++m_evictions;
            evictLruBack();
        }
    }

    void putUnlocked(const std::string& key, std::vector<float>&& data, std::span<const std::size_t> planRows)
    {
        const std::size_t newBytes = data.size() * sizeof(float);
        const auto it = m_index.find(key);
        if (it != m_index.end())
        {
            unlinkKeyFromRowMaps_(key);
            std::vector<float>& slot = it->second->second;
            m_currentPackedBytes -= slot.size() * sizeof(float);
            slot = std::move(data);
            m_currentPackedBytes += slot.size() * sizeof(float);
            m_lru.splice(m_lru.begin(), m_lru, it->second);
            linkKeyToRowMaps_(key, normalizeRowList_(planRows));
            evictUntilWithinByteCapPreferKeepFront();
            return;
        }

        while (shouldEvictBeforeInsert(newBytes))
        {
            ++m_evictions;
            evictLruBack();
        }
        m_lru.emplace_front(key, std::move(data));
        m_index[m_lru.front().first] = m_lru.begin();
        m_currentPackedBytes += m_lru.front().second.size() * sizeof(float);
        linkKeyToRowMaps_(key, normalizeRowList_(planRows));
    }

    mutable std::mutex m_mu;
    std::size_t m_maxEntries;
    std::size_t m_maxPackedBytes = 0;
    std::size_t m_currentPackedBytes = 0;
    std::uint64_t m_hits = 0;
    std::uint64_t m_misses = 0;
    std::uint64_t m_evictions = 0;
    std::uint64_t m_rowInvalidationEvictions = 0;
    using ListType = std::list<std::pair<std::string, std::vector<float>>>;
    ListType m_lru;
    std::unordered_map<std::string, ListType::iterator> m_index;
    std::unordered_map<std::size_t, std::unordered_set<std::string>> m_rowToKeys;
    std::unordered_map<std::string, std::vector<std::size_t>> m_keyToRows;
};

}  // namespace MoEIntegr
}  // namespace RawrXD
