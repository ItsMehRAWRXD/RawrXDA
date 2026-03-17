#pragma once

#include <QObject>
#include <QHash>
#include <QTimer>
#include <QDateTime>
#include <memory>
#include "ModelLoaderBridge.h"

class ModelCacheManager : public QObject {
    Q_OBJECT
public:
    explicit ModelCacheManager(ModelLoaderBridge* loader, QObject* parent = nullptr);
    
    // Smart model preloading based on usage patterns
    void preloadModel(const QString& modelPath);
    void releaseModel(const QString& modelName);
    
    // LRU cache eviction
    void setMaxCacheSizeGB(size_t gb);
    size_t getCurrentCacheSize() const;
    
    // Prefetch for agentic tasks
    void prefetchForTask(const QString& taskType);

private slots:
    void onModelLoaded(const QString& name, uint64_t size);
    void onModelUnloaded(const QString& name);
    void onEvictionTimer();

private:
    struct CacheEntry {
        QString path;
        size_t sizeBytes;
        QDateTime lastAccess;
        void* handle;
    };
    
    ModelLoaderBridge* m_loader;
    QHash<QString, CacheEntry> m_cache;
    size_t m_maxCacheSizeBytes = 32ULL * 1024 * 1024 * 1024; // 32GB default
    QTimer m_evictionTimer;
};
