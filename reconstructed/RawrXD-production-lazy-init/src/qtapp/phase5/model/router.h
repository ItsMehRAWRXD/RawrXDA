#pragma once

#include <QString>
#include <QObject>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>
#include <atomic>
#include <queue>
#include <functional>
#include "model_router/ModelRouterExtension.h"

// Forward declarations
class InferenceEngine;
class ModelPoolManager;
class RoutingAnalytics;

/**
 * @brief Inference modes for different use cases
 */
enum class InferenceMode {
    Standard,          // Normal inference - fast, balanced
    Max,               // Maximum quality - high temperature, full context
    Research,          // Research mode - detailed exploration, higher temperature
    DeepResearch,      // Deep research - extended context, multi-pass analysis
    Thinking,          // Extended thinking - chain of thought, verbose reasoning
    Custom             // User-defined parameters
};

/**
 * @brief Inference request context (Phase 5 specific)
 */
struct InferenceRequest {
    QString prompt;
    int maxTokens;
    double temperature;
    int contextWindow;
    bool useCache;
    QString preferredModel;
    InferenceMode mode;
    
    InferenceRequest() : maxTokens(512), temperature(0.7), contextWindow(2048),
                        useCache(true), mode(InferenceMode::Standard) {}
};

/**
 * @brief Model metadata and performance metrics
 */
struct ModelMetrics {
    QString modelId;
    QString modelName;
    
    // Performance
    double tokensPerSecond;
    double avgLatencyMs;
    double p95LatencyMs;
    int requestCount;
    int errorCount;
    
    // Utilization
    double memoryUsageMb;
    double gpuUtilization;
    int activeRequests;
    int queuedRequests;
    
    // Quality metrics
    double coherenceScore;
    double relevanceScore;
    double responseQuality;
    
    ModelMetrics() : tokensPerSecond(0), avgLatencyMs(0), p95LatencyMs(0),
                    requestCount(0), errorCount(0), memoryUsageMb(0),
                    gpuUtilization(0), activeRequests(0), queuedRequests(0),
                    coherenceScore(0.9), relevanceScore(0.9), responseQuality(0.9) {}
};

/**
 * @brief Phase 5: Advanced Model Router with load balancing and analytics
 * 
 * Orchestrates inference across multiple model endpoints using various
 * routing strategies. Provides real-time performance metrics and specialized
 * inference modes (MAX, Research, Deep Research, Thinking).
 */
class Phase5ModelRouter : public QObject {
    Q_OBJECT
    
public:
    explicit Phase5ModelRouter(QObject* parent = nullptr);
    ~Phase5ModelRouter();
    
    // ===== Model Management =====
    
    /**
     * @brief Register a model endpoint
     */
    bool registerModel(const ModelEndpoint& endpoint);
    
    /**
     * @brief Unregister a model endpoint
     */
    bool unregisterModel(const QString& modelId);
    
    /**
     * @brief Load a custom GGUF model
     * @param path Path to GGUF file
     * @param modelName Friendly name for the model
     * @return true if loaded successfully
     */
    bool loadCustomGGUFModel(const QString& path, const QString& modelName);
    
    /**
     * @brief Get list of available models
     */
    QStringList availableModels() const;
    
    /**
     * @brief Get endpoint by ID
     */
    ModelEndpoint* getEndpoint(const QString& endpointId);
    const ModelEndpoint* getEndpoint(const QString& endpointId) const;
    
    // ===== Routing & Inference =====
    
    /**
     * @brief Route and execute inference request
     * @param request The inference request
     * @return Generated text response
     */
    QString executeInference(const InferenceRequest& request);

    /**
     * @brief Execute inference asynchronously
     */
    void executeInferenceAsync(const InferenceRequest& request);

    /**
     * @brief Route request based on weighted random selection
     */
    RoutingDecision routeWeightedRandom(const InferenceRequest& request);

    /**
     * @brief Route request based on least connections
     */
    RoutingDecision routeLeastConnections(const InferenceRequest& request);

    /**
     * @brief Route request using adaptive/ML logic
     */
    RoutingDecision routeAdaptive(const InferenceRequest& request);

    /**
     * @brief Route request based on model priority
     */
    RoutingDecision routePriorityBased(const InferenceRequest& request);

    /**
     * @brief Route request based on cost optimization
     */
    RoutingDecision routeCostOptimized(const InferenceRequest& request);

