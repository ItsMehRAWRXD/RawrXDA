# RawrXD Cloud Integration System

## Overview

The RawrXD Cloud Integration System provides intelligent, cost-aware execution routing across multiple cloud providers and local inference engines.

### Key Components

1. **HybridCloudManager** - Main orchestrator for cloud execution decisions
2. **HFHubClient** - HuggingFace Hub API integration for model discovery and downloads
3. **Cloud Providers** - AWS (SageMaker/Bedrock), Azure (OpenAI/Cognitive Services), GCP (Vertex AI)
4. **Cost Management** - Automatic cost tracking and limit enforcement
5. **Health Monitoring** - Provider health checks and failover mechanisms

## Features

### 1. Multi-Cloud Support

**AWS SageMaker / Bedrock**
- Models: Claude 3 (Opus/Sonnet), Llama 2, Mistral
- Pricing: ~$0.001-0.015 per 1000 tokens
- Authentication: AWS credentials (Access Key + Secret Key)
- Endpoint: `https://bedrock-runtime.{region}.amazonaws.com`

**Azure Cognitive Services / OpenAI**
- Models: GPT-4, GPT-4 Turbo, GPT-3.5-turbo
- Pricing: ~$0.001-0.03 per 1000 tokens
- Authentication: Azure API key or subscription ID
- Endpoint: `https://{resource}.openai.azure.com/`

**Google Vertex AI**
- Models: Gemini Pro, Gemini Pro Vision, PaLM 2
- Pricing: ~$0.0001-0.003 per 1000 tokens
- Authentication: GCP OAuth2 token
- Endpoint: `us-central1-aiplatform.googleapis.com`

**HuggingFace Inference**
- Models: Meta Llama, Mistral, StarCoder
- Pricing: Free API (rate-limited) or paid inference
- Authentication: HuggingFace Hub token
- Endpoint: `https://api-inference.huggingface.co`

**Local (Ollama)**
- Models: Mistral, Neural Chat, StarCoder, Orca
- Pricing: Free (local inference)
- Endpoint: `http://localhost:11434`

### 2. Intelligent Routing

The system automatically routes requests based on:
- **Task Type** - Complex reasoning → Cloud, Simple queries → Local
- **Context Length** - Large contexts → Azure/Anthropic (200k tokens), Small → Local
- **Cost** - Respects daily/monthly budgets
- **Latency** - Uses cached provider latencies for decisions
- **Provider Health** - Avoids unhealthy endpoints

### 3. Cost Management

```cpp
// Set limits
manager->setCostLimit(10.0, 100.0);  // $10/day, $100/month
manager->setCostThreshold(0.01);      // Warn if any request > $0.01

// Track costs
auto metrics = manager->getCostMetrics();
std::cout << "Today: $" << metrics.todayCostUSD << std::endl;
std::cout << "Month: $" << metrics.monthCostUSD << std::endl;
std::cout << "Cloud requests: " << metrics.cloudRequests << std::endl;
```

### 4. Failover & Retry

```cpp
// Configure failover
FailoverConfig config;
config.enabled = true;
config.maxRetries = 3;
config.providerPriority = {"aws", "azure", "gcp", "ollama"};
config.fallbackToLocal = true;

manager->setFailoverConfig(config);

// Execute with automatic fallback
auto result = manager->executeWithFailover(request);
```

### 5. Performance Monitoring

```cpp
auto metrics = manager->getPerformanceMetrics();
std::cout << "Average latency: " << metrics.averageLatency << " ms\n";
std::cout << "Success rate: " << metrics.successRate << "%\n";
std::cout << "Failover count: " << metrics.failoverCount << "\n";

// Per-provider latency tracking
double awsLatency = manager->getAverageLatency("aws");
```

## Configuration

### Environment Variables

```bash
# AWS
export AWS_ACCESS_KEY_ID="AKIA..."
export AWS_SECRET_ACCESS_KEY="..."
export AWS_REGION="us-west-2"

# Azure
export AZURE_API_KEY="..."
export AZURE_SUBSCRIPTION_ID="..."

# Google Cloud
export GCP_PROJECT_ID="my-project"
export GCP_CREDENTIALS_JSON="{...}"

# HuggingFace
export HUGGINGFACE_HUB_TOKEN="hf_..."

# Anthropic
export ANTHROPIC_API_KEY="sk-ant-..."
```

### Programmatic Configuration

```cpp
// Load from environment
Cloud::CloudProviderConfigManager::loadFromEnvironment();

// Or configure manually
manager->setAWSCredentials("access_key", "secret_key", "us-west-2");
manager->setAzureCredentials("subscription_id", "api_key");
manager->setGCPCredentials("project_id", "access_token");
manager->setHuggingFaceKey("hf_token");
```

