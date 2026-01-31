#pragma once


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
class HealthCheckServer : public void {

public:
    explicit HealthCheckServer(InferenceEngine* engine, void* parent = nullptr);
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

    void requestReceived(const std::string& method, const std::string& path);
    void requestCompleted(const std::string& method, const std::string& path, int statusCode, double latency_ms);
    void errorOccurred(const std::string& error);

private:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    InferenceEngine* m_engine = nullptr;
    std::unique_ptr<void*> m_server;
    Metrics m_metrics;
    std::vector<double> m_latency_samples; // For percentile calculation
    
    // HTTP response helpers
    std::string createHealthJson();
    std::string createReadyJson();
    std::string createMetricsJson();
    std::string createPrometheusMetrics();
    std::string createModelJson();
    std::string createGPUJson();
    
    std::string buildHttpResponse(int statusCode, const std::string& contentType, 
                            const std::string& body);
    
    std::string extractPath(const std::string& request);
    std::string extractMethod(const std::string& request);
    std::string generateRequestId();
    
    // Production observability
    void logRequest(const std::string& requestId, const std::string& method, const std::string& path);
    void logResponse(const std::string& requestId, int statusCode, double latency_ms);
    void updateMetrics(bool success, double latency_ms);
    void calculatePercentiles();
};

