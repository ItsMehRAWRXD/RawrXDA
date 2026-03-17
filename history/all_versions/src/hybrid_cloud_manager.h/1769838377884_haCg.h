// hybrid_cloud_manager.h - Multi-Cloud AI Model Execution Manager
// SYNCHRONIZED WITH hybrid_cloud_manager.cpp
#ifndef HYBRID_CLOUD_MANAGER_H
#define HYBRID_CLOUD_MANAGER_H

#include <nlohmann/json.hpp>
#include <chrono> 
#include <memory>
// Forward decl
class UniversalModelRouter;

// Cloud provider information
struct CloudProvider {
    std::string providerId;        // aws, azure, gcp, huggingface, ollama
    std::string name;
    std::string endpoint;
    std::string apiKey;            // Optional for Ollama
    std::string region;
    bool isEnabled = false;
    bool isHealthy = false;
    std::chrono::system_clock::time_point lastHealthCheck;
    double costPerRequest = 0.0;     // USD (0.0 for local Ollama)
    double averageLatency = 1000.0;  // milliseconds
    nlohmann::json capabilities;
};

// Cloud model information
struct CloudModel {
    std::string modelId;
    std::string providerId;
    std::string name;
    std::string endpoint;
    std::vector<std::string> supportedTasks;
    double costPerToken = 0.0;
    int maxTokens = 4096;
    double averageLatency = 1000.0;
    bool isAvailable = true;
    nlohmann::json metadata;
};

// Execution request
struct ExecutionRequest {
    std::string requestId;
    std::string taskType;
    std::string prompt;
    std::string language;
    int maxTokens = 1024;
    double temperature = 0.7;
    nlohmann::json parameters;
    std::chrono::system_clock::time_point timestamp;
};

// Execution result
struct ExecutionResult {
    std::string requestId;
    std::string executionLocation; // "local" or provider ID
    std::string modelUsed;
    std::string response;
    int tokensUsed = 0;
    double latencyMs = 0.0;
    double cost = 0.0;
    bool success = false;
    std::string errorMessage;
    std::chrono::system_clock::time_point completedAt;
    nlohmann::json metadata;
};

// Hybrid execution decision
struct HybridExecution {
    std::string requestId;
    bool useCloud = false;
    std::string selectedProvider;
    std::string selectedModel;
    std::string reasoning;
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
    std::unordered_map<std::string, double> costByProvider;
    std::unordered_map<std::string, int> requestsByProvider;
};

// Performance metrics
struct PerformanceMetrics {
    double averageLatency = 0.0;
    double p95Latency = 0.0;
    double p99Latency = 0.0;
    int successRate = 100;     // Percentage
    int failoverCount = 0;
    std::unordered_map<std::string, double> latencyByProvider;
};

// Failover configuration
struct FailoverConfig {
    bool enabled = true;
    int maxRetries = 3;
    std::vector<std::string> providerPriority;
    bool fallbackToLocal = true;
};

class HybridCloudManager {

public:
    explicit HybridCloudManager(void* parent = nullptr);
    ~HybridCloudManager();

    // Provider management
    bool addProvider(const CloudProvider& provider);
    bool removeProvider(const std::string& providerId);
    bool configureProvider(const std::string& providerId, const std::string& apiKey, 
                          const std::string& endpoint, const std::string& region);
    void updateProvider(const CloudProvider& provider);
    std::vector<CloudProvider> getProviders() const;
    std::vector<CloudProvider> getAllProviders() const;
    CloudProvider getProvider(const std::string& providerId) const;
    std::vector<CloudProvider> getHealthyProviders() const;
    
    // Provider health
    void checkProviderHealth(const std::string& providerId);
    void checkAllProvidersHealth();
    bool isProviderHealthy(const std::string& providerId) const;
    
    // Model management
    void registerCloudModel(const CloudModel& model);
    std::vector<CloudModel> getAvailableCloudModels() const;
    std::vector<CloudModel> getModelsForProvider(const std::string& providerId) const;
    CloudModel selectBestCloudModel(const ExecutionRequest& request);
    
    // Hybrid execution
    HybridExecution planExecution(const ExecutionRequest& request,
                                 const std::string& executionType = "auto");
    ExecutionResult execute(const ExecutionRequest& request);
    ExecutionResult executeLocal(const ExecutionRequest& request);
    ExecutionResult executeCloud(const ExecutionRequest& request, 
                                 const std::string& providerId,
                                 const std::string& modelId);
    
    // Cloud-specific execution (from cpp)
    ExecutionResult executeOnCloud(const ExecutionRequest& request, 
                                   const std::string& providerId,
                                   const std::string& modelId);
    ExecutionResult executeOnOllama(const ExecutionRequest& request);
    ExecutionResult executeOnHuggingFace(const ExecutionRequest& request, const std::string& modelId);
    ExecutionResult executeOnAWS(const ExecutionRequest& request, const std::string& modelId);
    ExecutionResult executeOnAzure(const ExecutionRequest& request, const std::string& modelId);
    ExecutionResult executeOnGCP(const ExecutionRequest& request, const std::string& modelId);
    
