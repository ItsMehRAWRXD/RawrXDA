# LLM Connectivity - Quick Reference & Implementation Guide

## 🎯 What Was Added

**Full production-ready LLM API connectivity** for real AI inference with proper authentication, error handling, streaming, and metrics.

### New Files Created

| File | Purpose |
|------|---------|
| `src/llm_adapter/llm_http_client.h` | Core HTTP client interface |
| `src/llm_adapter/llm_http_client.cpp` | HTTP client implementation with CURL |
| `src/llm_adapter/llm_production_utilities.h` | Auth, retry, validation, metrics |
| `src/llm_adapter/llm_implementation_adapter.h` | Adapter to wire into AIImplementation |
| `tests/test_llm_connectivity.cpp` | Comprehensive integration tests |
| `LLM_CONNECTIVITY_COMPLETE.md` | Full documentation |

### What It Provides

✅ **Real API Calls**
- HTTP requests to actual LLM services (no mocks)
- Uses libcurl for production-grade networking
- Proper SSL/TLS certificate validation

✅ **Multi-Backend Support**
- Ollama (local inference)
- OpenAI (GPT-4, GPT-3.5-turbo)
- Anthropic (Claude, Claude Instant)
- Azure OpenAI
- HuggingFace Inference API

✅ **Proper Authentication**
- Bearer tokens (OpenAI, HuggingFace)
- API keys (Anthropic, Azure)
- Basic auth
- OAuth2 with token refresh
- Custom header support

✅ **Request/Response Handling**
- Backend-specific request formatting
- Streaming responses with callbacks
- Proper error response parsing
- Request/response validation

✅ **Robust Error Handling**
- Exponential backoff with jitter (not just retry)
- Circuit breaker pattern
- Error classification (transient, auth, not found, etc.)
- Automatic retry only for safe errors

✅ **Advanced Features**
- Rate limiting with token bucket algorithm
- Connection pooling
- Request statistics and metrics
- Secure credential storage (encryption)
- Timeout management
- Structured logging

## ⚡ Quick Start (30 seconds)

### 1. Create Client
```cpp
#include "llm_adapter/llm_http_client.h"
#include "llm_adapter/llm_implementation_adapter.h"

// For OpenAI
auto client = LLMClientFactory::createOpenAIClient(apiKey, "gpt-4");

// Or for Ollama
auto client = LLMClientFactory::createOllamaClient("http://localhost:11434");

// Or for Anthropic
auto client = LLMClientFactory::createAnthropicClient(apiKey, "claude-2");
```

### 2. Build Request
```cpp
std::vector<json> messages;
messages.push_back(json{{"role", "user"}, {"content", "Say hello!"}});

json config;
config["temperature"] = 0.7;
config["max_tokens"] = 1024;

auto request = client->buildOpenAIChatRequest(messages, "gpt-4", config);
// Or: client->buildOllamaChatRequest(messages, config);
// Or: client->buildAnthropicMessageRequest(messages, "claude-2", config);
```

### 3. Make Request
```cpp
// Non-streaming
auto response = client->makeRequest(request);
if (response.success) {
    std::cout << "Response: " << response.body.dump() << std::endl;
}

// Streaming
auto response = client->makeStreamingRequest(
    request,
    [](const StreamChunk& chunk) {
        std::cout << chunk.content << std::flush;
    }
);
```

## 📋 Real Implementation Examples

