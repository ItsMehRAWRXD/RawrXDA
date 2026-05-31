/**
 * TelemetryPersistence Implementation
 * Enhancement #8: Metrics and Diagnostics
 */

#include "telemetry_persistence.h"
#include <sstream>
#include <iomanip>
#include <deque>
#include <unordered_map>
#include <mutex>
#include <algorithm>

namespace TelemetryPersistence {

    // ===== Timer Implementation =====

    Timer::Timer() = default;
    Timer::~Timer() = default;

    void Timer::start() {
        m_start = std::chrono::steady_clock::now();
        m_running = true;
    }

    void Timer::stop() {
        m_end = std::chrono::steady_clock::now();
        m_running = false;
    }

    int64_t Timer::elapsedMicros() const {
        auto end = m_running ? std::chrono::steady_clock::now() : m_end;
        return std::chrono::duration_cast<std::chrono::microseconds>(
            end - m_start).count();
    }

    int64_t Timer::elapsedMillis() const {
        return elapsedMicros() / 1000;
    }

    bool Timer::isRunning() const {
        return m_running;
    }

    // ===== Histogram Implementation =====

    Histogram::Histogram() = default;
    Histogram::~Histogram() = default;

    void Histogram::record(int64_t value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        m_count++;
        m_sum += value;
        m_min = std::min(m_min, value);
        m_max = std::max(m_max, value);
        
        // Determine bucket
        int bucket = TM_BUCKET_INF;
        if (value < 100) bucket = TM_BUCKET_100US;
        else if (value < 1000) bucket = TM_BUCKET_1MS;
        else if (value < 10000) bucket = TM_BUCKET_10MS;
        else if (value < 100000) bucket = TM_BUCKET_100MS;
        else if (value < 1000000) bucket = TM_BUCKET_1S;
        else if (value < 10000000) bucket = TM_BUCKET_10S;
        
        m_buckets[bucket]++;
    }

    void Histogram::recordMicros(int64_t micros) {
        record(micros);
    }

    void Histogram::recordMillis(int64_t millis) {
        record(millis * 1000);
    }

