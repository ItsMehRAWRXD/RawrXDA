/**
 * @file telemetry_hooks.hpp
 * @brief Prometheus/OpenTelemetry metrics instrumentation for production observability
 *
 * Provides standardized metrics collection for:
 * - LLM invocation latency and success rates
 * - Circuit breaker state transitions
 * - Startup readiness check performance
 * - GGUF server request metrics
 * - Hotpatch operation tracking
 *
 * Supports export to Prometheus format for grafana/prometheus integration.
 */

#pragma once

#include <QString>
#include <QMap>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>
#include <atomic>
#include <chrono>

namespace RawrXD {

/**
 * @brief Metrics collector for Prometheus-style exposition
 *
 * Thread-safe metrics aggregation with support for:
 * - Counter: monotonic increasing values
 * - Gauge: values that can go up or down
 * - Histogram: distribution of measurements
 * - Summary: quantile-based distributions
 */
class MetricsCollector {
public:
    /**
     * @brief Increment a counter metric
     * @param name Metric name (e.g., "llm_requests_total")
     * @param value Amount to increment (default 1)
     * @param labels Optional labels as map (e.g., {{"backend", "ollama"}})
     */
    static void recordCounter(const QString& name, double value = 1.0,
                             const QMap<QString, QString>& labels = {});

    /**
     * @brief Record a gauge value (instant reading)
     * @param name Metric name (e.g., "circuit_breaker_failures")
     * @param value Current value
     * @param labels Optional labels
     */
    static void recordGauge(const QString& name, double value,
                           const QMap<QString, QString>& labels = {});

    /**
     * @brief Record a histogram observation (latency, size, etc.)
     * @param name Metric name (e.g., "llm_request_latency_ms")
     * @param value Measurement to histogram
     * @param labels Optional labels
     * @param buckets Custom bucket boundaries (optional)
     */
    static void recordHistogram(const QString& name, double value,
                               const QMap<QString, QString>& labels = {},
                               const QVector<double>& buckets = {});

    /**
     * @brief Export all metrics in Prometheus text format
     * @return Prometheus exposition format (text/plain; version=0.0.4)
     */
    static QString exportPrometheus();

    /**
     * @brief Export metrics as JSON for alternative destinations
     * @return JSON object with structured metrics
     */
    static QJsonObject exportJSON();

    /**
     * @brief Clear all metrics (for testing)
     */
    static void clear();

    /**
     * @brief Get single metric value
     * @param name Metric name
     * @return Value or 0 if not found
     */
    static double getValue(const QString& name);

private:
    struct MetricPoint {
        QString name;
        QString type;  // counter, gauge, histogram, summary
        double value = 0.0;
        QMap<QString, QString> labels;
        QVector<double> histogram_buckets;  // For histogram data
        QDateTime timestamp;
    };

    static QMap<QString, MetricPoint> s_metrics;
    static std::mutex s_metricsMutex;
};

/**
 * @brief Latency recorder for automatic timing measurements
 *
 * RAII-style timer that automatically records latency on destruction.
 * Useful for function-level instrumentation.
 *
 * @example
 * {
 *     LatencyRecorder timer("llm_inference_latency_ms", {{"model", "mistral"}});
 *     // ... do work ...
 *     // Latency recorded on scope exit
 * }
 */
class LatencyRecorder {
public:
    /**
     * @brief Constructor - starts timer
     * @param metricName Name of metric to record (will add "_ms" suffix if needed)
     * @param labels Optional Prometheus labels
     */
    LatencyRecorder(const QString& metricName,
                   const QMap<QString, QString>& labels = {});

    /**
     * @brief Destructor - records elapsed time as histogram
     */
    ~LatencyRecorder();

    /**
     * @brief Explicitly stop timer and record (optional, else records on destruction)
     * @return Elapsed milliseconds
     */
    qint64 stop();

private:
    QString m_metricName;
    QMap<QString, QString> m_labels;
    std::chrono::high_resolution_clock::time_point m_start;
    bool m_recorded = false;
};

/**
 * @brief LLM Invocation metrics tracker
 *
 * Tracks per-backend LLM request metrics:
 * - Request count (total and per-backend)
 * - Success/failure rates
 * - Latency distribution
 * - Token usage
 * - Cache hit rates
 */
class LLMMetrics {
public:
    struct Request {
        QString backend;           // ollama, claude, openai
        qint64 latencyMs = 0;
        int tokensUsed = 0;
        bool success = false;
        QString errorCode;         // For failures
        int retryAttempts = 1;
        bool cacheHit = false;
        QDateTime timestamp = QDateTime::currentDateTime();
    };

    /**
     * @brief Record LLM request completion
     * @param request Request metrics
     */
    static void recordRequest(const Request& request);

    /**
     * @brief Get request statistics
     * @return JSON object with counts, rates, latencies
     */
    static QJsonObject getStatistics();

    /**
     * @brief Get per-backend statistics
     * @param backend Backend name
     * @return JSON object with backend-specific metrics
     */
    static QJsonObject getBackendStats(const QString& backend);