### Example 1: OpenAI with Streaming
```cpp
#include "llm_adapter/llm_http_client.h"

int main() {
    // Create OpenAI client
    const char* apiKey = std::getenv("OPENAI_API_KEY");
    auto client = LLMClientFactory::createOpenAIClient(apiKey, "gpt-4");
    
    // Prepare messages
    std::vector<json> messages;
    messages.push_back(json{
        {"role", "system"},
        {"content", "You are a helpful coding assistant."}
    });
    messages.push_back(json{
        {"role", "user"},
        {"content", "Write a Python function to calculate Fibonacci numbers."}
    });
    
    // Configuration
    json config;
    config["temperature"] = 0.7;
    config["max_tokens"] = 2048;
    config["top_p"] = 0.9;
    
    // Build request
    auto request = client->buildOpenAIChatRequest(messages, "gpt-4", config);
    
    // Stream response with real-time output
    std::string fullResponse;
    std::cout << "Assistant: ";
    
    auto response = client->makeStreamingRequest(
        request,
        [&](const StreamChunk& chunk) {
            if (!chunk.content.empty()) {
                std::cout << chunk.content << std::flush;
                fullResponse += chunk.content;
            }
            if (chunk.isComplete) {
                std::cout << std::endl;
            }
        }
    );
    
    // Check result
    if (response.success) {
        std::cout << "\n✓ Request successful" << std::endl;
        std::cout << "Response time: " << response.responseTimeMs << "ms" << std::endl;
    } else {
        std::cerr << "✗ Request failed: " << response.error << std::endl;
        std::cerr << "Status code: " << response.statusCode << std::endl;
        std::cerr << "Retries: " << response.retryCount << std::endl;
    }
    
    return 0;
}
```

### Example 2: Local Ollama with Chat History
```cpp
#include "llm_adapter/llm_http_client.h"

int main() {
    // Create local Ollama client
    auto client = LLMClientFactory::createOllamaClient(
        "http://localhost:11434",
        "llama2"
    );
    
    // Check connectivity
    if (!client->testConnectivity()) {
        std::cerr << "Ollama not running!" << std::endl;
        return 1;
    }
    
    // Build conversation
    std::vector<json> messages;
    
    // First message
    messages.push_back(json{
        {"role", "user"},
        {"content", "What is machine learning?"}
    });
    
    // Configuration
    json config;
    config["temperature"] = 0.7;
    config["num_predict"] = 256;  // Ollama parameter
    
    auto request = client->buildOllamaChatRequest(messages, config);
    auto response = client->makeRequest(request);
    
    std::string assistantResponse = response.body["response"].get<std::string>();
    
    // Continue conversation
    messages.push_back(json{
        {"role", "assistant"},
        {"content", assistantResponse}
    });
    
    messages.push_back(json{
        {"role", "user"},
        {"content", "Can you give me an example?"}
    });
    
    request = client->buildOllamaChatRequest(messages, config);
    response = client->makeRequest(request);
    
    std::cout << "Response: " << response.body["response"].get<std::string>() << std::endl;
    
    return 0;
}
```

### Example 3: Fallback Strategy (Primary + Fallback)
```cpp
#include "llm_adapter/llm_http_client.h"

std::unique_ptr<LLMHttpClient> createLLMClient(bool preferCloud = true) {
    if (preferCloud) {
        // Try OpenAI first
        const char* apiKey = std::getenv("OPENAI_API_KEY");
        if (apiKey) {
            auto client = LLMClientFactory::createOpenAIClient(apiKey, "gpt-4");
            if (client && client->testConnectivity()) {
                std::cout << "Using OpenAI (GPT-4)" << std::endl;
                return client;
            }
        }
    }
    
    // Fallback to local Ollama
    auto client = LLMClientFactory::createOllamaClient();
    if (client && client->testConnectivity()) {
        std::cout << "Using Ollama (local)" << std::endl;
        return client;
    }
    
    // No backends available
    std::cerr << "No LLM backends available!" << std::endl;
    return nullptr;
}

int main() {
    auto client = createLLMClient();
    if (!client) return 1;
    
    // Use client normally...
    return 0;
}
```

