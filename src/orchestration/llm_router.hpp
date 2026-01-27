#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QObject>
#include <memory>
#include <atomic>
#include <atomic>

/**
 * @class ModelCapabilities
 * @brief Defines model capability scores for different task types
 */
struct ModelCapabilities {
    int reasoning = 0;           // Logic & analysis (0-100)
    int coding = 0;              // Code generation (0-100)
    int planning = 0;            // Task planning (0-100)
    int creativity = 0;          // Novel solutions (0-100)
    int speed = 0;               // Response latency (inverted: 100 = fastest, 0-100)
    int costEfficiency = 0;      // Token cost (inverted: 100 = cheapest, 0-100)
    
    /**
     * Get capability score for a specific task type
     */
    int getCapabilityScore(const QString& capability) const {
        if (capability == "reasoning") return reasoning;
        if (capability == "coding") return coding;
        if (capability == "planning") return planning;
        if (capability == "creativity") return creativity;
        if (capability == "speed") return speed;
        if (capability == "cost") return costEfficiency;
        // Balanced score
        return (reasoning + coding + planning + creativity + speed + costEfficiency) / 6;
    }
};

/**
 * @struct ModelInfo
 * @brief Complete model metadata and configuration
 */
struct ModelInfo {
    QString id;                  // Model identifier ("gpt-4", "claude-3", "llama70b")
    QString provider;            // Provider ("openai", "anthropic", "ollama", "local")
    QString endpoint;            // API endpoint or localhost:port
    QString apiKey;              // Optional API key for authentication
    
    int contextWindow = 8192;    // Max context window in tokens
    double avgTokenCost = 0.0;   // Cost per 1000 tokens
    double avgLatencyMs = 0.0;   // Average response time in milliseconds
    
    ModelCapabilities capabilities;
    bool available = true;       // Is model currently available?
};

/**
 * @struct RoutingDecision
 * @brief Result of model routing decision
 */
struct RoutingDecision {
    QString selectedModelId;           // ID of selected model
    int confidenceScore = 0;           // 0-100 confidence in selection
    QString routingReason;             // Explanation of routing decision
    QStringList alternativeModels;     // Other good options
    ModelInfo selectedInfo;            // Full model info
    
    // Metadata
    qint64 decisionTimeMs = 0;
    QString routingStrategy;           // Strategy used for routing
};

/**
 * @struct EnsembleResult
 * @brief Result of ensemble routing (multiple models)
 */
struct EnsembleResult {
    QStringList selectedModels;        // IDs of selected models
    QJsonArray responses;              // Responses from each model
    QString consensus;                 // Final agreed-upon response
    float agreementLevel = 0.0;        // How much models agreed (0-1)
    float finalConfidence = 0.0;       // Confidence in final result
};

/**
 * @class LLMRouter
 * @brief Intelligent LLM model router for optimal model selection
 * 
 * Provides capabilities for:
 * - Single model routing (select best model for task)
 * - Ensemble routing (use multiple models for consensus)
 * - Performance tracking and optimization
 * - Automatic fallback on model failure
 * - Cost-aware model selection
 */
class LLMRouter : public QObject {
    Q_OBJECT

public:
    explicit LLMRouter(QObject* parent = nullptr);
        ~LLMRouter() override;
    
    // ===== Model Registration =====
    
    /**
     * Register a model with the router
     */
    void registerModel(const ModelInfo& model);
    
    /**
     * Unregister a model
     */
    void unregisterModel(const QString& modelId);
    
    /**
     * Get model info
     */
    ModelInfo getModel(const QString& modelId) const;
    
    /**
     * Get list of available model IDs
     */
    QStringList getAvailableModels() const;
    
    // ===== Single Model Routing =====
    
    /**
     * Route a task to the optimal single model
     * @param taskDescription What the task is about
     * @param preferredCapability Which capability is most important ("reasoning", "coding", "planning", "speed", "cost", "balanced")
     * @param maxCostTokens Maximum tokens willing to use (0 = unlimited)
     * @return Routing decision with selected model
     */
    RoutingDecision route(
        const QString& taskDescription,
        const QString& preferredCapability = "balanced",
        int maxCostTokens = 0
    );
    
    // ===== Ensemble Routing =====
    
    /**
     * Route to multiple models for consensus
     * @param taskDescription The task description
     * @param numModels How many models to use
     * @param consensusMethod How to combine responses ("voting", "weighted", "unanimous")
     * @return Ensemble result with multiple model responses
     */
    EnsembleResult routeEnsemble(
        const QString& taskDescription,
        int numModels = 3,
        const QString& consensusMethod = "voting"
    );
    
    // ===== Performance Tracking =====
    
    /**
     * Record performance metrics after a model completes a task
     */
    void recordPerformance(
        const QString& modelId,
        int taskDurationMs,
        int tokensUsed,
        double qualityScore  // 0.0-1.0
    );
    
    // ===== Health & Status =====
    
    /**
     * Get status of a specific model
     */
    QJsonObject getModelStatus(const QString& modelId) const;
    
    /**
     * Get status of all models
     */
    QJsonArray getAllModelStatus() const;
    
    /**
     * Mark model as failed and trigger fallback
     */
    void handleModelFailure(const QString& modelId, const QString& error);
    
    /**
     * Get fallback model when primary fails
     */
    RoutingDecision getFallbackModel(const QString& failedModelId);
    
    // ===== Configuration =====
    
    /**
     * Set routing strategy
     */
    void setRoutingStrategy(const QString& strategy);
    
    /**
     * Enable/disable load balancing
     */
    void setLoadBalancingEnabled(bool enabled);
    
    /**
     * Enable/disable cost optimization
     */
    void setCostOptimizationEnabled(bool enabled);

signals:
    void modelRegistered(const QString& modelId);
    void modelUnregistered(const QString& modelId);
    void routingDecisionMade(const RoutingDecision& decision);
    void modelHealthChanged(const QString& modelId, bool healthy);
    void failoverTriggered(const QString& failedModel, const QString& fallbackModel);
    void routingStatsUpdated(const QJsonObject& stats);

private:
    struct PerformanceMetrics {
        mutable std::atomic<int> totalRequests{0};
        mutable std::atomic<int> successfulRequests{0};
        mutable std::atomic<int> failedRequests{0};
        mutable std::atomic<int> totalLatencyMs{0};
        mutable std::atomic<int> totalTokensUsed{0};
        double averageQualityScore = 0.0;
        QDateTime lastUsed;
    };
    
    QMap<QString, ModelInfo> m_models;
    QMap<QString, PerformanceMetrics*> m_metrics;  // Raw pointers, cleaned up in destructor
    
    // Configuration
    bool m_loadBalancingEnabled = true;
    bool m_costOptimizationEnabled = true;
    QString m_routingStrategy = "best-capability";
    
    // ===== Internal Scoring Methods =====
    
    int calculateTaskRelevanceScore(const ModelInfo& model, const QString& capability);
    int calculateCostEfficiencyScore(const ModelInfo& model, int maxTokens);
    int calculateLatencyScore(const ModelInfo& model);
    int calculateReliabilityScore(const QString& modelId);
    
    // Load balancing
    QString selectFromCandidates(const QStringList& candidates);
};
