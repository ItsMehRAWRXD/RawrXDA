#include "kv_cache_optimizer.h"
#include <QDebug>

KVCacheOptimizer::KVCacheOptimizer(QObject *parent)
    : QObject(parent)
    , m_cacheSizeLimit(32000) // Default limit: 32k tokens
    , m_slidingWindowSize(1000) // Default sliding window size
{
}

KVCacheOptimizer::~KVCacheOptimizer()
{
}

void KVCacheOptimizer::setCacheSizeLimit(int limit)
{
    m_cacheSizeLimit = limit;
}

void KVCacheOptimizer::addTokens(const QList<int> &tokens)
{
    m_cachedTokens.append(tokens);
    evictIfNeeded();
    m_lastAccessTime = QDateTime::currentDateTime();
}

QList<int> KVCacheOptimizer::getCachedTokens() const
{
    return m_cachedTokens;
}

void KVCacheOptimizer::evictIfNeeded()
{
    if (m_cachedTokens.size() > m_cacheSizeLimit) {
        // Implement dynamic sliding-window eviction
        int tokensToEvict = m_cachedTokens.size() - m_cacheSizeLimit;
        if (tokensToEvict > 0) {
            // Remove tokens from the beginning (oldest tokens)
            m_cachedTokens.erase(m_cachedTokens.begin(), m_cachedTokens.begin() + tokensToEvict);
            qDebug() << "Evicted" << tokensToEvict << "tokens from KV cache";
        }
    }
}

void KVCacheOptimizer::setSlidingWindowSize(int size)
{
    m_slidingWindowSize = size;
}