#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <memory>

#include "logging/logger.h"
#include "metrics/metrics.h"
#include "inference_engine.h"

struct CodeCompletion {
    std::string text;
    std::string detail;
    double confidence;
    std::string kind;
    int insertTextLength;
    int cursorOffset;
};

struct PerformanceMetrics {
    double avgLatencyMs;
    double p95LatencyMs;
    double p99LatencyMs;
    double cacheHitRate;
    size_t requestCount;
    size_t errorCount;
};

class RealTimeCompletionEngine {
private:
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;
    InferenceEngine* m_inferenceEngine;  // Pointer to shared inference engine

    // Caching
    std::unordered_map<std::string, std::vector<CodeCompletion>> m_completionCache;
    mutable std::mutex m_cacheMutex;

    // Performance tracking
    std::atomic<int64_t> m_totalRequests{0};
    std::atomic<int64_t> m_cacheHits{0};
    std::vector<double> m_latencyHistory;
    mutable std::mutex m_latencyMutex;

    // Configuration
    int m_maxCompletions = 5;
    int m_minConfidence = 60;
    int m_latencyTargetMs = 100;

public:
    RealTimeCompletionEngine(
        std::shared_ptr<Logger> logger,
        std::shared_ptr<Metrics> metrics
    );

    // Set InferenceEngine reference (called by AIIntegrationHub)
    void setInferenceEngine(InferenceEngine* engine) {
        m_inferenceEngine = engine;
        if (m_inferenceEngine && m_logger) {
            m_logger->info("InferenceEngine set for CompletionEngine");
        }
    }

    // Core completion interface
    std::vector<CodeCompletion> getCompletions(
        const std::string& prefix,
        const std::string& suffix,
        const std::string& fileType,
        const std::string& context
    );

    // Advanced features
    std::vector<CodeCompletion> getInlineCompletions(
        const std::string& currentLine,
        int cursorColumn,
        const std::string& filePath
    );

    std::vector<CodeCompletion> getMultiLineCompletions(
        const std::string& prefix,
        int maxLines = 3
    );

    // Context-aware completions
    std::vector<CodeCompletion> getContextualCompletions(
        const std::string& filePath,
        int line,
        int column,
        const std::string& scope
    );

    // Performance optimization
    void prewarmCache(const std::string& filePath);
    void clearCache();
    PerformanceMetrics getMetrics() const;

private:
    std::vector<CodeCompletion> generateCompletionsWithModel(
        const std::string& prompt,
        int maxTokens
    );

    std::string buildCompletionPrompt(
        const std::string& prefix,
        const std::string& suffix,
        const std::string& context
    );

    std::vector<CodeCompletion> postProcessCompletions(
        const std::string& modelOutput,
        const std::string& prefix
    );

    double calculateConfidence(
        const std::string& completion,
        const std::string& context
    );

    bool shouldUseCache(const std::string& key);
    void updateCache(const std::string& key, const std::vector<CodeCompletion>& completions);
    std::string generateCacheKey(const std::string& prefix, const std::string& suffix);
};
