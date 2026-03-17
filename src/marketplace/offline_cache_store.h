#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <nlohmann/json.hpp>

class OfflineCacheStore {
public:
    struct CacheEntry {
        std::string key;
        std::string filePath;
        size_t size;
        int64_t timestamp;
    };

    OfflineCacheStore();
    ~OfflineCacheStore();

    void cacheSearchResults(const std::string& query, const nlohmann::json& results);
    nlohmann::json getCachedSearchResults(const std::string& query);
    
    void cacheExtensionDetails(const std::string& extensionId, const nlohmann::json& details);
    nlohmann::json getCachedExtensionDetails(const std::string& extensionId);
    
    void cacheExtensionBundle(const std::string& extensionId, const std::string& bundlePath);
    bool hasCachedBundle(const std::string& extensionId);
    
    void clearCache();
    void setCacheSizeLimit(int64_t bytes);
    void setCacheExpirationDays(int days);

private:
    void initializeCacheDirectory();
    void updateCacheSize();
    std::string getCacheFilePath(const std::string& key);
    void enforceLimits();

    std::vector<CacheEntry> m_cacheEntries;
    int64_t m_cacheSizeLimit;
    int m_cacheExpirationDays;
    int64_t m_currentCacheSize;
    std::string m_cacheDir;
    std::mutex m_mutex;
};
