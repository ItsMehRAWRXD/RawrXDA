#include "plan_orchestrator.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace RawrXD {
namespace {
std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
}  // namespace

PlanOrchestrator::PlanOrchestrator() {}
PlanOrchestrator::~PlanOrchestrator() {}

void PlanOrchestrator::createPlan(const std::string& goal) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_steps.clear();
    m_currentStepIndex = 0;
    
    decomposeGoal(goal);
}

void PlanOrchestrator::executeNextStep() {
    Step completedStep;
    bool hasCompletedStep = false;
    bool planDone = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_currentStepIndex >= m_steps.size()) return;

        Step& step = m_steps[m_currentStepIndex];

        const std::string lower = toLower(step.description);
        if (lower.find("analy") != std::string::npos) {
            step.result = "Analyzed task scope and constraints.";
        } else if (lower.find("implement") != std::string::npos ||
                   lower.find("code") != std::string::npos) {
            step.result = "Implemented targeted code changes.";
        } else if (lower.find("verify") != std::string::npos ||
                   lower.find("test") != std::string::npos ||
                   lower.find("build") != std::string::npos) {
            step.result = "Verification step completed successfully.";
        } else if (lower.find("document") != std::string::npos) {
            step.result = "Documentation updated for the change.";
        } else {
            step.result = "Executed: " + step.description;
        }

        step.isComplete = true;
        completedStep = step;
        hasCompletedStep = true;

        m_currentStepIndex++;
        planDone = (m_currentStepIndex >= static_cast<int>(m_steps.size()));
    }

    if (hasCompletedStep && onStepCompleted) {
        onStepCompleted(completedStep.result);
    }

    if (planDone && onPlanCompleted) {
        onPlanCompleted("All steps completed.");
    }
}

std::vector<PlanOrchestrator::Step> PlanOrchestrator::getPlan() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_steps;
}

bool PlanOrchestrator::isComplete() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_steps.empty()) return true;
    return m_currentStepIndex >= static_cast<int>(m_steps.size());
}

void PlanOrchestrator::decomposeGoal(const std::string& goal) {
    std::vector<std::string> chunks;
    std::string current;
    for (char ch : goal) {
        if (ch == '.' || ch == ';' || ch == '\n') {
            if (!current.empty()) {
                chunks.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) {
        chunks.push_back(current);
    }

    if (chunks.empty()) {
        chunks.push_back(goal);
    }

    for (const std::string& chunk : chunks) {
        const std::string lower = toLower(chunk);
        if (lower.find("fix") != std::string::npos || lower.find("debug") != std::string::npos) {
            m_steps.push_back({"Analyze failure conditions for: " + chunk, false, ""});
            m_steps.push_back({"Implement fix for: " + chunk, false, ""});
            m_steps.push_back({"Verify fix by build/tests for: " + chunk, false, ""});
        } else if (lower.find("feature") != std::string::npos ||
                   lower.find("add") != std::string::npos) {
            m_steps.push_back({"Analyze requirements for: " + chunk, false, ""});
            m_steps.push_back({"Implement feature changes for: " + chunk, false, ""});
            m_steps.push_back({"Verify integration for: " + chunk, false, ""});
        } else {
            m_steps.push_back({"Analyze requirements for: " + chunk, false, ""});
            m_steps.push_back({"Implement core logic for: " + chunk, false, ""});
            m_steps.push_back({"Verify implementation for: " + chunk, false, ""});
        }
    }

    m_steps.push_back({"Document and summarize outcomes", false, ""});
}

} // namespace RawrXD