    /**
     * @brief Reset metrics
     */
    static void reset();

private:
    struct Stats {
        qint64 totalRequests = 0;
        qint64 successfulRequests = 0;
        qint64 failedRequests = 0;
        qint64 cachedResponses = 0;
        qint64 totalLatencyMs = 0;
        qint64 totalTokens = 0;
        double p50LatencyMs = 0;
        double p95LatencyMs = 0;
        double p99LatencyMs = 0;
    };

    static QMap<QString, Stats> s_backendStats;
    static QVector<qint64> s_allLatencies;
    static std::mutex s_statsMutex;

    static void updatePercentiles();
};

/**
 * @brief Circuit breaker event metrics
 *
 * Tracks circuit breaker state transitions and failover events
 */
class CircuitBreakerMetrics {
public:
    struct Event {
        QString backend;
        QString eventType;  // trip, reset, failover, success
        int failureCount = 0;
        qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    };

    /**
     * @brief Record circuit breaker event
     * @param event Event details
     */
    static void recordEvent(const Event& event);

    /**
     * @brief Get circuit breaker statistics
     * @return JSON with trip counts, uptime per backend
     */
    static QJsonObject getStatistics();

private:
    static QVector<Event> s_events;
    static std::mutex s_eventsMutex;
};

/**
 * @brief GGUF Server request metrics
 *
 * Tracks inference engine and GGUF server performance
 */
class GGUFMetrics {
public:
    struct InferenceRequest {
        QString model;
        int promptTokens = 0;
        int generatedTokens = 0;
        qint64 latencyMs = 0;
        float tokensPerSecond = 0.0;
        bool success = false;
        QString errorType;  // oom, timeout, device_error, etc.
        QDateTime timestamp = QDateTime::currentDateTime();
    };

    /**
     * @brief Record inference request
     * @param request Request metrics
     */
    static void recordInference(const InferenceRequest& request);

    /**
     * @brief Get GGUF server statistics
     * @return JSON with throughput, latency, model stats
     */
    static QJsonObject getStatistics();

    /**
     * @brief Get per-model statistics
     * @param model Model name
     * @return JSON with model-specific metrics
     */
    static QJsonObject getModelStats(const QString& model);

private:
    struct ModelStats {
        qint64 totalRequests = 0;
        qint64 successfulRequests = 0;
        qint64 totalPromptTokens = 0;
        qint64 totalGeneratedTokens = 0;
        qint64 totalLatencyMs = 0;
        float averageTokensPerSecond = 0.0;
        double p50LatencyMs = 0;
        double p95LatencyMs = 0;
    };

    static QMap<QString, ModelStats> s_modelStats;
    static std::mutex s_statsMutex;
};

/**
 * @brief Hotpatch operation metrics
 *
 * Tracks hotpatch application success rates and latency
 */
class HotpatchMetrics {
public:
    struct Operation {
        QString operationType;  // patch_apply, model_load, prompt_inject, etc.
        bool success = false;
        qint64 latencyMs = 0;
        QString errorDetail;
        int bytesModified = 0;
        QDateTime timestamp = QDateTime::currentDateTime();
    };

    /**
     * @brief Record hotpatch operation
     * @param op Operation details
     */
    static void recordOperation(const Operation& op);

    /**
     * @brief Get hotpatch statistics
     * @return JSON with success rates and latencies
     */
    static QJsonObject getStatistics();

private:
    struct Stats {
        qint64 totalOperations = 0;
        qint64 successfulOperations = 0;
        qint64 totalLatencyMs = 0;
        qint64 totalBytesModified = 0;
    };

    static QMap<QString, Stats> s_opStats;
    static std::mutex s_statsMutex;
};

/**
 * @brief Startup readiness metrics aggregator
 *
 * Supplements startup_readiness_checker.cpp with structured metrics
 */
class StartupMetrics {
public:
    struct CheckResult {
        QString checkName;
        bool success = false;
        qint64 latencyMs = 0;
        QString status;  // ready, degraded, failed
        int attempts = 1;
    };

    /**
     * @brief Record startup check result
     * @param result Check result
     */
    static void recordCheck(const CheckResult& result);

    /**
     * @brief Get startup timeline and metrics
     * @return JSON with all check results and total latency
     */
    static QJsonObject getStatistics();

private:
    static QVector<CheckResult> s_checks;
    static std::mutex s_checksMutex;
};

/**
 * @brief Global telemetry initialization
 *
 * Call once at application startup to enable all metrics collection
 */
void initializeTelemetry();

/**
 * @brief Export all collected metrics
 * @param format "prometheus" or "json"
 * @return Formatted metrics string/JSON
 */
QString exportMetrics(const QString& format = "prometheus");

/**
 * @brief Export metrics to file
 * @param filePath Path to write metrics (e.g., "/metrics.txt")
 * @param format "prometheus" or "json"
 * @return true if write successful
 */
bool exportMetricsToFile(const QString& filePath, const QString& format = "prometheus");

}  // namespace RawrXD
