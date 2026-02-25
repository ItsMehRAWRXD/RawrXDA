#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>

#include "logging/logger.h"
#include "metrics/metrics.h"
#include "library_integration.h"
#include "response_parser.h"
#include "model_tester.h"

using json = nlohmann::json;

/**
 * AIImplementation: Core real AI execution layer
 * 
 * This module provides:
 * - Multi-backend LLM support (Ollama, OpenAI, Anthropic, HuggingFace)
 * - Streaming completions with real-time parsing
 * - Function calling / tool use
 * - Prompt engineering utilities
 * - Token counting and optimization
 * - Error recovery and fallback strategies
 */

struct LLMConfig {
    std::string backend;           // "ollama", "openai", "anthropic", "huggingface"
    std::string endpoint;          // API endpoint URL
    std::string apiKey;            // API key (if needed)
    std::string modelName;         // Model identifier
    int maxTokens = 2048;
    double temperature = 0.7;
    double topP = 0.9;
    int topK = 40;
    bool stream = true;
    std::vector<std::string> stopSequences;
};

struct CompletionRequest {
    std::string prompt;
    std::vector<std::pair<std::string, std::string>> messages;  // For chat models
    std::map<std::string, std::string> systemContext;           // System vars
    bool useToolCalling = false;
    std::vector<json> availableTools;                           // For function calling
};

struct CompletionResponse {
    std::string completion;
    std::vector<ParsedCompletion> chunks;
    int totalTokens = 0;
    int promptTokens = 0;
    int completionTokens = 0;
    int64_t latencyMs = 0;
    bool success = false;
    std::string errorMessage;
    std::vector<json> toolCalls;  // If function calling was used
};

/**
 * Represents a tool/function the model can call
 */
struct ToolDefinition {
    std::string name;
    std::string description;
    json parameters;  // JSON Schema for parameters
    std::function<json(const json&)> handler;  // Implementation
};

class AIImplementation {
private:
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;
    std::shared_ptr<HTTPClient> m_httpClient;
    std::shared_ptr<ResponseParser> m_responseParser;
    std::shared_ptr<ModelTester> m_modelTester;

    // Configuration
    LLMConfig m_config;
    std::map<std::string, ToolDefinition> m_registeredTools;

    // State tracking
    std::vector<std::pair<std::string, std::string>> m_conversationHistory;  // role -> content
    size_t m_totalTokensUsed = 0;
    int64_t m_totalLatency = 0;

    // Internal methods
    std::string buildPrompt(const CompletionRequest& request);
    json buildChatMessages(const CompletionRequest& request);
    CompletionResponse parseOllamaResponse(const std::string& responseBody);
    CompletionResponse parseOpenAIResponse(const std::string& responseBody);
    CompletionResponse parseAnthropicResponse(const std::string& responseBody);
    bool validateToolCall(const json& toolCall);

public:
    AIImplementation(
        std::shared_ptr<Logger> logger,
        std::shared_ptr<Metrics> metrics,
        std::shared_ptr<HTTPClient> httpClient,
        std::shared_ptr<ResponseParser> responseParser,
        std::shared_ptr<ModelTester> modelTester
    );

    /**
     * Initialize with LLM configuration
     * @param config LLM backend and model settings
     * @return True if initialization successful
     */
    bool initialize(const LLMConfig& config);

    /**
     * Send completion request to LLM backend
     * @param request Completion request
     * @return Completion response with parsed chunks
     */
    CompletionResponse complete(const CompletionRequest& request);

    /**
     * Stream completion with callback for each chunk
     * @param request Completion request
     * @param chunkCallback Called for each parsed completion chunk
     * @return Overall completion response
     */
    CompletionResponse streamComplete(
        const CompletionRequest& request,
        std::function<void(const ParsedCompletion&)> chunkCallback
    );

    /**
     * Register a tool/function the model can call
     * @param tool Tool definition with handler
     */
    void registerTool(const ToolDefinition& tool);

    /**
     * Execute a tool call from the model
     * @param toolName Name of the tool to execute
     * @param parameters Parameters passed by the model
     * @return Result of tool execution
     */
    json executeTool(const std::string& toolName, const json& parameters);

    /**
     * Agentic loop: complete -> parse tool calls -> execute -> repeat
     * @param request Initial completion request
     * @param maxIterations Maximum number of iterations
     * @return Final completion response
     */
    CompletionResponse agenticLoop(
        const CompletionRequest& request,
        int maxIterations = 5
    );

    /**
     * Add message to conversation history
     * @param role "user" or "assistant"
     * @param content Message content
     */
    void addToHistory(const std::string& role, const std::string& content);

    /**
     * Clear conversation history
     */
    void clearHistory();

    /**
     * Get conversation history
     * @return Vector of (role, content) pairs
     */
    std::vector<std::pair<std::string, std::string>> getHistory() const;

    /**
     * Estimate token count for text
     * @param text Input text
     * @return Estimated token count
     */
    int estimateTokenCount(const std::string& text);

    /**
     * Check if model supports tool calling
     * @return True if tool calling is supported
     */
    bool supportsToolCalling() const;

    /**
     * Test connectivity to LLM backend
     * @return True if backend is reachable and responsive
     */
    bool testConnectivity();

    /**
     * Get current LLM configuration
     * @return Current configuration
     */
    LLMConfig getConfig() const;

    /**
     * Get usage statistics
     * @return JSON object with token usage and latency stats
     */
    json getUsageStats() const;
};

/**
 * Built-in prompt templates
 */
namespace PromptTemplates {
    std::string codeGeneration(
        const std::string& codeContext,
        const std::string& requirement
    );

    std::string codeReview(
        const std::string& code,
        const std::string& language
    );

    std::string bugFix(
        const std::string& code,
        const std::string& errorMessage,
        const std::string& stackTrace
    );

    std::string refactoring(
        const std::string& code,
        const std::string& language,
        const std::string& objective
    );

    std::string documentation(
        const std::string& code,
        const std::string& language
    );
}
