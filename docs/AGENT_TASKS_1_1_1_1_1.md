# Agent Tasks 1-1-1-1-1 — Autonomous Agentic Pipeline

**Parent audit**: `docs/AGENTIC_PIPELINE_AUDIT.md`  
**Goal**: Finish the autonomous, agentic multi-agent coordinator (IDE → Chat → Prompt → LLM → Token → Renderer) by completing five discrete sub-jobs. Each task is assigned to one agent (1-1-1-1-1).

---

## Task 1 — Wire pipeline into Win32 IDE and add to build

**Agent**: 1  
**Owner**: Wire + Build

**Objective**  
Make the Autonomous Agentic Pipeline Coordinator part of the IDE process and callable from the IDE: build it, create and own it in Win32IDE, set callbacks from existing IDE methods, and expose at least one way to run it (e.g. menu or command).

**Requirements**  
1. **Build**  
   - Add `Ship/RawrXD_AutonomousAgenticPipeline.cpp` to the RawrXD-Win32IDE target in `CMakeLists.txt` (or equivalent so the coordinator is compiled and linked).  
   - Ensure `Ship/RawrXD_AutonomousAgenticPipeline.h` is on the include path for Win32IDE sources (e.g. `include_directories` or `target_include_directories` with `Ship` or project root).

2. **Win32IDE ownership**  
   - In `Win32IDE` (e.g. `Win32IDE.h` / `Win32IDE.cpp`):  
     - Add a member: `std::unique_ptr<RawrXD::AutonomousAgenticPipelineCoordinator> m_autonomousPipeline` (or equivalent; forward-declare if needed).  
     - In a suitable init path (e.g. after backend/chat is ready), create the coordinator and set:  
       - `setBuildPrompt` → bound to `buildChatPrompt`  
       - `setRouteLLM` → bound to `routeWithIntelligence`  
       - `setOnToken` → bound to `onInferenceToken` (or equivalent streaming callback)  
       - `setAppendRenderer` → bound to `appendStreamingToken` or `appendCopilotResponse`  
     - In shutdown, call `stopAutonomousLoop()` and destroy the coordinator.

3. **Trigger (pick one or both)**  
   - Add a menu item or command that runs one pipeline cycle, e.g. `runPipeline(userMessage)` with a fixed or edit-buffer message.  
   - And/or add a menu item or command that starts/stops the autonomous loop (`startAutonomousLoop` / `stopAutonomousLoop`).

**Constraints**  
- No Qt; Win32 + STL only.  
- Use existing IDE methods; do not change their signatures unless necessary.  
- Prefer minimal change set: only the files needed for build, member, init, callbacks, and trigger(s).

**Deliverables**  
- CMakeLists.txt (or build script) changes so pipeline coordinator is built and linked.  
- Win32IDE changes (header + cpp) for member, init, callback wiring, and optional menu/command.  
- Short note in code or in this doc describing where init and trigger are.

**Acceptance**  
- IDE build succeeds with pipeline coordinator.  
- At least one way from the IDE to run the pipeline or start the autonomous loop.

---

## Task 2 — Bidirectional Pipeline ↔ AgentCoordinator

**Agent**: 2  
**Owner**: Coordinator integration

**Objective**  
Connect the autonomous pipeline to the existing RawrXD AgentCoordinator (Ship C API) so that: (a) the pipeline’s autonomous loop can pull tasks from the coordinator, and (b) optionally report results or completion back.

**Requirements**  
1. **Use C API**  
   - When `setExternalAgentCoordinator(void*)` is set with a non-null pointer, treat it as `AgentCoordinator*` from `Ship/RawrXD_AgentCoordinator.cpp` (C API).  
   - Declare or include the C API in a way that the pipeline (or a small bridge in the same binary/DLL) can call:  
     - At least: get pending count and “take” or “dequeue” a task (if the C API exposes it), or equivalent (e.g. submit from coordinator into pipeline’s queue).  
   - Document the expected C API symbols (e.g. `CreateAgentCoordinator`, `AgentCoordinator_SubmitTask`, `AgentCoordinator_GetPendingTaskCount`, and any needed to dequeue or get task description).

2. **Autonomous loop**  
   - In the autonomous loop, when `externalAgentCoordinator` is set:  
     - Call the coordinator to obtain the next task (description + priority if available).  
     - If a task is returned, run `runPipeline(description)` (or equivalent).  
     - On success/failure, optionally update the coordinator or pipeline stats (e.g. so the coordinator can mark task completed/failed).  
   - If the C API does not support dequeue, define a minimal contract (e.g. “coordinator pushes into pipeline’s `submitAgentTask`” via an event or callback) and implement that.

