#include "marketplace/offline_cache_store.h"
#include <QStandardPaths>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QDirIterator>
#include <QDebug>

OfflineCacheStore::OfflineCacheStore(QObject* parent)
    : QObject(parent)
    , m_cacheSizeLimit(100 * 1024 * 1024) // 100 MB default
    , m_cacheExpirationDays(30) // 30 days default
{
    initializeCacheDirectory();
    updateCacheSize();
    qDebug() << "[OfflineCacheStore] Initialized with cache dir:" << m_cacheDir.path();
}

OfflineCacheStore::~OfflineCacheStore() {
    // Cleanup if needed
}

void OfflineCacheStore::cacheSearchResults(const QString& query, const QJsonArray& results) {
    QString key = QString("search_%1").arg(query);
    QString filePath = getCacheFilePath(key);
    
    QJsonObject wrapper;
    wrapper["timestamp"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    wrapper["data"] = results;
    
    QJsonDocument doc(wrapper);
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        
        // Update cache entry
        CacheEntry entry;
        entry.key = key;
        entry.filePath = filePath;
        entry.size = QFileInfo(filePath).size();
        entry.timestamp = QDateTime::currentDateTime().toSecsSinceEpoch();
        
        m_cacheEntries.append(entry);
        updateCacheSize();
    }
}

void OfflineCacheStore::cacheExtensionDetails(const QString& extensionId, const QJsonObject& details) {
    QString key = QString("details_%1").arg(extensionId);
    QString filePath = getCacheFilePath(key);
    
    QJsonObject wrapper;
    wrapper["timestamp"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    wrapper["data"] = details;
    
    QJsonDocument doc(wrapper);
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        
        // Update cache entry
        CacheEntry entry;
        entry.key = key;
        entry.filePath = filePath;
        entry.size = QFileInfo(filePath).size();
        entry.timestamp = QDateTime::currentDateTime().toSecsSinceEpoch();
        
        m_cacheEntries.append(entry);
        updateCacheSize();
    }
}

void OfflineCacheStore::cacheExtensionBundle(const QString& extensionId, const QString& bundlePath) {
    QString key = QString("bundle_%1").arg(extensionId);
    QString cachePath = getCacheFilePath(key);
    
    // Copy bundle to cache
    QFile::copy(bundlePath, cachePath);
    
    // Update cache entry
    CacheEntry entry;
    entry.key = key;
    entry.filePath = cachePath;
    entry.size = QFileInfo(cachePath).size();
    entry.timestamp = QDateTime::currentDateTime().toSecsSinceEpoch();
    
    m_cacheEntries.append(entry);
    updateCacheSize();
    
    emit bundleLoaded(extensionId);
}

QJsonArray OfflineCacheStore::getCachedSearchResults(const QString& query) {
    QString key = QString("search_%1").arg(query);
    QString filePath = getCacheFilePath(key);
    
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        
        QJsonObject wrapper = doc.object();
        qint64 timestamp = wrapper["timestamp"].toVariant().toLongLong();
        qint64 now = QDateTime::currentDateTime().toSecsSinceEpoch();
        
        // Check if expired
        if (now - timestamp > m_cacheExpirationDays * 24 * 60 * 60) {
            file.remove(); // Remove expired cache
            return QJsonArray();
        }
        
        return wrapper["data"].toArray();
    }
    
    return QJsonArray();
}

QJsonObject OfflineCacheStore::getCachedExtensionDetails(const QString& extensionId) {
    QString key = QString("details_%1").arg(extensionId);
    QString filePath = getCacheFilePath(key);
    
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        
        QJsonObject wrapper = doc.object();
        qint64 timestamp = wrapper["timestamp"].toVariant().toLongLong();
        qint64 now = QDateTime::currentDateTime().toSecsSinceEpoch();
        
        // Check if expired
        if (now - timestamp > m_cacheExpirationDays * 24 * 60 * 60) {
            file.remove(); // Remove expired cache
            return QJsonObject();
        }
        
        return wrapper["data"].toObject();
    }
    
    return QJsonObject();
}

