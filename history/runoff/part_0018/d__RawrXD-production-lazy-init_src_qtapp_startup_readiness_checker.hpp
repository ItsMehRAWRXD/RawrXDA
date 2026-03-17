/**
 * @file startup_readiness_checker.hpp
 * @brief Production-ready startup validation system for autonomous agent IDE
 * @author RawrXD Team
 * @date 2026-01-08
 * @version 1.0.0
 *
 * Validates all critical subsystems before allowing agents to start:
 * - LLM endpoint connectivity (Ollama, Claude, OpenAI)
 * - GGUF server port availability and responsiveness
 * - Hotpatch manager initialization state
 * - Project root accessibility
 * - Required environment variables
 * - Network connectivity
 * - Disk space for model cache
 * 
 * Production Features:
 * - Retry logic with exponential backoff
 * - Concurrent health checks for speed
 * - Detailed diagnostic messages
 * - Visual progress feedback
 * - Non-blocking UI updates
 * - Comprehensive logging for troubleshooting
 */

#pragma once

#include <QDialog>
#include <QObject>
#include <QString>
#include <QMap>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProgressBar>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QElapsedTimer>
#include <QFuture>
#include <QtConcurrent>
#include <functional>

/**
 * @struct HealthCheckResult
 * @brief Result of a single health check operation
 */
struct HealthCheckResult {
    bool success = false;
    QString subsystem;                      ///< Name of subsystem checked
    QString message;                        ///< User-friendly status message
    QString technicalDetails;               ///< Technical details for debugging
    int latencyMs = -1;                     ///< Response time in milliseconds
    QDateTime timestamp;                    ///< When check was performed
    int attemptCount = 0;                   ///< Number of retry attempts
};

/**
 * @struct AgentReadinessReport
 * @brief Comprehensive readiness report for all subsystems
 */
struct AgentReadinessReport {
    bool overallReady = false;
    QMap<QString, HealthCheckResult> checks;
    QStringList failures;                   ///< List of failed subsystems
    QStringList warnings;                   ///< Non-critical warnings
    int totalLatency = 0;                   ///< Total validation time (ms)
    QDateTime startTime;
    QDateTime endTime;
};

/**
 * @class StartupReadinessChecker
 * @brief Validates IDE readiness for autonomous agent operations
 * 
 * Performs comprehensive pre-flight checks before allowing agents to start.
 * Displays real-time progress in a modal dialog with retry capabilities.
 * 
 * @example
 * @code
 * StartupReadinessChecker checker(mainWindow);
 * connect(&checker, &StartupReadinessChecker::readinessComplete,
 *         this, [](const AgentReadinessReport& report) {
 *     if (report.overallReady) {
 *         // Start agents
 *     } else {
 *         // Show configuration dialog
 *     }
 * });
 * checker.runChecks();
 * @endcode
 */
class StartupReadinessChecker : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Parent QObject for memory management
     */
    explicit StartupReadinessChecker(QObject* parent = nullptr);

    /**
     * @brief Destructor
     */
    ~StartupReadinessChecker();

    /**
     * @brief Run all health checks asynchronously
     * 
     * Starts concurrent health checks for all subsystems.
     * Emits readinessComplete when all checks finish.
     */
    void runChecks();

    /**
     * @brief Get last readiness report
     * @return Most recent AgentReadinessReport
     */
    AgentReadinessReport getLastReport() const { return m_lastReport; }

    /**
     * @brief Set maximum retry attempts per check
     * @param maxRetries Number of retries (default: 3)
     */
    void setMaxRetries(int maxRetries) { m_maxRetries = maxRetries; }

    /**
     * @brief Set base backoff between retries
     * @param backoffMs Backoff in milliseconds (applied linearly per attempt)
     */
    void setBackoffMs(int backoffMs) { m_backoffMs = backoffMs; }

    /**
     * @brief Set timeout for individual checks
     * @param timeoutMs Timeout in milliseconds (default: 5000)
     */
    void setTimeout(int timeoutMs) { m_timeoutMs = timeoutMs; }

signals:
    /**
     * @brief Emitted when all checks complete
     * @param report Comprehensive readiness report
     */
    void readinessComplete(const AgentReadinessReport& report);

    /**
     * @brief Emitted for individual check progress
     * @param subsystem Name of subsystem being checked
     * @param progress Progress percentage (0-100)
     * @param message Status message
     */
    void checkProgress(const QString& subsystem, int progress, const QString& message);

