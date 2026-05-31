# Phase D1: FlashAttention Integration Plan

## 🎯 Objective

Integrate FlashAttention kernel into RawrXD inference pipeline as the **foundation layer** for Phase D, maintaining parity with validation harness and establishing baseline performance metrics.

---

## 📋 Current State Analysis

### Existing FlashAttention Implementation

**Location**: `src/core/flash_attention.h` + `src/core/flash_attention.cpp`

**ASM Kernel**: `src/asm/FlashAttention_AVX512.asm`

**Current Features**:
- ✅ AVX-512F + BW + VL support detection
- ✅ 64-byte alignment validation for ZMM registers
- ✅ License gating (FEATURE_FLASH_ATTENTION = 0x40, Pro tier)
- ✅ Tile configuration (64x64x128 default)
- ✅ GQA (Grouped Query Attention) support
- ✅ Causal masking for autoregressive models
- ✅ Performance counters (calls, tiles)

**Integration Points**:
```cpp
// Current dispatch path (not yet wired into inference pipeline)
FlashAttentionEngine engine;
if (!engine.Initialize()) { /* fallback */ }
engine.Forward(cfg);
```

---

## 🔧 Integration Strategy

### Step 1: Wire into Inference Pipeline

**Target File**: `src/core/inference_handlers.cpp`

**Current Path**:
```cpp
// Existing attention implementation (standard matmul path)
void ComputeAttention(float* Q, float* K, float* V, float* O, /* ... */)
```

**Integration Point**:
```cpp
// Add FlashAttention dispatch path
void ComputeAttention(float* Q, float* K, float* V, float* O, /* ... */) {
    // Check if FlashAttention is available and licensed
    static FlashAttentionEngine* s_flashEngine = nullptr;
    
    if (!s_flashEngine) {
        s_flashEngine = new FlashAttentionEngine();
        if (!s_flashEngine->Initialize()) {
            delete s_flashEngine;
            s_flashEngine = nullptr;
        }
    }
    
    // Dispatch to FlashAttention if available
    if (s_flashEngine && s_flashEngine->IsReady()) {
        FlashAttentionConfig cfg;
        cfg.Q = Q;
        cfg.K = K;
        cfg.V = V;
        cfg.O = O;
        cfg.seqLenM = seqLenM;
        cfg.seqLenN = seqLenN;
        cfg.headDim = headDim;
        cfg.numHeads = numHeads;
        cfg.numKVHeads = numKVHeads;
        cfg.batchSize = batchSize;
        cfg.causal = causal;
        cfg.ComputeScale();
        
        int32_t result = s_flashEngine->Forward(cfg);
        if (result == 0) {
            return; // Success
        }
        // Fall through to standard path on error
    }
    
    // Standard attention fallback
    StandardAttention(Q, K, V, O, /* ... */);
}
```

---

### Step 2: Autotuner Integration

**Target File**: `src/core/gpu_kernel_autotuner.cpp`

**Add FlashAttention Tuning**:
```cpp
// Add to GPUKernelAutotuner::TuneFlashAttention()
void GPUKernelAutotuner::TuneFlashAttention(
    int32_t seqLenM, int32_t seqLenN, int32_t headDim,
    int32_t numHeads, int32_t numKVHeads, int32_t batchSize)
{
    // Get current tile configuration
    FlashAttentionTileConfig tileCfg;
    FlashAttention_GetTileConfig(&tileCfg);
    
    // Log current configuration
    std::cout << "[Autotuner] FlashAttention Tile Config:\n"
              << "  Tile M: " << tileCfg.tileM << "\n"
              << "  Tile N: " << tileCfg.tileN << "\n"
              << "  Head Dim: " << tileCfg.headDim << "\n"
              << "  Scratch: " << tileCfg.scratchBytes << " bytes\n";
    
    // Future: Add dynamic tile size tuning based on:
    // - Sequence length (seqLenM, seqLenN)
    // - Head dimension (headDim)
    // - Available L1/L2 cache
    // - Memory bandwidth
    
    // For now, use default 64x64x128 tiles
    // Future phases will add:
    // - 128x64x128 for long sequences
    // - 32x32x64 for short sequences
    // - Adaptive tile selection based on model architecture
}
```

---

### Step 3: Validation Harness Integration

**Target File**: `scripts/run_parity_gpu_validation.ps1`

