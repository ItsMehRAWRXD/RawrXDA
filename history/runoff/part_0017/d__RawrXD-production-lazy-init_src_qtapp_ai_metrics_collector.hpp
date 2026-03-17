#ifndef AI_METRICS_COLLECTOR_HPP
#define AI_METRICS_COLLECTOR_HPP

#include <QObject>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <QVector>
#include <QString>

/**
 * Metrics collector for AI operations
 * Collects performance metrics, latency, throughput, and error rates
 * Following production guidelines for observability
 */
class AIMetricsCollector : public QObject {
    Q_OBJECT

public:
    struct OperationMetric {
        QString operationType;      // "digestion", "training", "validation"
        QString operationName;      // Specific operation identifier
        QDateTime startTime;
        QDateTime endTime;
        qint64 latencyMs;
        bool success;
        QString errorMessage;
        QJsonObject additionalData;
    };

    struct SessionMetrics {
        QString sessionId;
        QString sessionType;        // "digestion" or "training"
        QDateTime startTime;
        QDateTime endTime;
        qint64 totalDurationMs;
        int totalOperations;
        int successfulOperations;
        int failedOperations;
        double successRate;
        double averageLatencyMs;
        double minLatencyMs;
        double maxLatencyMs;
        QJsonObject statistics;
    };

    explicit AIMetricsCollector(QObject* parent = nullptr);
    ~AIMetricsCollector();

    // Session management
    QString startSession(const QString& sessionType);
    void endSession(const QString& sessionId);
    SessionMetrics getSessionMetrics(const QString& sessionId) const;
    
    // Operation tracking
    void recordOperationStart(const QString& sessionId, const QString& operationType, const QString& operationName);
    void recordOperationEnd(const QString& sessionId, const QString& operationName, bool success, const QString& errorMessage = QString());
    void recordOperationMetric(const QString& sessionId, const QString& key, double value);
    
    // Performance metrics
    void recordLatency(const QString& operationType, qint64 latencyMs);
    void recordThroughput(const QString& operationType, int itemsProcessed, qint64 durationMs);
    void recordError(const QString& operationType, const QString& errorType);
    
    // Reporting
    QJsonObject getMetricsReport() const;
    QJsonObject getSessionReport(const QString& sessionId) const;
    QJsonArray getAllOperations(const QString& sessionId) const;
    
    // Statistics
    double getAverageLatency(const QString& operationType) const;
    double getSuccessRate(const QString& operationType) const;
    int getTotalOperations(const QString& operationType) const;
    int getErrorCount(const QString& operationType) const;
    
    // Export
    bool exportMetrics(const QString& filePath) const;
    void clearMetrics();

signals:
    void metricsUpdated(const QString& sessionId);
    void operationCompleted(const QString& sessionId, const OperationMetric& metric);
    void sessionCompleted(const QString& sessionId, const SessionMetrics& metrics);

private:
    struct SessionData {
        SessionMetrics metrics;
        QVector<OperationMetric> operations;
        QMap<QString, qint64> operationStartTimes;
        QJsonObject customMetrics;
    };

    void calculateSessionStatistics(SessionData& session);
    QString generateSessionId() const;

    mutable QMutex m_mutex;
    QMap<QString, SessionData> m_sessions;
    QMap<QString, QVector<qint64>> m_latencyHistory;
    QMap<QString, int> m_errorCounts;
    QMap<QString, int> m_operationCounts;
    QString m_currentSessionId;
};

#endif // AI_METRICS_COLLECTOR_HPP
