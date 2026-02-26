# Phase 2: Multi-Agent Collaboration Framework

**Status**: Foundation Complete  
**Date**: January 27, 2026  
**Target Completion**: February 15, 2026

---

## Overview

Phase 2 extends the Phase 1 agentic framework with **multi-agent collaboration**, **conflict resolution**, and **autonomous long-running tasks** guided by the 800B model streaming loader.

### Key Components

1. **AgentCoordinator** - Central orchestration for multi-agent systems
2. **ConflictResolver** - Handles resource contention and execution conflicts
3. **ModelGuidedPlanner** - Uses 800B model to generate execution plans
4. **Checkpoint System** - Enables task resumption after failures

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│           Model-Guided Planning Layer                       │
│  (800B Streaming Decoder → Execution Plans → Step Actions)  │
└──────────────────┬──────────────────────────────────────────┘
                   │
┌──────────────────▼──────────────────────────────────────────┐
│         AgentCoordinator (Central Registry)                 │
│  • Agent lifecycle (register/unregister/state management)   │
│  • Task submission and progress tracking                    │
│  • Lease-based resource locking                             │
│  • Checkpoint creation and restoration                      │
│  • Dependency tracking (DAG)                                │
└──────────────────┬──────────────────────────────────────────┘
                   │
      ┌────────────┼────────────┬──────────────────┐
      │            │            │                  │
┌─────▼─┐  ┌──────▼─┐  ┌──────▼──┐  ┌───────────▼──┐
│Agent 1│  │Agent 2 │  │Agent 3 │  │ ConflictResolver│
│       │  │        │  │        │  │  (Arbitration)  │
│ State │  │ State  │  │ State  │  │                 │
│ +Task │  │ +Task  │  │ +Task  │  │ • Detection     │
│ +Lock │  │ +Lock  │  │ +Lock  │  │ • Analysis      │
└───────┘  └────────┘  └────────┘  │ • Resolution    │
      │            │            │  └─────────────────┘
      └────────────┼────────────┘
                   │
┌──────────────────▼──────────────────────────────────────────┐
│      Phase 1 Framework (Manifestor, Wiring, Hotpatch)      │
│      + Phase 5 Swarm Orchestrator (GPU Acceleration)       │
└─────────────────────────────────────────────────────────────┘
```

---

## Module Reference

### 1. AgentCoordinator (`src/agentic/coordination/AgentCoordinator.{hpp,cpp}`)

**Singleton pattern** - Centralized agent and task management.

#### Agent Registration
```cpp
AgentCapabilities caps;
caps.requiredMemoryMB = 1024;
caps.priority = 75;  // 0-100, higher = more important
caps.contextWindowSize = 4096;

uint32_t agentId = AgentCoordinator::instance()
    .registerAgent("CodeAnalyzer", caps);
```

#### Task Submission
```cpp
TaskMetadata task;
task.taskName = "Refactor authentication module";
task.isLongRunning = true;
task.maxDurationSeconds = 7200;  // 2 hours
task.estimatedCompletion = now + 1h;

uint64_t taskId = AgentCoordinator::instance()
    .submitTask(agentId, task);
```

#### Resource Locking with Leases
```cpp
LeaseToken lease = AgentCoordinator::instance()
    .acquireLease(agentId, "/src/auth/module.cpp", 
                  std::chrono::seconds(300));

// Use resource...

// Renew if needed
AgentCoordinator::instance()
    .renewLease(lease, std::chrono::seconds(600));

// Release when done
AgentCoordinator::instance()
    .releaseLease(lease);
```

#### Checkpoint Management
```cpp
// Create checkpoint at 50% progress
std::vector<uint8_t> state = serialize_current_state();
uint64_t checkpointId = AgentCoordinator::instance()
    .createCheckpoint(taskId, 50, state);

// Later: restore from checkpoint
AgentCoordinator::instance()
    .restoreFromCheckpoint(checkpointId);
```

#### Task Dependencies
```cpp
AgentCoordinator::instance()
    .addTaskDependency(taskC, taskA);  // C depends on A
AgentCoordinator::instance()
    .addTaskDependency(taskC, taskB);  // C depends on B

// Check if ready to execute
if (AgentCoordinator::instance()
    .areAllDependenciesSatisfied(taskC)) {
    // Execute task C
}
```

### 2. ConflictResolver (`src/agentic/coordination/ConflictResolver.{hpp,cpp}`)

**Singleton pattern** - Detects and resolves multi-agent conflicts.

#### Conflict Types
- **FILE_WRITE_CONFLICT**: Two agents writing same file
- **RESOURCE_CONTENTION**: Competing for GPU/memory
- **DEPENDENCY_CONFLICT**: Circular or broken dependencies
- **STATE_CONFLICT**: Incompatible state transitions
- **EXECUTION_ORDER**: Race conditions
- **MODEL_CONTENTION**: Multiple agents using model

#### Conflict Analysis
```cpp
ConflictAnalysis analysis = ConflictResolver::instance()
    .analyzeConflict(conflictId);

