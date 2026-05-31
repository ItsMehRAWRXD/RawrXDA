/**
 * @file llm_providers.hpp
 * @brief Real LLM API Provider Implementations
 * 
 * Provides production-ready connectors for:
 * - OpenAI API (GPT-4, GPT-4o, GPT-3.5-turbo)
 * - Anthropic API (Claude 3.5 Sonnet, Claude 3 Opus)
 * - Google Gemini API (Gemini Pro, Gemini Ultra)
 * - DeepSeek API (DeepSeek Chat, DeepSeek Coder)
 * - Mistral API (Mistral Large, Mistral Medium)
 * - Local inference (Ollama, llama.cpp)
 * 
 * @author RawrXD Team
 * @version 1.0.0
 */

#pragma once

#include "tls_client.hpp"
#include <functional>
#include <future>
#include <queue>
#include <condition_variable>

namespace RawrXD::LLM {

// ============================================================================
// Common Types
// ============================================================================

struct Message {
    std::string role;      // "system", "user", "assistant"
    std::string content;
    std::string name;      // Optional name for multi-turn
    std::vector<std::string> images; // Base64 encoded images for vision
};

struct ChatCompletionRequest {
    std::string model;
    std::vector<Message> messages;
    float temperature = 0.7f;
    int maxTokens = 2048;
    float topP = 1.0f;
    float frequencyPenalty = 0.0f;
    float presencePenalty = 0.0f;
    std::vector<std::string> stop;
    bool stream = false;
    std::string requestId;
    
    // Provider-specific options
    std::map<std::string, std::string> extraHeaders;
    std::map<std::string, std::string> extraParams;
};

struct ChatCompletionResponse {
    std::string id;
    std::string model;
    std::vector<Message> choices;
    std::map<std::string, int64_t> usage; // prompt_tokens, completion_tokens, total_tokens
    std::string finishReason;
    std::chrono::milliseconds latency{0};
    std::string errorMessage;
    bool success = false;
};

struct EmbeddingRequest {
    std::string model;
    std::string input;
    std::string encodingFormat = "float"; // "float" or "base64"
};

struct EmbeddingResponse {
    std::string model;
    std::vector<float> embedding;
    int64_t tokens = 0;
    std::string errorMessage;
    bool success = false;
};

struct ModelInfo {
    std::string id;
    std::string name;
    std::string provider;
    int64_t contextLength;
    std::vector<std::string> capabilities; // "chat", "completion", "embedding", "vision"
    double inputCostPer1k;  // USD per 1k tokens
    double outputCostPer1k;  // USD per 1k tokens
};

// ============================================================================
// Streaming Callback Types
// ============================================================================

using StreamCallback = std::function<void(const std::string& delta, bool isFinal)>;
using ErrorCallback = std::function<void(const std::string& error)>;
using UsageCallback = std::function<void(int64_t promptTokens, int64_t completionTokens)>;

// ============================================================================
// LLM Provider Interface
// ============================================================================

class ILLMProvider {
public:
    virtual ~ILLMProvider() = default;
    
    // Provider identification
    virtual std::string getName() const = 0;
    virtual std::string getVersion() const = 0;
    virtual std::vector<std::string> getSupportedModels() const = 0;
    virtual std::vector<ModelInfo> getAvailableModels() const = 0;
    
    // Configuration
    virtual void setApiKey(const std::string& apiKey) = 0;
    virtual void setBaseUrl(const std::string& baseUrl) = 0;
    virtual void setTimeout(uint32_t timeoutMs) = 0;
    virtual void setMaxRetries(uint32_t retries) = 0;
    
    // Chat completions
    virtual ChatCompletionResponse chat(const ChatCompletionRequest& request) = 0;
    virtual void chatAsync(const ChatCompletionRequest& request,
                          std::function<void(const ChatCompletionResponse&)> callback) = 0;
    virtual void chatStream(const ChatCompletionRequest& request,
                          StreamCallback onDelta,
                          ErrorCallback onError,
                          UsageCallback onUsage) = 0;
    