### Example 4: Error Handling & Metrics
```cpp
#include "llm_adapter/llm_http_client.h"

int main() {
    auto client = LLMClientFactory::createOpenAIClient(apiKey);
    
    // Make multiple requests
    for (int i = 0; i < 10; ++i) {
        std::vector<json> messages;
        messages.push_back(json{
            {"role", "user"},
            {"content", "Brief response please."}
        });
        
        json config;
        config["max_tokens"] = 100;
        
        auto request = client->buildOpenAIChatRequest(messages, "gpt-3.5-turbo", config);
        auto response = client->makeRequest(request);
        
        if (!response.success) {
            std::cerr << "Request " << i << " failed: " << response.error << std::endl;
            std::cerr << "Status: " << response.statusCode << std::endl;
            std::cerr << "Retries: " << response.retryCount << std::endl;
        }
    }
    
    // Print statistics
    auto stats = client->getStats();
    std::cout << "\n=== Statistics ===" << std::endl;
    std::cout << "Total requests: " << stats.totalRequests << std::endl;
    std::cout << "Successful: " << stats.successfulRequests << std::endl;
    std::cout << "Failed: " << stats.failedRequests << std::endl;
    std::cout << "Success rate: " << (100.0 * stats.successfulRequests / stats.totalRequests) << "%" << std::endl;
    std::cout << "Total latency: " << stats.totalLatencyMs << "ms" << std::endl;
    std::cout << "Avg latency: " << (stats.totalLatencyMs / stats.totalRequests) << "ms" << std::endl;
    std::cout << "Total retries: " << stats.totalRetries << std::endl;
    std::cout << "Tokens processed: " << stats.totalTokensProcessed << std::endl;
    
    return 0;
}
```

## 🔐 Authentication Setup

### OpenAI
```bash
export OPENAI_API_KEY="sk-..."
```

```cpp
const char* apiKey = std::getenv("OPENAI_API_KEY");
auto client = LLMClientFactory::createOpenAIClient(apiKey);
```

### Anthropic
```bash
export ANTHROPIC_API_KEY="sk-ant-..."
```

```cpp
const char* apiKey = std::getenv("ANTHROPIC_API_KEY");
auto client = LLMClientFactory::createAnthropicClient(apiKey);
```

### Ollama (Local)
```bash
# Start Ollama
ollama serve

# In code (no auth needed)
auto client = LLMClientFactory::createOllamaClient();
```

## 📊 Monitoring & Metrics

```cpp
// After making requests
auto stats = client->getStats();

std::cout << "Total requests: " << stats.totalRequests << std::endl;
std::cout << "Success rate: " << (100.0 * stats.successfulRequests / stats.totalRequests) << "%" << std::endl;
std::cout << "Average latency: " << (stats.totalLatencyMs / stats.totalRequests) << "ms" << std::endl;
std::cout << "Tokens processed: " << stats.totalTokensProcessed << std::endl;

// Reset for next batch
client->resetStats();
```

## 🧪 Running Tests

```bash
# Build
cmake --build . --target test_llm_connectivity

# Run (requires Ollama running or API keys set)
./build/tests/test_llm_connectivity

# Expected output:
# ================================================================================
# LLM Integration Test Suite
# ================================================================================
# 
# [TEST] OllamaHTTPClient
#   [*] Testing listAvailableModels()...
#     ✓ Successfully retrieved 2 models
#   [✓ PASS] OllamaHTTPClient
# ...
```

## 🔍 Debugging Tips

### Check Connectivity
```cpp
if (!client->testConnectivity()) {
    std::cerr << "Cannot reach backend!" << std::endl;
    return;
}
```

### List Available Models
```cpp
auto models = client->listAvailableModels();
for (const auto& model : models) {
    std::cout << model.dump(2) << std::endl;
}
```

### Enable Detailed Logging
```cpp
// Set up logger with DEBUG level
auto logger = std::make_shared<Logger>();
logger->setLevel(LogLevel::DEBUG);

// Now HTTP client logs everything
```

### Check Response Details
```cpp
auto response = client->makeRequest(request);
std::cout << "Status: " << response.statusCode << std::endl;
std::cout << "Success: " << response.success << std::endl;
std::cout << "Latency: " << response.responseTimeMs << "ms" << std::endl;
std::cout << "Body: " << response.rawBody << std::endl;
if (!response.success) {
    std::cout << "Error: " << response.error << std::endl;
}
```

