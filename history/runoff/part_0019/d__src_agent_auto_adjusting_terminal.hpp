#pragma once
// ============================================================================
// Auto-Adjusting Terminal Timeout Manager
// Dynamically adjusts PowerShell terminal execution timeouts based on:
// - Task complexity
// - Historical execution time
// - System load
// - Quality requirements
// ============================================================================

#include <cstdint>
#include <atomic>
#include <mutex>
#include <vector>
#include <chrono>

namespace rawrxd::agent {

// ============================================================================
// Timeout Strategy
// ============================================================================
enum class TimeoutStrategy : uint8_t {
    FIXED = 0,         // Fixed timeout (user-specified)
    ADAPTIVE = 1,      // Learns from history
    COMPLEXITY = 2,    // Based on task complexity
    HYBRID = 3         // Combines adaptive + complexity
};

// ============================================================================
// Execution History Entry
// ============================================================================
struct ExecutionHistory {
    uint64_t timeoutMs;
    uint64_t actualDurationMs;
    bool timedOut;
    float taskComplexity;
    std::chrono::steady_clock::time_point timestamp;
};

// ============================================================================
// Timeout Prediction
// ============================================================================
struct TimeoutPrediction {
    uint64_t recommendedTimeoutMs;
    uint64_t minTimeoutMs;
    uint64_t maxTimeoutMs;
    float confidence;           // 0.0-1.0
    const char* reasoning;
};

// ============================================================================
// Terminal Statistics
// ============================================================================
struct TerminalStats {
    uint64_t totalExecutions;
    uint64_t timeouts;
    uint64_t successful;
    uint64_t avgActualDurationMs;
    uint64_t avgTimeoutMs;
    float timeoutRate;
    float avgUtilization;       // actualDuration / timeout
};

// ============================================================================
// AutoAdjustingTerminal
// ============================================================================
class AutoAdjustingTerminal {
public:
    AutoAdjustingTerminal();
    ~AutoAdjustingTerminal();
    
    // Lifecycle
    bool initialize();
    void shutdown();
    bool isInitialized() const { return m_initialized; }
    
    // Configuration
    void setStrategy(TimeoutStrategy strategy);
    TimeoutStrategy getStrategy() const { return m_strategy; }
    
    void setBaseTimeoutMs(uint64_t ms);
    uint64_t getBaseTimeoutMs() const { return m_baseTimeoutMs; }
    
    void setMinTimeoutMs(uint64_t ms);
    uint64_t getMinTimeoutMs() const { return m_minTimeoutMs; }
    
    void setMaxTimeoutMs(uint64_t ms);
    uint64_t getMaxTimeoutMs() const { return m_maxTimeoutMs; }
    
    void setLearningEnabled(bool enabled);
    bool isLearningEnabled() const { return m_learningEnabled; }
    
    void setAdjustmentAggressiveness(float factor); // 0.0-1.0
    float getAdjustmentAggressiveness() const { return m_aggressiveness; }
    
    // Timeout Prediction
    TimeoutPrediction predictTimeout(
        const char* command,
        float taskComplexity = 0.5f
    );
    
    // Execution Recording
    void recordExecution(
        const char* command,
        uint64_t timeoutMs,
        uint64_t actualDurationMs,
        bool timedOut,
        float taskComplexity
    );
    
    // Statistics
    TerminalStats getStatistics() const;
    void resetStatistics();
    
    // History Management
    void clearHistory();
    size_t getHistorySize() const;
    
private:
    // Prediction Algorithms
    uint64_t predictFixed();
    uint64_t predictAdaptive(const char* command);
    uint64_t predictComplexity(float complexity);
    uint64_t predictHybrid(const char* command, float complexity);
    
    // Analysis
    float analyzeCommandComplexity(const char* command);
    float calculateUtilizationTrend();
    float calculateTimeoutSuccessRate();
    
    // Adjustment
    uint64_t adjustBasedOnHistory(uint64_t baseTimeout);
    uint64_t clampTimeout(uint64_t timeout);
    
    // State
    std::atomic<bool> m_initialized;
    std::atomic<bool> m_learningEnabled;
    std::atomic<uint64_t> m_baseTimeoutMs;
    std::atomic<uint64_t> m_minTimeoutMs;
    std::atomic<uint64_t> m_maxTimeoutMs;
    
    TimeoutStrategy m_strategy;
    float m_aggressiveness;
    
    // History
    mutable std::mutex m_historyMutex;
    std::vector<ExecutionHistory> m_history;
    const size_t MAX_HISTORY = 1000;
    
    // Statistics
    mutable std::mutex m_statsMutex;
    TerminalStats m_stats;
};

} // namespace rawrxd::agent
