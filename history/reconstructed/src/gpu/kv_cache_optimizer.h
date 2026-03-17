#ifndef KV_CACHE_OPTIMIZER_H
#define KV_CACHE_OPTIMIZER_H

#include <vector>
#include <chrono>
#include <cstdio>

// KV-cache optimizer – dynamic sliding-window, cache eviction when context > 32 k.
class KVCacheOptimizer
{
public:
    explicit KVCacheOptimizer();
    ~KVCacheOptimizer();

    // Set cache size limit (in tokens)
    void setCacheSizeLimit(int limit);

    // Set sliding window size for eviction strategy
    void setSlidingWindowSize(int size);

    // Add tokens to the cache (uses GPU acceleration if available)
    void addTokens(const std::vector<int> &tokens);

    // Get all cached tokens
    std::vector<int> getCachedTokens() const;

    // Get cache statistics
    int getCacheSize() const { return static_cast<int>(m_cachedTokens.size()); }
    int getCacheSizeLimit() const { return m_cacheSizeLimit; }

    // Callback hooks (replacing Qt signals)
    void (*onCacheEvicted)(int tokensEvicted) = nullptr;
    void (*onCacheUpdated)(int totalTokens) = nullptr;

private:
    void evictIfNeeded();

    std::vector<int> m_cachedTokens;
    int m_cacheSizeLimit;
    int m_slidingWindowSize;
    std::chrono::steady_clock::time_point m_lastAccessTime;
    bool m_gpuCacheInitialized;
};

#endif // KV_CACHE_OPTIMIZER_H

