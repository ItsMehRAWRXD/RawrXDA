# Autonomous agent (Electron shell) — `bigdaddyg-ide`

The **Agent** panel drives `src/agentic/orchestrator.js` from the **main** process (Ollama-compatible `providers.js`).

## Capabilities

- **Workspace-aware planning** — Before planning, main scans the opened project (ignores `node_modules`, `.git`, common build dirs) and injects a path index into the LLM prompt.
- **JSON plan → steps** — `read_file`, `write_file`, `append_file`, `list_dir`, `ask_ai` (see orchestrator `buildPlanPrompt`).
- **Safety gates** — `write_file` / `append_file` require **Approve / Deny** unless **Auto-approve file writes** is on (same flag as Settings → **agentAutoApprove**).
- **Reflection** — Optional final LLM pass on goal satisfaction (`enableReflection` in policy; default on).
- **Cancel** — `agent:cancel` stops the task and resolves a pending approval as denied.

## IPC

| Channel | Args | Purpose |
|---------|------|---------|
| `agent:start` | `goal`, `policy?` | Start task (`autoApproveWrites`, `autoApproveReads`, `maxPlanTokens`, `enableReflection`, `maxSteps`) |
| `agent:status` | `taskId` | Full task snapshot (`steps`, `pendingApproval`, `log`, …) |
| `agent:approve` | `taskId`, `approved: boolean` | Resume after mutating step gate |
| `agent:cancel` | `taskId` | Abort |

Preload: `startAgent`, `getAgentStatus`, `approveAgentStep`, `cancelAgentTask`.

## Requirements

- **Project folder** must be opened (File → Open project) so `projectRoot` and workspace scan work.
- **Ollama** (or configured provider) reachable for plan + step execution.

## Files

- `electron/main.js` — `summarizeWorkspace`, IPC handlers  
- `electron/preload.js` — bridge  
- `src/agentic/orchestrator.js` — planner + gates + reflection  
- `src/contexts/AgentContext.js` — React state + polling  
- `src/components/AgentPanel.js` — UX for approvals  
