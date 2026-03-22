// ============================================================================
// ide_autonomous_integration.h — Win32 IDE integration for orchestrator
// ============================================================================
// Bridges AutonomousOrchestrator to the Win32 IDE UI, providing:
//   • Task submission from chat/command palette
//   • Real-time plan visualization
//   • Step-by-step execution UI
//   • Progress tracking and cancellation
//   • Result display and rollback controls
// ============================================================================

#pragma once

#include "autonomous_orchestrator.h"
#include <string>
#include <functional>

namespace RawrXD {

// ============================================================================
// IDEAutonomousIntegration — Main integration class
// ============================================================================
class IDEAutonomousIntegration {
public:
    IDEAutonomousIntegration(AutonomousOrchestrator& orchestrator)
        : m_orchestrator(orchestrator) {}

    /// Submit a task from IDE chat/command
    uint32_t SubmitTaskFromChat(const std::string& userMessage);

    /// Get formatted plan for UI display
    std::string FormatPlanForDisplay(uint32_t taskId);

    /// Get formatted progress for status bar
    std::string FormatProgressForStatusBar(uint32_t taskId);

    /// Get formatted results for output panel
    std::string FormatResultsForPanel(uint32_t taskId);

    /// Approve and execute plan
    bool ApproveAndExecute(uint32_t taskId);

    /// Cancel running task
    bool CancelTask(uint32_t taskId);

    /// Get all active tasks for task list
    std::string GetActiveTasksList();

    /// Register callback for task completion
    using TaskCompletionCallback = std::function<void(uint32_t taskId, bool success)>;
    void OnTaskCompletion(TaskCompletionCallback callback) {
        m_completionCallback = callback;
    }

    /// Register callback for step completion
    using StepCompletionCallback = std::function<void(uint32_t taskId, uint32_t stepId, bool success)>;
    void OnStepCompletion(StepCompletionCallback callback) {
        m_stepCallback = callback;
    }

private:
    AutonomousOrchestrator& m_orchestrator;
    TaskCompletionCallback m_completionCallback;
    StepCompletionCallback m_stepCallback;

    std::string ExtractTaskFromMessage(const std::string& message);
    std::vector<std::string> ExtractContextFiles(const std::string& message);
};

} // namespace RawrXD
