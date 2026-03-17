/**
 * @file cache_layer.hpp
 * @brief Enterprise Caching Layer
 * 
 * Features:
 * - In-memory LRU cache
 * - TTL-based expiration
 * - Multi-tier caching
 * - Cache statistics
 * - Thread-safe operations
 */

#pragma once

#include <string>
#include <unordered_map>
#include <list>
#include <memory>
#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <any>
#include <vector>
#include <atomic>

namespace enterprise {

// =============================================================================
// Cache Entry
// =============================================================================

template<typename T>
struct CacheEntry {
    std::string key;
    T value;
    std::chrono::steady_clock::time_point createdAt;
    std::chrono::steady_clock::time_point expiresAt;
    std::chrono::steady_clock::time_point lastAccessed;
    size_t hitCount = 0;
    size_t sizeBytes = 0;
    std::string tags;  // Comma-separated tags for invalidation
};

// =============================================================================
// Cache Configuration
// =============================================================================

struct CacheConfig {
    size_t maxEntries = 10000;
    size_t maxSizeBytes = 100 * 1024 * 1024;  // 100 MB
    int defaultTtlSeconds = 3600;              // 1 hour
    bool enableStats = true;
    bool enableLru = true;                     // Use LRU eviction
    int cleanupIntervalSeconds = 60;           // Cleanup interval
};

// =============================================================================
// Cache Statistics
// =============================================================================

struct CacheStats {
    std::atomic<long long> hits{0};
    std::atomic<long long> misses{0};
    std::atomic<long long> evictions{0};
    std::atomic<long long> expirations{0};
    std::atomic<long long> writes{0};
    std::atomic<long long> deletes{0};
    std::atomic<size_t> currentEntries{0};
    std::atomic<size_t> currentSizeBytes{0};
    
    CacheStats() = default;
    
    // Explicit copy constructor: load atomics as snapshots
    CacheStats(const CacheStats& other)
        : hits(other.hits.load()),
          misses(other.misses.load()),
          evictions(other.evictions.load()),
          expirations(other.expirations.load()),
          writes(other.writes.load()),
          deletes(other.deletes.load()),
          currentEntries(other.currentEntries.load()),
          currentSizeBytes(other.currentSizeBytes.load()) {}
    
    CacheStats& operator=(const CacheStats& other) {
        hits.store(other.hits.load());
        misses.store(other.misses.load());
        evictions.store(other.evictions.load());
        expirations.store(other.expirations.load());
        writes.store(other.writes.load());
        deletes.store(other.deletes.load());
        currentEntries.store(other.currentEntries.load());
        currentSizeBytes.store(other.currentSizeBytes.load());
        return *this;
    }

    double getHitRate() const {
        long long total = hits.load() + misses.load();
        if (total == 0) return 0.0;
        return static_cast<double>(hits.load()) / total;
    }
};

// =============================================================================
// LRU Cache Implementation
// =============================================================================

template<typename T>
class LRUCache {
public:
    explicit LRUCache(const CacheConfig& config = CacheConfig()) 
        : m_config(config) {}
    
    // Core operations
    bool set(const std::string& key, const T& value, int ttlSeconds = -1, 
             size_t sizeBytes = 0, const std::string& tags = "");
    std::optional<T> get(const std::string& key);
    bool exists(const std::string& key);
    bool remove(const std::string& key);
    void clear();
    
    // Bulk operations
    std::unordered_map<std::string, T> getMultiple(const std::vector<std::string>& keys);
    void setMultiple(const std::unordered_map<std::string, T>& entries, int ttlSeconds = -1);
    void removeMultiple(const std::vector<std::string>& keys);
    
    // Tag-based operations
    void invalidateByTag(const std::string& tag);
    std::vector<std::string> getKeysByTag(const std::string& tag);
    
    // Pattern matching
    std::vector<std::string> keys(const std::string& pattern = "*");
    