private slots:
    void onNetworkReplyFinished(QNetworkReply* reply);
    void onCheckTimeout();

private:
    // Health check implementations
    HealthCheckResult checkLLMEndpoint(const QString& backend, const QString& endpoint, const QString& apiKey);
    HealthCheckResult checkGGUFServer(quint16 port);
    HealthCheckResult checkHotpatchManager(class UnifiedHotpatchManager* manager);
    HealthCheckResult checkProjectRoot(const QString& projectPath);
    HealthCheckResult checkEnvironmentVariables();
    HealthCheckResult checkNetworkConnectivity();
    HealthCheckResult checkDiskSpace();
    HealthCheckResult checkModelCache();

    // Utility methods
    bool pingEndpoint(const QString& url, int& latencyMs);
    bool testTCPPort(const QString& host, quint16 port, int& latencyMs);
    qint64 getAvailableDiskSpace(const QString& path);
    QString formatBytes(qint64 bytes);
    void retryCheck(const QString& subsystem, std::function<HealthCheckResult()> checkFunc);
    HealthCheckResult runWithRetry(const QString& subsystem, std::function<HealthCheckResult()> checkFunc);
    void logReadinessMetrics(const AgentReadinessReport& report);

    // Members
    QNetworkAccessManager* m_networkManager;
    AgentReadinessReport m_lastReport;
    int m_maxRetries;
    int m_timeoutMs;
    int m_backoffMs;
    QElapsedTimer m_totalTimer;
    QMap<QString, QTimer*> m_checkTimers;
    QMap<QString, int> m_retryCount;
    QMap<QNetworkReply*, QString> m_pendingReplies;
    
    // Concurrent check tracking
    QMap<QString, QFuture<HealthCheckResult>> m_runningChecks;
    int m_completedChecks;
    int m_totalChecks;
};

/**
 * @class StartupReadinessDialog
 * @brief Visual dialog showing startup health check progress
 * 
 * Displays real-time progress of all health checks with:
 * - Individual subsystem status indicators
 * - Progress bars for each check
 * - Detailed diagnostic logs
 * - Retry buttons for failed checks
 * - Overall readiness summary
 * 
 * @note Modal dialog - blocks user interaction until checks complete
 */
class StartupReadinessDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Parent widget
     */
    explicit StartupReadinessDialog(QWidget* parent = nullptr);

    /**
     * @brief Destructor
     */
    ~StartupReadinessDialog();

    /**
     * @brief Start health checks and show dialog
     * @param hotpatchManager Pointer to hotpatch manager to validate
     * @param projectRoot Default project root path to validate
     * @return true if all checks pass, false otherwise
     */
    bool runChecks(class UnifiedHotpatchManager* hotpatchManager, 
                   const QString& projectRoot);

    /**
     * @brief Get final readiness report
     * @return AgentReadinessReport with all check results
     */
    AgentReadinessReport getReport() const { return m_report; }

private slots:
    void onCheckProgress(const QString& subsystem, int progress, const QString& message);
    void onReadinessComplete(const AgentReadinessReport& report);
    void onRetryClicked();
    void onSkipClicked();
    void onConfigureClicked();

private:
    void setupUI();
    void updateSubsystemStatus(const QString& subsystem, bool success, const QString& message);
    void showFinalSummary();

    // UI Components
    QVBoxLayout* m_mainLayout;
    QGroupBox* m_checksGroup;
    QVBoxLayout* m_checksLayout;
    QMap<QString, QLabel*> m_statusLabels;
    QMap<QString, QProgressBar*> m_progressBars;
    QTextEdit* m_diagnosticsLog;
    QLabel* m_summaryLabel;
    QPushButton* m_retryButton;
    QPushButton* m_skipButton;
    QPushButton* m_configureButton;
    QPushButton* m_continueButton;
    QProgressBar* m_overallProgress;

    // Data
    StartupReadinessChecker* m_checker;
    AgentReadinessReport m_report;
    class UnifiedHotpatchManager* m_hotpatchManager;
    QString m_projectRoot;
    bool m_checksPassed;
};

