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
 * @class SemanticDiffAnalyzer
 * @brief Production-ready AI-powered semantic diff analysis
 * 
 * Features:
 * - Semantic diff analysis beyond line-by-line comparison
 * - Breaking change detection with AI understanding
 * - Impact analysis across codebase
 * - Diff context enrichment with AI insights
 * - Structured logging with performance metrics
 * - Configuration-driven analysis strategies
 * - GDPR-compliant data handling
 */
class SemanticDiffAnalyzer : public QObject {
    Q_OBJECT

public:
    explicit SemanticDiffAnalyzer(QObject* parent = nullptr);
    ~SemanticDiffAnalyzer() override;

    // Configuration
    struct Config {
        QString aiEndpoint;
        QString apiKey;
        bool enableBreakingChangeDetection = true;
        bool enableImpactAnalysis = true;
        int maxDiffSize = 50000;
        bool enableMetrics = true;
        bool enableCaching = true;
        QString cacheDirectory;
    };

    void setConfig(const Config& config);
    Config getConfig() const;

    // Diff analysis structures
    struct SemanticChange {
        QString type;              // "function_modified", "class_added", "signature_changed", etc.
        QString name;              // Name of the changed entity
        QString description;       // AI-generated description
        QString file;              // File path
        int startLine;             // Start line in diff
        int endLine;               // End line in diff
        bool isBreaking;           // Whether this is a breaking change
        double impactScore;        // 0.0-1.0 impact severity
        QVector<QString> affectedFiles;  // Files potentially affected
    };

    struct DiffAnalysis {
        QVector<SemanticChange> changes;
        QString summary;           // AI-generated summary
        int breakingChangeCount;
        double overallImpactScore;
        QJsonObject metadata;      // Additional analysis data
    };

    // Core functionality
    DiffAnalysis analyzeDiff(const QString& diff);
    DiffAnalysis compareFiles(const QString& oldContent, const QString& newContent, const QString& filePath);
    QVector<QString> detectBreakingChanges(const DiffAnalysis& analysis);
    QJsonObject analyzeImpact(const SemanticChange& change);
    QString enrichDiffContext(const QString& diff);

    // Metrics
    struct Metrics {
        qint64 diffsAnalyzed = 0;
        qint64 semanticChangesDetected = 0;
        qint64 breakingChangesDetected = 0;
        qint64 impactAnalysesPerformed = 0;
        qint64 cacheHits = 0;
        qint64 cacheMisses = 0;
        qint64 errorCount = 0;
        double avgAnalysisLatencyMs = 0.0;
        double avgImpactScore = 0.0;
    };

    Metrics getMetrics() const;
    void resetMetrics();
    void clearCache();

signals:
    void analysisCompleted(const DiffAnalysis& analysis);
    void breakingChangeDetected(const SemanticChange& change);
    void highImpactChangeDetected(const SemanticChange& change);
    void errorOccurred(const QString& error);
    void metricsUpdated(const Metrics& metrics);

private:
    // Configuration
    Config m_config;
    mutable QMutex m_configMutex;

    // Metrics
    Metrics m_metrics;
    mutable QMutex m_metricsMutex;

    // Cache
    QMap<QString, DiffAnalysis> m_analysisCache;
    mutable QMutex m_cacheMutex;

    // Helper methods
    void logStructured(const QString& level, const QString& message, const QJsonObject& context = QJsonObject());
    void recordLatency(const QString& operation, const std::chrono::milliseconds& duration);
    QJsonObject makeAiRequest(const QString& endpoint, const QJsonObject& payload);
    QString calculateDiffHash(const QString& diff);
    DiffAnalysis getCachedAnalysis(const QString& diffHash);
    void cacheAnalysis(const QString& diffHash, const DiffAnalysis& analysis);
    bool validateDiff(const QString& diff);
};
