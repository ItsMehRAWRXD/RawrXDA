#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <atomic>
#include <chrono>
#include <mutex>

#include "logging/logger.h"
#include "metrics/metrics.h"

struct CacheEntry {
    std::string key;
    std::string value;
    size_t accessCount;
};

struct OptimizationProfile {
    std::string name;
    bool enableCaching;
    bool enableSpeculation;
    bool enableIncremental;
    int maxCacheSize;
    int cacheTTLSeconds;
    int latencyTargetMs;
};

class PerformanceOptimizerIntegration {
private:
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;

    std::unordered_map<std::string, CacheEntry> m_cache;
    std::vector<std::string> m_cacheLRU;
    size_t m_maxCacheSize = 1000;
    std::mutex m_cacheMutex;

    std::atomic<bool> m_speculationEnabled{true};
    std::atomic<int> m_latencyTargetMs{100};

    OptimizationProfile m_activeProfile;

public:
    PerformanceOptimizerIntegration(
        std::shared_ptr<Logger> logger,
        std::shared_ptr<Metrics> metrics
    );

    // Core optimization interface
    void optimizeForFile(const std::string& filePath);
    void setLatencyTarget(int milliseconds);
    void enableCaching(bool enable);
    void enableSpeculation(bool enable);

    // Caching
    std::string getCachedResult(const std::string& key);
    void cacheResult(const std::string& key, const std::string& result);
    void invalidateCache(const std::string& pattern = "");
    void clearCache();

    // Speculative execution
    void prewarmModels(const std::string& context);
    std::vector<std::string> getSpeculativeCompletions(const std::string& prefix);

    // Background processing
    void startBackgroundIndexing(const std::string& rootPath);
    void stopBackgroundIndexing();
    bool isBackgroundProcessing() const;

    // Performance monitoring
    double getCacheHitRate() const;

private:
    void evictLRU();
    void updateLRU(const std::string& key);
};
