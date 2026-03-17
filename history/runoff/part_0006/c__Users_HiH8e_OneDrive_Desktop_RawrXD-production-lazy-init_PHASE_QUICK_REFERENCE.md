# 10-PHASE ARCHITECTURE - QUICK REFERENCE

## Call Flow Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│ PHASE 1: PiFabric_Init()                                            │
│ Initialize subsystem globally (call once)                           │
└──────────────────────────┬──────────────────────────────────────────┘
                          │
┌──────────────────────────▼──────────────────────────────────────────┐
│ PHASE 2: PiFabric_ProbeDevice(pStats)                               │
│ Probe system capabilities (CPU, RAM, disk speed)                    │
└──────────────────────────┬──────────────────────────────────────────┘
                          │
┌──────────────────────────▼──────────────────────────────────────────┐
│ PHASE 3: PiFabric_MakePlan(hModel, pPlan)                          │
│ Generate optimal execution plan based on model size                │
└──────────────────────────┬──────────────────────────────────────────┘
                          │
┌──────────────────────────▼──────────────────────────────────────────┐
│ PHASE 5: GGUFChain_SelectAutoMask() + SelectChainMode()           │
│ Choose memory strategy (MEMORY/DISC/MMAP/HYBRID)                  │
└──────────────────────────┬──────────────────────────────────────────┘
                          │
┌──────────────────────────▼──────────────────────────────────────────┐
│ PHASE 6: PiRam_Policy_GetDefaults(pPolicy)                         │
│ Load policy configuration (passes, quant, chunk size)             │
└──────────────────────────┬──────────────────────────────────────────┘
                          │
┌──────────────────────────▼──────────────────────────────────────────┐
│ PHASE 9: PiFabric_ApplyRuntimePolicy(hFabric, pHints)             │
│ Apply policy with user hints                                       │
└──────────────────────────┬──────────────────────────────────────────┘
                          │
┌──────────────────────────▼──────────────────────────────────────────┐
│ PHASE 10: GGUFEngine_LoadAuto(path, size_lo, size_hi)             │
│ Load model with optimal parameters                                │
└──────────────────────────┬──────────────────────────────────────────┘
                          │
┌──────────────────────────▼──────────────────────────────────────────┐
│ PHASE 8: GGUFChain_Telemetry_Begin()                               │
│ Start performance counters for this operation                      │
└──────────────────────────┬──────────────────────────────────────────┘
                          │
┌──────────────────────────▼──────────────────────────────────────────┐
│ PHASE 10: GGUFEngine_StreamAuto(model, dst, bytes, method)        │
│ Stream tensor chunks with method selection & telemetry            │
└──────────────────────────┬──────────────────────────────────────────┘
                          │
┌──────────────────────────▼──────────────────────────────────────────┐
│ PHASE 8: GGUFChain_Telemetry_End(method, dwBytes)                 │
│ Record performance data for this chunk                            │
└──────────────────────────┬──────────────────────────────────────────┘
                          │
┌──────────────────────────▼──────────────────────────────────────────┐
│ PHASE 4: PiFabric_AdaptTick(hFabric, pStats)                      │
│ Adjust strategy based on runtime stats (OPTIONAL, periodic)      │
└──────────────────────────┬──────────────────────────────────────────┘
                          │
┌──────────────────────────▼──────────────────────────────────────────┐
│ PHASE 8: GGUFChain_Telemetry_Get(pOut)                            │
│ Retrieve accumulated statistics & metrics                         │
└──────────────────────────┬──────────────────────────────────────────┘
                          │
┌──────────────────────────▼──────────────────────────────────────────┐
│ PHASE 10: GGUFEngine_Close(hModel)                                 │
│ Cleanup and release resources                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Phase Summary

| # | Name | Purpose | Key Function | State |
|---|------|---------|--------------|-------|
| 1 | **Init** | Subsystem setup | `PiFabric_Init()` | Call once |
| 2 | **Probe** | Device caps | `PiFabric_ProbeDevice()` | Analyze hardware |
| 3 | **Plan** | Optimization | `PiFabric_MakePlan()` | Generate strategy |
| 4 | **Adapt** | Runtime tune | `PiFabric_AdaptTick()` | Periodic adjust |
| 5 | **Policy** | Method select | `GGUFChain_Select*()` | Choose strategy |
| 6 | **Config** | Policy mgmt | `PiRam_Policy_*()` | Load/save config |
| 7 | **C API** | Language bind | `PiFabric_*_C()` | Wrapper layer |
| 8 | **Telemetry** | Performance | `GGUFChain_Telemetry_*()` | Collect stats |
| 9 | **Runtime** | Apply policy | `PiFabric_ApplyRuntimePolicy()` | Set behavior |
| 10 | **Engine** | Execution | `GGUFEngine_*()` | Load/stream/close |

---

## Data Structures (Size x Layout)

### PiFabricHandle (48 bytes)
```
Offset  Field            Size  Purpose
──────  ─────────────────────  ────────────────
 0x00   hModel           4     Model handle ID
 0x04   pBase            4     Base memory pointer
 0x08   pData            4     Data section pointer
 0x0C   dwSizeLow        4     File size (low 32)
 0x10   dwSizeHigh       4     File size (high 32)
 0x14   dwMethodMask     4     Memory method flags
 0x18   dwChainMode      4     Chain strategy ID
 0x1C   dwTier           4     Optimization tier
 0x20   dwFlags          4     Status flags
 0x24   dwState          4     Runtime state
 0x28   dwReserved0      4     Reserved
 0x2C   dwReserved1      4     Reserved
```

