# Agentic Loops - Quick Reference

## Core Components

### 1. AgenticLoopState
**Purpose**: Complete state management for reasoning iterations

**Key Methods**:
- `startIteration(goal)` - Begin new iteration
- `endIteration(status, result)` - Finalize iteration
- `recordDecision(desc, reasoning, confidence)` - Track decisions
- `recordError(type, message)` - Log errors
- `takeSnapshot() / restoreFromSnapshot()` - Checkpointing
- `getMetrics()` - Get comprehensive metrics

**When to Use**: Always use as the foundation for tracking agentic loops

---

### 2. AgenticIterativeReasoning
**Purpose**: Main reasoning loop with 6 phases

**Phases**:
1. Analysis - Understand problem
2. Planning - Generate strategies
3. Execution - Execute chosen strategy
4. Verification - Verify results
5. Reflection - Analyze what worked
6. Adjustment - Update strategy

**Key Methods**:
- `reason(goal, maxIter, timeout)` - Run full loop
- `executeAnalysisPhase(goal)` - Individual phases
- `getReasoningTrace()` - See decision path

**When to Use**: For complex tasks requiring iterative refinement

---

### 3. AgenticMemorySystem
**Purpose**: Learn from past experiences

**Memory Types**:
- Episodic: Past interactions
- Semantic: General knowledge
- Procedural: Working strategies
- Working: Current context

**Key Methods**:
- `storeMemory(type, content)` - Save to memory
- `getRelevantMemories(context)` - Retrieve similar
- `recordExperience(task, goal, result, success, strategies)`
- `findSimilarExperiences(task, similarity)`
- `getStrategyEffectiveness(strategy)`

**When to Use**: Enable learning across multiple executions

---

### 4. AgenticAgentCoordinator
**Purpose**: Multi-agent task delegation

**Agent Roles**:
- Analyzer - Problem understanding
- Planner - Strategy creation
- Executor - Action execution
- Verifier - Result validation
- Optimizer - Performance tuning
- Learner - Experience management

**Key Methods**:
- `createAgent(role)` - Create specialized agent
- `assignTask(desc, params, role)` - Delegate work
- `executeAssignedTask(taskId)`
- `synchronizeAgentStates()` - Sync state across agents
- `getCoordinationMetrics()`

**When to Use**: Distribute complex tasks across specialized agents

---

### 5. AgenticObservability
**Purpose**: Monitoring and diagnostics

**Three Pillars**:
1. **Logging**: Structured logs with context
2. **Metrics**: Counters, gauges, histograms
3. **Tracing**: Distributed trace spans

**Key Methods**:
- `logInfo/Warn/Error(component, message, context)`
- `recordMetric(name, value, labels, unit)`
- `measureDuration(metricName)` - Auto-timing RAII guard
- `startTrace/Span()` - Distributed tracing
- `getSystemHealth()` - Health checks
- `detectBottlenecks()` - Performance analysis

**When to Use**: All production systems need observability

---

### 6. AgenticErrorHandler
**Purpose**: Robust error handling and recovery

**Recovery Strategies**:
- Retry: Exponential backoff
- Backtrack: Restore from checkpoint
- Fallback: Alternative path
- Escalate: Promote to higher handler
- Abort: Graceful termination

**Key Methods**:
- `recordError(type, message, component)`
- `executeRecovery(errorId)`
- `setRecoveryPolicy(policy)` - Define recovery per error type
- `handleError(exception, component, context)`
- `getErrorStatistics()`

**When to Use**: Protect critical operations and ensure resilience

---

### 7. AgenticConfiguration
**Purpose**: External configuration management

**Features**:
- Environment-specific config (Dev/Staging/Prod)
- Feature toggles with gradual rollout
- Validation and type safety
- Hot reloading
- Profile management
- Secret handling

**Key Methods**:
- `initializeFromEnvironment(env)`
- `loadFromJson/Env/Yaml(path)`
- `get/getString/getInt/getFloat/getBool(key, default)`
- `isFeatureEnabled(feature)`
- `setSecret(key, value)`
- `saveProfile/loadProfile(name)`

**When to Use**: All deployed systems need configuration

---

## Common Patterns

### Pattern 1: Basic Reasoning Loop
```cpp
AgenticLoopState state;
AgenticIterativeReasoning reasoning;
reasoning.initialize(engine, &state, inference);

auto result = reasoning.reason("task", 10, 300);
qDebug() << "Success:" << result.success;
qDebug() << "Iterations:" << result.iterationCount;
```

### Pattern 2: Error Recovery
```cpp
AgenticErrorHandler errorHandler;
errorHandler.initialize(&obs, &state);

try {
    doSomething();
} catch (const std::exception& e) {
    QString errorId = errorHandler.recordError(
        ErrorType::ExecutionError,
        e.what(),
        "Component"
    );
    errorHandler.executeRecovery(errorId);
}
```

