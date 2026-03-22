// ============================================================================
// ide_autonomous_integration.cpp — IDE integration implementation
// ============================================================================

#include "ide_autonomous_integration.h"
#include <sstream>

namespace RawrXD {

uint32_t IDEAutonomousIntegration::SubmitTaskFromChat(const std::string& userMessage) {
    std::string task = ExtractTaskFromMessage(userMessage);
    std::vector<std::string> files = ExtractContextFiles(userMessage);

    return m_orchestrator.SubmitTask(task, files, TaskPriority::NORMAL);
}

std::string IDEAutonomousIntegration::FormatPlanForDisplay(uint32_t taskId) {
    auto plan = m_orchestrator.GetPlan(taskId);
    if (!plan) return "Plan not found";

    std::ostringstream ss;
    ss << "📋 Execution Plan\n";
    ss << "================\n\n";
    ss << "Task: " << plan->taskDescription << "\n";
    ss << "Estimated Duration: " << plan->totalEstimatedMs << "ms\n";
    ss << "Steps: " << plan->steps.size() << "\n\n";

    for (size_t i = 0; i < plan->steps.size(); i++) {
        const auto& step = plan->steps[i];
        ss << (i + 1) << ". " << step.action << "\n";
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

        std::string gateStr;
        switch (step.safetyGate) {
            case SafetyGateType::NONE: gateStr = "None"; break;
            case SafetyGateType::CONFIRM: gateStr = "Requires Confirmation"; break;
            case SafetyGateType::PREVIEW: gateStr = "Preview Required"; break;
            case SafetyGateType::ROLLBACK_CAPABLE: gateStr = "Rollback Capable"; break;
            case SafetyGateType::RESOURCE_CHECK: gateStr = "Resource Check"; break;
            default: gateStr = "Unknown"; break;
        }
        ss << "   Safety Gate: " << gateStr << "\n";
        ss << "\n";
    }

    return ss.str();
}

std::string IDEAutonomousIntegration::FormatProgressForStatusBar(uint32_t taskId) {
    auto progress = m_orchestrator.GetProgress(taskId);

    std::ostringstream ss;
    ss << "Task #" << taskId << " | ";
    ss << progress.completedSteps << "/" << progress.totalSteps << " steps | ";
    ss << progress.percentComplete << "% | ";
    ss << progress.currentStep;

    return ss.str();
}

std::string IDEAutonomousIntegration::FormatResultsForPanel(uint32_t taskId) {
    std::string status = m_orchestrator.GetStatus(taskId);
    std::string result = m_orchestrator.GetResult(taskId);

    std::ostringstream ss;
    ss << "=== Task Results ===\n\n";
    ss << status << "\n";
    ss << "=== Step Results ===\n\n";
    ss << result;

    return ss.str();
}

bool IDEAutonomousIntegration::ApproveAndExecute(uint32_t taskId) {
    if (!m_orchestrator.ApprovePlan(taskId)) {
        return false;
    }

    return m_orchestrator.ExecutePlan(taskId, false);
}

bool IDEAutonomousIntegration::CancelTask(uint32_t taskId) {
    return m_orchestrator.CancelTask(taskId);
}

std::string IDEAutonomousIntegration::GetActiveTasksList() {
    std::ostringstream ss;
    ss << "Active Tasks:\n";
    ss << "=============\n\n";

    // In a real implementation, would iterate through all tasks
    // For now, return placeholder
    ss << "(No active tasks)\n";

    return ss.str();
}

std::string IDEAutonomousIntegration::ExtractTaskFromMessage(const std::string& message) {
    // Simple extraction: look for common task keywords
    if (message.find("refactor") != std::string::npos) {
        return "Refactor code";
    } else if (message.find("test") != std::string::npos) {
        return "Add tests";
    } else if (message.find("document") != std::string::npos) {
        return "Generate documentation";
    } else if (message.find("analyze") != std::string::npos) {
        return "Analyze code";
    }

    // Default: use the message as-is
    return message;
}

std::vector<std::string> IDEAutonomousIntegration::ExtractContextFiles(const std::string& message) {
    std::vector<std::string> files;

    // Simple extraction: look for file paths in message
    // In a real implementation, would use more sophisticated parsing
    // For now, return empty (will use current editor context)

    return files;
}

} // namespace RawrXD