    // Maintenance
    void cleanup();
    void resize(size_t maxEntries, size_t maxSizeBytes);
    
    // Statistics
    CacheStats getStats() const {
        CacheStats s;
        s.hits.store(m_stats.hits.load());
        s.misses.store(m_stats.misses.load());
        s.evictions.store(m_stats.evictions.load());
        s.expirations.store(m_stats.expirations.load());
        s.writes.store(m_stats.writes.load());
        s.deletes.store(m_stats.deletes.load());
        s.currentEntries.store(m_stats.currentEntries.load());
        s.currentSizeBytes.store(m_stats.currentSizeBytes.load());
        return s;
    }
    void resetStats();
    
    // Configuration
    CacheConfig getConfig() const { return m_config; }
    
private:
    using ListIterator = typename std::list<std::string>::iterator;
    
    void evictIfNeeded();
    void evictLru();
    void promoteToFront(const std::string& key);
    bool isExpired(const CacheEntry<T>& entry) const;
    bool matchesPattern(const std::string& key, const std::string& pattern) const;
    
    CacheConfig m_config;
    std::unordered_map<std::string, CacheEntry<T>> m_cache;
    std::unordered_map<std::string, ListIterator> m_keyIterators;
    std::list<std::string> m_lruList;  // Front = most recent
    CacheStats m_stats;
    mutable std::mutex m_mutex;
};

// =============================================================================
// Template Implementation
// =============================================================================

template<typename T>
bool LRUCache<T>::set(const std::string& key, const T& value, int ttlSeconds, 
                       size_t sizeBytes, const std::string& tags) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto now = std::chrono::steady_clock::now();
    int ttl = ttlSeconds >= 0 ? ttlSeconds : m_config.defaultTtlSeconds;
    
    // Check if key exists
    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        // Update existing entry
        m_stats.currentSizeBytes -= it->second.sizeBytes;
        it->second.value = value;
        it->second.expiresAt = now + std::chrono::seconds(ttl);
        it->second.lastAccessed = now;
        it->second.sizeBytes = sizeBytes;
        it->second.tags = tags;
        m_stats.currentSizeBytes += sizeBytes;
        
        promoteToFront(key);
    } else {
        // New entry
        evictIfNeeded();
        
        CacheEntry<T> entry;
        entry.key = key;
        entry.value = value;
        entry.createdAt = now;
        entry.expiresAt = now + std::chrono::seconds(ttl);
        entry.lastAccessed = now;
        entry.sizeBytes = sizeBytes;
        entry.tags = tags;
        
        m_cache[key] = std::move(entry);
        m_lruList.push_front(key);
        m_keyIterators[key] = m_lruList.begin();
        
        m_stats.currentEntries++;
        m_stats.currentSizeBytes += sizeBytes;
    }
    
    m_stats.writes++;
    return true;
}

template<typename T>
std::optional<T> LRUCache<T>::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_cache.find(key);
    if (it == m_cache.end()) {
        m_stats.misses++;
        return std::nullopt;
    }
    
    // Check expiration
    if (isExpired(it->second)) {
        // Remove expired entry
        m_lruList.erase(m_keyIterators[key]);
        m_keyIterators.erase(key);
        m_stats.currentSizeBytes -= it->second.sizeBytes;
        m_cache.erase(it);
        m_stats.currentEntries--;
        m_stats.expirations++;
        m_stats.misses++;
        return std::nullopt;
    }
    
    // Update access info
    it->second.lastAccessed = std::chrono::steady_clock::now();
    it->second.hitCount++;
    promoteToFront(key);
    
    m_stats.hits++;
    return it->second.value;
}

template<typename T>
bool LRUCache<T>::exists(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_cache.find(key);
    if (it == m_cache.end()) {
        return false;
    }
    
    return !isExpired(it->second);
}

