// ============================================================================
// OrchestrationSessionState.cpp
// Implementation: Shared Per-Session Orchestration State
// ============================================================================

#include "OrchestrationSessionState.h"
#include <algorithm>
#include <sstream>

namespace RawrXD::Orchestration
{

OrchestrationSessionState& OrchestrationSessionState::instance()
{
    static OrchestrationSessionState g_instance;
    return g_instance;
}

OrchestrationSessionState::OrchestrationSessionState()
{
    m_metrics.session_start = std::chrono::steady_clock::now();
}

void OrchestrationSessionState::setCurrentIntent(const IntentClassification& intent)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_intent_history.push_back(intent);
    // Keep history bounded to last 20 intents
    if (m_intent_history.size() > 20)
    {
        m_intent_history.erase(m_intent_history.begin());
    }
}

IntentClassification OrchestrationSessionState::getCurrentIntent() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_intent_history.empty())
    {
        return IntentClassification{"unknown", 0.0f, {}, "no intent history"};
    }
    return m_intent_history.back();
}

std::vector<IntentClassification> OrchestrationSessionState::getIntentHistory() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_intent_history;
}

void OrchestrationSessionState::clearIntentHistory()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_intent_history.clear();
}

void OrchestrationSessionState::recordToolExecution(const ToolExecutionResult& result)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tool_results.push_back(result);
    // Keep results bounded to last 20 executions
    if (m_tool_results.size() > 20)
    {
        m_tool_results.erase(m_tool_results.begin());
    }

    // Update metrics
    if (result.success)
    {
        m_metrics.tool_executions_succeeded.fetch_add(1, std::memory_order_relaxed);
    }
    else
    {
        m_metrics.tool_executions_failed.fetch_add(1, std::memory_order_relaxed);
    }

    // Invalidate synthesis cache
    m_synthesis_signal_cache.clear();
    m_last_synthesis_update = std::chrono::steady_clock::now();
}

std::vector<ToolExecutionResult> OrchestrationSessionState::getRecentToolResults(size_t max_count) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ToolExecutionResult> recent;
    size_t start = m_tool_results.size() > max_count ? m_tool_results.size() - max_count : 0;
    recent.insert(recent.end(), m_tool_results.begin() + start, m_tool_results.end());
    return recent;
}

std::string OrchestrationSessionState::getSynthesisSignal() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Return cached version if still valid (< 1 second old)
    if (!m_synthesis_signal_cache.empty())
    {
        auto now = std::chrono::steady_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_synthesis_update).count();
        if (age < 1000)
        {
            return m_synthesis_signal_cache;
        }
    }

    // Rebuild synthesis signal from recent results
    std::ostringstream oss;
    oss << "[OrchestratorSynthesis]\n";

    if (m_tool_results.empty())
    {
        oss << "No prior tool results.\n";
    }
    else
    {
        oss << "Recent tool execution summary:\n";
        size_t start = m_tool_results.size() > 5 ? m_tool_results.size() - 5 : 0;
        for (size_t i = start; i < m_tool_results.size(); ++i)
        {
            const auto& result = m_tool_results[i];
            oss << "  - " << result.tool_name << ": ";
            if (result.success)
            {
                oss << "OK (" << result.duration_ms << "ms)\n";
            }
            else
            {
                oss << "FAILED: " << result.error << "\n";
            }
        }
    }

    m_synthesis_signal_cache = oss.str();
    m_last_synthesis_update = std::chrono::steady_clock::now();
    return m_synthesis_signal_cache;
}

OrchestrationSessionState::OrchestrationMetrics& OrchestrationSessionState::getMetrics()
{
    return m_metrics;
}

void OrchestrationSessionState::recordOrchestrationPass(bool success)
{
    m_metrics.orchestrations_executed.fetch_add(1, std::memory_order_relaxed);
    if (success)
    {
        // Recalculate average confidence
        float sum = 0.0f;
        int count = 0;
        for (const auto& intent : m_intent_history)
        {
            sum += intent.confidence;
            count++;
        }
        if (count > 0)
        {
            m_metrics.average_confidence = sum / count;
        }
    }
    else
    {
        m_metrics.fallback_count.fetch_add(1, std::memory_order_relaxed);
    }
}

bool OrchestrationSessionState::shouldExecuteTools(float model_confidence_threshold) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_intent_history.empty())
    {
        return false;
    }
    return m_intent_history.back().confidence >= model_confidence_threshold;
}

void OrchestrationSessionState::setConfidenceThreshold(float threshold)
{
    m_confidence_threshold = std::max(0.0f, std::min(1.0f, threshold));
}

float OrchestrationSessionState::getConfidenceThreshold() const
{
    return m_confidence_threshold;
}

std::chrono::steady_clock::duration OrchestrationSessionState::sessionDuration() const
{
    return std::chrono::steady_clock::now() - m_metrics.session_start;
}

void OrchestrationSessionState::reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_intent_history.clear();
    m_tool_results.clear();
    m_synthesis_signal_cache.clear();
    m_metrics.orchestrations_executed.store(0, std::memory_order_relaxed);
    m_metrics.tool_executions_succeeded.store(0, std::memory_order_relaxed);
    m_metrics.tool_executions_failed.store(0, std::memory_order_relaxed);
    m_metrics.fallback_count.store(0, std::memory_order_relaxed);
    m_metrics.session_start = std::chrono::steady_clock::now();
}

}  // namespace RawrXD::Orchestration
