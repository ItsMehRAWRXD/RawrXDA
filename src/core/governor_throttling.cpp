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

GovernorThrottling::GovernorThrottling()
    : m_cpuUsage(0.0f), m_gpuUsage(0.0f), m_memoryUsage(0.0f),
      m_cpuThreshold(0.8f), m_gpuThreshold(0.8f), m_memoryThreshold(0.9f),
      m_running(false) {
}

GovernorThrottling::~GovernorThrottling() {
    shutdown();
}

bool GovernorThrottling::initialize() {
    // Initialize PDH for CPU monitoring
    if (PdhOpenQueryA(nullptr, 0, &m_cpuQuery) != ERROR_SUCCESS) {
        std::cerr << "Failed to open PDH query" << std::endl;
        return false;
    }

    if (PdhAddCounterA(m_cpuQuery, "\\Processor(_Total)\\% Processor Time", 0, &m_cpuCounter) != ERROR_SUCCESS) {
        std::cerr << "Failed to add CPU counter" << std::endl;
        PdhCloseQuery(m_cpuQuery);
        return false;
    }

    m_running = true;
    m_monitorThread = std::thread([this]() { this->monitoringThread(); });

    std::cout << "Governor Throttling initialized" << std::endl;
    return true;
}

void GovernorThrottling::shutdown() {
    m_running = false;
    if (m_monitorThread.joinable()) {
        m_monitorThread.join();
    }
    PdhCloseQuery(m_cpuQuery);
}

void GovernorThrottling::setCpuThreshold(float threshold) {
    m_cpuThreshold = std::max(0.0f, std::min(1.0f, threshold));
}

void GovernorThrottling::setGpuThreshold(float threshold) {
    m_gpuThreshold = std::max(0.0f, std::min(1.0f, threshold));
}

void GovernorThrottling::setMemoryThreshold(float threshold) {
    m_memoryThreshold = std::max(0.0f, std::min(1.0f, threshold));
}

bool GovernorThrottling::shouldThrottle() const {
    return m_cpuUsage > m_cpuThreshold ||
           m_gpuUsage > m_gpuThreshold ||
           m_memoryUsage > m_memoryThreshold;
}

float GovernorThrottling::getCpuUsage() const {
    return m_cpuUsage;
}

float GovernorThrottling::getGpuUsage() const {
    return m_gpuUsage;
}

float GovernorThrottling::getMemoryUsage() const {
    return m_memoryUsage;
}

void GovernorThrottling::throttleInference(int delayMs) {
    if (shouldThrottle()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }
}

void GovernorThrottling::throttleBackgroundTasks() {
    if (shouldThrottle()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void GovernorThrottling::monitoringThread() {
    while (m_running) {
        // Update CPU usage
        PdhCollectQueryData(m_cpuQuery);
        PDH_FMT_COUNTERVALUE value;
        if (PdhGetFormattedCounterValue(m_cpuCounter, PDH_FMT_DOUBLE, nullptr, &value) == ERROR_SUCCESS) {
            m_cpuUsage = static_cast<float>(value.doubleValue) / 100.0f;
        }

        // Update memory usage
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memInfo)) {
            m_memoryUsage = 1.0f - static_cast<float>(memInfo.ullAvailPhys) / static_cast<float>(memInfo.ullTotalPhys);
        }

        // Update GPU usage (placeholder - in real implementation, use DXGI or NVAPI)
        // For now, estimate based on CPU
        m_gpuUsage = std::min(1.0f, m_cpuUsage * 1.2f); // Slightly higher than CPU

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

} // namespace RawrXD