template<typename T>
bool LRUCache<T>::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_cache.find(key);
    if (it == m_cache.end()) {
        return false;
    }
    
    m_lruList.erase(m_keyIterators[key]);
    m_keyIterators.erase(key);
    m_stats.currentSizeBytes -= it->second.sizeBytes;
    m_cache.erase(it);
    m_stats.currentEntries--;
    m_stats.deletes++;
    
    return true;
}

template<typename T>
void LRUCache<T>::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_cache.clear();
    m_lruList.clear();
    m_keyIterators.clear();
    m_stats.currentEntries = 0;
    m_stats.currentSizeBytes = 0;
}

template<typename T>
std::unordered_map<std::string, T> LRUCache<T>::getMultiple(const std::vector<std::string>& keys) {
    std::unordered_map<std::string, T> result;
    for (const auto& key : keys) {
        auto value = get(key);
        if (value) {
            result[key] = *value;
        }
    }
    return result;
}

template<typename T>
void LRUCache<T>::setMultiple(const std::unordered_map<std::string, T>& entries, int ttlSeconds) {
    for (const auto& [key, value] : entries) {
        set(key, value, ttlSeconds);
    }
}

template<typename T>
void LRUCache<T>::removeMultiple(const std::vector<std::string>& keys) {
    for (const auto& key : keys) {
        remove(key);
    }
}

template<typename T>
void LRUCache<T>::invalidateByTag(const std::string& tag) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::string> toRemove;
    for (const auto& [key, entry] : m_cache) {
        if (entry.tags.find(tag) != std::string::npos) {
            toRemove.push_back(key);
        }
    }
    
    for (const auto& key : toRemove) {
        auto it = m_cache.find(key);
        if (it != m_cache.end()) {
            m_lruList.erase(m_keyIterators[key]);
            m_keyIterators.erase(key);
            m_stats.currentSizeBytes -= it->second.sizeBytes;
            m_cache.erase(it);
            m_stats.currentEntries--;
        }
    }
}

template<typename T>
std::vector<std::string> LRUCache<T>::getKeysByTag(const std::string& tag) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::string> result;
    for (const auto& [key, entry] : m_cache) {
        if (entry.tags.find(tag) != std::string::npos && !isExpired(entry)) {
            result.push_back(key);
        }
    }
    return result;
}

template<typename T>
std::vector<std::string> LRUCache<T>::keys(const std::string& pattern) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::string> result;
    for (const auto& [key, entry] : m_cache) {
        if (matchesPattern(key, pattern) && !isExpired(entry)) {
            result.push_back(key);
        }
    }
    return result;
}

template<typename T>
void LRUCache<T>::cleanup() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto now = std::chrono::steady_clock::now();
    std::vector<std::string> expired;
    
    for (const auto& [key, entry] : m_cache) {
        if (now >= entry.expiresAt) {
            expired.push_back(key);
        }
    }
    
    for (const auto& key : expired) {
        auto it = m_cache.find(key);
        if (it != m_cache.end()) {
            m_lruList.erase(m_keyIterators[key]);
            m_keyIterators.erase(key);
            m_stats.currentSizeBytes -= it->second.sizeBytes;
            m_cache.erase(it);
            m_stats.currentEntries--;
            m_stats.expirations++;
        }
    }
}

template<typename T>
void LRUCache<T>::resize(size_t maxEntries, size_t maxSizeBytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_config.maxEntries = maxEntries;
    m_config.maxSizeBytes = maxSizeBytes;
    
    while (m_stats.currentEntries > maxEntries || m_stats.currentSizeBytes > maxSizeBytes) {
        evictLru();
    }
}

template<typename T>
void LRUCache<T>::resetStats() {
    m_stats.hits = 0;
    m_stats.misses = 0;
    m_stats.evictions = 0;
    m_stats.expirations = 0;
    m_stats.writes = 0;
    m_stats.deletes = 0;
}

template<typename T>
void LRUCache<T>::evictIfNeeded() {
    while (m_stats.currentEntries >= m_config.maxEntries || 
           m_stats.currentSizeBytes >= m_config.maxSizeBytes) {
        evictLru();
    }
}

