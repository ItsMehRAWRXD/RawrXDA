#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>

namespace RawrXD {
namespace IDE {

// Performance optimization cache entry
struct CacheEntry {
    std::string key;
    std::string value;
    int accessCount;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastAccessedAt;
};

// Performance metrics
struct PerformanceMetrics {
    float avgSuggestionLatency;      // ms
    float p99SuggestionLatency;      // ms
    float cacheHitRate;              // 0.0-1.0
    float specExecutionSuccessRate;  // 0.0-1.0
    int totalSuggestions;
    int cacheHits;
    int cacheMisses;
};

// Performance optimizer
class PerformanceOptimizer {
public:
    PerformanceOptimizer();
    ~PerformanceOptimizer() = default;

    // Context caching
    void cacheContext(
        const std::string& key,
        const std::string& context
    );

    std::string getCachedContext(const std::string& key);
    bool hasCachedContext(const std::string& key);

    // Speculative execution (pre-generate common completions)
    void speculativelyGenerate(
        const std::string& context,
        const std::vector<std::string>& likelyPrefixes
    );

    std::vector<std::string> getSpeculativeCompletions(const std::string& prefix);

    // Incremental parsing (parse only changed regions)
    struct ParseDelta {
        int startLine;
        int endLine;
        std::vector<std::string> changes;
    };

    void incrmentallyParse(
        const std::string& filePath,
        const ParseDelta& delta
    );

    // Background indexing
    void startBackgroundIndexing(const std::string& projectPath);
    void stopBackgroundIndexing();
    bool isIndexing() const;
    float getIndexingProgress() const;

    // Warm up caches
    void warmupCache(const std::string& projectPath);

    // Prewarm common patterns
    void preloadCommonPatterns(const std::string& language);

    // Clear caches
    void clearCache();
    void clearOldEntries(int ageSeconds);

    // Configure performance
    void setMaxCacheSize(int sizeBytes);
    void setSpeculativePrefixes(const std::vector<std::string>& prefixes);
    void setIndexingThreadCount(int threads);

    // Get metrics
    PerformanceMetrics getMetrics();

    // Record latency (for statistics)
    void recordLatency(float latencyMs, bool cacheHit);

private:
    struct SpeculativeEntry {
        std::string prefix;
        std::vector<std::string> completions;
        std::chrono::system_clock::time_point generated;
    };

    struct ParseCache {
        std::string filePath;
        std::vector<std::string> lines;
        std::chrono::system_clock::time_point lastUpdated;
    };

    // Eviction policy (LRU)
    void evictOldestEntry();

    std::unordered_map<std::string, CacheEntry> m_contextCache;
    std::vector<SpeculativeEntry> m_speculativeCache;
    std::unordered_map<std::string, ParseCache> m_parseCache;

    bool m_indexing;
    float m_indexingProgress;
    int m_maxCacheSize;
    std::vector<std::string> m_speculativePrefixes;
    int m_indexThreadCount;

    // Metrics
    float m_totalLatency;
    int m_recordCount;
    int m_cacheHits;
    int m_cacheMisses;
};

} // namespace IDE
} // namespace RawrXD
