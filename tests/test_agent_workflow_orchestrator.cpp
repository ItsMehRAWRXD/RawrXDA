#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "../src/agentic/agent_workflow_orchestrator.cpp"

namespace {

int g_passed = 0;
int g_failed = 0;

void expect(bool condition, const char* message) {
    if (condition) {
        std::cout << "[PASS] " << message << std::endl;
        ++g_passed;
    } else {
        std::cout << "[FAIL] " << message << std::endl;
        ++g_failed;
    }
}

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

} // namespace

int main() {
    namespace fs = std::filesystem;
    using RawrXD::Agentic::AgentWorkflowOrchestrator;
    using RawrXD::Agentic::ApprovalLevel;
    using RawrXD::Agentic::ApprovalRequest;
    using RawrXD::Agentic::WorkflowEvent;
    using RawrXD::Agentic::WorkflowEventKind;

    std::cout << "========================================" << std::endl;
    std::cout << "Agent Workflow Orchestrator Smoke Test" << std::endl;
    std::cout << "========================================" << std::endl;

    auto& orchestrator = AgentWorkflowOrchestrator::instance();
    orchestrator.clear();

    std::vector<WorkflowEventKind> firstRunEvents;
    orchestrator.setEventCallback([&firstRunEvents](const WorkflowEvent& event) {
        firstRunEvents.push_back(event.kind);
    });

    bool rejectFirstModify = true;
    orchestrator.setApprovalCallback([&rejectFirstModify](const ApprovalRequest& request) {
        if (rejectFirstModify && request.taskId == "exec_0" && request.level == ApprovalLevel::Modify) {
            rejectFirstModify = false;
            return false;
        }
        return true;
    });

    const std::string workflowId = orchestrator.buildStandardWorkflow(
        "Autonomy Smoke",
        "Execute real autonomy path",
        {"Patch code", "Validate change"});

    const fs::path checkpointPath = fs::temp_directory_path() / "rawrxd_agent_workflow_orchestrator.chk";
    std::error_code error;
    fs::remove(checkpointPath, error);

    expect(orchestrator.setCheckpointPath(workflowId, checkpointPath.string()), "Checkpoint path configured");

    const bool firstRunOk = orchestrator.executeWorkflow(workflowId);
    expect(!firstRunOk, "First run fails on approval gate");
    expect(fs::exists(checkpointPath), "Checkpoint file written after partial execution");

    const std::string failedStatus = orchestrator.getWorkflowStatus(workflowId);
    expect(contains(failedStatus, "[Failed]"), "Status reports failed workflow");
    expect(contains(failedStatus, "[RolledBack] plan"), "Rollback applied to completed predecessor task");

    bool sawRollbackStart = false;
    bool sawCheckpointSave = false;
    for (WorkflowEventKind kind : firstRunEvents) {
        if (kind == WorkflowEventKind::RollbackStarted) {
            sawRollbackStart = true;
        }
        if (kind == WorkflowEventKind::CheckpointSaved) {
            sawCheckpointSave = true;
        }
    }
    expect(sawRollbackStart, "Rollback event emitted on failed run");
    expect(sawCheckpointSave, "Checkpoint event emitted on failed run");

    orchestrator.clear();

    std::vector<std::string> resumedTaskOrder;
    orchestrator.setEventCallback([&resumedTaskOrder](const WorkflowEvent& event) {
        if (event.kind == WorkflowEventKind::TaskStarted) {
            resumedTaskOrder.push_back(event.taskId);
        }
    });
    orchestrator.setApprovalCallback([](const ApprovalRequest&) {
        return true;
    });

    const bool resumedOk = orchestrator.resumeWorkflow(checkpointPath.string());
    expect(resumedOk, "Resume from checkpoint succeeds after approvals are granted");

    const auto resumedIds = orchestrator.listWorkflows();
    expect(resumedIds.size() == 1, "Exactly one resumed workflow is active in orchestrator state");

    if (resumedIds.size() != 1) {
        orchestrator.clear();
        orchestrator.setEventCallback({});
        orchestrator.setApprovalCallback({});
        fs::remove(checkpointPath, error);
        std::cout << "========================================" << std::endl;
        std::cout << "Passed: " << g_passed << std::endl;
        std::cout << "Failed: " << g_failed << std::endl;
        std::cout << "========================================" << std::endl;
        return 1;
    }

    const std::string resumedStatus = orchestrator.getWorkflowStatus(resumedIds.front());
    expect(contains(resumedStatus, "[Succeeded]"), "Resumed workflow completes successfully");
    expect(contains(resumedStatus, "[Succeeded] plan"), "Plan task reruns successfully on resume");
    expect(contains(resumedStatus, "[Succeeded] exec_0"), "First execution step succeeds on resume");
    expect(contains(resumedStatus, "[Succeeded] exec_1"), "Second execution step succeeds on resume");
    expect(contains(resumedStatus, "[Succeeded] validate"), "Validate task succeeds on resume");
    expect(contains(resumedStatus, "[Succeeded] commit"), "Commit task succeeds on resume");

    expect(resumedTaskOrder.size() == 5, "Resume executes all five tasks in workflow");
    expect(resumedTaskOrder.size() >= 5 && resumedTaskOrder[0] == "plan", "Resume starts with plan task");
    expect(resumedTaskOrder.size() >= 5 && resumedTaskOrder[1] == "exec_0", "First execution step follows plan");
    expect(resumedTaskOrder.size() >= 5 && resumedTaskOrder[2] == "exec_1", "Second execution step follows first step");
    expect(resumedTaskOrder.size() >= 5 && resumedTaskOrder[3] == "validate", "Validate runs after execution steps");
    expect(resumedTaskOrder.size() >= 5 && resumedTaskOrder[4] == "commit", "Commit runs last");

    orchestrator.clear();
    orchestrator.setEventCallback({});
    orchestrator.setApprovalCallback({});
    fs::remove(checkpointPath, error);

    std::cout << "========================================" << std::endl;
    std::cout << "Passed: " << g_passed << std::endl;
    std::cout << "Failed: " << g_failed << std::endl;
    std::cout << "========================================" << std::endl;

    return g_failed == 0 ? 0 : 1;
}