template<typename T>
void LRUCache<T>::evictLru() {
    if (m_lruList.empty()) return;
    
    // Evict from back (least recently used)
    std::string key = m_lruList.back();
    m_lruList.pop_back();
    
    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        m_stats.currentSizeBytes -= it->second.sizeBytes;
        m_cache.erase(it);
        m_keyIterators.erase(key);
        m_stats.currentEntries--;
        m_stats.evictions++;
    }
}

template<typename T>
void LRUCache<T>::promoteToFront(const std::string& key) {
    if (!m_config.enableLru) return;
    
    auto it = m_keyIterators.find(key);
    if (it != m_keyIterators.end()) {
        m_lruList.erase(it->second);
        m_lruList.push_front(key);
        it->second = m_lruList.begin();
    }
}

template<typename T>
bool LRUCache<T>::isExpired(const CacheEntry<T>& entry) const {
    return std::chrono::steady_clock::now() >= entry.expiresAt;
}

template<typename T>
bool LRUCache<T>::matchesPattern(const std::string& key, const std::string& pattern) const {
    if (pattern == "*") return true;
    
    // Simple wildcard matching
    if (pattern.back() == '*') {
        std::string prefix = pattern.substr(0, pattern.length() - 1);
        return key.find(prefix) == 0;
    }
    if (pattern.front() == '*') {
        std::string suffix = pattern.substr(1);
        return key.length() >= suffix.length() && 
               key.compare(key.length() - suffix.length(), suffix.length(), suffix) == 0;
    }
    
    return key == pattern;
}

// =============================================================================
// Multi-Tier Cache
// =============================================================================

class MultiTierCache {
public:
    enum class Tier { L1_MEMORY, L2_SHARED, L3_DISTRIBUTED };
    
    MultiTierCache();
    
    // Set with tier specification
    bool set(const std::string& key, const std::string& value, int ttlSeconds = -1,
             Tier maxTier = Tier::L1_MEMORY);
    
    // Get searches all tiers
    std::optional<std::string> get(const std::string& key);
    
    // Remove from all tiers
    bool remove(const std::string& key);
    
    // Clear specific tier
    void clear(Tier tier);
    void clearAll();
    
    // Get tier-specific cache
    LRUCache<std::string>& getL1() { return m_l1; }
    
    // Statistics
    struct TierStats {
        CacheStats l1;
        CacheStats l2;
        CacheStats l3;
    };
    TierStats getStats() const;
    
private:
    LRUCache<std::string> m_l1;  // In-process memory cache
    LRUCache<std::string> m_l2;  // Shared memory (simulated)
    LRUCache<std::string> m_l3;  // Distributed cache (simulated)
};

// =============================================================================
// Cache Manager Singleton
// =============================================================================

class CacheManager {
public:
    static CacheManager& instance();
    
    // Named cache management
    template<typename T>
    LRUCache<T>& getCache(const std::string& name);
    
    void removeCache(const std::string& name);
    std::vector<std::string> listCaches() const;
    
    // Global operations
    void clearAll();
    void cleanupAll();
    
    // Default string cache
    LRUCache<std::string>& defaultCache() { return m_defaultCache; }
    
    // Multi-tier cache
    MultiTierCache& multiTier() { return m_multiTier; }
    
private:
    CacheManager();
    
    LRUCache<std::string> m_defaultCache;
    MultiTierCache m_multiTier;
    std::unordered_map<std::string, std::any> m_namedCaches;
    mutable std::mutex m_mutex;
};

template<typename T>
LRUCache<T>& CacheManager::getCache(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_namedCaches.find(name);
    if (it == m_namedCaches.end()) {
        m_namedCaches[name] = std::make_shared<LRUCache<T>>();
    }
    
    return *std::any_cast<std::shared_ptr<LRUCache<T>>>(m_namedCaches[name]);
}

} // namespace enterprise
