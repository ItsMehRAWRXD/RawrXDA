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
#include <functional>
#include <atomic>
#include <mutex>
#include <nlohmann/json.hpp>
#include <memory>

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
    
    // Vision/Multimodal Support
    std::vector<uint8_t> imageData;             ///< Raw image data for vision models
    std::string imageMediaType;                 ///< MIME type: "image/png", "image/jpeg", etc.
    bool hasImage() const { return !imageData.empty(); }
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
 *
 * Responsibilities:
 * - Connect to Ollama (local) or cloud LLM API
 * - Build system prompt with available tools
 * - Send wish with context to LLM
 * - Parse JSON action plan from response
 * - Handle timeouts, retries, fallbacks
 * - Validate plan sanity (no infinite loops, dangerous commands)
 *
 * @note Thread-safe via Qt's signal/slot mechanism
 * @note Uses queued connections for network operations
 *
 * @example
 * @code
 * ModelInvoker invoker;
 * invoker.setLLMBackend("ollama", "http://localhost:11434");
 *
 * InvocationParams params;
 * params.wish = "Add Q8_K kernel";
 * params.context = "RawrXD quantization project";
 *
 * connect(&invoker, &ModelInvoker::planGenerated,
 *         this, &MyClass::onPlanReady);
 *
 * invoker.invokeAsync(params);
 * @endcode
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
     * @brief Analyze an image using vision-capable model (Claude/GPT-4V)
     * @param imageData Base64-encoded image data
     * @param mediaType MIME type (image/png, image/jpeg, etc.)
     * @param prompt User's question about the image
     * @return LLM response with image analysis
     * 
     * @note Requires vision-capable backend (claude-3-sonnet, gpt-4-vision)
     */
    LLMResponse analyzeImage(const std::vector<uint8_t>& imageData,
                             const std::string& mediaType,
                             const std::string& prompt);

    /**
     * @brief Capture and analyze screenshot (for UI debugging)
     * @param prompt Question about the screenshot
     * @return LLM response with UI analysis
     */
    LLMResponse analyzeScreenshot(const std::string& prompt);

    /**
     * @brief Enable/disable request caching (for identical wishes)
     * @param enabled true to cache LLM responses
     */
    void setCachingEnabled(bool enabled) { m_cachingEnabled = enabled; }

    // Callbacks (replacing Qt signals)
    using PlanGenerationStartedCallback = std::function<void(const std::string& wish)>;
    using PlanGeneratedCallback = std::function<void(const nlohmann::json& plan, const std::string& reasoning)>;
    using InvocationErrorCallback = std::function<void(const std::string& error, bool recoverable)>;
    using StatusUpdatedCallback = std::function<void(const std::string& message)>;

    void setPlanGenerationStartedCallback(PlanGenerationStartedCallback cb) { m_onPlanGenerationStarted = std::move(cb); }
    void setPlanGeneratedCallback(PlanGeneratedCallback cb) { m_onPlanGenerated = std::move(cb); }
    void setInvocationErrorCallback(InvocationErrorCallback cb) { m_onInvocationError = std::move(cb); }
    void setStatusUpdatedCallback(StatusUpdatedCallback cb) { m_onStatusUpdated = std::move(cb); }

private:
    void onLLMResponseReceived(const std::string& data);
    void onNetworkError(const std::string& error);
    void onRequestTimeout();

private:
    /**
     * @brief Build system prompt with tool descriptions
     * @return Complete system prompt for LLM
     */
    std::string buildSystemPrompt(const std::vector<std::string>& tools);

    /**
     * @brief Build user message with wish and context
     * @param params Invocation parameters
     * @return User message for LLM
     */
    std::string buildUserMessage(const InvocationParams& params);

    /**
     * @brief Send HTTP request to Ollama API
     * @param params Request parameters
     * @return HTTP response as QJsonObject
     */
    nlohmann::json sendOllamaRequest(const std::string& model,
                                   const std::string& prompt,
                                   int maxTokens,
                                   double temperature);

    /**
     * @brief Send HTTP request to Claude API
     */
    nlohmann::json sendClaudeRequest(const std::string& prompt,
                                   int maxTokens,
                                   double temperature);

    /**
     * @brief Send HTTP request to OpenAI API
     */
    nlohmann::json sendOpenAIRequest(const std::string& prompt,
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
    nlohmann::json parsePlan(const std::string& llmOutput);

    /**
     * @brief Validate plan sanity before returning
     * @param plan Proposed action plan
     * @return true if plan is safe to execute
     */
    bool validatePlanSanity(const nlohmann::json& plan);

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

    std::map<std::string, LLMResponse> m_responseCache;    ///< Response cache

    std::string m_customSystemPrompt;           ///< Override system prompt
    std::map<std::string, float> m_codebaseEmbeddings;    ///< RAG embeddings

    PlanGenerationStartedCallback m_onPlanGenerationStarted;
    PlanGeneratedCallback m_onPlanGenerated;
    InvocationErrorCallback m_onInvocationError;
    StatusUpdatedCallback m_onStatusUpdated;
};
