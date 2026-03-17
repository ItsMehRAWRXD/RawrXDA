# Phase 7.5 Tool Pipeline - Quick Reference Guide

## 📍 File Location
`src/masm/final-ide/tool_pipeline.asm` (2,000+ LOC, 6 public functions)

## 🔗 Registry Configuration
**Key**: `HKCU\Software\RawrXD\ToolPipeline`

| Setting | Type | Default | Purpose |
|---------|------|---------|---------|
| `MaxRouters` | DWORD | 4 | Max concurrent routers |
| `MaxModels` | DWORD | 10 | Max models to register |
| `RoutingMode` | DWORD | 2 | Routing algorithm (1-4) |
| `BatchTimeout` | QWORD | 5000 | Batch timeout (ms) |
| `RebalanceInterval` | QWORD | 5000 | Rebalance frequency (ms) |
| `ResourceStrategy` | DWORD | 2 | Resource allocation (1-3) |
| `LogLevel` | DWORD | 2 | Logging verbosity |

## 📋 Routing Modes

```
1 = ROUND_ROBIN         (Next router in sequence)
2 = LEAST_LOADED        (Minimum load %)
3 = AFFINITY            (Sticky routing)
4 = LATENCY_AWARE       (Lowest latency)
```

## 🛠️ Public API Quick Reference

### 1. Initialize Pipeline
```asm
mov ecx, 4              ; max routers
mov edx, 10             ; max models
call ToolPipeline_Initialize
; RAX = 0 (success) or error code
```

### 2. Route Request
```asm
mov rcx, requestId      ; unique ID
mov edx, TOOL_TYPE_INFERENCE
xor r8, r8              ; no args
call ToolPipeline_RouteRequest
; RAX = router ID (1-64) or error
```

### 3. Register Model
```asm
mov ecx, modelId        ; model ID
mov rdx, capabilities   ; feature bitmask
xor r8, r8              ; resource req pointer
call ToolPipeline_RegisterModel
; RAX = 0 (success) or error code
```

### 4. Create Batch
```asm
mov ecx, 1000           ; batch size
mov rdx, 5000           ; 5 second timeout
call ToolPipeline_CreateBatch
; RAX = batch ID (1-512) or error
```

### 5. Schedule Resources
```asm
mov ecx, 8              ; 8 nodes
mov edx, 32             ; 32 cores per node
mov r8, 256*1024        ; 256GB per node
call ToolPipeline_ScheduleResources
; RAX = 0 (success) or error code
```

### 6. Get Metrics
```asm
call ToolPipeline_GetPipelineMetrics
; RAX = pointer to TOOL_PIPELINE_METRICS
mov r8, [rax + OFFSET TOOL_PIPELINE_METRICS.RequestsRouted]
```

## 📊 Tool Types

```c
TOOL_TYPE_INFERENCE     = 1    // Primary inference
TOOL_TYPE_EMBEDDING     = 2    // Vector generation
TOOL_TYPE_RANKING       = 3    // Semantic ranking
TOOL_TYPE_CLASSIFICATION= 4    // Classification
TOOL_TYPE_SYNTHESIS     = 5    // Text/image generation
TOOL_TYPE_CUSTOM        = 6    // User-defined
```

## ⚠️ Error Codes

| Code | Meaning | Recovery |
|------|---------|----------|
| `0x00000000` | Success | N/A |
| `0x00000001` | Invalid router | Check MaxRouters |
| `0x00000002` | Model limit | Increase MaxModels |
| `0x00000003` | Batch full | Dispatch batch |
| `0x00000004` | Resource exhausted | Increase node count |
| `0x00000005` | Routing failed | Check initialization |
| `0x00000006` | Invalid batch | Check batch ID |
| `0x00000007` | Scheduling failed | Check resources |

## 🔄 Typical Workflow

```
1. ToolPipeline_Initialize(4, 10)
   ↓
2. Register 3 models:
   - Model 1: Inference
   - Model 2: Embedding
   - Model 3: Ranking
   ↓
3. ToolPipeline_ScheduleResources(2, 32, 256GB)
   ↓
4. Main loop:
   a. ToolPipeline_RouteRequest(reqId, INFERENCE, args)
      → Router 1, 2, 3, or 4
   b. ToolPipeline_CreateBatch(1000, 5000)
      → Batch ID 42
   c. Add 1000 requests to batch
   d. Batch auto-processes (timeout or full)
   e. Retrieve results
   ↓
5. Get metrics:
   ToolPipeline_GetPipelineMetrics()
   → Monitor routing, batching, resource usage
```

