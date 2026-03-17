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

#include "../include/license_enforcement.h"

namespace RawrXD::Cache {

struct CacheEntry {
    std::string response;
    time_t timestamp;
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
        if (!RawrXD::Enforce::LicenseEnforcer::Instance().allow(
                RawrXD::License::FeatureID::ResponseCaching, __FUNCTION__)) {
            misses++;
            return false;  // Cache disabled
        }
        
        licensed = true;
        
        auto it = cache.find(prompt);
        if (it != cache.end() && !isExpired(it->second)) {
            response = it->second.response;
            it->second.accessCount++;
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
        if (!RawrXD::Enforce::LicenseEnforcer::Instance().allow(
                RawrXD::License::FeatureID::ResponseCaching, __FUNCTION__)) {
            return;  // Don't cache if not licensed
        }
        
        licensed = true;
        
        CacheEntry entry;
        entry.response = response;
        entry.timestamp = std::time(nullptr);
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
        
        // Find oldest accessed entry
        std::string oldestKey;
        uint64_t minAccess = UINT64_MAX;
        
        for (auto& kv : cache) {
            if (kv.second.accessCount < minAccess) {
                minAccess = kv.second.accessCount;
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
