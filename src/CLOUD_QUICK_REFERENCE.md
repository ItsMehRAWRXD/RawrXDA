# RawrXD Cloud Integration - Quick Reference

## Installation & Setup

### 1. Set Environment Variables

```bash
# AWS
set AWS_ACCESS_KEY_ID=AKIA...
set AWS_SECRET_ACCESS_KEY=...
set AWS_REGION=us-west-2

# Azure
set AZURE_API_KEY=...
set AZURE_SUBSCRIPTION_ID=...

# Google Cloud
set GCP_PROJECT_ID=my-project
set GCP_CREDENTIALS_JSON={...}

# HuggingFace
set HUGGINGFACE_HUB_TOKEN=hf_...

# Anthropic
set ANTHROPIC_API_KEY=sk-ant-...
```

### 2. Include Headers

```cpp
#include "cloud_integration.h"  // All-in-one header
// OR
#include "hybrid_cloud_manager.h"
#include "hf_hub_client.cpp"
#include "cloud_provider_config.h"
```

### 3. Initialize Service

```cpp
using namespace RawrXD::Cloud;

auto service = std::make_unique<CloudIntegrationService>();
service->configureFromEnvironment();  // Load credentials from env vars
```

## Common Tasks

### Execute a Prompt (Auto Routing)

```cpp
// Simple execution
auto response = service->execute("What is Python?", "chat", 512);

// With cost limit
auto response = service->executeWithCostLimit(
    "Explain quantum computing", 
    0.10  // Max $0.10
);
```

### Search for Models

```cpp
auto models = service->searchModels("mistral", 10);
for (const auto& model : models) {
    std::cout << model.repo_id << " - " << model.downloads << " downloads\n";
}
```

### Download a Model

```cpp
service->downloadModel(
    "mistralai/Mistral-7B-Instruct-v0.1",
    "mistral-7b.gguf",
    "./models",
    [](uint64_t current, uint64_t total) {
        std::cout << "\r" << (current * 100 / total) << "%";
        std::cout.flush();
    }
);
```

### Get Cost Metrics

```cpp
auto metrics = service->getCostMetrics();
std::cout << "Today: $" << metrics.todayCostUSD << "\n";
std::cout << "Month: $" << metrics.monthCostUSD << "\n";
std::cout << "Cloud requests: " << metrics.cloudRequests << "\n";
```

### Check Provider Health

```cpp
auto healthy = service->checkHealth();
for (const auto& provider : healthy) {
    std::cout << provider.name << " - OK (" 
              << provider.averageLatency << " ms)\n";
}
```

### Set Budget Limits

```cpp
service->setDailyBudget(10.0);      // $10/day
service->setMonthlyBudget(100.0);   // $100/month
service->setPreferLocal(true);      // Default to local for cost saving
```

## Advanced Configuration

### Custom Execution Planning

```cpp
auto manager = service->getCloudManager();

ExecutionRequest request;
request.prompt = "Your prompt here";
request.taskType = "complex_reasoning";

// Get routing decision without executing
auto plan = manager->planExecution(request, "auto");
std::cout << "Would route to: " << plan.selectedProvider << "\n";
std::cout << "Estimated cost: $" << plan.estimatedCost << "\n";

// Execute if acceptable
if (plan.estimatedCost < 0.05) {
    auto result = manager->execute(request);
}
```

### Specific Cloud Provider Execution

```cpp
auto manager = service->getCloudManager();

// Execute on specific provider
auto result = manager->executeOnAWS(request, "anthropic.claude-3-opus");
// OR
result = manager->executeOnAzure(request, "gpt-4");
// OR
result = manager->executeOnGCP(request, "gemini-pro");
```

### Failover Configuration

```cpp
RawrXD::FailoverConfig failover;
failover.enabled = true;
failover.maxRetries = 3;
failover.providerPriority = {"aws", "azure", "gcp", "ollama"};
failover.fallbackToLocal = true;

auto manager = service->getCloudManager();
manager->setFailoverConfig(failover);

// Will automatically retry with fallback on failure
auto result = manager->executeWithFailover(request);
```

## Provider Matrix

| Provider | Models | Pricing | Endpoint | Auth |
|----------|--------|---------|----------|------|
| **AWS** | Claude, Llama, Mistral | $0.001-0.015/1k | bedrock-runtime.region.amazonaws.com | Access Key + Secret |
| **Azure** | GPT-4, GPT-3.5 | $0.001-0.03/1k | resource.openai.azure.com | API Key |
| **GCP** | Gemini, PaLM | $0.0001-0.003/1k | aiplatform.googleapis.com | OAuth2 Token |
| **HuggingFace** | Llama, Mistral, CodeCovery | Free-Paid | api-inference.huggingface.co | Hub Token |
| **Ollama** | Mistral, Neural Chat, etc | Free (Local) | localhost:11434 | None |

