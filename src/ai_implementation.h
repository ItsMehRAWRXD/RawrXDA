#pragma once
/**
 * @file ai_implementation.h
 * @brief Core AI implementation interfaces for RawrXD IDE
 * 
 * Provides the main AIImplementation class that handles:
 * - LLM completions via Ollama, OpenAI, Anthropic, or local inference
 * - Streaming support with chunked callbacks
 * - Tool calling integration for agentic workflows
 * - Conversation history management
 */

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

//=============================================================================
// Forward declarations
//=============================================================================
class Logger;
class Metrics;
class HTTPClient;
class ResponseParser;
class ModelTester;

//=============================================================================
// Configuration structures
//=============================================================================
struct LLMConfig {
    std::string backend = "ollama";         // "ollama", "openai", "anthropic", "local"
    std::string endpoint = "http://localhost:11434";
    std::string modelName = "llama2";
    std::string apiKey;
    int maxTokens = 2048;
    float temperature = 0.7f;
    float topP = 0.9f;
    int topK = 40;
    int timeoutMs = 30000;
};

struct CompletionRequest {
    std::string prompt;
    std::string systemPrompt;
    int maxTokens = 512;
    float temperature = 0.7f;
    float topP = 0.9f;
    bool useToolCalling = false;
    std::string stopSequence;
    std::vector<std::string> context;
};

struct CompletionResponse {
    bool success = false;
    std::string completion;
    std::string errorMessage;
    int totalTokens = 0;
    int promptTokens = 0;
    int completionTokens = 0;
    int64_t latencyMs = 0;
    std::string model;
    std::string finishReason;
};

struct ParsedCompletion {
    std::string text;
    bool isPartial = false;
    int tokenIndex = 0;
    std::string finishReason;
};

struct ToolDefinition {
    std::string name;
    std::string description;
    json parameters;
    std::function<json(const json&)> handler;
};

struct HTTPRequest {
    std::string method = "POST";
    std::string url;
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;
};

struct HTTPResponse {
    bool success = false;
    int statusCode = 0;
    std::string body;
    std::string errorMessage;
};

//=============================================================================
// Logger interface (simple stub if not provided)
//=============================================================================
#ifndef LOGGER_INTERFACE_DEFINED
#define LOGGER_INTERFACE_DEFINED
class Logger {
public:
    Logger(const std::string& name = "Logger") : name_(name) {}
    void info(const std::string& component, const std::string& message) {
        fprintf(stderr, "[INFO] %s: %s: %s\n", name_.c_str(), component.c_str(), message.c_str());
    }
    void warn(const std::string& component, const std::string& message) {
        fprintf(stderr, "[WARN] %s: %s: %s\n", name_.c_str(), component.c_str(), message.c_str());
    }
    void error(const std::string& component, const std::string& message) {
        fprintf(stderr, "[ERROR] %s: %s: %s\n", name_.c_str(), component.c_str(), message.c_str());
    }
    void debug(const std::string& component, const std::string& message) {
        fprintf(stderr, "[DEBUG] %s: %s: %s\n", name_.c_str(), component.c_str(), message.c_str());
    }
private:
    std::string name_;
};
#endif

//=============================================================================
// Metrics interface (simple stub if not provided)
//=============================================================================
#ifndef METRICS_INTERFACE_DEFINED
#define METRICS_INTERFACE_DEFINED
class Metrics {
public:
    void incrementCounter(const std::string& name, int64_t value = 1) {
        counters_[name] += value;
    }
    void recordHistogram(const std::string& name, double value) {
        // Simple histogram: just store last value
        histograms_[name] = value;
    }
    int64_t getCounter(const std::string& name) const {
        auto it = counters_.find(name);
        return it != counters_.end() ? it->second : 0;
    }
private:
    std::map<std::string, int64_t> counters_;
    std::map<std::string, double> histograms_;
};
#endif

