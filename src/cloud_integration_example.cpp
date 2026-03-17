/**
 * @file cloud_integration_example.cpp
 * @brief Example usage of RawrXD Cloud Integration System
 * 
 * Demonstrates:
 * - Configuring cloud providers (AWS, Azure, GCP, HuggingFace)
 * - HFHubClient for model discovery and downloads
 * - HybridCloudManager for intelligent routing
 * - Cost tracking and limit enforcement
 * - Failover and retry mechanisms
 */

#include "hybrid_cloud_manager.h"
#include "hf_hub_client.cpp"
#include "cloud_provider_config.h"
#include <iostream>
#include <memory>

void printHeader(const std::string& text) {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "  " << text << std::endl;
    std::cout << std::string(70, '=') << std::endl;
}

void printCostMetrics(const RawrXD::HybridCloudManager& manager) {
    auto metrics = manager.getCostMetrics();
    std::cout << "\n💰 Cost Metrics:" << std::endl;
    std::cout << "  Total Cost: $" << metrics.totalCostUSD << std::endl;
    std::cout << "  Today Cost: $" << metrics.todayCostUSD << std::endl;
    std::cout << "  Month Cost: $" << metrics.monthCostUSD << std::endl;
    std::cout << "  Total Requests: " << metrics.totalRequests << std::endl;
    std::cout << "  Cloud Requests: " << metrics.cloudRequests << std::endl;
    std::cout << "  Local Requests: " << metrics.localRequests << std::endl;
}

void printPerformanceMetrics(const RawrXD::HybridCloudManager& manager) {
    auto metrics = manager.getPerformanceMetrics();
    std::cout << "\n⚡ Performance Metrics:" << std::endl;
    std::cout << "  Average Latency: " << metrics.averageLatency << " ms" << std::endl;
    std::cout << "  Success Rate: " << metrics.successRate << "%" << std::endl;
    std::cout << "  Failover Count: " << metrics.failoverCount << std::endl;
}

//=============================================================================
// Example 1: HuggingFace Model Discovery & Download
//=============================================================================
void example_huggingface_integration() {
    printHeader("HuggingFace Hub Integration");
    
    HFHubClient client;
    
    // 1. Load HuggingFace token from environment
    RawrXD::Cloud::CloudProviderConfigManager::loadFromEnvironment();
    std::string hfToken = RawrXD::Cloud::CloudProviderConfigManager::getHuggingFaceConfig().apiToken;
    
    if (hfToken.empty()) {
        std::cout << "⚠️  HuggingFace token not set. Set HUGGINGFACE_HUB_TOKEN environment variable.\n";
        return;
    }
    
    // 2. Search for models
    std::cout << "\n🔍 Searching for GGUF models matching 'mistral'...\n";
    auto models = client.searchModels("mistral", 5, hfToken);
    
    std::cout << "Found " << models.size() << " models:\n";
    for (size_t i = 0; i < models.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << models[i].repo_id 
                  << " (" << models[i].downloads << " downloads)\n";
    }
    
    if (models.empty()) {
        std::cout << "No models found.\n";
        return;
    }
    
    // 3. Get detailed model info
    const auto& targetModel = models[0];
    std::cout << "\n📋 Fetching details for " << targetModel.repo_id << "...\n";
    auto modelInfo = client.getModelInfo(targetModel.repo_id, hfToken);
    
    std::cout << "  Description: " << modelInfo.description << "\n";
    std::cout << "  Files: " << modelInfo.files.size() << "\n";
    std::cout << "  Total Size: " << client.formatBytes(modelInfo.total_size) << "\n";
    
    // 4. List available quantizations
    std::cout << "\n📊 Available quantizations:\n";
    auto quantizations = client.getAvailableQuantizations(targetModel.repo_id, hfToken);
    for (const auto& q : quantizations) {
        std::cout << "  - " << q << "\n";
    }
    
    // 5. Download a specific model file (with progress)
    if (!modelInfo.files.empty()) {
        std::cout << "\n⬇️  Starting download of " << modelInfo.files[0] << "...\n";
        
        bool success = client.downloadModel(
            targetModel.repo_id,
            modelInfo.files[0],
            "./models",
            [](uint64_t current, uint64_t total) {
                int percent = (current * 100) / total;
                std::cout << "\r  Progress: [" << std::string(percent / 5, '=') 
                         << std::string(20 - percent / 5, ' ') << "] " 
                         << percent << "% (" << (current / 1024 / 1024) << "/" 
                         << (total / 1024 / 1024) << " MB)";
                std::cout.flush();
            },
            hfToken
        );
        
        std::cout << "\n\n" << (success ? "✅ Download successful!" : "❌ Download failed!") << "\n";
    }
}

