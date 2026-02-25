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

    // Add tokens to cache
    void addTokens(const std::vector<int> &tokens);

    // Get cached tokens
    std::vector<int> getCachedTokens() const;

    // Get cache statistics
    int getCacheSize() const { return static_cast<int>(m_cachedTokens.size()); }
    int getCacheSizeLimit() const { return m_cacheSizeLimit; }

    // Set sliding window size
    void setSlidingWindowSize(int size);

    // Callback hooks (replacing Qt signals)
    void (*onCacheEvicted)(int tokensEvicted) = nullptr;
    void (*onCacheUpdated)(int totalTokens) = nullptr;

private:
    void evictIfNeeded();

    int m_cacheSizeLimit;
    int m_slidingWindowSize;
    std::vector<int> m_cachedTokens;
    std::chrono::steady_clock::time_point m_lastAccessTime;
    bool m_gpuCacheInitialized;
};

#endif // KV_CACHE_OPTIMIZER_H