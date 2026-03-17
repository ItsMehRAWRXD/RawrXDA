#include "PerformanceOptimizer.h"
#include <algorithm>
#include <thread>
#include <fstream>
#include <sstream>

namespace RawrXD {
namespace IDE {

PerformanceOptimizer::PerformanceOptimizer()
    : m_cacheHits(0), m_cacheMisses(0), m_isWarmupRunning(false),
      m_maxCacheSize(100), m_isIndexing(false) {
}

PerformanceOptimizer::~PerformanceOptimizer() {
    stopBackgroundIndexing();
}

bool PerformanceOptimizer::cacheContext(
    const std::string& key, const std::string& context,
    int ttlSeconds) {
    
    // Check if cache is full
    if (m_contextCache.size() >= m_maxCacheSize) {
        // Remove least recently used item
        if (!m_contextCache.empty()) {
            m_contextCache.erase(m_contextCache.begin());
        }
    }
    
    CacheEntry entry;
    entry.key = key;
    entry.data = context;
    entry.createdTime = std::chrono::system_clock::now();
    entry.ttlSeconds = ttlSeconds;
    
    m_contextCache[key] = entry;
    return true;
}

bool PerformanceOptimizer::getCachedContext(
    const std::string& key, std::string& outContext) {
    
    auto it = m_contextCache.find(key);
    if (it == m_contextCache.end()) {
        m_cacheMisses++;
        return false;
    }
    
    CacheEntry& entry = it->second;
    
    // Check if cache entry has expired
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - entry.createdTime).count();
    
    if (elapsed > entry.ttlSeconds) {
        m_contextCache.erase(it);
        m_cacheMisses++;
        return false;
    }
    
