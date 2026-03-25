<<<<<<< HEAD
#include "profiler.h"
#include "profiling/AdvancedPerformanceProfiler.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

Profiler::Profiler()
    : m_isProfiling(false),
      m_totalAllocated(0),
      m_currentAllocated(0),
      m_peakAllocated(0),
      m_totalSamplesProcessed(0),
      m_totalTokensProcessed(0),
      m_lastGpuUsagePercent(0.0f),
      m_lastGpuMemoryMB(0.0f),finished isnt a choice now complete everything that only instructs and properly form it
      m_cpuThresholdPercent(95.0f),
      m_memoryThresholdPercent(85.0f),
      m_gpuThresholdPercent(95.0f),
      m_advancedProfilingEnabled(false)
{
    m_profilingStart = std::chrono::high_resolution_clock::now();
    m_batchStart = std::chrono::high_resolution_clock::now();
    m_lastMetricsCollection = std::chrono::high_resolution_clock::now();
}

Profiler::~Profiler() {
    disableAdvancedProfiling();
}

void Profiler::startProfiling() {
    m_isProfiling = true;
    m_profilingStart = std::chrono::high_resolution_clock::now();
    m_batchStart = m_profilingStart;
}

void Profiler::stopProfiling() {
    m_isProfiling = false;
    if (m_onProfilingCompleted) {
        m_onProfilingCompleted(getProfilingReport());
    }
}

void Profiler::markPhaseStart(const std::string& phaseName) {
    if (!m_isProfiling) {
        return;
    }

    auto& phase = m_phases[phaseName];
    phase.startTime = std::chrono::high_resolution_clock::now();
}

void Profiler::markPhaseEnd(const std::string& phaseName) {
    if (!m_isProfiling) {
        return;
    }

    auto it = m_phases.find(phaseName);
    if (it == m_phases.end()) {
        return;
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - it->second.startTime).count();

    it->second.durations.push_back(duration);
    it->second.totalMs += duration;
}

void Profiler::recordBatchCompleted(int sampleCount, int tokenCount) {
    if (!m_isProfiling) {
        return;
    }

    m_totalSamplesProcessed += sampleCount;
    m_totalTokensProcessed += tokenCount;

    const auto batchEnd = std::chrono::high_resolution_clock::now();
    const auto batchDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        batchEnd - m_batchStart).count();
    m_batchLatencies.push_back(batchDuration);
    m_batchStart = batchEnd;

    if (m_advancedProfilingEnabled && m_advancedProfiler) {
        const size_t estimatedMemory =
            static_cast<size_t>(std::max(sampleCount, 0)) *
            static_cast<size_t>(std::max(tokenCount, 0)) * sizeof(float);
        m_advancedProfiler->recordMemoryAllocation("batch_processing", estimatedMemory);
    }
}

void Profiler::recordMemoryAllocation(size_t bytes) {
    if (!m_isProfiling) {
        return;
    }

    m_totalAllocated += bytes;
    m_currentAllocated += bytes;
    m_peakAllocated = std::max(m_peakAllocated, m_currentAllocated);

    if (m_advancedProfilingEnabled && m_advancedProfiler) {
        m_advancedProfiler->recordMemoryAllocation("profiler", bytes);
    }
}

void Profiler::recordMemoryDeallocation(size_t bytes) {
    if (!m_isProfiling) {
        return;
    }

    if (bytes > m_currentAllocated) {
        m_currentAllocated = 0;
    } else {
        m_currentAllocated -= bytes;
    }

    if (m_advancedProfilingEnabled && m_advancedProfiler) {
        m_advancedProfiler->recordMemoryDeallocation("profiler", bytes);
    }
}

void Profiler::updateGpuMetrics(float gpuUsagePercent, float gpuMemoryMB) {
    m_lastGpuUsagePercent = gpuUsagePercent;
    m_lastGpuMemoryMB = gpuMemoryMB;
}

