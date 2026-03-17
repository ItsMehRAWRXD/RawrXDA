#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <QString>
#include <QJsonObject>
#include <QException>
#include <functional>
#include <memory>

namespace RawrXD {

enum class ErrorSeverity {
    LOW,
    MEDIUM,
    HIGH,
    CRITICAL
};

enum class ErrorCategory {
    CONFIGURATION,
    NETWORK,
    DATABASE,
    FILE_SYSTEM,
    MEMORY,
    MODEL,
    AGENT,
    UI,
    UNKNOWN
};

class ErrorContext {
public:
    ErrorContext() = default;
    
    ErrorContext& setSeverity(ErrorSeverity severity) { severity_ = severity; return *this; }
    ErrorContext& setCategory(ErrorCategory category) { category_ = category; return *this; }
    ErrorContext& setOperation(const QString& operation) { operation_ = operation; return *this; }
    ErrorContext& setUserId(const QString& userId) { userId_ = userId; return *this; }
    ErrorContext& setSessionId(const QString& sessionId) { sessionId_ = sessionId; return *this; }
    ErrorContext& addMetadata(const QString& key, const QJsonValue& value) { metadata_[key] = value; return *this; }
    
    ErrorSeverity severity() const { return severity_; }
    ErrorCategory category() const { return category_; }
    QString operation() const { return operation_; }
    QString userId() const { return userId_; }
    QString sessionId() const { return sessionId_; }
    QJsonObject metadata() const { return metadata_; }
    
private:
    ErrorSeverity severity_ = ErrorSeverity::MEDIUM;
    ErrorCategory category_ = ErrorCategory::UNKNOWN;
    QString operation_;
    QString userId_;
    QString sessionId_;
    QJsonObject metadata_;
};

class ProductionException : public QException {
public:
    ProductionException(const QString& message, const ErrorContext& context = ErrorContext());
    
    const char* what() const noexcept override;
    QString message() const { return message_; }
    ErrorContext context() const { return context_; }
    
    void raise() const override { throw *this; }
    QException* clone() const override { return new ProductionException(*this); }
    
private:
    QString message_;
    ErrorContext context_;
};

class ErrorHandler {
public:
    static ErrorHandler& instance();
    
    void initialize();
    void shutdown();
    
    // Centralized error handling
    void handleError(const QString& message, const ErrorContext& context = ErrorContext());
    void handleException(const std::exception& e, const ErrorContext& context = ErrorContext());
    void handleProductionException(const ProductionException& e);
    
    // Resource guards
    template<typename Resource>
    class ResourceGuard {
    public:
        ResourceGuard(Resource* resource, std::function<void(Resource*)> cleanupFunc)
            : resource_(resource), cleanupFunc_(cleanupFunc) {}
        
        ~ResourceGuard() {
            if (resource_ && cleanupFunc_) {
                try {
                    cleanupFunc_(resource_);
                } catch (const std::exception& e) {
                    ErrorHandler::instance().handleException(e, ErrorContext()
                        .setSeverity(ErrorSeverity::HIGH)
                        .setCategory(ErrorCategory::MEMORY)
                        .setOperation("ResourceGuard cleanup"));
                }
            }
        }
        
        Resource* get() const { return resource_; }
        Resource* operator->() const { return resource_; }
        
    private:
        Resource* resource_;
        std::function<void(Resource*)> cleanupFunc_;
    };
    
    // High-level exception handler
    void setGlobalExceptionHandler(std::function<void(const std::exception&)> handler);
    
    // Recovery mechanisms
    bool attemptRecovery(ErrorCategory category);
    void registerRecoveryHandler(ErrorCategory category, std::function<bool()> handler);
    
private:
    ErrorHandler() = default;
    ~ErrorHandler();
    
    void logError(const QString& message, const ErrorContext& context);
    void notifyMonitoring(const QString& message, const ErrorContext& context);
    bool shouldEscalate(const ErrorContext& context);
    
    std::function<void(const std::exception&)> globalExceptionHandler_;
    QHash<ErrorCategory, std::function<bool()>> recoveryHandlers_;
    QHash<ErrorCategory, int> errorCounts_;
    
    static const int MAX_ERRORS_BEFORE_ESCALATION = 10;
};

// Convenience macros
#define ERROR_HANDLE(message, context) RawrXD::ErrorHandler::instance().handleError(message, context)
#define ERROR_EXCEPTION(e, context) RawrXD::ErrorHandler::instance().handleException(e, context)
#define ERROR_PRODUCTION(e) RawrXD::ErrorHandler::instance().handleProductionException(e)

#define RESOURCE_GUARD(resource, cleanup) RawrXD::ErrorHandler::ResourceGuard<std::remove_pointer<decltype(resource)>::type> guard(resource, cleanup)

} // namespace RawrXD

#endif // ERROR_HANDLER_H