### PiFabricPlan (28 bytes)
```
Offset  Field            Size  Purpose
──────  ─────────────────────  ────────────────
 0x00   dwMethodMask     4     Memory strategy bits
 0x04   dwChainMode      4     Chain mode (0=seq, 2=adaptive)
 0x08   dwTargetTier     4     Optimization target
 0x0C   dwPassCount      4     Number of passes
 0x10   dwQuantLevel     4     Quantization bits (4=Q4)
 0x14   dwChunkSize      4     Chunk size for streaming
 0x18   dwFlags          4     Additional options
```

### GGUFTelemetry (64 bytes)
```
┌─ MethodDisc (16 bytes)
│  ├─ dwCalls      : Call count
│  ├─ dwSuccess    : Success count
│  ├─ dwTotalMs    : Total ms
│  └─ dwBytes      : Total bytes
├─ MethodMemory (16 bytes) - Same structure
├─ MethodMMap (16 bytes)   - Same structure
├─ MethodHybrid (16 bytes) - Same structure
└─ dwFlags (4 bytes)
```

---

## Memory Strategy Selection

### By File Size

| Size | Method | Policy |
|------|--------|--------|
| <4MB | MEMORY | Load entirely in RAM |
| 4-256MB | MEMORY+MMAP+DISC | Hybrid adaptive |
| 256MB-4GB | MMAP+DISC | Prefer memory-mapped |
| >4GB | DISC | Direct disk streaming |

### Chain Modes

| Mode | ID | Behavior |
|------|----|----|
| SEQUENTIAL | 0 | Load tensors in order |
| ADAPTIVE | 2 | Auto-optimize based on hardware |

---

## Telemetry Tracking

Each method tracked separately:
- **MethodDisc**: Direct disk I/O method
- **MethodMemory**: Full in-memory method
- **MethodMMap**: Memory-mapped file method  
- **MethodHybrid**: Hybrid strategy method

Track per-method:
- Call count
- Success count
- Total milliseconds
- Total bytes transferred

---

## Integration Points

### Minimal Integration (3 functions)

```asm
; 1. Initialize once
call PiFabric_Init

; 2. Get device capabilities
lea eax, [probe_stats]
call PiFabric_ProbeDevice

; 3. Load model auto-optimized
call GGUFEngine_LoadAuto      ; Returns handle
```

### Full Integration (9 functions)

```asm
; Add adaptive tuning loop
Loop:
    ; Periodic adaptation
    call PiFabric_AdaptTick
    
    ; Check telemetry
    lea eax, [telemetry]
    call GGUFChain_Telemetry_Get
    
    ; Adjust if needed
    cmp [eax].dwErrors, threshold
    ja AdjustPolicy
    
    jmp Loop
```

---

## Function Signatures

### Phase 1: Init
```asm
PiFabric_Init()                           → EAX = 1 (success)
PiFabric_Open(path, method_mask, mode)    → EAX = handle
```

### Phase 2: Probe
```asm
PiFabric_ProbeDevice(pStats)              → EAX = 1 (success)
```

### Phase 3: Plan
```asm
PiFabric_MakePlan(hModel, pPlan)          → EAX = 1 (success)
```

### Phase 5: Policy Select
```asm
GGUFChain_SelectAutoMask(size_lo, size_hi)    → EAX = mask
GGUFChain_SelectChainMode(size_lo, size_hi)   → EAX = mode
```

### Phase 8: Telemetry
```asm
GGUFChain_Telemetry_Begin()               → (no return)
GGUFChain_Telemetry_End(method, bytes)    → (no return)
GGUFChain_Telemetry_Get(pOut)             → EAX = 1 (success)
```

### Phase 10: Engine
```asm
GGUFEngine_LoadAuto(path, lo, hi)         → EAX = handle
GGUFEngine_StreamAuto(h, dst, bytes, m)   → EAX = bytes read
GGUFEngine_Close(hModel)                  → EAX = 0
```

---

## Performance Notes

- **Phase 1**: One-time 10-100μs
- **Phase 2**: ~1-5ms (CPUID + disk test)
- **Phase 3**: <1ms (lookup table)
- **Phase 5**: <100ns (branch prediction)
- **Phase 8**: ~100ns per call (counter reads)
- **Phase 10**: Dominated by disk/memory I/O

---

## Testing Checklist

- [ ] PiFabric_Init() runs without errors
- [ ] PiFabric_ProbeDevice() fills stats correctly
- [ ] GGUFEngine_LoadAuto() returns valid handle
- [ ] GGUFEngine_StreamAuto() transfers bytes
- [ ] Telemetry accumulates correct counts
- [ ] PiFabric_AdaptTick() responds to high CPU
- [ ] Model loads <100ms for <1GB files
- [ ] Telemetry shows correct method selection
- [ ] Multiple models load in sequence
- [ ] Memory usage stays within bounds

