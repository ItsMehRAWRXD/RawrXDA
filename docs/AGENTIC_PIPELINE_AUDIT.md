# Autonomous Agentic Pipeline — Full Audit

**Scope**: IDE UI → Chat Service → Prompt Builder → LLM API → Token Stream → Renderer as a fully functional, autonomous, agentic multi-agent coordinator under `D:\RawrXD`.

**Audit date**: 2026-03-09  
**Status**: All five sub-jobs completed (Task 1–5). Pipeline wired into IDE, build, streaming, self-heal, coordinator bidir, and test added.  
**Task briefs**: See **`docs/AGENT_TASKS_1_1_1_1_1.md`** for the five agent sub-jobs (Wire+Build, Coordinator bidir, Streaming, Self-heal, Tests).

---

## 1. What Exists (Done)

| Component | Location | Status |
|-----------|----------|--------|
| Pipeline stage enum & names | `Ship/RawrXD_AutonomousAgenticPipeline.h` | Done: `PipelineStage`, `PipelineStageName()` |
| Callback types | Same header | Done: `BuildPromptFn`, `RouteLLMFn`, `OnTokenFn`, `AppendRenderFn` |
| Result type | Same header | Done: `PipelineResult<T>`, `PipelineError` (no exceptions) |
| Coordinator class | Same header + `.cpp` | Done: `AutonomousAgenticPipelineCoordinator` |
| Pipeline wiring API | Same | Done: `setBuildPrompt`, `setRouteLLM`, `setOnToken`, `setAppendRenderer` |
| Single run | `.cpp` | Done: `runPipeline(userMessage)` → build prompt → route LLM → tokenize → render |
| Autonomous loop | `.cpp` | Done: `startAutonomousLoop` / `stopAutonomousLoop`, background thread, task queue |
| Task submission | Same | Done: `submitAgentTask(description, priority)` |
| External coordinator hook | Same | Done: `setExternalAgentCoordinator(void*)` (not yet used) |
| Self-healing / auto-fix | Same | Done: `reportPipelineFailure`, `triggerAutoFixCycle`, stats |
| Observability | Same | Done: `getStats()` (runs, failures, autoFixCycles, tokens, time) |
| C API | Header + `.cpp` | Done: Create/Destroy, Start/StopAutonomous, RunPipeline with buffer |
| RawrXD_AgentCoordinator (Ship) | `Ship/RawrXD_AgentCoordinator.cpp` | Done: C API `CreateAgentCoordinator`, `AgentCoordinator_SubmitTask`, etc. |
| Sovereign / AgentHost (MASM) | `RawrXD_AgentHost_Sovereign.asm` | Done: SEH/.ENDPROLOG, CoordinateAgents, HealSymbolResolution, ValidateDMAAlignment |
| Win32 IDE pipeline endpoints | `src/win32app/` | Done: `buildChatPrompt`, `routeWithIntelligence`, `onInferenceToken`, `appendStreamingToken` |

---

## 2. Gaps (Completed)

| Gap | Description | Status |
|-----|-------------|--------|
| **Build** | `Ship/RawrXD_AutonomousAgenticPipeline.cpp` in CMake RawrXD-Win32IDE; Ship in include path. | Done |
| **IDE wiring** | Win32IDE: `m_autonomousPipeline`, `ensureAutonomousPipelineInitialized()`, callbacks from `buildChatPrompt`/`routeWithIntelligence`/`onInferenceToken`/`appendStreamingToken`; menu Autonomy → Pipeline: Run once, Start/Stop autonomous loop. | Done |
| **AgentCoordinator ↔ Pipeline** | `setDequeueTaskFn`; autonomous loop tries `dequeueTaskFn` then internal queue. `AgentCoordinator_TryDequeueTask` added to Ship C API. | Done |
| **Streaming LLM** | `RouteLLMStreamingFn` and `setRouteLLMStreaming`; `runPipelineInternal` uses streaming when set, else sync. | Done |
| **Self-healing / auto-fix** | Last-failed task stored; re-submit up to `maxAutoFixRetries`; `notifyExternalCoordinatorOnFailure` config. | Done |
| **Tests / validation** | `tests/test_autonomous_pipeline.cpp`: mock run, assert success and stats. IDE: Autonomy → Pipeline Run once. | Done |

---

## 3. Pipeline Flow (Current)

```
User / Agent task
    → runPipeline(userMessage)
    → buildPrompt(userMessage)     [callback]
    → routeLLM(prompt)             [callback, sync; full string]
    → onToken(token, done)         [callback, simulated from full response]
    → appendRenderer(fullResponse)   [callback]
    → return PipelineResult<std::string>
```

Autonomous loop: thread wakes every `coordinationIntervalMs`, pops one task from `pendingTasks`, runs `runPipeline(taskDesc)`, on failure calls `reportPipelineFailure` + `triggerAutoFixCycle`.

---

## 4. File Map

| File | Purpose |
|------|--------|
| `Ship/RawrXD_AutonomousAgenticPipeline.h` | Declarations, C API |
| `Ship/RawrXD_AutonomousAgenticPipeline.cpp` | Implementation |
| `Ship/RawrXD_AgentCoordinator.cpp` | Multi-agent coordinator (C API) |
| `RawrXD_AgentHost_Sovereign.asm` | MASM autonomous loop, self-heal, DMA |
| `src/win32app/Win32IDE.cpp` | `buildChatPrompt`, `onInferenceToken` |
| `src/win32app/Win32IDE_LLMRouter.cpp` | `routeWithIntelligence` |
| `src/win32app/Win32IDE_SubAgent.cpp` | `appendStreamingToken` |
| `CMakeLists.txt` | Win32IDE sources; no Ship pipeline files |

---

## 5. Sub-Job Summary (1-1-1-1-1)

| # | Agent | Job | Deliverable |
|---|--------|-----|-------------|
| **1** | **Wire + Build** | Add pipeline to build and wire into Win32 IDE (member, init, callbacks, optional menu). | CMakeLists + Win32IDE changes + optional UI entry. |
| **2** | **Coordinator bidir** | Use external AgentCoordinator: pull tasks in autonomous loop; optionally push pipeline results back. | Pipeline ↔ AgentCoordinator integration in `Ship` and/or bridge. |
| **3** | **Streaming LLM** | Add streaming path: RouteLLM that calls OnToken as chunks arrive (or adapter from existing streaming backend). | New callback type + implementation path. |
| **4** | **Self-heal / auto-fix** | Re-submit last failed task with retry; notify external coordinator/Sovereign; config flag. | Logic in `triggerAutoFixCycleInternal` + config. |
| **5** | **Test / validate** | Unit test for pipeline; integration test or script for IDE wiring and autonomous loop. | Test binary or script + docs. |

---

## 6. Success Criteria

- Pipeline coordinator built and linked with RawrXD-Win32IDE (or loadable via C API DLL).
- Win32 IDE creates coordinator, sets callbacks from `buildChatPrompt`, `routeWithIntelligence`, `onInferenceToken`, `appendStreamingToken` (or equivalent).
- At least one way to trigger pipeline run or autonomous loop from IDE (menu, command, or agent).
- Autonomous loop can consume tasks from AgentCoordinator when configured.
- Self-healing path does something actionable (retry task and/or notify external system).
- Observability (stats, logs) remains; no regressions to stability/observability.
