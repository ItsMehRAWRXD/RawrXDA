#pragma once

#include <chrono>
#include <unordered_map>
#include <string>
#include <vector>
#include <mutex>

namespace RawrXD {

/**
 * @brief Time-To-Live (TTL) and Cache Eviction policy for VectorIndex.
 * Ensures that stale codebase embeddings are periodically purged to save memory
 * and ensure RAG accuracy.
 */
class VectorIndexCacheManager {
public:
    struct CacheEntry {
        uint64_t lastAccess;
        float freshnessScore;
        std::string filePath;
    };

    VectorIndexCacheManager(uint64_t ttlMs = 3600000) : m_ttlMs(ttlMs) {}

    void record_access(const std::string& vectorId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        uint64_t now = get_now_ms();
        m_cache[vectorId].lastAccess = now;
    }

    std::vector<std::string> get_stale_entries() {
        std::lock_guard<std::mutex> lock(m_mutex);
        uint64_t now = get_now_ms();
        std::vector<std::string> stale;
        for (auto it = m_cache.begin(); it != m_cache.end(); ) {
            if (now - it->second.lastAccess > m_ttlMs) {
                stale.push_back(it->first);
                it = m_cache.erase(it);
            } else {
                ++it;
            }
        }
        return stale;
    }

    void set_freshness(const std::string& vectorId, float score) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cache[vectorId].freshnessScore = score;
    }

private:
    uint64_t get_now_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    uint64_t m_ttlMs;
    std::unordered_map<std::string, CacheEntry> m_cache;
    std::mutex m_mutex;
};

} // namespace RawrXD
