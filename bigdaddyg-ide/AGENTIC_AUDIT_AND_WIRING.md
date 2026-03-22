## Agentic Audit and Wiring

### Fake/Scaffolded Paths Found
1. Stub providers throwing runtime errors.
- `src/agentic/providers.js` had `Copilot/AmazonQ/Cursor` classes that always threw `"not yet implemented"`.

2. Agent orchestration executed text-only pseudo steps.
- `src/agentic/orchestrator.js` asked AI to "Execute this development step" but never performed real file or workspace actions.

3. Provider selection UX exposed disabled providers without clear state handling.
- Toolbar allowed selecting providers that were non-functional.

4. Agent panel hid execution payloads.
- Step results/errors existed but were not rendered, making failures appear opaque.

### Wiring Implemented
1. Real provider adapters in place of stubs.
- Replaced stubs with `OllamaCompatibleProvider` implementation for `bigdaddyg`, `copilot`, `amazonq`, `cursor`.
- Providers are now config-driven and only enabled when configured.

2. Orchestrator now performs real executable step types.
- Added runtime-backed step execution for:
  - `read_file`
  - `write_file`
  - `append_file`
  - `list_dir`
  - `search_repo` (bounded substring search in main process; text-like extensions only)
  - `ask_ai`
- Plan prompt now requests deterministic JSON schema with typed steps.
- **Safety:** project-relative paths only; `..` and absolute paths rejected before I/O.
- **Policy:** `autoApproveReads`, `autoApproveWrites`, `requireAskAiApproval` (gates `ask_ai`), `enableReflection`.
- **UI:** task list (`agent:list-tasks`) + Agent panel toggles; persisted in shell settings (`IdeFeaturesContext`).

3. Safe runtime wiring in Electron main process.
- Injected orchestrator runtime with project-root constrained file operations.
- Preserved root-path guard for security boundaries.

4. Provider UX clarified.
- Toolbar now marks disabled providers and initializes active provider to an enabled one.
- Selecting unavailable provider returns explicit backend error.

5. Agent visibility improved.
- `AgentPanel` now displays step errors and result payload previews.

6. **AgenticPlanningOrchestrator** (unified planning + gates).
- Main entry: `agentic_planning_orchestrator.js` extends `orchestrator.js`.
- **Plan review** (`requirePlanApproval`) and **mutation batch** (`batchApproveMutations`) use the same `agent:approve` IPC with `pendingApproval.kind`.
- **Rollback:** snapshots before write/append, `agent:rollback`, runtime `deleteFile` for created files.
- **UI:** Agent panel shows plan/mutation-queue digests, risk level badges, rollback control; settings persist new policy flags (`IdeFeaturesContext`).

### Remaining Reality Constraints
1. External provider APIs are still "compatible mode" unless endpoint/model credentials are configured.
- If `copilot/amazonq/cursor` are disabled in config, they remain unavailable by design.

2. The orchestrator is now executable, but still single-agent sequential (no multi-agent parallel planner).
- This is functional, not stubbed, but not a distributed autonomous runtime.