if (analysis.isMergeable) {
    std::string merged;
    ConflictResolver::instance()
        .resolveByMerge(conflictId, merged);
} else if (analysis.severityScore > 0.8f) {
    // Escalate to human review
}
```

#### Resolution Strategies

**Priority-Based** (higher priority agent wins)
```cpp
uint32_t winner;
ConflictResolver::instance()
    .resolveByPriority(conflictId, winner);
```

**Serialization** (execute sequentially)
```cpp
std::vector<uint32_t> order;
ConflictResolver::instance()
    .resolveBySerializing(conflictId, order);
// Execute agents in order[0], then order[1], etc.
```

**Rollback** (revert changes)
```cpp
std::vector<uint64_t> tasks = {taskA, taskB};
ConflictResolver::instance()
    .resolveByRollback(conflictId, tasks);
```

**Three-Way Merge** (automatic merge)
```cpp
std::string merged;
ConflictResolver::instance()
    .attemptThreeWayMerge(baseVersion, agentAVersion, 
                         agentBVersion, merged);
```

#### Proactive Lock Prevention
```cpp
// Prevent conflicts by acquiring resource locks
bool acquired = ConflictResolver::instance()
    .preventConflictByLocking(agentId, "/src/auth.cpp", 300);

if (acquired) {
    // Safely edit resource without conflicts
    // ...
    
    // Release when done
    ConflictResolver::instance()
        .releaseResourceLock(agentId, "/src/auth.cpp");
}
```

### 3. ModelGuidedPlanner (`src/agentic/planning/ModelGuidedPlanner.{hpp,cpp}`)

**Singleton pattern** - Uses 800B model to generate execution plans.

#### Plan Generation
```cpp
std::vector<std::string> context = {
    "src/main.cpp",
    "src/utils.h",
    "docs/ARCHITECTURE.md"
};

uint64_t planId = ModelGuidedPlanner::instance()
    .generatePlan(taskId, 
                 "Optimize memory allocation in core engine",
                 context,
                 20);  // max 20 steps

ExecutionPlan plan = ModelGuidedPlanner::instance()
    .getPlan(planId);
```

#### Plan Structure
```cpp
struct ExecutionPlan {
    std::vector<PlanStep> steps;  // [ANALYZE → REFACTOR → TEST → VALIDATE]
    float overallConfidence;       // 0.0-1.0 (from 800B model)
    uint32_t estimatedTotalSeconds;
    bool executionApproved;        // Requires human approval
};

struct PlanStep {
    PlanAction action;             // ANALYZE_CODE, GENERATE_CODE, etc.
    std::string actionDescription;
    std::string targetResource;    // File, function, etc.
    std::vector<std::string> args; // Parameters
    std::vector<uint32_t> priorSteps;  // Dependencies
    float confidenceScore;         // 0.0-1.0
    uint32_t estimatedDurationSeconds;
    bool isParallelizable;         // Can run in parallel
    std::string rationale;         // Why model chose this
};
```

#### Streaming Token Generation (for plan building)
```cpp
// Initialize streaming decoder for this task
StreamingDecoderState decoder = ModelGuidedPlanner::instance()
    .initializeStreamingDecoder(taskId, taskDescription);

// Get tokens one at a time from 800B model
std::string token;
while (ModelGuidedPlanner::instance()
       .getNextToken(decoder, token)) {
    // Accumulate tokens into complete plan
    plan_text += token;
    
    // Can display partial output to user in real-time
    display_streaming_output(token);
}

ModelGuidedPlanner::instance()
    .finalizeStreaming(decoder);
```

#### Plan Approval Workflow
```cpp
ModelGuidedPlanner::instance()
    .submitPlanForApproval(planId, "Needs human review");

// After human review...
ModelGuidedPlanner::instance()
    .approvePlan(planId, "Approver: john@company.com");

// Or reject if issues found
ModelGuidedPlanner::instance()
    .rejectPlan(planId, "Conflicting with database schema changes");
```

#### Adaptive Replanning
```cpp
// If step execution fails, ask model for alternative plan
std::vector<uint64_t> failedSteps = {2, 5};
uint64_t newPlanId = ModelGuidedPlanner::instance()
    .replan(originalPlanId, failedSteps);

// Continue with new plan
ExecutionPlan newPlan = ModelGuidedPlanner::instance()
    .getPlan(newPlanId);
