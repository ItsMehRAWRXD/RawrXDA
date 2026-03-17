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

#include <memory>
#include <string>
#include <vector>
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
    int maxTokens = 2000;                   ///< Output token limit
    double temperature = 0.7;               ///< Sampling temperature (0-1)
    int timeoutMs = 30000;                  ///< Request timeout
};

/**
 * @struct LLMResponse
 * @brief Parsed response from LLM
 */
struct LLMResponse {
    bool success = false;
    std::string rawOutput;                      ///< Full LLM response text
    nlohmann::json parsedPlan;                  ///< Structured action plan
    std::string reasoning;                      ///< Agent's reasoning
    int tokensUsed = 0;
    std::string error;
};

/**
 * @class ModelInvoker
 * @brief Bridges natural language wishes to structured action plans via LLM
 *
 * Responsibilities:
 * - Connect to Ollama (local) or cloud LLM API
 * - Build system prompt with available tools
 * - Send wish with context to LLM
 * - Parse JSON action plan from response
 * - Handle timeouts, retries, fallbacks
 * - Validate plan sanity (no infinite loops, dangerous commands)
 *
 * @note Thread-safe via callback mechanism
 * @note Network operations are non-blocking with callback registration
 */
class ModelInvoker {
public:
    explicit ModelInvoker();
    ~ModelInvoker();

    /**
     * @brief Set the LLM backend and endpoint
     * @param backend Type: "ollama", "claude", "openai"
     * @param endpoint URL to LLM service
     * @param apiKey Optional API key for cloud services
     *
     * @note Ollama: endpoint = "http://localhost:11434"
     * @note Claude: endpoint = "https://api.anthropic.com", requires apiKey
     * @note OpenAI: endpoint = "https://api.openai.com/v1", requires apiKey
     */
    void setLLMBackend(const std::string& backend,
                       const std::string& endpoint,
                       const std::string& apiKey = std::string());

    /**
     * @brief Get current LLM backend type
     * @return Backend name ("ollama", "claude", "openai")
     */
    std::string getLLMBackend() const { return m_backend; }

    /**
     * @brief Synchronous wish→plan transformation (blocks caller)
     * @param params Invocation parameters
     * @return Parsed LLM response with action plan
     *
     * @warning Blocks the calling thread. Use invokeAsync() for UI thread.
     * @note Should not be called from main/UI thread
     */
    LLMResponse invoke(const InvocationParams& params);

    /**
     * @brief Asynchronous wish→plan transformation (non-blocking)
     * @param params Invocation parameters
     *
     * Emits planGenerated() signal when complete.
     * Safe to call from UI thread.
     *
     * @see planGenerated()
     */
    void invokeAsync(const InvocationParams& params);

    /**
     * @brief Cancel any in-flight LLM request
     */
    void cancelPendingRequest();

    /**
     * @brief Check if request is in progress
     * @return true if LLM call is pending
     */
    bool isInvoking() const { return m_isInvoking; }

    /**
     * @brief Set custom system prompt template
     * @param template_ Prompt template with placeholders:
     *        {tools}, {wish}, {context}, {codebase}
     *
     * Default template is built-in; override for customization.
     */
    void setSystemPromptTemplate(const std::string& template_);

    /**
     * @brief Set RAG codebase embeddings (for context injection)
     * @param embeddings Map of file path → relevance score
     *
     * Used to inject relevant code snippets into LLM context.
     */
    void setCodebaseEmbeddings(const std::map<std::string, float>& embeddings);

    /**
     * @brief Enable/disable request caching (for identical wishes)
     * @param enabled true to cache LLM responses
     */
    void setCachingEnabled(bool enabled) { m_cachingEnabled = enabled; }

\npublic:\n    /**
     * @brief Emitted when LLM plan generation begins
     * @param wish The user's request
     */
    void planGenerationStarted(const std::string& wish);

    /**
     * @brief Emitted when plan is ready
     * @param response Parsed response with action plan
     *
     * Connect to this to receive structured actions.
     */
    void planGenerated(const LLMResponse& response);

    /**
     * @brief Emitted on error during invocation
     * @param error Error message
     * @param recoverable true if request can be retried
     */
    void invocationError(const std::string& error, bool recoverable);

    /**
     * @brief Emitted periodically during long requests
     * @param message Status message
     */
    void statusUpdated(const std::string& message);

\nprivate:\n    /**
     * @brief Handle network response from LLM backend
     */
    void onLLMResponseReceived(const std::vector<uint8_t>& data);

    /**
     * @brief Handle network error
     */
    void onNetworkError(const std::string& error);

    /**
     * @brief Handle timeout
     */
    void onRequestTimeout();

private:
    /**
     * @brief Build system prompt with tool descriptions
     * @return Complete system prompt for LLM
     */
    std::string buildSystemPrompt(const std::stringList& tools);

    /**
     * @brief Build user message with wish and context
     * @param params Invocation parameters
     * @return User message for LLM
     */
    std::string buildUserMessage(const InvocationParams& params);

    /**
     * @brief Send HTTP request to Ollama API
     * @param params Request parameters
     * @return HTTP response as void*
     */
    void* sendOllamaRequest(const std::string& model,
                                   const std::string& prompt,
                                   int maxTokens,
                                   double temperature);

    /**
     * @brief Send HTTP request to Claude API
     */
    void* sendClaudeRequest(const std::string& prompt,
                                   int maxTokens,
                                   double temperature);

    /**
     * @brief Send HTTP request to OpenAI API
     */
    void* sendOpenAIRequest(const std::string& prompt,
                                   int maxTokens,
                                   double temperature);

    /**
     * @brief Parse LLM response into structured plan
     * @param llmOutput Raw text response from LLM
     * @return Parsed JSON array of actions
     *
     * Attempts multiple parsing strategies:
     * 1. Direct JSON extraction (```json ... ```)
     * 2. Regex-based action matching
     * 3. Fallback to best-effort parsing
     */
    void* parsePlan(const std::string& llmOutput);

    /**
     * @brief Validate plan sanity before returning
     * @param plan Proposed action plan
     * @return true if plan is safe to execute
     */
    bool validatePlanSanity(const void*& plan);

    /**
     * @brief Get cache key for request (for caching)
     */
    std::string getCacheKey(const InvocationParams& params) const;

    /**
     * @brief Load cached response if available
     */
    LLMResponse getCachedResponse(const std::string& key) const;

    /**
     * @brief Store response in cache
     */
    void cacheResponse(const std::string& key, const LLMResponse& response);

    // ─────────────────────────────────────────────────────────────────────
    // Member Variables
    // ─────────────────────────────────────────────────────────────────────

    std::string m_backend;                      ///< LLM backend type
    std::string m_endpoint;                     ///< LLM service URL
    std::string m_apiKey;                       ///< Optional API key
    std::string m_model = "mistral";            ///< Default model name

    bool m_isInvoking = false;              ///< Request in progress
    bool m_cachingEnabled = true;           ///< Enable response caching

    std::unique_ptr<void*> m_networkManager;
    std::map<std::string, LLMResponse> m_responseCache;    ///< Response cache

    std::string m_customSystemPrompt;           ///< Override system prompt
    std::map<std::string, float> m_codebaseEmbeddings;    ///< RAG embeddings
};