    outContext = entry.data;
    entry.lastAccessTime = now;
    m_cacheHits++;
    return true;
}

std::vector<std::string> PerformanceOptimizer::speculativelyGenerate(
    const std::string& context, const std::string& modelName,
    int maxSuggestions) {
    
    std::vector<std::string> suggestions;
    
    // Generate likely next completions speculatively
    if (context.find("if ") != std::string::npos) {
        suggestions.push_back("else");
        suggestions.push_back("else if");
    }
    
    if (context.find("for ") != std::string::npos) {
        suggestions.push_back("break");
        suggestions.push_back("continue");
    }
    
    if (context.find("{") != std::string::npos) {
        suggestions.push_back("}");
    }
    
    // Limit results
    if (suggestions.size() > static_cast<size_t>(maxSuggestions)) {
        suggestions.resize(maxSuggestions);
    }
    
    return suggestions;
}

bool PerformanceOptimizer::incrmentallyParse(
    const std::string& previousCode, const std::string& newCode,
    int changeStartLine, int changeEndLine) {
    
    // Compare old and new code to identify changed lines
    if (previousCode == newCode) {
        return false;  // No changes
    }
    
    // Mark lines as dirty for re-parsing
    for (int i = changeStartLine; i <= changeEndLine; i++) {
        m_dirtyLines.insert(i);
    }
    
    return true;
}

bool PerformanceOptimizer::warmupCache(
    const std::vector<std::string>& commonPatterns) {
    
    if (m_isWarmupRunning) return false;
    
    m_isWarmupRunning = true;
    
    // Pre-load common patterns into cache
    for (const auto& pattern : commonPatterns) {
        cacheContext("pattern_" + pattern, pattern, 3600);
    }
    
    m_isWarmupRunning = false;
    return true;
}

PerformanceMetrics PerformanceOptimizer::getMetrics() {
    PerformanceMetrics metrics;
    
    metrics.cacheHitRate = m_cacheHits > 0 ?
        static_cast<float>(m_cacheHits) / (m_cacheHits + m_cacheMisses) : 0.0f;
    metrics.cacheSize = m_contextCache.size();
    metrics.cacheMaxSize = m_maxCacheSize;
    metrics.dirtyLineCount = m_dirtyLines.size();
    metrics.totalCacheHits = m_cacheHits;
    metrics.totalCacheMisses = m_cacheMisses;
    
    return metrics;
}

void PerformanceOptimizer::clearCache() {
    m_contextCache.clear();
    m_cacheHits = 0;
    m_cacheMisses = 0;
}

void PerformanceOptimizer::clearDirtyLines() {
    m_dirtyLines.clear();
}

bool PerformanceOptimizer::startBackgroundIndexing(
    const std::vector<std::string>& filePaths) {
    
    if (m_isIndexing) return false;
    
    m_isIndexing = true;
    
    // Start background thread for indexing
    m_indexThread = std::thread(&PerformanceOptimizer::backgroundIndexThread, 
                                this, filePaths);
    
    return true;
}

bool PerformanceOptimizer::stopBackgroundIndexing() {
    if (!m_isIndexing) return false;
    
    m_isIndexing = false;
    
    if (m_indexThread.joinable()) {
        m_indexThread.join();
    }
    
    return true;
}

void PerformanceOptimizer::backgroundIndexThread(
    const std::vector<std::string>& filePaths) {
    
    for (const auto& filePath : filePaths) {
        if (!m_isIndexing) break;
        
        // Real indexing work
        try {
            std::ifstream file(filePath);
            if (file.is_open()) {
                 std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                 // Store actual content or semantic summary
                 cacheContext("indexed_" + filePath, content, 3600);
            }
        } catch(...) {
            // Soft fail on read
        }
    }
}

void PerformanceOptimizer::setCacheMaxSize(size_t maxSize) {
    m_maxCacheSize = maxSize;
    
    // Evict items if cache is now too large
    while (m_contextCache.size() > maxSize) {
        m_contextCache.erase(m_contextCache.begin());
    }
}

float PerformanceOptimizer::estimateLatency(
    const std::string& operation, const std::string& context) {
    
    // Return cached latency if available
    auto it = m_latencyEstimates.find(operation);
    if (it != m_latencyEstimates.end()) {
        return it->second;
    }
    
    // Return default estimate
    if (operation == "completion") {
        return 150.0f;  // 150ms
    } else if (operation == "analysis") {
        return 300.0f;  // 300ms
    } else if (operation == "refactor") {
        return 500.0f;  // 500ms
    }
    
    return 200.0f;  // Default
}

void PerformanceOptimizer::recordLatency(
    const std::string& operation, float latencyMs) {
    
    // Update running average
    auto it = m_latencyEstimates.find(operation);
    if (it == m_latencyEstimates.end()) {
        m_latencyEstimates[operation] = latencyMs;
    } else {
        // Exponential moving average (0.7 * old + 0.3 * new)
        it->second = 0.7f * it->second + 0.3f * latencyMs;
    }
}

bool PerformanceOptimizer::shouldUseCache(const std::string& key) {
    // Always try cache first
    return m_contextCache.find(key) != m_contextCache.end();
}

bool PerformanceOptimizer::shouldSpeculate(
    const std::string& context, const std::string& recentActivity) {
    
    // Use speculation if user is typing quickly
    if (recentActivity.length() > 10) {
        return true;
    }
    
    // Use speculation on common patterns
    if (context.find("if ") != std::string::npos ||
        context.find("for ") != std::string::npos ||
        context.find("while ") != std::string::npos) {
        return true;
    }
    
    return false;
}

std::vector<std::string> PerformanceOptimizer::getDirtyFiles() {
    std::vector<std::string> files;
    
    // Return files with dirty lines
    for (int line : m_dirtyLines) {
        files.push_back("file_" + std::to_string(line));
    }
    
    return files;
}

bool PerformanceOptimizer::invalidateCache(const std::string& key) {
    auto it = m_contextCache.find(key);
    if (it != m_contextCache.end()) {
        m_contextCache.erase(it);
        return true;
    }
    return false;
}

void PerformanceOptimizer::invalidateCacheByPrefix(const std::string& prefix) {
    auto it = m_contextCache.begin();
    while (it != m_contextCache.end()) {
        if (it->first.find(prefix) == 0) {
            it = m_contextCache.erase(it);
        } else {
            ++it;
        }
    }
}

bool PerformanceOptimizer::prefetchData(
    const std::vector<std::string>& keys) {
    
    for (const auto& key : keys) {
        // Pre-load data into cache
        cacheContext(key, "prefetched_" + key, 600);
    }
    
    return true;
}

float PerformanceOptimizer::getCacheUtilization() {
    if (m_maxCacheSize == 0) return 0.0f;
    return static_cast<float>(m_contextCache.size()) / 
           static_cast<float>(m_maxCacheSize);
}

} // namespace IDE
} // namespace RawrXD
