# LLM Connectivity - Full Production Utility Implementation

## 🎯 Overview

This implementation provides **FULL real LLM API connectivity** for RawrXD Agentic IDE with proper authentication, request/response handling, error recovery, and streaming support for:

- **Ollama** (local inference)
- **OpenAI** (GPT-4, GPT-3.5-turbo, etc.)
- **Anthropic** (Claude, Claude Instant)
- **Azure OpenAI** (enterprise)
- **HuggingFace Inference API**

## 📦 New Components

### 1. **LLMHttpClient** (`llm_http_client.h/cpp`)
Core production-ready HTTP client for LLM APIs

**Features:**
- Real HTTP requests using libcurl with proper SSL/TLS
- Multi-backend support (Ollama, OpenAI, Anthropic, etc.)
- Proper authentication (API keys, Bearer tokens, Basic auth, OAuth2)
- Request/response validation and error handling
- Streaming support with callback-based parsing
- Exponential backoff retry logic with jitter
- Connection pooling and timeout management
- Structured logging with severity levels
- Rate limiting with token bucket algorithm
- Request statistics and metrics collection

**Key Classes/Structs:**
- `LLMHttpClient` - Main HTTP client
- `APIRequest` - HTTP request abstraction
- `APIResponse` - HTTP response with metadata
- `StreamChunk` - Individual streaming response chunk
- `HTTPConfig` - Configuration settings
- `AuthCredentials` - Authentication credentials
- `RequestStats` - Usage statistics

**Example Usage:**
```cpp
// Create and initialize client
LLMHttpClient client;
HTTPConfig config;
config.baseUrl = "http://localhost:11434";
config.timeoutMs = 30000;

AuthCredentials creds;
creds.type = AuthType::NONE;

client.initialize(LLMBackend::OLLAMA, config, creds);

// Make streaming request
json genConfig;
genConfig["model"] = "llama2";
genConfig["temperature"] = 0.7;

auto request = client.buildOllamaCompletionRequest("Hello", genConfig);
auto response = client.makeStreamingRequest(
    request,
    [](const StreamChunk& chunk) {
        std::cout << chunk.content << std::flush;
    }
);
```

### 2. **Production Utilities** (`llm_production_utilities.h`)
Enterprise-grade authentication, retry, and validation systems

**Components:**

#### AuthenticationManager
- Secure credential storage with AES-256-GCM encryption
- OAuth2 token refresh with expiry tracking
- API key rotation with audit logging
- Basic auth encoding
- Custom header support for proprietary APIs

#### RetryPolicy
- Exponential backoff with jitter
- Circuit breaker pattern for failure isolation
- Error category classification
- Request deadline enforcement
- Adaptive retry based on error type

#### RequestValidator
- JSON schema validation
- Parameter bounds checking
- Prompt injection prevention
- Token count estimation
- Malformed response detection

#### LLMMetrics
- Request latency tracking (p50, p95, p99)
- Token throughput monitoring
- Error rate analysis by type
- API usage by model
- Cost tracking and estimation

### 3. **Implementation Adapter** (`llm_implementation_adapter.h`)
Bridges `AIImplementation` with real LLM APIs

**Features:**
- Seamless integration with existing AIImplementation
- Backend-agnostic request handling
- Automatic credential refresh
- Health checks and circuit breaking
- Detailed metrics collection

### 4. **Integration Tests** (`tests/test_llm_connectivity.cpp`)
Comprehensive test suite validating:
- Real HTTP requests to each backend
- Authentication and authorization
- Request/response formatting
- Streaming responses
- Error handling and retries
- Rate limiting
- Metrics collection

## 🔧 Supported Backends

### Ollama (Local)
**Endpoint:** `http://localhost:11434`
**Auth:** None required
**APIs:**
- `POST /api/generate` - Text completion
- `POST /api/chat` - Chat completion
- `GET /api/tags` - List models

**Example:**
```cpp
auto client = LLMClientFactory::createOllamaClient(
    "http://localhost:11434",
    "llama2"
);

std::vector<json> messages;
messages.push_back(json{{"role", "user"}, {"content", "Hello"}});

json config;
config["temperature"] = 0.7;

auto request = client->buildOllamaChatRequest(messages, config);
auto response = client->makeStreamingRequest(request, chunkCallback);
```

