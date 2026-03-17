#include "agent_orchestrator.hpp"
#include "license_enforcement.h"
#include <iostream>
#include <chrono>
#include <sstream>

namespace RawrXD {

AgentOrchestrator::AgentOrchestrator(ModelInvoker* invoker, ActionExecutor* executor)
    : m_invoker(invoker)
    , m_executor(executor)
{
}

void AgentOrchestrator::startMission(const std::string& wish)
{
    if (m_state != AgentState::Idle) {
        if (m_onErrorEncountered) {
            m_onErrorEncountered("A mission is already in progress", false);
        }
        return;
    }

    m_currentWish = wish;
    m_history.clear();
    m_retryCount = 0;
    m_stopRequested = false;
    m_missionStartTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    m_state = AgentState::Thinking;
    if (m_onMissionStarted) {
        m_onMissionStarted(wish);
    }
    if (m_onStateChanged) {
        m_onStateChanged(m_state);
    }

    think();
}

void AgentOrchestrator::stopMission()
{
    m_stopRequested = true;
    m_executor->cancelExecution();
    m_state = AgentState::Idle;
    if (m_onStateChanged) {
        m_onStateChanged(m_state);
    }
}

void AgentOrchestrator::think()
{
    if (m_onProgressUpdated) {
        m_onProgressUpdated("Agent is planning...", 10);
    }
    
    InvocationParams params;
    params.wish = m_currentWish;
    params.context = buildFullContext();
    // In real life, these would come from the ToolsPanel registry (44 tools)
    params.availableTools = {"search_files", "file_edit", "run_build", "execute_tests", "terminal_exec", "mcp_call"};

    m_invoker->invokeAsync(params);
}

void AgentOrchestrator::onPlanGenerated(const nlohmann::json& plan, const std::string& reasoning)
{
    if (m_state != AgentState::Thinking) return;

    AgentStep step;
    step.state = m_state;
    step.planReasoning = reasoning;
    step.actions = plan;
    step.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    m_history.push_back(step);

    m_state = AgentState::Executing;
    if (m_onStateChanged) {
        m_onStateChanged(m_state);
    }
    execute();
}

void AgentOrchestrator::execute()
{
    if (m_onProgressUpdated) {
        m_onProgressUpdated("Agent is executing tools...", 40);
    }
    
    if (m_history.empty()) return;
    
    // Pass the last plan to executor
    m_executor->executePlan(m_history.back().actions, true); 
}

void AgentOrchestrator::onExecutionCompleted(bool success, const nlohmann::json& results)
{
    if (m_state != AgentState::Executing) return;

    m_history.back().results = results;

    if (!success) {
        std::string error = "Unknown execution error";
        if (results.contains("error") && results["error"].is_string()) {
            error = results["error"].get<std::string>();
        }
        
        m_retryCount++;
        if (m_retryCount >= m_maxRetries) {
            m_state = AgentState::Failed;
            if (m_onStateChanged) {
                m_onStateChanged(m_state);
            }
            if (m_onMissionCompleted) {
                m_onMissionCompleted(false, "Max retries reached: " + error);
            }
        } else {
            if (m_onErrorEncountered) {
                m_onErrorEncountered(error, true);
            }
            heal(error);
        }
        return;
    }

    m_state = AgentState::Verifying;
    if (m_onStateChanged) {
        m_onStateChanged(m_state);
    }
    verify();
}

void AgentOrchestrator::verify()
{
    if (m_onProgressUpdated) {
        m_onProgressUpdated("Verifying task completion...", 80);
    }
    
    // Ask LLM if the task is actually done based on history
    InvocationParams params;
    params.wish = "VERIFY: Is the task '" + m_currentWish + "' complete based on these results?";
    params.context = buildFullContext();
    params.availableTools = {"read_files", "run_build", "execute_tests"};

    // Note: We might need a separate signal/slot for verification 
    // but for now we reuse the plan logic with a specific prompt
    m_invoker->invokeAsync(params); 
}

void AgentOrchestrator::heal(const std::string& error)
{
    m_state = AgentState::SelfHealing;
    if (m_onStateChanged) {
        m_onStateChanged(m_state);
    }
    if (m_onProgressUpdated) {
        m_onProgressUpdated("Self-healing: fixing errors...", 50);
    }

    std::string detailedError = error;
    if (m_parser) {
        auto diagnostics = m_parser->parse(error);
        if (!diagnostics.empty()) {
            std::ostringstream oss;
            oss << "Structured Diagnostics Found:\n";
            for (const auto& diag : diagnostics) {
                oss << "[" << (diag.severity == DiagnosticSeverity::Error ? "ERROR" : "WARNING") 
                    << "] " << diag.filePath << ":" << diag.line << " - " << diag.message << "\n";
            }
            detailedError = oss.str();
        }
    }

    InvocationParams params;
    params.wish = "FIX: The previous attempt failed. Here is the analyzed error:\n" + detailedError + "\n\nOriginal wish: " + m_currentWish;
    params.context = buildFullContext();
    params.availableTools = {"search_files", "file_edit", "run_build"};

    m_invoker->invokeAsync(params);
}

void AgentOrchestrator::onHealPlanGenerated(const nlohmann::json& plan, const std::string& reasoning)
{
    // Transition back to execution with the new healing plan
    onPlanGenerated(plan, reasoning);
}

std::string AgentOrchestrator::buildFullContext() const
{
    std::ostringstream oss;
    oss << m_projectContext << "\n\nMISSION HISTORY:\n";
    for (const auto& step : m_history) {
        oss << "REASONING: " << step.planReasoning << "\n";
        oss << "ACTIONS: " << step.actions.dump() << "\n";
        oss << "RESULTS: " << step.results.dump() << "\n---\n";
    }
    return oss.str();
}

} // namespace RawrXD