//=============================================================================
// Example 2: Hybrid Cloud Manager - Basic Execution
//=============================================================================
void example_hybrid_cloud_basic() {
    printHeader("Hybrid Cloud Manager - Basic Execution");
    
    RawrXD::Cloud::CloudProviderConfigManager::loadFromEnvironment();
    
    // Create cloud manager
    auto manager = std::make_unique<RawrXD::HybridCloudManager>();
    
    // Configure providers with API keys from environment
    auto& awsConfig = RawrXD::Cloud::CloudProviderConfigManager::getAWSConfig();
    auto& azureConfig = RawrXD::Cloud::CloudProviderConfigManager::getAzureConfig();
    auto& gcpConfig = RawrXD::Cloud::CloudProviderConfigManager::getGCPConfig();
    
    if (!awsConfig.accessKeyId.empty()) {
        manager->setAWSCredentials(awsConfig.accessKeyId, awsConfig.secretAccessKey, awsConfig.region);
        std::cout << "✅ AWS SageMaker configured\n";
    }
    
    if (!azureConfig.apiKey.empty()) {
        manager->setAzureCredentials(azureConfig.subscriptionId, azureConfig.apiKey);
        std::cout << "✅ Azure OpenAI configured\n";
    }
    
    if (!gcpConfig.projectId.empty()) {
        manager->setGCPCredentials(gcpConfig.projectId, gcpConfig.accessToken);
        std::cout << "✅ GCP Vertex AI configured\n";
    }
    
    // Check provider health
    std::cout << "\n🏥 Checking provider health...\n";
    manager->checkAllProvidersHealth();
    
    auto healthyProviders = manager->getHealthyProviders();
    std::cout << "Healthy providers: " << healthyProviders.size() << "\n";
    for (const auto& provider : healthyProviders) {
        std::cout << "  ✓ " << provider.name << " (avg latency: " << provider.averageLatency << " ms)\n";
    }
    
    // Execute a request
    RawrXD::ExecutionRequest request;
    request.requestId = "demo-001";
    request.prompt = "What is the capital of France?";
    request.taskType = "chat";
    request.maxTokens = 512;
    request.temperature = 0.7;
    
    // Plan execution strategy
    std::cout << "\n🎯 Planning execution strategy...\n";
    auto executionPlan = manager->planExecution(request, "auto");
    std::cout << "  Use Cloud: " << (executionPlan.useCloud ? "Yes" : "No") << "\n";
    std::cout << "  Provider: " << executionPlan.selectedProvider << "\n";
    std::cout << "  Model: " << executionPlan.selectedModel << "\n";
    std::cout << "  Estimated Cost: $" << executionPlan.estimatedCost << "\n";
    
    // Execute request
    std::cout << "\n⚙️  Executing request...\n";
    auto result = manager->execute(request);
    std::cout << "  Status: " << (result.success ? "✅ Success" : "❌ Failed") << "\n";
    std::cout << "  Location: " << result.executionLocation << "\n";
    std::cout << "  Model Used: " << result.modelUsed << "\n";
    std::cout << "  Latency: " << result.latencyMs << " ms\n";
    std::cout << "  Cost: $" << result.cost << "\n";
    
    // Show metrics
    printCostMetrics(*manager);
    printPerformanceMetrics(*manager);
}

