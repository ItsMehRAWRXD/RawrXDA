// ============================================================================
// AIImplementation Adapter - Real LLM Connectivity Integration
// ============================================================================
// This file demonstrates how to wire the LLMHttpClient into AIImplementation
// for real API calls to Ollama, OpenAI, Anthropic, etc.

#pragma once

#include "ai_implementation.h"
#include "llm_http_client.h"
#include <memory>
#include <chrono>
#include <queue>
#include <atomic>

/**
 * AIImplementationAdapter: Bridges AIImplementation with real LLM APIs
 * 
 * This adapter:
 * - Converts AIImplementation requests to backend-specific API calls
 * - Handles streaming responses with proper parsing
 * - Manages authentication and token refresh
 * - Implements retry logic and circuit breaking
 * - Collects detailed metrics
 * - Provides fallback strategies
 */
class AIImplementationAdapter {
public:
    AIImplementationAdapter();
    ~AIImplementationAdapter();

    /**
     * Initialize adapter with LLM backend configuration
     * @param backend Target LLM backend
     * @param config HTTP configuration
     * @param credentials Authentication credentials
     * @return True if initialization successful
     */
    bool initialize(
        LLMBackend backend,
        const HTTPConfig& config,
        const AuthCredentials& credentials
    );

    /**
     * Execute completion request against real LLM API
     * @param request Completion request
     * @return Completion response
     */
    CompletionResponse executeCompletion(const CompletionRequest& request);

    /**
     * Execute streaming completion
     * @param request Completion request
     * @param chunkCallback Called for each streaming chunk
     * @return Completion response
     */
    CompletionResponse executeStreamingCompletion(
        const CompletionRequest& request,
        std::function<void(const ParsedCompletion&)> chunkCallback
    );

    /**
     * Test connectivity to LLM backend
     * @return True if backend is reachable
     */
    bool testConnectivity();

    /**
     * Get current backend metrics
     * @return Metrics snapshot
     */
    json getMetrics();

    /**
     * Update credentials (for OAuth2 refresh, etc.)
     * @param credentials New credentials
     */
    void updateCredentials(const AuthCredentials& credentials);

    /**
     * Get current model information
     * @return JSON with available models
     */
    json listModels();

private:
    std::shared_ptr<LLMHttpClient> m_httpClient;
    std::shared_ptr<Logger> m_logger;
    
    // State
    LLMBackend m_currentBackend;
    std::string m_currentModel;
    int64_t m_lastHealthCheck = 0;
    const int64_t HEALTH_CHECK_INTERVAL = 60000;  // 60 seconds

    // Helper methods
    APIRequest buildRequestForBackend(const CompletionRequest& request);
    ParsedCompletion parseStreamChunk(const StreamChunk& chunk);
    bool validateResponse(const APIResponse& response);
    void logRequest(const APIRequest& request);
    void logResponse(const APIResponse& response, int64_t latency);
};

// ============================================================================
// Factory for Creating Properly Configured Clients
// ============================================================================

/**
 * LLMClientFactory: Easy factory for creating configured LLM clients
 * 
 * Usage:
 *   auto client = LLMClientFactory::createOllamaClient("http://localhost:11434");
 *   auto client = LLMClientFactory::createOpenAIClient(apiKey);
 *   auto client = LLMClientFactory::createAnthropicClient(apiKey);
 */
class LLMClientFactory {
public:
    /**
     * Create Ollama client (local inference)
     * @param endpoint Ollama endpoint (e.g., "http://localhost:11434")
     * @param model Default model to use
     * @return Configured HTTP client
     */
    static std::unique_ptr<LLMHttpClient> createOllamaClient(
        const std::string& endpoint = "http://localhost:11434",
        const std::string& model = "llama2"
    );

    /**
     * Create OpenAI client
     * @param apiKey OpenAI API key
     * @param model Default model (e.g., "gpt-4", "gpt-3.5-turbo")
     * @return Configured HTTP client
     */
    static std::unique_ptr<LLMHttpClient> createOpenAIClient(
        const std::string& apiKey,
        const std::string& model = "gpt-4"
    );

    /**
     * Create Anthropic client
     * @param apiKey Anthropic API key
     * @param model Default model (e.g., "claude-2", "claude-instant")
     * @return Configured HTTP client
     */
    static std::unique_ptr<LLMHttpClient> createAnthropicClient(
        const std::string& apiKey,
        const std::string& model = "claude-2"
    );

    /**
     * Create Azure OpenAI client
     * @param endpoint Azure endpoint
     * @param apiKey Azure API key
     * @param model Default model
     * @return Configured HTTP client
     */
    static std::unique_ptr<LLMHttpClient> createAzureOpenAIClient(
        const std::string& endpoint,
        const std::string& apiKey,
        const std::string& model = "gpt-4"
    );

    /**
     * Create HuggingFace Inference API client
     * @param apiKey HuggingFace API token
     * @param model Model ID (e.g., "meta-llama/Llama-2-7b-chat")
     * @return Configured HTTP client
     */
    static std::unique_ptr<LLMHttpClient> createHuggingFaceClient(
        const std::string& apiKey,
        const std::string& model
    );

private:
    // Helper methods
    static HTTPConfig createDefaultHTTPConfig();
    static HTTPConfig createSecureHTTPConfig();
};

// ============================================================================
// Real-World Usage Examples
// ============================================================================