    // Intelligent routing
    bool shouldUseCloudExecution(const ExecutionRequest& request);
    std::string selectOptimalProvider(const ExecutionRequest& request);
    double calculateExecutionCost(const std::string& providerId, 
                                  const std::string& modelId,
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
    double getTotalCost() const;
    void setCostLimit(double dailyLimitUSD, double monthlyLimitUSD);
    void setCostThreshold(double thresholdUSD);
    bool isWithinCostLimits() const;
    void resetCostMetrics();
    
    // Performance monitoring
    PerformanceMetrics getPerformanceMetrics() const;
    double getAverageLatency(const std::string& providerId = "") const;
    int getSuccessRate() const;
    
    // Execution history
    std::vector<ExecutionResult> getExecutionHistory(int limit = 0) const;
    void clearExecutionHistory();
    
    // Configuration
    void setPreferLocal(bool prefer);
    void setCloudFallbackEnabled(bool enabled);
    void setCostThresholdPerRequest(double thresholdUSD);
    void setLatencyThreshold(int thresholdMs);
    void setAutoScaling(bool enabled);
    void enableLocalExecution(bool enable);
    void setHealthCheckInterval(int milliseconds);
    void setMaxRetries(int retries);
    
    // Cloud switching
    bool switchToCloud(const std::string& reason);
    bool switchToLocal(const std::string& reason);
    bool isUsingCloud() const;
    
    // API key management
    void setAPIKey(const std::string& providerId, const std::string& apiKey);
    void setAWSCredentials(const std::string& accessKey, const std::string& secretKey,
                          const std::string& region);
    void setAzureCredentials(const std::string& subscriptionId, const std::string& apiKey);
    void setGCPCredentials(const std::string& projectId, const std::string& apiKey);
    void setHuggingFaceKey(const std::string& apiKey);
    
    // Request queuing
    void queueRequest(const ExecutionRequest& request);
    std::vector<ExecutionRequest> getPendingRequests() const;
    void processPendingRequests();


    void executionStarted(const std::string& requestId);
    void executionComplete(const ExecutionResult& result);
    void providerHealthChanged(const std::string& providerId, bool isHealthy);
    void costLimitReached(const std::string& limitType);
    void failoverTriggered(const std::string& fromProvider, const std::string& toProvider);
    void cloudSwitched(bool usingCloud);
    void errorOccurred(const std::string& error);
    void healthCheckCompleted();

private:
    void onNetworkReplyFinished(void** reply);
    void onHealthCheckTimerTimeout();

private:
    // Setup
    void setupDefaultProviders();
    
    // Execution helpers
    ExecutionResult sendCloudRequest(const std::string& providerId,
                                    const std::string& modelId,
                                    const ExecutionRequest& request);
    void* createRequestPayload(const ExecutionRequest& request,
                                    const std::string& providerId);
    ExecutionResult parseCloudResponse(const std::vector<uint8_t>& data,
                                      const std::string& providerId);
    
    // Routing algorithms
    double calculateProviderScore(const CloudProvider& provider,
                                 const ExecutionRequest& request);
    double calculateCostEfficiency(const CloudProvider& provider,
                                   int estimatedTokens);
    double calculateLatencyScore(const CloudProvider& provider);
    double calculateReliabilityScore(const CloudProvider& provider);
    
    // Health monitoring
    bool validateCloudHealth(const std::string& providerId);
    void recordProviderLatency(const std::string& providerId, double latencyMs);
    void recordProviderFailure(const std::string& providerId);
    
    // Cost tracking
    void recordCost(const std::string& providerId, double cost);
    void updateCostMetrics(const ExecutionResult& result);
    
    // Performance tracking
    void recordLatency(double latencyMs);
    void recordSuccess(bool success);
    void updatePerformanceMetrics(const ExecutionResult& result);
    
    // Failover logic
    std::string getNextFallbackProvider(const std::string& currentProvider);
    bool shouldRetry(int attemptNumber, const std::string& errorType);
    
    // Data members - SYNCHRONIZED with cpp
    std::unordered_map<std::string, CloudProvider> providers;
    std::vector<CloudModel> cloudModels;
    std::vector<ExecutionResult> executionHistory;  // Changed to std::vector from std::unordered_map
    std::vector<ExecutionRequest> requestQueue;
    
    CostMetrics costMetrics;
    PerformanceMetrics performanceMetrics;
    FailoverConfig failoverConfig;
    
    void** networkManager;
    std::unordered_map<std::string, void**> activeRequests;
    
    void** healthCheckTimer;
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
    
    std::unique_ptr<UniversalModelRouter> router;
};

#endif // HYBRID_CLOUD_MANAGER_H

