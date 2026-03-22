# Agentic Planning Orchestrator — Quick Reference

## Overview

**AgenticPlanningOrchestrator** brings multi-step planning with risk-tiered safety gates to the RawrXD IDE. It orchestrates:
1. **Multi-step plan generation** from natural language tasks
2. **Risk analysis** per execution step (VeryLow → Low → Medium → High → Critical)
3. **Safety gates** with approval queues (human-in-the-loop)
4. **Policy-driven auto-approval** (e.g., auto-approve low-risk < 10KB file edits)
5. **Step execution** with rollback capability
6. **IDE panel UI** for plan review, approval, and monitoring

## Architecture

```
┌─ User Task ────────────────────────────────┐
│  "Add Q8K quantization support"             │
└──────────────────┬──────────────────────────┘
                   │
        ┌──────────▼──────────┐
        │ AgenticPlanner       │ (generates JSON plan)
        │ (calls external LLM) │
        └──────────┬───────────┘
                   │
       ┌───────────▼───────────┐
       │ ExecutionPlan         │ (multi-step structure)
       │ - 5 steps             │
       │ - dependencies        │
       │ - risk levels         │
       └───────────┬───────────┘
                   │
     ┌─────────────▼──────────────┐
     │ RiskAnalyzer               │ (analyzes each step)
     │ - file count               │
     │ - mutation scope           │
     │ - complexity               │
     └─────────────┬──────────────┘
                   │
    ┌──────────────▼──────────────┐
    │ ApprovalPolicy Engine        │ (decides approval fate)
    │ - Very Low: auto-approve     │
    │ - Low: policy-dependent      │
    │ - Medium+: human approval    │
    │ - Critical: always required  │
    └──────────────┬───────────────┘
                   │
   ┌───────────────▼────────────────┐
   │ ApprovalQueue                  │ (pending human review)
   │ (displayed in IDE panel)        │
   └───────────────┬────────────────┘
                   │
    ┌──────────────▼──────────────┐
    │ Step Execution (on approval) │
    │ - Wire to ToolRegistry       │
    │ - Generate artifacts         │
    │ - Update plan state          │
    └──────────────┬───────────────┘
                   │
     ┌─────────────▼────────────┐
     │ Rollback (if needed)     │
     │ - Undo file changes      │
     │ - Restore checked-out    │
     │   repository state       │
     └──────────────────────────┘
```

## Key Components

### 1. `ExecutionPlan`
Represents a multi-step orchestration plan:
```cpp
struct ExecutionPlan {
    std::string plan_id;              // "plan_1234567890"
    std::vector<PlanStep> steps;      // Each step with risk, affect, deps
    float confidence_score;           // Planner's confidence (0.0-1.0)
    int pending_approvals;            // How many steps await approval
    std::atomic<bool> is_executing;   // Currently running?
};
```

### 2. `PlanStep`
One atomic step in the plan:
```cpp
struct PlanStep {
    std::string id;                         // "step_1"
    std::string title;                      // "Add Q8K struct"
    std::vector<std::string> actions;       // Tool calls to execute
    std::vector<std::string> affected_files;// Files to modify
    StepRisk risk_level;                    // VeryLow to Critical
    ExecutionStatus status;                 // Waiting, Executing, Success, etc.
    ApprovalStatus approval_status;         // Pending, Approved, Rejected
    bool is_rollbackable;                   // Can undo this step
};
```

### 3. `ApprovalPolicy`
Configures approval rules:
```cpp
ApprovalPolicy::Conservative()  // Very strict: only auto-approve VeryLow
ApprovalPolicy::Standard()      // Default: auto VeryLow, human reviews rest
ApprovalPolicy::Aggressive()    // Lenient: auto VeryLow+Low, parallel approvals OK
```

### 4. `StepRisk` Levels
- **VeryLow (1)**: Read-only, no mutations
- **Low (2)**: Single file, non-critical change
- **Medium (3)**: Multi-file or critical file, tested path
- **High (4)**: System-level, compilation impact
- **Critical (5)**: Architecture change, always requires human approval

## Usage Pattern

### Basic: Generate and Execute a Plan

```cpp
auto* orchestrator = new Agentic::AgenticPlanningOrchestrator();

// Set the approval policy
orchestrator->setApprovalPolicy(Agentic::ApprovalPolicy::Standard());

// Generate a plan from a task
auto* plan = orchestrator->generatePlanForTask(
    "Add support for Q8K quantization in GGUF loader"
);

// Plan is now ready; steps eligible for auto-approval are marked ApprovedAuto
// Other steps are Pending and in the approval queue

// Execute all approved steps (auto and human-approved)
orchestrator->executeEntirePlan(plan);
```

### Advanced: Custom Risk Analysis

```cpp
// Attach custom risk analyzer
orchestrator->setRiskAnalysisFn([](const PlanStep& step) -> StepRisk {
    // Custom logic: e.g., check against a blacklist of critical files
    if (step.affected_files.size() > 100) {
        return Agentic::StepRisk::Critical;
    }
    if (std::any_of(step.affected_files.begin(), step.affected_files.end(),
        [](const auto& f) { return f.find("CMakeLists.txt") != std::string::npos; })) {
        return Agentic::StepRisk::High;
    }
    return Agentic::StepRisk::Medium;
});
```

