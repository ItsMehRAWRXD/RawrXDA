// ============================================================================
// Auto-Adjusting Terminal Implementation
// ============================================================================

#include "auto_adjusting_terminal.hpp"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace rawrxd::agent {

AutoAdjustingTerminal::AutoAdjustingTerminal()
    : m_initialized(false)
    , m_learningEnabled(true)
    , m_baseTimeoutMs(30000)      // 30 seconds default
    , m_minTimeoutMs(5000)        // 5 seconds minimum
    , m_max TimeoutMs(3600000)     // 1 hour maximum
    , m_strategy(TimeoutStrategy::HYBRID)
    , m_aggressiveness(0.5f)
{
    std::memset(&m_stats, 0, sizeof(m_stats));
}

AutoAdjustingTerminal::~AutoAdjustingTerminal() {
    shutdown();
}

bool AutoAdjustingTerminal::initialize() {
    if (m_initialized.load(std::memory_order_acquire)) {
        return true;
    }
    
    m_history.clear();
    m_history.reserve(MAX_HISTORY);
    
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        std::memset(&m_stats, 0, sizeof(m_stats));
    }
    
    m_initialized.store(true, std::memory_order_release);
    return true;
}

void AutoAdjustingTerminal::shutdown() {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return;
    }
    
    m_initialized.store(false, std::memory_order_release);
}

void AutoAdjustingTerminal::setStrategy(TimeoutStrategy strategy) {
    m_strategy = strategy;
}

void AutoAdjustingTerminal::setBaseTimeoutMs(uint64_t ms) {
    m_baseTimeoutMs.store(clampTimeout(ms), std::memory_order_release);
}

void AutoAdjustingTerminal::setMinTimeoutMs(uint64_t ms) {
    if (ms > 1000) { // At least 1 second
        m_minTimeoutMs.store(ms, std::memory_order_release);
    }
}

void AutoAdjustingTerminal::setMaxTimeoutMs(uint64_t ms) {
    m_maxTimeoutMs.store(ms, std::memory_order_release);
}

void AutoAdjustingTerminal::setLearningEnabled(bool enabled) {
    m_learningEnabled.store(enabled, std::memory_order_release);
}

void AutoAdjustingTerminal::setAdjustmentAggressiveness(float factor) {
    m_aggressiveness = std::max(0.0f, std::min(1.0f, factor));
}

// ============================================================================
// Timeout Prediction
// ============================================================================
TimeoutPrediction AutoAdjustingTerminal::predictTimeout(
    const char* command,
    float taskComplexity
) {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return TimeoutPrediction{30000, 5000, 60000, 0.0f, "Not initialized"};
    }
    
    uint64_t predicted = 0;
    const char* reasoning = "";
    
    switch (m_strategy) {
        case TimeoutStrategy::FIXED:
            predicted = predictFixed();
            reasoning = "Fixed timeout strategy";
            break;
        case TimeoutStrategy::ADAPTIVE:
            predicted = predictAdaptive(command);
            reasoning = "Adaptive based on historical data";
            break;
        case TimeoutStrategy::COMPLEXITY:
            predicted = predictComplexity(taskComplexity);
            reasoning = "Complexity-based prediction";
            break;
        case TimeoutStrategy::HYBRID:
            predicted = predictHybrid(command, taskComplexity);
            reasoning = "Hybrid adaptive + complexity";
            break;
    }
    
    uint64_t minTimeout = m_minTimeoutMs.load(std::memory_order_acquire);
    uint64_t maxTimeout = m_maxTimeoutMs.load(std::memory_order_acquire);
    
    // Calculate confidence based on history size
    float confidence = 0.5f;
    if (m_history.size() > 100) {
        confidence = 0.9f;
    } else if (m_history.size() > 20) {
        confidence = 0.7f;
    }
    
    return TimeoutPrediction{
        predicted,
        minTimeout,
        maxTimeout,
        confidence,
        reasoning
    };
}

uint64_t AutoAdjustingTerminal::predictFixed() {
    return m_baseTimeoutMs.load(std::memory_order_acquire);
}

