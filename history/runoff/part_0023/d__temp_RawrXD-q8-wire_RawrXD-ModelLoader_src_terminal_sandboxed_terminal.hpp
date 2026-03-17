#pragma once

#include <QString>
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QMutex>
#include <QVector>
#include <QRegularExpression>
#include <chrono>
#include <memory>

/**
 * @class SandboxedTerminal
 * @brief Production-ready terminal with security isolation and command filtering
 * 
 * Features:
 * - Process isolation with resource limits
 * - Command filtering with whitelist/blacklist
 * - Output filtering and sanitization
 * - Security auditing with comprehensive logging
 * - Structured logging with metrics
 * - Configuration-driven security policies
 * - Timeout management
 * - Environment variable control
 */
class SandboxedTerminal : public QObject {
    Q_OBJECT

public:
    explicit SandboxedTerminal(QObject* parent = nullptr);
    ~SandboxedTerminal() override;

    // Configuration
    struct Config {
        QStringList commandWhitelist;        // Allowed commands
        QStringList commandBlacklist;        // Explicitly forbidden commands
        bool useWhitelistMode = true;        // If true, only whitelist allowed; if false, blacklist forbidden
        int maxExecutionTimeMs = 30000;      // Command timeout
        int maxOutputSize = 1048576;         // 1MB max output
        bool enableOutputFiltering = true;   // Filter sensitive data from output
        bool enableAuditLog = true;
        QString auditLogPath;
        QString workingDirectory;
        QStringList allowedEnvironmentVars;  // Environment variables to preserve
        bool enableResourceLimits = true;
        qint64 maxMemoryBytes = 536870912;   // 512MB
        int maxCpuPercent = 80;
        bool enableMetrics = true;
    };

    void setConfig(const Config& config);
    Config getConfig() const;

    // Command execution
    struct CommandResult {
        QString output;
        QString error;
        int exitCode;
        bool timedOut;
        bool wasBlocked;
        QString blockReason;
        qint64 executionTimeMs;
    };

    CommandResult executeCommand(const QString& command, const QStringList& args = QStringList());
    bool isCommandAllowed(const QString& command) const;
    QString sanitizeOutput(const QString& output) const;
    
    // Process management
    bool isRunning() const;
    void terminate();
    void kill();

    // Metrics
    struct Metrics {
        qint64 commandsExecuted = 0;
        qint64 commandsBlocked = 0;
        qint64 commandsTimedOut = 0;
        qint64 outputBytesFiltered = 0;
        qint64 securityViolations = 0;
        qint64 errorCount = 0;
        double avgExecutionTimeMs = 0.0;
    };

    Metrics getMetrics() const;
    void resetMetrics();

signals:
    void commandStarted(const QString& command);
    void commandFinished(const CommandResult& result);
    void commandBlocked(const QString& command, const QString& reason);
    void securityViolation(const QString& violation);
    void errorOccurred(const QString& error);
    void metricsUpdated(const Metrics& metrics);

private:
    // Configuration
    Config m_config;
    mutable QMutex m_configMutex;

    // Process
    QProcess* m_process;
    mutable QMutex m_processMutex;

    // Metrics
    Metrics m_metrics;
    mutable QMutex m_metricsMutex;

    // Helper methods
    void logStructured(const QString& level, const QString& message, const QJsonObject& context = QJsonObject());
    void recordLatency(const QString& operation, const std::chrono::milliseconds& duration);
    void logAudit(const QString& action, const QJsonObject& details);
    bool validateCommand(const QString& command, QString& blockReason) const;
    QStringList buildSanitizedEnvironment() const;
    bool enforceResourceLimits();
    QString filterSensitiveData(const QString& data) const;
};