    // Embeddings
    virtual EmbeddingResponse embed(const EmbeddingRequest& request) = 0;
    
    // Model management
    virtual bool isModelAvailable(const std::string& modelId) const = 0;
    virtual ModelInfo getModelInfo(const std::string& modelId) const = 0;
    
    // Health check
    virtual bool healthCheck() = 0;
    
    // Cost tracking
    virtual double estimateCost(const ChatCompletionRequest& request) const = 0;
    virtual double getTotalCost() const = 0;
    virtual void resetCostTracking() = 0;
};

// ============================================================================
// OpenAI Provider
// ============================================================================

class OpenAIProvider : public ILLMProvider {
public:
    explicit OpenAIProvider(const TLSConfig& tlsConfig = TLSConfig{});
    ~OpenAIProvider() override;
    
    // ILLMProvider implementation
    std::string getName() const override { return "OpenAI"; }
    std::string getVersion() const override { return "1.0.0"; }
    std::vector<std::string> getSupportedModels() const override;
    std::vector<ModelInfo> getAvailableModels() const override;
    
    void setApiKey(const std::string& apiKey) override;
    void setBaseUrl(const std::string& baseUrl) override;
    void setTimeout(uint32_t timeoutMs) override;
    void setMaxRetries(uint32_t retries) override;
    
    ChatCompletionResponse chat(const ChatCompletionRequest& request) override;
    void chatAsync(const ChatCompletionRequest& request,
                  std::function<void(const ChatCompletionResponse&)> callback) override;
    void chatStream(const ChatCompletionRequest& request,
                   StreamCallback onDelta,
                   ErrorCallback onError,
                   UsageCallback onUsage) override;
    
    EmbeddingResponse embed(const EmbeddingRequest& request) override;
    
    bool isModelAvailable(const std::string& modelId) const override;
    ModelInfo getModelInfo(const std::string& modelId) const override;
    
    bool healthCheck() override;
    
    double estimateCost(const ChatCompletionRequest& request) const override;
    double getTotalCost() const override;
    void resetCostTracking() override;
    
private:
    std::unique_ptr<TLSClient> m_client;
    std::string m_apiKey;
    std::string m_baseUrl = "https://api.openai.com/v1";
    uint32_t m_timeoutMs = 120000;
    uint32_t m_maxRetries = 3;
    mutable std::mutex m_costMutex;
    double m_totalCost = 0.0;
    
    // Model pricing (USD per 1k tokens)
    std::map<std::string, std::pair<double, double>> m_pricing = {
        {"gpt-4o", {0.005, 0.015}},
        {"gpt-4o-mini", {0.00015, 0.0006}},
        {"gpt-4-turbo", {0.01, 0.03}},
        {"gpt-4", {0.03, 0.06}},
        {"gpt-3.5-turbo", {0.0005, 0.0015}},
        {"o1-preview", {0.015, 0.06}},
        {"o1-mini", {0.003, 0.012}},
    };
    
    // JSON serialization
    std::string serializeRequest(const ChatCompletionRequest& request) const;
    std::string serializeEmbeddingRequest(const EmbeddingRequest& request) const;
    
    // JSON parsing
    ChatCompletionResponse parseResponse(const std::string& json) const;
    EmbeddingResponse parseEmbeddingResponse(const std::string& json) const;
    
    // Streaming parser
    void parseStreamChunk(const std::string& chunk,
                         std::string& content,
                         std::string& finishReason,
                         bool& isDone) const;
};

// ============================================================================
// Anthropic Provider
// ============================================================================

class AnthropicProvider : public ILLMProvider {
public:
    explicit AnthropicProvider(const TLSConfig& tlsConfig = TLSConfig{});
    ~AnthropicProvider() override;
    
