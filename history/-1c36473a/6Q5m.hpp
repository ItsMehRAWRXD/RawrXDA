#ifndef METRICS_COLLECTOR_HPP
#define METRICS_COLLECTOR_HPP

#include <QString>
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QVector>
#include <QDateTime>
#include <QElapsedTimer>
#include <QVariant>
#include <QMutex>
#include <limits>

class MetricsCollector : public QObject
{
    Q_OBJECT

public:
    static MetricsCollector& instance();

    explicit MetricsCollector(QObject* parent = nullptr);
    ~MetricsCollector() override;

    void startRequest(qint64 requestId, const QString& modelName, int promptTokens);
    void endRequest(qint64 requestId, int tokensGenerated, bool success, const QString& error = QString());
    void recordToken(qint64 requestId);
    void recordEvent(const QString& eventName, const QMap<QString, QVariant>& properties = {});
    void recordMemoryUsage(size_t bytes);

    struct RequestMetrics {
        qint64 requestId = 0;
        QDateTime startTime;
        QDateTime endTime;
        QString modelName;
        int promptTokens = 0;
        int tokensGenerated = 0;
        bool success = false;
        QString errorMessage;
        size_t memoryUsed = 0;
        qint64 durationMs = 0;
        float tokensPerSecond = 0.0f;
    };

    struct AggregateMetrics {
        int totalRequests = 0;
        int successfulRequests = 0;
        int failedRequests = 0;
        QDateTime firstRequest;
        QDateTime lastRequest;
        qint64 minLatencyMs = std::numeric_limits<qint64>::max();
        qint64 maxLatencyMs = 0;
        qint64 avgLatencyMs = 0;
        qint64 p50LatencyMs = 0;
        qint64 p95LatencyMs = 0;
        qint64 p99LatencyMs = 0;
        float minTokensPerSec = std::numeric_limits<float>::max();
        float maxTokensPerSec = 0.0f;
        float avgTokensPerSec = 0.0f;
        size_t peakMemoryUsage = 0;
        size_t avgMemoryUsage = 0;
    };

    RequestMetrics getRequestMetrics(qint64 requestId) const;
    AggregateMetrics getAggregateMetrics() const;

    QString exportToJson() const;
    QString exportToCsv() const;

    void reset();
    void setEnabled(bool enabled);
    bool isEnabled() const;
    void calculatePercentiles();

signals:
    void requestStarted(qint64 requestId);
    void requestCompleted(qint64 requestId, const RequestMetrics& metrics);
    void metricsUpdated();
    void performanceWarning(QString warning);

private:
    bool m_enabled = true;
    size_t m_currentMemoryUsage = 0;
    QMap<qint64, RequestMetrics> m_activeRequests;
    QMap<qint64, QElapsedTimer> m_timers;
    QVector<RequestMetrics> m_completedRequests;
    mutable QMutex m_mutex;
};

#endif // METRICS_COLLECTOR_HPP
