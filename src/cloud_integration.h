/**
 * @file cloud_integration.h
 * @brief Unified Cloud Integration API for RawrXD IDEIncludes all cloud functionality in one header
 * 
 * This singleheader provides access to:
 * - HybridCloudManager for intelligent routing
 * - HFHubClient for model discovery
 * - Cloud provider configurations
 * - Cost and performance tracking
 * 
 * Usage:
 *   #include "cloud_integration.h"
 *   
 *   auto manager = std::make_unique<RawrXD::HybridCloudManager>();
 *   manager->setAWSCredentials(...);
 *   auto result = manager->execute(request);
 */

#ifndef CLOUD_INTEGRATION_H
#define CLOUD_INTEGRATION_H

#include "hybrid_cloud_manager.h"
#include "hf_hub_client.cpp"
#include "cloud_provider_config.h"
#include <memory>
#include <functional>

namespace RawrXD {
namespace Cloud {

/**
 * Unified Cloud Integration Service
 * 
 * Provides simplified access to all cloud integration features:
 * - Multi-cloud routing
 * - Model discovery & downloads
 * - Cost management
 * - Health monitoring
 * - Failover & retry
 */
class CloudIntegrationService {
public:
    /**
     * Initialize cloud service with default configuration
     */
    CloudIntegrationService() 
        : m_cloudManager(std::make_unique<HybridCloudManager>()),
          m_hfClient(std::make_unique<HFHubClient>()) {
        
        // Load configuration from environment variables
        CloudProviderConfigManager::loadFromEnvironment();
        
        // Initialize cloud manager with default providers
        initializeDefaultProviders();
    }
    
    ~CloudIntegrationService() = default;
    
    //=========================================================================
    // Cloud Manager Access
    //=========================================================================
    
    /**
     * Get access to the cloud manager for custom configuration
     */
    HybridCloudManager* getCloudManager() { return m_cloudManager.get(); }
    const HybridCloudManager* getCloudManager() const { return m_cloudManager.get(); }
    
    /**
     * Get access to HuggingFace client
     */
    HFHubClient* getHFClient() { return m_hfClient.get(); }
    const HFHubClient* getHFClient() const { return m_hfClient.get(); }
    
    //=========================================================================
    // Configuration Helpers
    //=========================================================================
    
    /**
     * Configure all cloud providers from environment variables
     * 
     * Expected environment variables:
     * - AWS_ACCESS_KEY_ID, AWS_SECRET_ACCESS_KEY, AWS_REGION
     * - AZURE_API_KEY, AZURE_SUBSCRIPTION_ID
     * - GCP_PROJECT_ID, GCP_CREDENTIALS_JSON
     * - HUGGINGFACE_HUB_TOKEN
     * - ANTHROPIC_API_KEY
     */
    void configureFromEnvironment() {
        auto& aws = CloudProviderConfigManager::getAWSConfig();
        auto& azure = CloudProviderConfigManager::getAzureConfig();
        auto& gcp = CloudProviderConfigManager::getGCPConfig();
        auto& hf = CloudProviderConfigManager::getHuggingFaceConfig();
        auto& anthropic = CloudProviderConfigManager::getAnthropicConfig();
        
        if (!aws.accessKeyId.empty()) {
            m_cloudManager->setAWSCredentials(
                aws.accessKeyId, 
                aws.secretAccessKey, 
                aws.region
            );
        }
        
        if (!azure.apiKey.empty()) {
            m_cloudManager->setAzureCredentials(
                azure.subscriptionId, 
                azure.apiKey
            );
        }
        
        if (!gcp.projectId.empty()) {
            m_cloudManager->setGCPCredentials(
                gcp.projectId, 
                gcp.accessToken
            );
        }
        
        if (!hf.apiToken.empty()) {
            m_cloudManager->setHuggingFaceKey(hf.apiToken);
        }
    }
    
