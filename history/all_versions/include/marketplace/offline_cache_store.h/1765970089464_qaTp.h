#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>

/**
 * @class OfflineCacheStore
 * @brief Manages offline caching of extension marketplace data
 * 
 * This class handles:
 * - Caching extension search results
 * - Caching extension details
 * - Managing cache size and expiration
 * - Loading cached data when offline
 * - Air-gapped bundle loading
 */
class OfflineCacheStore : public QObject {
    Q_OBJECT

public:
    explicit OfflineCacheStore(QObject* parent = nullptr);
    ~OfflineCacheStore();

    // Cache management
    void cacheSearchResults(const QString& query, const QJsonArray& results);
    void cacheExtensionDetails(const QString& extensionId, const QJsonObject& details);
    void cacheExtensionBundle(const QString& extensionId, const QString& bundlePath);
    
    // Cache retrieval
    QJsonArray getCachedSearchResults(const QString& query);
    QJsonObject getCachedExtensionDetails(const QString& extensionId);
    QString getCachedExtensionBundle(const QString& extensionId);
    
    // Cache maintenance
    void clearCache();
    void setCacheSizeLimit(qint64 bytes);
    void setCacheExpiration(int days);
    void cleanupExpiredEntries();
    
    // Air-gapped deployment
    bool loadAirGappedBundle(const QString& bundlePath);
    bool exportExtensionBundle(const QString& extensionId, const QString& outputPath);
    QStringList listAvailableBundles();

signals:
    void cacheCleared();
    void cacheSizeChanged(qint64 size);
    void bundleLoaded(const QString& extensionId);
    void bundleExported(const QString& extensionId, const QString& outputPath);

private:
    struct CacheEntry {
        QString key;
        QString filePath;
        qint64 size;
        qint64 timestamp;
    };

    QDir m_cacheDir;
    qint64 m_cacheSizeLimit;
    int m_cacheExpirationDays;
    QList<CacheEntry> m_cacheEntries;

    bool initializeCacheDirectory();
    QString getCacheFilePath(const QString& key);
    void updateCacheSize();
    void removeExpiredEntries();
    qint64 getDirectorySize(const QDir& dir);
    bool compressFile(const QString& inputPath, const QString& outputPath);
    bool decompressFile(const QString& inputPath, const QString& outputPath);
    QString hashKey(const QString& key);
};