# AgenticPlanningOrchestrator (Win32 IDE)

**Scope:** Native plan dialog (`Win32IDE_PlanExecutor.cpp`) — not Electron.

## Role

Unifies **risk tiering**, **workspace/shell safety gates**, and **human-in-the-loop** approval before `executePlan()` runs.

| Component | Location |
|-----------|----------|
| Types (`PlanRiskTier`, `PlanMutationGate`, `PlanExecutionPolicy`) | `src/win32app/Win32IDE_Types.h` |
| Orchestrator API | `src/full_agentic_ide/AgenticPlanningOrchestrator.{h,cpp}` |
| UI (dual approve, ListView “Gate” column) | `src/win32app/Win32IDE_PlanExecutor.cpp` |

## Flow

1. Model returns structured steps → `parsePlanSteps()` fills `AgentPlan`.
2. `classifyPlan()` sets `riskTier` (from `RISK:` line + step-type heuristics) and initial `mutationGate`:
   - Low + read-only types (analysis/verification) → `AutoApproved`
   - Otherwise → `AwaitingHuman`
   - Critical tier still requires full dialog approval path
3. `applyWorkspaceSafetyGates(workspaceRoot)`:
   - Target paths must stay under workspace (no `..`); else `SafetyBlocked`
   - Shell steps: denylist on title/description (`rm -rf`, `format`, etc.)
4. Dialog:
   - **Approve all** → `FullApproval` + `approveAllPendingMutations()` → run all non-blocked steps
   - **Low-risk only** → `SafeStepsOnly` + `approveLowRiskOnly()` → only `riskTier == Low` execute; others `SkippedByPolicy`
5. `executePlan()` skips steps where `stepShouldExecute()` is false (blocked / policy / not approved). **Rollback** for failed code-edit steps unchanged (pre-step file snapshot).

## Env / tuning

No env vars yet; thresholds are code-defined in `AgenticPlanningOrchestrator.cpp`.