uint64_t AutoAdjustingTerminal::predictAdaptive(const char* command) {
    std::lock_guard<std::mutex> lock(m_historyMutex);
    
    if (m_history.empty()) {
        return m_baseTimeoutMs.load(std::memory_order_acquire);
    }
    
    // Calculate average actual duration from recent history
    size_t lookback = std::min(size_t(50), m_history.size());
    uint64_t totalDuration = 0;
    size_t count = 0;
    
    for (size_t i = m_history.size() - lookback; i < m_history.size(); ++i) {
        if (!m_history[i].timedOut) {
            totalDuration += m_history[i].actualDurationMs;
            count++;
        }
    }
    
    if (count == 0) {
        return m_baseTimeoutMs.load(std::memory_order_acquire);
    }
    
    uint64_t avgDuration = totalDuration / count;
    
    // Apply safety margin (1.5x to 3x based on aggressiveness)
    float safetyMultiplier = 1.5f + (m_aggressiveness * 1.5f);
    uint64_t predicted = static_cast<uint64_t>(avgDuration * safetyMultiplier);
    
    return clampTimeout(predicted);
}

uint64_t AutoAdjustingTerminal::predictComplexity(float complexity) {
    uint64_t baseTimeout = m_baseTimeoutMs.load(std::memory_order_acquire);
    
    // Scale timeout based on complexity (0.0-1.0)
    // Low complexity: 0.5x base
    // High complexity: 5x base
    float multiplier = 0.5f + (complexity * 4.5f);
    
    uint64_t predicted = static_cast<uint64_t>(baseTimeout * multiplier);
    return clampTimeout(predicted);
}

uint64_t AutoAdjustingTerminal::predictHybrid(const char* command, float complexity) {
    // Combine adaptive and complexity predictions
    uint64_t adaptive = predictAdaptive(command);
    uint64_t complexityBased = predictComplexity(complexity);
    
    // Weighted average (60% adaptive, 40% complexity)
    uint64_t predicted = static_cast<uint64_t>(
        adaptive * 0.6 + complexityBased * 0.4
    );
    
    return clampTimeout(predicted);
}

// ============================================================================
// Execution Recording
// ============================================================================
void AutoAdjustingTerminal::recordExecution(
    const char* command,
    uint64_t timeoutMs,
    uint64_t actualDurationMs,
    bool timedOut,
    float taskComplexity
) {
    if (!m_learningEnabled.load(std::memory_order_acquire)) {
        return;
    }
    
    ExecutionHistory entry{
        timeoutMs,
        actualDurationMs,
        timedOut,
        taskComplexity,
        std::chrono::steady_clock::now()
    };
    
    {
        std::lock_guard<std::mutex> lock(m_historyMutex);
        
        // Maintain history size limit
        if (m_history.size() >= MAX_HISTORY) {
            m_history.erase(m_history.begin());
        }
        
        m_history.push_back(entry);
    }
    
    // Update statistics
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        
        m_stats.totalExecutions++;
        
        if (timedOut) {
            m_stats.timeouts++;
        } else {
            m_stats.successful++;
        }
        
        // Update averages
        m_stats.avgActualDurationMs = 
            (m_stats.avgActualDurationMs * (m_stats.totalExecutions - 1) + actualDurationMs) 
            / m_stats.totalExecutions;
        
        m_stats.avgTimeoutMs = 
            (m_stats.avgTimeoutMs * (m_stats.totalExecutions - 1) + timeoutMs) 
            / m_stats.totalExecutions;
        
        m_stats.timeoutRate = 
            static_cast<float>(m_stats.timeouts) / m_stats.totalExecutions;
        
        if (!timedOut && timeoutMs > 0) {
            float utilization = static_cast<float>(actualDurationMs) / timeoutMs;
            m_stats.avgUtilization = 
                (m_stats.avgUtilization * (m_stats.successful - 1) + utilization) 
                / m_stats.successful;
        }
    }
}

// ============================================================================
// Statistics
// ============================================================================
TerminalStats AutoAdjustingTerminal::getStatistics() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

void AutoAdjustingTerminal::resetStatistics() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    std::memset(&m_stats, 0, sizeof(m_stats));
}

void AutoAdjustingTerminal::clearHistory() {
    std::lock_guard<std::mutex> lock(m_historyMutex);
    m_history.clear();
}

