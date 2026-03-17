#include "error_handler.h"
#include "logging/structured_logger.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QThread>
#include <QJsonDocument>
#include <stdexcept>

namespace RawrXD {

ProductionException::ProductionException(const QString& message, const ErrorContext& context)
    : message_(message), context_(context) {}

const char* ProductionException::what() const noexcept {
    return message_.toUtf8().constData();
}

ErrorHandler& ErrorHandler::instance() {
    static ErrorHandler instance;
    return instance;
}

void ErrorHandler::initialize() {
    // Set up global exception handler
    std::set_terminate([]() {
        try {
            if (auto exc = std::current_exception()) {
                std::rethrow_exception(exc);
            }
        } catch (const std::exception& e) {
            ErrorHandler::instance().handleException(e, ErrorContext()
                .setSeverity(ErrorSeverity::CRITICAL)
                .setCategory(ErrorCategory::UNKNOWN)
                .setOperation("Unhandled exception"));
        } catch (...) {
            ErrorHandler::instance().handleError("Unknown unhandled exception", ErrorContext()
                .setSeverity(ErrorSeverity::CRITICAL)
                .setCategory(ErrorCategory::UNKNOWN)
                .setOperation("Unhandled exception"));
        }
        
        // Ensure application exits cleanly
        QCoreApplication::exit(1);
    });
    
    LOG_INFO("Error handler initialized");
}

void ErrorHandler::shutdown() {
    LOG_INFO("Error handler shutting down");
}

void ErrorHandler::handleError(const QString& message, const ErrorContext& context) {
    logError(message, context);
    notifyMonitoring(message, context);
    
    if (shouldEscalate(context)) {
        // Escalate to monitoring system or admin notification
        LOG_ERROR("Error requires escalation", {{ "message", message }, { "severity", static_cast<int>(context.severity()) }});
    }
}

void ErrorHandler::handleException(const std::exception& e, const ErrorContext& context) {
    QString message = QString("Exception: %1").arg(e.what());
    
    ErrorContext enhancedContext = context;
    enhancedContext.addMetadata("exception_type", typeid(e).name());
    
    handleError(message, enhancedContext);
    
    if (globalExceptionHandler_) {
        globalExceptionHandler_(e);
    }
}

void ErrorHandler::handleProductionException(const ProductionException& e) {
    handleError(e.message(), e.context());
}

bool ErrorHandler::attemptRecovery(ErrorCategory category) {
    if (recoveryHandlers_.contains(category)) {
        try {
            return recoveryHandlers_[category]();
        } catch (const std::exception& e) {
            handleException(e, ErrorContext()
                .setSeverity(ErrorSeverity::HIGH)
                .setCategory(category)
                .setOperation("Recovery attempt"));
            return false;
        }
    }
    return false;
}

void ErrorHandler::registerRecoveryHandler(ErrorCategory category, std::function<bool()> handler) {
    recoveryHandlers_[category] = handler;
}

void ErrorHandler::setGlobalExceptionHandler(std::function<void(const std::exception&)> handler) {
    globalExceptionHandler_ = handler;
}

void ErrorHandler::logError(const QString& message, const ErrorContext& context) {
    QJsonObject logContext;
    logContext["severity"] = static_cast<int>(context.severity());
    logContext["category"] = static_cast<int>(context.category());
    logContext["operation"] = context.operation();
    logContext["user_id"] = context.userId();
    logContext["session_id"] = context.sessionId();
    
    QJsonObject metadata = context.metadata();
    if (!metadata.isEmpty()) {
        logContext["metadata"] = metadata;
    }
    
    // Update error counts
    errorCounts_[context.category()]++;
    logContext["error_count"] = errorCounts_[context.category()];
    
    // Log based on severity
    switch (context.severity()) {
        case ErrorSeverity::LOW:
            LOG_WARN(message, logContext);
            break;
        case ErrorSeverity::MEDIUM:
            LOG_WARN(message, logContext);
            break;
        case ErrorSeverity::HIGH:
            LOG_ERROR(message, logContext);
            break;
        case ErrorSeverity::CRITICAL:
            LOG_FATAL(message, logContext);
            break;
        default:
            LOG_ERROR(message, logContext);
            break;
    }
}

void ErrorHandler::notifyMonitoring(const QString& message, const ErrorContext& context) {
    // In production, this would send alerts to monitoring systems
    // For now, just log the notification
    QJsonObject monitoringData;
    monitoringData["message"] = message;
    monitoringData["severity"] = static_cast<int>(context.severity());
    monitoringData["category"] = static_cast<int>(context.category());
    monitoringData["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    
    LOG_INFO("Monitoring notification", monitoringData);
}

bool ErrorHandler::shouldEscalate(const ErrorContext& context) {
    // Escalate if critical or if too many errors of same category
    if (context.severity() == ErrorSeverity::CRITICAL) {
        return true;
    }
    
    if (errorCounts_[context.category()] > MAX_ERRORS_BEFORE_ESCALATION) {
        return true;
    }
    
    return false;
}

ErrorHandler::~ErrorHandler() {
    shutdown();
}

} // namespace RawrXD