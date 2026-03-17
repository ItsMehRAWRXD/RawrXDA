#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <nlohmann/json.hpp>

namespace RawrXD {

/**
 * @brief Sentry crash reporting integration for production error tracking
 * 
 * NATIVE C++20 IMPLEMENTATION (Zero Qt)
 */
class SentryIntegration {
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
    void captureException(const std::string& exception, const nlohmann::json& context = nlohmann::json());
    
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

    using ErrorReportedCallback = std::function<void(const std::string& errorId)>;
    using TransactionCompletedCallback = std::function<void(const std::string& transactionId, long long durationMs)>;

    void setErrorReportedCallback(ErrorReportedCallback cb) { m_onErrorReported = std::move(cb); }
    void setTransactionCompletedCallback(TransactionCompletedCallback cb) { m_onTransactionCompleted = std::move(cb); }

private:
    explicit SentryIntegration();
    ~SentryIntegration();
    
    static SentryIntegration* s_instance;
    
    bool m_initialized;
    std::string m_dsn;
    std::map<std::string, long long> m_activeTransactions;  ///< Transaction ID -> start timestamp
    std::vector<nlohmann::json> m_breadcrumbs;  ///< Breadcrumb trail for context

    ErrorReportedCallback m_onErrorReported;
    TransactionCompletedCallback m_onTransactionCompleted;
    
    // PRODUCTION-READY: Send event to Sentry HTTP API via WinHTTP
    void sendEvent(const nlohmann::json& event);
};

} // namespace RawrXD
