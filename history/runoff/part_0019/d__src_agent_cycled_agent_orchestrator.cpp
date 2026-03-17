// ============================================================================
// Cycled Agent Orchestrator Implementation
// ============================================================================

#include "cycled_agent_orchestrator.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <thread>

namespace rawrxd::agent {

// ============================================================================
// CycleResult helpers
// ============================================================================
CycleResult CycleResult::ok(const char* msg, uint32_t iters, uint32_t models, uint64_t ms, float conf) {
    return CycleResult{true, msg, iters, models, ms, conf};
}

CycleResult CycleResult::error(const char* msg, uint32_t iters) {
    return CycleResult{false, msg, iters, 0, 0, 0.0f};

}

// ============================================================================
// CycledAgentOrchestrator Implementation
// ============================================================================
CycledAgentOrchestrator::CycledAgentOrchestrator()
    : m_initialized(false)
    , m_aborting(false)
    , m_modelCount(1)
    , m_maxIterationsPerCycle(10)
    , m_autoAdjustment(true)
    , m_qualityMode(QualityMode::AUTO)
    , m_agentMode(AgentMode::AGENT)
    , m_iterationCallback(nullptr)
    , m_iterationUserData(nullptr)
    , m_progressCallback(nullptr)
    , m_progressUserData(nullptr)
    , m_minQualityThreshold(0.7f)
    , m_targetQualityThreshold(0.95f)
{
    std::memset(&m_stats, 0, sizeof(m_stats));
}

CycledAgentOrchestrator::~CycledAgentOrchestrator() {
    shutdown();
}

bool CycledAgentOrchestrator::initialize() {
    if (m_initialized.load(std::memory_order_acquire)) {
        return true;
    }
    
    // Initialize statistics
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        std::memset(&m_stats, 0, sizeof(m_stats));
    }
    
    m_adjustmentHistory.clear();
    m_adjustmentHistory.reserve(MAX_HISTORY);
    
    m_initialized.store(true, std::memory_order_release);
    return true;
}

void CycledAgentOrchestrator::shutdown() {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return;
    }
    
    m_aborting.store(true, std::memory_order_release);
    
    // Allow any active cycles to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    m_initialized.store(false, std::memory_order_release);
}

void CycledAgentOrchestrator::setModelCount(uint32_t count) {
    if (count < 1) count = 1;
    if (count > 99) count = 99;
    m_modelCount.store(count, std::memory_order_release);
}

void CycledAgentOrchestrator::setQualityMode(QualityMode mode) {
    m_qualityMode = mode;
    
    // Adjust thresholds based on mode
    switch (mode) {
        case QualityMode::AUTO:
            m_minQualityThreshold = 0.7f;
            m_targetQualityThreshold = 0.9f;
            break;
        case QualityMode::MAX:
        case QualityMode::QUANTUM:
            m_minQualityThreshold = 0.9f;
            m_targetQualityThreshold = 0.99f;
            break;
        case QualityMode::SPEED:
            m_minQualityThreshold = 0.5f;
            m_targetQualityThreshold = 0.75f;
            break;
    }
}

void CycledAgentOrchestrator::setAgentMode(AgentMode mode) {
    m_agentMode = mode;
}

void CycledAgentOrchestrator::setMaxIterationsPerCycle(uint32_t max) {
    if (max < 1) max = 1;
    if (max > 999) max = 999;
    m_maxIterationsPerCycle.store(max, std::memory_order_release);
}

void CycledAgentOrchestrator::setAutoAdjustmentEnabled(bool enabled) {
    m_autoAdjustment.store(enabled, std::memory_order_release);
}

void CycledAgentOrchestrator::setIterationCallback(IterationCallback callback, void* userData) {
    m_iterationCallback = callback;
    m_iterationUserData = userData;
}

void CycledAgentOrchestrator::setProgressCallback(ProgressCallback callback, void* userData) {
    m_progressCallback = callback;
    m_progressUserData = userData;
}

void CycledAgentOrchestrator::abort() {
    m_aborting.store(true, std::memory_order_release);
}

CycleStats CycledAgentOrchestrator::getStatistics() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

void CycledAgentOrchestrator::resetStatistics() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    std::memset(&m_stats, 0, sizeof(m_stats));
}