## Usage Examples

### Basic Execution

```cpp
auto manager = std::make_unique<HybridCloudManager>();
manager->setAWSCredentials(accessKey, secretKey, region);

ExecutionRequest request;
request.requestId = "req-001";
request.prompt = "Explain quantum computing";
request.taskType = "complex_reasoning";
request.maxTokens = 1024;

auto result = manager->execute(request);
if (result.success) {
    std::cout << "Response: " << result.response << std::endl;
    std::cout << "Cost: $" << result.cost << std::endl;
    std::cout << "Latency: " << result.latencyMs << " ms" << std::endl;
}
```

### Execution Planning

```cpp
// Get routing decision without executing
auto plan = manager->planExecution(request, "auto");
std::cout << "Use Cloud: " << (plan.useCloud ? "Yes" : "No") << std::endl;
std::cout << "Provider: " << plan.selectedProvider << std::endl;
std::cout << "Estimated Cost: $" << plan.estimatedCost << std::endl;

// Execute if cost is acceptable
if (plan.estimatedCost < 0.10) {
    auto result = manager->execute(request);
}
```

### HuggingFace Model Discovery

```cpp
HFHubClient client;

// Search for models
auto models = client.searchModels("mistral", 10, hfToken);
for (const auto& model : models) {
    std::cout << model.repo_id << " (" << model.downloads << " downloads)" << std::endl;
}

// Get model details
auto info = client.getModelInfo("mistralai/Mistral-7B-Instruct-v0.1");
std::cout << "Files: " << info.files.size() << std::endl;
std::cout << "Size: " << client.formatBytes(info.total_size) << std::endl;

// Download with progress
bool success = client.downloadModel(
    "mistralai/Mistral-7B-Instruct-v0.1",
    "mistral-7b.gguf",
    "./models",
    [](uint64_t current, uint64_t total) {
        std::cout << "\rDownloading: " << (current * 100 / total) << "%";
        std::cout.flush();
    },
    hfToken
);
```

### Request Queuing

```cpp
// Queue multiple requests
ExecutionRequest req1, req2, req3;
manager->queueRequest(req1);
manager->queueRequest(req2);
manager->queueRequest(req3);

// Process all in batch
manager->processPendingRequests();
```

### Cloud Switching

```cpp
// Monitor resource usage and switch dynamically
if (manager->isWithinCostLimits()) {
    manager->switchToCloud("Within budget");
} else {
    manager->switchToLocal("Cost limit approaching");
}

// Check current mode
bool usingCloud = manager->isUsingCloud();
```

## API Reference

### HybridCloudManager

#### Execution Methods
- `ExecutionResult execute(const ExecutionRequest& request)` - Execute with automatic routing
- `ExecutionResult executeLocal(const ExecutionRequest& request)` - Force local execution
- `ExecutionResult executeCloud(const ExecutionRequest& request, const std::string& provider, const std::string& model)`
- `ExecutionResult executeWithFailover(const ExecutionRequest& request)` - Auto-retry with fallback
- `HybridExecution planExecution(const ExecutionRequest& request, const std::string& type = "auto")` - Get routing plan

#### Cloud Provider Methods
- `ExecutionResult executeOnAWS(const ExecutionRequest& request, const std::string& model)`
- `ExecutionResult executeOnAzure(const ExecutionRequest& request, const std::string& model)`
- `ExecutionResult executeOnGCP(const ExecutionRequest& request, const std::string& model)`
- `ExecutionResult executeOnHuggingFace(const ExecutionRequest& request, const std::string& model)`
- `ExecutionResult executeOnOllama(const ExecutionRequest& request)`

#### Configuration Methods
- `void setAWSCredentials(const std::string& accessKey, const std::string& secretKey, const std::string& region)`
- `void setAzureCredentials(const std::string& subscriptionId, const std::string& apiKey)`
- `void setGCPCredentials(const std::string& projectId, const std::string& apiKey)`
- `void setHuggingFaceKey(const std::string& apiKey)`
- `void setLatencyThreshold(int thresholdMs)`
- `void setMaxRetries(int retries)`

#### Cost Management
- `CostMetrics getCostMetrics() const`
- `double getTodayCost() const`
- `double getMonthCost() const`
- `double getTotalCost() const`
- `void setCostLimit(double dailyUSD, double monthlyUSD)`
- `void setCostThreshold(double thresholdUSD)`
- `bool isWithinCostLimits() const`

#### Health & Monitoring
- `void checkProviderHealth(const std::string& providerId)`
- `void checkAllProvidersHealth()`
- `bool isProviderHealthy(const std::string& providerId) const`
- `std::vector<CloudProvider> getHealthyProviders() const`
- `PerformanceMetrics getPerformanceMetrics() const`
- `double getAverageLatency(const std::string& provider = "") const`

