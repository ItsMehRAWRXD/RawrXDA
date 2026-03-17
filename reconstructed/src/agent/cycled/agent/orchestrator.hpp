#pragma once
// ============================================================================
// Cycled Agent Orchestrator — 1x-99x Model Coordination
// Part of RawrXD Advanced Agentic Framework
// Supports: agent/plan/debug/ask modes, Auto/Max, Multi-Model cycling
// ============================================================================

#include <cstdint>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <functional>
#include <chrono>

namespace rawrxd::agent {

// ============================================================================
// Agent Mode (matches Cursor interface)
// ============================================================================
enum class AgentMode : uint8_t {
    AGENT = 0,    // Direct execution
    PLAN = 1,     // Planning phase
    DEBUG = 2,    // Debug mode
    ASK = 3       // Query/consultation
};

// ============================================================================
// Quality Mode
// ============================================================================
enum class QualityMode : uint8_t {
    AUTO = 0,      // Balance quality and speed
    MAX = 1,       // Maximum quality, no speed constraint
    SPEED = 2,     // Prioritize speed
    QUANTUM = 3    // Ultimate precision, maximum compression
};

// ============================================================================
// Agent Cycle Statistics
// ============================================================================
struct CycleStats {
    uint64_t totalCycles;
    uint64_t successfulCycles;
    uint64_t failedCycles;
    uint64_t averageIterations;
    uint64_t totalIterations;
    double averageDurationMs;
    double totalDurationMs;
    uint32_t peakModelCount;
    uint32_t currentActiveModels;
};

// ============================================================================
// Task Complexity Assessment
// ============================================================================
struct ComplexityMetrics {
    float codebaseSize;      // 0.0-1.0 normalized
    float dependencyDepth;   // 0.0-1.0 normalized
    float algorithmicComplexity; // 0.0-1.0 normalized
    float domainSpecificity; // 0.0-1.0 normalized
    float overallScore;      // Weighted average
    
    uint32_t predictedIterations;
    uint32_t predictedModelCount;
    uint64_t estimatedTimeMs;
};

// ============================================================================
// Agent Cycle Result
// ============================================================================
struct CycleResult {
    bool success;
    const char* detail;
    uint32_t iterationsUsed;
    uint32_t modelsUsed;
    uint64_t durationMs;
    float confidenceScore; // 0.0-1.0
    
    static CycleResult ok(const char* msg, uint32_t iters, uint32_t models, uint64_t ms, float conf = 1.0f);
    static CycleResult error(const char* msg, uint32_t iters = 0);
};

// ============================================================================
// Iteration Callback
// ============================================================================
using IterationCallback = void(*)(uint32_t iteration, uint32_t total, const char* status, void* userData);
using ProgressCallback = void(*)(float progress, const char* message, void* userData);

// ============================================================================
// CycledAgentOrchestrator — Main orchestration engine
// ============================================================================
class CycledAgentOrchestrator {
public:
    CycledAgentOrchestrator();
    ~CycledAgentOrchestrator();
    
    // Lifecycle
    bool initialize();
    void shutdown();
    bool isInitialized() const { return m_initialized; }
    
    // Configuration
    void setModelCount(uint32_t count); // 1-99
    uint32_t getModelCount() const { return m_modelCount; }
    
    void setQualityMode(QualityMode mode);
    QualityMode getQualityMode() const { return m_qualityMode; }
    
    void setAgentMode(AgentMode mode);
    AgentMode getAgentMode() const { return m_agentMode; }
    
    void setMaxIterationsPerCycle(uint32_t max);
    uint32_t getMaxIterationsPerCycle() const { return m_maxIterationsPerCycle; }
    
    void setAutoAdjustmentEnabled(bool enabled);
    bool isAutoAdjustmentEnabled() const { return m_autoAdjustment; }
    
    // Callbacks
    void setIterationCallback(IterationCallback callback, void* userData);
    void setProgressCallback(ProgressCallback callback, void* userData);
    
    // Task Execution
    CycleResult executeTask(
        const char* taskDescription,
        const char* codebaseContext,
        uint32_t maxIterations = 0 // 0 = use default
    );
    
    // Complexity Analysis
    ComplexityMetrics analyzeComplexity(
        const char* taskDescription,
        const char* codebaseContext
    );
    
    // Statistics
    CycleStats getStatistics() const;
    void resetStatistics();
    
    // Abort current cycle
    void abort();
    bool isAborting() const { return m_aborting.load(std::memory_order_acquire); }
    
private:
    // Internal execution
    CycleResult executeSingleIteration(
        const char* taskDescription,
        const char* context,
        uint32_t iteration,
        uint32_t modelIndex
    );
    
    CycleResult executeParallelModels(
        const char* taskDescription,
        const char* context,
        uint32_t iteration
    );
    
    float assessResultQuality(const char* result);
    bool shouldContinueIteration(float quality, uint32_t iteration);
    
    void adjustModelCount();
    void adjustQualityThreshold();
    
    float calculateCodebaseSize(const char* context);
    float calculateDependencyDepth(const char* context);
    float calculateAlgorithmicComplexity(const char* task);
    float calculateDomainSpecificity(const char* task);
    
    void notifyIteration(uint32_t iteration, uint32_t total, const char* status);
    void notifyProgress(float progress, const char* message);
    
    // State
    std::atomic<bool> m_initialized;
    std::atomic<bool> m_aborting;
    std::atomic<uint32_t> m_modelCount;
    std::atomic<uint32_t> m_maxIterationsPerCycle;
    std::atomic<bool> m_autoAdjustment;
    
    QualityMode m_qualityMode;
    AgentMode m_agentMode;
    
    // Statistics
    mutable std::mutex m_statsMutex;
    CycleStats m_stats;
    
    // Callbacks
    IterationCallback m_iterationCallback;
    void* m_iterationUserData;
    ProgressCallback m_progressCallback;
    void* m_progressUserData;
    
    // Quality thresholds
    float m_minQualityThreshold;
    float m_targetQualityThreshold;
    
    // Auto-adjustment history
    struct AdjustmentHistory {
        uint32_t modelCount;
        float qualityAchieved;
        uint64_t durationMs;
        std::chrono::steady_clock::time_point timestamp;
    };
    std::vector<AdjustmentHistory> m_adjustmentHistory;
    const size_t MAX_HISTORY = 100;
};

} // namespace rawrxd::agent