// ============================================================================
// Complexity Analysis
// ============================================================================
ComplexityMetrics CycledAgentOrchestrator::analyzeComplexity(
    const char* taskDescription,
    const char* codebaseContext
) {
    ComplexityMetrics metrics{};
    
    metrics.codebaseSize = calculateCodebaseSize(codebaseContext);
    metrics.dependencyDepth = calculateDependencyDepth(codebaseContext);
    metrics.algorithmicComplexity = calculateAlgorithmicComplexity(taskDescription);
    metrics.domainSpecificity = calculateDomainSpecificity(taskDescription);
    
    // Weighted average (adjust weights based on empirical data)
    metrics.overallScore = 
        metrics.codebaseSize * 0.25f +
        metrics.dependencyDepth * 0.25f +
        metrics.algorithmicComplexity * 0.30f +
        metrics.domainSpecificity * 0.20f;
    
    // Predict iterations based on complexity
    if (metrics.overallScore < 0.3f) {
        metrics.predictedIterations = 1;
        metrics.predictedModelCount = 1;
        metrics.estimatedTimeMs = 5000;
    } else if (metrics.overallScore < 0.6f) {
        metrics.predictedIterations = 3;
        metrics.predictedModelCount = 2;
        metrics.estimatedTimeMs = 15000;
    } else if (metrics.overallScore < 0.8f) {
        metrics.predictedIterations = 8;
        metrics.predictedModelCount = 4;
        metrics.estimatedTimeMs = 45000;
    } else {
        metrics.predictedIterations = 20;
        metrics.predictedModelCount = 8;
        metrics.estimatedTimeMs = 120000;
    }
    
    // Apply quality mode multipliers
    switch (m_qualityMode) {
        case QualityMode::MAX:
            metrics.predictedIterations = static_cast<uint32_t>(metrics.predictedIterations * 1.5f);
            metrics.predictedModelCount = static_cast<uint32_t>(metrics.predictedModelCount * 2.0f);
            break;
        case QualityMode::QUANTUM:
            metrics.predictedIterations = static_cast<uint32_t>(metrics.predictedIterations * 3.0f);
            metrics.predictedModelCount = static_cast<uint32_t>(metrics.predictedModelCount * 3.0f);
            break;
        case QualityMode::SPEED:
            metrics.predictedIterations = std::max(1u, static_cast<uint32_t>(metrics.predictedIterations * 0.5f));
            metrics.predictedModelCount = 1;
            break;
        default:
            break;
    }
    
    // Clamp to valid ranges
    metrics.predictedModelCount = std::min(99u, std::max(1u, metrics.predictedModelCount));
    metrics.predictedIterations = std::min(999u, std::max(1u, metrics.predictedIterations));
    
    return metrics;
}