```

---

## Long-Running Task Pattern

### Multi-Hour Task with Checkpoints

```cpp
class LongRunningTask {
public:
    uint64_t execute() {
        // 1. Submit to coordinator
        AgentCoordinator& coord = AgentCoordinator::instance();
        
        TaskMetadata meta;
        meta.taskName = "Full codebase optimization";
        meta.isLongRunning = true;
        meta.maxDurationSeconds = 14400;  // 4 hours
        
        uint64_t taskId = coord.submitTask(agentId, meta);
        
        // 2. Get execution plan from 800B model
        ModelGuidedPlanner& planner = ModelGuidedPlanner::instance();
        uint64_t planId = planner.generatePlan(taskId, 
            "Optimize full codebase for performance",
            getContextFiles());
        
        // 3. Execute plan steps with checkpoints
        ExecutionPlan plan = planner.getPlan(planId);
        
        for (uint32_t stepIdx = 0; stepIdx < plan.steps.size(); ++stepIdx) {
            const auto& step = plan.steps[stepIdx];
            
            // 3a. Check if dependencies satisfied
            if (!planner.isStepDependencySatisfied(planId, stepIdx)) {
                continue;  // Skip, dependencies not ready
            }
            
            // 3b. Execute step
            bool success = execute_plan_step(step);
            
            if (!success) {
                // 3c. Create checkpoint before failure
                std::vector<uint8_t> state = save_current_state();
                coord.createCheckpoint(taskId, (stepIdx * 100) / plan.steps.size(),
                                      state);
                
                // 3d. Ask model for alternative
                std::vector<uint64_t> failedSteps = {stepIdx};
                uint64_t newPlanId = planner.replan(planId, failedSteps);
                plan = planner.getPlan(newPlanId);
                continue;
            }
            
            // 3e. Update progress
            uint32_t progress = ((stepIdx + 1) * 100) / plan.steps.size();
            coord.updateTaskProgress(taskId, progress);
            
            // 3f. Create checkpoint every 25% progress
            if (progress % 25 == 0) {
                std::vector<uint8_t> state = save_current_state();
                coord.createCheckpoint(taskId, progress, state);
            }
        }
        
        // 4. Task complete
        coord.updateTaskProgress(taskId, 100);
        return taskId;
    }

private:
    uint32_t agentId;
    
    bool execute_plan_step(const PlanStep& step) {
        switch (step.action) {
            case PlanAction::ANALYZE_CODE:
                return analyze_code(step.targetResource, step.args);
            case PlanAction::GENERATE_CODE:
                return generate_code(step.targetResource, step.args);
            case PlanAction::REFACTOR:
                return refactor(step.targetResource, step.args);
            case PlanAction::TEST:
                return run_tests(step.targetResource, step.args);
            case PlanAction::OPTIMIZE:
                return optimize(step.targetResource, step.args);
            default:
                return false;
        }
    }
    