    /**
     * Quick setup with API keys
     * 
     * Example:
     *   service.quickSetup({
     *       {"aws_key", "AKIA..."},
     *       {"aws_secret", "..."},
     *       {"azure_key", "..."},
     *       {"gcp_token", "..."}
     *   });
     */
    void quickSetup(const std::unordered_map<std::string, std::string>& config) {
        if (config.count("aws_key") && config.count("aws_secret")) {
            std::string region = config.count("aws_region") ? 
                config.at("aws_region") : "us-west-2";
            m_cloudManager->setAWSCredentials(
                config.at("aws_key"),
                config.at("aws_secret"),
                region
            );
        }
        
        if (config.count("azure_key")) {
            std::string subId = config.count("azure_subscription") ?
                config.at("azure_subscription") : "";
            m_cloudManager->setAzureCredentials(subId, config.at("azure_key"));
        }
        
        if (config.count("gcp_project") && config.count("gcp_token")) {
            m_cloudManager->setGCPCredentials(
                config.at("gcp_project"),
                config.at("gcp_token")
            );
        }
        
        if (config.count("hf_token")) {
            m_cloudManager->setHuggingFaceKey(config.at("hf_token"));
        }
    }
    
    //=========================================================================
    // Convenience Methods
    //=========================================================================
    
    /**
     * Simple execution with automatic routing
     * 
     * @param prompt User prompt
     * @param taskType Type of task (chat, code_generation, complex_reasoning)
     * @param maxTokens Maximum response length
     * @return Response text or error
     * 
     * Example:
     *   auto response = service.execute("What is AI?", "chat", 512);
     */
    std::string execute(const std::string& prompt,
                       const std::string& taskType = "chat",
                       int maxTokens = 1024) {
        ExecutionRequest request;
        request.requestId = generateRequestId();
        request.prompt = prompt;
        request.taskType = taskType;
        request.maxTokens = maxTokens;
        
        auto result = m_cloudManager->execute(request);
        return result.success ? result.response : ("ERROR: " + result.errorMessage);
    }
    
    /**
     * Execute with cost limit check
     * 
     * @param prompt User prompt
     * @param maxCost Maximum cost in USD
     * @param taskType Type of task
     * @return Response text or error
     */
    std::string executeWithCostLimit(const std::string& prompt,
                                     double maxCost = 0.10,
                                     const std::string& taskType = "chat") {
        ExecutionRequest request;
        request.requestId = generateRequestId();
        request.prompt = prompt;
        request.taskType = taskType;
        
        auto plan = m_cloudManager->planExecution(request, "auto");
        
        if (plan.estimatedCost > maxCost) {
            return "ERROR: Estimated cost $" + std::to_string(plan.estimatedCost) +
                   " exceeds limit $" + std::to_string(maxCost);
        }
        
        auto result = m_cloudManager->execute(request);
        return result.success ? result.response : ("ERROR: " + result.errorMessage);
    }
    
    /**
     * Search for and list models
     * 
     * @param query Search query
     * @param limit Max results
     * @return Vector of model metadata
     */
    std::vector<HFHubClient::ModelMetadata> searchModels(
        const std::string& query,
        int limit = 10) {
        
        auto& hfConfig = CloudProviderConfigManager::getHuggingFaceConfig();
        return m_hfClient->searchModels(query, limit, hfConfig.apiToken);
    }
    
    /**
     * Download a model with progress tracking
     * 
     * @param repo_id HuggingFace repository ID
     * @param filename File to download
     * @param outputDir Output directory
     * @param onProgress Progress callback
     * @return Success status
     */
    bool downloadModel(const std::string& repo_id,
                      const std::string& filename,
                      const std::string& outputDir,
                      std::function<void(uint64_t, uint64_t)> onProgress = nullptr) {
        
        auto& hfConfig = CloudProviderConfigManager::getHuggingFaceConfig();
        return m_hfClient->downloadModel(
            repo_id, 
            filename, 
            outputDir, 
            onProgress,
            hfConfig.apiToken
        );
    }
    
    //=========================================================================
    // Analytics & Monitoring
    //=========================================================================
    