// ============================================================================
// Task Execution
// ============================================================================
CycleResult CycledAgentOrchestrator::executeTask(
    const char* taskDescription,
    const char* codebaseContext,
    uint32_t maxIterations
) {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return CycleResult::error("Orchestrator not initialized");
    }
    
    m_aborting.store(false, std::memory_order_release);
    
    auto startTime = std::chrono::steady_clock::now();
    
    // Analyze complexity
    ComplexityMetrics complexity = analyzeComplexity(taskDescription, codebaseContext);
    
    notifyProgress(0.0f, "Analyzing task complexity...");
    
    // Determine iteration count
    uint32_t targetIterations = maxIterations > 0 
        ? maxIterations 
        : std::min(complexity.predictedIterations, m_maxIterationsPerCycle.load(std::memory_order_acquire));
    
    // Auto-adjust model count if enabled
    if (m_autoAdjustment.load(std::memory_order_acquire)) {
        setModelCount(complexity.predictedModelCount);
    }
    
    uint32_t currentModelCount = m_modelCount.load(std::memory_order_acquire);
    
    notifyProgress(0.1f, "Starting agent cycle...");
    
    // Execute iterations
    float bestQuality = 0.0f;
    uint32_t iterationsCompleted = 0;
    
    for (uint32_t iter = 0; iter < targetIterations; ++iter) {
        if (m_aborting.load(std::memory_order_acquire)) {
            return CycleResult::error("Cycle aborted by user", iterationsCompleted);
        }
        
        char statusBuf[256];
        snprintf(statusBuf, sizeof(statusBuf), "Iteration %u/%u (Models: %u)", 
                 iter + 1, targetIterations, currentModelCount);
        notifyIteration(iter + 1, targetIterations, statusBuf);
        
        float progress = 0.1f + (0.8f * static_cast<float>(iter) / targetIterations);
        notifyProgress(progress, statusBuf);
        
        // Execute with current model count
        CycleResult iterResult = (currentModelCount > 1)
            ? executeParallelModels(taskDescription, codebaseContext, iter)
            : executeSingleIteration(taskDescription, codebaseContext, iter, 0);
        
        iterationsCompleted++;
        
        if (!iterResult.success) {
            // Record failure
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_stats.failedCycles++;
            m_stats.totalIterations += iterationsCompleted;
            return CycleResult::error(iterResult.detail, iterationsCompleted);
        }
        
        float currentQuality = iterResult.confidenceScore;
        if (currentQuality > bestQuality) {
            bestQuality = currentQuality;
        }
        
        // Check if we've reached target quality
        if (!shouldContinueIteration(bestQuality, iter)) {
            break;
        }
        
        // Adjust model count dynamically
        if (m_autoAdjustment.load(std::memory_order_acquire) && iter > 0 && iter % 3 == 0) {
            adjustModelCount();
            currentModelCount = m_modelCount.load(std::memory_order_acquire);
        }
    }
    
    auto endTime = std::chrono::steady_clock::now();
    uint64_t durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    notifyProgress(1.0f, "Cycle complete");
    
    // Update statistics
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalCycles++;
        m_stats.successfulCycles++;
        m_stats.totalIterations += iterationsCompleted;
        m_stats.totalDurationMs += durationMs;
        m_stats.averageIterations = m_stats.totalIterations / m_stats.totalCycles;
        m_stats.averageDurationMs = m_stats.totalDurationMs / m_stats.totalCycles;
        m_stats.peakModelCount = std::max(m_stats.peakModelCount, currentModelCount);
        m_stats.currentActiveModels = currentModelCount;
    }
    
    // Record adjustment history
    AdjustmentHistory history{currentModelCount, bestQuality, durationMs, std::chrono::steady_clock::now()};
    if (m_adjustmentHistory.size() >= MAX_HISTORY) {
        m_adjustmentHistory.erase(m_adjustmentHistory.begin());
    }
    m_adjustmentHistory.push_back(history);
    
    return CycleResult::ok("Cycle completed successfully", iterationsCompleted, currentModelCount, durationMs, bestQuality);
}

// ============================================================================
// Single Iteration Execution
// ============================================================================
CycleResult CycledAgentOrchestrator::executeSingleIteration(
    const char* taskDescription,
    const char* context,
    uint32_t iteration,
    uint32_t modelIndex
) {
    // TODO: Integrate with actual model inference engine
    // For now, simulate processing
    
    auto start = std::chrono::steady_clock::now();
    
    // Simulate inference time based on complexity
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto end = std::chrono::steady_clock::now();
    uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    // Simulate quality assessment
    float quality = 0.6f + (static_cast<float>(iteration) / 100.0f);
    if (quality > 1.0f) quality = 1.0f;
    
    return CycleResult::ok("Iteration complete", 1, 1, ms, quality);
}

