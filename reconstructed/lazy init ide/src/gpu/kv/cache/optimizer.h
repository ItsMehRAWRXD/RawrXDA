#ifndef KV_CACHE_OPTIMIZER_H
#define KV_CACHE_OPTIMIZER_H

#include <QObject>
#include <QList>
#include <QDateTime>
#include <QVector>

class KVCacheOptimizer : public QObject
{
    Q_OBJECT

public:
    explicit KVCacheOptimizer(QObject *parent = nullptr);
    ~KVCacheOptimizer();

    // Set cache size limit (in tokens)
    void setCacheSizeLimit(int limit);
    
    // Set sliding window size for eviction strategy
    void setSlidingWindowSize(int size);
    
    // Add tokens to the cache (uses GPU acceleration if available)
    void addTokens(const QList<int> &tokens);
    
    // Get all cached tokens
    QList<int> getCachedTokens() const;
    
    // Get cache statistics
    int getCacheSize() const { return m_cachedTokens.size(); }
    int getCacheSizeLimit() const { return m_cacheSizeLimit; }
    QDateTime getLastAccessTime() const { return m_lastAccessTime; }

signals:
    void cacheEvicted(int tokensEvicted);
    void cacheUpdated(int totalTokens);

private:
    void evictIfNeeded();

    QList<int> m_cachedTokens;
    int m_cacheSizeLimit;
    int m_slidingWindowSize;
    QDateTime m_lastAccessTime;
    bool m_gpuCacheInitialized;
};

#endif // KV_CACHE_OPTIMIZER_H
