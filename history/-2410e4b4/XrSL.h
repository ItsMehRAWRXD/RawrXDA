// hybrid_cloud_manager.h - Multi-Cloud AI Model Execution Manager
// SYNCHRONIZED WITH hybrid_cloud_manager.cpp
#ifndef HYBRID_CLOUD_MANAGER_H
#define HYBRID_CLOUD_MANAGER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDateTime>
#include <QTimer>
#include <QElapsedTimer>
#include <QEventLoop>

// Cloud provider information
struct CloudProvider {
    QString providerId;        // aws, azure, gcp, huggingface, ollama
    QString name;
    QString endpoint;
    QString apiKey;            // Optional for Ollama
    QString region;
    bool isEnabled = false;
    bool isHealthy = false;
    QDateTime lastHealthCheck;
    double costPerRequest = 0.0;     // USD (0.0 for local Ollama)
    double averageLatency = 1000.0;  // milliseconds
    QJsonObject capabilities;
};

// Cloud model information
struct CloudModel {
    QString modelId;
    QString providerId;
    QString name;
    QString endpoint;
    QStringList supportedTasks;
    double costPerToken = 0.0;
    int maxTokens = 4096;
    double averageLatency = 1000.0;
    bool isAvailable = true;
    QJsonObject metadata;
};

// Execution request
struct ExecutionRequest {
    QString requestId;
    QString taskType;
    QString prompt;
    QString language;
    int maxTokens = 1024;
    double temperature = 0.7;
    QJsonObject parameters;
    QDateTime timestamp;
};

// Execution result
struct ExecutionResult {
    QString requestId;
    QString executionLocation; // "local" or provider ID
    QString modelUsed;
    QString response;
    int tokensUsed = 0;
    double latencyMs = 0.0;
    double cost = 0.0;
    bool success = false;
    QString errorMessage;
    QDateTime completedAt;
    QJsonObject metadata;
};

// Hybrid execution decision
struct HybridExecution {
    QString requestId;
    bool useCloud = false;
    QString selectedProvider;
    QString selectedModel;
    QString reasoning;
    double estimatedCost = 0.0;
    double estimatedLatency = 0.0;
    double confidenceScore = 0.0;
};

// Cost tracking
struct CostMetrics {
    double totalCostUSD = 0.0;
    double todayCostUSD = 0.0;
    double monthCostUSD = 0.0;
    int totalRequests = 0;
    int cloudRequests = 0;
    int localRequests = 0;
    QHash<QString, double> costByProvider;
    QHash<QString, int> requestsByProvider;
};

// Performance metrics
struct PerformanceMetrics {
    double averageLatency = 0.0;
    double p95Latency = 0.0;
    double p99Latency = 0.0;
    int successRate = 100;     // Percentage
    int failoverCount = 0;
    QHash<QString, double> latencyByProvider;
};

// Failover configuration
struct FailoverConfig {
    bool enabled = true;
    int maxRetries = 3;
    int timeoutMs = 30000;
    QVector<QString> fallbackProviders;
    bool autoSwitchOnFailure = true;
};

class HybridCloudManager : public QObject {
    Q_OBJECT

public:
    explicit HybridCloudManager(QObject* parent = nullptr);
    ~HybridCloudManager();

    // Provider management
    void addProvider(const CloudProvider& provider);
    void removeProvider(const QString& providerId);
    void updateProvider(const CloudProvider& provider);
    QVector<CloudProvider> getProviders() const;
    CloudProvider getProvider(const QString& providerId) const;
    QVector<CloudProvider> getHealthyProviders() const;
    
    // Provider health
    void checkProviderHealth(const QString& providerId);
    void checkAllProvidersHealth();
    bool isProviderHealthy(const QString& providerId) const;
    
    // Model management
    void registerCloudModel(const CloudModel& model);
    QVector<CloudModel> getAvailableCloudModels() const;
    QVector<CloudModel> getModelsForProvider(const QString& providerId) const;
    CloudModel selectBestCloudModel(const ExecutionRequest& request);
    
    // Hybrid execution
    HybridExecution planExecution(const ExecutionRequest& request,
                                 const QString& executionType = "auto");
    ExecutionResult execute(const ExecutionRequest& request);
    ExecutionResult executeLocal(const ExecutionRequest& request);
    ExecutionResult executeCloud(const ExecutionRequest& request, 
                                 const QString& providerId,
                                 const QString& modelId);
    
    // Cloud-specific execution (from cpp)
    ExecutionResult executeOnOllama(const ExecutionRequest& request, const QString& modelId);
    ExecutionResult executeOnHuggingFace(const ExecutionRequest& request, const QString& modelId);
    ExecutionResult executeOnAWS(const ExecutionRequest& request, const QString& modelId);
    ExecutionResult executeOnAzure(const ExecutionRequest& request, const QString& modelId);
    ExecutionResult executeOnGCP(const ExecutionRequest& request, const QString& modelId);
    
    // Intelligent routing
    bool shouldUseCloudExecution(const ExecutionRequest& request);
    QString selectOptimalProvider(const ExecutionRequest& request);
    double calculateExecutionCost(const QString& providerId, 
                                  const QString& modelId,
                                  int estimatedTokens);
    
    // Execution tracking
    void recordExecution(const ExecutionResult& result);
    