    /**
     * @brief Route general request using active strategy
     */
    RoutingDecision routeRequest(const InferenceRequest& request);
    
    /**
     * @brief Set routing strategy
     */
    void setRoutingStrategy(const QString& strategy); // "round-robin", "weighted", "least-connections", "adaptive"
    
    // ===== Inference Modes =====
    
    /**
     * @brief Execute in MAX mode (maximum quality)
     */
    QString executeMax(const QString& prompt, int maxTokens = 512);
    
    /**
     * @brief Execute in Research mode (detailed exploration)
     */
    QString executeResearch(const QString& prompt, int maxTokens = 1024);
    
    /**
     * @brief Execute in Deep Research mode (extended analysis)
     */
    QString executeDeepResearch(const QString& prompt, int maxTokens = 2048);
    
    /**
     * @brief Execute in Thinking mode (chain of thought reasoning)
     */
    QString executeThinking(const QString& prompt, int maxTokens = 4096);
    
    /**
     * @brief Execute with custom parameters
     */
    QString executeCustom(const QString& prompt, const QJsonObject& params);
    
    // ===== Performance Monitoring =====
    
    /**
     * @brief Get metrics for a specific model
     */
    ModelMetrics getMetrics(const QString& modelId) const;
    
    /**
     * @brief Get all model metrics
     */
    QList<ModelMetrics> getAllMetrics() const;
    
    /**
     * @brief Get overall router health
     */
    QJsonObject getHealthStatus() const;
    
    /**
     * @brief Get performance dashboard data
     */
    QJsonObject getDashboardData() const;
    
    /**
     * @brief Enable/disable performance profiling
     */
    void enableProfiling(bool enable);
    
    // ===== Model Pooling =====
    
    /**
     * @brief Get or create a model pool
     */
    bool ensureModelPooled(const QString& modelId, int poolSize = 1);
    
    /**
     * @brief Release pooled models
     */
    void releaseModelPool(const QString& modelId);
    
    /**
     * @brief Set model cache size
     */
    void setModelCacheSize(int sizeBytes);
    
    // ===== Configuration =====
    
    /**
     * @brief Save configuration to file
     */
    bool saveConfiguration(const QString& path) const;
    
    /**
     * @brief Load configuration from file
     */
    bool loadConfiguration(const QString& path);
    
    /**
     * @brief Get configuration as JSON
     */
    QJsonObject getConfiguration() const;

signals:
    /**
     * @brief Emitted when inference completes
     */
    void inferenceCompleted(const QString& modelId, const QString& result, double tokensPerSecond);
    
    /**
     * @brief Emitted when model loads successfully
     */
    void modelLoaded(const QString& modelId);
    
    /**
     * @brief Emitted when model fails to load
     */
    void modelLoadFailed(const QString& modelId, const QString& error);
    
    /**
     * @brief Emitted when model becomes unhealthy
     */
    void modelUnhealthy(const QString& modelId, const QString& reason);
    
    /**
     * @brief Emitted when metrics update
     */
    void metricsUpdated(const QJsonObject& metrics);
    
    /**
     * @brief Emitted when routing decision made
     */
    void routingDecisionMade(const QString& selectedModel, const QString& reason);
    
    /**
     * @brief Progress signal for token generation
     */
    void progressUpdated(int tokensGenerated, double tokensPerSecond);

private:
    // Internal implementation
    void updateMetrics(const QString& modelId, double latencyMs, bool success);
    void performHealthChecks();
    void selectModelByStrategy(const InferenceRequest& request);
    QString getConfigurationPath() const;
    
    // Member variables
    QMap<QString, std::shared_ptr<ModelEndpoint>> m_endpoints;
    QMap<QString, ModelMetrics> m_metrics;
    std::unique_ptr<ModelPoolManager> m_poolManager;
    std::unique_ptr<RoutingAnalytics> m_analytics;
    
    QString m_currentRoutingStrategy;
    int m_modelCacheSize;
    bool m_profilingEnabled;
    int m_totalRequests;
    
    mutable QMutex m_endpointMutex;
    mutable QMutex m_metricsMutex;
    
    QTimer* m_healthCheckTimer;
    QTimer* m_metricsFlushTimer;
    
    std::function<QString(const InferenceRequest&)> m_inferenceCallback;
};
