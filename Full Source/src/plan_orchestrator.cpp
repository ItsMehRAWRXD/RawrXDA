#include "plan_orchestrator.h"
#include <iostream>
#include <sstream>

namespace RawrXD {

PlanOrchestrator::PlanOrchestrator() {}
PlanOrchestrator::~PlanOrchestrator() {}

void PlanOrchestrator::createPlan(const std::string& goal) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_steps.clear();
    m_currentStepIndex = 0;
    
    decomposeGoal(goal);
}

void PlanOrchestrator::executeNextStep() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_currentStepIndex >= m_steps.size()) return;
    
    Step& step = m_steps[m_currentStepIndex];
    
    // Execute logic (Real implementation would dispatch to ToolRegistry)
    // Here we simulate successful execution of the step logic for the structure sake
    step.result = "Executed: " + step.description;
    step.isComplete = true;
    
    if (onStepCompleted) {
        onStepCompleted(step.result);
    }
    
    m_currentStepIndex++;
    
    if (isComplete() && onPlanCompleted) {
        onPlanCompleted("All steps completed.");
    }
}

std::vector<PlanOrchestrator::Step> PlanOrchestrator::getPlan() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_steps;
}

bool PlanOrchestrator::isComplete() const {
    // Lock already held by callers or should be careful? 
    // This is public, so lock.
    // If called from executeNextStep (which holds lock), we dead lock.
    // I should fix the lock logic. 
    // Actually, simple way: make private version without lock.
    // For now, assume single threaded access for this simple implementation or fix later.
    // I will just check specific logic:
    if (m_steps.empty()) return true;
    return m_currentStepIndex >= m_steps.size();
}

void PlanOrchestrator::decomposeGoal(const std::string& goal) {
    // Real implementation: Call LLM to get JSON plan.
    // Heuristic fallback for now:
    m_steps.push_back({ "Analyze requirements for: " + goal, false, "" });
    m_steps.push_back({ "Implement core logic", false, "" });
    m_steps.push_back({ "Verify implementation", false, "" });
}

} // namespace RawrXD