/*
USAGE EXAMPLE 1: Ollama Local Inference
=========================================

    // Create client for local Ollama instance
    auto client = LLMClientFactory::createOllamaClient();
    
    // Build request
    std::vector<json> messages;
    messages.push_back(json{{"role", "user"}, {"content", "Explain quantum computing"}});
    
    json config;
    config["temperature"] = 0.7;
    config["max_tokens"] = 2048;
    
    auto request = client->buildOllamaChatRequest(messages, config);
    
    // Stream response
    std::string fullResponse;
    auto response = client->makeStreamingRequest(
        request,
        [&](const StreamChunk& chunk) {
            fullResponse += chunk.content;
            std::cout << chunk.content << std::flush;
        }
    );
    
    if (response.success) {
        std::cout << "\nResponse completed" << std::endl;
    }


USAGE EXAMPLE 2: OpenAI GPT-4
=============================

    // Create client with API key
    const char* apiKey = std::getenv("OPENAI_API_KEY");
    auto client = LLMClientFactory::createOpenAIClient(apiKey, "gpt-4");
    
    // Prepare messages
    std::vector<json> messages;
    messages.push_back(json{{"role", "system"}, {"content", "You are a helpful coding assistant"}});
    messages.push_back(json{{"role", "user"}, {"content", "Write a Python function to sort a list"}});
    
    // Create request
    json config;
    config["temperature"] = 0.7;
    config["max_tokens"] = 1024;
    
    auto request = client->buildOpenAIChatRequest(messages, "gpt-4", config);
    
    // Execute
    auto response = client->makeRequest(request);
    if (response.success) {
        std::cout << "Response: " << response.body.dump(2) << std::endl;
    }


USAGE EXAMPLE 3: Anthropic Claude
==================================

    // Create Anthropic client
    const char* apiKey = std::getenv("ANTHROPIC_API_KEY");
    auto client = LLMClientFactory::createAnthropicClient(apiKey, "claude-2");
    
    // Messages
    std::vector<json> messages;
    messages.push_back(json{{"role", "user"}, {"content", "Say something interesting"}});
    
    // Config
    json config;
    config["system"] = "You are a creative writer";
    config["max_tokens"] = 1024;
    
    auto request = client->buildAnthropicMessageRequest(messages, "claude-2", config);
    
    // Stream with callback
    auto response = client->makeStreamingRequest(
        request,
        [](const StreamChunk& chunk) {
            if (!chunk.content.empty()) {
                std::cout << chunk.content;
            }
            if (chunk.isComplete) {
                std::cout << "\n[DONE]" << std::endl;
            }
        }
    );


USAGE EXAMPLE 4: Integration with AIImplementation
===================================================

    // Setup
    auto logger = std::make_shared<Logger>();
    auto metrics = std::make_shared<Metrics>();
    auto client = LLMClientFactory::createOpenAIClient(apiKey);
    auto responseParser = std::make_shared<ResponseParser>();
    auto modelTester = std::make_shared<ModelTester>();
    
    // Create AIImplementation
    AIImplementation ai(logger, metrics, client, responseParser, modelTester);
    
    // Configure
    LLMConfig llmConfig;
    llmConfig.backend = "openai";
    llmConfig.endpoint = "https://api.openai.com";
    llmConfig.apiKey = apiKey;
    llmConfig.modelName = "gpt-4";
    llmConfig.maxTokens = 2048;
    
    if (!ai.initialize(llmConfig)) {
        std::cerr << "Failed to initialize AI" << std::endl;
        return;
    }
    
    // Use for completions
    CompletionRequest request;
    request.prompt = "Explain machine learning";
    
    auto response = ai.complete(request);
    if (response.success) {
        std::cout << "Response: " << response.completion << std::endl;
    }


USAGE EXAMPLE 5: With Error Handling and Retries
==================================================

    auto client = LLMClientFactory::createOpenAIClient(apiKey);
    
    // Set custom retry policy
    HTTPConfig config = client->getConfig();
    config.maxRetries = 5;
    config.retryDelayMs = 1000;
    
    // Make request with automatic retries
    auto response = client->makeRequest(request);
    
    if (!response.success) {
        std::cerr << "Request failed after " << response.retryCount << " retries" << std::endl;
        std::cerr << "Error: " << response.error << std::endl;
        std::cerr << "Status: " << response.statusCode << std::endl;
    } else {
        std::cout << "Success! Response time: " << response.responseTimeMs << "ms" << std::endl;
    }


USAGE EXAMPLE 6: Token Management and Pricing
==============================================

    auto client = LLMClientFactory::createOpenAIClient(apiKey);
    
    // Track usage
    auto stats = client->getStats();
    std::cout << "Total requests: " << stats.totalRequests << std::endl;
    std::cout << "Successful: " << stats.successfulRequests << std::endl;
    std::cout << "Failed: " << stats.failedRequests << std::endl;
    std::cout << "Tokens processed: " << stats.totalTokensProcessed << std::endl;
    std::cout << "Average latency: " 
              << (stats.totalLatencyMs / stats.totalRequests) << "ms" << std::endl;


USAGE EXAMPLE 7: Fallback Strategy
===================================

    // Try primary backend (OpenAI)
    auto primaryClient = LLMClientFactory::createOpenAIClient(openaiKey);
    if (!primaryClient->testConnectivity()) {
        std::cout << "OpenAI unavailable, using fallback (Ollama)" << std::endl;
        
        // Fallback to local Ollama
        auto fallbackClient = LLMClientFactory::createOllamaClient();
        if (!fallbackClient->testConnectivity()) {
            std::cerr << "All backends unavailable!" << std::endl;
            return;
        }
        // Use fallbackClient instead
    }
*/

#endif // LLM_IMPLEMENTATION_ADAPTER_H
