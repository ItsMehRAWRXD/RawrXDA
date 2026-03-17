#pragma once

#include <QString>
#include <QObject>
#include <QStringList>
#include <functional>
#include <memory>

/**
 * @file error_handler.h
 * @brief Global exception handling and error recovery
 * 
 * Provides:
 * - Central exception capture
 * - Error dialog display
 * - Recovery mechanisms
 * - Error logging
 * - User-friendly error messages
 */

class ErrorHandler : public QObject {
    Q_OBJECT

public:
    enum class ErrorLevel {
        Info,      // User information
        Warning,   // Something unusual happened
        Error,     // Operation failed, recovery possible
        Critical,  // System failure, recovery unlikely
        Fatal      // Complete failure, app should exit
    };

    enum class ErrorCategory {
        FileSystem,
        Model,
        Build,
        Debug,
        Execution,
        Network,
        Configuration,
        Memory,
        Unknown
    };

    struct ErrorInfo {
        QString message;
        QString details;
        QString stackTrace;
        ErrorLevel level = ErrorLevel::Error;
        ErrorCategory category = ErrorCategory::Unknown;
        int errorCode = 0;
        long long timestamp = 0;  // Unix timestamp
        QString sourceFile;
        int sourceLine = 0;
        QString sourceFunction;
    };

    struct RecoveryAction {
        QString name;
        std::function<bool()> action;
        bool isDefault = false;
    };

    static ErrorHandler& instance();

    /**
     * Handle exception (called from catch blocks)
     */
    void handleException(const std::exception& e,
                        ErrorCategory category = ErrorCategory::Unknown,
                        const QString& sourceFile = "",
                        int sourceLine = 0);

    /**
     * Handle error (called for non-exception errors)
     */
    void handleError(const QString& message,
                    ErrorLevel level = ErrorLevel::Error,
                    ErrorCategory category = ErrorCategory::Unknown);

    /**
     * Handle error with details
     */
    void handleErrorWithDetails(const QString& message,
                               const QString& details,
                               ErrorLevel level = ErrorLevel::Error,
                               ErrorCategory category = ErrorCategory::Unknown);

    /**
     * Show error dialog with recovery options
     */
    bool showErrorDialog(const ErrorInfo& error, const QList<RecoveryAction>& recoveryActions);

    /**
     * Show simple error dialog
     */
    void showErrorDialog(const QString& title, const QString& message, ErrorLevel level);

    /**
     * Show warning dialog
     */
    void showWarningDialog(const QString& title, const QString& message);

    /**
     * Show info dialog
     */
    void showInfoDialog(const QString& title, const QString& message);

    /**
     * Report error (convenience method)
     */
    void reportError(const QString& message, const QString& levelOrDetails = QString(), ErrorLevel level = ErrorLevel::Error);

    /**
     * Ask user to confirm action
     */
    bool askConfirmation(const QString& title, const QString& message, bool defaultYes = false);

    /**
     * Get last error
     */
    const ErrorInfo& getLastError() const;

    /**
     * Get error history
     */
    QList<ErrorInfo> getErrorHistory() const;
    QList<ErrorInfo> getErrorHistory(int count = 50) const;

    /**
     * Clear error history
     */
    void clearErrorHistory();

    /**
     * Register recovery action for error type
     */
    void registerRecoveryAction(ErrorCategory category, const RecoveryAction& action);

    /**
     * Get recovery actions for error type
     */
    QList<RecoveryAction> getRecoveryActions(ErrorCategory category) const;

    /**
     * Check if error is recoverable
     */
    bool isRecoverable(const ErrorInfo& error) const;

    /**
     * Get user-friendly error message
     */
    QString getUserFriendlyMessage(const ErrorInfo& error) const;

    /**
     * Get error level name
     */
    static QString getErrorLevelName(ErrorLevel level);

    /**
     * Get error category name
     */
    static QString getErrorCategoryName(ErrorCategory category);

    /**
     * Log error to file
     */
    void logErrorToFile(const ErrorInfo& error);

    /**
     * Get error log path
     */
    QString getErrorLogPath() const;

    /**
     * Check if app should exit after error
     */
    bool shouldExitAfterError(const ErrorInfo& error) const;

signals:
    /**
     * Error occurred
     */
    void errorOccurred(const ErrorInfo& error);

    /**
     * Error reported (convenience signal)
     */
    void errorReported(const QString& message);

    /**
     * Critical error reported
     */
    void criticalErrorReported(const QString& message);

    /**
     * Error dialog shown
     */
    void errorDialogShown(const ErrorInfo& error);

    /**
     * Recovery action taken
     */
    void recoveryActionTaken(const QString& actionName, bool success);

    /**
     * Error logged
     */
    void errorLogged(const QString& logFile);

private slots:
    void onApplicationException();

private:
    ErrorHandler();
    ~ErrorHandler() override;

    ErrorInfo m_lastError;
    QList<ErrorInfo> m_errorHistory;
    QMap<ErrorCategory, QList<RecoveryAction>> m_recoveryActions;

    static constexpr int MAX_HISTORY_SIZE = 100;
    static constexpr const char* ERROR_LOG_FILENAME = "rawrxd_errors.log";

    QString formatErrorForLog(const ErrorInfo& error) const;
    void initializeRecoveryActions();
};