// ============================================================================
// Parallel Model Execution
// ============================================================================
CycleResult CycledAgentOrchestrator::executeParallelModels(
    const char* taskDescription,
    const char* context,
    uint32_t iteration
) {
    uint32_t modelCount = m_modelCount.load(std::memory_order_acquire);
    
    auto start = std::chrono::steady_clock::now();
    
    // Execute models in parallel (simplified)
    std::vector<std::thread> threads;
    threads.reserve(modelCount);
    
    std::vector<CycleResult> results(modelCount);
    
    for (uint32_t i = 0; i < modelCount; ++i) {
        threads.emplace_back([this, &results, i, taskDescription, context, iteration]() {
            results[i] = executeSingleIteration(taskDescription, context, iteration, i);
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    auto end = std::chrono::steady_clock::now();
    uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    // Aggregate results - select best quality
    float bestQuality = 0.0f;
    for (const auto& result : results) {
        if (result.success && result.confidenceScore > bestQuality) {
            bestQuality = result.confidenceScore;
        }
    }
    
    return CycleResult::ok("Parallel execution complete", 1, modelCount, ms, bestQuality);
}

// ============================================================================
// Quality Assessment
// ============================================================================
float CycledAgentOrchestrator::assessResultQuality(const char* result) {
    // TODO: Implement sophisticated quality metrics
    // For now, return placeholder
    return 0.8f;
}

bool CycledAgentOrchestrator::shouldContinueIteration(float quality, uint32_t iteration) {
    // Always continue if below minimum threshold
    if (quality < m_minQualityThreshold) {
        return true;
    }
    
    // Check if target reached
    if (quality >= m_targetQualityThreshold) {
        return false;
    }
    
    // Apply quality mode specific logic
    switch (m_qualityMode) {
        case QualityMode::SPEED:
            return quality < 0.75f && iteration < 3;
        case QualityMode::MAX:
        case QualityMode::QUANTUM:
            return quality < 0.99f;
        case QualityMode::AUTO:
        default:
            return quality < 0.9f;
    }
}

// ============================================================================
// Auto-Adjustment
// ============================================================================
void CycledAgentOrchestrator::adjustModelCount() {
    if (m_adjustmentHistory.size() < 3) {
        return; // Need more data
    }
    
    // Analyze recent performance
    float avgQuality = 0.0f;
    uint64_t avgDuration = 0;
    
    size_t lookback = std::min(size_t(5), m_adjustmentHistory.size());
    for (size_t i = m_adjustmentHistory.size() - lookback; i < m_adjustmentHistory.size(); ++i) {
        avgQuality += m_adjustmentHistory[i].qualityAchieved;
        avgDuration += m_adjustmentHistory[i].durationMs;
    }
    avgQuality /= lookback;
    avgDuration /= lookback;
    
    uint32_t current = m_modelCount.load(std::memory_order_acquire);
    
    // If quality is low, increase model count
    if (avgQuality < m_minQualityThreshold && current < 99) {
        setModelCount(current + 1);
    }
    // If quality is high and we're using many models, reduce
    else if (avgQuality > m_targetQualityThreshold && current > 2) {
        setModelCount(current - 1);
    }
}

void CycledAgentOrchestrator::adjustQualityThreshold() {
    // TODO: Implement adaptive threshold adjustment
}

// ============================================================================
// Complexity Calculation Helpers
// ============================================================================
float CycledAgentOrchestrator::calculateCodebaseSize(const char* context) {
    if (!context) return 0.1f;
    
    size_t len = std::strlen(context);
    
    // Normalize to 0.0-1.0 (assume max 1MB context)
    float normalized = static_cast<float>(len) / (1024.0f * 1024.0f);
    return std::min(1.0f, normalized);
}

float CycledAgentOrchestrator::calculateDependencyDepth(const char* context) {
    if (!context) return 0.1f;
    
    // Count include statements, imports, namespace depth
    size_t includeCount = 0;
    const char* ptr = context;
    while ((ptr = std::strstr(ptr, "#include")) != nullptr) {
        includeCount++;
        ptr++;
    }
    
    // Normalize (assume max 100 includes = deep dependency)
    return std::min(1.0f, static_cast<float>(includeCount) / 100.0f);
}

float CycledAgentOrchestrator::calculateAlgorithmicComplexity(const char* task) {
    if (!task) return 0.1f;
    
    // Simple heuristic: look for complexity indicators
    const char* indicators[] = {
        "optimize", "algorithm", "parallel", "concurrent", "thread",
        "mutex", "lock", "atomic", "race", "deadlock",
        "recursive", "dynamic programming", "graph", "tree",
        "complex", "difficult", "advanced", "quantum"
    };
    
    size_t matches = 0;
    for (const char* indicator : indicators) {
        if (std::strstr(task, indicator) != nullptr) {
            matches++;
        }
    }
    
    return std::min(1.0f, static_cast<float>(matches) / 10.0f);
}

float CycledAgentOrchestrator::calculateDomainSpecificity(const char* task) {
    if (!task) return 0.1f;
    
    // Look for domain-specific keywords
    const char* domains[] = {
        "MASM", "assembly", "Vulkan", "GPU", "CUDA",
        "tensor", "inference", "quantization", "GGUF",
        "hotpatch", "detour", "injection", "binary"
    };
    
    size_t matches = 0;
    for (const char* domain : domains) {
        if (std::strstr(task, domain) != nullptr) {
            matches++;
        }
    }
    
    return std::min(1.0f, static_cast<float>(matches) / 8.0f);
}

// ============================================================================
// Notification Helpers
// ============================================================================
void CycledAgentOrchestrator::notifyIteration(uint32_t iteration, uint32_t total, const char* status) {
    if (m_iterationCallback) {
        m_iterationCallback(iteration, total, status, m_iterationUserData);
    }
}

void CycledAgentOrchestrator::notifyProgress(float progress, const char* message) {
    if (m_progressCallback) {
        m_progressCallback(progress, message, m_progressUserData);
    }
}

} // namespace rawrxd::agent
