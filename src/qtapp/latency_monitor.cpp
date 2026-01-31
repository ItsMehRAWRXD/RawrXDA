#include "latency_monitor.h"
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

namespace RawrXD {

LatencyMonitor::LatencyMonitor()
    
{  // Signal connection removed\nm_pingTimer.setInterval(500); // ping every 500ms
    m_pingTimer.start();  // Signal connection removed\nm_metricsTimer.setInterval(1000); // update metrics every second
    m_metricsTimer.start();
}

int LatencyMonitor::ping() const
{
    return m_stats.currentPing;
}

void LatencyMonitor::recordPing(int latencyMs)
{
    m_stats.currentPing = latencyMs;
    
    if (m_stats.minPing < 0 || latencyMs < m_stats.minPing) {
        m_stats.minPing = latencyMs;
    }
    if (m_stats.maxPing < 0 || latencyMs > m_stats.maxPing) {
        m_stats.maxPing = latencyMs;
    }
    
    m_stats.totalLatency += latencyMs;
    m_stats.totalSamples++;
    
    updateStats();
    pingUpdated(latencyMs);
    statsUpdated(m_stats);
}

void LatencyMonitor::setStatus(const std::string& status)
{
    m_stats.status = status;
    statsUpdated(m_stats);
}

void LatencyMonitor::setBackend(const std::string& backend)
{
    m_stats.backendName = backend;
    statsUpdated(m_stats);
}

void LatencyMonitor::reset()
{
    m_stats.currentPing = -1;
    m_stats.minPing = -1;
    m_stats.maxPing = -1;
    m_stats.avgPing = -1;
    m_stats.totalSamples = 0;
    m_stats.totalLatency = 0;
    m_stats.status = "idle";
}

void LatencyMonitor::onPingTimer()
{
    // Measure latency: model-to-IDE communication round-trip
    m_timer.start();
    
    QCoreApplication::processEvents();
    
    int latency = static_cast<int>(m_timer.elapsed());
    
    if (latency < 1) {
        latency = 1;
    }
    
    recordPing(latency);
}

void LatencyMonitor::updateSystemMetrics()
{
#ifdef _WIN32
    // RAM Usage
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        m_stats.ramUsageMB = static_cast<double>(pmc.PrivateUsage) / (1024.0 * 1024.0);
    }

    // CPU Usage
    FILETIME idleTime, kernelTime, userTime;
    FILETIME creationTime, exitTime, procKernelTime, procUserTime;
    
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime) &&
        GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &procKernelTime, &procUserTime)) 
    {
        auto ftToLong = [](FILETIME ft) {
            return (static_cast<long long>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        };

        long long currentCpuTime = ftToLong(procKernelTime) + ftToLong(procUserTime);
        long long currentSysTime = ftToLong(kernelTime) + ftToLong(userTime);

        if (m_lastCpuTime > 0) {
            long long cpuDiff = currentCpuTime - m_lastCpuTime;
            long long sysDiff = currentSysTime - m_lastSysTime;
            if (sysDiff > 0) {
                m_stats.cpuUsagePercent = (100.0 * cpuDiff) / sysDiff;
            }
        }

        m_lastCpuTime = currentCpuTime;
        m_lastSysTime = currentSysTime;
    }
#endif
    statsUpdated(m_stats);
}

void LatencyMonitor::updateStats()
{
    if (m_stats.totalSamples > 0) {
        m_stats.avgPing = static_cast<int>(m_stats.totalLatency / m_stats.totalSamples);
    }
}

} // namespace RawrXD



