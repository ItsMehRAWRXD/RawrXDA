# Autonomous agent (Electron `bigdaddyg-ide`)

Parity-oriented orchestration for multi-step goals with **workspace context**, **risk tiers**, **unified approval queues**, and optional **rollback**.

## AI backend (important)

The Electron shell talks to models via **Ollama-compatible HTTP** (`POST …/api/generate`) from the **main process** (`src/agentic/providers.js`). It does **not** load `.gguf` inside Electron. It also **does not listen on 11434**—it is strictly a **client** to whatever you run locally; see **`docs/LOCALHOST_COMMUNICATION_MAP.md`** (full loopback reverse map).

- **Config:** `bigdaddyg-ide/config/providers.json` — default base URL **`http://127.0.0.1:11434`** (IPv4 loopback). Using `localhost` can make Node try **`::1`** first while Ollama listens only on IPv4 → **`ECONNREFUSED ::1:11434`**.
- **Win32 RawrXD (your product):** uses **your own integrated inference**, **WASM-aligned** with **BGzipXD** (loader + runner + codec + RE) on native Win32 — see **`docs/BGZIPXD_WASM.md`** and **`docs/INFERENCE_PATH_MATRIX.md`** (`LocalGGUF` / `routeToLocalGGUF`). That path is **not** the Electron shell and **not** Ollama unless you explicitly select the Ollama backend in the switcher.

## `AgenticPlanningOrchestrator`

Main process uses **`src/agentic/agentic_planning_orchestrator.js`**, which extends `orchestrator.js` and adds:

1. **Plan digest** — each planned step gets `riskLevel` (`low` | `medium` | `high`) from the model’s optional `risk` field or from step type (writes → high, reads → low, `ask_ai` → medium). `task.plan.riskDigest` mirrors the ordered plan.
2. **`requirePlanApproval`** — after the JSON plan is parsed, execution pauses with `pendingApproval.kind === 'plan_review'` until the user approves or denies the **entire** plan (IDE shows step digest + summary).
3. **`batchApproveMutations`** — when writes are **not** auto-approved, all `write_file` / `append_file` steps can be approved in **one** gate (`pendingApproval.kind === 'mutation_batch'`). On approve, `task.mutationBatchApproved` skips per-step mutate gates.
4. **`enableRollbackSnapshots`** (default **on**) — before each write/append, the orchestrator snapshots prior file contents (or “did not exist”) into `task.rollbackStack`. **`agent:rollback`** reapplies snapshots LIFO (uses runtime `deleteFile` for files the agent created).

Base **`AgentOrchestrator`** still defines step execution, `waitForUserDecision` (approval payload includes `kind`), and the `afterPlanReady` hook (no-op in base; implemented in the planning subclass).

## Capabilities

| Step type      | Risk tier | Risk level (default) | Runtime | Notes |
|----------------|-----------|----------------------|---------|--------|
| `list_dir`     | read      | low                  | `readDir` | Relative path under project root |
| `read_file`    | read      | low                  | `readFile` | Path traversal blocked |
| `search_repo`  | read      | low                  | `searchWorkspace` | Substring search; capped files/bytes/hits |
| `write_file`   | mutate    | high                 | `writeFile` | Gates / batch / rollback as configured |
| `append_file`  | mutate    | high                 | `appendFile` | Same |
| `ask_ai`       | cognitive | medium               | LLM invoke | Optional approval if `requireAskAiApproval` |

## Policy (per run + persisted defaults)

- `autoApproveReads` — skip gate for read/search/list (default **on** in settings).
- `autoApproveWrites` — skip gate for write/append (default **off**).
- `requireAskAiApproval` — pause before every `ask_ai` (default **off**).
- `enableReflection` — final LLM recap pass (default **on**).
- `requirePlanApproval` — pause for whole-plan review (default **off**).
- `batchApproveMutations` — single gate for all mutations in the plan (default **off**).
- `enableRollbackSnapshots` — capture pre-mutation state (default **on**).

## IPC

- `agent:start` — `(goal, policy)` → `taskId`
- `agent:status` — `(taskId)` → task snapshot (**`rollbackStack` stripped**; includes `rollbackCount`)
- `agent:approve` — `(taskId, approved)` — unblocks pending approval (`step` | `plan_review` | `mutation_batch`)
- `agent:cancel` — `(taskId)`
- `agent:list-tasks` — `{ active, history }` slim rows (`rollbackCount`, `hasPendingApproval`, …)
- `agent:rollback` — `(taskId)` → `{ restored }` or error (LIFO restore)

## Related sources

- `bigdaddyg-ide/src/agentic/agentic_planning_orchestrator.js`
- `bigdaddyg-ide/src/agentic/orchestrator.js`
- `bigdaddyg-ide/electron/main.js` (`agentRuntime`, `searchWorkspace`, `deleteFile`)
- `bigdaddyg-ide/src/contexts/AgentContext.js`
- `bigdaddyg-ide/src/components/AgentPanel.js`
