# RawrXD Agentic / Autonomous Features — Audit and Additions

**Date**: 2026-02-12  
**Scope**: Audit of agentic and autonomous capabilities; implementation of missing roadmap items.

---

## 1. Audit Summary

### 1.1 Existing (Implemented)

| Component | Location | Status |
|-----------|----------|--------|
| **ModelInvoker** | `src/agent/model_invoker.hpp/.cpp` | ✅ Full: Ollama, Claude, OpenAI; fallback chain; cache; plan parse/validate |
| **ActionExecutor** | `src/agent/action_executor.hpp/.cpp` | ✅ Full: FileEdit, SearchFiles, RunBuild, ExecuteTests, CommitGit, InvokeCommand, RecursiveAgent, QueryUser; backup/rollback; process run (Win32/POSIX) |
| **IDEAgentBridge** | `src/agent/ide_agent_bridge.hpp/.cpp` | ✅ Wish→plan→execute; approval flow; dry-run; history |
| **Planner** | `src/agent/planner.hpp/.cpp` | ✅ Domain logic: planQuantKernel, planRelease, planWebProject, planSelfReplication, planBulkFix, planGeneric |
| **AgenticFailureDetector** | `src/agent/agentic_failure_detector.hpp/.cpp` | ✅ Refusal, hallucination, format, loop, token, resource, timeout, safety |
| **SelfPatch** | `src/agent/self_patch.hpp/.cpp` | ✅ addKernel, addCpp, hotReload, patchFile; callbacks |
| **EditorAgentIntegration** | `src/gui/editor_agent_integration.hpp/.cpp` | ✅ Win32 ghost text; trigger/accept; overlay; C API |
| **AutonomousRecoveryOrchestrator** | `src/agentic/autonomous_recovery_orchestrator.hpp/.cpp` | ✅ T4: classify→symbolize→recover→verify→commit; strategies; cooldown |
| **Digestion engine** | `src/win32app/digestion_engine_stub.cpp` | ✅ Stub with real line/comment/metrics and JSON report |

### 1.2 Gaps Addressed in This Pass

| Gap | Change |
|-----|--------|
| **IDEAgentBridge ↔ ActionExecutor types** | Bridge used `nlohmann::json` in callbacks while executor uses `JsonValue` (simple_json). Fixed by converting `JsonValue` → `nlohmann::json` in bridge callbacks and passing `JsonValue::parse(plan.dump())` into `executePlan()`. |
| **IDEAgentBridge callback helpers** | Replaced undefined `agentError()`, `agentCompleted()`, etc. with direct `onAgentError`, `onAgentCompleted`, … calls. |
| **Pre-execution safety** | Added to **AgenticFailureDetector**: `validateActionBeforeExecution(ActionSummary)`, `isDangerousCommand()`, `wouldCauseDataLoss()`, `suggestRecoveryAction(FailureInfo)`. |
| **EvalFramework** | Did not exist. Added **EvalFramework** (`src/agent/eval_framework.hpp/.cpp`): test cases, pluggable executor, report, execution log. |
| **Autonomous retry/backoff** | Added to **IDEAgentBridge**: `setRetryPolicy(maxRetries, initialBackoffMs)`, exponential backoff on plan-generation failure, `retryPlanGeneration()`. |

---

## 2. New and Updated APIs

### 2.1 AgenticFailureDetector

```cpp
struct ActionSummary {
    std::string type;    // e.g. "file_edit", "invoke_command"
    std::string target;
    std::string params;
};

bool validateActionBeforeExecution(const ActionSummary& action);
bool isDangerousCommand(const std::string& commandStr) const;
bool wouldCauseDataLoss(const ActionSummary& action) const;
std::string suggestRecoveryAction(const FailureInfo& failure) const;
```

### 2.2 IDEAgentBridge

```cpp
void setRetryPolicy(int maxRetries, int initialBackoffMs = 1000);
```

- On plan generation failure, if `m_retriesLeft > 0`, retries with exponential backoff (capped at 30s).

### 2.3 EvalFramework (new)

```cpp
struct EvalTestCase {
    std::string wish;
    std::string expectedOutcome;
    int maxExecutionTimeMs = 30000;
    bool requiresNetwork = false;
};

struct EvalRunResult { std::string wish; bool success; int elapsedMs; std::string error; std::string resultSummary; };
struct EvalReport { int totalRuns; int successCount; double successRate; std::vector<EvalRunResult> results; };

using EvalExecutorFn = std::function<EvalRunResult(const std::string& wish, int timeoutMs)>;

void setExecutor(EvalExecutorFn fn);
void setLogPath(const std::string& path);
EvalReport runEvaluation(const std::vector<EvalTestCase>& cases);
void logExecution(const std::string& wish, bool success, int timeMs, const std::string& error);
```

---

## 3. Build

- **eval_framework.cpp** added to `CMakeLists.txt` in all agent source lists that include `model_invoker`/`planner`.

---

## 4. Remaining / Optional (from roadmap)

- **Wiring FailureDetector into executor**: Before each action, call `AgenticFailureDetector::validateActionBeforeExecution(ActionSummary)` and skip or escalate if false.
- **EvalFramework executor**: Implement `EvalExecutorFn` using `IDEAgentBridge::executeWish` (or equivalent) and run regression suites.
- **EditorAgentIntegration ↔ IDEAgentBridge**: Connect editor ghost-text “plan wish” to bridge so suggestions come from the same pipeline.
- **AutonomousRecoveryOrchestrator + LLM**: Configure `setLLMFixGenerator` for T4 source-level fixes.

---

## 5. Files Touched

| File | Change |
|------|--------|
| `src/agent/ide_agent_bridge.hpp` | Retry policy; `m_lastParams`, `m_retriesLeft`, `m_currentRetryBackoffMs`, `retryPlanGeneration()` |
| `src/agent/ide_agent_bridge.cpp` | JsonValue↔nlohmann conversion; callback fixes; retry with exponential backoff |
| `src/agent/agentic_failure_detector.hpp` | `ActionSummary`; `validateActionBeforeExecution`, `isDangerousCommand`, `wouldCauseDataLoss`, `suggestRecoveryAction` |
| `src/agent/agentic_failure_detector.cpp` | Implementations for above; dangerous-command patterns |
| `src/agent/eval_framework.hpp` | New |
| `src/agent/eval_framework.cpp` | New |
| `CMakeLists.txt` | Add `src/agent/eval_framework.cpp` in agent source lists |
| `AGENTIC_AUDIT_AND_FEATURES.md` | This document |
