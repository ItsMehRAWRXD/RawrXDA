#include "kv_cache_optimizer.h"
#include "logging/logger.h"
#include <algorithm>

static Logger s_kvLogger("KVCache");

KVCacheOptimizer::KVCacheOptimizer()
    : m_cacheSizeLimit(32000)    // Default limit: 32k tokens
    , m_slidingWindowSize(1000)  // Default sliding window size
    , m_lastAccessTime{}
    , m_gpuCacheInitialized(false)
{
}

KVCacheOptimizer::~KVCacheOptimizer()
{
}

void KVCacheOptimizer::setCacheSizeLimit(int limit)
{
    m_cacheSizeLimit = limit;
}

void KVCacheOptimizer::addTokens(const std::vector<int> &tokens)
{
    m_cachedTokens.insert(m_cachedTokens.end(), tokens.begin(), tokens.end());
    evictIfNeeded();
    m_lastAccessTime = std::chrono::steady_clock::now();

    if (onCacheUpdated) {
        onCacheUpdated(static_cast<int>(m_cachedTokens.size()));
    }
}

std::vector<int> KVCacheOptimizer::getCachedTokens() const
{
    return m_cachedTokens;
}

void KVCacheOptimizer::evictIfNeeded()
{
    if (static_cast<int>(m_cachedTokens.size()) > m_cacheSizeLimit) {
        // Implement dynamic sliding-window eviction
        int tokensToEvict = static_cast<int>(m_cachedTokens.size()) - m_cacheSizeLimit;
        if (tokensToEvict > 0) {
            // Remove tokens from the beginning (oldest tokens)
            m_cachedTokens.erase(m_cachedTokens.begin(), m_cachedTokens.begin() + tokensToEvict);
            s_kvLogger.info("Evicted {} tokens from KV cache", tokensToEvict);

            if (onCacheEvicted) {
                onCacheEvicted(tokensToEvict);
            }
        }
    }
}

void KVCacheOptimizer::setSlidingWindowSize(int size)
{
    m_slidingWindowSize = size;
}