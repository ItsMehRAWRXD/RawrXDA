#pragma once

#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <map>
#include <future>
#include <thread>
#include "metrics.h"
#include "logger.h"
#include "cpu_inference_engine.h"

// Structs
struct CodeCompletion {
    std::string text;
    std::string detail;
    std::string kind;
    double confidence;
    int insertTextLength;
    int cursorOffset;
};

struct PerformanceMetrics {
    double avgLatencyMs;
    double p95LatencyMs;
    double p99LatencyMs;
    double cacheHitRate;
    long long requestCount;
    long long errorCount;
};

class RealTimeCompletionEngine {
public:
    RealTimeCompletionEngine(std::shared_ptr<Logger> logger, std::shared_ptr<Metrics> metrics);
    virtual ~RealTimeCompletionEngine() = default;

    // Synchronous Core API
    std::vector<CodeCompletion> getCompletions(const std::string& prefix, 
                                             const std::string& suffix, 
                                             const std::string& fileType, 
                                             const std::string& context);
                                             
    std::vector<CodeCompletion> getInlineCompletions(const std::string& currentLine, 
                                                   int cursorColumn, 
                                                   const std::string& filePath);
                                                   
    std::vector<CodeCompletion> getMultiLineCompletions(const std::string& prefix, 
                                                      int maxLines);
                                                      
    std::vector<CodeCompletion> getContextualCompletions(const std::string& filePath, 
                                                       int line, 
                                                       int column, 
                                                       const std::string& scope);

    // Asynchronous Core API (returns futures for non-blocking operation)
    std::future<std::vector<CodeCompletion>> getCompletionsAsync(const std::string& prefix, 
                                                                const std::string& suffix, 
                                                                const std::string& fileType, 
                                                                const std::string& context);
                                             
    std::future<std::vector<CodeCompletion>> getInlineCompletionsAsync(const std::string& currentLine, 
                                                                      int cursorColumn, 
                                                                      const std::string& filePath);
                                                   
    std::future<std::vector<CodeCompletion>> getMultiLineCompletionsAsync(const std::string& prefix, 
                                                                         int maxLines);
                                                      
    std::future<std::vector<CodeCompletion>> getContextualCompletionsAsync(const std::string& filePath, 
                                                                          int line, 
                                                                          int column, 
                                                                          const std::string& scope);

    // Cache Management
    void prewarmCache(const std::string& filePath);
    void clearCache();
    
    // Monitoring
    PerformanceMetrics getMetrics() const;

    // Dependency Injection
    void setInferenceEngine(RawrXD::InferenceEngine* engine) { m_inferenceEngine = engine; }

private:
    std::string generateCacheKey(const std::string& prefix, const std::string& suffix);
    bool shouldUseCache(const std::string& key);
    void updateCache(const std::string& key, const std::vector<CodeCompletion>& completions);

    std::string buildCompletionPrompt(const std::string& prefix, const std::string& suffix, const std::string& context);
    std::vector<CodeCompletion> generateCompletionsWithModel(const std::string& prompt, int maxTokens);
    std::vector<CodeCompletion> postProcessCompletions(const std::string& modelOutput, const std::string& prefix);
    double calculateConfidence(const std::string& completion, const std::string& prompt);

private:
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;
    RawrXD::InferenceEngine* m_inferenceEngine;

    std::mutex m_cacheMutex;
    std::unordered_map<std::string, std::vector<CodeCompletion>> m_completionCache;
    
    mutable std::mutex m_latencyMutex;
    std::vector<double> m_latencyHistory;
    
    long long m_totalRequests = 0;
    long long m_cacheHits = 0;
    long long m_errorCount = 0;
    double m_minConfidence = 50.0;
};
