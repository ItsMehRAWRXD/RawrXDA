// ============================================================================
// src/cache/response_cache.cpp — Response Output Caching Layer
// ============================================================================
// Cache model responses for identical prompts with LRU eviction
// Professional feature: FeatureID::ResponseCaching
// ============================================================================

#include <string>
#include <unordered_map>
#include <queue>
#include <ctime>
#include <limits>

// Stub license check for test mode
#ifdef BUILD_CACHE_TEST
#define LICENSE_CHECK(feature) true
#else
#include "../include/license_enforcement.h"
#define LICENSE_CHECK(feature) RawrXD::Enforce::LicenseEnforcer::Instance().allow(feature, __FUNCTION__)
#endif

namespace RawrXD::Cache {

struct CacheEntry {
    std::string response;
    time_t timestamp;       // Creation time (for TTL)
    time_t lastAccessTime;  // Last get time (for LRU eviction)
    uint64_t accessCount;
    size_t sizeBytes;
};

class ResponseCache {
private:
    std::unordered_map<std::string, CacheEntry> cache;
    std::queue<std::string> lruQueue;
    bool licensed;
    size_t maxSizeBytes;
    size_t currentSizeBytes;
    uint64_t hits, misses;
    
public:
    ResponseCache(size_t maxSize = 100 * 1024 * 1024)  // 100 MB default
        : licensed(false), maxSizeBytes(maxSize), currentSizeBytes(0), hits(0), misses(0) {
        
        // License check deferred to get/set operations
    }
    
    ~ResponseCache() {
        cache.clear();
    }
    
    // Retrieve cached response for prompt
    bool get(const std::string& prompt, std::string& response) {
        if (!LICENSE_CHECK(RawrXD::License::FeatureID::ResponseCaching)) {
            misses++;
            return false;  // Cache disabled
        }
        
        licensed = true;
        
        auto it = cache.find(prompt);
        if (it != cache.end() && !isExpired(it->second)) {
            response = it->second.response;
            it->second.accessCount++;
            it->second.lastAccessTime = std::time(nullptr);
            hits++;
            
            printf("[CACHE] Cache HIT: prompt=%.30s... (hits: %llu, misses: %llu)\n",
                   prompt.c_str(), hits, misses);
            
            return true;
        }
        
        misses++;
        return false;
    }
    
    // Store response in cache
    void set(const std::string& prompt, const std::string& response) {
        if (!LICENSE_CHECK(RawrXD::License::FeatureID::ResponseCaching)) {
            return;  // Don't cache if not licensed
        }
        
        licensed = true;
        
        time_t now = std::time(nullptr);
        CacheEntry entry;
        entry.response = response;
        entry.timestamp = now;
        entry.lastAccessTime = now;
        entry.accessCount = 1;
        entry.sizeBytes = response.size();
        
        // Check if need to evict
        if (currentSizeBytes + entry.sizeBytes > maxSizeBytes) {
            evictLRU();
        }
        
        cache[prompt] = entry;
        lruQueue.push(prompt);
        currentSizeBytes += entry.sizeBytes;
        
        printf("[CACHE] Cache SET: prompt=%.30s..., response size=%zu bytes\n",
               prompt.c_str(), entry.sizeBytes);
    }
    
    // Clear cache completely
    void clear() {
        if (!licensed) return;
        
        cache.clear();
        while (!lruQueue.empty()) lruQueue.pop();
        currentSizeBytes = 0;
        
        printf("[CACHE] Cache cleared\n");
    }
    
    // Invalidate entries matching pattern
    void invalidate(const std::string& pattern) {
        if (!licensed) {
            printf("[CACHE] Invalidation denied - feature not licensed\n");
            return;
        }
        
        size_t count = 0;
        for (auto it = cache.begin(); it != cache.end(); ) {
            if (it->first.find(pattern) != std::string::npos) {
                currentSizeBytes -= it->second.sizeBytes;
                it = cache.erase(it);
                count++;
            } else {
                ++it;
            }
        }
        
        printf("[CACHE] Invalidated %zu entries matching pattern\n", count);
    }
    
    // Get cache statistics
    void getStats(uint64_t& hitCount, uint64_t& missCount, double& hitRate) {
        hitCount = hits;
        missCount = misses;
        hitRate = (hits + misses > 0) ? (double)hits / (hits + misses) : 0.0;
    }
    
    size_t getSize() const { return currentSizeBytes; }
    size_t getEntryCount() const { return cache.size(); }
    
private:
    bool isExpired(const CacheEntry& entry, int ttlSeconds = 3600) {
        time_t now = std::time(nullptr);
        return (now - entry.timestamp) > ttlSeconds;
    }
    
    void evictLRU() {
        if (cache.empty()) return;
        
        // Find least recently accessed entry (by lastAccessTime)
        std::string oldestKey;
        time_t minAccessTime = std::numeric_limits<time_t>::max();
        
        for (const auto& kv : cache) {
            if (kv.second.lastAccessTime < minAccessTime) {
                minAccessTime = kv.second.lastAccessTime;
                oldestKey = kv.first;
            }
        }
        
        if (!oldestKey.empty()) {
            currentSizeBytes -= cache[oldestKey].sizeBytes;
            cache.erase(oldestKey);
            printf("[CACHE] LRU eviction: removed entry\n");
        }
    }
};

} // namespace RawrXD::Cache