## 📈 Metrics Available

```c
RequestsReceived        // Total requests
RequestsRouted          // Successfully routed
RequestsFailed          // Routing failures
RequestsCompleted       // Completed requests
AvgRoutingLatency       // Decision time (us)
BatchesCreated          // Total batches
BatchesCompleted        // Finished batches
ModelsLoaded            // Currently active
ModelsRegistered        // Total registered
NodesActive             // Active cluster nodes
ResourceRebalances      // Rebalance operations
SchedulingDecisions     // Scheduler calls
```

## 🔐 Thread Safety

- ✅ **All functions thread-safe** via critical sections
- ✅ **Routers protected** by shared ManagerLock
- ✅ **Models protected** by shared ManagerLock
- ✅ **Batches protected** by shared ManagerLock
- ✅ **Scheduler protected** by shared ManagerLock

## 💾 Memory Footprint

```
Router registry:    64 × 104B = 6.6 KB
Model registry:     256 × 72B = 18 KB
Batch array:        512 × 80B = 40 KB
Scheduler nodes:    128 × 112B = 14.4 KB
Manager + metrics:  240B + 96B = 336 B
────────────────────────────────────
TOTAL:  ~80 KB for full infrastructure
```

## ⚡ Performance

| Operation | Latency | Throughput |
|-----------|---------|-----------|
| Routing | O(N) where N≤64 | 100k+ req/sec |
| Model registration | O(1) | 10k+ models/sec |
| Batch creation | O(1) | 1k+ batches/sec |
| Resource scheduling | O(N) where N≤128 | 1k+ ops/sec |

## 🧪 Test Functions

**Test_ToolPipeline_Routing()** - Phase 5
- Initialize with 4 routers
- Register 3 models
- Route 5 requests
- Verify distribution

**Test_ToolPipeline_Batching()** - Phase 5
- Create batch (size=100, timeout=5s)
- Add 50 requests
- Check state transitions
- Verify metrics

## 📝 Logging Output

```
[INFO] Request routed (request_id=1001, tool_type=1, router=2)
[INFO] Batch created (batch_id=42, max_size=1000)
[INFO] Model registered (model_id=10, type=1, gpu_mem=24576)
[INFO] Resource scheduling (nodes=8, strategy=2)
[ERROR] Routing failed (request_id=1001, error=5)
```

## 🚀 Integration Points

**Below (Dependencies)**:
- Phase 7.4 Security - Validates routing decisions
- Phase 7.6 Hot-patching - Patches models in-flight
- Phase 7.7 Failure Recovery - Recovers from errors

**Above (Dependents)**:
- Phase 7.8 WebUI - Displays metrics
- Phase 7.9 Inference - Uses for distribution
- Phase 7.10 Knowledge Base - Models as tools

## 💡 Pro Tips

1. **Set RoutingMode early**: Changes affect all future routing
2. **Batch size matters**: Large batches → high throughput, high latency
3. **Resource rebalancing**: Automatic every 5s, no manual intervention
4. **Monitor metrics**: Track RequestsRouted, AvgRoutingLatency, NodesActive
5. **Conservative strategy**: Use for stability (resource headroom)
6. **Aggressive strategy**: Use for maximum throughput (tight allocation)

## 🔍 Debugging Checklist

- [ ] Manager initialized (check return code)
- [ ] Models registered (count < 256)
- [ ] Routers available (count > 0)
- [ ] Routing mode set (1-4)
- [ ] Resources scheduled (nodes > 0)
- [ ] Metrics accessible (GetPipelineMetrics)
- [ ] Thread safety (no race conditions)
- [ ] Memory allocated (no leaks)

---

**Quick Links**:
- Full Spec: `PHASE7_5_TOOL_PIPELINE_COMPLETION_REPORT.md`
- Source: `src/masm/final-ide/tool_pipeline.asm`
- Registry: `HKCU\Software\RawrXD\ToolPipeline`