### HFHubClient

- `std::vector<ModelMetadata> searchModels(const std::string& query, int limit = 10, const std::string& token = "")`
- `ModelMetadata getModelInfo(const std::string& repo_id, const std::string& token = "")`
- `bool downloadModel(const std::string& repo_id, const std::string& filename, const std::string& outputDir, std::function<void(uint64_t, uint64_t)> progressCallback, const std::string& token)`
- `bool downloadFullModel(const std::string& repo_id, const std::string& outputDir, ...)`
- `std::vector<std::string> getAvailableQuantizations(const std::string& repo_id, const std::string& token = "")`
- `std::vector<std::pair<std::string, uint64_t>> getModelFileList(const std::string& repo_id, const std::string& token = "")`
- `bool validateToken(const std::string& token)`

## Error Handling

All cloud execution methods return an `ExecutionResult` with:
- `bool success` - Whether execution succeeded
- `std::string errorMessage` - Error details if failed
- `std::string response` - The actual response from the model/API
- `double latencyMs` - How long the request took
- `double cost` - Cost of the request in USD

```cpp
auto result = manager->execute(request);

if (!result.success) {
    std::cerr << "Error: " << result.errorMessage << std::endl;
    
    // Could retry or switch providers
    if (result.executionLocation == "aws") {
        // Try Azure instead
        result = manager->executeOnAzure(request, "gpt-4");
    }
}
```

## Performance Optimization

1. **Enable Health Checks** - Regular health checks find issues early
   ```cpp
   manager->setHealthCheckInterval(30000);  // 30 seconds
   manager->checkAllProvidersHealth();
   ```

2. **Cache Provider Metrics** - Available via `getPerformanceMetrics()`
   ```cpp
   auto metrics = manager->getPerformanceMetrics();
   // Use metrics.latencyByProvider to pick fastest provider
   ```

3. **Batch Requests** - Use request queuing for better throughput
   ```cpp
   manager->queueRequest(req1);
   manager->queueRequest(req2);
   manager->processPendingRequests();  // Process all at once
   ```

4. **Monitor Costs** - Keep tracking of spend to stay within budget
   ```cpp
   if (!manager->isWithinCostLimits()) {
       manager->switchToLocal("Budget exceeded");
   }
   ```

## Troubleshooting

### Provider Connection Issues
```cpp
// Check if provider is reachable
manager->checkProviderHealth("aws");
auto provider = manager->getProvider("aws");
if (!provider.isEnabled) {
    std::cerr << "AWS provider is down" << std::endl;
}
```

### Authentication Failures
- Verify API keys are set: `manager->setAWSCredentials(...)`
- Check environment variables are loaded: `CloudProviderConfigManager::loadFromEnvironment()`
- Validate credentials are correct format for each provider

### High Latency
```cpp
// Check provider latencies
auto metrics = manager->getPerformanceMetrics();
for (const auto& [provider, latency] : metrics.latencyByProvider) {
    std::cout << provider << ": " << latency << " ms" << std::endl;
}
```

### Cost Overages
```cpp
// Set strict limits and monitor
manager->setCostLimit(5.0, 50.0);  // $5/day, $50/month

if (manager->getTodayCost() > 4.5) {
    manager->switchToLocal("Approaching daily limit");
}
```

## Advanced Configuration

### Custom Provider Configuration
```cpp
CloudProvider custom;
custom.providerId = "custom-api";
custom.name = "My Custom API";
custom.endpoint = "https://api.example.com";
custom.apiKey = "secret-key";
custom.costPerRequest = 0.001;

manager->addProvider(custom);
manager->configureProvider("custom-api", "secret-key", "https://api.example.com", "us-east-1");
```

### Execution History & Analytics
```cpp
// Get last 100 executions
auto history = manager->getExecutionHistory(100);

for (const auto& result : history) {
    std::cout << "Request " << result.requestId << ": "
              << result.executionLocation << " - $" << result.cost << std::endl;
}
```

## Building & Compiling

### Requirements
- Windows (uses WinHTTP API)
- C++17 or later
- nlohmann/json (header-only)

### Compilation
```bash
cl.exe /std:c++17 /W4 \
  hybrid_cloud_manager.cpp \
  hf_hub_client.cpp \
  cloud_integration_example.cpp \
  /link winhttp.lib
```

## See Also
- [AWS Bedrock Documentation](https://docs.aws.amazon.com/bedrock/)
- [Azure OpenAI Documentation](https://learn.microsoft.com/azure/cognitive-services/openai/)
- [Google Vertex AI Documentation](https://cloud.google.com/vertex-ai/docs)
- [HuggingFace Hub API](https://huggingface.co/docs/hub/api)
- [Ollama Documentation](https://ollama.ai/)

