#pragma once
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>

namespace RawrXD::Inference {

struct PerformanceMetrics {
    std::chrono::microseconds totalTime{0};
    size_t operationCount{0};
    double averageLatencyMs{0.0};
    double throughputOpsPerSec{0.0};
    size_t memoryPeakUsage{0};
    size_t errorCount{0};
};

class PerformanceMonitor {
public:
    static PerformanceMonitor& instance();

    void startOperation(const std::string& operation);
    void endOperation(const std::string& operation);
    void recordLatency(const std::string& operation, int64_t microseconds);
    void recordError(const std::string& operation);
    void recordMemoryUsage(const std::string& operation, size_t bytes);

    PerformanceMetrics getMetrics(const std::string& operation) const;
    std::unordered_map<std::string, PerformanceMetrics> getAllMetrics() const;

    void resetMetrics(const std::string& operation = "");
    void enableDetailedLogging(bool enable) { detailedLogging_ = enable; }

private:
    PerformanceMonitor() = default;

    struct OperationData {
        std::chrono::steady_clock::time_point startTime;
        bool active{false};
        PerformanceMetrics metrics;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, OperationData> operations_;
    bool detailedLogging_{false};
};

} // namespace RawrXD::Inference