    // ILLMProvider implementation
    std::string getName() const override { return "Anthropic"; }
    std::string getVersion() const override { return "1.0.0"; }
    std::vector<std::string> getSupportedModels() const override;
    std::vector<ModelInfo> getAvailableModels() const override;
    
    void setApiKey(const std::string& apiKey) override;
    void setBaseUrl(const std::string& baseUrl) override;
    void setTimeout(uint32_t timeoutMs) override;
    void setMaxRetries(uint32_t retries) override;
    
    ChatCompletionResponse chat(const ChatCompletionRequest& request) override;
    void chatAsync(const ChatCompletionRequest& request,
                  std::function<void(const ChatCompletionResponse&)> callback) override;
    void chatStream(const ChatCompletionRequest& request,
                   StreamCallback onDelta,
                   ErrorCallback onError,
                   UsageCallback onUsage) override;
    
    EmbeddingResponse embed(const EmbeddingRequest& request) override;
    
    bool isModelAvailable(const std::string& modelId) const override;
    ModelInfo getModelInfo(const std::string& modelId) const override;
    
    bool healthCheck() override;
    
    double estimateCost(const ChatCompletionRequest& request) const override;
    double getTotalCost() const override;
    void resetCostTracking() override;
    
private:
    std::unique_ptr<TLSClient> m_client;
    std::string m_apiKey;
    std::string m_baseUrl = "https://api.anthropic.com/v1";
    uint32_t m_timeoutMs = 120000;
    uint32_t m_maxRetries = 3;
    mutable std::mutex m_costMutex;
    double m_totalCost = 0.0;
    
    // Model pricing (USD per 1k tokens)
    std::map<std::string, std::pair<double, double>> m_pricing = {
        {"claude-3-5-sonnet-20241022", {0.003, 0.015}},
        {"claude-3-5-haiku-20241022", {0.0008, 0.004}},
        {"claude-3-opus-20240229", {0.015, 0.075}},
        {"claude-3-sonnet-20240229", {0.003, 0.015}},
        {"claude-3-haiku-20240307", {0.00025, 0.00125}},
    };
    
    std::string serializeRequest(const ChatCompletionRequest& request) const;
    ChatCompletionResponse parseResponse(const std::string& json) const;
    void parseStreamChunk(const std::string& chunk,
                         std::string& content,
                         std::string& finishReason,
                         bool& isDone) const;
};

// ============================================================================
// Google Gemini Provider
// ============================================================================

class GeminiProvider : public ILLMProvider {
public:
    explicit GeminiProvider(const TLSConfig& tlsConfig = TLSConfig{});
    ~GeminiProvider() override;
    
    // ILLMProvider implementation
    std::string getName() const override { return "Google Gemini"; }
    std::string getVersion() const override { return "1.0.0"; }
    std::vector<std::string> getSupportedModels() const override;
    std::vector<ModelInfo> getAvailableModels() const override;
    
    void setApiKey(const std::string& apiKey) override;
    void setBaseUrl(const std::string& baseUrl) override;
    void setTimeout(uint32_t timeoutMs) override;
    void setMaxRetries(uint32_t retries) override;
    
    ChatCompletionResponse chat(const ChatCompletionRequest& request) override;
    void chatAsync(const ChatCompletionRequest& request,
                  std::function<void(const ChatCompletionResponse&)> callback) override;
    void chatStream(const ChatCompletionRequest& request,
                   StreamCallback onDelta,
                   ErrorCallback onError,
                   UsageCallback onUsage) override;
    
    EmbeddingResponse embed(const EmbeddingRequest& request) override;
    
    bool isModelAvailable(const std::string& modelId) const override;
    ModelInfo getModelInfo(const std::string& modelId) const override;
    
    bool healthCheck() override;
    