QString OfflineCacheStore::getCachedExtensionBundle(const QString& extensionId) {
    QString key = QString("bundle_%1").arg(extensionId);
    QString filePath = getCacheFilePath(key);
    
    QFileInfo fileInfo(filePath);
    if (fileInfo.exists()) {
        return filePath;
    }
    
    return QString();
}

void OfflineCacheStore::clearCache() {
    QDirIterator it(m_cacheDir.path(), QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QFile::remove(it.next());
    }
    
    m_cacheEntries.clear();
    updateCacheSize();
    emit cacheCleared();
}

void OfflineCacheStore::setCacheSizeLimit(qint64 bytes) {
    m_cacheSizeLimit = bytes;
    // In a real implementation, we would prune the cache if it exceeds the limit
}

void OfflineCacheStore::setCacheExpiration(int days) {
    m_cacheExpirationDays = days;
}

void OfflineCacheStore::cleanupExpiredEntries() {
    removeExpiredEntries();
}

bool OfflineCacheStore::loadAirGappedBundle(const QString& bundlePath) {
    // In a real implementation, this would load an air-gapped bundle
    // For now, we'll just simulate success
    qDebug() << "[OfflineCacheStore] Loading air-gapped bundle:" << bundlePath;
    return true;
}

bool OfflineCacheStore::exportExtensionBundle(const QString& extensionId, const QString& outputPath) {
    QString cachedBundle = getCachedExtensionBundle(extensionId);
    if (cachedBundle.isEmpty()) {
        return false;
    }
    
    if (QFile::copy(cachedBundle, outputPath)) {
        emit bundleExported(extensionId, outputPath);
        return true;
    }
    
    return false;
}

QStringList OfflineCacheStore::listAvailableBundles() {
    QStringList bundles;
    
    for (const auto& entry : m_cacheEntries) {
        if (entry.key.startsWith("bundle_")) {
            bundles.append(entry.key.mid(7)); // Remove "bundle_" prefix
        }
    }
    
    return bundles;
}

bool OfflineCacheStore::initializeCacheDirectory() {
    QString cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cachePath.isEmpty()) {
        return false;
    }
    
    m_cacheDir = QDir(cachePath).filePath("marketplace_cache");
    if (!m_cacheDir.exists()) {
        return m_cacheDir.mkpath(".");
    }
    
    return true;
}

QString OfflineCacheStore::getCacheFilePath(const QString& key) {
    QString hashedKey = hashKey(key);
    return m_cacheDir.filePath(hashedKey + ".cache");
}

void OfflineCacheStore::updateCacheSize() {
    qint64 size = getDirectorySize(m_cacheDir);
    emit cacheSizeChanged(size);
}

void OfflineCacheStore::removeExpiredEntries() {
    qint64 now = QDateTime::currentDateTime().toSecsSinceEpoch();
    qint64 expirationSeconds = m_cacheExpirationDays * 24 * 60 * 60;
    
    auto it = m_cacheEntries.begin();
    while (it != m_cacheEntries.end()) {
        if (now - it->timestamp > expirationSeconds) {
            QFile::remove(it->filePath);
            it = m_cacheEntries.erase(it);
        } else {
            ++it;
        }
    }
    
    updateCacheSize();
}

qint64 OfflineCacheStore::getDirectorySize(const QDir& dir) {
    qint64 size = 0;
    QDirIterator it(dir.path(), QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QFileInfo fileInfo(it.next());
        if (fileInfo.isFile()) {
            size += fileInfo.size();
        }
    }
    return size;
}

bool OfflineCacheStore::compressFile(const QString& inputPath, const QString& outputPath) {
    // In a real implementation, this would compress the file
    // For now, we'll just copy it
    return QFile::copy(inputPath, outputPath);
}

bool OfflineCacheStore::decompressFile(const QString& inputPath, const QString& outputPath) {
    // In a real implementation, this would decompress the file
    // For now, we'll just copy it
    return QFile::copy(inputPath, outputPath);
}

QString OfflineCacheStore::hashKey(const QString& key) {
    return QString(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Md5).toHex());
}