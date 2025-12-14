// hybrid_cloud_manager.h - Multi-Cloud AI Model Execution Manager
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

// Cloud provider information
struct CloudProvider {
    QString providerId;        // aws, azure, gcp, huggingface, ollama
    QString name;
    QString endpoint;
    QString apiKey;            // Optional for Ollama
    QString region;
    bool isEnabled;
    bool isHealthy;
    QDateTime lastHealthCheck;
    double costPerRequest;     // USD (0.0 for local Ollama)
    double averageLatency;     // milliseconds
    QJsonObject capabilities;
};

// Cloud model information
struct CloudModel {
    QString modelId;
    QString providerId;
    QString name;
    QString endpoint;
    QStringList supportedTasks;
    double costPerToken;
    int maxTokens;
    double averageLatency;
    bool isAvailable;
    QJsonObject metadata;
};

// Execution request
struct ExecutionRequest {
    QString requestId;
    QString taskType;
    QString prompt;
    QString language;
    int maxTokens;
    double temperature;
    QJsonObject parameters;
    QDateTime timestamp;
};

// Execution result
struct ExecutionResult {
    QString requestId;
    QString executionLocation; // "local" or provider ID
    QString modelUsed;
    QString response;
    int tokensUsed;
    double latencyMs;
    double cost;
    bool success;
    QString errorMessage;
    QDateTime completedAt;
    QJsonObject metadata;
};

// Hybrid execution decision
struct HybridExecution {
    QString requestId;
    bool useCloud;
    QString selectedProvider;
    QString selectedModel;
    QString reasoning;
    double estimatedCost;
    double estimatedLatency;
    double confidenceScore;
};

// Cost tracking
struct CostMetrics {
    double totalCostUSD;
    double todayCostUSD;
    double monthCostUSD;
    int totalRequests;
    int cloudRequests;
    int localRequests;
    QHash<QString, double> costByProvider;
    QHash<QString, int> requestsByProvider;
};

// Performance metrics
struct PerformanceMetrics {
    double averageLatency;
    double p95Latency;
    double p99Latency;
    int successRate;           // Percentage
    int failoverCount;
    QHash<QString, double> latencyByProvider;
};

// Failover configuration
struct FailoverConfig {
    bool enabled;
    int maxRetries;
    int timeoutMs;
    QVector<QString> fallbackProviders;
    bool autoSwitchOnFailure;
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
    
    // Intelligent routing
    bool shouldUseCloud(const ExecutionRequest& request);
    QString selectOptimalProvider(const ExecutionRequest& request);
    double calculateExecutionCost(const QString& providerId, 
                                  const QString& modelId,
                                  int estimatedTokens);
    
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
    
    // Cloud-specific operations
    ExecutionResult executeAWS(const ExecutionRequest& request);
    ExecutionResult executeAzure(const ExecutionRequest& request);
    ExecutionResult executeGCP(const ExecutionRequest& request);
    ExecutionResult executeHuggingFace(const ExecutionRequest& request);
    
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

private slots:
    void onNetworkReplyFinished(QNetworkReply* reply);
    void onHealthCheckTimerTimeout();

private:
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
    
    // Data members
    QHash<QString, CloudProvider> providers;
    QVector<CloudModel> cloudModels;
    QHash<QString, ExecutionResult> executionHistory;
    QVector<ExecutionRequest> requestQueue;
    
    CostMetrics costMetrics;
    PerformanceMetrics performanceMetrics;
    FailoverConfig failoverConfig;
    
    QNetworkAccessManager* networkManager;
    QHash<QString, QNetworkReply*> activeRequests;
    
    QTimer* healthCheckTimer;
    
    bool preferLocal;
    bool cloudFallbackEnabled;
    double costThresholdPerRequest;
    int latencyThreshold;
    bool autoScalingEnabled;
    bool currentlyUsingCloud;
    
    double dailyCostLimit;
    double monthlyCostLimit;
    
    // Configuration
    static constexpr int HEALTH_CHECK_INTERVAL_MS = 30000; // 30 seconds
    static constexpr double DEFAULT_COST_THRESHOLD = 0.01;  // $0.01 per request
    static constexpr int DEFAULT_LATENCY_THRESHOLD = 2000;  // 2 seconds
    static constexpr int MAX_RETRY_ATTEMPTS = 3;
};

#endif // HYBRID_CLOUD_MANAGER_H
