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

#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <memory>

/**
 * @struct InvocationParams
 * @brief Parameters for LLM invocation
 */
struct InvocationParams {
    QString wish;                           ///< User's natural language request
    QString context;                        ///< IDE state/environment context
    QStringList availableTools;             ///< Tools accessible to agent
    QString codebaseContext;                ///< Relevant codebase snippets (RAG)
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
    QString rawOutput;                      ///< Full LLM response text
    QJsonArray parsedPlan;                  ///< Structured action plan
    QString reasoning;                      ///< Agent's reasoning (for logging)
    int tokensUsed = 0;
    QString error;
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
class ModelInvoker : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Qt parent object
     */
    explicit ModelInvoker(QObject* parent = nullptr);

    /**
     * @brief Destructor
     */
    ~ModelInvoker() override;

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
    void setLLMBackend(const QString& backend,
                       const QString& endpoint,
                       const QString& apiKey = QString());

    /**
     * @brief Get current LLM backend type
     * @return Backend name ("ollama", "claude", "openai")
     */
    QString getLLMBackend() const { return m_backend; }

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
    void setSystemPromptTemplate(const QString& template_);

    /**
     * @brief Set RAG codebase embeddings (for context injection)
     * @param embeddings Map of file path → relevance score
     *
     * Used to inject relevant code snippets into LLM context.
     */
    void setCodebaseEmbeddings(const QMap<QString, float>& embeddings);

    /**
     * @brief Enable/disable request caching (for identical wishes)
     * @param enabled true to cache LLM responses
     */
    void setCachingEnabled(bool enabled) { m_cachingEnabled = enabled; }

signals:
    /**
     * @brief Emitted when LLM plan generation begins
     * @param wish The user's request
     */
    void planGenerationStarted(const QString& wish);

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
    void invocationError(const QString& error, bool recoverable);

    /**
     * @brief Emitted periodically during long requests
     * @param message Status message
     */
    void statusUpdated(const QString& message);

private slots:
    /**
     * @brief Handle network response from LLM backend
     */
    void onLLMResponseReceived(const QByteArray& data);

    /**
     * @brief Handle network error
     */
    void onNetworkError(const QString& error);

    /**
     * @brief Handle timeout
     */
    void onRequestTimeout();

private:
    /**
     * @brief Build system prompt with tool descriptions
     * @return Complete system prompt for LLM
     */
    QString buildSystemPrompt(const QStringList& tools);

    /**
     * @brief Build user message with wish and context
     * @param params Invocation parameters
     * @return User message for LLM
     */
    QString buildUserMessage(const InvocationParams& params);

    /**
     * @brief Send HTTP request to Ollama API
     * @param params Request parameters
     * @return HTTP response as QJsonObject
     */
    QJsonObject sendOllamaRequest(const QString& model,
                                   const QString& prompt,
                                   int maxTokens,
                                   double temperature);

    /**
     * @brief Send HTTP request to Claude API
     */
    QJsonObject sendClaudeRequest(const QString& prompt,
                                   int maxTokens,
                                   double temperature);

    /**
     * @brief Send HTTP request to OpenAI API
     */
    QJsonObject sendOpenAIRequest(const QString& prompt,
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
    QJsonArray parsePlan(const QString& llmOutput);

    /**
     * @brief Validate plan sanity before returning
     * @param plan Proposed action plan
     * @return true if plan is safe to execute
     */
    bool validatePlanSanity(const QJsonArray& plan);

    /**
     * @brief Get cache key for request (for caching)
     */
    QString getCacheKey(const InvocationParams& params) const;

    /**
     * @brief Load cached response if available
     */
    LLMResponse getCachedResponse(const QString& key) const;

    /**
     * @brief Store response in cache
     */
    void cacheResponse(const QString& key, const LLMResponse& response);

    // ─────────────────────────────────────────────────────────────────────
    // Member Variables
    // ─────────────────────────────────────────────────────────────────────

    QString m_backend;                      ///< LLM backend type
    QString m_endpoint;                     ///< LLM service URL
    QString m_apiKey;                       ///< Optional API key
    QString m_model = "mistral";            ///< Default model name

    bool m_isInvoking = false;              ///< Request in progress
    bool m_cachingEnabled = true;           ///< Enable response caching

    std::unique_ptr<QNetworkAccessManager> m_networkManager;
    QMap<QString, LLMResponse> m_responseCache;    ///< Response cache

    QString m_customSystemPrompt;           ///< Override system prompt
    QMap<QString, float> m_codebaseEmbeddings;    ///< RAG embeddings
};
