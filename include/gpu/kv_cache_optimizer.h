#ifndef KV_CACHE_OPTIMIZER_H
#define KV_CACHE_OPTIMIZER_H

#include <QObject>
#include <QMap>
#include <QVariant>
#include <QDateTime>

// KV-cache optimizer – dynamic sliding-window, cache eviction when context > 32 k.
class KVCacheOptimizer : public QObject
{
    Q_OBJECT

public:
    explicit KVCacheOptimizer(QObject *parent = nullptr);
    ~KVCacheOptimizer();

    // Set cache size limit (in tokens)
    void setCacheSizeLimit(int limit);

    // Add tokens to cache
    void addTokens(const QList<int> &tokens);

    // Get cached tokens
    QList<int> getCachedTokens() const;

    // Evict tokens if cache is full
    void evictIfNeeded();

    // Set sliding window size
    void setSlidingWindowSize(int size);

private:
    int m_cacheSizeLimit;
    int m_slidingWindowSize;
    QList<int> m_cachedTokens;
    QDateTime m_lastAccessTime;
};

#endif // KV_CACHE_OPTIMIZER_H