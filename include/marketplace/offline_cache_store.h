#pragma once

// ============================================================================
// OfflineCacheStore — C++20, no Qt. Offline caching of extension marketplace data.
// ============================================================================

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/**
 * Handles: caching search results/details/bundles, cache size/expiration,
 * air-gapped bundle loading.
 */
class OfflineCacheStore {
public:
    OfflineCacheStore() = default;
    ~OfflineCacheStore();

    void cacheSearchResults(const std::string& query, const std::string& resultsJson);
    void cacheExtensionDetails(const std::string& extensionId, const std::string& detailsJson);
    void cacheExtensionBundle(const std::string& extensionId, const std::string& bundlePath);

    std::string getCachedSearchResults(const std::string& query);
    std::string getCachedExtensionDetails(const std::string& extensionId);
    std::string getCachedExtensionBundle(const std::string& extensionId);

    void clearCache();
    void setCacheSizeLimit(int64_t bytes);
    void setCacheExpiration(int days);
    void cleanupExpiredEntries();

    bool loadAirGappedBundle(const std::string& bundlePath);
    bool exportExtensionBundle(const std::string& extensionId, const std::string& outputPath);
    std::vector<std::string> listAvailableBundles();

    using BundleLoadedFn = std::function<void(const std::string& extensionId)>;
    using BundleExportedFn = std::function<void(const std::string& extensionId, const std::string& outputPath)>;
    void setOnCacheCleared(std::function<void()> fn) { m_onCacheCleared = std::move(fn); }
    void setOnCacheSizeChanged(std::function<void(int64_t)> fn) { m_onCacheSizeChanged = std::move(fn); }
    void setOnBundleLoaded(BundleLoadedFn fn) { m_onBundleLoaded = std::move(fn); }
    void setOnBundleExported(BundleExportedFn fn) { m_onBundleExported = std::move(fn); }

private:
    struct CacheEntry {
        std::string key;
        std::string filePath;
        int64_t size = 0;
        int64_t timestamp = 0;
    };

    std::string m_cacheDir;
    int64_t m_cacheSizeLimit = 0;
    int m_cacheExpirationDays = 30;
    std::vector<CacheEntry> m_cacheEntries;

    bool initializeCacheDirectory();
    std::string getCacheFilePath(const std::string& key);
    void updateCacheSize();
    void removeExpiredEntries();
    int64_t getDirectorySize(const std::string& dir);
    bool compressFile(const std::string& inputPath, const std::string& outputPath);
    bool decompressFile(const std::string& inputPath, const std::string& outputPath);
    std::string hashKey(const std::string& key);

    std::function<void()> m_onCacheCleared;
    std::function<void(int64_t)> m_onCacheSizeChanged;
    BundleLoadedFn m_onBundleLoaded;
    BundleExportedFn m_onBundleExported;
};
