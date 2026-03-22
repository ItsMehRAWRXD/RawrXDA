# Agent policy settings (Electron main ↔ orchestrator)

All toggles under **Settings › Copilot / Cursor** in the block **“Agent usage & power”** are **real**: they are merged into the policy object passed to `agent:start` → `AgenticPlanningOrchestrator.startTask` → `AgentOrchestrator`.

| Setting | Policy field | Effect |
|--------|----------------|--------|
| Enable autonomous agent | `autonomousEnabled` | When false, `agent:start` IPC returns an error and `startTask` throws if called with `false`. |
| Agent terminal / shell steps | `allowShell` | When true, the planner may emit `run_terminal`; stripped from the plan when false. Execution uses `agentRuntime.runShellCommand` in `electron/main.js` (bounded `spawn`, `cwd` = project root). **Not** the UI xterm PTY. |
| Auto-approve agent shell steps | `autoApproveTerminal` | Skips the approval gate for `run_terminal` when true. |
| Swarming | `swarmingEnabled` | When true, planner may emit `task` steps; `executeStep` runs nested full `executeTask` per sub-goal (parallel or sequential). Stripped when false. |
| Require approval for swarm steps | `requireSubagentApproval` | Gates `task` steps when true. |
| Max mode | `agentMaxMode` | Increases `maxPlanTokens` and `askAiMaxTokens` (hard caps in `orchestrator.js`). |
| Deep thinking | `agentDeepThinking` | Extra planner/reflection token headroom plus a **second** LLM pass (`reflect_deep`) after the normal reflection. |
| Agent context limit | `agentContextLimitChars` | Truncates workspace tree text before `buildPlanPrompt` (log line records truncation). |

Batch mutation approval (`batchApproveMutations`) now queues **write/append** when writes are not auto-approved, and **`run_terminal`** when terminal is not auto-approved (independent combinations).

IRC `agentStart(goal, {})` keeps defaults: autonomous on, shell and swarm off.
