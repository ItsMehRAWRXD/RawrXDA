// ============================================================================
// Cache Manager — Intelligent Caching System
// Multi-tier caching with predictive preloading
// ============================================================================
#pragma once
#include "../inference/SovereignInferenceClient.h"
#include "../core/session_manager.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <chrono>
#include <list>

namespace RawrXD::Cache {

enum class CacheTier {
    MEMORY,
    DISK,
    NETWORK
};

enum class CachePolicy {
    LRU,
    LFU,
    FIFO,
    ADAPTIVE
};

struct CacheEntry {
    std::string key;
    std::vector<uint8_t> data;
    size_t size;
    CacheTier tier;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastAccessed;
    int accessCount;
    std::map<std::string, std::string> metadata;
};

struct CacheStats {
    size_t totalSize;
    size_t maxSize;
    int hitCount;
    int missCount;
    double hitRate;
    int evictionCount;
    std::map<CacheTier, size_t> tierSizes;
};

struct PrefetchPrediction {
    std::string key;
    double confidence;
    std::chrono::system_clock::time_point predictedAt;
};

class CacheManager {
public:
    explicit CacheManager(std::shared_ptr<Core::SessionManager> sessionManager,
                         size_t maxMemorySize = 100 * 1024 * 1024, // 100MB
                         size_t maxDiskSize = 1024 * 1024 * 1024)  // 1GB
        : m_sessionManager(sessionManager)
        , m_maxMemorySize(maxMemorySize)
        , m_maxDiskSize(maxDiskSize)
        , m_policy(CachePolicy::LRU) {}

    void Put(const std::string& key, const std::vector<uint8_t>& data,
            CacheTier tier = CacheTier::MEMORY) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Check if entry exists
        auto it = m_entries.find(key);
        if (it != m_entries.end()) {
            // Update existing entry
            it->second.data = data;
            it->second.size = data.size();
            it->second.lastAccessed = std::chrono::system_clock::now();
            UpdateLRU(key);
            return;
        }
        
        // Check size limits and evict if necessary
        if (tier == CacheTier::MEMORY) {
            while (m_memorySize + data.size() > m_maxMemorySize && !m_memoryQueue.empty()) {
                EvictEntry(m_memoryQueue.back());
            }
        }
        
        // Create new entry
        CacheEntry entry;
        entry.key = key;
        entry.data = data;
        entry.size = data.size();
        entry.tier = tier;
        entry.createdAt = std::chrono::system_clock::now();
        entry.lastAccessed = entry.createdAt;
        entry.accessCount = 0;
        
        m_entries[key] = entry;
        
        if (tier == CacheTier::MEMORY) {
            m_memorySize += data.size();
            m_memoryQueue.push_front(key);
            m_lruMap[key] = m_memoryQueue.begin();
        }
        
        m_stats.totalSize += data.size();
    }

    std::optional<std::vector<uint8_t>> Get(const std::string& key) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_entries.find(key);
        if (it == m_entries.end()) {
            m_stats.missCount++;
            UpdateStats();
            return std::nullopt;
        }
        
        // Update access statistics
        it->second.lastAccessed = std::chrono::system_clock::now();
        it->second.accessCount++;
        
        // Update LRU
        if (it->second.tier == CacheTier::MEMORY) {
            UpdateLRU(key);
        }
        
        // Promote from disk to memory if frequently accessed
        if (it->second.tier == CacheTier::DISK && it->second.accessCount > 5) {
            PromoteToMemory(key);
        }
        
        m_stats.hitCount++;
        UpdateStats();
        
