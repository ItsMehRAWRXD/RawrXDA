/**
 * @file metrics_stubs.cpp
 * @brief Simplified LLMMetrics and CircuitBreakerMetrics implementation for Qt shell linkage
 * @date 2026-01-17
 */

#include <cstdint>
#include <cstddef>
#include <QString>
#include <QMutex>
#include <QMutexLocker>
#include <QDebug>
#include <atomic>
#include <deque>
#include <vector>
#include <algorithm>

namespace RawrXD {

class LLMMetrics {
private:
    static QMutex g_metrics_mutex;
    static std::atomic<uint64_t> g_total_requests;
    static std::atomic<uint64_t> g_successful_requests;
    static std::atomic<uint64_t> g_failed_requests;
    static std::atomic<uint64_t> g_total_tokens;
    static std::atomic<uint64_t> g_total_retries;
    static std::atomic<uint64_t> g_cache_hits;
    static std::atomic<uint64_t> g_total_latency_ms;
    static std::deque<uint64_t> g_latency_window;
    static constexpr size_t MAX_WINDOW_SIZE = 1000;

public:
    struct Request {
        QString backend;
        uint64_t latencyMs;
        int tokensUsed;
        bool success;
        int retryAttempts;
        bool cacheHit;
    };

    struct MetricsSummary {
        uint64_t totalRequests;
        uint64_t successfulRequests;
        uint64_t failedRequests;
        uint64_t totalTokens;
        uint64_t cacheHits;
        double avgLatencyMs;
        uint64_t p50LatencyMs;
        uint64_t p95LatencyMs;
        uint64_t p99LatencyMs;
        double successRate;
        double cacheHitRate;
    };

    static void recordRequest(const Request& request) {
        QMutexLocker lock(&g_metrics_mutex);

        g_total_requests++;
        g_total_latency_ms += request.latencyMs;
        g_total_tokens += request.tokensUsed;
        g_total_retries += request.retryAttempts;

        if (request.success) {
            g_successful_requests++;
        } else {
            g_failed_requests++;
        }

        if (request.cacheHit) {
            g_cache_hits++;
        }

        g_latency_window.push_back(request.latencyMs);
        if (g_latency_window.size() > MAX_WINDOW_SIZE) {
            g_latency_window.pop_front();
        }

        qDebug() << "[LLMMetrics]" << request.backend << "latency=" << request.latencyMs
                 << "tokens=" << request.tokensUsed << "success=" << request.success
                 << "retries=" << request.retryAttempts << "cache_hit=" << request.cacheHit;
    }

    static uint64_t getPercentile(int percentile) {
        if (g_latency_window.empty()) {
            return 0;
        }

        std::vector<uint64_t> sorted{
            g_latency_window.begin(), g_latency_window.end()};
        std::sort(sorted.begin(), sorted.end());

        size_t idx = (static_cast<size_t>(percentile) * sorted.size()) / 100;
        if (idx >= sorted.size()) idx = sorted.size() - 1;
        return sorted[idx];
    }

    static MetricsSummary getSummary() {
        QMutexLocker lock(&g_metrics_mutex);

        MetricsSummary summary;
        summary.totalRequests = g_total_requests;
        summary.successfulRequests = g_successful_requests;
        summary.failedRequests = g_failed_requests;
        summary.totalTokens = g_total_tokens;
        summary.cacheHits = g_cache_hits;
        summary.avgLatencyMs = g_total_requests > 0 ?
            static_cast<double>(g_total_latency_ms) / g_total_requests : 0.0;
        summary.p50LatencyMs = getPercentile(50);
        summary.p95LatencyMs = getPercentile(95);
        summary.p99LatencyMs = getPercentile(99);
        summary.successRate = g_total_requests > 0 ?
            static_cast<double>(g_successful_requests) / g_total_requests : 0.0;
        summary.cacheHitRate = g_total_requests > 0 ?
            static_cast<double>(g_cache_hits) / g_total_requests : 0.0;

        return summary;
    }

    static void reset() {
        QMutexLocker lock(&g_metrics_mutex);
        g_total_requests = 0;
        g_successful_requests = 0;
        g_failed_requests = 0;
        g_total_tokens = 0;
        g_total_retries = 0;
        g_cache_hits = 0;
        g_total_latency_ms = 0;
        g_latency_window.clear();
        qDebug() << "[LLMMetrics] Metrics reset";
    }
};

class CircuitBreakerMetrics {
private:
    static QMutex g_cb_mutex;
    static std::atomic<uint64_t> g_state_changes;
    static std::atomic<uint64_t> g_open_events;
    static std::atomic<uint64_t> g_half_open_events;
    static std::atomic<uint64_t> g_closed_events;
    static std::atomic<uint64_t> g_rejected_requests;

public:
    struct Event {
        QString backend;
        QString status;
        QString reason;
    };

    struct CBSummary {
        uint64_t stateChanges;
        uint64_t openEvents;
        uint64_t halfOpenEvents;
        uint64_t closedEvents;
        uint64_t rejectedRequests;
    };

    static void recordEvent(const Event& event) {
        QMutexLocker lock(&g_cb_mutex);

        g_state_changes++;
        if (event.status == "open") {
            g_open_events++;
        } else if (event.status == "half-open") {
            g_half_open_events++;
        } else if (event.status == "closed") {
            g_closed_events++;
        }

        qDebug() << "[CircuitBreaker]" << event.backend << event.status << event.reason;
    }

    static void recordRejection(const QString& backend) {
        g_rejected_requests++;
        qDebug() << "[CircuitBreaker] Request rejected for backend:" << backend;
    }

    static CBSummary getSummary() {
        QMutexLocker lock(&g_cb_mutex);

        return {
            g_state_changes,
            g_open_events,
            g_half_open_events,
            g_closed_events,
            g_rejected_requests
        };
    }

    static void reset() {
        QMutexLocker lock(&g_cb_mutex);
        g_state_changes = 0;
        g_open_events = 0;
        g_half_open_events = 0;
        g_closed_events = 0;
        g_rejected_requests = 0;
        qDebug() << "[CircuitBreaker] Metrics reset";
    }
};

QMutex LLMMetrics::g_metrics_mutex;
std::atomic<uint64_t> LLMMetrics::g_total_requests{0};
std::atomic<uint64_t> LLMMetrics::g_successful_requests{0};
std::atomic<uint64_t> LLMMetrics::g_failed_requests{0};
std::atomic<uint64_t> LLMMetrics::g_total_tokens{0};
std::atomic<uint64_t> LLMMetrics::g_total_retries{0};
std::atomic<uint64_t> LLMMetrics::g_cache_hits{0};
std::atomic<uint64_t> LLMMetrics::g_total_latency_ms{0};
std::deque<uint64_t> LLMMetrics::g_latency_window;

QMutex CircuitBreakerMetrics::g_cb_mutex;
std::atomic<uint64_t> CircuitBreakerMetrics::g_state_changes{0};
std::atomic<uint64_t> CircuitBreakerMetrics::g_open_events{0};
std::atomic<uint64_t> CircuitBreakerMetrics::g_half_open_events{0};
std::atomic<uint64_t> CircuitBreakerMetrics::g_closed_events{0};
std::atomic<uint64_t> CircuitBreakerMetrics::g_rejected_requests{0};

}  // namespace RawrXD