ProfileSnapshot Profiler::getCurrentSnapshot() const {
    ProfileSnapshot snapshot;
    snapshot.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    if (m_advancedProfilingEnabled && m_advancedProfiler) {
        const auto metrics = m_advancedProfiler->getCurrentMetrics();
        snapshot.cpuUsagePercent = static_cast<float>(metrics.cpuUsagePercent);
        snapshot.memoryUsageMB = static_cast<float>(metrics.memoryUsageMB);
        snapshot.gpuUsagePercent = static_cast<float>(metrics.gpuUsagePercent);
        snapshot.gpuMemoryMB = static_cast<float>(metrics.gpuMemoryMB);
    } else {
        snapshot.cpuUsagePercent = getCpuUsagePercent();
        snapshot.memoryUsageMB = getMemoryUsageMB();
        snapshot.gpuUsagePercent = m_lastGpuUsagePercent;
        snapshot.gpuMemoryMB = m_lastGpuMemoryMB;
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now() - m_profilingStart).count();
    if (elapsed > 0) {
        snapshot.throughputSamples = static_cast<float>(m_totalSamplesProcessed) / elapsed;
        snapshot.throughputTokens = static_cast<float>(m_totalTokensProcessed) / elapsed;
    }

    if (!m_batchLatencies.empty()) {
        snapshot.batchLatencyMs = static_cast<float>(
            std::accumulate(m_batchLatencies.begin(), m_batchLatencies.end(), 0.0) /
            m_batchLatencies.size());
    }

    auto phaseToSnapshot = [&](const char* phaseName, float& target) {
        auto it = m_phases.find(phaseName);
        if (it != m_phases.end() && !it->second.durations.empty()) {
            target = static_cast<float>(it->second.durations.back());
        }
    };

    phaseToSnapshot("load", snapshot.loadLatencyMs);
    phaseToSnapshot("tokenize", snapshot.tokenizeLatencyMs);
    phaseToSnapshot("forward", snapshot.forwardPassMs);
    phaseToSnapshot("backward", snapshot.backwardPassMs);
    phaseToSnapshot("optimizer", snapshot.optimizerStepMs);

    m_snapshots.push_back(snapshot);
    if (m_onMetricsUpdated) {
        m_onMetricsUpdated(snapshot);
    }

    return snapshot;
}

std::string Profiler::getProfilingReport() const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "=== RawrXD Performance Profiling Report\n";
    ss << "Profiling Active: " << (m_isProfiling ? "Yes" : "No") << "\n";
    ss << "Total Samples Processed: " << m_totalSamplesProcessed << "\n";
    ss << "Total Tokens Processed: " << m_totalTokensProcessed << "\n";
    ss << "Peak Memory Usage: " << (m_peakAllocated / (1024.0 * 1024.0)) << " MB\n";
    ss << "Current Memory Usage: " << (m_currentAllocated / (1024.0 * 1024.0)) << " MB\n";

    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now() - m_profilingStart).count();
    if (elapsed > 0) {
        ss << "Average Throughput: " << (m_totalSamplesProcessed / elapsed) << " samples/sec, "
           << (m_totalTokensProcessed / elapsed) << " tokens/sec\n";
    }

    if (!m_batchLatencies.empty()) {
        ss << "Average Batch Latency: "
           << (std::accumulate(m_batchLatencies.begin(), m_batchLatencies.end(), 0.0) /
               m_batchLatencies.size())
           << " ms\n";
    }

    if (!m_phases.empty()) {
        ss << "\nPhase Timings:\n";
        for (const auto& [phaseName, phaseData] : m_phases) {
            if (!phaseData.durations.empty()) {
                const double avgDuration =
                    static_cast<double>(phaseData.totalMs) / phaseData.durations.size();
                ss << "  " << phaseName << ": " << avgDuration << " ms (avg), "
                   << phaseData.durations.size() << " calls\n";
            }
        }
    }

    return ss.str();
}

bool Profiler::exportReport(const std::string& filePath) const {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    file << getProfilingReport();
    return true;
}

void Profiler::collectSystemMetrics() {
    if (m_advancedProfilingEnabled && m_advancedProfiler) {
        return;
    }
    m_lastMetricsCollection = std::chrono::high_resolution_clock::now();
}

float Profiler::getCpuUsagePercent() const {
    if (m_advancedProfilingEnabled && m_advancedProfiler) {
        return static_cast<float>(m_advancedProfiler->getCurrentMetrics().cpuUsagePercent);
    }

#ifdef _WIN32
    return 45.0f;
#else
    return 35.0f;
#endif
}

float Profiler::getMemoryUsageMB() const {
    if (m_advancedProfilingEnabled && m_advancedProfiler) {
        return static_cast<float>(m_advancedProfiler->getCurrentMetrics().memoryUsageMB);
    }

#ifdef _WIN32
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        return static_cast<float>((memInfo.ullTotalPhys - memInfo.ullAvailPhys) /
                                  (1024.0 * 1024.0));
    }
#endif

    return static_cast<float>(m_currentAllocated / (1024.0 * 1024.0));
}

