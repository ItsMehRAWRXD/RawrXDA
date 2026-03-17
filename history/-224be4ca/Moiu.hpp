/**
 * @file model_invoker.hpp
 * @brief LLM invocation layer for wish→plan transformation
 *
 * Handles communication with local Ollama or cloud APIs to convert
 * natural language wishes into structured action plans.
 *
 * @author RawrXD Agent Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>
#include <windows.h>

/**
 * @struct InvocationParams
 * @brief Parameters for LLM invocation
 */
struct InvocationParams {
    std::string wish;                           ///< User's natural language request
    std::string context;                        ///< IDE state/environment context
    std::vector<std::string> availableTools;    ///< Tools accessible to agent
    std::string codebaseContext;                ///< Relevant codebase snippets (RAG)
    int maxTokens = 2000;                       ///< Output token limit
    double temperature = 0.7;                   ///< Sampling temperature (0-1)
    int timeoutMs = 30000;                      ///< Request timeout
};

/**
 * @struct LLMResponse
 * @brief Parsed response from LLM
 */
struct LLMResponse {
    bool success = false;
    std::string rawOutput;                      ///< Full LLM response text
    nlohmann::json parsedPlan;                  ///< Structured action plan
    std::string reasoning;                      ///< Agent's reasoning (for logging)
    int tokensUsed = 0;
    std::string error;
};

/**
 * @class ModelInvoker
 * @brief Bridges natural language wishes to structured action plans via LLM
 */
class ModelInvoker {
public:
    /**
     * @brief Constructor
     */
    explicit ModelInvoker();

    /**
     * @brief Destructor
     */
    virtual ~ModelInvoker();

    /**
     * @brief Set the LLM backend and endpoint
     */
    void setLLMBackend(const std::string& backend,
                       const std::string& endpoint,
                       const std::string& apiKey = "");

    /**
     * @brief Get current LLM backend type
     */
    std::string getLLMBackend() const { return m_backend; }

    /**
     * @brief Synchronous wish→plan transformation (blocks caller)
     */
    LLMResponse invoke(const InvocationParams& params);

    /**
     * @brief Asynchronous wish→plan transformation (non-blocking)
     */
    void invokeAsync(const InvocationParams& params);

    /**
     * @brief Cancel any in-flight LLM request
     */
    void cancelPendingRequest();

    /**
     * @brief Check if request is in progress
     */
    bool isInvoking() const { return m_isInvoking; }

    /**
     * @brief Set custom system prompt template
     */
    void setSystemPromptTemplate(const std::string& template_);

    /**
     * @brief Set RAG codebase embeddings
     */
    void setCodebaseEmbeddings(const std::map<std::string, float>& embeddings);

    /**
     * @brief Raw query without plan parsing (for code completion/chat)
     */
    LLMResponse queryRaw(const std::string& systemPrompt, const std::string& userPrompt, int maxTokens = 2000);

    /**
     * @brief Enable/disable request caching
     */
    void setCachingEnabled(bool enabled) { m_cachingEnabled = enabled; }

    /**
     * @brief Set endpoint URL directly (for hot patching)
     */
    void setEndpoint(const std::string& endpoint) { m_endpoint = endpoint; }

    nlohmann::json sendOllamaRequest(const std::string& model,
                                   const std::string& prompt,
                                   int maxTokens,
                                   double temperature);

    nlohmann::json sendClaudeRequest(const std::string& prompt,
                                   int maxTokens,
                                   double temperature);

    nlohmann::json sendOpenAIRequest(const std::string& prompt,
                                   int maxTokens,
                                   double temperature);

    nlohmann::json sendRawrXDRequest(const std::string& messageBlock, int maxTokens, float temperature);

private:
    /**
     * @brief Build system prompt with tool descriptions
     */
    std::string buildSystemPrompt(const std::vector<std::string>& tools);

    /**
     * @brief Build user message with wish and context
     */
    std::string buildUserMessage(const InvocationParams& params);

    nlohmann::json parsePlan(const std::string& llmOutput);

    bool validatePlanSanity(const nlohmann::json& plan);

    std::string getCacheKey(const InvocationParams& params) const;

    LLMResponse getCachedResponse(const std::string& key) const;

    void cacheResponse(const std::string& key, const LLMResponse& response);

    // Helper method for HTTP requests
    std::string performHttpRequest(const std::string& url, 
                                  const std::string& method, 
                                  const std::string& body, 
                                  const std::map<std::string, std::string>& headers);

    nlohmann::json m_cache;
    bool m_cachingEnabled = true;
};
