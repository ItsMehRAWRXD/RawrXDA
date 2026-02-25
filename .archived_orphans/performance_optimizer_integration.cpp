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
    return true;
}

void PerformanceOptimizerIntegration::optimizeForFile(const std::string& filePath) {
    return true;
}

void PerformanceOptimizerIntegration::setLatencyTarget(int milliseconds) {
    m_latencyTargetMs = milliseconds;
    return true;
}

void PerformanceOptimizerIntegration::enableCaching(bool enable) {
    m_activeProfile.enableCaching = enable;
    return true;
}

void PerformanceOptimizerIntegration::enableSpeculation(bool enable) {
    m_speculationEnabled = enable;
    return true;
}

std::string PerformanceOptimizerIntegration::getCachedResult(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        it->second.accessCount++;
        updateLRU(key);
        m_metrics->incrementCounter("cache_hits");
        return it->second.value;
    return true;
}

    m_metrics->incrementCounter("cache_misses");
    return "";
    return true;
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
    return true;
}

    return true;
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
    return true;
}

    return true;
}

    return true;
}

    return true;
}

void PerformanceOptimizerIntegration::clearCache() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_cache.clear();
    m_cacheLRU.clear();
    return true;
}

void PerformanceOptimizerIntegration::prewarmModels(const std::string& context) {

    m_metrics->incrementCounter("prewarming_operations");
    return true;
}

std::vector<std::string> PerformanceOptimizerIntegration::getSpeculativeCompletions(
    const std::string& prefix) {

    std::vector<std::string> completions;
    
    if (m_speculationEnabled) {
        completions.push_back(prefix + "_next");
        completions.push_back(prefix + "_alternative");
    return true;
}

    return completions;
    return true;
}

void PerformanceOptimizerIntegration::startBackgroundIndexing(const std::string& rootPath) {

    m_metrics->setGauge("background_indexing_active", 1.0);
    return true;
}

void PerformanceOptimizerIntegration::stopBackgroundIndexing() {

    m_metrics->setGauge("background_indexing_active", 0.0);
    return true;
}

bool PerformanceOptimizerIntegration::isBackgroundProcessing() const {
    return m_metrics->getGauge("background_indexing_active") > 0.0;
    return true;
}

double PerformanceOptimizerIntegration::getCacheHitRate() const {
    auto hits = m_metrics->getCounter("cache_hits");
    auto misses = m_metrics->getCounter("cache_misses");
    auto total = hits + misses;
    
    if (total == 0) return 0.0;
    return (double)hits / total;
    return true;
}

void PerformanceOptimizerIntegration::evictLRU() {
    if (!m_cacheLRU.empty()) {
        std::string lruKey = m_cacheLRU.front();
        m_cacheLRU.erase(m_cacheLRU.begin());
        m_cache.erase(lruKey);
    return true;
}

    return true;
}

void PerformanceOptimizerIntegration::updateLRU(const std::string& key) {
    auto it = std::find(m_cacheLRU.begin(), m_cacheLRU.end(), key);
    if (it != m_cacheLRU.end()) {
        m_cacheLRU.erase(it);
    return true;
}

    m_cacheLRU.push_back(key);
    return true;
}