### OpenAI
**Endpoint:** `https://api.openai.com`
**Auth:** Bearer token (API key)
**APIs:**
- `POST /v1/chat/completions` - Chat completion
- `GET /v1/models` - List models

**Example:**
```cpp
auto client = LLMClientFactory::createOpenAIClient(
    apiKey,
    "gpt-4"
);

std::vector<json> messages;
messages.push_back(json{{"role", "user"}, {"content", "Explain quantum computing"}});

json config;
config["temperature"] = 0.7;
config["max_tokens"] = 2048;

auto request = client->buildOpenAIChatRequest(messages, "gpt-4", config);
auto response = client->makeRequest(request);
```

### Anthropic
**Endpoint:** `https://api.anthropic.com`
**Auth:** API key via `x-api-key` header
**APIs:**
- `POST /messages` - Message creation
- `GET /models` - List models

**Example:**
```cpp
auto client = LLMClientFactory::createAnthropicClient(
    apiKey,
    "claude-2"
);

std::vector<json> messages;
messages.push_back(json{{"role", "user"}, {"content", "Hello"}});

json config;
config["system"] = "You are a helpful assistant";
config["max_tokens"] = 1024;

auto request = client->buildAnthropicMessageRequest(messages, "claude-2", config);
auto response = client->makeStreamingRequest(request, chunkCallback);
```

### Azure OpenAI
**Endpoint:** `https://{resource}.openai.azure.com`
**Auth:** API key via `api-key` header
**APIs:** Same as OpenAI

### HuggingFace Inference API
**Endpoint:** `https://api-inference.huggingface.co`
**Auth:** Bearer token
**Models:** Any HF-hosted model

## 📊 Authentication Handling

### Bearer Token (OpenAI, HuggingFace)
```cpp
AuthCredentials creds;
creds.type = AuthType::BEARER_TOKEN;
creds.token = "sk-...";
```

### API Key (Anthropic, Azure)
```cpp
AuthCredentials creds;
creds.type = AuthType::API_KEY;
creds.apiKey = "sk-ant-...";
```

### Basic Auth (Custom APIs)
```cpp
AuthCredentials creds;
creds.type = AuthType::BASIC_AUTH;
creds.username = "user";
creds.password = "pass";
```

### OAuth2
```cpp
AuthCredentials creds;
creds.type = AuthType::OAUTH2;
creds.token = "access_token";
creds.oauthTokenUrl = "https://auth.example.com/token";
creds.clientId = "client_id";
creds.clientSecret = "client_secret";
creds.tokenExpiresAt = <timestamp_ms>;
```

## 🔄 Streaming Responses

All backends support streaming with callback-based parsing:

```cpp
std::string fullResponse;
int chunkCount = 0;

auto response = client->makeStreamingRequest(
    request,
    [&](const StreamChunk& chunk) {
        if (!chunk.content.empty()) {
            fullResponse += chunk.content;
            std::cout << chunk.content << std::flush;
            chunkCount++;
        }
        if (chunk.isComplete) {
            std::cout << "\n[Generation complete]" << std::endl;
        }
    }
);

std::cout << "Total chunks: " << chunkCount << std::endl;
std::cout << "Response time: " << response.responseTimeMs << "ms" << std::endl;
```

## ⚡ Error Handling & Retry Logic

### Automatic Retries
```cpp
HTTPConfig config;
config.maxRetries = 3;           // Max retry attempts
config.retryDelayMs = 500;       // Initial delay
config.baseUrl = endpoint;

// Automatically retries on:
// - 429 (Too Many Requests)
// - 500+ (Server errors)
// - Timeouts
```

### Error Categories
```
TRANSIENT       - Temporary (429, 503, timeout) - Safe to retry
AUTH_ERROR      - Auth failure (401, 403) - Don't retry
INVALID_REQUEST - Bad request (400) - Don't retry
NOT_FOUND       - 404 - Don't retry unless timing issue
SERVER_ERROR    - 500+ - Retry with caution
NETWORK_ERROR   - Connection issues - Retry aggressively
```

### Circuit Breaker
```
CLOSED    → Operating normally
OPEN      → Too many failures, reject requests
HALF_OPEN → Testing if service recovered
```

## 📈 Metrics & Monitoring

