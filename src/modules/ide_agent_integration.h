// ============================================================================
// ide_agent_integration.h — Win32 IDE integration for autonomous agent
// ============================================================================
// Bridges AutonomousAgent to IDE UI with:
//   • Chat command parsing
//   • Plan visualization
//   • Approval UI
//   • Progress tracking
//   • Result display
// ============================================================================

#pragma once

#include "autonomous_agent.h"
#include <string>
#include <vector>
#include <functional>

namespace RawrXD {

// ============================================================================
// IDEAgentIntegration — Main IDE integration class
// ============================================================================
class IDEAgentIntegration {
public:
    IDEAgentIntegration(AutonomousAgent& agent)
        : m_agent(agent) {
        // Set up callbacks
        m_agent.OnTaskCompletion([this](uint32_t taskId, bool success) {
            OnTaskCompleted(taskId, success);
        });

        m_agent.OnApprovalNeeded([this](uint32_t taskId, const ApprovalRequest& req) {
            OnApprovalNeeded(taskId, req);
        });

        m_agent.OnProgress([this](uint32_t taskId, const AutonomousAgent::TaskProgress& progress) {
            OnProgressUpdated(taskId, progress);
        });
    }

    /// Submit task from IDE chat
    uint32_t SubmitTaskFromChat(const std::string& userMessage,
                                const std::string& workspacePath = "");

    /// Get formatted plan for display
    std::string FormatPlanForDisplay(uint32_t taskId);

    /// Get formatted approval request
    std::string FormatApprovalRequest(const ApprovalRequest& req);

    /// Get formatted progress for status bar
    std::string FormatProgressForStatusBar(uint32_t taskId);

    /// Get formatted results
    std::string FormatResultsForPanel(uint32_t taskId);

    /// Approve plan and execute
    bool ApproveAndExecute(uint32_t taskId);

    /// Approve specific step
    bool ApproveStep(uint32_t taskId, uint32_t stepId);

    /// Reject approval request
    bool RejectApproval(uint32_t taskId, uint32_t stepId, const std::string& reason);

    /// Cancel task
    bool CancelTask(uint32_t taskId);

    /// Get all active tasks
    std::string GetActiveTasksList();

    /// Get pending approvals
    std::string GetPendingApprovalsDisplay();

    /// Enable/disable dry-run mode
    void SetDryRunMode(bool enabled) {
        m_agent.SetDryRunMode(enabled);
    }

    /// Enable/disable reflection
    void SetReflectionEnabled(bool enabled) {
        m_agent.SetReflectionEnabled(enabled);
    }

private:
    AutonomousAgent& m_agent;

    std::string ExtractTaskFromMessage(const std::string& message);
    std::vector<std::string> ExtractContextFiles(const std::string& message);

    void OnTaskCompleted(uint32_t taskId, bool success);
    void OnApprovalNeeded(uint32_t taskId, const ApprovalRequest& req);
    void OnProgressUpdated(uint32_t taskId, const AutonomousAgent::TaskProgress& progress);
};

} // namespace RawrXD
