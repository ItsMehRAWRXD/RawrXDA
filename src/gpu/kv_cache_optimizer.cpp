#include "kv_cache_optimizer.h"
#include <cstdio>
#include <algorithm>
#include <numeric>
#include <cmath>

// ============================================================================
// KV Cache Optimizer — LRU/LFU hybrid eviction with frequency scoring
// ============================================================================
// Eviction policy: each token has a retention score combining:
//   - recency:   newer tokens score higher (linear decay from age)
//   - frequency: tokens attended more often score higher (log scaling)
//   - locality:  tokens in the sliding-window tail are always kept
// Tokens with the lowest retention scores are evicted first.
// ============================================================================

KVCacheOptimizer::KVCacheOptimizer()
    : m_cacheSizeLimit(32000)
    , m_slidingWindowSize(1000)
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

void KVCacheOptimizer::setSlidingWindowSize(int size)
{
    m_slidingWindowSize = size;
}

void KVCacheOptimizer::addTokens(const std::vector<int> &tokens)
{
    m_cachedTokens.insert(m_cachedTokens.end(), tokens.begin(), tokens.end());

    // Initialize metadata for new tokens
    for (size_t i = 0; i < tokens.size(); ++i) {
        TokenMeta meta;
        meta.accessCount  = 1;
        meta.insertionAge = m_ageClock++;
        m_tokenMeta.push_back(meta);
    }

    evictIfNeeded();
    m_lastAccessTime = std::chrono::steady_clock::now();

    if (onCacheUpdated) {
        onCacheUpdated(static_cast<int>(m_cachedTokens.size()));
    }
}

void KVCacheOptimizer::touchRange(int startIdx, int count)
{
    int sz = static_cast<int>(m_tokenMeta.size());
    if (startIdx < 0) startIdx = 0;
    int end = startIdx + count;
    if (end > sz) end = sz;
    for (int i = startIdx; i < end; ++i)
        m_tokenMeta[i].accessCount++;
}

std::vector<int> KVCacheOptimizer::getCachedTokens() const
{
    return m_cachedTokens;
}

// Retention score: higher = more worth keeping
float KVCacheOptimizer::retentionScore(int idx) const
{
    const auto& meta = m_tokenMeta[idx];
    int sz = static_cast<int>(m_cachedTokens.size());

    // Recency component: linear [0..1], newest = 1.0
    float recency = (m_ageClock > 0)
        ? static_cast<float>(meta.insertionAge) / static_cast<float>(m_ageClock)
        : 0.0f;

    // Frequency component: log-scaled access count [0..~1]
    float frequency = std::log2f(static_cast<float>(meta.accessCount + 1)) / 10.0f;

    // Locality boost: tokens in the tail sliding window get a large bonus
    int distFromEnd = sz - 1 - idx;
    float locality = (distFromEnd < m_slidingWindowSize) ? 1.0f : 0.0f;

    // Weighted combination: locality dominates, then recency, then frequency
    return locality * 5.0f + recency * 2.0f + frequency * 1.0f;
}

void KVCacheOptimizer::evictIfNeeded()
{
    int sz = static_cast<int>(m_cachedTokens.size());
    if (sz <= m_cacheSizeLimit) return;

    int tokensToEvict = sz - m_cacheSizeLimit;

    // Build score index for the eviction-eligible region (everything
    // outside the sliding window tail)
    int protectedStart = sz - m_slidingWindowSize;
    if (protectedStart < 0) protectedStart = 0;
    int eligibleCount = protectedStart;

    if (eligibleCount <= tokensToEvict) {
        // Must evict entire eligible region — just trim from the front
        m_cachedTokens.erase(m_cachedTokens.begin(),
                             m_cachedTokens.begin() + tokensToEvict);
        m_tokenMeta.erase(m_tokenMeta.begin(),
                          m_tokenMeta.begin() + tokensToEvict);
    } else {
        // Score-based eviction: find the lowest-scored tokens
        std::vector<int> indices(eligibleCount);
        std::iota(indices.begin(), indices.end(), 0);

        // Partial sort to find the tokensToEvict lowest scores
        std::partial_sort(indices.begin(),
                          indices.begin() + tokensToEvict,
                          indices.end(),
                          [&](int a, int b) {
                              return retentionScore(a) < retentionScore(b);
                          });

        // Mark for removal (sort indices in descending order to erase safely)
        std::sort(indices.begin(), indices.begin() + tokensToEvict,
                  std::greater<int>());
        for (int i = 0; i < tokensToEvict; ++i) {
            int idx = indices[i];
            m_cachedTokens.erase(m_cachedTokens.begin() + idx);
            m_tokenMeta.erase(m_tokenMeta.begin() + idx);
        }
    }

    m_totalEvictions += tokensToEvict;

    fprintf(stderr, "[KVCache] Evicted %d tokens (scored), cache=%d/%d\n",
            tokensToEvict, static_cast<int>(m_cachedTokens.size()), m_cacheSizeLimit);

    if (onCacheEvicted) {
        onCacheEvicted(tokensToEvict);
    }
}

double KVCacheOptimizer::avgRetentionScore() const
{
    if (m_tokenMeta.empty()) return 0.0;
    double sum = 0.0;
    for (int i = 0; i < static_cast<int>(m_tokenMeta.size()); ++i)
        sum += retentionScore(i);
    return sum / static_cast<double>(m_tokenMeta.size());
}
