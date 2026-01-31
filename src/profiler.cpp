#include "profiler.h"


#include <windows.h>
#include <psapi.h>
#include <thread>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "kernel32.lib")

Profiler::Profiler(void* parent)
    : void(parent)
    , m_metricsTimer(new void*(this))
{
    // Set up periodic metrics collection every 500ms
// Qt connect removed
    m_metricsTimer->setInterval(500); // 500ms intervals
}

void Profiler::startProfiling()
{
    if (m_isProfiling) {
        return;
    }

    m_isProfiling = true;
    m_profilingStart = std::chrono::high_resolution_clock::now();
    m_lastMetricsCollection = m_profilingStart;
    
    m_totalSamplesProcessed = 0;
    m_totalTokensProcessed = 0;
    m_totalAllocated = 0;
    m_currentAllocated = 0;
    m_peakAllocated = 0;
    m_phases.clear();
    m_batchLatencies.clear();
    m_snapshots.clear();

    m_metricsTimer->start();
}

void Profiler::stopProfiling()
{
    if (!m_isProfiling) {
        return;
    }

    m_metricsTimer->stop();
    m_isProfiling = false;

    analyzeMetrics();
    
    void* report = getProfilingReport();
    profilingCompleted(report);
    
}

void Profiler::markPhaseStart(const std::string& phaseName)
{
    if (!m_isProfiling) return;
    
    m_phases[phaseName].startTime = std::chrono::high_resolution_clock::now();
}

void Profiler::markPhaseEnd(const std::string& phaseName)
{
    if (!m_isProfiling) return;
    
    auto it = m_phases.find(phaseName);
    if (it == m_phases.end()) {
        return;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - it->startTime).count();
    
    it->durations.push_back(duration);
    it->totalMs += duration;

}

void Profiler::recordBatchCompleted(int sampleCount, int tokenCount)
{
    if (!m_isProfiling) return;
    
    auto now = std::chrono::high_resolution_clock::now();
    auto batchDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_batchStart).count();
    
    m_batchLatencies.push_back(batchDuration);
    m_totalSamplesProcessed += sampleCount;
    m_totalTokensProcessed += tokenCount;
    
    m_batchStart = now;
}

void Profiler::recordMemoryAllocation(size_t bytes)
{
    if (!m_isProfiling) return;
    
    m_totalAllocated += bytes;
    m_currentAllocated += bytes;
    if (m_currentAllocated > m_peakAllocated) {
        m_peakAllocated = m_currentAllocated;
    }
}

void Profiler::recordMemoryDeallocation(size_t bytes)
{
    if (!m_isProfiling) return;
    
    if (m_currentAllocated >= bytes) {
        m_currentAllocated -= bytes;
    } else {
        m_currentAllocated = 0;
    }
}

void Profiler::updateGpuMetrics(float gpuUsagePercent, float gpuMemoryMB)
{
    m_lastGpuUsagePercent = gpuUsagePercent;
    m_lastGpuMemoryMB = gpuMemoryMB;
}

float Profiler::getCpuUsagePercent() const
{
    // Windows-specific CPU usage calculation
    static HANDLE hProcess = GetCurrentProcess();
    static FILETIME prevUser = {}, prevKernel = {};
    FILETIME user = {}, kernel = {}, dummy1 = {}, dummy2 = {};

    if (!GetProcessTimes(hProcess, &dummy1, &dummy2, &kernel, &user)) {
        return 0.0f;
    }

    // Convert FILETIME to 64-bit
    auto toInt64 = [](const FILETIME& ft) {
        ULARGE_INTEGER li;
        li.LowPart = ft.dwLowDateTime;
        li.HighPart = ft.dwHighDateTime;
        return li.QuadPart;
    };

    uint64_t userTime = toInt64(user);
    uint64_t kernelTime = toInt64(kernel);
    uint64_t prevUserTime = toInt64(prevUser);
    uint64_t prevKernelTime = toInt64(prevKernel);

    // Calculate deltas
    uint64_t userDelta = userTime - prevUserTime;
    uint64_t kernelDelta = kernelTime - prevKernelTime;

    prevUser = user;
    prevKernel = kernel;

    // Convert to percentage (approx for single core)
    float cpuPercent = static_cast<float>(userDelta + kernelDelta) / 10000.0f / 100.0f;
    return std::min(cpuPercent, 100.0f);
}

float Profiler::getMemoryUsageMB() const
{
    // Windows-specific memory usage
    HANDLE hProcess = GetCurrentProcess();
    PROCESS_MEMORY_COUNTERS pmc = {};

    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize / (1024.0f * 1024.0f);
    }

    return 0.0f;
}