    double estimateCost(const ChatCompletionRequest& request) const override;
    double getTotalCost() const override;
    void resetCostTracking() override;
    
private:
    std::unique_ptr<TLSClient> m_client;
    std::string m_apiKey;
    std::string m_baseUrl = "https://generativelanguage.googleapis.com/v1beta";
    uint32_t m_timeoutMs = 120000;
    uint32_t m_maxRetries = 3;
    mutable std::mutex m_costMutex;
    double m_totalCost = 0.0;
    
    // Model pricing (USD per 1k tokens)
    std::map<std::string, std::pair<double, double>> m_pricing = {
        {"gemini-1.5-pro", {0.00125, 0.005}},
        {"gemini-1.5-flash", {0.000075, 0.0003}},
        {"gemini-1.0-pro", {0.00025, 0.0005}},
        {"gemini-1.0-ultra", {0.0025, 0.0075}},
    };
    
    std::string serializeRequest(const ChatCompletionRequest& request) const;
    ChatCompletionResponse parseResponse(const std::string& json) const;
    void parseStreamChunk(const std::string& chunk,
                         std::string& content,
                         std::string& finishReason,
                         bool& isDone) const;
};

// ============================================================================
// DeepSeek Provider
// ============================================================================

class DeepSeekProvider : public ILLMProvider {
public:
    explicit DeepSeekProvider(const TLSConfig& tlsConfig = TLSConfig{});
    ~DeepSeekProvider() override;
    
    // ILLMProvider implementation
    std::string getName() const override { return "DeepSeek"; }
    std::string getVersion() const override { return "1.0.0"; }
    std::vector<std::string> getSupportedModels() const override;
    std::vector<ModelInfo> getAvailableModels() const override;
    
    void setApiKey(const std::string& apiKey) override;
    void setBaseUrl(const std::string& baseUrl) override;
    void setTimeout(uint32_t timeoutMs) override;
    void setMaxRetries(uint32_t retries) override;
    
    ChatCompletionResponse chat(const ChatCompletionRequest& request) override;
    void chatAsync(const ChatCompletionRequest& request,
                  std::function<void(const ChatCompletionResponse&)> callback) override;
    void chatStream(const ChatCompletionRequest& request,
                   StreamCallback onDelta,
                   ErrorCallback onError,
                   UsageCallback onUsage) override;
    
    EmbeddingResponse embed(const EmbeddingRequest& request) override;
    
    bool isModelAvailable(const std::string& modelId) const override;
    ModelInfo getModelInfo(const std::string& modelId) const override;
    
    bool healthCheck() override;
    
    double estimateCost(const ChatCompletionRequest& request) const override;
    double getTotalCost() const override;
    void resetCostTracking() override;
    
private:
    std::unique_ptr<TLSClient> m_client;
    std::string m_apiKey;
    std::string m_baseUrl = "https://api.deepseek.com/v1";
    uint32_t m_timeoutMs = 120000;
    uint32_t m_maxRetries = 3;
    mutable std::mutex m_costMutex;
    double m_totalCost = 0.0;
    
    // Model pricing (USD per 1k tokens)
    std::map<std::string, std::pair<double, double>> m_pricing = {
        {"deepseek-chat", {0.00014, 0.00028}},
        {"deepseek-coder", {0.00014, 0.00028}},
        {"deepseek-reasoner", {0.00055, 0.00219}},
    };
    
    std::string serializeRequest(const ChatCompletionRequest& request) const;
    ChatCompletionResponse parseResponse(const std::string& json) const;
};

// ============================================================================
// Mistral Provider
// ============================================================================

class MistralProvider : public ILLMProvider {
public:
    explicit MistralProvider(const TLSConfig& tlsConfig = TLSConfig{});
    ~MistralProvider() override;
    
    // ILLMProvider implementation
    std::string getName() const override { return "Mistral"; }
    std::string getVersion() const override { return "1.0.0"; }
    std::vector<std::string> getSupportedModels() const override;
    std::vector<ModelInfo> getAvailableModels() const override;
    
