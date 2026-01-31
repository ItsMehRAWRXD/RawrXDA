#include "performance_optimizer_integration.h"
#include <algorithm>

PerformanceOptimizerIntegration::PerformanceOptimizerIntegration(
    std::shared_ptr<Logger> logger,
    std::shared_ptr<Metrics> metrics)
    : m_logger(logger), m_metrics(metrics) {


    m_activeProfile.name = "default";
    m_activeProfile.enableCaching = true;
    m_activeProfile.enableSpeculation = true;
    m_activeProfile.maxCacheSize = 1000;
    m_activeProfile.latencyTargetMs = 100;
}

void PerformanceOptimizerIntegration::optimizeForFile(const std::string& filePath) {

}

void PerformanceOptimizerIntegration::setLatencyTarget(int milliseconds) {
    m_latencyTargetMs = milliseconds;

}

void PerformanceOptimizerIntegration::enableCaching(bool enable) {
    m_activeProfile.enableCaching = enable;

}

void PerformanceOptimizerIntegration::enableSpeculation(bool enable) {
    m_speculationEnabled = enable;

}

std::string PerformanceOptimizerIntegration::getCachedResult(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        it->second.accessCount++;
        updateLRU(key);
        m_metrics->incrementCounter("cache_hits");
        return it->second.value;
    }
    
    m_metrics->incrementCounter("cache_misses");
    return "";
}

void PerformanceOptimizerIntegration::cacheResult(
    const std::string& key,
    const std::string& result) {

    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    CacheEntry entry;
    entry.key = key;
    entry.value = result;
    entry.accessCount = 1;
    
    m_cache[key] = entry;
    updateLRU(key);
    
    if (m_cache.size() > m_maxCacheSize) {
        evictLRU();
    }
}

void PerformanceOptimizerIntegration::invalidateCache(const std::string& pattern) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    if (pattern.empty()) {
        m_cache.clear();
        m_cacheLRU.clear();
    } else {
        // Invalidate entries matching pattern
        for (auto it = m_cache.begin(); it != m_cache.end();) {
            if (it->first.find(pattern) != std::string::npos) {
                it = m_cache.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void PerformanceOptimizerIntegration::clearCache() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_cache.clear();
    m_cacheLRU.clear();

}

void PerformanceOptimizerIntegration::prewarmModels(const std::string& context) {

    m_metrics->incrementCounter("prewarming_operations");
}

std::vector<std::string> PerformanceOptimizerIntegration::getSpeculativeCompletions(
    const std::string& prefix) {

    std::vector<std::string> completions;
    
    if (m_speculationEnabled) {
        completions.push_back(prefix + "_next");
        completions.push_back(prefix + "_alternative");
    }
    
    return completions;
}

void PerformanceOptimizerIntegration::startBackgroundIndexing(const std::string& rootPath) {

    m_metrics->setGauge("background_indexing_active", 1.0);
}

void PerformanceOptimizerIntegration::stopBackgroundIndexing() {

    m_metrics->setGauge("background_indexing_active", 0.0);
}

bool PerformanceOptimizerIntegration::isBackgroundProcessing() const {
    return m_metrics->getGauge("background_indexing_active") > 0.0;
}

double PerformanceOptimizerIntegration::getCacheHitRate() const {
    auto hits = m_metrics->getCounter("cache_hits");
    auto misses = m_metrics->getCounter("cache_misses");
    auto total = hits + misses;
    
    if (total == 0) return 0.0;
    return (double)hits / total;
}

void PerformanceOptimizerIntegration::evictLRU() {
    if (!m_cacheLRU.empty()) {
        std::string lruKey = m_cacheLRU.front();
        m_cacheLRU.erase(m_cacheLRU.begin());
        m_cache.erase(lruKey);

    }
}

void PerformanceOptimizerIntegration::updateLRU(const std::string& key) {
    auto it = std::find(m_cacheLRU.begin(), m_cacheLRU.end(), key);
    if (it != m_cacheLRU.end()) {
        m_cacheLRU.erase(it);
    }
    m_cacheLRU.push_back(key);
}
