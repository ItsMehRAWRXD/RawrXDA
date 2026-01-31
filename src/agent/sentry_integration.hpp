#pragma once


/**
 * @brief Sentry crash reporting integration for production error tracking
 * 
 * PRODUCTION-READY FEATURES:
 * - Centralized error capture with stack traces
 * - Performance monitoring (latency tracking)
 * - Breadcrumb logging for debugging context
 * - Environment-based configuration (DSN from env var)
 * - Privacy-respecting (no PII collection)
 */
class SentryIntegration : public void
{

public:
    static SentryIntegration* instance();
    
    /**
     * @brief Initialize Sentry with DSN from environment variable SENTRY_DSN
     * @return true if initialization successful
     */
    bool initialize();
    
    /**
     * @brief Capture an exception with context
     * @param exception Exception message
     * @param context Additional context (file, function, line)
     */
    void captureException(const std::string& exception, const void*& context = void*());
    
    /**
     * @brief Capture a message (non-error event)
     * @param message Message to log
     * @param level Severity level (info, warning, error)
     */
    void captureMessage(const std::string& message, const std::string& level = "info");
    
    /**
     * @brief Add breadcrumb for debugging trail
     * @param message Breadcrumb message
     * @param category Event category (navigation, http, etc.)
     */
    void addBreadcrumb(const std::string& message, const std::string& category = "default");
    
    /**
     * @brief Start performance transaction
     * @param operation Operation name (e.g., "model.load", "inference.generate")
     * @return Transaction ID
     */
    std::string startTransaction(const std::string& operation);
    
    /**
     * @brief Finish performance transaction
     * @param transactionId Transaction ID from startTransaction
     */
    void finishTransaction(const std::string& transactionId);
    
    /**
     * @brief Set user context (only non-PII data like user ID hash)
     * @param userId Anonymized user identifier
     */
    void setUser(const std::string& userId);

    void errorReported(const std::string& errorId);
    void transactionCompleted(const std::string& transactionId, qint64 durationMs);

private:
    explicit SentryIntegration(void* parent = nullptr);
    ~SentryIntegration();
    
    static SentryIntegration* s_instance;
    
    bool m_initialized;
    std::string m_dsn;
    std::unordered_map<std::string, qint64> m_activeTransactions;  ///< Transaction ID -> start timestamp
    std::vector<void*> m_breadcrumbs;  ///< Breadcrumb trail for context
    
    // PRODUCTION-READY: Send event to Sentry HTTP API
    void sendEvent(const void*& event);
};