### Pattern 3: Learning Loop
```cpp
AgenticMemorySystem memory;

// After task completion
memory.recordExperience(
    taskDesc,
    goalState,
    resultState,
    successful,
    usedStrategies
);

// Next task - find similar experiences
auto similar = memory.findSimilarExperiences(newTask, 0.7);
```

### Pattern 4: Multi-Agent Execution
```cpp
AgenticAgentCoordinator coordinator;
coordinator.initialize(engine, inference);

QString analyzer = coordinator.createAgent(AgentRole::Analyzer);
QString task = coordinator.assignTask("Analyze...", {}, AgentRole::Analyzer);
coordinator.executeAssignedTask(task);
auto result = coordinator.getTaskResult(task);
```

### Pattern 5: Production Observability
```cpp
AgenticObservability obs;

{
    auto timer = obs.measureDuration("operation_name");
    // ... operation ...
} // Duration auto-recorded

obs.recordMetric("custom_metric", 42.5f, labels, "unit");
QJsonObject health = obs.getSystemHealth();
```

---

## Configuration Example

`config/agentic.json`:
```json
{
  "agentic": {
    "max_iterations": 15,
    "timeout_seconds": 300
  },
  "observability": {
    "log_level": "INFO",
    "max_logs": 10000
  },
  "error_handler": {
    "max_retries": 3,
    "graceful_degradation": true
  },
  "memory": {
    "max_memories": 1000,
    "decay_rate": 0.99
  },
  "model": {
    "temperature": 0.8,
    "top_p": 0.9,
    "max_tokens": 512
  }
}
```

---

## Key Metrics

### Per-Iteration Metrics
- `iteration_number`: Which iteration (1-N)
- `phase`: Current phase (Analysis→Reflection)
- `decision_count`: How many decisions made
- `error_count`: Errors encountered
- `phase_duration_ms`: Time per phase

### System Metrics
- `total_iterations`: Sum of all iterations
- `success_rate`: % completed successfully
- `error_rate`: Errors per minute
- `decision_success_rate`: % of good decisions
- `average_confidence`: Avg decision confidence

### Agent Metrics
- `agents_created`: Total agents spawned
- `tasks_assigned`: Total tasks delegated
- `tasks_completed`: Finished tasks
- `total_utilization`: CPU share used
- `average_task_duration`: Time per task

### Memory Metrics
- `total_memories`: Stored memories
- `total_experiences`: Recorded experiences
- `average_success_rate`: Success of past tasks
- `most_effective_strategy`: Best performing strategy

### Error Metrics
- `total_errors`: All errors encountered
- `error_by_type`: Breakdown by error type
- `recovery_success_rate`: % recoveries successful
- `circuit_breaker_trips`: Catastrophic failures

---

## Performance Targets

| Metric | Target | Warning | Critical |
|--------|--------|---------|----------|
| Iteration Time | <5s | >10s | >30s |
| Decision Confidence | >0.75 | <0.60 | <0.40 |
| Error Recovery Rate | >95% | <80% | <50% |
| Memory Access Time | <1ms | >5ms | >50ms |
| Log Write Latency | <1ms | >10ms | N/A |
| Agent Task Duration | <10s | >30s | >60s |

---

## Troubleshooting Quick Tips

| Problem | Solution |
|---------|----------|
| Loop not converging | Check phase transitions, increase iterations |
| Memory growing | Call `pruneOldMemories()`, reduce retention |
| Errors not recovering | Set recovery policy, check circuit breaker |
| Slow performance | Use sampling in observability, profile |
| State corruption | Enable checkpointing, restore from snapshot |
| Agents idle | Check task assignment, rebalance workload |

---

## Integration Checklist

- [ ] Include all 7 header files
- [ ] Add all 7 source files to build
- [ ] Load configuration before use
- [ ] Initialize error handler with observability
- [ ] Enable observability logging
- [ ] Set up memory system
- [ ] Create reasoning loop
- [ ] Monitor metrics

---

## File Sizes (Approximate)

| Component | Header | Implementation | Total |
|-----------|--------|-----------------|-------|
| LoopState | 250 lines | 700 lines | 950 |
| Reasoning | 200 lines | 500 lines | 700 |
| Memory | 350 lines | 650 lines | 1000 |
| Coordinator | 300 lines | 550 lines | 850 |
| Observability | 280 lines | 750 lines | 1030 |
| ErrorHandler | 250 lines | 450 lines | 700 |
| Configuration | 320 lines | 600 lines | 920 |
| **Total** | **1950 lines** | **4200 lines** | **6150 lines** |

---

## Summary

This complete agentic loop system provides everything needed for:
- ✅ Complex iterative reasoning
- ✅ State management & recovery
- ✅ Learning from experience
- ✅ Multi-agent coordination
- ✅ Production monitoring
- ✅ Error resilience
- ✅ Configuration flexibility

**All in 6150+ lines of clean, production-ready code.**

Use this quick reference alongside the detailed documentation for implementation.
