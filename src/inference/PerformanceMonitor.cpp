#include "PerformanceMonitor.h"
#include <iostream>
#include <algorithm>

namespace RawrXD::Inference {

PerformanceMonitor& PerformanceMonitor::instance() {
    static PerformanceMonitor instance;
    return instance;
}

void PerformanceMonitor::startOperation(const std::string& operation) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& data = operations_[operation];
    if (data.active) {
        // Operation already running, this shouldn't happen
        return;
    }
    data.startTime = std::chrono::steady_clock::now();
    data.active = true;

    if (detailedLogging_) {
        std::cout << "[Performance] Started: " << operation << std::endl;
    }
}

void PerformanceMonitor::endOperation(const std::string& operation) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = operations_.find(operation);
    if (it == operations_.end() || !it->second.active) {
        return;
    }

    auto& data = it->second;
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        endTime - data.startTime);

    data.metrics.totalTime += duration;
    data.metrics.operationCount++;
    data.metrics.averageLatencyMs = static_cast<double>(data.metrics.totalTime.count()) /
                                   (data.metrics.operationCount * 1000.0);

    auto totalSeconds = std::chrono::duration_cast<std::chrono::seconds>(
        data.metrics.totalTime).count();
    if (totalSeconds > 0) {
        data.metrics.throughputOpsPerSec = static_cast<double>(data.metrics.operationCount) /
                                          totalSeconds;
    }

    data.active = false;

    if (detailedLogging_) {
        std::cout << "[Performance] Completed: " << operation
                  << " in " << duration.count() << "us" << std::endl;
    }
}

void PerformanceMonitor::recordError(const std::string& operation) {
    std::lock_guard<std::mutex> lock(mutex_);
    operations_[operation].metrics.errorCount++;
}

void PerformanceMonitor::recordMemoryUsage(const std::string& operation, size_t bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& data = operations_[operation];
    data.metrics.memoryPeakUsage = std::max(data.metrics.memoryPeakUsage, bytes);
}

PerformanceMetrics PerformanceMonitor::getMetrics(const std::string& operation) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = operations_.find(operation);
    return it != operations_.end() ? it->second.metrics : PerformanceMetrics{};
}

std::unordered_map<std::string, PerformanceMetrics> PerformanceMonitor::getAllMetrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::unordered_map<std::string, PerformanceMetrics> result;
    for (const auto& [op, data] : operations_) {
        result[op] = data.metrics;
    }
    return result;
}

void PerformanceMonitor::resetMetrics(const std::string& operation) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (operation.empty()) {
        operations_.clear();
    } else {
        operations_.erase(operation);
    }
}

} // namespace RawrXD::Inference