size_t AutoAdjustingTerminal::getHistorySize() const {
    std::lock_guard<std::mutex> lock(m_historyMutex);
    return m_history.size();
}

// ============================================================================
// Analysis Helpers
// ============================================================================
float AutoAdjustingTerminal::analyzeCommandComplexity(const char* command) {
    if (!command) return 0.3f;
    
    // Heuristic complexity analysis
    size_t len = std::strlen(command);
    
    // Indicators of complexity
    int pipeCount = 0;
    int redirectCount = 0;
    int loopKeywords = 0;
    int functionCalls = 0;
    
    const char* ptr = command;
    while (*ptr) {
        if (*ptr == '|') pipeCount++;
        if (*ptr == '>' || *ptr == '<') redirectCount++;
        ptr++;
    }
    
    // Check for keywords
    const char* complexKeywords[] = {
        "foreach", "for", "while", "do", "function",
        "parallel", "job", "invoke", "remote"
    };
    
    for (const char* keyword : complexKeywords) {
        if (std::strstr(command, keyword)) {
            loopKeywords++;
        }
    }
    
    // Calculate complexity score
    float complexity = 0.1f;
    complexity += std::min(0.2f, len / 1000.0f);
    complexity += pipeCount * 0.1f;
    complexity += redirectCount * 0.05f;
    complexity += loopKeywords * 0.2f;
    
    return std::min(1.0f, complexity);
}

float AutoAdjustingTerminal::calculateUtilizationTrend() {
    std::lock_guard<std::mutex> lock(m_historyMutex);
    
    if (m_history.size() < 5) {
        return 0.5f; // Neutral
    }
    
    // Calculate average utilization over last N entries
    size_t lookback = std::min(size_t(20), m_history.size());
    float totalUtil = 0.0f;
    size_t count = 0;
    
    for (size_t i = m_history.size() - lookback; i < m_history.size(); ++i) {
        if (!m_history[i].timedOut && m_history[i].timeoutMs > 0) {
            float util = static_cast<float>(m_history[i].actualDurationMs) 
                         / m_history[i].timeoutMs;
            totalUtil += util;
            count++;
        }
    }
    
    return count > 0 ? (totalUtil / count) : 0.5f;
}

float AutoAdjustingTerminal::calculateTimeoutSuccessRate() {
    std::lock_guard<std::mutex> lock(m_historyMutex);
    
    if (m_history.empty()) {
        return 1.0f;
    }
    
    size_t lookback = std::min(size_t(50), m_history.size());
    size_t timeouts = 0;
    
    for (size_t i = m_history.size() - lookback; i < m_history.size(); ++i) {
        if (m_history[i].timedOut) {
            timeouts++;
        }
    }
    
    return 1.0f - (static_cast<float>(timeouts) / lookback);
}

// ============================================================================
// Adjustment
// ============================================================================
uint64_t AutoAdjustingTerminal::adjustBasedOnHistory(uint64_t baseTimeout) {
    float utilizationTrend = calculateUtilizationTrend();
    float successRate = calculateTimeoutSuccessRate();
    
    // If utilization is high (close to timeout), increase
    if (utilizationTrend > 0.8f) {
        baseTimeout = static_cast<uint64_t>(baseTimeout * 1.3);
    }
    // If utilization is low and success rate is high, decrease
    else if (utilizationTrend < 0.5f && successRate > 0.95f) {
        baseTimeout = static_cast<uint64_t>(baseTimeout * 0.8);
    }
    
    // If timeout rate is high, increase significantly
    if (successRate < 0.8f) {
        baseTimeout = static_cast<uint64_t>(baseTimeout * 1.5);
    }
    
    return clampTimeout(baseTimeout);
}

uint64_t AutoAdjustingTerminal::clampTimeout(uint64_t timeout) {
    uint64_t minTimeout = m_minTimeoutMs.load(std::memory_order_acquire);
    uint64_t maxTimeout = m_maxTimeoutMs.load(std::memory_order_acquire);
    
    if (timeout < minTimeout) return minTimeout;
    if (timeout > maxTimeout) return maxTimeout;
    return timeout;
}

} // namespace rawrxd::agent
