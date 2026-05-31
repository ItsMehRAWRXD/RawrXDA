#ifndef KV_CACHE_OPTIMIZER_H
#define KV_CACHE_OPTIMIZER_H

#include <vector>
#include <chrono>
#include <cstdio>
#include <cstdint>

// KV-cache optimizer – LRU/LFU hybrid eviction with sliding-window and
// frequency-based retention scoring for long-context inference.
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

    // Mark a token range as recently attended (boosts retention score)
    void touchRange(int startIdx, int count);

    // Get all cached tokens
    std::vector<int> getCachedTokens() const;

    // Get cache statistics
    int getCacheSize() const { return static_cast<int>(m_cachedTokens.size()); }
    int getCacheSizeLimit() const { return m_cacheSizeLimit; }
    uint64_t totalEvictions() const { return m_totalEvictions; }
    double   avgRetentionScore() const;

    // Callback hooks (replacing Qt signals)
    void (*onCacheEvicted)(int tokensEvicted) = nullptr;
    void (*onCacheUpdated)(int totalTokens) = nullptr;

private:
    // Per-token metadata for eviction scoring
    struct TokenMeta {
        uint32_t accessCount = 1;       // how many times this token was attended
        uint32_t insertionAge = 0;      // monotonic counter at insertion time
    };

    void evictIfNeeded();
    float retentionScore(int idx) const;

    std::vector<int>       m_cachedTokens;
    std::vector<TokenMeta> m_tokenMeta;
    int                    m_cacheSizeLimit;
    int                    m_slidingWindowSize;
    uint32_t               m_ageClock = 0;           // monotonic insertion counter
    uint64_t               m_totalEvictions = 0;

    std::chrono::steady_clock::time_point m_lastAccessTime;
    bool m_gpuCacheInitialized;
};

#endif // KV_CACHE_OPTIMIZER_H