**Add FlashAttention Test**:
```powershell
# Add to validation harness
function Test-FlashAttention {
    Write-Host "[FlashAttention] Testing AVX-512 kernel..." -ForegroundColor Cyan
    
    # Test 1: License gate
    $licenseTest = Test-LicenseFeature -Feature "FlashAttention"
    if (-not $licenseTest) {
        Write-Host "[FlashAttention] License gate: PASS (Pro tier required)" -ForegroundColor Yellow
        return $true  # Skip if not licensed
    }
    
    # Test 2: AVX-512 capability
    $avxTest = Test-AVX512Capability
    if (-not $avxTest) {
        Write-Host "[FlashAttention] AVX-512: NOT AVAILABLE (fallback to standard)" -ForegroundColor Yellow
        return $true  # Fallback is acceptable
    }
    
    # Test 3: Alignment validation
    $alignTest = Test-PointerAlignment -Alignment 64
    if (-not $alignTest) {
        Write-Host "[FlashAttention] Alignment: FAIL" -ForegroundColor Red
        return $false
    }
    
    # Test 4: Forward pass correctness
    $forwardTest = Test-FlashAttentionForward -SeqLenM 128 -SeqLenN 128 -HeadDim 64 -NumHeads 32
    if (-not $forwardTest) {
        Write-Host "[FlashAttention] Forward pass: FAIL" -ForegroundColor Red
        return $false
    }
    
    Write-Host "[FlashAttention] All tests: PASS" -ForegroundColor Green
    return $true
}

# Add to main validation loop
$tests += @{
    Name = "FlashAttention"
    Test = { Test-FlashAttention }
}
```

---

### Step 4: Performance Benchmarking

**Target File**: `scripts/benchmark_flashattention.ps1` (NEW)

**Create Benchmark Script**:
```powershell
#!/usr/bin/env pwsh
# ============================================================================
# benchmark_flashattention.ps1 — FlashAttention Performance Benchmark
# ============================================================================

param(
    [int]$SeqLenM = 512,
    [int]$SeqLenN = 512,
    [int]$HeadDim = 64,
    [int]$NumHeads = 32,
    [int]$NumKVHeads = 8,  # GQA
    [int]$BatchSize = 1,
    [int]$Warmup = 5,
    [int]$Iterations = 100
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "FlashAttention Performance Benchmark" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Configuration
Write-Host "Configuration:" -ForegroundColor Yellow
Write-Host "  SeqLenM:     $SeqLenM"
Write-Host "  SeqLenN:     $SeqLenN"
Write-Host "  HeadDim:     $HeadDim"
Write-Host "  NumHeads:    $NumHeads"
Write-Host "  NumKVHeads:  $NumKVHeads"
Write-Host "  BatchSize:   $BatchSize"
Write-Host "  Warmup:      $Warmup"
Write-Host "  Iterations:  $Iterations"
Write-Host ""

# Run benchmark
$exePath = "D:\rawrxd\build\Release\rawrxd-cli.exe"
$args = @(
    "benchmark-flashattention",
    "--seq-len-m", $SeqLenM,
    "--seq-len-n", $SeqLenN,
    "--head-dim", $HeadDim,
    "--num-heads", $NumHeads,
    "--num-kv-heads", $NumKVHeads,
    "--batch-size", $BatchSize,
    "--warmup", $Warmup,
    "--iterations", $Iterations
)

Write-Host "Running benchmark..." -ForegroundColor Green
& $exePath $args

Write-Host ""
Write-Host "Benchmark complete." -ForegroundColor Green
```

---

## 📊 Performance Metrics to Capture

### Baseline Metrics (Before Integration)

**Standard Attention Path**:
- Tokens/sec: [TBD]
- Latency (ms): [TBD]
- VRAM usage: [TBD]
- Memory bandwidth (GB/s): [TBD]

### Target Metrics (After Integration)

**FlashAttention Path**:
- Tokens/sec: ≥10-25% improvement
- Latency (ms): ≤10% reduction
- VRAM usage: No fragmentation spikes
- Memory bandwidth (GB/s): ≥15% improvement

### Validation Gates

**Gate 1: No Regression**
- ✅ All 17-check validation passing
- ✅ No new test failures
- ✅ No memory leaks

**Gate 2: Performance Improvement**
- ✅ ≥10% attention speedup (tokens/sec)
- ✅ ≤10% latency reduction
- ✅ Stable VRAM usage