    /**
     * Get current cost metrics
     */
    CostMetrics getCostMetrics() const {
        return m_cloudManager->getCostMetrics();
    }
    
    /**
     * Get current performance metrics
     */
    PerformanceMetrics getPerformanceMetrics() const {
        return m_cloudManager->getPerformanceMetrics();
    }
    
    /**
     * Print summary of cloud usage and costs
     */
    void printSummary() const {
        auto costs = getCostMetrics();
        auto perf = getPerformanceMetrics();
        
        std::cout << "\n========================================\n";
        std::cout << "  Cloud Integration Summary\n";
        std::cout << "========================================\n";
        std::cout << "\n💰 Costs:\n";
        std::cout << "  Today:   $" << std::fixed << std::setprecision(2) << costs.todayCostUSD << "\n";
        std::cout << "  Month:   $" << costs.monthCostUSD << "\n";
        std::cout << "  Total:   $" << costs.totalCostUSD << "\n";
        std::cout << "\n📊 Requests:\n";
        std::cout << "  Total:   " << costs.totalRequests << "\n";
        std::cout << "  Cloud:   " << costs.cloudRequests << "\n";
        std::cout << "  Local:   " << costs.localRequests << "\n";
        std::cout << "\n⚡ Performance:\n";
        std::cout << "  Avg Latency:  " << perf.averageLatency << " ms\n";
        std::cout << "  Success Rate: " << perf.successRate << "%\n";
        std::cout << "  Failovers:    " << perf.failoverCount << "\n";
        std::cout << "\n========================================\n\n";
    }
    
    //=========================================================================
    // Health & Status
    //=========================================================================
    
    /**
     * Check health of all cloud providers
     * 
     * @return Vector of healthy providers
     */
    std::vector<CloudProvider> checkHealth() {
        m_cloudManager->checkAllProvidersHealth();
        return m_cloudManager->getHealthyProviders();
    }
    
    /**
     * Get list of available providers
     */
    std::vector<CloudProvider> getAvailableProviders() const {
        return m_cloudManager->getProviders();
    }
    
    /**
     * Check if a specific provider is healthy
     */
    bool isProviderHealthy(const std::string& providerId) const {
        return m_cloudManager->isProviderHealthy(providerId);
    }
    
    //=========================================================================
    // Configuration Setters
    //=========================================================================
    
    void setDailyBudget(double usdAmount) {
        m_cloudManager->setCostLimit(usdAmount, 30 * usdAmount);
    }
    
    void setMonthlyBudget(double usdAmount) {
        m_cloudManager->setCostLimit(usdAmount / 30, usdAmount);
    }
    
    void setPreferLocal(bool prefer) {
        m_cloudManager->setPreferLocal(prefer);
    }
    
    void setMaxRetries(int retries) {
        m_cloudManager->setMaxRetries(retries);
    }
    
    void enableAutoFailover(bool enable) {
        m_cloudManager->enableFailover(enable);
    }
    
private:
    std::unique_ptr<HybridCloudManager> m_cloudManager;
    std::unique_ptr<HFHubClient> m_hfClient;
    
    /**
     * Initialize default cloud providers
     */
    void initializeDefaultProviders() {
        // Cloud providers are initialized in HybridCloudManager constructor
        // This method can be extended for additional setup
    }
    
    /**
     * Generate unique request ID
     */
    static std::string generateRequestId() {
        static int counter = 0;
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);
        
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "req-%ld-%d", timestamp, ++counter);
        return std::string(buffer);
    }
};

} // namespace Cloud
} // namespace RawrXD

// Convenience using declarations
using RawrXD::HybridCloudManager;
using RawrXD::ExecutionRequest;
using RawrXD::ExecutionResult;
using RawrXD::CloudProvider;
using RawrXD::CloudModel;
using RawrXD::CostMetrics;
using RawrXD::PerformanceMetrics;
using RawrXD::HFHubClient;

#endif // CLOUD_INTEGRATION_H
