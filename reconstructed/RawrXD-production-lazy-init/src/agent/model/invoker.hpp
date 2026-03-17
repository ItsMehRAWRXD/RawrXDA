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
#include <QPair>
#include <QTimer>
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
     * @brief Set the endpoint URL for the LLM service
     * @param endpoint New endpoint URL
     *
     * This allows dynamic switching of the endpoint, for example
     * during hot patching. The backend type remains unchanged.
     */
    void setEndpoint(const QString& endpoint) { setLLMBackend(m_backend, endpoint, m_apiKey); }

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

    /**
     * @brief Set backend failover chain (ordered list of fallback backends)
     * @param chain List of backend names to try in sequence (e.g., ["ollama", "claude", "openai"])
     *
     * When circuit breaker trips on one backend, automatically switch to next in chain.
     * If not set, uses default chain: Ollama -> Claude -> OpenAI
     */
    void setBackendFailoverChain(const QStringList& chain) { m_backendFailoverChain = chain; }

    /**
     * @brief Get current failure count for a backend
     * @param backend Backend name
     * @return Number of consecutive failures (0 if none)
     */
    int getBackendFailureCount(const QString& backend) const {
        return m_failureCountPerBackend.value(backend, 0);
    }

    /**
     * @brief Manually reset circuit breaker for a backend (for testing/diagnostics)
     * @param backend Backend name to reset
     */
    void manualResetCircuitBreaker(const QString& backend) {
        m_failureCountPerBackend[backend] = 0;
        m_circuitBreakerOpen[backend] = false;
        m_lastFailurePerBackend.remove(backend);
        qInfo() << "[ModelInvoker::CB] Manual reset for" << backend;
    }

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

    /**
     * @brief Emitted when circuit breaker trips for a backend
     * @param backend Backend name that failed
     * @param failureCount Number of consecutive failures
     * @param message Reason for circuit breaker activation
     */
    void circuitBreakerTripped(const QString& backend, int failureCount, const QString& message);

    /**
     * @brief Emitted when backend failover occurs
     * @param fromBackend Original backend that failed
     * @param toBackend Fallback backend to attempt
     */
    void backendFailover(const QString& fromBackend, const QString& toBackend);

    /**
     * @brief Emitted when circuit breaker resets (half-open state)
     * @param backend Backend name attempting recovery
     */
    void circuitBreakerReset(const QString& backend);

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
    QString buildSystemPrompt(const QStringList& tools) const;

    /**
     * @brief Build user message with wish and context
     * @param params Invocation parameters
     * @return User message for LLM
     */
    QString buildUserMessage(const InvocationParams& params) const;

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

    /**
     * @brief Start asynchronous request with retry/backoff
     * @param params Invocation parameters
     * @param attempt Current attempt number (1-based)
     */
    void startAsyncRequest(const InvocationParams& params, int attempt);

    /**
     * @brief Build network payload for current backend (async path)
     */
    QPair<QNetworkRequest, QByteArray> buildRequestPayload(const InvocationParams& params) const;

    /**
     * @brief Clean up any pending reply and timers
     */
    void clearPending();

    /**
     * @brief Check if circuit breaker is open for a backend
     * @param backend Backend name to check
     * @return true if breaker is open (fail-fast mode)
     */
    bool isCircuitBreakerOpen(const QString& backend);

    /**
     * @brief Record a failure for a backend
     * @param backend Backend name that failed
     * @param emit_signal If true, emit circuitBreakerTripped when threshold reached
     */
    void recordBackendFailure(const QString& backend, bool emit_signal = true);

    /**
     * @brief Reset failure counter for a backend on successful response
     * @param backend Backend name that succeeded
     */
    void resetBackendFailureCount(const QString& backend);

    /**
     * @brief Attempt to transition circuit breaker from open to half-open
     * @param backend Backend name to probe
     * @return true if enough time has passed for reset attempt
     */
    bool shouldAttemptCircuitReset(const QString& backend);

    /**
     * @brief Get next available backend in failover chain
     * @param currentBackend Current (failed) backend
     * @return Name of next backend to try, or empty string if no fallback available
     */
    QString getNextFailoverBackend(const QString& currentBackend);

    // ─────────────────────────────────────────────────────────────────────
    // Member Variables
    // ─────────────────────────────────────────────────────────────────────

    QString m_backend;                      ///< LLM backend type
    QString m_endpoint;                     ///< LLM service URL
    QString m_apiKey;                       ///< Optional API key
    QString m_model = "mistral";            ///< Default model name

    bool m_isInvoking = false;              ///< Request in progress
    bool m_cachingEnabled = true;           ///< Enable response caching

    // Async orchestration
    int m_maxRetries = 3;                   ///< Max retry attempts per request
    int m_timeoutMs = 15000;                ///< Per-request timeout
    int m_retryDelayMs = 750;               ///< Base backoff delay (ms)
    int m_currentAttempt = 0;               ///< Current attempt for in-flight async request
    QNetworkReply* m_pendingReply = nullptr;///< In-flight reply (async)
    std::unique_ptr<QTimer> m_timeoutTimer; ///< Timeout guard for async calls
    InvocationParams m_pendingParams;       ///< Params for current async request
    std::chrono::high_resolution_clock::time_point m_requestStartTime; ///< For latency tracking

    std::unique_ptr<QNetworkAccessManager> m_networkManager;
    QMap<QString, LLMResponse> m_responseCache;    ///< Response cache

    QString m_customSystemPrompt;           ///< Override system prompt
    QMap<QString, float> m_codebaseEmbeddings;    ///< RAG embeddings

    // ─────────────────────────────────────────────────────────────────────
    // Circuit Breaker (Production Resilience)
    // ─────────────────────────────────────────────────────────────────────
    
    static constexpr int CIRCUIT_BREAKER_THRESHOLD = 5;    ///< Consecutive failures to trip breaker
    static constexpr int CIRCUIT_BREAKER_RESET_MS = 30000; ///< Time to attempt reset (ms)
    
    QMap<QString, int> m_failureCountPerBackend;    ///< Consecutive failures per backend
    QMap<QString, QDateTime> m_lastFailurePerBackend; ///< Last failure timestamp per backend
    QMap<QString, bool> m_circuitBreakerOpen;       ///< Circuit breaker state per backend
    QStringList m_backendFailoverChain;             ///< Ordered list of backends to try in sequence
};