3. **No breaking changes**  
   - Pipeline remains usable when `setExternalAgentCoordinator(nullptr)`; autonomous loop continues to use only its internal `pendingTasks` queue.

**Constraints**  
- Do not change the existing AgentCoordinator C API unless the audit agrees. Prefer adapter in pipeline/bridge code.  
- Keep dependencies one-way where possible: pipeline calls coordinator; avoid coordinator depending on pipeline types.

**Deliverables**  
- Code in `Ship/RawrXD_AutonomousAgenticPipeline.cpp` (and header if needed) to:  
  - Resolve and use external coordinator (C API).  
  - In autonomous loop, pull task from coordinator when set, run pipeline, optionally report back.  
- Short doc or comments: C API functions used and the task flow (pull vs push).

**Acceptance**  
- With external coordinator set, autonomous loop consumes tasks from it when available.  
- With external coordinator null, behavior unchanged (internal queue only).

---

## Task 3 — Streaming LLM path in pipeline

**Agent**: 3  
**Owner**: Streaming

**Objective**  
Support a streaming path so that when the LLM backend streams tokens, the pipeline can forward them to the renderer (and token callback) as they arrive, instead of waiting for the full response and then simulating a token stream.

**Requirements**  
1. **Streaming callback type**  
   - Add a new callback type (e.g. `RouteLLMStreamingFn`) with signature along the lines of:  
     - `void(const std::string& prompt, OnTokenFn onToken)`  
     - or `bool(const std::string& prompt, OnTokenFn onToken)` if cancellation/error is needed.  
   - Document that the implementation should call `onToken(token, isFinal)` for each chunk and call `onToken("", true)` when done (or on error).

2. **Pipeline support**  
   - In `AutonomousAgenticPipelineCoordinator`:  
     - Add `setRouteLLMStreaming(RouteLLMStreamingFn fn)` (or equivalent).  
     - In `runPipeline` / `runPipelineInternal`:  
       - If a streaming route is set, use it: call it with the built prompt and the pipeline’s `onToken` (and optionally `appendRenderer` for each chunk or only on final).  
       - If only the sync `RouteLLM` is set, keep current behavior (sync call, then simulate token stream from full response).  
   - Ensure thread safety if the streaming callback is invoked from another thread (e.g. marshal to pipeline thread or document that callbacks must be thread-safe).

3. **IDE wiring (optional)**  
   - If the existing IDE has a streaming inference path (e.g. a method that takes an `OnToken`-like callback), add a note or one-line example in the audit or in Win32IDE of how to set `setRouteLLMStreaming` to that method.  
   - Not required to implement full IDE streaming in this task if it would require large refactors; documenting the contract is enough.

**Constraints**  
- Backward compatible: when only sync `RouteLLM` is set, behavior unchanged.  
- No Qt; Win32 + STL only.

**Deliverables**  
- Header: new callback type and `setRouteLLMStreaming`.  
- Implementation: use streaming path when set; fallback to sync + simulated tokens otherwise.  
- Brief comment or doc on threading and how to wire from IDE streaming backend.

**Acceptance**  
- When streaming route is set and used, `onToken` is called incrementally (no need to wait for full response before first token).  
- When only sync route is set, existing behavior preserved.

---

## Task 4 — Self-healing and auto-fix integration

**Agent**: 4  
**Owner**: Self-healing

**Objective**  
Make `triggerAutoFixCycle` and failure handling actionable: re-submit last failed task with retry limit, and optionally notify an external coordinator or Sovereign/AgentHost.

**Requirements**  
1. **Last failed task**  
   - In the pipeline implementation, store the last failed task (e.g. `lastFailedMessage` and `lastFailedRetryCount`).  
   - In `triggerAutoFixCycleInternal`:  
     - If `lastFailedRetryCount < config.maxAutoFixRetries`, re-submit the last failed message (e.g. push back into `pendingTasks` or call `runPipeline` again), increment retry count, and log.  
     - If retry limit reached, do not re-submit; optionally call a “failure exhausted” callback or set a flag for observability.

2. **Config**  
   - Add a config option, e.g. `notifyExternalCoordinatorOnFailure` (bool).  
   - When true and `externalAgentCoordinator` is set, on pipeline failure (and optionally on “retries exhausted”) call into the external coordinator (e.g. C API to report failure) if such a function exists.  
   - Document the expected C API for “report failure” (e.g. symbol name and signature); if the current AgentCoordinator has no such API, document “reserved for future” and leave a single call site commented or guarded.