    std::vector<uint64_t> Histogram::getBuckets() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_buckets;
    }

    uint64_t Histogram::getCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_count;
    }

    double Histogram::getMean() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_count > 0 ? m_sum / m_count : 0;
    }

    int64_t Histogram::getMin() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_min == INT64_MAX ? 0 : m_min;
    }

    int64_t Histogram::getMax() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_max;
    }

    int64_t Histogram::getPercentile(double p) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_count == 0) return 0;
        
        uint64_t target = static_cast<uint64_t>(m_count * p);
        uint64_t cumulative = 0;
        
        // Bucket boundaries in microseconds
        const int64_t boundaries[] = {100, 1000, 10000, 100000, 1000000, 10000000, INT64_MAX};
        
        for (int i = 0; i < TM_BUCKET_COUNT; i++) {
            cumulative += m_buckets[i];
            if (cumulative >= target) {
                return boundaries[i];
            }
        }
        
        return boundaries[TM_BUCKET_COUNT - 1];
    }

    void Histogram::reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::fill(m_buckets.begin(), m_buckets.end(), 0);
        m_count = 0;
        m_sum = 0;
        m_min = INT64_MAX;
        m_max = 0;
    }

    // ===== Counter Implementation =====

    Counter::Counter() = default;
    Counter::~Counter() = default;

    void Counter::increment(uint64_t delta) {
        m_value.fetch_add(delta, std::memory_order_relaxed);
    }

    void Counter::decrement(uint64_t delta) {
        m_value.fetch_sub(delta, std::memory_order_relaxed);
    }

    uint64_t Counter::get() const {
        return m_value.load(std::memory_order_relaxed);
    }

    void Counter::reset() {
        m_value.store(0, std::memory_order_relaxed);
    }

    // ===== Gauge Implementation =====

    Gauge::Gauge() = default;
    Gauge::~Gauge() = default;

    void Gauge::set(double value) {
        m_value.store(value, std::memory_order_relaxed);
    }

    void Gauge::add(double delta) {
        double current = m_value.load(std::memory_order_relaxed);
        m_value.store(current + delta, std::memory_order_relaxed);
    }

    double Gauge::get() const {
        return m_value.load(std::memory_order_relaxed);
    }

    void Gauge::reset() {
        m_value.store(0.0, std::memory_order_relaxed);
    }

    // ===== TelemetryCollector Implementation =====

    class TelemetryCollector::Impl {
    public:
        int level = TM_LEVEL_BASIC;
        uint32_t enabledCategories = TM_CAT_ALL;
        
        Histogram persistenceHistogram;
        Histogram compressionHistogram;
        Histogram encryptionHistogram;
        Histogram replicationHistogram;
        Histogram migrationHistogram;
        
        Counter persistCounter;
        Counter persistSuccessCounter;
        Counter loadCounter;
        Counter loadSuccessCounter;
        Counter checkpointCounter;
        Counter recoveryCounter;
        Counter conflictCounter;
        
        Gauge queueDepthGauge;
        Gauge cacheSizeGauge;
        Gauge activeExecutionsGauge;
        
        std::atomic<uint64_t> compressionBytesSaved{0};
    };

    TelemetryCollector::TelemetryCollector() 
        : m_impl(std::make_unique<Impl>()) {
    }

    TelemetryCollector::~TelemetryCollector() = default;

    void TelemetryCollector::initialize(int level) {
        m_impl->level = level;
    }

    void TelemetryCollector::shutdown() {
        // Cleanup: flush pending events and close connections
        fprintf(stderr, "[TelemetryCollector] Shutting down\n");
        flush();
        m_running = false;
        if (m_workerThread.joinable()) {
            m_workerThread.join();
        }
    }

    void TelemetryCollector::recordPersistenceTime(int64_t micros, bool success) {
        if (m_impl->level >= TM_LEVEL_BASIC) {
            m_impl->persistenceHistogram.record(micros);
        }
        incrementPersistCount(success);
    }

    void TelemetryCollector::recordCompressionTime(
        int64_t micros, size_t originalSize, size_t compressedSize) {
        
        if (m_impl->level >= TM_LEVEL_DETAILED) {
            m_impl->compressionHistogram.record(micros);
            m_impl->compressionBytesSaved += (originalSize - compressedSize);
        }
    }

    void TelemetryCollector::recordEncryptionTime(int64_t micros) {
        if (m_impl->level >= TM_LEVEL_DETAILED) {
            m_impl->encryptionHistogram.record(micros);
        }
    }

    void TelemetryCollector::recordReplicationTime(int64_t micros, bool success) {
        if (m_impl->level >= TM_LEVEL_DETAILED) {
            m_impl->replicationHistogram.record(micros);
        }
    }

    void TelemetryCollector::recordMigrationTime(int64_t micros, int fromVersion, int toVersion) {
        if (m_impl->level >= TM_LEVEL_BASIC) {
            m_impl->migrationHistogram.record(micros);
        }
    }

    void TelemetryCollector::incrementPersistCount(bool success) {
        m_impl->persistCounter.increment();
        if (success) {
            m_impl->persistSuccessCounter.increment();
        }
    }

    void TelemetryCollector::incrementLoadCount(bool success) {
        m_impl->loadCounter.increment();
        if (success) {
            m_impl->loadSuccessCounter.increment();
        }
    }

    void TelemetryCollector::incrementCheckpointCount() {
        m_impl->checkpointCounter.increment();
    }

    void TelemetryCollector::incrementRecoveryCount() {
        m_impl->recoveryCounter.increment();
    }

    void TelemetryCollector::incrementConflictCount() {
        m_impl->conflictCounter.increment();
    }

    void TelemetryCollector::setQueueDepth(size_t depth) {
        m_impl->queueDepthGauge.set(static_cast<double>(depth));
    }

    void TelemetryCollector::setCacheSize(size_t bytes) {
        m_impl->cacheSizeGauge.set(static_cast<double>(bytes));
    }

    void TelemetryCollector::setActiveExecutions(size_t count) {
        m_impl->activeExecutionsGauge.set(static_cast<double>(count));
    }

    nlohmann::json TelemetryCollector::getTelemetrySnapshot() const {
        nlohmann::json snapshot;
        
        snapshot["level"] = m_impl->level;
        snapshot["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
        
        snapshot["counters"]["persistTotal"] = m_impl->persistCounter.get();
        snapshot["counters"]["persistSuccess"] = m_impl->persistSuccessCounter.get();
        snapshot["counters"]["loadTotal"] = m_impl->loadCounter.get();
        snapshot["counters"]["loadSuccess"] = m_impl->loadSuccessCounter.get();
        snapshot["counters"]["checkpoints"] = m_impl->checkpointCounter.get();
        snapshot["counters"]["recoveries"] = m_impl->recoveryCounter.get();
        snapshot["counters"]["conflicts"] = m_impl->conflictCounter.get();
        
        snapshot["gauges"]["queueDepth"] = m_impl->queueDepthGauge.get();
        snapshot["gauges"]["cacheSize"] = m_impl->cacheSizeGauge.get();
        snapshot["gauges"]["activeExecutions"] = m_impl->activeExecutionsGauge.get();
        
        return snapshot;
    }

    nlohmann::json TelemetryCollector::getHistogramData(const std::string& name) const {
        Histogram* h = nullptr;
        
        if (name == "persistence") h = &m_impl->persistenceHistogram;
        else if (name == "compression") h = &m_impl->compressionHistogram;
        else if (name == "encryption") h = &m_impl->encryptionHistogram;
        else if (name == "replication") h = &m_impl->replicationHistogram;
        else if (name == "migration") h = &m_impl->migrationHistogram;
        
        if (!h) return nlohmann::json::object();
        
        nlohmann::json data;
        data["count"] = h->getCount();
        data["mean"] = h->getMean();
        data["min"] = h->getMin();
        data["max"] = h->getMax();
        data["p50"] = h->getPercentile(0.5);
        data["p95"] = h->getPercentile(0.95);
        data["p99"] = h->getPercentile(0.99);
        
        auto buckets = h->getBuckets();
        data["buckets"]["100us"] = buckets[TM_BUCKET_100US];
        data["buckets"]["1ms"] = buckets[TM_BUCKET_1MS];
        data["buckets"]["10ms"] = buckets[TM_BUCKET_10MS];
        data["buckets"]["100ms"] = buckets[TM_BUCKET_100MS];
        data["buckets"]["1s"] = buckets[TM_BUCKET_1S];
        data["buckets"]["10s"] = buckets[TM_BUCKET_10S];
        data["buckets"]["inf"] = buckets[TM_BUCKET_INF];
        
        return data;
    }

    std::string TelemetryCollector::exportPrometheus() const {
        std::ostringstream oss;
        
        oss << "# HELP rawrxd_persist_total Total persistence operations\n";
        oss << "# TYPE rawrxd_persist_total counter\n";
        oss << "rawrxd_persist_total " << m_impl->persistCounter.get() << "\n\n";
        
        oss << "# HELP rawrxd_persist_success Successful persistence operations\n";
        oss << "# TYPE rawrxd_persist_success counter\n";
        oss << "rawrxd_persist_success " << m_impl->persistSuccessCounter.get() << "\n\n";
        
        oss << "# HELP rawrxd_queue_depth Current queue depth\n";
        oss << "# TYPE rawrxd_queue_depth gauge\n";
        oss << "rawrxd_queue_depth " << m_impl->queueDepthGauge.get() << "\n";
        
        return oss.str();
    }

    std::string TelemetryCollector::exportStatsD() const {
        std::ostringstream oss;
        
        oss << "rawrxd.persist.total:" << m_impl->persistCounter.get() << "|c\n";
        oss << "rawrxd.persist.success:" << m_impl->persistSuccessCounter.get() << "|c\n";
        oss << "rawrxd.queue.depth:" << m_impl->queueDepthGauge.get() << "|g\n";
        
        return oss.str();
    }

    nlohmann::json TelemetryCollector::exportJson() const {
        return getTelemetrySnapshot();
    }

    int TelemetryCollector::getLevel() const {
        return m_impl->level;
    }

    void TelemetryCollector::enableCategory(uint32_t category) {
        m_impl->enabledCategories |= category;
    }

    void TelemetryCollector::disableCategory(uint32_t category) {
        m_impl->enabledCategories &= ~category;
    }

    void TelemetryCollector::flush() {
        // Flush any pending telemetry data
        // Implementation: ensure all metrics are written/synchronized
    }

    // ===== DiagnosticLogger Implementation =====

    class DiagnosticLogger::Impl {
    public:
        std::deque<nlohmann::json> events;
        size_t maxEvents = 10000;
        std::mutex mutex;
    };

    DiagnosticLogger::DiagnosticLogger() 
        : m_impl(std::make_unique<Impl>()) {
    }

    DiagnosticLogger::~DiagnosticLogger() = default;

    void DiagnosticLogger::logEvent(
        const std::string& category,
        const std::string& event,
        const nlohmann::json& details) {
        
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        
        nlohmann::json entry;
        entry["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
        entry["category"] = category;
        entry["event"] = event;
        entry["details"] = details;
        entry["level"] = "info";
        
        m_impl->events.push_back(entry);
        
        if (m_impl->events.size() > m_impl->maxEvents) {
            m_impl->events.pop_front();
        }
    }

    void DiagnosticLogger::logError(
        const std::string& category,
        const std::string& error,
        const std::string& context) {
        
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        
        nlohmann::json entry;
        entry["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
        entry["category"] = category;
        entry["event"] = error;
        entry["context"] = context;
        entry["level"] = "error";
        
        m_impl->events.push_back(entry);
        
        if (m_impl->events.size() > m_impl->maxEvents) {
            m_impl->events.pop_front();
        }
    }

    void DiagnosticLogger::logWarning(
        const std::string& category,
        const std::string& warning) {
        
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        
        nlohmann::json entry;
        entry["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
        entry["category"] = category;
        entry["event"] = warning;
        entry["level"] = "warning";
        
        m_impl->events.push_back(entry);
        
        if (m_impl->events.size() > m_impl->maxEvents) {
            m_impl->events.pop_front();
        }
    }

    std::vector<nlohmann::json> DiagnosticLogger::getRecentEvents(size_t count) const {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        
        std::vector<nlohmann::json> result;
        size_t start = m_impl->events.size() > count ? m_impl->events.size() - count : 0;
        
        for (size_t i = start; i < m_impl->events.size(); i++) {
            result.push_back(m_impl->events[i]);
        }
        
        return result;
    }

    std::vector<nlohmann::json> DiagnosticLogger::getEventsByCategory(
        const std::string& category,
        size_t count) const {
        
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        
        std::vector<nlohmann::json> result;
        
        for (auto it = m_impl->events.rbegin(); it != m_impl->events.rend() && result.size() < count; ++it) {
            if (it->value("category", "") == category) {
                result.push_back(*it);
            }
        }
        
        return result;
    }

    nlohmann::json DiagnosticLogger::exportReport() const {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        
        nlohmann::json report;
        report["totalEvents"] = m_impl->events.size();
        report["generatedAt"] = std::chrono::system_clock::now().time_since_epoch().count();
        
        // Count by level
        std::map<std::string, size_t> levelCounts;
        for (const auto& event : m_impl->events) {
            levelCounts[event.value("level", "unknown")]++;
        }
        
        for (const auto& pair : levelCounts) {
            report["byLevel"][pair.first] = pair.second;
        }
        
        return report;
    }

    void DiagnosticLogger::clear() {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->events.clear();
    }

    // ===== PersistenceProfiler Implementation =====

    class PersistenceProfiler::Impl {
    public:
        std::unordered_map<std::string, std::vector<int64_t>> sectionTimes;
        std::unordered_map<std::string, Timer> activeTimers;
        std::mutex mutex;
    };

    PersistenceProfiler::PersistenceProfiler() 
        : m_impl(std::make_unique<Impl>()) {
    }

    PersistenceProfiler::~PersistenceProfiler() = default;

    void PersistenceProfiler::beginSection(const std::string& name) {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->activeTimers[name].start();
    }

    void PersistenceProfiler::endSection(const std::string& name) {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        
        auto it = m_impl->activeTimers.find(name);
        if (it != m_impl->activeTimers.end()) {
            it->second.stop();
            m_impl->sectionTimes[name].push_back(it->second.elapsedMicros());
            m_impl->activeTimers.erase(it);
        }
    }

    nlohmann::json PersistenceProfiler::getProfileData() const {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        
        nlohmann::json data;
        
        for (const auto& pair : m_impl->sectionTimes) {
            const auto& times = pair.second;
            
            if (times.empty()) continue;
            
            int64_t total = 0;
            int64_t min = times[0];
            int64_t max = times[0];
            
            for (int64_t t : times) {
                total += t;
                min = std::min(min, t);
                max = std::max(max, t);
            }
            
            data[pair.first]["count"] = times.size();
            data[pair.first]["total_us"] = total;
            data[pair.first]["mean_us"] = total / times.size();
            data[pair.first]["min_us"] = min;
            data[pair.first]["max_us"] = max;
        }
        
        return data;
    }

    void PersistenceProfiler::reset() {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->sectionTimes.clear();
        m_impl->activeTimers.clear();
    }

    // ===== Global Instances =====

    TelemetryCollector& getGlobalCollector() {
        static TelemetryCollector instance;
        return instance;
    }

    DiagnosticLogger& getGlobalLogger() {
        static DiagnosticLogger instance;
        return instance;
    }

    PersistenceProfiler& getGlobalProfiler() {
        static PersistenceProfiler instance;
        return instance;
    }

    // ===== ScopedTimer Implementation =====

    ScopedTimer::ScopedTimer(const std::string& metricName)
        : m_metricName(metricName) {
        m_timer.start();
    }

    ScopedTimer::~ScopedTimer() {
        m_timer.stop();
        // In production: record to collector
    }

} // namespace TelemetryPersistence