//=============================================================================
// Example 3: Cost Management & Limits
//=============================================================================
void example_cost_management() {
    printHeader("Cost Management & Limits");
    
    auto manager = std::make_unique<RawrXD::HybridCloudManager>();
    
    // Set cost limits
    manager->setCostLimit(10.0, 100.0);  // $10/day, $100/month
    manager->setCostThreshold(0.01);      // Warn at $0.01/request
    
    std::cout << "📊 Cost Limits Set:\n";
    std::cout << "  Daily Limit: $10.00\n";
    std::cout << "  Monthly Limit: $100.00\n";
    std::cout << "  Per-Request Threshold: $0.01\n";
    
    // Simulate multiple requests
    std::cout << "\n⏳ Simulating 5 cloud requests...\n";
    for (int i = 0; i < 5; ++i) {
        RawrXD::ExecutionRequest req;
        req.requestId = "cost-test-" + std::to_string(i);
        req.prompt = "Test prompt " + std::to_string(i);
        req.taskType = "chat";
        
        auto result = manager->execute(req);
        
        std::cout << "  Request " << (i + 1) << ": ";
        std::cout << (manager->isWithinCostLimits() ? "✅ Within limits" : "⚠️  OVER LIMIT") << "\n";
    }
    
    // Show final cost metrics
    printCostMetrics(*manager);
}

//=============================================================================
// Example 4: Failover & Retry Logic
//=============================================================================
void example_failover_retry() {
    printHeader("Failover & Retry Mechanisms");
    
    auto manager = std::make_unique<RawrXD::HybridCloudManager>();
    
    // Configure failover
    RawrXD::FailoverConfig failoverConfig;
    failoverConfig.enabled = true;
    failoverConfig.maxRetries = 3;
    failoverConfig.providerPriority = {"aws", "azure", "gcp", "ollama"};
    failoverConfig.fallbackToLocal = true;
    
    manager->setFailoverConfig(failoverConfig);
    
    std::cout << "🔄 Failover Configuration:\n";
    std::cout << "  Enabled: Yes\n";
    std::cout << "  Max Retries: " << failoverConfig.maxRetries << "\n";
    std::cout << "  Provider Priority:\n";
    for (const auto& p : failoverConfig.providerPriority) {
        std::cout << "    → " << p << "\n";
    }
    std::cout << "  Fallback to Local: Yes\n";
    
    // Execute with failover
    RawrXD::ExecutionRequest request;
    request.requestId = "failover-test";
    request.prompt = "Test failover with retries";
    request.taskType = "complex_reasoning";
    
    std::cout << "\n⚙️  Executing with failover...\n";
    auto result = manager->executeWithFailover(request);
    
    std::cout << "  Final Status: " << (result.success ? "✅ Success" : "❌ Failed") << "\n";
    std::cout << "  Execution Location: " << result.executionLocation << "\n";
}

//=============================================================================
// Example 5: Cloud Switching Strategies
//=============================================================================
void example_cloud_switching() {
    printHeader("Cloud Switching Strategies");
    
    auto manager = std::make_unique<RawrXD::HybridCloudManager>();
    
    std::cout << "🔀 Initial Configuration:\n";
    std::cout << "  Prefer Local: " << (true ? "Yes" : "No") << "\n";
    std::cout << "  Using Cloud: " << (manager->isUsingCloud() ? "Yes" : "No") << "\n";
    
    // Switch to cloud
    std::cout << "\n☁️  Switching to cloud execution...\n";
    manager->switchToCloud("Complex reasoning task detected");
    std::cout << "  Using Cloud: " << (manager->isUsingCloud() ? "Yes" : "No") << "\n";
    
    // Switch back to local
    std::cout << "\n💻 Switching to local execution...\n";
    manager->switchToLocal("Cost limit approaching");
    std::cout << "  Using Cloud: " << (manager->isUsingCloud() ? "Yes" : "No") << "\n";
}

//=============================================================================
// Main Entry Point
//=============================================================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "  RawrXD Cloud Integration Examples" << std::endl;
    std::cout << "  CloudManager + HFHubClient + AWS/Azure/GCP" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        // Example 1: HuggingFace
        // Uncomment to run (requires HUGGINGFACE_HUB_TOKEN):
        // example_huggingface_integration();
        
        // Example 2: Basic hybrid execution
        // example_hybrid_cloud_basic();
        
        // Example 3: Cost management
        example_cost_management();
        
        // Example 4: Failover & retry
        example_failover_retry();
        
        // Example 5: Cloud switching
        example_cloud_switching();
        
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "  Examples completed!" << std::endl;
        std::cout << std::string(70, '=') << "\n" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
