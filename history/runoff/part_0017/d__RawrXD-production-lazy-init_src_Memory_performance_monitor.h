#pragma once
#include <chrono>
#include <atomic>
#include <memory>

// Simple performance monitoring stub to satisfy includes
// NOTE: This is a legacy stub. Use src/performance_monitor.h (QObject-based) instead.
class PerformanceMonitorStub {
public:
    PerformanceMonitorStub() = default;
    ~PerformanceMonitorStub() = default;
    
    void recordMetric(const std::string& name, double value) {}
    void startTimer(const std::string& name) {}
    void stopTimer(const std::string& name) {}
    double getMetric(const std::string& name) const { return 0.0; }
};