        return it->second.data;
    }

    void Invalidate(const std::string& key) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_entries.find(key);
        if (it != m_entries.end()) {
            EvictEntry(key);
        }
    }

    void InvalidatePattern(const std::string& pattern) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<std::string> toInvalidate;
        for (const auto& [key, entry] : m_entries) {
            if (key.find(pattern) != std::string::npos) {
                toInvalidate.push_back(key);
            }
        }
        
        for (const auto& key : toInvalidate) {
            EvictEntry(key);
        }
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        m_entries.clear();
        m_memoryQueue.clear();
        m_lruMap.clear();
        m_memorySize = 0;
        m_stats = CacheStats{};
    }

    CacheStats GetStats() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_stats;
    }

    void SetPolicy(CachePolicy policy) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_policy = policy;
    }

    void Prefetch(const std::vector<std::string>& keys) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (const auto& key : keys) {
            if (m_entries.find(key) == m_entries.end()) {
                // Predict and load
                m_prefetchQueue.push(key);
            }
        }
        
        ProcessPrefetchQueue();
    }

    std::vector<PrefetchPrediction> PredictPrefetchNeeds(const std::string& currentKey) {
        std::vector<PrefetchPrediction> predictions;
        
        // Simple prediction based on access patterns
        // In a real implementation, this would use ML
        
        // Find keys with similar prefix
        std::string prefix = currentKey.substr(0, currentKey.find_last_of('/') + 1);
        
        for (const auto& [key, entry] : m_entries) {
            if (key.find(prefix) == 0 && key != currentKey) {
                PrefetchPrediction pred;
                pred.key = key;
                pred.confidence = 0.7;
                pred.predictedAt = std::chrono::system_clock::now();
                predictions.push_back(pred);
            }
        }
        
        return predictions;
    }

    std::string GenerateCacheReport() {
        std::ostringstream report;
        report << "# Cache Report\n\n";
        
        auto stats = GetStats();
        report << "## Statistics\n";
        report << "- **Total Size:** " << FormatBytes(stats.totalSize) << "\n";
        report << "- **Max Size:** " << FormatBytes(stats.maxSize) << "\n";
        report << "- **Hit Count:** " << stats.hitCount << "\n";
        report << "- **Miss Count:** " << stats.missCount << "\n";
        report << "- **Hit Rate:** " << std::fixed << std::setprecision(2) 
               << (stats.hitRate * 100) << "%\n";
        report << "- **Evictions:** " << stats.evictionCount << "\n\n";
        
        report << "## Entries\n";
        report << "| Key | Size | Tier | Access Count |\n";
        report << "|-----|------|------|--------------|\n";
        
        for (const auto& [key, entry] : m_entries) {
            report << "| " << key.substr(0, 30) << " | " << FormatBytes(entry.size) << " | "
                   << TierToString(entry.tier) << " | " << entry.accessCount << " |\n";
        }
        
        return report.str();
    }

private:
    std::shared_ptr<Core::SessionManager> m_sessionManager;
    mutable std::mutex m_mutex;
    std::map<std::string, CacheEntry> m_entries;
    std::list<std::string> m_memoryQueue;
    std::map<std::string, std::list<std::string>::iterator> m_lruMap;
    std::queue<std::string> m_prefetchQueue;
    
    size_t m_maxMemorySize;
    size_t m_maxDiskSize;
    size_t m_memorySize = 0;
    CachePolicy m_policy;
    CacheStats m_stats;

    void UpdateLRU(const std::string& key) {
        auto it = m_lruMap.find(key);
        if (it != m_lruMap.end()) {
            m_memoryQueue.erase(it->second);
            m_memoryQueue.push_front(key);
            it->second = m_memoryQueue.begin();
        }
    }

    void EvictEntry(const std::string& key) {
        auto it = m_entries.find(key);
        if (it == m_entries.end()) return;
        
        if (it->second.tier == CacheTier::MEMORY) {
            auto lruIt = m_lruMap.find(key);
            if (lruIt != m_lruMap.end()) {
                m_memoryQueue.erase(lruIt->second);
                m_lruMap.erase(lruIt);
            }
            m_memorySize -= it->second.size;
        }
        
        m_stats.totalSize -= it->second.size;
        m_entries.erase(it);
        m_stats.evictionCount++;
    }

    void PromoteToMemory(const std::string& key) {
        auto it = m_entries.find(key);
        if (it == m_entries.end()) return;
        
        // Check memory limit
        while (m_memorySize + it->second.size > m_maxMemorySize && !m_memoryQueue.empty()) {
            EvictEntry(m_memoryQueue.back());
        }
        
        it->second.tier = CacheTier::MEMORY;
        m_memorySize += it->second.size;
        m_memoryQueue.push_front(key);
        m_lruMap[key] = m_memoryQueue.begin();
    }

    void ProcessPrefetchQueue() {
        while (!m_prefetchQueue.empty()) {
            auto key = m_prefetchQueue.front();
            m_prefetchQueue.pop();
            
            // Load from source
            // This would integrate with your data source
        }
    }

    void UpdateStats() {
        int total = m_stats.hitCount + m_stats.missCount;
        if (total > 0) {
            m_stats.hitRate = static_cast<double>(m_stats.hitCount) / total;
        }
        m_stats.maxSize = m_maxMemorySize + m_maxDiskSize;
    }

    std::string FormatBytes(size_t bytes) {
        const char* units[] = {"B", "KB", "MB", "GB"};
        int unitIndex = 0;
        double size = static_cast<double>(bytes);
        
        while (size >= 1024.0 && unitIndex < 3) {
            size /= 1024.0;
            unitIndex++;
        }
        
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
        return oss.str();
    }

    std::string TierToString(CacheTier tier) {
        switch (tier) {
            case CacheTier::MEMORY: return "Memory";
            case CacheTier::DISK: return "Disk";
            case CacheTier::NETWORK: return "Network";
            default: return "Unknown";
        }
    }
};

} // namespace RawrXD::Cache