### Usage Statistics
```cpp
auto stats = client->getStats();
std::cout << "Total requests: " << stats.totalRequests << std::endl;
std::cout << "Successful: " << stats.successfulRequests << std::endl;
std::cout << "Failed: " << stats.failedRequests << std::endl;
std::cout << "Total retries: " << stats.totalRetries << std::endl;
std::cout << "Avg latency: " << (stats.totalLatencyMs / stats.totalRequests) << "ms" << std::endl;
std::cout << "Tokens processed: " << stats.totalTokensProcessed << std::endl;
```

### Detailed Metrics
```cpp
struct MetricsSnapshot {
    int totalRequests;
    int successfulRequests;
    int failedRequests;
    int64_t totalLatencyMs;
    int64_t totalTokensProcessed;
    int activeConnections;
    double errorRate;
    
    // Percentiles
    int64_t p50LatencyMs;
    int64_t p95LatencyMs;
    int64_t p99LatencyMs;
    
    // Cost tracking
    double estimatedCost;
};
```

## 🚀 Rate Limiting

Token bucket algorithm prevents overwhelming APIs:

```cpp
client->setRateLimit(100.0);  // 100 requests per second

// Automatically enforced for all requests
bool allowed = client->checkRateLimit();
if (!allowed) {
    std::cout << "Rate limit exceeded, request queued" << std::endl;
}
```

## 🔐 Secure Credential Management

### Encryption
```cpp
AuthenticationManager authMgr;

// Encrypt sensitive data
std::string encrypted = authMgr.encryptSensitive(apiKey);

// Decrypt when needed
std::string decrypted = authMgr.decryptSensitive(encrypted);
```

### Storage
```cpp
// Save credentials securely
AuthCredentials creds;
creds.type = AuthType::API_KEY;
creds.apiKey = apiKey;

authMgr.saveCredentials(LLMBackend::OPENAI, creds, "./creds.enc");

// Load later
authMgr.loadCredentials(LLMBackend::OPENAI, "./creds.enc");
```

## 📝 Integration Steps

### 1. Include Headers
```cpp
#include "llm_adapter/llm_http_client.h"
#include "llm_adapter/llm_implementation_adapter.h"
#include "llm_adapter/llm_production_utilities.h"
```

### 2. Create Client
```cpp
auto client = LLMClientFactory::createOpenAIClient(apiKey, "gpt-4");
```

### 3. Make Requests
```cpp
auto request = client->buildOpenAIChatRequest(messages, "gpt-4", config);
auto response = client->makeStreamingRequest(request, chunkCallback);
```

### 4. Handle Responses
```cpp
if (response.success) {
    std::cout << "Response: " << response.rawBody << std::endl;
} else {
    std::cout << "Error: " << response.error << std::endl;
    std::cout << "Retries attempted: " << response.retryCount << std::endl;
}
```

## 🧪 Testing

Run comprehensive integration tests:

```bash
# Build tests
cmake --build . --target test_llm_connectivity

# Run tests (requires either local Ollama or API keys set)
./tests/test_llm_connectivity

# Expected output:
# [TEST] OllamaHTTPClient
#   [*] Testing listAvailableModels()...
#     ✓ Successfully retrieved 2 models
#   [*] Testing text completion...
#     ✓ Completion request successful
#     Response time: 1234ms
#   [*] Testing streaming response...
#     ✓ Streaming successful (45 chunks)
```

## 📚 Configuration Examples

### Ollama (Local)
```cpp
HTTPConfig config;
config.baseUrl = "http://localhost:11434";
config.timeoutMs = 60000;  // 60s for local inference
config.maxRetries = 2;

AuthCredentials creds;
creds.type = AuthType::NONE;

auto client = LLMClientFactory::createOllamaClient();
```

### OpenAI (Cloud)
```cpp
HTTPConfig config;
config.baseUrl = "https://api.openai.com";
config.timeoutMs = 30000;  // 30s for cloud
config.maxRetries = 3;
config.validateSSL = true;

AuthCredentials creds;
creds.type = AuthType::API_KEY;
creds.apiKey = std::getenv("OPENAI_API_KEY");

auto client = LLMClientFactory::createOpenAIClient(creds.apiKey);
```

### Anthropic (Claude)
```cpp
HTTPConfig config;
config.baseUrl = "https://api.anthropic.com";
config.timeoutMs = 30000;
config.maxRetries = 3;

AuthCredentials creds;
creds.type = AuthType::API_KEY;
creds.apiKey = std::getenv("ANTHROPIC_API_KEY");
creds.customHeader = "x-api-key";

auto client = LLMClientFactory::createAnthropicClient(creds.apiKey);
```