## Error Handling

```cpp
auto result = manager->execute(request);

if (!result.success) {
    std::cerr << "Failed: " << result.errorMessage << "\n";
    
    // Retry with different provider
    if (result.executionLocation == "aws") {
        result = manager->executeOnAzure(request, "gpt-4");
    }
}
```

## Debugging

### Enable Provider Health Checks

```cpp
auto manager = service->getCloudManager();
manager->setHealthCheckInterval(30000);  // 30 seconds
manager->checkAllProvidersHealth();

auto providers = manager->getProviders();
for (const auto& p : providers) {
    std::cout << p.name << ": " << (p.isEnabled ? "OK" : "DOWN") << "\n";
}
```

### Monitor Latency

```cpp
auto metrics = service->getPerformanceMetrics();
std::cout << "AWS latency: " 
          << metrics.latencyByProvider["aws"] << " ms\n";
```

### View Execution History

```cpp
auto manager = service->getCloudManager();
auto history = manager->getExecutionHistory(10);  // Last 10 executions

for (const auto& result : history) {
    std::cout << "Request " << result.requestId << "\n";
    std::cout << "  Location: " << result.executionLocation << "\n";
    std::cout << "  Cost: $" << result.cost << "\n";
    std::cout << "  Latency: " << result.latencyMs << " ms\n";
}
```

## Code Snippets

### Simple CLI Integration

```cpp
int main() {
    auto service = std::make_unique<CloudIntegrationService>();
    service->configureFromEnvironment();
    service->setDailyBudget(10.0);
    
    std::string prompt;
    while (std::getline(std::cin, prompt)) {
        auto response = service->execute(prompt, "chat", 1024);
        std::cout << response << "\n\n";
    }
    
    service->printSummary();
    return 0;
}
```

### Batch Processing with Cost Control

```cpp
auto service = std::make_unique<CloudIntegrationService>();
service->setDailyBudget(50.0);

std::vector<std::string> prompts = {...};
std::vector<std::string> results;

for (const auto& prompt : prompts) {
    auto response = service->executeWithCostLimit(prompt, 0.10);
    results.push_back(response);
    
    auto metrics = service->getCostMetrics();
    if (metrics.todayCostUSD > 45.0) {
        std::cout << "Approaching daily limit. Switching to local.\n";
        service->setPreferLocal(true);
    }
}
```

### Model Selection & Download

```cpp
auto service = std::make_unique<CloudIntegrationService>();

std::cout << "Searching for code models...\n";
auto models = service->searchModels("coder", 5);

if (!models.empty()) {
    const auto& best = models[0];
    std::cout << "Downloading " << best.repo_id << "\n";
    
    service->downloadModel(
        best.repo_id,
        "model.gguf",
        "./local_models",
        [](uint64_t cur, uint64_t total) {
            std::cout << "\r  " << (cur * 100 / total) << "%";
            std::cout.flush();
        }
    );
    std::cout << "\nDone!\n";
}
```

## Pricing Quick Reference

| Provider | Model | Input/1k tokens | Output/1k tokens |
|----------|-------|-----------------|------------------|
| AWS | Claude 3 Opus | $0.015 | $0.075 |
| AWS | Claude 3 Sonnet | $0.003 | $0.015 |
| AWS | Llama 2 70B | $0.001 | - |
| Azure | GPT-4 | $0.03 | $0.06 |
| Azure | GPT-3.5 | $0.0005 | $0.0015 |
| GCP | Gemini Pro | $0.0005 | $0.0015 |
| GCP | PaLM 2 | $0.0001 | $0.0003 |
| HuggingFace | Llama 70B | Free (rate-limited) | - |
| Ollama | Any | Free (local) | - |

## Performance Tips

1. **Cache Models Locally** - Use Ollama or local GGUF for frequently used models
2. **Monitor Latency** - Use metrics to pick fastest provider
3. **Batch Requests** - Queue multiple requests for better throughput
4. **Set Realistic Budgets** - Help identify cost-effective provider mix
5. **Check Health Regularly** - Catch outages early

## Links & Resources

- [Cloud Integration Guide](./CLOUD_INTEGRATION_GUIDE.md)
- [AWS Bedrock](https://aws.amazon.com/bedrock/)
- [Azure OpenAI](https://azure.microsoft.com/en-us/products/ai-services/openai-service/)
- [Google Vertex AI](https://cloud.google.com/vertex-ai)
- [HuggingFace Hub](https://huggingface.co/hub)
- [Ollama](https://ollama.ai/)