    void setApiKey(const std::string& apiKey) override;
    void setBaseUrl(const std::string& baseUrl) override;
    void setTimeout(uint32_t timeoutMs) override;
    void setMaxRetries(uint32_t retries) override;
    
    ChatCompletionResponse chat(const ChatCompletionRequest& request) override;
    void chatAsync(const ChatCompletionRequest& request,
                  std::function<void(const ChatCompletionResponse&)> callback) override;
    void chatStream(const ChatCompletionRequest& request,
                   StreamCallback onDelta,
                   ErrorCallback onError,
                   UsageCallback onUsage) override;
    
    EmbeddingResponse embed(const EmbeddingRequest& request) override;
    
    bool isModelAvailable(const std::string& modelId) const override;
    ModelInfo getModelInfo(const std::string& modelId) const override;
    
    bool healthCheck() override;
    
    double estimateCost(const ChatCompletionRequest& request) const override;
    double getTotalCost() const override;
    void resetCostTracking() override;
    
private:
    std::unique_ptr<TLSClient> m_client;
    std::string m_apiKey;
    std::string m_baseUrl = "https://api.mistral.ai/v1";
    uint32_t m_timeoutMs = 120000;
    uint32_t m_maxRetries = 3;
    mutable std::mutex m_costMutex;
    double m_totalCost = 0.0;
    
    // Model pricing (USD per 1k tokens)
    std::map<std::string, std::pair<double, double>> m_pricing = {
        {"mistral-large-latest", {0.002, 0.006}},
        {"mistral-medium-latest", {0.0004, 0.0012}},
        {"mistral-small-latest", {0.0001, 0.0003}},
        {"codestral-latest", {0.0003, 0.0009}},
        {"open-mistral-nemo", {0.00003, 0.00009}},
    };
    
    std::string serializeRequest(const ChatCompletionRequest& request) const;
    ChatCompletionResponse parseResponse(const std::string& json) const;
};

// ============================================================================
// Local Provider (Ollama/llama.cpp)
// ============================================================================

class LocalProvider : public ILLMProvider {
public:
    explicit LocalProvider(const TLSConfig& tlsConfig = TLSConfig{});
    ~LocalProvider() override;
    
    // ILLMProvider implementation
    std::string getName() const override { return "Local"; }
    std::string getVersion() const override { return "1.0.0"; }
    std::vector<std::string> getSupportedModels() const override;
    std::vector<ModelInfo> getAvailableModels() const override;
    
    void setApiKey(const std::string& apiKey) override; // Not used for local
    void setBaseUrl(const std::string& baseUrl) override;
    void setTimeout(uint32_t timeoutMs) override;
    void setMaxRetries(uint32_t retries) override;
    
    ChatCompletionResponse chat(const ChatCompletionRequest& request) override;
    void chatAsync(const ChatCompletionRequest& request,
                  std::function<void(const ChatCompletionResponse&)> callback) override;
    void chatStream(const ChatCompletionRequest& request,
                   StreamCallback onDelta,
                   ErrorCallback onError,
                   UsageCallback onUsage) override;
    
    EmbeddingResponse embed(const EmbeddingRequest& request) override;
    
    bool isModelAvailable(const std::string& modelId) const override;
    ModelInfo getModelInfo(const std::string& modelId) const override;
    
    bool healthCheck() override;
    
    double estimateCost(const ChatCompletionRequest& request) const override { return 0.0; } // Free
    double getTotalCost() const override { return 0.0; }
    void resetCostTracking() override {}
    
    // Local-specific methods
    std::vector<std::string> listModels();
    bool loadModel(const std::string& modelId);
    bool unloadModel(const std::string& modelId);
    bool isModelLoaded(const std::string& modelId) const;
    
private:
    std::unique_ptr<TLSClient> m_client;
    std::string m_baseUrl = "http://localhost:11434"; // Ollama default
    uint32_t m_timeoutMs = 300000; // 5 minutes for local
    uint32_t m_maxRetries = 1;
    mutable std::mutex m_modelMutex;
    std::map<std::string, bool> m_loadedModels;
    
