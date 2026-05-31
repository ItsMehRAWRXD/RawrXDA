// ============================================================================
// Real-Time Performance Profiler — Inference Performance Monitoring
// Continuous profiling of LLM inference with historical tracking
// ============================================================================
#pragma once
#include "../inference/RawrXD_LlamaNative.h"
#include "../core/settings_persistence.h"
#include <memory>
#include <vector>
#include <map>
#include <chrono>
#include <atomic>
#include <thread>

namespace RawrXD::Performance {

struct PerformanceMetrics {
    double inferenceTimeMs;
    double tokensPerSecond;
    double memoryUsageMB;
    double cpuUtilization;
    double gpuUtilization;
    std::map<std::string, double> layerTimings;
    int promptTokens;
    int completionTokens;
    std::chrono::system_clock::time_point timestamp;
    std::string modelName;
    std::string backendType;
};

struct ProfilingSession {
    std::string sessionId;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    std::vector<PerformanceMetrics> metrics;
    double avgTokensPerSecond;
    double peakMemoryUsageMB;
    int totalInferences;
};

class RealtimeProfiler {
public:
    RealtimeProfiler(std::shared_ptr<RawrXD::Inference::LlamaNativeBridge> inferenceEngine,
                    std::shared_ptr<RawrXD::Core::SettingsPersistence> settings)
        : m_inferenceEngine(inferenceEngine)
        , m_settings(settings)
        , m_isProfiling(false)
        , m_currentSession(std::make_shared<ProfilingSession>()) {
        LoadSettings();
    }

    PerformanceMetrics ProfileInference(const std::string& prompt) {
        PerformanceMetrics metrics;
        metrics.timestamp = std::chrono::system_clock::now();
        
        // Capture baseline metrics
        auto baselineMemory = GetMemoryUsage();
        auto baselineCpu = GetCPUUtilization();
        
        // Time the inference
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Run inference
        auto result = m_inferenceEngine->Generate(prompt, 1024, 0.7f, 0.9f, 40);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double, std::milli>(endTime - startTime);
        
        // Calculate metrics
        metrics.inferenceTimeMs = duration.count();
        metrics.tokensPerSecond = result.tokens_generated > 0 
            ? (result.tokens_generated * 1000.0) / duration.count()
            : 0.0;
        metrics.memoryUsageMB = GetMemoryUsage() - baselineMemory;
        metrics.cpuUtilization = GetCPUUtilization();
        metrics.gpuUtilization = GetGPUUtilization();
        metrics.promptTokens = result.prompt_tokens;
        metrics.completionTokens = result.tokens_generated;
        
        // Get layer timings if available
        metrics.layerTimings = GetLayerTimings();
        
        // Store metrics
        if (m_isProfiling) {
            m_currentSession->metrics.push_back(metrics);
            UpdateSessionStats();
        }
        
        m_historicalMetrics.push_back(metrics);
        TrimHistoricalData();
        
        return metrics;
    }

