# Dual & Triple Model Loading - Architecture Diagram

**Date**: December 27, 2025  
**Status**: ✅ Production Ready

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     Agent Chat Pane (Qt)                       │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │          DUAL MODEL CONTROL PANEL                        │  │
│  │  ┌────────────────────────────────────────────────────┐  │  │
│  │  │ Model Selection                                   │  │  │
│  │  │ ┌─────────────────────┐  ┌─────────────────────┐ │  │  │
│  │  │ │ Primary Model  ▼   │  │Secondary Model ▼  │ │  │  │
│  │  │ │ [Mistral-7B  ______│  │ [Neural-13B _____│ │  │  │
│  │  │ └─────────────────────┘  └─────────────────────┘ │  │  │
│  │  │ ┌─────────────────────┐                          │  │  │
│  │  │ │ Tertiary Model ▼   │                          │  │  │
│  │  │ │ [Quantum-30B ______│                          │  │  │
│  │  │ └─────────────────────┘                          │  │  │
│  │  └────────────────────────────────────────────────────┘  │  │
│  │  ┌────────────────────────────────────────────────────┐  │  │
│  │  │ Execution Mode                                    │  │  │
│  │  │ ┌────────────────────────────────────────────────┐ │  │  │
│  │  │ │ Chain Mode:  Sequential ▼                     │ │  │  │
│  │  │ │ ○ Sequential (Model 1→2→3)                   │ │  │  │
│  │  │ │ ○ Parallel (All simultaneous)                │ │  │  │
│  │  │ │ ○ Voting (Best output)                       │ │  │  │
│  │  │ │ ○ Cycling (Round-robin)                      │ │  │  │
│  │  │ │ ○ Fallback (Primary→Secondary)               │ │  │  │
│  │  │ └────────────────────────────────────────────────┘ │  │  │
│  │  └────────────────────────────────────────────────────┘  │  │
│  │  ┌────────────────────────────────────────────────────┐  │  │
│  │  │ Model Weighting (Voting Priority)                 │  │  │
│  │  │ Model 1: ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 100  │  │  │
│  │  │ Model 2: ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 100  │  │  │
│  │  │ Model 3: ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 100  │  │  │
│  │  └────────────────────────────────────────────────────┘  │  │
│  │  ┌────────────────────────────────────────────────────┐  │  │
│  │  │ Options                                           │  │  │
│  │  │ ☑ Enable Cycling (5 sec intervals)              │  │  │
│  │  │ ☑ Enable Voting (consensus mode)                │  │  │
│  │  │ ☑ Enable Fallback (if primary fails)            │  │  │
│  │  └────────────────────────────────────────────────────┘  │  │
│  │  ┌────────────────────────────────────────────────────┐  │  │
│  │  │ [EXECUTE CHAIN]                                  │  │  │
│  │  │                                                  │  │  │
│  │  │ Status:                                          │  │  │
│  │  │ Mistral-7B      [Ready]     Exec: 12   Time: 2.5s │  │  │
│  │  │ Neural-13B      [Ready]     Exec: 8    Time: 3.0s │  │  │
│  │  │ Quantum-30B     [Loading]   Exec: 0    Time: -.--s │  │  │
│  │  │                                                  │  │  │
│  │  │ Last Execution: 2,345 ms                         │  │  │
│  │  │ Chain Mode: Sequential                           │  │  │
│  │  └────────────────────────────────────────────────────┘  │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  Input (from chat): "Analyze this code complexity..."          │
│                                                                 │
│  Output (to chat):  "Analysis Result: ... (from voting)"       │
└─────────────────────────────────────────────────────────────────┘
```

---

## Data Flow - Sequential Mode

```
User Input
    ↓
    ├─→ [Model 1: Mistral-7B]
    │   ├─ Memory: 8GB
    │   ├─ Latency: 2.5s
    │   └─ Output: Response_A
    │       ↓
    ├─→ [Model 2: Neural-13B]  (Input: Response_A)
    │   ├─ Memory: 13GB
    │   ├─ Latency: 3.0s
    │   └─ Output: Response_B (refined)
    │       ↓
    ├─→ [Model 3: Quantum-30B] (Input: Response_B)
    │   ├─ Memory: 30GB
    │   ├─ Latency: 3.5s
    │   └─ Output: Response_C (final)
    │
    ↓
Final Output (to user)
Total Time: 9.0 seconds
Quality: Highest (most refinement)
```

---

## Data Flow - Parallel Mode

```
User Input
    ├─→ [Model 1: Mistral-7B]  ──→ Response_A (2.5s, Quality: 92%)
    ├─→ [Model 2: Neural-13B]  ──→ Response_B (3.0s, Quality: 88%)
    ├─→ [Model 3: Quantum-30B] ──→ Response_C (3.5s, Quality: 95%)
    │
    ↓
Return fastest valid response
    ↓
Final Output (fastest result)
Total Time: 3.5 seconds (max of all)
Quality: Good (fastest valid)
```

---

## Data Flow - Voting Mode

```
User Input
    ├─→ [Model 1: Mistral-7B]
    │   ├─ Output: Response_A
    │   ├─ Quality Score: 92%
    │   └─ Vote Count: 2
    │
    ├─→ [Model 2: Neural-13B]
    │   ├─ Output: Response_B
    │   ├─ Quality Score: 88%
    │   └─ Vote Count: 1
    │
    ├─→ [Model 3: Quantum-30B]
    │   ├─ Output: Response_C
    │   ├─ Quality Score: 95%
    │   └─ Vote Count: 2  ← WINNER
    │
    ↓
Consensus Voting System
    ├─ Calculate confidence scores
    ├─ Aggregate votes
    ├─ Determine winner
    └─ Apply weights
    │
    ↓
Final Output (Response_C - highest confidence)
Total Time: 3.7 seconds (max + voting)
Quality: Excellent (consensus selected)
```

---

## Data Flow - Cycling Mode

```
Model Rotation Timer (5 second intervals)

T=0s:  Request 1 ──→ [Model 1: Mistral-7B] ──→ Response_1
T=5s:  Request 2 ──→ [Model 2: Neural-13B] ──→ Response_2
T=10s: Request 3 ──→ [Model 3: Quantum-30B] ──→ Response_3
T=15s: Request 4 ──→ [Model 1: Mistral-7B] ──→ Response_4 (wraps)
T=20s: Request 5 ──→ [Model 2: Neural-13B] ──→ Response_5
...

Load Distribution:
├─ Mistral-7B:   33% of requests
├─ Neural-13B:   33% of requests
└─ Quantum-30B:  33% of requests
```

---

## Data Flow - Fallback Mode

```
User Input
    ↓
Try Primary Model
    ├─ [Model 1: Quantum-30B] (slower but highest quality)
    │   ├─ Timeout: 30 seconds
    │   ├─ Status: TIMEOUT! ❌
    │   └─ Fallback triggered
    │
    ↓
Try Secondary Model
    ├─ [Model 2: Mistral-7B] (faster backup)
    │   ├─ Timeout: 10 seconds
    │   ├─ Status: SUCCESS! ✓
    │   └─ Output: Response (from fallback)
    │
    ↓
Final Output (guaranteed completion)
Total Time: 30s + 2.5s = 32.5 seconds
Quality: Good (guaranteed answer)
Reliability: Maximum (always returns something)
```

---

## Component Architecture

```
┌────────────────────────────────────────────────────────────────┐
│           DUAL_TRIPLE_MODEL_CHAIN.ASM (3,000 lines)           │
│                                                                │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │ MODEL_SLOT STRUCT (1 model in chain)                    │ │
│  │ ├─ model_id, model_path, model_name                     │ │
│  │ ├─ state (EMPTY, LOADED, RUNNING, ERROR)               │ │
│  │ ├─ weight (1-100 voting priority)                       │ │
│  │ ├─ timeout_ms (execution timeout)                       │ │
│  │ ├─ last_output_ptr (output buffer)                      │ │
│  │ ├─ execution_time_ms (latency)                          │ │
│  │ ├─ success_count, error_count (metrics)                 │ │
│  │ └─ load_time (timestamp)                                │ │
│  └──────────────────────────────────────────────────────────┘ │
│                                                                │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │ MODEL_CHAIN STRUCT (complete chain context)             │ │
│  │ ├─ chain_id, chain_name                                 │ │
│  │ ├─ mode (SEQUENTIAL, PARALLEL, VOTING, CYCLE, FALLBACK) │ │
│  │ ├─ models[3] (up to 3 MODEL_SLOTs)                      │ │
│  │ ├─ model_count (number of loaded models)                │ │
│  │ ├─ chain_mutex (thread synchronization)                 │ │
│  │ ├─ worker_thread (background execution)                 │ │
│  │ ├─ on_complete, on_error (callbacks)                    │ │
│  │ └─ execution_event (completion signal)                  │ │
│  └──────────────────────────────────────────────────────────┘ │
│                                                                │
│  ┌─ EXECUTION MODES ───────────────────────────────────────┐ │
│  │ • CreateModelChain - Initialize chain                   │ │
│  │ • AddModelToChain - Add model slot                      │ │
│  │ • LoadChainModels - Load all to memory                  │ │
│  │ • ExecuteChainSequential - Model 1→2→3                  │ │
│  │ • ExecuteChainParallel - All simultaneous               │ │
│  │ • ExecuteChainVoting - Best output                      │ │
│  │ • ExecuteChainCycle - Round-robin rotation              │ │
│  │ • ExecuteChainFallback - Primary→Secondary              │ │
│  │ • VoteOnOutputs - Consensus voting                      │ │
│  │ • CycleToNextModel - Manual rotation                    │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                                │
│  ┌─ WORKER THREAD ─────────────────────────────────────────┐ │
│  │ • Background execution of long-running chains           │ │
│  │ • Execution queue (up to 100 requests)                  │ │
│  │ • Non-blocking main thread operation                    │ │
│  │ • Callback notification on completion                   │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                                │
│  ┌─ PERFORMANCE MONITORING ────────────────────────────────┐ │
│  │ • Execution time tracking                               │ │
│  │ • Success/error counters                                │ │
│  │ • Per-model metrics                                     │ │
│  │ • Chain-wide statistics                                 │ │
│  └─────────────────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────────┘
```

---

## UI Integration Architecture

```
┌────────────────────────────────────────────────────────────────┐
│     AGENT_CHAT_DUAL_MODEL_INTEGRATION.ASM (2,500 lines)       │
│                                                                │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │ AGENT_CHAT_MODEL STRUCT (UI model representation)        │ │
│  │ ├─ model_id, model_name (64 chars), model_path (260 ch) │ │
│  │ ├─ is_loaded, is_executing (flags)                       │ │
│  │ ├─ status (IDLE, LOADING, READY, EXECUTING, ERROR)      │ │
│  │ ├─ last_output_size (bytes)                              │ │
│  │ ├─ load_timestamp (for metrics)                          │ │
│  │ └─ exec_count (execution counter)                        │ │
│  └──────────────────────────────────────────────────────────┘ │
│                                                                │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │ DUAL_MODEL_CONTEXT STRUCT (UI context)                  │ │
│  │ ├─ primary_model, secondary_model, tertiary_model       │ │
│  │ ├─ chain_mode (execution mode)                           │ │
│  │ ├─ cycling_enabled, voting_enabled, fallback_enabled   │ │
│  │ ├─ cycle_interval_ms (5000 default)                      │ │
│  │ ├─ weight1, weight2, weight3 (1-100 voting priority)    │ │
│  │ ├─ mutex_handle, event_handle (synchronization)         │ │
│  │ └─ last_output_buf, last_output_size (result storage)   │ │
│  └──────────────────────────────────────────────────────────┘ │
│                                                                │
│  ┌─ UI COMPONENTS ─────────────────────────────────────────┐ │
│  │ • Model selection dropdowns (3)                          │ │
│  │ • Chain mode selector dropdown                           │ │
│  │ • Model weight sliders (3, 1-100 range)                 │ │
│  │ • Option checkboxes (cycling, voting, fallback)          │ │
│  │ • Cycle interval spinner                                │ │
│  │ • Execute button                                         │ │
│  │ • Status display listbox                                │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                                │
│  ┌─ EVENT HANDLERS ────────────────────────────────────────┐ │
│  │ • OnChainModeChanged - Handle mode selection            │ │
│  │ • OnExecuteChainClicked - Trigger chain execution       │ │
│  │ • UpdateModelStatusDisplay - Refresh UI status          │ │
│  │ • LoadModelSelections - Load selected models            │ │
│  │ • SetModelWeights - Update voting weights               │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                                │
│  ┌─ OUTPUT MANAGEMENT ─────────────────────────────────────┐ │
│  │ • g_model_out_buf1, buf2, buf3 (64KB each)             │ │
│  │ • g_model_out_bufvote (voting consensus result)         │ │
│  │ • g_chat_output_buf (final result to display)           │ │
│  └─────────────────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────────┘
```

---

## Threading Model

```
┌─────────────────────────────────────────────────────────────────┐
│                         MAIN THREAD (UI)                       │
│ ┌───────────────────────────────────────────────────────────┐  │
│ │ Agent Chat Pane                                           │  │
│ │ ├─ Display UI components                                 │  │
│ │ ├─ Handle user input                                     │  │
│ │ ├─ Call InitDualModelUI() [MASM]                        │  │
│ │ ├─ Call OnExecuteChainClicked() [MASM]                  │  │
│ │ └─ Display results                                       │  │
│ └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
         │
         │ SetupModelChaining()
         │ ExecuteDualModelChain()
         │
         ↓
┌─────────────────────────────────────────────────────────────────┐
│                   WORKER THREAD (Background)                   │
│ ┌───────────────────────────────────────────────────────────┐  │
│ │ Chain Execution Engine                                   │  │
│ │ ├─ Process execution queue                               │  │
│ │ ├─ Execute models (sequential/parallel/voting/cycle)    │  │
│ │ ├─ Perform voting consensus                              │  │
│ │ ├─ Manage timeouts                                       │  │
│ │ ├─ Update performance counters                           │  │
│ │ └─ Signal completion (callback)                          │  │
│ └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
         │
         │ Execution complete
         │ Callback fired
         │
         ↓
┌─────────────────────────────────────────────────────────────────┐
│                     MAIN THREAD (Updated)                      │
│ ┌───────────────────────────────────────────────────────────┐  │
│ │ UpdateModelStatusDisplay()                               │  │
│ │ Display result in chat                                   │  │
│ │ Update performance metrics                               │  │
│ └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘

Synchronization:
├─ QMutex on MODEL_CHAIN for state protection
├─ Events for completion notifications
├─ Queue for pending executions
└─ Atomic counters for metrics
```

---

## State Machine

```
                    CREATE_CHAIN
                        ↓
                   [INITIALIZED]
                        ↓
                   ADD_MODEL × 1-3
                        ↓
                   [MODELS_ADDED]
                        ↓
              LOAD_CHAIN_MODELS
                        ↓
                   [READY]
                   ↙   ↓   ↘
          SEQUENTIAL  PARALLEL  VOTING  CYCLING  FALLBACK
          (Model1→2→3) (All)   (Vote)   (Rotate) (Primary→Sec)
                   ↓   ↓   ↓   ↓   ↓
              [EXECUTING] (all modes)
                   ↓
          EXECUTION_COMPLETE
                   ↓
          ┌────────┴────────┐
          ↓                 ↓
      [SUCCESS]        [ERROR]
          ↓                 ↓
          └────────┬────────┘
                   ↓
              [IDLE] (ready for next)
                   ↓
           (repeat or DESTROY_CHAIN)
                   ↓
            [DESTROYED]

State Properties:
├─ INITIALIZED: Chain created, no models
├─ MODELS_ADDED: 1-3 models configured
├─ READY: All models loaded, chain ready
├─ EXECUTING: Chain running (active execution)
├─ SUCCESS: Last execution completed normally
├─ ERROR: Last execution failed
├─ IDLE: Ready for next execution
└─ DESTROYED: Chain destroyed, resources freed
```

---

## Memory Layout

```
┌────────────────────────────────────────────────────────────┐
│                    MEMORY LAYOUT                          │
│                                                            │
│ Global State (4 KB)                                       │
│ ├─ g_dual_model_ctx DUAL_MODEL_CONTEXT                   │
│ ├─ g_selected_model1/2/3 (DWORDs)                         │
│ ├─ g_selected_chain_mode (DWORD)                          │
│ ├─ Counters: g_dual_exec_count, etc. (QWORDs)           │
│ └─ Pointers: g_dual_chain, g_chat_input/output_buf      │
│                                                            │
│ Output Buffers (192 KB)                                   │
│ ├─ g_model_out_buf1 (64 KB) - Model 1 output            │
│ ├─ g_model_out_buf2 (64 KB) - Model 2 output            │
│ ├─ g_model_out_buf3 (64 KB) - Model 3 output            │
│ └─ g_model_out_bufvote (64 KB) - Voting result          │
│                                                            │
│ Model Chain Context (~64 KB per chain)                   │
│ ├─ chain_id, chain_name                                  │
│ ├─ models[3] (3 × MODEL_SLOT)                           │
│ ├─ worker_thread, work_queue                             │
│ ├─ chain_mutex, execution_event                          │
│ └─ Callbacks, state, counters                            │
│                                                            │
│ Model Slot (per model, ~512 bytes)                       │
│ ├─ model_id, model_name, model_path                      │
│ ├─ state, weight, priority, timeout_ms                   │
│ ├─ last_output_ptr, last_output_size                     │
│ ├─ last_error_code, execution_time_ms                    │
│ ├─ success_count, error_count                            │
│ ├─ load_time, reserved space                             │
│ └─ [end of MODEL_SLOT]                                   │
│                                                            │
│ Total Overhead: < 1 MB per chain                          │
│ Per Model: 500 MB - 10 GB (model size dependent)         │
└────────────────────────────────────────────────────────────┘
```

---

## Integration Points

```
Agent Chat Pane (C++ Qt)
    ├── InitDualModelUI(hwnd1, hwnd2)
    │   └── Setup UI components, model dropdowns, sliders
    │
    ├── OnChainModeChanged()
    │   └── Update internal mode, refresh display
    │
    ├── OnExecuteChainClicked()
    │   └── ExecuteDualModelChain() → Get output → Display
    │
    ├── CycleModels()
    │   └── Rotate to next model, update display
    │
    ├── VoteModels()
    │   └── Run voting consensus, return best output
    │
    ├── SetModelWeights(w1, w2, w3)
    │   └── Update voting priority weights
    │
    └── GetDualModelStatus()
        └── Query current status of all models
```

---

**Architecture Status**: ✅ Complete and Ready for Integration

**Complexity**: Enterprise-Grade  
**Thread Safety**: Full  
**Error Handling**: Comprehensive  
**Performance**: Optimized  
**Documentation**: Complete