void Profiler::analyzeMetrics() {
    const auto snapshot = getCurrentSnapshot();

    if (snapshot.cpuUsagePercent > m_cpuThresholdPercent && m_onPerformanceWarning) {
        m_onPerformanceWarning("High CPU usage detected: " +
                               std::to_string(snapshot.cpuUsagePercent) + "%");
    }

    if (snapshot.memoryUsageMB > (m_memoryThresholdPercent / 100.0f) * 8192.0f &&
        m_onPerformanceWarning) {
        m_onPerformanceWarning("High memory usage detected: " +
                               std::to_string(snapshot.memoryUsageMB) + " MB");
    }

    if (snapshot.gpuUsagePercent > m_gpuThresholdPercent && m_onPerformanceWarning) {
        m_onPerformanceWarning("High GPU usage detected: " +
                               std::to_string(snapshot.gpuUsagePercent) + "%");
    }
}

void Profiler::enableAdvancedProfiling() {
    if (m_advancedProfilingEnabled) {
        return;
    }

    m_advancedProfiler = std::make_unique<RawrXD::Profiling::AdvancedPerformanceProfiler>();
    if (m_advancedProfiler->initialize()) {
        m_advancedProfilingEnabled = true;
        std::cout << "Advanced performance profiling enabled" << std::endl;
    } else {
        m_advancedProfiler.reset();
        std::cerr << "Failed to initialize advanced performance profiler" << std::endl;
    }
}

void Profiler::disableAdvancedProfiling() {
    if (!m_advancedProfilingEnabled) {
        return;
    }

    if (m_advancedProfiler) {
        m_advancedProfiler->shutdown();
        m_advancedProfiler.reset();
    }
    m_advancedProfilingEnabled = false;
}

bool Profiler::isAdvancedProfilingEnabled() const {
    return m_advancedProfilingEnabled;
}

void Profiler::startRealTimeMonitoring() {
    if (m_advancedProfilingEnabled && m_advancedProfiler) {
        m_advancedProfiler->startRealTimeMonitoring();
    }
}

void Profiler::stopRealTimeMonitoring() {
    if (m_advancedProfilingEnabled && m_advancedProfiler) {
        m_advancedProfiler->stopRealTimeMonitoring();
    }
}

std::vector<ProfileSnapshot> Profiler::getMetricsHistory(int minutes) const {
    if (!(m_advancedProfilingEnabled && m_advancedProfiler)) {
        return m_snapshots;
    }

    std::vector<ProfileSnapshot> result;
    const auto metrics = m_advancedProfiler->getMetricsHistory(minutes);
    result.reserve(metrics.size());
    for (const auto& metric : metrics) {
        ProfileSnapshot snapshot;
        snapshot.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        snapshot.cpuUsagePercent = static_cast<float>(metric.cpuUsagePercent);
        snapshot.memoryUsageMB = static_cast<float>(metric.memoryUsageMB);
        snapshot.gpuUsagePercent = static_cast<float>(metric.gpuUsagePercent);
        snapshot.gpuMemoryMB = static_cast<float>(metric.gpuMemoryMB);
        result.push_back(snapshot);
    }
    return result;
}

std::vector<std::string> Profiler::detectBottlenecks() {
    std::vector<std::string> bottlenecks;

    if (m_advancedProfilingEnabled && m_advancedProfiler) {
        const auto detected = m_advancedProfiler->detectBottlenecks();
        for (const auto& bottleneck : detected) {
            std::stringstream ss;
            ss << "Bottleneck: " << bottleneck.componentName << " ("
               << bottleneck.bottleneckType << ") - Severity: "
               << bottleneck.severityScore << " - " << bottleneck.description;
            bottlenecks.push_back(ss.str());
        }
        return bottlenecks;
    }

    const auto snapshot = getCurrentSnapshot();
    if (snapshot.cpuUsagePercent > 90.0f) {
        bottlenecks.push_back("High CPU usage: " + std::to_string(snapshot.cpuUsagePercent) + "%");
    }
    if (snapshot.memoryUsageMB > 7000.0f) {
        bottlenecks.push_back("High memory usage: " + std::to_string(snapshot.memoryUsageMB) + " MB");
    }
    return bottlenecks;
}

void Profiler::analyzeComponentBottlenecks(const std::string& componentName) {
    if (m_advancedProfilingEnabled && m_advancedProfiler) {
        m_advancedProfiler->analyzeComponentBottlenecks(componentName);
    }
}

