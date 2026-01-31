#include "marketplace/offline_cache_store.h"
OfflineCacheStore::OfflineCacheStore()
    
    , m_cacheSizeLimit(100 * 1024 * 1024) // 100 MB default
    , m_cacheExpirationDays(30) // 30 days default
{
    initializeCacheDirectory();
    updateCacheSize();
    // // qDebug:  "[OfflineCacheStore] Initialized with cache dir:" << m_cacheDir.path();
}

OfflineCacheStore::~OfflineCacheStore() {
    // Cleanup if needed
}

void OfflineCacheStore::cacheSearchResults(const std::string& query, const void*& results) {
    std::string key = std::string("search_%1").arg(query);
    std::string filePath = getCacheFilePath(key);
    
    void* wrapper;
    wrapper["timestamp"] = // DateTime::currentDateTime().toSecsSinceEpoch();
    wrapper["data"] = results;
    
    void* doc(wrapper);
    // File operation removed;
    if (file.open(std::iostream::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        
        // Update cache entry
        CacheEntry entry;
        entry.key = key;
        entry.filePath = filePath;
        entry.size = // FileInfo: filePath).size();
        entry.timestamp = // DateTime::currentDateTime().toSecsSinceEpoch();
        
        m_cacheEntries.append(entry);
        updateCacheSize();
    }
}

void OfflineCacheStore::cacheExtensionDetails(const std::string& extensionId, const void*& details) {
    std::string key = std::string("details_%1").arg(extensionId);
    std::string filePath = getCacheFilePath(key);
    
    void* wrapper;
    wrapper["timestamp"] = // DateTime::currentDateTime().toSecsSinceEpoch();
    wrapper["data"] = details;
    
    void* doc(wrapper);
    // File operation removed;
    if (file.open(std::iostream::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        
        // Update cache entry
        CacheEntry entry;
        entry.key = key;
        entry.filePath = filePath;
        entry.size = // FileInfo: filePath).size();
        entry.timestamp = // DateTime::currentDateTime().toSecsSinceEpoch();
        
        m_cacheEntries.append(entry);
        updateCacheSize();
    }
}

void OfflineCacheStore::cacheExtensionBundle(const std::string& extensionId, const std::string& bundlePath) {
    std::string key = std::string("bundle_%1").arg(extensionId);
    std::string cachePath = getCacheFilePath(key);
    
    // Copy bundle to cache
    std::filesystem::copy(bundlePath, cachePath);
    
    // Update cache entry
    CacheEntry entry;
    entry.key = key;
    entry.filePath = cachePath;
    entry.size = // FileInfo: cachePath).size();
    entry.timestamp = // DateTime::currentDateTime().toSecsSinceEpoch();
    
    m_cacheEntries.append(entry);
    updateCacheSize();
    
    bundleLoaded(extensionId);
}

void* OfflineCacheStore::getCachedSearchResults(const std::string& query) {
    std::string key = std::string("search_%1").arg(query);
    std::string filePath = getCacheFilePath(key);
    
    // File operation removed;
    if (file.open(std::iostream::ReadOnly)) {
        void* doc = void*::fromJson(file.readAll());
        file.close();
        
        void* wrapper = doc.object();
        int64_t timestamp = wrapper["timestamp"].toVariant().toLongLong();
        int64_t now = // DateTime::currentDateTime().toSecsSinceEpoch();
        
        // Check if expired
        if (now - timestamp > m_cacheExpirationDays * 24 * 60 * 60) {
            file.remove(); // Remove expired cache
            return void*();
        }
        
        return wrapper["data"].toArray();
    }
    
    return void*();
}

void* OfflineCacheStore::getCachedExtensionDetails(const std::string& extensionId) {
    std::string key = std::string("details_%1").arg(extensionId);
    std::string filePath = getCacheFilePath(key);
    
    // File operation removed;
    if (file.open(std::iostream::ReadOnly)) {
        void* doc = void*::fromJson(file.readAll());
        file.close();
        
        void* wrapper = doc.object();
        int64_t timestamp = wrapper["timestamp"].toVariant().toLongLong();
        int64_t now = // DateTime::currentDateTime().toSecsSinceEpoch();
        
        // Check if expired
        if (now - timestamp > m_cacheExpirationDays * 24 * 60 * 60) {
            file.remove(); // Remove expired cache
            return void*();
        }
        
        return wrapper["data"].toObject();
    }
    
    return void*();
}

std::string OfflineCacheStore::getCachedExtensionBundle(const std::string& extensionId) {
    std::string key = std::string("bundle_%1").arg(extensionId);
    std::string filePath = getCacheFilePath(key);
    
    // Info fileInfo(filePath);
    if (fileInfo.exists()) {
        return filePath;
    }
    
    return std::string();
}

void OfflineCacheStore::clearCache() {
    // DirIterator it(m_cacheDir.path(), // DirIterator::Subdirectories);
    while (it.hasNext()) {
        std::filesystem::remove(it.next());
    }
    
    m_cacheEntries.clear();
    updateCacheSize();
    cacheCleared();
}

void OfflineCacheStore::setCacheSizeLimit(int64_t bytes) {
    m_cacheSizeLimit = bytes;
    // In a real implementation, we would prune the cache if it exceeds the limit
}

void OfflineCacheStore::setCacheExpiration(int days) {
    m_cacheExpirationDays = days;
}

void OfflineCacheStore::cleanupExpiredEntries() {
    removeExpiredEntries();
}

bool OfflineCacheStore::loadAirGappedBundle(const std::string& bundlePath) {
    // In a real implementation, this would load an air-gapped bundle
    // For now, we'll just simulate success
    // // qDebug:  "[OfflineCacheStore] Loading air-gapped bundle:" << bundlePath;
    return true;
}

bool OfflineCacheStore::exportExtensionBundle(const std::string& extensionId, const std::string& outputPath) {
    std::string cachedBundle = getCachedExtensionBundle(extensionId);
    if (cachedBundle.empty()) {
        return false;
    }
    
    if (std::filesystem::copy(cachedBundle, outputPath)) {
        bundleExported(extensionId, outputPath);
        return true;
    }
    
    return false;
}

std::stringList OfflineCacheStore::listAvailableBundles() {
    std::stringList bundles;
    
    for (const auto& entry : m_cacheEntries) {
        if (entry.key.startsWith("bundle_")) {
            bundles.append(entry.key.mid(7)); // Remove "bundle_" prefix
        }
    }
    
    return bundles;
}

bool OfflineCacheStore::initializeCacheDirectory() {
    std::string cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cachePath.empty()) {
        return false;
    }
    
    m_cacheDir = // (cachePath).filePath("marketplace_cache");
    if (!m_cacheDir.exists()) {
        return m_cacheDir.mkpath(".");
    }
    
    return true;
}

std::string OfflineCacheStore::getCacheFilePath(const std::string& key) {
    std::string hashedKey = hashKey(key);
    return m_cacheDir.filePath(hashedKey + ".cache");
}

void OfflineCacheStore::updateCacheSize() {
    int64_t size = getDirectorySize(m_cacheDir);
    cacheSizeChanged(size);
}

void OfflineCacheStore::removeExpiredEntries() {
    int64_t now = // DateTime::currentDateTime().toSecsSinceEpoch();
    int64_t expirationSeconds = m_cacheExpirationDays * 24 * 60 * 60;
    
    auto it = m_cacheEntries.begin();
    while (it != m_cacheEntries.end()) {
        if (now - it->timestamp > expirationSeconds) {
            std::filesystem::remove(it->filePath);
            it = m_cacheEntries.erase(it);
        } else {
            ++it;
        }
    }
    
    updateCacheSize();
}

int64_t OfflineCacheStore::getDirectorySize(const // & dir) {
    int64_t size = 0;
    // DirIterator it(dir.path(), // DirIterator::Subdirectories);
    while (it.hasNext()) {
        // Info fileInfo(it.next());
        if (fileInfo.isFile()) {
            size += fileInfo.size();
        }
    }
    return size;
}

bool OfflineCacheStore::compressFile(const std::string& inputPath, const std::string& outputPath) {
    // In a real implementation, this would compress the file
    // For now, we'll just copy it
    return std::filesystem::copy(inputPath, outputPath);
}

bool OfflineCacheStore::decompressFile(const std::string& inputPath, const std::string& outputPath) {
    // In a real implementation, this would decompress the file
    // For now, we'll just copy it
    return std::filesystem::copy(inputPath, outputPath);
}

std::string OfflineCacheStore::hashKey(const std::string& key) {
    return std::string(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Md5).toHex());
}