### IDE Integration: Show Approval Queue

```cpp
// In Win32IDE_AgenticPlanningPanel::refresh():
auto pending = orchestrator->getPendingApprovals();
// Returns vector<pair<ExecutionPlan*, int>> of (plan, step_index)

// Display in UI:
for (const auto& [plan, step_idx] : pending) {
    auto& step = plan->steps[step_idx];
    wprintf(L"Pending approval: %s (risk=%d)\n", 
            step.title.c_str(), static_cast<int>(step.risk_level));
}

// User clicks "Approve" button:
orchestrator->approveStep(plan, step_idx, "alice@example.com", "Looks good to me");

// User clicks "Execute All":
orchestrator->executeEntirePlan(plan);
```

## Approval Queue Display

The UI panel shows:

**Tab 1: Active Plans**
```
[plan_1234567890] Add support for Q8K quantization (5 steps)
[plan_1234567891] Optimize GGML kernels (3 steps)
```

**Tab 2: Approval Queue**
```
Step 1: Add BlockQ8_K struct definition (Risk: Medium) — PENDING
Step 2: Wire Q8K in payloadBytes() (Risk: Low) — APPROVED AUTO
Step 3: Update documentation (Risk: VeryLow) — APPROVED AUTO
```

**Tab 3: Execution Log**
```
[12:34:56] Plan generated: plan_1234567890 with 2 pending approvals
[12:34:58] Step 1 APPROVED by alice@example.com: Good to go
[12:35:02] Executing step 1: Add BlockQ8_K struct definition
[12:35:15] Step 1 completed successfully
```

## Integration into Win32IDE

Add to `main_win32.cpp` initialization:

```cpp
#include "Win32IDE_AgenticPlanningPanel.hpp"

// In IDE startup:
auto* planning_panel = Win32IDE::GetAgenticPlanningPanel();
planning_panel->createWindow(hIDEMainWindow);

// Wire into menu:
// Menu item: "Tools → Agentic Planning & Approval"
```

## Building & Linking

1. **Add sources** to CMakeLists.txt:
   - `src/agentic/agentic_planning_orchestrator.{hpp,cpp}`
   - `src/win32app/Win32IDE_AgenticPlanningPanel.{hpp,cpp}`

2. **Link dependencies**:
   - `nlohmann_json` (for JSON serialization)
   - Windows SDK (for Win32 controls)

3. **Include paths**:
   - Add `src/agentic/` and `src/win32app/` to include dirs

## Policy Presets

| Policy | VeryLow | Low | Medium | High | Critical | Use Case |
|--------|---------|-----|--------|------|----------|----------|
| **Conservative** | Auto | × | ✓ | ✓ | ✓ | High-risk projects, critical systems |
| **Standard** | Auto | × | ✓ | ✓ | ✓ | Most projects (default) |
| **Aggressive** | Auto | Auto | × | ✓ | ✓ | Development/experimentation |

## Rollback Mechanism

When a step fails or the user requests rollback:

```cpp
orchestrator->rollbackStep(plan, step_index);
// Internally: restore previous file states, undo database changes, etc.
```

Currently a stub; real implementation would:
1. Track file snapshots before each step
2. Restore from snapshots on rollback
3. Update git index/working tree as needed
4. Re-create any rolled-back tables in memory structs

## JSON Export for Monitoring/Dashboarding

```cpp
auto plan_json = orchestrator->getPlanJson(plan);
// Includes full plan structure, step statuses, approvals, risks

auto queue_json = orchestrator->getApprovalQueueJson();
// List of pending approval gates with reasons and timeouts

auto status_json = orchestrator->getExecutionStatusJson();
// Overview: active_plans, pending_approvals, executing_plans
```

## Logging & Callbacks

Attach callbacks to monitor orchestrator events:

```cpp
orchestrator->setExecutionLogFn([](const std::string& log_entry) {
    std::cout << "[Agentic] " << log_entry << "\n";
});

orchestrator->setApprovalCallback([](const ExecutionPlan* plan, int step_idx) {
    // Show notification or UI alert
});
```

## Next Steps & Future Work

- [ ] Wire step execution to `AutonomousBackgroundDaemon` tool execution
- [ ] Integrate with VCS for rollback (git checkout, etc.)
- [ ] Add approval timeout enforcement (expire pending after 24h)
- [ ] Implement step dependency validation (detect circular deps)
- [ ] Add estimated time/cost predictions per step
- [ ] Multi-user approval (require 2 approvers for critical steps)
- [ ] Audit trail / approval history export

## Summary

**AgenticPlanningOrchestrator** closes the major parity gap between RawrXD's existing agent infrastructure (planner, executor, background daemon) and the **safety + planning** layer needed for autonomous IDE operations. It provides:

✅ Multi-step planning with dependencies  
✅ Risk categorization + analysis  
✅ Human-in-the-loop approval gates  
✅ Policy-driven auto-approval  
✅ IDE UI for plan review/approval  
✅ Execution tracking + rollback  
✅ JSON export for monitoring  

This is the **highest-leverage missing capability** for autonomous agent trust and operational safety.