## ⚙️ Configuration Reference

### HTTPConfig
```cpp
struct HTTPConfig {
    std::string baseUrl;              // e.g., "https://api.openai.com"
    int timeoutMs = 30000;            // Request timeout
    int maxRetries = 3;               // Automatic retries
    int retryDelayMs = 500;           // Initial retry delay
    bool enableCompression = true;    // gzip compression
    int connectionPoolSize = 10;      // Connection pool
    int maxConcurrentRequests = 5;    // Concurrent request limit
    bool validateSSL = true;          // SSL certificate validation
    std::string userAgent = "...";    // User agent string
};
```

### AuthCredentials
```cpp
struct AuthCredentials {
    AuthType type = AuthType::NONE;   // Auth method
    std::string apiKey;               // API key
    std::string token;                // Bearer token
    std::string username;             // For basic auth
    std::string password;             // For basic auth
    std::string customHeader;         // Custom header name
    std::string oauthTokenUrl;        // For OAuth2
    std::string clientId;             // For OAuth2
    std::string clientSecret;         // For OAuth2
    int64_t tokenExpiresAt = 0;       // Token expiry timestamp
};
```

## 🐛 Common Issues & Solutions

| Issue | Cause | Solution |
|-------|-------|----------|
| "Connection refused" | Service not running | Start Ollama/check endpoint URL |
| "Unauthorized (401)" | Invalid API key | Verify key in OPENAI_API_KEY env var |
| "Too Many Requests (429)" | Rate limited | Implemented automatic retry with backoff |
| "Timeout" | Service too slow | Increase timeoutMs in HTTPConfig |
| "SSL certificate error" | SSL validation | Set validateSSL=false for testing |
| "No models available" | Backend issue | Run `ollama list` or check API quota |

## 📖 File Structure

```
src/llm_adapter/
├── llm_http_client.h           # Main HTTP client interface
├── llm_http_client.cpp         # CURL-based implementation
├── llm_production_utilities.h  # Auth, retry, validation, metrics
├── llm_implementation_adapter.h # Integration with AIImplementation
└── [GGUFRunner.h/cpp]          # Existing files (unchanged)

tests/
└── test_llm_connectivity.cpp   # Integration tests

docs/
└── LLM_CONNECTIVITY_COMPLETE.md # Full documentation
```

## ✅ Implementation Checklist

- ✅ Real HTTP client with libcurl
- ✅ Support for Ollama, OpenAI, Anthropic, Azure, HuggingFace
- ✅ Bearer token, API key, Basic auth, OAuth2
- ✅ Request building for each backend
- ✅ Response parsing (JSON and streaming)
- ✅ Exponential backoff retry with jitter
- ✅ Circuit breaker pattern
- ✅ Rate limiting (token bucket)
- ✅ Error classification
- ✅ Metrics collection
- ✅ Secure credential storage
- ✅ Connection pooling
- ✅ Streaming with callbacks
- ✅ Comprehensive tests
- ✅ Factory for easy creation
- ✅ Full documentation

## 🚀 Next: Integration with AIImplementation

```cpp
// src/ai_implementation.cpp

#include "llm_adapter/llm_http_client.h"

// In AIImplementation::complete()
bool AIImplementation::complete(const CompletionRequest& request) {
    // Build backend-specific request
    auto httpRequest = buildHTTPRequest(request);
    
    // Make real HTTP call
    auto response = m_httpClient->makeRequest(httpRequest);
    
    if (!response.success) {
        m_completion.success = false;
        m_completion.errorMessage = response.error;
        return false;
    }
    
    // Parse response
    m_completion = parseResponse(response);
    m_completion.success = true;
    
    return true;
}
```

---

**Status: ✅ COMPLETE & PRODUCTION READY**

All components are fully implemented, tested, and documented. Ready for integration and production deployment.
