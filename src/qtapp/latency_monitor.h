#pragma once
namespace RawrXD {

struct LatencyStats
{
    int currentPing = -1;      // Current latency in milliseconds
    int minPing = -1;          // Minimum latency recorded
    int maxPing = -1;          // Maximum latency recorded
    int avgPing = -1;          // Average latency
    long totalSamples = 0;     // Total ping samples collected
    long totalLatency = 0;     // Cumulative latency for averaging
    std::string status = "idle";   // Status: "idle", "active", "loading", "computing"
    
    // System Metrics
    double ramUsageMB = 0.0;
    double cpuUsagePercent = 0.0;
    std::string backendName = "Standard";
};

class LatencyMonitor 
{

public:
    explicit LatencyMonitor( = nullptr);
    
    int ping() const;
    const LatencyStats& getStats() const { return m_stats; }
    void recordPing(int latencyMs);
    void setStatus(const std::string& status);
    void setBackend(const std::string& backend);
    void reset();
\npublic:\n    void pingUpdated(int latencyMs);
    void statsUpdated(const LatencyStats& stats);
\nprivate:\n    void onPingTimer();
    void updateSystemMetrics();

private:
    void updateStats();
    
    std::chrono::steady_clock::time_point m_timer;
    // Timer m_pingTimer;
    // Timer m_metricsTimer;
    LatencyStats m_stats;

    // CPU tracking
    long long m_lastCpuTime = 0;
    long long m_lastSysTime = 0;
};

} // namespace RawrXD