    void StartContinuousProfiling() {
        if (m_isProfiling) return;
        
        m_isProfiling = true;
        m_currentSession = std::make_shared<ProfilingSession>();
        m_currentSession->sessionId = GenerateSessionId();
        m_currentSession->startTime = std::chrono::system_clock::now();
        
        // Start background profiling thread
        m_profilingThread = std::thread([this]() {
            while (m_isProfiling) {
                CollectSystemMetrics();
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        });
    }

    void StopContinuousProfiling() {
        if (!m_isProfiling) return;
        
        m_isProfiling = false;
        
        if (m_profilingThread.joinable()) {
            m_profilingThread.join();
        }
        
        m_currentSession->endTime = std::chrono::system_clock::now();
        FinalizeSession();
    }

    std::vector<PerformanceMetrics> GetHistoricalMetrics(int limit = 100) const {
        std::vector<PerformanceMetrics> recent;
        int start = std::max(0, static_cast<int>(m_historicalMetrics.size()) - limit);
        for (size_t i = start; i < m_historicalMetrics.size(); ++i) {
            recent.push_back(m_historicalMetrics[i]);
        }
        return recent;
    }

    ProfilingSession GetCurrentSession() const {
        return *m_currentSession;
    }

    std::vector<ProfilingSession> GetPastSessions(int limit = 10) const {
        std::vector<ProfilingSession> recent;
        int start = std::max(0, static_cast<int>(m_pastSessions.size()) - limit);
        for (size_t i = start; i < m_pastSessions.size(); ++i) {
            recent.push_back(*m_pastSessions[i]);
        }
        return recent;
    }

    PerformanceMetrics GetAverageMetrics(const std::vector<PerformanceMetrics>& metrics) const {
        if (metrics.empty()) return {};
        
        PerformanceMetrics avg;
        double totalTokensPerSec = 0.0;
        double totalMemory = 0.0;
        double totalCpu = 0.0;
        double totalGpu = 0.0;
        
        for (const auto& m : metrics) {
            totalTokensPerSec += m.tokensPerSecond;
            totalMemory += m.memoryUsageMB;
            totalCpu += m.cpuUtilization;
            totalGpu += m.gpuUtilization;
        }
        
        avg.tokensPerSecond = totalTokensPerSec / metrics.size();
        avg.memoryUsageMB = totalMemory / metrics.size();
        avg.cpuUtilization = totalCpu / metrics.size();
        avg.gpuUtilization = totalGpu / metrics.size();
        
        return avg;
    }

    void ExportMetrics(const std::string& filePath) const {
        std::ofstream file(filePath);
        file << "timestamp,inference_time_ms,tokens_per_sec,memory_mb,cpu_pct,gpu_pct\n";
        
        for (const auto& m : m_historicalMetrics) {
            auto time = std::chrono::system_clock::to_time_t(m.timestamp);
            file << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << ","
                 << m.inferenceTimeMs << ","
                 << m.tokensPerSecond << ","
                 << m.memoryUsageMB << ","
                 << m.cpuUtilization << ","
                 << m.gpuUtilization << "\n";
        }
    }

private:
    std::shared_ptr<RawrXD::Inference::LlamaNativeBridge> m_inferenceEngine;
    std::shared_ptr<RawrXD::Core::SettingsPersistence> m_settings;
    std::atomic<bool> m_isProfiling;
    std::shared_ptr<ProfilingSession> m_currentSession;
    std::vector<std::shared_ptr<ProfilingSession>> m_pastSessions;
    std::vector<PerformanceMetrics> m_historicalMetrics;
    std::thread m_profilingThread;
    
    static constexpr size_t MAX_HISTORY_SIZE = 10000;

    void LoadSettings() {
        // Load profiling settings from persistence
    }

    double GetMemoryUsage() const {
        // Platform-specific memory usage
        #ifdef _WIN32
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            return pmc.WorkingSetSize / (1024.0 * 1024.0);
        }
        #endif
        return 0.0;
    }

    double GetCPUUtilization() const {
        // Platform-specific CPU utilization
        return 0.0; // Placeholder
    }

    double GetGPUUtilization() const {
        // Platform-specific GPU utilization
        return 0.0; // Placeholder
    }

    std::map<std::string, double> GetLayerTimings() const {
        // Get per-layer timing from inference engine
        return {};
    }

    void CollectSystemMetrics() {
        // Collect system-level metrics during profiling
    }

    void UpdateSessionStats() {
        if (m_currentSession->metrics.empty()) return;
        
        double totalTokensPerSec = 0.0;
        double peakMemory = 0.0;
        
        for (const auto& m : m_currentSession->metrics) {
            totalTokensPerSec += m.tokensPerSecond;
            peakMemory = std::max(peakMemory, m.memoryUsageMB);
        }
        
        m_currentSession->avgTokensPerSecond = totalTokensPerSec / m_currentSession->metrics.size();
        m_currentSession->peakMemoryUsageMB = peakMemory;
        m_currentSession->totalInferences = static_cast<int>(m_currentSession->metrics.size());
    }

    void FinalizeSession() {
        m_pastSessions.push_back(m_currentSession);
        
        // Trim old sessions
        if (m_pastSessions.size() > 100) {
            m_pastSessions.erase(m_pastSessions.begin());
        }
    }

    void TrimHistoricalData() {
        if (m_historicalMetrics.size() > MAX_HISTORY_SIZE) {
            m_historicalMetrics.erase(m_historicalMetrics.begin(),
                                     m_historicalMetrics.begin() + (m_historicalMetrics.size() - MAX_HISTORY_SIZE));
        }
    }

    std::string GenerateSessionId() const {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << "session_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
        return oss.str();
    }
};

} // namespace RawrXD::Performance
