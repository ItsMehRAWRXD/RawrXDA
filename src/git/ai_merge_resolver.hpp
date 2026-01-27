#pragma once

#include <QString>
#include <QJsonObject>
#include <QObject>
#include <QMutex>
#include <QVector>
#include <QPair>
#include <chrono>
#include <memory>

/**
 * @class AIMergeResolver
 * @brief Production-ready AI-powered merge conflict resolution
 * 
 * Features:
 * - Three-way merge conflict resolution using AI analysis
 * - Semantic merge analysis with context understanding
 * - Automated conflict detection and resolution suggestions
 * - Manual override capability with audit trails
 * - Structured logging with performance metrics
 * - Configuration-driven resolution strategies
 * - GDPR-compliant data handling
 */
class AIMergeResolver : public QObject {
    Q_OBJECT

public:
    explicit AIMergeResolver(QObject* parent = nullptr);
    ~AIMergeResolver() override;

    // Configuration
    struct Config {
        QString aiEndpoint;
        QString apiKey;
        bool enableAutoResolve = false;
        int maxConflictSize = 10000;
        double minConfidenceThreshold = 0.75;
        bool enableMetrics = true;
        bool enableAuditLog = true;
        QString auditLogPath;
    };

    void setConfig(const Config& config);
    Config getConfig() const;

    // Conflict representation
    struct ConflictBlock {
        QString file;
        int startLine;
        int endLine;
        QString baseVersion;
        QString currentVersion;
        QString incomingVersion;
        QString context;
    };

    struct Resolution {
        QString resolvedContent;
        double confidence;
        QString strategy;
        QString explanation;
        bool requiresManualReview;
    };

    // Core functionality
    QVector<ConflictBlock> detectConflicts(const QString& filePath);
    Resolution resolveConflict(const ConflictBlock& conflict);
    bool applyResolution(const QString& filePath, const Resolution& resolution, int lineStart, int lineEnd);
    
    QJsonObject analyzeSemanticMerge(const QString& base, const QString& current, const QString& incoming);
    QVector<QString> detectBreakingChanges(const QString& diff);

    // Metrics
    struct Metrics {
        qint64 conflictsDetected = 0;
        qint64 conflictsResolved = 0;
        qint64 autoResolved = 0;
        qint64 manualResolved = 0;
        qint64 breakingChangesDetected = 0;
        qint64 errorCount = 0;
        double avgResolutionConfidence = 0.0;
        double avgResolutionLatencyMs = 0.0;
    };

    Metrics getMetrics() const;
    void resetMetrics();

signals:
    void conflictsDetected(int count);
    void conflictResolved(const Resolution& resolution);
    void breakingChangeDetected(const QString& change);
    void errorOccurred(const QString& error);
    void metricsUpdated(const Metrics& metrics);

private:
    // Configuration
    Config m_config;
    mutable QMutex m_configMutex;

    // Metrics
    Metrics m_metrics;
    mutable QMutex m_metricsMutex;

    // Helper methods
    void logStructured(const QString& level, const QString& message, const QJsonObject& context = QJsonObject());
    void recordLatency(const QString& operation, const std::chrono::milliseconds& duration);
    void logAudit(const QString& action, const QJsonObject& details);
    QJsonObject makeAiRequest(const QString& endpoint, const QJsonObject& payload);
    QString extractConflictMarkers(const QString& content, ConflictBlock& conflict);
    bool validateResolution(const Resolution& resolution, const ConflictBlock& conflict);
};
