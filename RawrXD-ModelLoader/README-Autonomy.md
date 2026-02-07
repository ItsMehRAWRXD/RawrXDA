# RawrXD IDE Autonomy Framework (Initial Scaffold)

## Purpose
Adds a lightweight, extensible autonomous orchestration layer (`AutonomyManager`) that can plan and execute iterative actions using the existing `AgenticBridge`.

## Components
- `Win32IDE_Autonomy.h/.cpp`: Core manager (goal, memory, planner, executor, rate limiter).

## Design Overview
- Goal: Single active textual objective.
- Memory: Rolling window of observations (capped at 2048 items) generated from executed actions.
- Planner: Naive heuristic (context gathering -> periodic summarization -> reflection prompt).
- Executor: Delegates to `AgenticBridge::ExecuteAgentCommand()`. Distinguishes `tool:` vs `prompt:` prefixes.
- Rate Limiting: Prevents runaway loops (default 30 actions/minute).
- Auto Loop: Background thread with ~800ms cadence performing `tick()` until disabled.

## Extend Points
1. Replace `planNextAction()` with chain-of-thought or tree search.
2. Add vector store / semantic memory (embedding + retrieval).
3. Insert safety filters (deny-list for destructive tools, CPU usage guards).
4. Provide UI telemetry (status bar autonomy indicator, memory viewer panel).
5. Add persistence (serialize goal + memory to disk on shutdown; reload on startup).

## Future Additions
- Reflection phase after N actions (score plan quality; adjust goal).
- Multi-goal queue with prioritization.
- Tool outcome classification (success/failure) with reinforcement scoring.
- Adaptive rate limiting based on latency/back-pressure from agent commands.

## Usage (Future Integration)
```cpp
// Inside Win32IDE initialization after AgenticBridge setup:
m_autonomyManager = std::make_unique<AutonomyManager>(m_agenticBridge.get());
m_autonomyManager->setGoal("Index project and summarize architecture");
m_autonomyManager->enableAutoLoop(true);
```

## Status
This is a scaffold: compilation only. No UI hooks yet.
