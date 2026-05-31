#pragma once

// ============================================================================
// OfflineCacheStore — C++20, no Qt. Offline caching of extension marketplace data.
// ============================================================================

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <mutex>

/**
 * Handles: caching search results/details/bundles, cache size/expiration.
 */
class OfflineCacheStore {
public:
    OfflineCacheStore();
    ~OfflineCacheStore();

    void cacheSearchResults(const std::string& query, const std::string& resultsJson);
    void cacheExtensionDetails(const std::string& extensionId, const std::string& detailsJson);
    void cacheExtensionBundle(const std::string& extensionId, const std::string& bundlePath);

    std::string getCachedSearchResults(const std::string& query);
    std::string getCachedExtensionDetails(const std::string& extensionId);
    bool hasCachedBundle(const std::string& extensionId);

    void clearCache();
    void setCacheSizeLimit(int64_t bytes);
    void setCacheExpirationDays(int days);
    void cleanupExpiredEntries();

    using BundleLoadedFn = std::function<void(const std::string& extensionId)>;
    void setOnCacheCleared(std::function<void()> fn) { m_onCacheCleared = std::move(fn); }
    void setOnCacheSizeChanged(std::function<void(int64_t)> fn) { m_onCacheSizeChanged = std::move(fn); }
    void setOnBundleLoaded(BundleLoadedFn fn) { m_onBundleLoaded = std::move(fn); }

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
    int64_t m_currentCacheSize = 0;
    std::vector<CacheEntry> m_cacheEntries;
    std::mutex m_mutex;

    void initializeCacheDirectory();
    std::string getCacheFilePath(const std::string& key);
    void updateCacheSize();
    void enforceLimits();

    std::function<void()> m_onCacheCleared;
    std::function<void(int64_t)> m_onCacheSizeChanged;
    BundleLoadedFn m_onBundleLoaded;
};
