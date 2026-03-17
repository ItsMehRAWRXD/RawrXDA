#pragma once

namespace mem {

/**
 * @class Metrics
 * @brief Wrapper for telemetry integration
 * 
 * All metrics automatically forward to your TelemetryManager for Prometheus/Jaeger/Zipkin
 */
class Metrics {
public:
    /**
     * @brief Increment counter metric
     * @param name Metric name (e.g., "memory_user_facts_total")
     * @param value Increment amount (default 1)
     */
    static void increment(const char* name, int64_t value = 1);

    /**
     * @brief Record histogram/gauge value
     * @param name Metric name (e.g., "memory_context_tokens")
     * @param value Numeric value
     */
    static void record(const char* name, double value);

    /**
     * @brief Record latency in milliseconds
     * @param name Metric name (e.g., "memory_encryption_ms")
     * @param ms Milliseconds
     */
    static void recordLatency(const char* name, int64_t ms);

    /**
     * @brief Record bytes
     * @param name Metric name (e.g., "memory_usage_bytes")
     * @param bytes Number of bytes
     */
    static void recordBytes(const char* name, int64_t bytes);

    /**
     * @brief Start timing for latency measurement
     * @return Timestamp (opaque, pass to recordLatency)
     */
    static int64_t startTimer();

    /**
     * @brief Get elapsed milliseconds since startTimer()
     */
    static int64_t elapsedMs(int64_t startTime);
};

} // namespace mem
