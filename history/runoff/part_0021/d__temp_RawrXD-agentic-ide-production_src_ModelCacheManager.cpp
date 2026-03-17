#include "ModelCacheManager.h"
#include <QDebug>
#include <QDateTime>
#include <algorithm>

ModelCacheManager::ModelCacheManager(ModelLoaderBridge* loader, QObject* parent)
    : QObject(parent), m_loader(loader) {
    
    connect(loader, &ModelLoaderBridge::modelLoaded, 
            this, &ModelCacheManager::onModelLoaded);
    connect(loader, &ModelLoaderBridge::modelUnloaded,
            this, &ModelCacheManager::onModelUnloaded);
    
    m_evictionTimer.setInterval(30000); // 30 seconds
    connect(&m_evictionTimer, &QTimer::timeout,
            this, &ModelCacheManager::onEvictionTimer);
    m_evictionTimer.start();
}

void ModelCacheManager::preloadModel(const QString& path) {
    if (m_cache.contains(path)) {
        m_cache[path].lastAccess = QDateTime::currentDateTime();
        return;
    }
    
    m_loader->loadModelAsync(path);
}

void ModelCacheManager::releaseModel(const QString& modelName) {
    m_loader->unloadModel(modelName);
}

void ModelCacheManager::setMaxCacheSizeGB(size_t gb) {
    m_maxCacheSizeBytes = gb * 1024ULL * 1024 * 1024;
    
    if (getCurrentCacheSize() > m_maxCacheSizeBytes) {
        onEvictionTimer(); // Immediate eviction
    }
}

size_t ModelCacheManager::getCurrentCacheSize() const {
    size_t total = 0;
    for (const auto& entry : m_cache) {
        total += entry.sizeBytes;
    }
    return total;
}

void ModelCacheManager::prefetchForTask(const QString& taskType) {
    // Task-specific model recommendations
    QString modelPath;
    
    if (taskType == "code_completion") {
        modelPath = "phi-3-mini.gguf";
    } else if (taskType == "chat") {
        modelPath = "dolphin3.gguf";
    } else if (taskType == "test_gen") {
        modelPath = "qwen3-coder.gguf";
    } else {
        return;
    }
    
    preloadModel(modelPath);
}

void ModelCacheManager::onModelLoaded(const QString& name, uint64_t size) {
    CacheEntry entry;
    entry.path = name;
    entry.sizeBytes = size;
    entry.lastAccess = QDateTime::currentDateTime();
    entry.handle = nullptr;
    
    m_cache[name] = entry;
    qInfo() << "Cache: Added" << name << "(" << (size / 1024 / 1024) << "MB)";
}

void ModelCacheManager::onModelUnloaded(const QString& name) {
    m_cache.remove(name);
    qInfo() << "Cache: Removed" << name;
}

void ModelCacheManager::onEvictionTimer() {
    size_t currentSize = getCurrentCacheSize();
    if (currentSize <= m_maxCacheSizeBytes) return;
    
    // Sort by lastAccess (least recently used)
    QList<QString> paths = m_cache.keys();
    std::sort(paths.begin(), paths.end(), [&](const QString& a, const QString& b) {
        return m_cache[a].lastAccess < m_cache[b].lastAccess;
    });
    
    // Evict LRU models until under limit
    for (const QString& path : paths) {
        if (currentSize <= m_maxCacheSizeBytes) break;
        
        qInfo() << "Evicting model from cache:" << path;
        releaseModel(path);
        currentSize -= m_cache[path].sizeBytes;
        m_cache.remove(path);
    }
}