3. **Sovereign / AgentHost**  
   - Optionally: if the codebase has a way to invoke Sovereign self-healing (e.g. `HealSymbolResolution` or auto-fix cycle) from C++, add a single call or callback in `triggerAutoFixCycleInternal` (e.g. “onAutoFixTriggered” callback or a known symbol).  
   - If no C++ call path exists, document “Sovereign integration: reserved for MASM/C bridge” and leave a clear TODO.

**Constraints**  
- Do not remove or weaken existing logging/observability.  
- Retry logic must be bounded by `maxAutoFixRetries`.

**Deliverables**  
- Implementation: last-failed task storage, re-submit with retry count, config flag, and optional external notify.  
- Config field(s) in `Config` and header.  
- Short doc or comments: retry semantics and optional coordinator/Sovereign hook.

**Acceptance**  
- After a pipeline failure, auto-fix re-submits the same task up to `maxAutoFixRetries` times.  
- When retries exhausted, no infinite re-submit.  
- Optional external notification is documented and, if implemented, callable.

---

## Task 5 — Tests and validation for pipeline and IDE wiring

**Agent**: 5  
**Owner**: Test / validate

**Objective**  
Add automated tests and a minimal validation path for the autonomous agentic pipeline and, where applicable, IDE wiring.

**Requirements**  
1. **Unit test (pipeline)**  
   - Add at least one test (e.g. Catch2 or project’s test harness) that:  
     - Creates `AutonomousAgenticPipelineCoordinator`.  
     - Sets mock callbacks (build prompt → identity or fixed string; route LLM → return fixed string; onToken/appendRenderer → capture or count).  
     - Calls `runPipeline("test message")`.  
     - Asserts success and that the final response (or token count / append count) matches expectations.  
   - If the project has no test runner in-tree, add a small standalone executable or script that does the same (e.g. C++ main that runs the above and exits 0 on success).

2. **Autonomous loop**  
   - One test or script that:  
     - Submits a task via `submitAgentTask`.  
     - Starts the autonomous loop.  
     - (Optional) Waits for a short period or until one pipeline run completes (e.g. mock route returns quickly).  
     - Stops the loop.  
     - Asserts that stats (e.g. `pipelineRuns` or `getStats()`) reflect at least one run.  
   - Guard or skip if running the loop in test is not possible (e.g. no thread support in test env).

3. **IDE wiring (optional)**  
   - If Task 1 is done: add a short validation step (e.g. in docs or as a manual test checklist) that:  
     - Build and run IDE; trigger “Run pipeline” or “Start autonomous loop” once.  
     - Confirm no crash and that a response appears in the UI or logs.  
   - Document where to find the trigger (menu name, command id, or script).

**Constraints**  
- Tests must not require network or real LLM; use mocks.  
- Follow project’s test style (Catch2, GTest, or script).

**Deliverables**  
- Unit test(s) for pipeline run and optionally autonomous loop.  
- Brief validation note or checklist for IDE wiring in `docs/AGENTIC_PIPELINE_AUDIT.md` or `AGENT_TASKS_1_1_1_1_1.md`.

**Acceptance**  
- `runPipeline` with mocks passes.  
- Autonomous loop test passes or is skipped with a clear reason.  
- IDE validation is documented.

---

## Completion order

- **Task 1** (Wire + Build) should be done first so the coordinator is in the build and the IDE can drive it.  
- **Task 2** (Coordinator bidir) can start once Task 1 is in place (or in parallel if only pipeline code is touched).  
- **Task 3** (Streaming) is independent; can run in parallel.  
- **Task 4** (Self-heal) can run in parallel; may depend on Task 2 for “notify coordinator” if that API is on AgentCoordinator.  
- **Task 5** (Tests) should run after Task 1 (and ideally after 2–4) so tests can cover wired and integrated behavior.

**Handoff**  
- Each agent: implement your task, update the audit or this file with “Done” and the changed files.  
- Merge order: 1 → 2, 4 → 5; 3 anytime.

### Validation checklist (Task 5 — completed)
- **Pipeline unit test**: Build/run `tests/test_autonomous_pipeline.cpp` (Ship in include path); expect PASS.
- **IDE**: Autonomy → Pipeline: Run once / Start / Stop autonomous loop; no crash, output reflects run.