    std::string serializeRequest(const ChatCompletionRequest& request) const;
    ChatCompletionResponse parseResponse(const std::string& json) const;
    std::vector<ModelInfo> fetchLocalModels();
};

// ============================================================================
// Provider Factory
// ============================================================================

class LLMProviderFactory {
public:
    enum class ProviderType {
        OpenAI,
        Anthropic,
        Gemini,
        DeepSeek,
        Mistral,
        Local
    };
    
    static std::unique_ptr<ILLMProvider> create(ProviderType type,
                                                  const TLSConfig& tlsConfig = TLSConfig{});
    
    static std::vector<ProviderType> getAvailableProviders();
    static std::string providerToString(ProviderType type);
    static ProviderType stringToProvider(const std::string& name);
};

// ============================================================================
// Multi-Provider Router
// ============================================================================

class LLMRouter {
public:
    explicit LLMRouter(const TLSConfig& tlsConfig = TLSConfig{});
    ~LLMRouter();
    
    // Provider management
    void addProvider(LLMProviderFactory::ProviderType type, const std::string& apiKey = "");
    void removeProvider(LLMProviderFactory::ProviderType type);
    void setProviderPriority(LLMProviderFactory::ProviderType type, int priority);
    
    // Model routing
    void setModelProvider(const std::string& modelId, LLMProviderFactory::ProviderType type);
    LLMProviderFactory::ProviderType getProviderForModel(const std::string& modelId) const;
    
    // Fallback chain
    void setFallbackChain(const std::vector<LLMProviderFactory::ProviderType>& chain);
    
    // Chat with automatic routing
    ChatCompletionResponse chat(const ChatCompletionRequest& request);
    void chatAsync(const ChatCompletionRequest& request,
                   std::function<void(const ChatCompletionResponse&)> callback);
    void chatStream(const ChatCompletionRequest& request,
                   StreamCallback onDelta,
                   ErrorCallback onError,
                   UsageCallback onUsage);
    
    // Cost optimization
    void setCostOptimization(bool enabled);
    double estimateCost(const ChatCompletionRequest& request) const;
    
    // Health and monitoring
    bool healthCheck(LLMProviderFactory::ProviderType type);
    std::map<LLMProviderFactory::ProviderType, bool> healthCheckAll();
    
    // Statistics
    struct ProviderStats {
        uint64_t totalRequests = 0;
        uint64_t successfulRequests = 0;
        uint64_t failedRequests = 0;
        double totalCost = 0.0;
        double avgLatencyMs = 0.0;
    };
    
    ProviderStats getStats(LLMProviderFactory::ProviderType type) const;
    std::map<LLMProviderFactory::ProviderType, ProviderStats> getAllStats() const;
    void resetStats();
    
private:
    TLSConfig m_tlsConfig;
    std::map<LLMProviderFactory::ProviderType, std::unique_ptr<ILLMProvider>> m_providers;
    std::map<std::string, LLMProviderFactory::ProviderType> m_modelRouting;
    std::vector<LLMProviderFactory::ProviderType> m_fallbackChain;
    std::map<LLMProviderFactory::ProviderType, int> m_priorities;
    bool m_costOptimization = false;
    mutable std::mutex m_statsMutex;
    std::map<LLMProviderFactory::ProviderType, ProviderStats> m_stats;
    
    ILLMProvider* getProvider(LLMProviderFactory::ProviderType type);
    const ILLMProvider* getProvider(LLMProviderFactory::ProviderType type) const;
    void recordStats(LLMProviderFactory::ProviderType type, 
                     bool success, 
                     double cost,
                     std::chrono::milliseconds latency);
};

} // namespace RawrXD::LLM
