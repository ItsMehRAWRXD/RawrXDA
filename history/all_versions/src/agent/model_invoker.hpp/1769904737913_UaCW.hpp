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
     * @brief Enable/disable request caching
     */
    void setCachingEnabled(bool enabled) { m_cachingEnabled = enabled; }

    /**
     * @brief Set endpoint URL directly (for hot patching)
     */
    void setEndpoint(const std::string& endpoint) { m_endpoint = endpoint; }

    // Callbacks (replacing signals)
    std::function<void(const std::string&)> onPlanGenerationStarted;
    std::function<void(const LLMResponse&)> onPlanGenerated;
    std::function<void(const std::string&, bool)> onInvocationError;
    std::function<void(const std::string&)> onStatusUpdated;

private:
    /**
     * @brief Build system prompt with tool descriptions
     */
    std::string buildSystemPrompt(const std::vector<std::string>& tools);

    /**
     * @brief Build user message with wish and context
     */
    std::string buildUserMessage(const InvocationParams& params);

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

    nlohmann::json parsePlan(const std::string& llmOutput);

    bool validatePlanSanity(const nlohmann::json& plan);

    std::string getCacheKey(const InvocationParams& params) const;

    LLMResponse getCachedResponse(const std::string& key) const;

    void cacheResponse(const std::string& key, const LLMResponse& response);

    // Network helper (placeholder or stub)
    std::string performHttpRequest(const std::string& url, 
                                   const std::string& method, 
                                   const std::string& body, 
                                   const std::map<std::string, std::string>& headers);

    // ─────────────────────────────────────────────────────────────────────
    // Member Variables
    // ─────────────────────────────────────────────────────────────────────

    std::string m_backend;
    std::string m_endpoint;
    std::string m_apiKey;
    std::string m_model = "mistral";

    bool m_isInvoking = false;
    bool m_cachingEnabled = true;

    std::map<std::string, LLMResponse> m_responseCache;
    std::string m_customSystemPrompt;
    std::map<std::string, float> m_codebaseEmbeddings;
};
