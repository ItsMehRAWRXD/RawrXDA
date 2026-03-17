/**
 * @file cache_layer.cpp
 * @brief Enterprise Caching Layer Implementation
 */

#include "enterprise/cache_layer.hpp"
#include <algorithm>
#include <iostream>

namespace enterprise {

// =============================================================================
// MultiTierCache Implementation
// =============================================================================

MultiTierCache::MultiTierCache() 
    : m_l1(CacheConfig{10000, 50 * 1024 * 1024, 300, true, true, 30})     // 10K entries, 50MB, 5min TTL
    , m_l2(CacheConfig{50000, 200 * 1024 * 1024, 1800, true, true, 60})   // 50K entries, 200MB, 30min TTL
    , m_l3(CacheConfig{100000, 500 * 1024 * 1024, 3600, true, true, 120}) // 100K entries, 500MB, 1hr TTL
{
}

bool MultiTierCache::set(const std::string& key, const std::string& value, 
                          int ttlSeconds, Tier maxTier) {
    bool success = m_l1.set(key, value, ttlSeconds);
    
    if (maxTier >= Tier::L2_SHARED) {
        m_l2.set(key, value, ttlSeconds);
    }
    
    if (maxTier >= Tier::L3_DISTRIBUTED) {
        m_l3.set(key, value, ttlSeconds);
    }
    
    return success;
}

std::optional<std::string> MultiTierCache::get(const std::string& key) {
    // Try L1 first
    auto l1Result = m_l1.get(key);
    if (l1Result) {
        return l1Result;
    }
    
    // Try L2
    auto l2Result = m_l2.get(key);
    if (l2Result) {
        // Promote to L1
        m_l1.set(key, *l2Result, 300);  // 5 min TTL in L1
        return l2Result;
    }
    
    // Try L3
    auto l3Result = m_l3.get(key);
    if (l3Result) {
        // Promote to L1 and L2
        m_l1.set(key, *l3Result, 300);
        m_l2.set(key, *l3Result, 1800);
        return l3Result;
    }
    
    return std::nullopt;
}

bool MultiTierCache::remove(const std::string& key) {
    bool removed = false;
    removed |= m_l1.remove(key);
    removed |= m_l2.remove(key);
    removed |= m_l3.remove(key);
    return removed;
}

void MultiTierCache::clear(Tier tier) {
    switch (tier) {
        case Tier::L1_MEMORY:
            m_l1.clear();
            break;
        case Tier::L2_SHARED:
            m_l2.clear();
            break;
        case Tier::L3_DISTRIBUTED:
            m_l3.clear();
            break;
    }
}

void MultiTierCache::clearAll() {
    m_l1.clear();
    m_l2.clear();
    m_l3.clear();
}

MultiTierCache::TierStats MultiTierCache::getStats() const {
    return {m_l1.getStats(), m_l2.getStats(), m_l3.getStats()};
}

// =============================================================================
// CacheManager Singleton Implementation
// =============================================================================

CacheManager& CacheManager::instance() {
    static CacheManager instance;
    return instance;
}

CacheManager::CacheManager() 
    : m_defaultCache(CacheConfig{20000, 100 * 1024 * 1024, 3600, true, true, 60}) {
}

void CacheManager::removeCache(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_namedCaches.erase(name);
}

std::vector<std::string> CacheManager::listCaches() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    names.reserve(m_namedCaches.size());
    for (const auto& [name, _] : m_namedCaches) {
        names.push_back(name);
    }
    return names;
}

void CacheManager::clearAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_defaultCache.clear();
    m_multiTier.clearAll();
    m_namedCaches.clear();
}

void CacheManager::cleanupAll() {
    m_defaultCache.cleanup();
    m_multiTier.getL1().cleanup();
}

} // namespace enterprise
