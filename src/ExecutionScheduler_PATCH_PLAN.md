# ExecutionScheduler v2 — Minimal Patch Plan
# Exact file-level edits for production branch integration

## Overview
This document provides the minimal diff strategy for integrating all 10 bottleneck
fixes into the existing RawrXD production branch. Each fix is designed to be
applied independently with backward compatibility.

---

## File Mapping

### New Files (Add These)
```
src/ExecutionScheduler_v2.h              # Complete v2 implementation
src/ExecutionScheduler_v2.cpp            # v2 implementation
src/ExecutionScheduler_Integration_Guide.h  # Migration helpers
src/ExecutionScheduler_v2_test.cpp       # Validation tests
```

### Modified Files (Edit These)
```
src/ExecutionScheduler.h                 # Add v2 integration hooks
src/ExecutionScheduler.cpp                 # Add migration layer
src/cpu_inference_engine.h               # Use MonotonicClock
src/cpu_inference_engine.cpp             # Replace timing sources
src/BackendOrchestrator.cpp              # Use deferred commands
src/BackendOrchestrator.h                # Add command queue
```

---

## Patch 1: ExecutionScheduler.h — Add v2 Integration

```cpp
// Add to existing ExecutionScheduler class:

class ExecutionScheduler {
    // ... existing members ...
    
    // BOTTLENECK FIX #1: Lock-free queue option
    #ifdef USE_LOCKFREE_QUEUES
    std::vector<std::unique_ptr<LockFreeWorkStealingQueue<ExecTask, 1024>>> m_lockfree_queues;
    #else
    std::vector<std::unique_ptr<WorkerQueue>> m_queues;
    #endif
    
    // BOTTLENECK FIX #2: Incremental DAG
    std::unique_ptr<IncrementalDAG> m_dag;
    
    // BOTTLENECK FIX #3: Phase state machine
    std::unique_ptr<PhaseStateMachine> m_phase_machine;
    
    // BOTTLENECK FIX #4: Slab allocator
    std::unique_ptr<SlabAllocator<ExecTask, 4096>> m_task_slab;
    
    // BOTTLENECK FIX #7: Telemetry ring
    std::unique_ptr<LockFreeTelemetryRing<65536>> m_telemetry;
    
    // BOTTLENECK FIX #8: Monotonic clock
    MonotonicClock::TimePoint m_start_time;
    
    // BOTTLENECK FIX #9: Deferred commands
    std::unique_ptr<CommandPacketQueue> m_deferred_queue;
    
    // BOTTLENECK FIX #10: Phase budgets
    std::array<PhaseBudget, 5> m_phase_budgets;
};
```

---

## Patch 2: cpu_inference_engine.cpp — Replace Timing

```cpp
// BEFORE:
auto start = std::chrono::steady_clock::now();
// ... work ...
auto end = std::chrono::steady_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

// AFTER:
auto start = MonotonicClock::now();
// ... work ...
auto end = MonotonicClock::now();
auto duration = MonotonicClock::microseconds(end - start);
```

---

## Patch 3: BackendOrchestrator.cpp — Deferred Commands

```cpp
// BEFORE (blocking):
void BackendOrchestrator::dispatch_to_gpu(int kernel_id, void* args) {
    gpu_engine->execute(kernel_id, args);  // Blocks
}

// AFTER (non-blocking):
void BackendOrchestrator::dispatch_to_gpu(int kernel_id, void* args) {
    DeferredCommand cmd;
    cmd.type = CommandType::GPU_DISPATCH;
    cmd.kernel_id = kernel_id;
    cmd.payload = args;
    m_deferred_queue->enqueue(cmd);  // Non-blocking
}
```

---

## Patch 4: CMakeLists.txt — Add v2 Sources

```cmake
# Add to existing sources:
set(EXECUTION_SCHEDULER_SOURCES
    src/ExecutionScheduler.cpp
    src/ExecutionScheduler_v2.cpp        # NEW
    src/ExecutionScheduler_Integration_Guide.cpp  # NEW
)

# Add feature flag:
add_compile_definitions(
    $<$<BOOL:${USE_SCHEDULER_V2}>:USE_SCHEDULER_V2>
    $<$<BOOL:${USE_LOCKFREE_QUEUES}>:USE_LOCKFREE_QUEUES>
)
```

---

## Patch 5: Feature Flag Integration

```cpp
// In main.cpp or initialization:

#include "ExecutionScheduler_Integration_Guide.h"

int main(int argc, char** argv) {
    // Check for v2 flag
    bool use_v2 = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--scheduler-v2") == 0) {
            use_v2 = true;
            break;
        }
    }
    
    // Initialize appropriate scheduler
    if (use_v2) {
        ExecutionScheduler_Migration::init_v2(
            std::thread::hardware_concurrency()
        );
        ExecutionScheduler_Migration::use_v2.store(true);
    } else {
        // Use existing v1
        ExecutionScheduler::Instance();
    }
    
    // ... rest of initialization ...
}
```

---

## Rollback Strategy

If issues are detected:

1. **Immediate:** Set `ExecutionScheduler_Migration::use_v2 = false`
2. **Short-term:** Restart without `--scheduler-v2` flag
3. **Long-term:** Revert CMake changes and rebuild

---

## Validation Checklist

Before full deployment:

- [ ] Smoke tests pass (ExecutionScheduler_v2_test.cpp)
- [ ] Titan 100-cycle soak test passes
- [ ] 70B model inference stable for 1 hour
- [ ] TPS variance < 5% under load
- [ ] No memory leaks (slab allocator working)
- [ ] Telemetry ring buffer not dropping events
- [ ] Phase budgets enforced (no overruns)
- [ ] Lock-free queues showing steal events
- [ ] Monotonic clock calibrated correctly

---

## Performance Targets

| Metric | v1 Baseline | v2 Target | Validation |
|--------|-------------|-----------|------------|
| Avg queue wait | 2-5ms | <100μs | Telemetry |
| Submit latency | O(V+E) | O(1) | Benchmark |
| Phase transition | Blocking | <10μs | Timing |
| Task allocation | ~200ns | ~20ns | Profiler |
| Registry lookup | ~50ns | ~5ns | Benchmark |
| KV coupling | Variable | Async | Queue depth |
| Telemetry | ~500ns | ~20ns | Timing |
| Clock precision | ~100ns | ~10ns | TSC |
| Engine calls | Blocking | Deferred | Latency |
| Phase budgets | Unbounded | Enforced | Overruns |

---

## Deployment Phases

### Phase 1: Validation (Week 1)
- Deploy to test cluster
- Run Titan soak tests
- Validate metrics

### Phase 2: Canary (Week 2)
- 10% of production traffic
- Monitor for regressions
- Compare v1 vs v2 metrics

### Phase 3: Gradual Rollout (Week 3-4)
- 25% → 50% → 75% → 100%
- Monitor at each step
- Rollback if issues

### Phase 4: Full Cutover (Week 5)
- Remove v1 code paths
- Clean up feature flags
- Update documentation

---

## Support Contacts

- Performance issues: Check ExecutionScheduler_v2_BOTTLENECK_FIXES.md
- Integration questions: See ExecutionScheduler_Integration_Guide.h
- Rollback procedures: See Rollback Strategy above
