#pragma once

// <QObject> removed (Qt-free build)
// Qt include removed (Qt-free build)
// Qt include removed (Qt-free build)
// <QString> removed (Qt-free build)
// Qt include removed (Qt-free build)
#include <memory>
#include <chrono>

class InferenceEngine;

/**
 * @brief Production-Ready HTTP REST API server for monitoring and observability
 * 
 * Provides comprehensive health and metrics endpoints for production deployment:
 * - GET /health: Server health status with detailed metrics
 * - GET /ready: Kubernetes-style readiness probe
 * - GET /metrics: Prometheus-compatible performance metrics
 * - POST /infer: Synchronous inference endpoint
 * - POST /infer/async: Asynchronous inference queue
 * - GET /model: Current model information
 * - GET /gpu: GPU memory usage and status
 * 
 * Production Features:
 * - Structured JSON logging with request IDs
 * - Latency tracking (P50, P95, P99)
 * - Error rate monitoring
 * - Resource utilization metrics
 * - Configurable via PRODUCTION_CONFIGURATION_GUIDE.md
 */
class HealthCheckServer  {
    /* Q_OBJECT */

public:
    explicit HealthCheckServer(InferenceEngine* engine, QObject* parent = nullptr);
    ~HealthCheckServer();

    bool startServer(quint16 port = 8888);  // Default from production config
    void stopServer();
    bool isRunning() const { return m_server != nullptr; }
    
    // Production metrics
    struct Metrics {
        quint64 total_requests = 0;
        quint64 successful_requests = 0;
        quint64 failed_requests = 0;
        double avg_response_time_ms = 0.0;
        double p95_response_time_ms = 0.0;
        double p99_response_time_ms = 0.0;
        std::chrono::system_clock::time_point last_request_time;
    };
    
    Metrics getMetrics() const { return m_metrics; }

signals:
    void requestReceived(const QString& method, const QString& path);
    void requestCompleted(const QString& method, const QString& path, int statusCode, double latency_ms);
    void errorOccurred(const QString& error);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    InferenceEngine* m_engine = nullptr;
    std::unique_ptr<QTcpServer> m_server;
    Metrics m_metrics;
    std::vector<double> m_latency_samples; // For percentile calculation
    
    // HTTP response helpers
    QString createHealthJson();
    QString createReadyJson();
    QString createMetricsJson();
    QString createPrometheusMetrics();
    QString createModelJson();
    QString createGPUJson();
    
    QString buildHttpResponse(int statusCode, const QString& contentType, 
                            const QString& body);
    
    QString extractPath(const QString& request);
    QString extractMethod(const QString& request);
    QString generateRequestId();
    
    // Production observability
    void logRequest(const QString& requestId, const QString& method, const QString& path);
    void logResponse(const QString& requestId, int statusCode, double latency_ms);
    void updateMetrics(bool success, double latency_ms);
    void calculatePercentiles();
};