## 🛠️ Building & Compilation

### CMake Integration
```cmake
# In CMakeLists.txt
find_package(CURL REQUIRED)

add_library(llm_adapter
    src/llm_adapter/llm_http_client.cpp
    src/llm_adapter/llm_implementation_adapter.cpp
)

target_link_libraries(llm_adapter PUBLIC CURL::libcurl nlohmann_json::nlohmann_json)

# Tests
add_executable(test_llm_connectivity tests/test_llm_connectivity.cpp)
target_link_libraries(test_llm_connectivity llm_adapter)
```

### Dependencies
- **libcurl** - HTTP requests
- **nlohmann/json** - JSON parsing
- **OpenSSL** - Encryption (optional)
- **C++20** - Modern C++ features

## 📊 Performance

### Expected Latencies
- **Ollama (local)**: 500ms - 5s (model dependent)
- **OpenAI (cloud)**: 100ms - 2s
- **Anthropic (cloud)**: 500ms - 3s

### Throughput
- **Streaming**: Real-time token-by-token
- **Batching**: Support for multiple concurrent requests
- **Rate limiting**: Configurable from 1 to 10,000 req/s

### Resource Usage
- **Memory**: ~10MB base + buffer for responses
- **Connections**: Configurable pool size (default 10)
- **Threads**: Async request handling

## 🐛 Troubleshooting

### Connection Refused
```
Error: "Connection refused"
Solution: Check endpoint URL and ensure service is running
         For Ollama: run `ollama serve`
         For cloud APIs: verify API endpoint and credentials
```

### Authentication Failed
```
Error: "Unauthorized (401)"
Solution: Verify API key/token in AuthCredentials
         Check that auth type matches backend requirements
         Ensure token hasn't expired
```

### Rate Limited
```
Error: "Too Many Requests (429)"
Solution: Implemented automatic retry with exponential backoff
         Consider using setRateLimit() to throttle requests
         Check API quota limits
```

### Timeout
```
Error: "Request timeout"
Solution: Increase timeoutMs in HTTPConfig
         Check network connectivity
         For long completions, use streaming
```

## 📖 Additional Resources

- [Ollama API Docs](https://github.com/ollama/ollama/blob/main/docs/api.md)
- [OpenAI API Docs](https://platform.openai.com/docs/api-reference)
- [Anthropic API Docs](https://docs.anthropic.com/claude/reference/api-reference)
- [libcurl Documentation](https://curl.se/libcurl/)

## ✅ Completed Features

- ✅ Real HTTP client with CURL
- ✅ Multi-backend support (Ollama, OpenAI, Anthropic, Azure, HF)
- ✅ Proper authentication handlers
- ✅ Request/response validation
- ✅ Streaming support with parsing
- ✅ Exponential backoff retry logic
- ✅ Rate limiting with token bucket
- ✅ Error classification and recovery
- ✅ Circuit breaker pattern
- ✅ Metrics collection and reporting
- ✅ Secure credential storage
- ✅ OAuth2 token refresh support
- ✅ Connection pooling
- ✅ Comprehensive integration tests
- ✅ Factory for easy client creation

## 🚀 Next Steps

1. Set environment variables:
   ```bash
   export OPENAI_API_KEY="sk-..."
   export ANTHROPIC_API_KEY="sk-ant-..."
   ```

2. Run tests:
   ```bash
   cmake --build . --target test_llm_connectivity
   ```

3. Integrate with AIImplementation:
   ```cpp
   auto adapter = AIImplementationAdapter();
   adapter.initialize(LLMBackend::OPENAI, config, credentials);
   auto response = adapter.executeCompletion(request);
   ```

4. Monitor metrics:
   ```cpp
   auto metrics = adapter.getMetrics();
   std::cout << metrics.dump(2) << std::endl;
   ```

---

**Implementation Status: ✅ PRODUCTION READY**

All components are fully implemented with:
- Real API connectivity (no mocks)
- Comprehensive error handling
- Detailed logging and metrics
- Complete authentication support
- Streaming responses
- Automatic retries and recovery
- Rate limiting and circuit breaking
- Secure credential management
- Integration tests