**Gate 3: Stability**
- ✅ No fragmentation spikes
- ✅ Consistent performance across runs
- ✅ Graceful fallback on error

---

## 🚀 Implementation Steps

### Step 1: Create Integration Branch
```bash
git checkout -b feature/phaseD-flashattention-integration
```

### Step 2: Wire FlashAttention into Inference Pipeline
- Modify `src/core/inference_handlers.cpp`
- Add FlashAttention dispatch path
- Implement fallback to standard attention

### Step 3: Add Autotuner Support
- Modify `src/core/gpu_kernel_autotuner.cpp`
- Add `TuneFlashAttention()` method
- Log tile configuration

### Step 4: Update Validation Harness
- Modify `scripts/run_parity_gpu_validation.ps1`
- Add FlashAttention test suite
- Integrate into 17-check validation

### Step 5: Create Benchmark Script
- Create `scripts/benchmark_flashattention.ps1`
- Add CLI benchmark command
- Capture baseline metrics

### Step 6: Run Validation + Benchmark
```powershell
# Run full validation
.\scripts\run_parity_gpu_validation.ps1

# Run FlashAttention benchmark
.\scripts\benchmark_flashattention.ps1 -SeqLenM 512 -SeqLenN 512 -HeadDim 64 -NumHeads 32
```

### Step 7: Capture Metrics + Commit
```powershell
# Capture baseline metrics
.\scripts\benchmark_flashattention.ps1 > baseline_metrics.txt

# Commit integration
git add -A
git commit -m "feat: Integrate FlashAttention kernel into inference pipeline

- Wire FlashAttention into ComputeAttention dispatch path
- Add autotuner support for tile configuration
- Integrate into 17-check validation harness
- Add performance benchmark script
- Target: ≥10-25% attention speedup, stable VRAM usage

Phase D1: Foundation layer for kernel integration"
```

---

## 🎯 Success Criteria

### Must Pass
- ✅ All 17-check validation passing
- ✅ No regression in existing tests
- ✅ Graceful fallback on error
- ✅ License gating working correctly

### Should Pass
- ✅ ≥10% attention speedup (tokens/sec)
- ✅ ≤10% latency reduction
- ✅ Stable VRAM usage (no fragmentation)

### Nice to Have
- ✅ ≥25% attention speedup
- ✅ ≥15% memory bandwidth improvement
- ✅ Adaptive tile size selection

---

## 📝 Notes

### Integration Philosophy
- **Isolated Integration**: Only FlashAttention, no other changes
- **Deterministic Regression Tracking**: Clear attribution of improvements
- **Kernel-Level Performance Clarity**: Baseline metrics before/after

### Fallback Strategy
- If FlashAttention fails (license, AVX-512, alignment), fall back to standard attention
- No user-visible impact on error
- Log all failures for diagnostics

### Future Enhancements (Phase D2+)
- Dynamic tile size tuning based on sequence length
- Multi-query attention (MQA) optimization
- Sliding window attention support
- FlashAttention-2 integration (if available)

---

## 🔗 Related Files

### Core Implementation
- `src/core/flash_attention.h` — C++ bridge header
- `src/core/flash_attention.cpp` — C++ wrapper implementation
- `src/asm/FlashAttention_AVX512.asm` — AVX-512 kernel

### Integration Points
- `src/core/inference_handlers.cpp` — Dispatch path
- `src/core/gpu_kernel_autotuner.cpp` — Autotuner support
- `scripts/run_parity_gpu_validation.ps1` — Validation harness
- `scripts/benchmark_flashattention.ps1` — Performance benchmark

### Documentation
- `docs/phaseD1_flashattention_integration_plan.md` — This document
- `PHASE_2C_TO_2D_COMPLETION_SUMMARY.md` — Phase completion summary

---

## 📅 Timeline

**Estimated Duration**: 1-2 days

**Day 1**:
- Wire FlashAttention into inference pipeline
- Add autotuner support
- Update validation harness

**Day 2**:
- Create benchmark script
- Run validation + benchmark
- Capture metrics + commit

---

## ✅ Ready to Proceed

Phase D1 is ready to begin. The integration points are clear, the strategy is isolated, and the success criteria are defined. This will establish the foundation layer for the progressive kernel convergence pipeline.

**Next Action**: Begin Step 2 (Wire FlashAttention into Inference Pipeline)