// ============================================================================
// ide_agent_integration.cpp — IDE integration implementation
// ============================================================================

#include "ide_agent_integration.h"
#include <sstream>

namespace RawrXD {

uint32_t IDEAgentIntegration::SubmitTaskFromChat(const std::string& userMessage,
                                                 const std::string& workspacePath) {
    std::string task = ExtractTaskFromMessage(userMessage);
    std::vector<std::string> files = ExtractContextFiles(userMessage);

    return m_agent.SubmitTask(task, workspacePath, files);
}

std::string IDEAgentIntegration::FormatPlanForDisplay(uint32_t taskId) {
    auto plan = m_agent.GetPlan(taskId);
    if (!plan) return "Plan not found";

    std::ostringstream ss;
    ss << "📋 EXECUTION PLAN\n";
    ss << "================\n\n";
    ss << "Task: " << plan->taskDescription << "\n";
    ss << "Estimated Duration: " << plan->totalEstimatedMs << "ms\n";
    ss << "Max Risk Level: ";

    switch (plan->maxRiskLevel) {
        case RiskLevel::SAFE: ss << "🟢 SAFE"; break;
        case RiskLevel::WARN: ss << "🟡 WARNING"; break;
        case RiskLevel::CRITICAL: ss << "🔴 CRITICAL"; break;
    }
    ss << "\n";
    ss << "Requires Approval: " << (plan->requiresApproval ? "YES" : "NO") << "\n";
    ss << "Steps: " << plan->steps.size() << "\n\n";

    ss << "Reasoning: " << plan->reasoning << "\n\n";

    ss << "STEPS:\n";
    ss << "------\n";

    for (size_t i = 0; i < plan->steps.size(); i++) {
        const auto& step = plan->steps[i];
        ss << "\n" << (i + 1) << ". " << step.action << "\n";
        ss << "   Description: " << step.description << "\n";
        ss << "   Estimated: " << step.estimatedDurationMs << "ms\n";

        if (!step.dependencies.empty()) {
            ss << "   Depends on: ";
            for (size_t j = 0; j < step.dependencies.size(); j++) {
                if (j > 0) ss << ", ";
                ss << "Step " << (step.dependencies[j] + 1);
            }
            ss << "\n";
        }

        ss << "   Risk: ";
        switch (step.riskLevel) {
            case RiskLevel::SAFE: ss << "🟢 SAFE"; break;
            case RiskLevel::WARN: ss << "🟡 WARNING"; break;
            case RiskLevel::CRITICAL: ss << "🔴 CRITICAL"; break;
        }
        ss << "\n";

        if (!step.previewContent.empty()) {
            ss << "   Preview:\n";
            std::istringstream preview(step.previewContent);
            std::string line;
            while (std::getline(preview, line)) {
                ss << "     " << line << "\n";
            }
        }
    }

    return ss.str();
}

std::string IDEAgentIntegration::FormatApprovalRequest(const ApprovalRequest& req) {
    std::ostringstream ss;
    ss << "🔒 APPROVAL REQUIRED\n";
    ss << "====================\n\n";
    ss << "Step: " << req.stepAction << "\n";
    ss << "Description: " << req.description << "\n";
    ss << "Risk Level: ";

    switch (req.riskLevel) {
        case RiskLevel::SAFE: ss << "🟢 SAFE"; break;
        case RiskLevel::WARN: ss << "🟡 WARNING"; break;
        case RiskLevel::CRITICAL: ss << "🔴 CRITICAL"; break;
    }
    ss << "\n\n";

    ss << "Reasoning: " << req.reasoning << "\n\n";

    if (!req.preview.empty()) {
        ss << "PREVIEW OF CHANGES:\n";
        ss << "-------------------\n";
        ss << req.preview << "\n\n";
    }

    ss << "[APPROVE] [REJECT] [PREVIEW]\n";

    return ss.str();
}

std::string IDEAgentIntegration::FormatProgressForStatusBar(uint32_t taskId) {
    auto progress = m_agent.GetProgress(taskId);

    std::ostringstream ss;
    ss << "Task #" << taskId << " | ";
    ss << progress.completedSteps << "/" << progress.totalSteps << " steps | ";
    ss << progress.percentComplete << "% | ";
    ss << progress.currentStep;

    return ss.str();
}

std::string IDEAgentIntegration::FormatResultsForPanel(uint32_t taskId) {
    std::string status = m_agent.GetStatus(taskId);
    std::string result = m_agent.GetResult(taskId);

    std::ostringstream ss;
    ss << "=== TASK RESULTS ===\n\n";
    ss << status << "\n";
    ss << "=== STEP RESULTS ===\n\n";
    ss << result;

    return ss.str();
}

bool IDEAgentIntegration::ApproveAndExecute(uint32_t taskId) {
    if (!m_agent.ApprovePlan(taskId)) {
        return false;
    }

    return m_agent.ExecutePlan(taskId, false);
}

bool IDEAgentIntegration::ApproveStep(uint32_t taskId, uint32_t stepId) {
    return m_agent.ApproveStep(taskId, stepId);
}

bool IDEAgentIntegration::RejectApproval(uint32_t taskId, uint32_t stepId,
                                         const std::string& reason) {
    return m_agent.CancelTask(taskId);
}

bool IDEAgentIntegration::CancelTask(uint32_t taskId) {
    return m_agent.CancelTask(taskId);
}

std::string IDEAgentIntegration::GetActiveTasksList() {
    std::ostringstream ss;
    ss << "ACTIVE TASKS:\n";
    ss << "=============\n\n";

    // In real implementation, would iterate through all tasks
    ss << "(Task list would be displayed here)\n";

    return ss.str();
}

std::string IDEAgentIntegration::GetPendingApprovalsDisplay() {
    std::ostringstream ss;
    ss << "PENDING APPROVALS:\n";
    ss << "==================\n\n";

    // In real implementation, would display pending approvals
    ss << "(Pending approvals would be displayed here)\n";

    return ss.str();
}

std::string IDEAgentIntegration::ExtractTaskFromMessage(const std::string& message) {
    // Simple extraction: look for common task keywords
    if (message.find("refactor") != std::string::npos) {
        return "Refactor code";
    } else if (message.find("test") != std::string::npos) {
        return "Add tests";
    } else if (message.find("document") != std::string::npos) {
        return "Generate documentation";
    } else if (message.find("analyze") != std::string::npos) {
        return "Analyze code";
    } else if (message.find("optimize") != std::string::npos) {
        return "Optimize performance";
    } else if (message.find("fix") != std::string::npos) {
        return "Fix issues";
    }

    return message;
}

std::vector<std::string> IDEAgentIntegration::ExtractContextFiles(const std::string& message) {
    std::vector<std::string> files;

    // Simple extraction: look for file paths in message
    // In a real implementation, would use more sophisticated parsing

    return files;
}

void IDEAgentIntegration::OnTaskCompleted(uint32_t taskId, bool success) {
    // Task completed - could trigger UI update
    // In real implementation, would notify UI layer
}

void IDEAgentIntegration::OnApprovalNeeded(uint32_t taskId, const ApprovalRequest& req) {
    // Approval needed - could show dialog or notification
    // In real implementation, would show approval UI
}

void IDEAgentIntegration::OnProgressUpdated(uint32_t taskId,
                                            const AutonomousAgent::TaskProgress& progress) {
    // Progress updated - could update status bar
    // In real implementation, would update UI in real-time
}

} // namespace RawrXD