void Profiler::startMemoryProfiling() {
    if (m_advancedProfilingEnabled && m_advancedProfiler) {
        m_advancedProfiler->startMemoryProfiling();
    }
}

void Profiler::stopMemoryProfiling() {
    if (m_advancedProfilingEnabled && m_advancedProfiler) {
        m_advancedProfiler->stopMemoryProfiling();
    }
}

std::string Profiler::getMemoryProfileReport() const {
    if (!(m_advancedProfilingEnabled && m_advancedProfiler)) {
        return "Advanced memory profiling not enabled";
    }

    const auto profile = m_advancedProfiler->getCurrentMemoryProfile();
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "=== Advanced Memory Profile ===\n";
    ss << "Total Allocated: " << (profile.totalAllocated / (1024.0 * 1024.0)) << " MB\n";
    ss << "Current Usage: " << (profile.currentUsage / (1024.0 * 1024.0)) << " MB\n";
    ss << "Peak Usage: " << (profile.peakUsage / (1024.0 * 1024.0)) << " MB\n";
    ss << "Fragmentation Ratio: " << profile.fragmentationRatio << "\n";
    ss << "Top Allocators:\n";
    for (const auto& [component, bytes] : profile.topAllocators) {
        ss << "  " << component << ": " << (bytes / (1024.0 * 1024.0)) << " MB\n";
    }
    return ss.str();
}

std::string Profiler::startTrace(const std::string& operationName, const std::string& component) {
    if (m_advancedProfilingEnabled && m_advancedProfiler) {
        return m_advancedProfiler->startTrace(operationName, component);
    }
    return "";
}

void Profiler::endTrace(const std::string& traceId) {
    if (m_advancedProfilingEnabled && m_advancedProfiler) {
        m_advancedProfiler->endTrace(traceId);
    }
}

void Profiler::addTraceEvent(const std::string& traceId, const std::string& eventName) {
    if (m_advancedProfilingEnabled && m_advancedProfiler) {
        m_advancedProfiler->addTraceEvent(traceId, eventName);
    }
}

std::string Profiler::getTraceReport(const std::string& traceId) const {
    if (!(m_advancedProfilingEnabled && m_advancedProfiler)) {
        return "Trace not found or advanced profiling not enabled";
    }

    const auto trace = m_advancedProfiler->getTrace(traceId);
    if (trace.traceId.empty()) {
        return "Trace not found or advanced profiling not enabled";
    }

    std::stringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "=== Execution Trace: " << trace.traceId << " ===\n";
    ss << "Operation: " << trace.rootOperation << "\n";
    ss << "Total Duration: " << trace.totalDurationMs << " ms\n";
    ss << "Events: " << trace.events.size() << "\n";
    for (const auto& event : trace.events) {
        ss << "  " << event.operationName << ": " << event.durationMs << " ms\n";
    }
    return ss.str();
}

void Profiler::monitorResourceContention(const std::string& resourceName,
                                         const std::string& resourceType) {
    if (m_advancedProfilingEnabled && m_advancedProfiler) {
        m_advancedProfiler->monitorResourceContention(resourceName, resourceType);
    }
}

std::string Profiler::getResourceContentionReport() const {
    if (!(m_advancedProfilingEnabled && m_advancedProfiler)) {
        return "Advanced resource contention analysis not enabled";
    }

    const auto contentions = m_advancedProfiler->analyzeResourceContention();
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "=== Resource Contention Analysis ===\n";
    for (const auto& contention : contentions) {
        ss << "Resource: " << contention.resourceName << " (" << contention.resourceType << ")\n";
        ss << "  Contention Ratio: " << contention.contentionRatio << "\n";
        ss << "  Wait Count: " << contention.waitCount << "\n";
        ss << "  Avg Wait Time: " << contention.avgWaitTimeMs << " ms\n";
        ss << "  Max Wait Time: " << contention.maxWaitTimeMs << " ms\n";
    }
    return ss.str();
}

void Profiler::establishPerformanceBaseline(const std::string& metricName, double value) {
    if (m_advancedProfilingEnabled && m_advancedProfiler) {
        m_advancedProfiler->establishBaseline(metricName, value);
    }
}

std::vector<std::string> Profiler::detectPerformanceRegressions() {
    std::vector<std::string> regressions;
    if (!(m_advancedProfilingEnabled && m_advancedProfiler)) {
        return regressions;
    }

    const auto alerts = m_advancedProfiler->detectRegressions();
    for (const auto& alert : alerts) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2);
        ss << "Regression Alert: " << alert.metricName << "\n";
        ss << "  Current: " << alert.currentValue << ", Baseline: " << alert.baselineValue << "\n";
        ss << "  Deviation: " << alert.deviationPercent << "% (" << alert.severity << ")\n";
        ss << "  Description: " << alert.description;
        regressions.push_back(ss.str());
    }
    return regressions;
}