    std::vector<uint8_t> save_current_state() {
        // Serialize: current file edits, line numbers, progress
        return {};
    }
};
```

---

## Multi-Agent Coordination Pattern

### Three Agents Working on Different Modules

```cpp
class MultiAgentCoordinator {
public:
    void coordinate_three_agents() {
        AgentCoordinator& coord = AgentCoordinator::instance();
        ConflictResolver& resolver = ConflictResolver::instance();
        ModelGuidedPlanner& planner = ModelGuidedPlanner::instance();
        
        // 1. Register agents
        uint32_t agentA = coord.registerAgent("ModuleA-Engineer", {.priority = 80});
        uint32_t agentB = coord.registerAgent("ModuleB-Engineer", {.priority = 70});
        uint32_t agentC = coord.registerAgent("ModuleC-Validator", {.priority = 75});
        
        // 2. Define tasks with dependencies
        TaskMetadata taskA;
        taskA.taskName = "Build ModuleA";
        uint64_t taskIdA = coord.submitTask(agentA, taskA);
        
        TaskMetadata taskB;
        taskB.taskName = "Build ModuleB";
        uint64_t taskIdB = coord.submitTask(agentB, taskB);
        
        TaskMetadata taskC;
        taskC.taskName = "Validate integration";
        uint64_t taskIdC = coord.submitTask(agentC, taskC);
        
        // C depends on both A and B
        coord.addTaskDependency(taskIdC, taskIdA);
        coord.addTaskDependency(taskIdC, taskIdB);
        
        // 3. Agents work in parallel
        AgentA_Thread agentA_worker(agentA, taskIdA);
        AgentB_Thread agentB_worker(agentB, taskIdB);
        
        agentA_worker.start();
        agentB_worker.start();
        
        // 4. Monitor for conflicts
        while (agentA_worker.is_running() || agentB_worker.is_running()) {
            std::vector<uint64_t> conflicts = coord.getActiveConflicts();
            
            for (uint64_t conflictId : conflicts) {
                // Resolve conflict
                ConflictAnalysis analysis = resolver.analyzeConflict(conflictId);
                
                if (analysis.isMergeable) {
                    std::string merged;
                    resolver.resolveByMerge(conflictId, merged);
                } else {
                    uint32_t winner;
                    resolver.resolveByPriority(conflictId, winner);
                }
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // 5. Validate agent C's task (now dependencies satisfied)
        AgentC_Validator agentC_worker(agentC, taskIdC);
        agentC_worker.start();
        agentC_worker.wait();
        
        // 6. Report statistics
        auto stats = coord.getStatistics();
        print_report(stats);
    }
};
```

---

## Implementation Checklist

### Phase 2 Completion Targets

- [ ] **AgentCoordinator**
  - [x] Singleton & thread-safety
  - [x] Agent lifecycle management
  - [x] Task submission & progress tracking
  - [x] Lease-based locking
  - [x] Checkpoint creation/restoration
  - [ ] Heartbeat monitor (background thread)
  - [ ] Conflict detector (background thread)
  - [ ] State persistence (export/import)

- [ ] **ConflictResolver**
  - [x] Conflict type definitions
  - [x] Analysis framework
  - [x] Resolution strategies (priority, serialization, rollback)
  - [ ] Three-way merge implementation
  - [ ] Resource lock management
  - [ ] Human review escalation

- [ ] **ModelGuidedPlanner**
  - [x] Plan structure & validation
  - [x] Streaming decoder state machine
  - [x] Adaptive replanning
  - [x] Approval workflow
  - [ ] 800B model integration
  - [ ] Token streaming from Phase-5 Swarm
  - [ ] Plan parsing from JSON/MSGPACK

- [ ] **Integration**
  - [ ] Update CMakeLists.txt (add coordination/, planning/ modules)
  - [ ] Integration tests
  - [ ] End-to-end multi-agent scenarios
  - [ ] Performance benchmarks (throughput, latency)
  - [ ] Stress testing (100+ agents, 1000+ tasks)

### Test Scenarios

```
✓ Single agent, single task (baseline)
✓ Two agents, independent tasks (no conflict)
✓ Two agents, same resource (conflict detection)
✓ Priority-based resolution
✓ Merge-based resolution
✓ Long-running task with checkpoints
✓ Task failure & replanning
✓ Dependency DAG with 5+ levels
✓ Model-guided planning (mock 800B)
✓ Streaming token generation
```

---

## Performance Targets

| Metric | Target | Notes |
|--------|--------|-------|
| Agent registration | <1ms | O(1) singleton registration |
| Task submission | <2ms | O(1) task list insertion |
| Lease acquisition | <500µs | Lock-free if possible |
| Conflict detection | <50ms | Background thread scan |
| Conflict resolution | <100ms | Depends on strategy |
| Plan generation (streaming) | <5s | 800B model latency |
| Checkpoint save | <100ms | Serialize task state |
| Checkpoint restore | <200ms | Deserialize & validate |

---

## File Structure

```
D:\rawrxd\src\agentic\
├── coordination/
│   ├── AgentCoordinator.hpp
│   ├── AgentCoordinator.cpp
│   ├── ConflictResolver.hpp
│   └── ConflictResolver.cpp
├── planning/
│   ├── ModelGuidedPlanner.hpp
│   └── ModelGuidedPlanner.cpp
├── CMakeLists.txt (updated)
└── ... (Phase 1 modules)
```

---

## Next Steps

1. **Implement background threads** in AgentCoordinator
   - Heartbeat monitor (detect stalled agents)
   - Conflict detector (proactive conflict discovery)

2. **Implement Phase-5 Swarm integration** in ModelGuidedPlanner
   - Connect to 800B model streaming loader
   - Token generation pipeline
   - Plan JSON parsing

3. **Add three-way merge algorithm** to ConflictResolver
   - Line-by-line diffing
   - Conflict detection
   - Automatic merge when possible

4. **Create comprehensive test suite**
   - Unit tests for each component
   - Integration tests for multi-agent scenarios
   - Stress tests with 100+ concurrent agents

5. **Update CMakeLists.txt**
   - Add coordination/ and planning/ module sources
   - Link with Phase 5 (SwarmOrchestrator)
   - Add Phase 2 tests

---

**Status**: Phase 2 Foundation Complete (5 of 30 tasks)  
**ETA**: February 15, 2026  
**Owner**: RawrXD Agentic Framework Team
