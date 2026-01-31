#include "SLOTracker.h"

#include <chrono>
#include <iostream>

namespace {
int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void LogInfo(const std::string& event, const std::string& detail) {
    
}

void LogWarn(const std::string& event, const std::string& detail) {
    
}
}

void SLOTracker::defineSLO(const std::string& service, double target, int windowMinutes) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_counts[service] = {0, 0, NowMs(), windowMinutes};
    m_slos[service] = {target, 100.0, windowMinutes};
    LogInfo("defined", service + " target=" + std::to_string(target) + " window_min=" + std::to_string(windowMinutes));
}

void SLOTracker::recordSuccess(const std::string& service) {
    std::lock_guard<std::mutex> locker(m_mutex);
    const int64_t now = NowMs();
    prune(service, now);
    auto& wc = m_counts[service];
    wc.successes++;
    m_slos[service].current = availabilityLocked(service);
}

void SLOTracker::recordFailure(const std::string& service) {
    std::lock_guard<std::mutex> locker(m_mutex);
    const int64_t now = NowMs();
    prune(service, now);
    auto& wc = m_counts[service];
    wc.failures++;

    m_slos[service].current = availabilityLocked(service);
    const auto it = m_slos.find(service);
    const double target = (it == m_slos.end()) ? 0.0 : it->second.target;
    const double avail = m_slos[service].current;
    if (target > 0.0 && avail < target) {
        LogWarn("breach", service + " availability=" + std::to_string(avail) + " target=" + std::to_string(target));
        sloBreached(service, avail, target);
    }
}

double SLOTracker::availability(const std::string& service) const {
    std::lock_guard<std::mutex> locker(m_mutex);
    return availabilityLocked(service);
}

SLOTracker::SLO SLOTracker::slo(const std::string& service) const {
    std::lock_guard<std::mutex> locker(m_mutex);
    SLO result{};
    const auto it = m_slos.find(service);
    if (it != m_slos.end()) {
        result = it->second;
    }
    result.current = availabilityLocked(service);
    return result;
}

void SLOTracker::prune(const std::string& service, int64_t nowMs) {
    auto& wc = m_counts[service];
    if (wc.windowMinutes <= 0) {
        wc.windowMinutes = 1440;
    }
    const int64_t windowMs = static_cast<int64_t>(wc.windowMinutes) * 60 * 1000;
    if (wc.windowStart == 0) {
        wc.windowStart = nowMs;
    }
    if (nowMs - wc.windowStart > windowMs) {
        wc.successes = 0;
        wc.failures = 0;
        wc.windowStart = nowMs;
    }
}

double SLOTracker::availabilityLocked(const std::string& service) const {
    auto it = m_counts.find(service);
    if (it == m_counts.end()) {
        return 100.0;
    }
    const auto& wc = it->second;
    const int total = wc.successes + wc.failures;
    if (total == 0) {
        return 100.0;
    }
    return (static_cast<double>(wc.successes) / total) * 100.0;
}

void SLOTracker::sloBreached(const std::string& service, double availability, double target) {
    (void)service;
    (void)availability;
    (void)target;
}

