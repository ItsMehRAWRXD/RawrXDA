# Async Inference Integration - Quick Reference Card

## In 30 Seconds

The bounded autonomous executor runs a **perception→decision→action→feedback loop** that:
1. **Perceives** current file state
2. **Queries the model asynchronously** for next action (non-blocking)
3. **Waits max 5 seconds** for decision completion
4. **Executes real tools** based on parsed action
5. **Collects feedback** for next iteration
6. **Repeats** up to 10 times (or until user clicks stop)

All async - UI never freezes.

---

## Core Signals

```
ITERATION SIGNALS (per cycle)
├─ iterationStarted(int)                    Start of iteration
├─ perceptionPhaseComplete(int, context)    Perceived files/errors
├─ decisionPhaseComplete(int, decision)     Model's response
├─ actionPhaseComplete(int, result)         Tool execution done
├─ feedbackPhaseComplete(int, feedback)     Results collected
└─ iterationCompleted(int)                  Iteration done

LIFECYCLE SIGNALS
├─ loopStarted(task)                        Loop begins
├─ loopFinished()                           Loop completed
├─ loopStopped()                            User clicked stop
└─ loopError(error)                         Fatal error

PROGRESS SIGNALS
├─ progressUpdated(iter, max, status)       Real-time progress
└─ outputLogged(entry)                      Log line for UI
```

---

## Quick Start (5 Minutes)

### 1. Create Executor
```cpp
BoundedAutonomousExecutor* executor = new BoundedAutonomousExecutor(
    m_inferenceEngine,   // Must be initialized
    m_toolExecutor,      // Must be initialized
    m_editor,            // File context
    m_terminals          // Output display
);
```

### 2. Wire UI Signals
```cpp
// Status bar progress
connect(executor, &BoundedAutonomousExecutor::progressUpdated,
        statusBar, [](int iter, int max, const QString& status) {
    statusBar->showMessage(
        QString("Iteration %1/%2 - %3").arg(iter).arg(max).arg(status)
    );
});

// Terminal output
connect(executor, &BoundedAutonomousExecutor::outputLogged,
        terminal, [](const QString& log) { terminal->append(log); });

// Stop button
connect(stopButton, &QPushButton::clicked,
        executor, &BoundedAutonomousExecutor::requestShutdown);
```

### 3. Start Loop
```cpp
executor->startAutonomousLoop(
    "Refactor the code for clarity",  // Task description
    10  // Max iterations (1-50)
);
```

That's it! Loop runs asynchronously.

---

## Async Flow Diagram

```
Decision Phase (Async)          Action Phase (Wait)         Result
═══════════════════════          ══════════════════════      ══════

executeDecisionPhase()
├─ Build prompt
├─ m_decisionPhaseWaiting=true
├─ generateStreaming()          while(m_decisionPhaseWaiting
└─ RETURN IMMEDIATELY           && elapsed < 5000)
   │                              │
   │                              ├─ Check every 100ms
   │                              ├─ ProcessEvents()
   │                              │
   [Background: Model]            │
   ├─ Emit streamToken()          │
   ├─ Emit streamToken()          │
   ├─ Emit streamToken()          │
   ├─ ... (more tokens)           │
   └─ Emit streamFinished()       │
      ├─ Parse response           │
      └─ m_decisionPhaseWaiting   │
         = FALSE ←────────────────┤
                                  │
                                  └─ Exit wait loop
                                     └─ Execute action
                                        └─ Call real tools
                                           └─ Success!
```

---

## State Queries

### Check Status
```cpp
bool running = executor->isRunning();           // Currently executing?
int iter = executor->currentIteration();        // What iteration?
int max = executor->maxIterations();            // How many total?
const LoopState& state = executor->currentState();
```

### Get Results
```cpp
const QVector<ExecutionLog>& logs = executor->executionLogs();
ExecutionLog log = executor->iterationLog(3);   // Get iteration 3
QString summary = executor->executionSummary(); // Human-readable

// Log contains:
// - iteration, timestamp, cycleTimeMs
// - perceptionSummary, decisionReasoning, actionDescription
// - success, errorMessage, toolsUsed, filesModified[]
```

---

## Common Patterns

### Pattern 1: Stop Button
```cpp
QPushButton* stop = new QPushButton("Stop");
connect(stop, &QPushButton::clicked, 
        executor, &BoundedAutonomousExecutor::requestShutdown);
```