    // Failover and retry
    void enableFailover(bool enable);
    void setFailoverConfig(const FailoverConfig& config);
    ExecutionResult executeWithFailover(const ExecutionRequest& request);
    bool retryWithFallback(const ExecutionRequest& request, 
                          ExecutionResult& result);
    
    // Cost management
    CostMetrics getCostMetrics() const;
    double getTodayCost() const;
    double getMonthCost() const;
    void setCostLimit(double dailyLimitUSD, double monthlyLimitUSD);
    bool isWithinCostLimits() const;
    void resetCostMetrics();
    
    // Performance monitoring
    PerformanceMetrics getPerformanceMetrics() const;
    double getAverageLatency(const QString& providerId = "") const;
    int getSuccessRate() const;
    
    // Configuration
    void setPreferLocal(bool prefer);
    void setCloudFallbackEnabled(bool enabled);
    void setCostThresholdPerRequest(double thresholdUSD);
    void setLatencyThreshold(int thresholdMs);
    void setAutoScaling(bool enabled);
    
    // Cloud switching
    bool switchToCloud(const QString& reason);
    bool switchToLocal(const QString& reason);
    bool isUsingCloud() const;
    
    // API key management
    void setAPIKey(const QString& providerId, const QString& apiKey);
    void setAWSCredentials(const QString& accessKey, const QString& secretKey,
                          const QString& region);
    void setAzureCredentials(const QString& subscriptionId, const QString& apiKey);
    void setGCPCredentials(const QString& projectId, const QString& apiKey);
    void setHuggingFaceKey(const QString& apiKey);
    
    // Request queuing
    void queueRequest(const ExecutionRequest& request);
    QVector<ExecutionRequest> getPendingRequests() const;
    void processPendingRequests();

signals:
    void executionStarted(const QString& requestId);
    void executionComplete(const ExecutionResult& result);
    void providerHealthChanged(const QString& providerId, bool isHealthy);
    void costLimitReached(const QString& limitType);
    void failoverTriggered(const QString& fromProvider, const QString& toProvider);
    void cloudSwitched(bool usingCloud);
    void errorOccurred(const QString& error);
    void healthCheckCompleted();

private slots:
    void onNetworkReplyFinished(QNetworkReply* reply);
    void onHealthCheckTimerTimeout();

private:
    // Setup
    void setupDefaultProviders();
    
    // Execution helpers
    ExecutionResult sendCloudRequest(const QString& providerId,
                                    const QString& modelId,
                                    const ExecutionRequest& request);
    QJsonObject createRequestPayload(const ExecutionRequest& request,
                                    const QString& providerId);
    ExecutionResult parseCloudResponse(const QByteArray& data,
                                      const QString& providerId);
    
    // Routing algorithms
    double calculateProviderScore(const CloudProvider& provider,
                                 const ExecutionRequest& request);
    double calculateCostEfficiency(const CloudProvider& provider,
                                   int estimatedTokens);
    double calculateLatencyScore(const CloudProvider& provider);
    double calculateReliabilityScore(const CloudProvider& provider);
    
    // Health monitoring
    bool validateCloudHealth(const QString& providerId);
    void recordProviderLatency(const QString& providerId, double latencyMs);
    void recordProviderFailure(const QString& providerId);
    
    // Cost tracking
    void recordCost(const QString& providerId, double cost);
    void updateCostMetrics(const ExecutionResult& result);
    
    // Performance tracking
    void recordLatency(double latencyMs);
    void recordSuccess(bool success);
    void updatePerformanceMetrics(const ExecutionResult& result);
    
    // Failover logic
    QString getNextFallbackProvider(const QString& currentProvider);
    bool shouldRetry(int attemptNumber, const QString& errorType);
    
    // Data members - SYNCHRONIZED with cpp
    QHash<QString, CloudProvider> providers;
    QVector<CloudModel> cloudModels;
    QVector<ExecutionResult> executionHistory;  // Changed to QVector from QHash
    QVector<ExecutionRequest> requestQueue;
    
    CostMetrics costMetrics;
    PerformanceMetrics performanceMetrics;
    FailoverConfig failoverConfig;
    
    QNetworkAccessManager* networkManager;
    QHash<QString, QNetworkReply*> activeRequests;
    
    QTimer* healthCheckTimer;
    int healthCheckIntervalMs;
    
    bool preferLocal = true;
    bool cloudFallbackEnabled = true;
    bool localExecutionEnabled = true;   // Added from cpp
    double costThresholdPerRequest = 0.01;
    double costThresholdUSD = 0.01;       // Added from cpp  
    int latencyThreshold = 2000;
    int maxRetries = 3;                   // Added from cpp
    bool autoScalingEnabled = false;
    bool currentlyUsingCloud = false;
    double totalCostUSD = 0.0;            // Added from cpp
    
    double dailyCostLimit = 10.0;
    double monthlyCostLimit = 100.0;
    
    // Configuration constants
    static constexpr int HEALTH_CHECK_INTERVAL_MS = 30000; // 30 seconds
    static constexpr double DEFAULT_COST_THRESHOLD = 0.01;  // $0.01 per request
    static constexpr int DEFAULT_LATENCY_THRESHOLD = 2000;  // 2 seconds
    static constexpr int MAX_RETRY_ATTEMPTS = 3;
};

#endif // HYBRID_CLOUD_MANAGER_H
