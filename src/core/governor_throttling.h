#pragma once

#include <windows.h>
#include <pdh.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

#pragma comment(lib, "pdh.lib")

namespace RawrXD {

class GovernorThrottling {
public:
    GovernorThrottling();
    ~GovernorThrottling();

    bool initialize();
    void shutdown();

    // Throttling control
    void setCpuThreshold(float threshold); // 0.0-1.0
    void setGpuThreshold(float threshold); // 0.0-1.0
    void setMemoryThreshold(float threshold); // 0.0-1.0

    // Check if operation should be throttled
    bool shouldThrottle() const;

    // Get current system metrics
    float getCpuUsage() const;
    float getGpuUsage() const;
    float getMemoryUsage() const;

    // Throttling actions
    void throttleInference(int delayMs);
    void throttleBackgroundTasks();

private:
    void monitoringThread();

    PDH_HQUERY m_cpuQuery;
    PDH_HCOUNTER m_cpuCounter;
    std::atomic<float> m_cpuUsage;
    std::atomic<float> m_gpuUsage;
    std::atomic<float> m_memoryUsage;

    float m_cpuThreshold;
    float m_gpuThreshold;
    float m_memoryThreshold;

    std::thread m_monitorThread;
    std::atomic<bool> m_running;
    mutable std::mutex m_mutex;
};

} // namespace RawrXD