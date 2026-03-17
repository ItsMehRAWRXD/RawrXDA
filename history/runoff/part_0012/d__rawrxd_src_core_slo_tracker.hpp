#pragma once
// ============================================================================
// SLO Tracker — Service Level Objective Monitoring (Phase 33 Quick-Win Port)
// Pure C++20 — zero Qt/platform dependencies
// Ported from src/qtapp/SLOTracker.h with full feature parity
// ============================================================================

#ifndef RAWRXD_SLO_TRACKER_HPP
#define RAWRXD_SLO_TRACKER_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <deque>

// ============================================================================
// SLO Definition
// ============================================================================
struct SLODefinition {
    std::string serviceName;      // e.g. "inference", "transcription", "tts"
    double targetAvailability;    // e.g. 0.999 for 99.9%
    int64_t windowMs;            // rolling window (e.g. 3600000 for 1 hour)
    int64_t maxLatencyMs;        // max acceptable p99 latency
    int64_t minThroughput;       // min requests per second
};

// ============================================================================
// SLO Event
// ============================================================================
struct SLOEvent {
    std::chrono::steady_clock::time_point timestamp;
    bool success;
    int64_t latencyMs;
};

// ============================================================================
// SLO Status
// ============================================================================
struct SLOStatus {
    std::string serviceName;
    double currentAvailability;  // 0.0 - 1.0
    double targetAvailability;
    bool breached;
    int64_t totalRequests;
    int64_t successCount;
    int64_t failureCount;
    int64_t windowMs;
    double avgLatencyMs;
    double p99LatencyMs;
    int64_t errorBudgetRemaining; // requests before breach
};

// ============================================================================
// Breach callback
// ============================================================================
using SLOBreachCallback = void(*)(const SLOStatus& status, void* userData);

// ============================================================================
// SLOTracker class
// ============================================================================
class SLOTracker {
public:
    SLOTracker() = default;
    ~SLOTracker() = default;

    // Define an SLO for a service
    void defineSLO(const SLODefinition& def) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_definitions[def.serviceName] = def;
        m_events[def.serviceName] = {};
    }

    // Record a success
    void recordSuccess(const std::string& service, int64_t latencyMs = 0) {
        record(service, true, latencyMs);
    }

    // Record a failure
    void recordFailure(const std::string& service, int64_t latencyMs = 0) {
        record(service, false, latencyMs);
    }

    // Get current SLO status for a service
    SLOStatus getStatus(const std::string& service) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return computeStatus(service);
    }

    // Get all SLO statuses
    std::vector<SLOStatus> getAllStatuses() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<SLOStatus> results;
        for (auto& [name, _] : m_definitions) {
            results.push_back(computeStatus(name));
        }
        return results;
    }

    // Check if any SLO is breached
    bool hasBreaches() {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& [name, _] : m_definitions) {
            auto status = computeStatus(name);
            if (status.breached) return true;
        }
        return false;
    }

    // Set breach callback
    void setBreachCallback(SLOBreachCallback cb, void* userData) {
        m_breachCb = cb;
        m_breachUserData = userData;
    }

    // Reset all tracked data
    void reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_events.clear();
    }

    // Get list of tracked services
    std::vector<std::string> getServices() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::string> services;
        for (auto& [name, _] : m_definitions) {
            services.push_back(name);
        }
        return services;
    }

private:
    void record(const std::string& service, bool success, int64_t latencyMs) {
        std::lock_guard<std::mutex> lock(m_mutex);

        SLOEvent evt;
        evt.timestamp = std::chrono::steady_clock::now();
        evt.success = success;
        evt.latencyMs = latencyMs;
        m_events[service].push_back(evt);

        // Prune old events outside window
        pruneEvents(service);

        // Check for breach
        auto status = computeStatus(service);
        if (status.breached && m_breachCb) {
            m_breachCb(status, m_breachUserData);
        }
    }

    void pruneEvents(const std::string& service) {
        auto it = m_definitions.find(service);
        if (it == m_definitions.end()) return;

        auto cutoff = std::chrono::steady_clock::now() -
                      std::chrono::milliseconds(it->second.windowMs);

        auto& events = m_events[service];
        while (!events.empty() && events.front().timestamp < cutoff) {
            events.pop_front();
        }
    }

    SLOStatus computeStatus(const std::string& service) {
        SLOStatus status = {};
        status.serviceName = service;

        auto defIt = m_definitions.find(service);
        if (defIt == m_definitions.end()) return status;

        const auto& def = defIt->second;
        status.targetAvailability = def.targetAvailability;
        status.windowMs = def.windowMs;

        auto& events = m_events[service];
        status.totalRequests = static_cast<int64_t>(events.size());

        if (events.empty()) {
            status.currentAvailability = 1.0;
            status.breached = false;
            return status;
        }

        // Compute availability
        double totalLatency = 0;
        std::vector<int64_t> latencies;
        for (auto& evt : events) {
            if (evt.success) status.successCount++;
            else status.failureCount++;
            totalLatency += evt.latencyMs;
            latencies.push_back(evt.latencyMs);
        }

        status.currentAvailability = static_cast<double>(status.successCount) /
                                      static_cast<double>(status.totalRequests);
        status.avgLatencyMs = totalLatency / status.totalRequests;

        // P99 latency
        if (!latencies.empty()) {
            std::sort(latencies.begin(), latencies.end());
            size_t p99Idx = static_cast<size_t>(latencies.size() * 0.99);
            if (p99Idx >= latencies.size()) p99Idx = latencies.size() - 1;
            status.p99LatencyMs = static_cast<double>(latencies[p99Idx]);
        }

        // Breach detection
        status.breached = (status.currentAvailability < def.targetAvailability);

        // Error budget
        if (def.targetAvailability < 1.0) {
            double allowedFailureRate = 1.0 - def.targetAvailability;
            int64_t allowedFailures = static_cast<int64_t>(status.totalRequests * allowedFailureRate);
            status.errorBudgetRemaining = allowedFailures - status.failureCount;
            if (status.errorBudgetRemaining < 0) status.errorBudgetRemaining = 0;
        }

        return status;
    }

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, SLODefinition> m_definitions;
    std::unordered_map<std::string, std::deque<SLOEvent>> m_events;
    SLOBreachCallback m_breachCb = nullptr;
    void* m_breachUserData = nullptr;
};

#endif // RAWRXD_SLO_TRACKER_HPP