### Pattern 2: Progress Bar
```cpp
QProgressBar* bar = new QProgressBar();
connect(executor, QOverload<int, int, const QString&>::of(&BoundedAutonomousExecutor::progressUpdated),
        this, [bar](int iter, int max, const QString&) {
    bar->setMaximum(max);
    bar->setValue(iter);
});
```

### Pattern 3: Log Display
```cpp
QPlainTextEdit* log = new QPlainTextEdit();
connect(executor, &BoundedAutonomousExecutor::outputLogged,
        log, &QPlainTextEdit::appendPlainText);
```

### Pattern 4: Completion Handler
```cpp
connect(executor, &BoundedAutonomousExecutor::loopFinished, this, [this]() {
    QString summary = executor->executionSummary();
    QMessageBox::information(this, "Done", summary);
});
```

---

## Error Scenarios

### Scenario 1: Model Hangs
- Inference doesn't respond
- Action phase waits 5 seconds
- Timeout → `actionPhaseFailed()` signal
- Iteration marked failed
- Loop continues (unless confidence drops)

**User sees**: "Decision phase timeout" in logs

### Scenario 2: Tool Fails
- Tool execution returns error
- `onToolExecutionError()` updates state
- `actionPhaseFailed()` signal
- Feedback includes: "action failed: X"
- Next iteration: model learns and adapts

**User sees**: Tool error in progress logs

### Scenario 3: User Stops Loop
- Click stop button
- `requestShutdown()` called
- Loop checks shutdown flag
- Timer stops
- `loopStopped()` signal
- Clean exit

**User sees**: "Loop stopped by user"

---

## Implementation Checklist

### Before First Run
- [ ] InferenceEngine initialized
- [ ] AgenticToolExecutor initialized
- [ ] MultiTabEditor available
- [ ] TerminalPool available
- [ ] CMakeLists.txt includes bounded_autonomous_executor files

### After Integration
- [ ] Create BoundedAutonomousExecutor instance
- [ ] Connect UI signals (progress, output, stop)
- [ ] Start loop via startAutonomousLoop()
- [ ] Test with maxIterations=1 first
- [ ] Test stop button
- [ ] Review logs from first run

### Before Production
- [ ] Error handling validated
- [ ] Timeout behavior verified (5 second max)
- [ ] Tool execution confirmed (not mock)
- [ ] Logging complete and readable
- [ ] UI responsive during inference
- [ ] Multiple runs successful

---

## Performance Tips

### Faster Loops
1. Use smaller iteration limits (5 instead of 10)
2. Use faster models (Q4_0 vs F32)
3. Reduce prompt size (fewer files in context)
4. Increase timeout if model is slow (action phase waits)

### More Reliable
1. Start with 3-5 iterations (learn model behavior)
2. Monitor logs closely (understand decisions)
3. Gradually increase iteration limit
4. Adjust confidence threshold if exiting early

### Better Results
1. Write clear, specific task descriptions
2. Keep codebase clean (fewer files to analyze)
3. Run unit tests between loops (validation)
4. Review logs between runs (tune model prompts)

---

## Debugging

### Enable Verbose Logging
```cpp
// Constructor already does structured logging
// Check QDebug output for all phase transitions
```

### Check Real-Time Output
```
[12:34:56.123] DEBUG | DECISION | Querying inference engine...
[12:34:56.234] TOKEN[123456]: ACTION_TYPE
[12:34:56.345] TOKEN[123456]: : refactor
[12:34:57.100] DEBUG | STREAM_COMPLETE | Response length: 89 chars
[12:34:57.101] DEBUG | ACTION | Executing action: refactor with details: src/main.cpp|Optimize
[12:34:57.500] ACTION: refactor - SUCCESS
```

### Validate State
```cpp
// After iteration
ExecutionLog log = executor->iterationLog(3);
qDebug() << "Iteration 3:";
qDebug() << "  Success:" << log.success;
qDebug() << "  Action:" << log.actionDescription;
qDebug() << "  Tools:" << log.toolsUsed;
qDebug() << "  Time:" << log.cycleTimeMs << "ms";
```