void Profiler::collectSystemMetrics()
{
    if (!m_isProfiling) return;

    ProfileSnapshot snapshot;
    snapshot.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    snapshot.cpuUsagePercent = getCpuUsagePercent();
    snapshot.memoryUsageMB = getMemoryUsageMB();
    snapshot.gpuUsagePercent = m_lastGpuUsagePercent;
    snapshot.gpuMemoryMB = m_lastGpuMemoryMB;

    // Calculate throughput metrics
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now() - m_profilingStart).count();
    
    if (elapsed > 0) {
        snapshot.throughputSamples = static_cast<float>(m_totalSamplesProcessed) / elapsed;
        snapshot.throughputTokens = static_cast<float>(m_totalTokensProcessed) / elapsed;
    }

    // Calculate batch latency
    if (!m_batchLatencies.empty()) {
        int64_t totalLatency = 0;
        for (auto lat : m_batchLatencies) {
            totalLatency += lat;
        }
        snapshot.batchLatencyMs = static_cast<float>(totalLatency) / m_batchLatencies.size();
    }

    // Phase latencies
    auto getAveragePhaseTime = [this](const std::string& phase) {
        auto it = m_phases.find(phase);
        if (it != m_phases.end() && !it->durations.empty()) {
            return static_cast<float>(it->totalMs) / it->durations.size();
        }
        return 0.0f;
    };

    snapshot.loadLatencyMs = getAveragePhaseTime("loadDataset");
    snapshot.tokenizeLatencyMs = getAveragePhaseTime("tokenizeDataset");
    snapshot.forwardPassMs = getAveragePhaseTime("forwardPass");
    snapshot.backwardPassMs = getAveragePhaseTime("backwardPass");
    snapshot.optimizerStepMs = getAveragePhaseTime("optimizerStep");

    m_snapshots.push_back(snapshot);
    metricsUpdated(snapshot);

    // Check for performance issues
    if (snapshot.cpuUsagePercent > m_cpuThresholdPercent) {
        performanceWarning(
            std::string("High CPU usage: %1%"));
    }

    if (snapshot.memoryUsageMB > 4096.0f) { // 4GB threshold
        performanceWarning(
            std::string("High memory usage: %1 MB"));
    }

    if (snapshot.gpuUsagePercent > m_gpuThresholdPercent && snapshot.gpuUsagePercent > 0) {
        performanceWarning(
            std::string("High GPU usage: %1%"));
    }
}

Profiler::ProfileSnapshot Profiler::getCurrentSnapshot() const
{
    if (m_snapshots.empty()) {
        return ProfileSnapshot();
    }
    return m_snapshots.back();
}

void* Profiler::getProfilingReport() const
{
    void* report;
    
    // Overall stats
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - m_profilingStart).count();
    report["elapsedTimeMs"] = static_cast<int>(elapsed);
    report["totalSamplesProcessed"] = m_totalSamplesProcessed;
    report["totalTokensProcessed"] = m_totalTokensProcessed;
    report["peakMemoryMB"] = static_cast<double>(m_peakAllocated / (1024.0 * 1024.0));

    // Phase statistics
    void* phaseStats;
    for (auto it = m_phases.begin(); it != m_phases.end(); ++it) {
        void* phaseData;
        phaseData["totalMs"] = static_cast<int>(it->totalMs);
        phaseData["callCount"] = static_cast<int>(it->durations.size());
        
        if (!it->durations.empty()) {
            int64_t minDuration = *std::min_element(it->durations.begin(), it->durations.end());
            int64_t maxDuration = *std::max_element(it->durations.begin(), it->durations.end());
            
            phaseData["minMs"] = static_cast<int>(minDuration);
            phaseData["maxMs"] = static_cast<int>(maxDuration);
            phaseData["avgMs"] = static_cast<int>(it->totalMs / it->durations.size());
        }
        
        phaseStats[it.key()] = phaseData;
    }
    report["phases"] = phaseStats;

    // Throughput statistics
    void* throughput;
    if (elapsed > 0) {
        throughput["samplesPerSecond"] = static_cast<double>(m_totalSamplesProcessed) * 1000.0 / elapsed;
        throughput["tokensPerSecond"] = static_cast<double>(m_totalTokensProcessed) * 1000.0 / elapsed;
    }
    report["throughput"] = throughput;

    // Latency percentiles for batches
    if (!m_batchLatencies.empty()) {
        std::vector<int64_t> sorted = m_batchLatencies;
        std::sort(sorted.begin(), sorted.end());
        
        void* latency;
        latency["p50"] = static_cast<int>(sorted[sorted.size() / 2]);
        latency["p95"] = static_cast<int>(sorted[static_cast<size_t>(sorted.size() * 0.95)]);
        latency["p99"] = static_cast<int>(sorted[static_cast<size_t>(sorted.size() * 0.99)]);
        latency["min"] = static_cast<int>(sorted.front());
        latency["max"] = static_cast<int>(sorted.back());
        
        report["batchLatencyMs"] = latency;
    }

    // Metrics snapshots (last 100)
    void* snapshots;
    size_t start = m_snapshots.size() > 100 ? m_snapshots.size() - 100 : 0;
    for (size_t i = start; i < m_snapshots.size(); ++i) {
        void* snap;
        snap["timestamp"] = static_cast<int64_t>(m_snapshots[i].timestamp);
        snap["cpuPercent"] = static_cast<double>(m_snapshots[i].cpuUsagePercent);
        snap["memoryMB"] = static_cast<double>(m_snapshots[i].memoryUsageMB);
        snap["gpuPercent"] = static_cast<double>(m_snapshots[i].gpuUsagePercent);
        snap["gpuMemoryMB"] = static_cast<double>(m_snapshots[i].gpuMemoryMB);
        snap["throughputSamples"] = static_cast<double>(m_snapshots[i].throughputSamples);
        snap["throughputTokens"] = static_cast<double>(m_snapshots[i].throughputTokens);
        
        snapshots.append(snap);
    }
    report["metricsSnapshots"] = snapshots;

    return report;
}

bool Profiler::exportReport(const std::string& filePath) const
{
    void* report = getProfilingReport();
    void* doc(report);

    std::fstream file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    file.write(doc.toJson());
    file.close();

    return true;
}

void Profiler::analyzeMetrics()
{
    // Check for anomalies and warnings
    if (!m_snapshots.empty()) {
        float avgCpu = 0.0f, avgMemory = 0.0f, avgGpu = 0.0f;
        
        for (const auto& snap : m_snapshots) {
            avgCpu += snap.cpuUsagePercent;
            avgMemory += snap.memoryUsageMB;
            avgGpu += snap.gpuUsagePercent;
        }
        
        avgCpu /= m_snapshots.size();
        avgMemory /= m_snapshots.size();
        avgGpu /= m_snapshots.size();
        
    }
}


