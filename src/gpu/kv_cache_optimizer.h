#ifndef KV_CACHE_OPTIMIZER_H
#define KV_CACHE_OPTIMIZER_H

class KVCacheOptimizer 
{public:
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
    int getCacheSize() const { return m_cachedTokens.size(); }
    int getCacheSizeLimit() const { return m_cacheSizeLimit; }
    // DateTime getLastAccessTime() const { return m_lastAccessTime; }
\npublic:\n    void cacheEvicted(int tokensEvicted);
    void cacheUpdated(int totalTokens);

private:
    void evictIfNeeded();

    std::vector<int> m_cachedTokens;
    int m_cacheSizeLimit;
    int m_slidingWindowSize;
    // DateTime m_lastAccessTime;
    bool m_gpuCacheInitialized;
};

#endif // KV_CACHE_OPTIMIZER_H

