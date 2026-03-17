#pragma once

#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <any>
#include <ctime>

class CacheEntry {
public:
    std::string key;
    std::any value;
    std::time_t createdAt;
    std::time_t expiresAt;
    bool isExpired() const;
};

class Cache {
private:
    std::string m_name;
    std::map<std::string, CacheEntry> m_entries;
    std::mutex m_mutex;
    size_t m_maxSize;

public:
    explicit Cache(const std::string& name, size_t maxSize = 1000);

    void set(const std::string& key, const std::any& value, int ttlSeconds = -1);
    std::any get(const std::string& key);
    bool has(const std::string& key) const;
    void remove(const std::string& key);
    void clear();
    size_t size() const;
};

class CacheManager {
private:
    static CacheManager* s_instance;
    static std::mutex s_mutex;
    std::map<std::string, std::shared_ptr<Cache>> m_caches;

    CacheManager() = default;

public:
    static CacheManager& instance();

    Cache& defaultCache();
    Cache& getOrCreateCache(const std::string& name, size_t maxSize = 1000);
    void removeCache(const std::string& name);
    void clearAllCaches();
};