### Monitor Signals
```cpp
connect(executor, &BoundedAutonomousExecutor::iterationStarted,
        this, [](int i) { qDebug() << "Iteration" << i << "started"; });
        
connect(executor, &BoundedAutonomousExecutor::decisionPhaseComplete,
        this, [](int i, const QString& d) { qDebug() << "Decision:" << d.left(50); });
        
connect(executor, &BoundedAutonomousExecutor::actionPhaseFailed,
        this, [](int i, const QString& e) { qDebug() << "Action failed:" << e; });
```

---

## FAQ

**Q: Why does decision phase take so long?**
A: It's querying the model (1200-2000ms). Async design keeps UI responsive during this.

**Q: Can I stop mid-iteration?**
A: Yes! Click stop button. Action phase exits, no new iterations start.

**Q: What if inference hangs?**
A: Action phase times out after 5 seconds, iteration marked failed.

**Q: Is tool execution synchronous?**
A: Yes, tools execute sequentially per iteration. Decision phase is async.

**Q: Can I adjust max iterations?**
A: Yes, pass different value: `startAutonomousLoop(task, 20);`

**Q: Where are logs stored?**
A: In-memory only. Capture via `outputLogged` signal or query `executionLogs()`.

**Q: Can tools run in parallel?**
A: Not in current design (serial per iteration). Future enhancement possible.

**Q: How do I validate results?**
A: Query `executionSummary()` or `iterationLog(i)` for detailed audit trail.

---

## Files to Know

| File | Purpose |
|------|---------|
| `bounded_autonomous_executor.hpp` | Class definition, signals, state structs |
| `bounded_autonomous_executor.cpp` | Implementation, async callbacks, tool dispatch |
| `ASYNC_INFERENCE_INTEGRATION.md` | Deep dive: architecture, sync, timing |
| `ASYNC_INFERENCE_USAGE_GUIDE.md` | Examples, patterns, troubleshooting |
| `ASYNC_INFERENCE_CODE_CHANGES.md` | What changed, why, verification |
| `ASYNC_INFERENCE_FINAL_SUMMARY.md` | Complete overview, signal flow, state |

---

## Key Concepts

### Non-Blocking Async
```cpp
// Perception (fast, sync)    → [50ms] → Decision query (fast dispatch)
// Decision (async in bg)      → [1200-2000ms background] → Completion signal
// Action (waits for signal)   → [0-5000ms] → Tool execution
// Feedback (fast, sync)       → [30ms] → Next iteration prep
```

### Request ID Filtering
```cpp
// Each decision gets unique ID: currentMSecsSinceEpoch()
// All signals filtered by ID to prevent cross-talk
// Allows future concurrent requests
```

### Timeout Protection
```cpp
// 5-second max wait in action phase
// Prevents infinite loop if inference hangs
// Graceful error handling + logging
```

### Feedback Loop
```cpp
// Results from iteration N → Context for iteration N+1
// Model learns from consequences of its actions
// Improves decisions over multiple iterations
```

---

## Production Deployment

### Setup
1. Add files to CMakeLists.txt
2. Create UI dialog with task input + progress display
3. Wire signals to status bar, terminal, buttons
4. Test with maxIterations=1 (single test run)

### Monitoring
1. Review logs after each run
2. Check execution summary for success rate
3. Validate tool execution (not mock)
4. Monitor iteration times (should be 2-3 seconds each)

### Safety
1. Keep maxIterations ≤ 10 initially
2. Manual approval of first autonomous runs
3. Clear stop button always visible
4. Full audit trail in logs

### Scaling
1. Increase iterations based on confidence
2. Reduce if confidence drops (model struggling)
3. Adjust prompt size (fewer files = faster)
4. Monitor resource usage (InferenceEngine + tools)

---

## Success Metrics

| Metric | Target | How to Verify |
|--------|--------|---------------|
| UI responsive | Always responsive | UI doesn't freeze during inference |
| Tool execution | Real, not mock | Check logs for `Tool 'refactor' executed` |
| Timeout | Max 5 seconds | Logs show no decisions >5000ms |
| Iterations | Bounded | Loop stops at maxIterations |
| Stop button | Works instantly | Click stop, loop exits within 100ms |
| Error handling | Graceful | Failures logged, loop continues |
| Audit trail | Complete | All logs stored in executionLogs() |

---

That's it! You now have everything needed to use the async autonomous executor.

Good luck! 🚀