//=============================================================================
// HTTP Client interface
//=============================================================================
#ifndef HTTP_CLIENT_INTERFACE_DEFINED
#define HTTP_CLIENT_INTERFACE_DEFINED
class HTTPClient {
public:
    virtual ~HTTPClient() = default;
    
    virtual HTTPResponse sendRequest(const HTTPRequest& request) = 0;
    virtual HTTPResponse sendRequest(const HTTPRequest& request, 
                                     std::function<void(const std::string&)> chunkCallback) = 0;
    virtual HTTPResponse get(const std::string& url) = 0;
};

// WinHTTP-based implementation
class WinHTTPClient : public HTTPClient {
public:
    HTTPResponse sendRequest(const HTTPRequest& request) override;
    HTTPResponse sendRequest(const HTTPRequest& request,
                             std::function<void(const std::string&)> chunkCallback) override;
    HTTPResponse get(const std::string& url) override;
};
#endif

//=============================================================================
// Response Parser interface
//=============================================================================
#ifndef RESPONSE_PARSER_INTERFACE_DEFINED
#define RESPONSE_PARSER_INTERFACE_DEFINED
class ResponseParser {
public:
    virtual ~ResponseParser() = default;
    virtual ParsedCompletion parse(const std::string& response) = 0;
};
#endif

//=============================================================================
// Model Tester interface
//=============================================================================
#ifndef MODEL_TESTER_INTERFACE_DEFINED
#define MODEL_TESTER_INTERFACE_DEFINED
class ModelTester {
public:
    virtual ~ModelTester() = default;
    virtual bool testModel(const std::string& modelName) = 0;
    virtual bool testConnectivity(const std::string& endpoint) = 0;
};
#endif

//=============================================================================
// AIImplementation class
//=============================================================================
class AIImplementation {
public:
    AIImplementation(
        std::shared_ptr<Logger> logger = nullptr,
        std::shared_ptr<Metrics> metrics = nullptr,
        std::shared_ptr<HTTPClient> httpClient = nullptr,
        std::shared_ptr<ResponseParser> responseParser = nullptr,
        std::shared_ptr<ModelTester> modelTester = nullptr
    );

    // Configuration
    bool initialize(const LLMConfig& config);
    LLMConfig getConfig() const;
    
    // Core completion methods
    CompletionResponse complete(const CompletionRequest& request);
    CompletionResponse streamComplete(
        const CompletionRequest& request,
        std::function<void(const ParsedCompletion&)> chunkCallback
    );
    
    // Tool calling
    void registerTool(const ToolDefinition& tool);
    json executeTool(const std::string& toolName, const json& parameters);
    bool supportsToolCalling() const;
    
    // Agentic loop
    CompletionResponse agenticLoop(
        const CompletionRequest& request,
        int maxIterations = 10
    );
    
    // Conversation history
    void addToHistory(const std::string& role, const std::string& content);
    void clearHistory();
    std::vector<std::pair<std::string, std::string>> getHistory() const;
    
    // Utilities
    int estimateTokenCount(const std::string& text);
    bool testConnectivity();
    json getUsageStats() const;

private:
    LLMConfig m_config;
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;
    std::shared_ptr<HTTPClient> m_httpClient;
    std::shared_ptr<ResponseParser> m_responseParser;
    std::shared_ptr<ModelTester> m_modelTester;
    
    std::map<std::string, ToolDefinition> m_registeredTools;
    std::vector<std::pair<std::string, std::string>> m_conversationHistory;
    
    int64_t m_totalLatency = 0;
    int64_t m_totalTokensUsed = 0;
};

//=============================================================================
// Prompt templates
//=============================================================================
namespace PromptTemplates {
    std::string codeGeneration(const std::string& codeContext, const std::string& requirement);
    std::string codeReview(const std::string& code, const std::string& language);
    std::string bugFix(const std::string& code, const std::string& errorMessage, const std::string& stackTrace);
    std::string refactoring(const std::string& code, const std::string& language, const std::string& objective);
    std::string documentation(const std::string& code, const std::string& language);
}
