#pragma once
// PerformanceMonitor.h — System performance tracking (CPU, GPU, memory)
// Pure Win32/STL implementation using PDH counters
// No Qt. No exceptions. C++20 only.

#include <windows.h>
#include <pdh.h>
#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <thread>
#include <iostream>

#pragma comment(lib, "pdh.lib")

class PerformanceMonitor {
public:
    struct Snapshot {
        double cpu_percent = 0.0;
        double gpu_percent = 0.0;
        uint64_t memory_used_mb = 0;
        uint64_t memory_total_mb = 0;
        uint64_t uptime_seconds = 0;
        std::unordered_map<std::string, double> custom_gauges;
    };

    PerformanceMonitor() = default;
    ~PerformanceMonitor() { StopTracking(); }

    void StartTracking(int poll_interval_ms = 1000) {
        if (m_running.exchange(true)) return;
        m_pollInterval = poll_interval_ms;
        m_startTime = std::chrono::steady_clock::now();
        m_thread = std::thread([this]() { PollLoop(); });
    }

    void StopTracking() {
        m_running.store(false);
        if (m_thread.joinable()) m_thread.join();
    }

    Snapshot GetSnapshot() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_snapshot;
    }

    void SetGauge(const std::string& name, double value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_snapshot.custom_gauges[name] = value;
    }

    bool IsTracking() const { return m_running.load(); }

private:
    void PollLoop() {
        PDH_HQUERY query = nullptr;
        PDH_HCOUNTER cpuCounter = nullptr;
        PdhOpenQuery(nullptr, 0, &query);
        PdhAddEnglishCounterW(query, L"\\Processor(_Total)\\% Processor Time", 0, &cpuCounter);
        PdhCollectQueryData(query); // Initial sample

        while (m_running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(m_pollInterval));

            // CPU
            if (query) {
                PdhCollectQueryData(query);
                PDH_FMT_COUNTERVALUE val = {};
                if (PdhGetFormattedCounterValue(cpuCounter, PDH_FMT_DOUBLE, nullptr, &val) == ERROR_SUCCESS) {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_snapshot.cpu_percent = val.doubleValue;
                }
            }

            // Memory
            MEMORYSTATUSEX memInfo = {};
            memInfo.dwLength = sizeof(memInfo);
            if (GlobalMemoryStatusEx(&memInfo)) {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_snapshot.memory_total_mb = memInfo.ullTotalPhys / (1024 * 1024);
                m_snapshot.memory_used_mb = (memInfo.ullTotalPhys - memInfo.ullAvailPhys) / (1024 * 1024);
            }

            // Uptime
            {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime).count();
                std::lock_guard<std::mutex> lock(m_mutex);
                m_snapshot.uptime_seconds = static_cast<uint64_t>(elapsed);
            }
        }

        if (query) PdhCloseQuery(query);
    }

    mutable std::mutex m_mutex;
    std::atomic<bool> m_running{false};
    int m_pollInterval = 1000;
    Snapshot m_snapshot;
    std::thread m_thread;
    std::chrono::steady_clock::time_point m_startTime;
};
