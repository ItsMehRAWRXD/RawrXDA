#pragma once
#include <chrono>
#include <atomic>
#include <memory>

// Simple performance monitoring stub to satisfy includes
class PerformanceMonitor {
public:
    PerformanceMonitor() = default;
    ~PerformanceMonitor() = default;
    
    void recordMetric(const std::string& name, double value) {}
    void startTimer(const std::string& name) {}
    void stopTimer(const std::string& name) {}
    double getMetric(const std::string& name) const { return 0.0; }
};
