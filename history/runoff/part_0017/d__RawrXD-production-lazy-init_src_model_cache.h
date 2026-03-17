#ifndef MODEL_CACHE_H
#define MODEL_CACHE_H

#include <QString>
#include <QHash>
#include <QDateTime>
#include <QMutex>
#include <QSharedPointer>
#include <QByteArray>
#include <QFile>
#include <QCryptographicHash>

namespace RawrXD {

struct ModelCacheEntry {
    QString modelId;
    QString filePath;
    QByteArray data;
    QDateTime lastAccessed;
    QDateTime created;
    qint64 size;
    QString checksum;
    bool compressed;
    bool valid;
    
    ModelCacheEntry() : size(0), compressed(false), valid(false) {}
    ModelCacheEntry(const QString& id, const QString& path, const QByteArray& modelData)
        : modelId(id), filePath(path), data(modelData), lastAccessed(QDateTime::currentDateTime())
        , created(QDateTime::currentDateTime()), size(modelData.size()), compressed(false), valid(true) {
        updateChecksum();
    }
    
    void updateChecksum() {
        QCryptographicHash hash(QCryptographicHash::Sha256);
        hash.addData(data);
        checksum = QString(hash.result().toHex());
    }
    
    bool isValid() const {
        if (!valid) return false;
        
        // Check file existence if path is provided
        if (!filePath.isEmpty() && !QFile::exists(filePath)) {
            return false;
        }
        
        // Verify checksum if data is loaded
        if (!data.isEmpty()) {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            hash.addData(data);
            QString currentChecksum = QString(hash.result().toHex());
            return currentChecksum == checksum;
        }
        
        return true;
    }
    
    qint64 getAgeSeconds() const {
        return created.secsTo(QDateTime::currentDateTime());
    }
    
    void touch() {
        lastAccessed = QDateTime::currentDateTime();
    }
};

class ModelCache {
public:
    static ModelCache& instance();
    
    void initialize(const QString& cacheDir = QString(), qint64 maxSizeGB = 50);
    void shutdown();
    
    // Cache operations
    bool storeModel(const QString& modelId, const QByteArray& modelData, bool compress = true);
    QSharedPointer<ModelCacheEntry> getModel(const QString& modelId);
    bool removeModel(const QString& modelId);
    bool containsModel(const QString& modelId);
    
    // Cache management
    void cleanup(); // Remove expired/invalid entries
    void compressCache(); // Compress large entries
    qint64 getCurrentSize() const;
    qint64 getMaxSize() const;
    int getEntryCount() const;
    
    // Validation
    bool validateCache();
    bool validateModel(const QString& modelId);
    
    // Persistence
    bool saveCacheIndex();
    bool loadCacheIndex();
    
private:
    ModelCache() = default;
    ~ModelCache();
    
    QString getCacheFilePath(const QString& modelId);
    bool storeToFile(const QString& filePath, const QByteArray& data, bool compress);
    QByteArray loadFromFile(const QString& filePath, bool decompress);
    QByteArray compressData(const QByteArray& data);
    QByteArray decompressData(const QByteArray& data);
    
    void evictLRU(); // Evict least recently used entries
    bool isCacheFull() const;
    
    QString cacheDir_;
    qint64 maxSizeBytes_;
    QHash<QString, QSharedPointer<ModelCacheEntry>> cache_;
    mutable QMutex mutex_;
    bool initialized_ = false;
    
    static const qint64 DEFAULT_MAX_SIZE = 50LL * 1024 * 1024 * 1024; // 50GB
    static const int TTL_HOURS = 24;
};

// Convenience macros
#define MODEL_CACHE_STORE(id, data) RawrXD::ModelCache::instance().storeModel(id, data)
#define MODEL_CACHE_GET(id) RawrXD::ModelCache::instance().getModel(id)
#define MODEL_CACHE_REMOVE(id) RawrXD::ModelCache::instance().removeModel(id)
#define MODEL_CACHE_CONTAINS(id) RawrXD::ModelCache::instance().containsModel(id)

} // namespace RawrXD

#endif // MODEL_CACHE_H