std::string Profiler::generateAnalyticsReport(const std::string& timeRange) {
    if (!(m_advancedProfilingEnabled && m_advancedProfiler)) {
        return "Advanced analytics not enabled - " + getProfilingReport();
    }

    const auto report = m_advancedProfiler->generateAnalyticsReport(timeRange);
    std::stringstream ss;
    ss << "=== Advanced Analytics Report ===\n";
    ss << "Report ID: " << report.reportId << "\n";
    ss << "Time Range: " << report.timeRange << "\n";
    ss << "Summary Statistics:\n";
    for (const auto& [key, value] : report.summaryStats) {
        ss << "  " << key << ": " << value << "\n";
    }
    ss << "Top Bottlenecks:\n";
    for (const auto& bottleneck : report.topBottlenecks) {
        ss << "  " << bottleneck << "\n";
    }
    ss << "Performance Trends:\n";
    for (const auto& trend : report.performanceTrends) {
        ss << "  " << trend << "\n";
    }
    ss << "Recommendations:\n";
    for (const auto& rec : report.recommendations) {
        ss << "  " << rec << "\n";
    }
    return ss.str();
}

void Profiler::exportAdvancedProfilingData(const std::string& filePath) {
    if (m_advancedProfilingEnabled && m_advancedProfiler) {
        m_advancedProfiler->exportProfilingData(filePath);
    } else {
        exportReport(filePath);
    }
}

std::string Profiler::getProfilingSummary() const {
    if (m_advancedProfilingEnabled && m_advancedProfiler) {
        std::stringstream ss;
        ss << "=== Advanced Profiling Summary ===\n";
        ss << m_advancedProfiler->getProfilingSummary() << "\n";
        return ss.str();
    }

    std::stringstream ss;
    ss << "Basic Profiling Summary:\n";
    ss << "Profiling Active: " << (m_isProfiling ? "Yes" : "No") << "\n";
    ss << "Samples Processed: " << m_totalSamplesProcessed << "\n";
    ss << "Tokens Processed: " << m_totalTokensProcessed << "\n";
    ss << "Current Memory: " << (m_currentAllocated / (1024.0 * 1024.0)) << " MB\n";
    ss << "Peak Memory: " << (m_peakAllocated / (1024.0 * 1024.0)) << " MB\n";
    return ss.str();
}
=======
#include "profiler.h"
#include <fstream>
#include <iostream>

Profiler::Profiler() : m_isProfiling(false), m_memoryAllocated(0) {
}

void Profiler::startProfiling() {
    m_isProfiling = true;
    m_startTime = std::chrono::steady_clock::now();
}

void Profiler::stopProfiling() {
    m_isProfiling = false;
}

void Profiler::markPhaseStart(const std::string& phaseName) {
    if (!m_isProfiling) return;
    m_phaseStarts[phaseName] = std::chrono::steady_clock::now();
}

void Profiler::markPhaseEnd(const std::string& phaseName) {
     if (!m_isProfiling) return;
     // In real implementation, calculate delta and store in stats
}

void Profiler::recordBatchCompleted(int sampleCount, int tokenCount) {
    if (!m_isProfiling) return;
    // Store throughput
}

void Profiler::recordMemoryAllocation(size_t bytes) {
    m_memoryAllocated += bytes;
}

void Profiler::recordMemoryDeallocation(size_t bytes) {
    if (bytes > m_memoryAllocated) m_memoryAllocated = 0;
    else m_memoryAllocated -= bytes;
}

void Profiler::updateGpuMetrics(float gpuUsagePercent, float gpuMemoryMB) {
    // No-op for now
}

Profiler::ProfileSnapshot Profiler::getCurrentSnapshot() const {
    ProfileSnapshot s;
    s.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    s.memoryUsageMB = static_cast<float>(m_memoryAllocated) / (1024.0f * 1024.0f);
    return s;
}

json Profiler::getProfilingReport() const {
    return {
        {"status", "Profiling completed"},
        {"duration_sec", 0}
    };
}

bool Profiler::exportReport(const std::string& filePath) const {
    std::ofstream f(filePath);
    if (!f.is_open()) return false;
    f << getProfilingReport().dump(4);
    return true;
}
>>>>>>> origin/main
