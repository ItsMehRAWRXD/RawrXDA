#pragma once
/**
 * TelemetryPersistence - Enhancement #8: Metrics and Diagnostics
 * 
 * Captures detailed persistence metrics and diagnostic data.
 * Enables performance analysis and troubleshooting.
 * 
 * Symbols: TM_LEVEL_OFF, TM_LEVEL_BASIC, TM_LEVEL_DETAILED, TM_LEVEL_DEBUG
 */

#include <string>
#include <vector>
#include <array>
#include <chrono>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <cstdint>
#include <nlohmann/json.hpp>

// Telemetry levels
#define TM_LEVEL_OFF            0
#define TM_LEVEL_BASIC          1
#define TM_LEVEL_DETAILED       2
#define TM_LEVEL_DEBUG          3

// Metric categories
#define TM_CAT_PERSISTENCE      0x01
#define TM_CAT_COMPRESSION      0x02
#define TM_CAT_ENCRYPTION       0x04
#define TM_CAT_REPLICATION      0x08
#define TM_CAT_MIGRATION        0x10
#define TM_CAT_ALL              0xFF

// Histogram buckets (in microseconds)
#define TM_BUCKET_100US         0
#define TM_BUCKET_1MS           1
#define TM_BUCKET_10MS          2
#define TM_BUCKET_100MS         3
#define TM_BUCKET_1S            4
#define TM_BUCKET_10S           5
#define TM_BUCKET_INF           6
#define TM_BUCKET_COUNT         7

namespace TelemetryPersistence {

    /**
     * Timing measurement
     */
    class Timer {
    public:
        Timer();
        ~Timer();

        void start();
        void stop();
        int64_t elapsedMicros() const;
        int64_t elapsedMillis() const;
        bool isRunning() const;

    private:
        std::chrono::steady_clock::time_point m_start;
        std::chrono::steady_clock::time_point m_end;
        bool m_running = false;
    };

    /**
     * Metric value
     */
    struct MetricValue {
        int64_t timestamp;
        double value;
        std::map<std::string, std::string> tags;
    };

    /**
     * Histogram for latency distribution
     */
    class Histogram {
    public:
        Histogram();
        ~Histogram();

        void record(int64_t value);
        void recordMicros(int64_t micros);
        void recordMillis(int64_t millis);

        // Get bucket counts
        std::vector<uint64_t> getBuckets() const;
        
        // Get statistics
        uint64_t getCount() const;
        double getMean() const;
        int64_t getMin() const;
        int64_t getMax() const;
        int64_t getPercentile(double p) const;

        void reset();

    private:
        std::vector<uint64_t> m_buckets = std::vector<uint64_t>(TM_BUCKET_COUNT, 0);
        uint64_t m_count = 0;
        double m_sum = 0;
        int64_t m_min = INT64_MAX;
        int64_t m_max = 0;
        mutable std::mutex m_mutex;
    };

    /**
     * Counter metric
     */
    class Counter {
    public:
        Counter();
        ~Counter();

        void increment(uint64_t delta = 1);
        void decrement(uint64_t delta = 1);
        uint64_t get() const;
        void reset();

    private:
        std::atomic<uint64_t> m_value{0};
    };

    /**
     * Gauge metric
     */
    class Gauge {
    public:
        Gauge();
        ~Gauge();

        void set(double value);
        void add(double delta);
        double get() const;
        void reset();

    private:
        std::atomic<double> m_value{0.0};
    };

    /**
     * Telemetry collector
     */
    class TelemetryCollector {
    public:
        TelemetryCollector();
        ~TelemetryCollector();

        // Initialize with level
        void initialize(int level = TM_LEVEL_BASIC);
        void shutdown();

        // Record metrics
        void recordPersistenceTime(int64_t micros, bool success);
        void recordCompressionTime(int64_t micros, size_t originalSize, size_t compressedSize);
        void recordEncryptionTime(int64_t micros);
        void recordReplicationTime(int64_t micros, bool success);
        void recordMigrationTime(int64_t micros, int fromVersion, int toVersion);

        // Increment counters
        void incrementPersistCount(bool success);
        void incrementLoadCount(bool success);
        void incrementCheckpointCount();
        void incrementRecoveryCount();
        void incrementConflictCount();

        // Set gauges
        void setQueueDepth(size_t depth);
        void setCacheSize(size_t bytes);
        void setActiveExecutions(size_t count);

        // Get telemetry data
        nlohmann::json getTelemetrySnapshot() const;
        nlohmann::json getHistogramData(const std::string& name) const;

        // Export to various formats
        std::string exportPrometheus() const;
        std::string exportStatsD() const;
        nlohmann::json exportJson() const;

        // Get current level
        int getLevel() const;

        // Enable/disable categories
        void enableCategory(uint32_t category);
        void disableCategory(uint32_t category);

        // Flush pending telemetry
        void flush();

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
        
        std::atomic<bool> m_running{false};
        std::thread m_workerThread;
    };

    /**
     * Diagnostic logger
     */
    class DiagnosticLogger {
    public:
        DiagnosticLogger();
        ~DiagnosticLogger();

        // Log diagnostic event
        void logEvent(
            const std::string& category,
            const std::string& event,
            const nlohmann::json& details);
        
        void logError(
            const std::string& category,
            const std::string& error,
            const std::string& context);
        
        void logWarning(
            const std::string& category,
            const std::string& warning);

        // Get recent events
        std::vector<nlohmann::json> getRecentEvents(size_t count = 100) const;
        
        // Get events by category
        std::vector<nlohmann::json> getEventsByCategory(
            const std::string& category,
            size_t count = 100) const;

        // Export diagnostic report
        nlohmann::json exportReport() const;

        // Clear history
        void clear();

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

    /**
     * Performance profiler
     */
    class PersistenceProfiler {
    public:
        PersistenceProfiler();
        ~PersistenceProfiler();

        // Begin profiled section
        void beginSection(const std::string& name);
        void endSection(const std::string& name);

        // Profile function call
        template<typename Func>
        auto profile(const std::string& name, Func&& func) -> decltype(func()) {
            beginSection(name);
            auto result = func();
            endSection(name);
            return result;
        }

        // Get profile data
        nlohmann::json getProfileData() const;
        void reset();

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

    /**
     * Global instances
     */
    TelemetryCollector& getGlobalCollector();
    DiagnosticLogger& getGlobalLogger();
    PersistenceProfiler& getGlobalProfiler();

    // RAII timer for automatic measurement
    class ScopedTimer {
    public:
        explicit ScopedTimer(const std::string& metricName);
        ~ScopedTimer();

    private:
        std::string m_metricName;
        Timer m_timer;
    };

} // namespace TelemetryPersistence

// Convenience macros
#define TM_TIMER(name) TelemetryPersistence::ScopedTimer _tm_timer(name)
#define TM_RECORD_TIME(name, micros) \
    TelemetryPersistence::getGlobalCollector().record##name##Time(micros, true)
#define TM_INCREMENT_COUNTER(name) \
    TelemetryPersistence::getGlobalCollector().increment##name##Count(true)
