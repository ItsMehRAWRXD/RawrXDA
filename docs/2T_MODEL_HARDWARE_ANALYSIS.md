# 2T+ Parameter Model Hardware Requirements Analysis

## Executive Summary

This document analyzes the hardware requirements for running 2 Trillion+ parameter models on the RawrXD inference stack, based on the current architecture and known scaling characteristics.

## Current Hardware Baseline

| Component | Specification |
|-----------|---------------|
| **CPU** | AMD Ryzen 9 7950X3D (16 cores / 32 threads) |
| **RAM** | 64 GB DDR5 |
| **GPU** | AMD Radeon RX 7800 XT (16 GB VRAM) |
| **Storage** | 11 TB NVMe SSD |

## Parameter Size Scaling Analysis

### Memory Requirements by Quantization

| Model Size | Q4_K_M | Q5_K_M | Q6_K | Q8_0 | FP16 |
|------------|--------|--------|------|------|------|
| 7B | 4.5 GB | 5.5 GB | 6.5 GB | 7.5 GB | 14 GB |
| 13B | 8 GB | 10 GB | 12 GB | 14 GB | 26 GB |
| 34B | 20 GB | 24 GB | 28 GB | 34 GB | 68 GB |
| 70B | 40 GB | 48 GB | 56 GB | 68 GB | 140 GB |
| 120B | 68 GB | 82 GB | 96 GB | 116 GB | 240 GB |
| 180B | 102 GB | 123 GB | 144 GB | 174 GB | 360 GB |
| **2T** | **1.1 TB** | **1.4 TB** | **1.6 TB** | **1.9 TB** | **4 TB** |

### 2T Parameter Model Requirements

For a 2 Trillion parameter model:

#### Minimum Viable Configuration (Q4_K_M)
- **RAM**: 1.2 TB minimum (1.1 TB model + overhead)
- **VRAM**: 16 GB (partial offload, extremely slow)
- **Storage**: 2 TB NVMe (model + cache)
- **Expected Performance**: 0.1-0.5 tokens/second

#### Recommended Configuration (Q5_K_M)
- **RAM**: 1.5 TB minimum
- **VRAM**: 48-80 GB (for meaningful GPU acceleration)
- **Storage**: 4 TB NVMe
- **Expected Performance**: 0.5-2 tokens/second

#### Optimal Configuration (Q6_K)
- **RAM**: 2 TB
- **VRAM**: 80-160 GB (multi-GPU)
- **Storage**: 8 TB NVMe
- **Expected Performance**: 2-5 tokens/second

## Current System Limitations

### Memory Gap Analysis

| Requirement | Current | Gap |
|-------------|---------|-----|
| RAM for Q4 | 1.2 TB | 64 GB | **1.1 TB short** |
| VRAM for partial offload | 48 GB | 16 GB | **32 GB short** |
| Storage for model | 2 TB | 11 TB | **9 TB surplus** |

### Bottleneck Identification

1. **Primary**: System RAM (64 GB vs 1.2 TB needed)
2. **Secondary**: GPU VRAM (16 GB vs 48+ GB needed)
3. **Residual risk**: Storage *throughput/latency* under paged access (capacity is no longer a blocker)

## Simulation Strategy

Since we cannot run a 2T model on current hardware, we can simulate the behavior:

### 1. Memory Pressure Simulation

```cpp
// Simulate 2T model memory access patterns
void Simulate2TModelAccess() {
    // Simulate 1.1 TB memory footprint
    const size_t simulatedModelSize = 1100ULL * 1024 * 1024 * 1024;
    
    // Use sparse access patterns to simulate layer-by-layer inference
    // without actually allocating 1.1 TB
    const size_t workingSetSize = 4ULL * 1024 * 1024 * 1024; // 4 GB working set
    
    // Measure:
    // - Cache miss rates
    // - Memory bandwidth utilization
    // - Layer swap latency
}
```

### 2. Inference Latency Extrapolation

Based on known scaling from smaller models:

| Model | Params | T/s (CPU) | T/s (GPU) | Scaling Factor |
|-------|--------|-----------|-----------|----------------|
| 7B | 7B | 15 | 80 | 1x |
| 13B | 13B | 8 | 45 | 1.86x |
| 34B | 34B | 3 | 18 | 4.86x |
| 70B | 70B | 1.2 | 8 | 10x |
| **2T** | **2000B** | **~0.04** | **~0.3** | **285x** |

### 3. Architecture Stress Test

```cpp
// Test inference pipeline with simulated large model
void StressTestInferencePipeline() {
    // 1. Simulate large KV cache (would be ~200 GB for 2T model)
    // 2. Test memory-mapped model loading
    // 3. Measure layer-by-layer inference overhead
    // 4. Profile attention mechanism scaling
    // 5. Test speculative decoding with small draft model
}
```

## Hardware Upgrade Path

### Phase 1: Memory Expansion (Immediate)
- **Upgrade**: 64 GB → 256 GB DDR5
- **Cost**: ~$800-1200
- **Benefit**: Can run 70B models comfortably, test 120B

### Phase 2: Workstation Build (6 months)
- **RAM**: 512 GB - 1 TB DDR5
- **GPU**: 2x RTX 4090 (48 GB total) or AMD MI300
- **Cost**: ~$8,000-15,000
- **Benefit**: Can run 180B models, test 2T with heavy quantization

### Phase 3: Server Build (12 months)
- **RAM**: 2 TB DDR5
- **GPU**: 4x RTX 4090 or 2x H100
- **Cost**: ~$25,000-50,000
- **Benefit**: Full 2T model support at usable speeds

## Software Optimizations for Large Models

### 1. Memory-Mapped Inference
```cpp
// Already implemented in RawrXDModelLoader
// Allows partial model loading without full RAM
class StreamingGGUFLoader {
    // Maps model layers on-demand
    // Reduces RAM requirement to working set only
};
```

### 2. Layer Offloading
```cpp
// Hybrid CPU-GPU inference
// - Hot layers in VRAM
// - Cold layers in RAM
// - Coldest layers on disk (memory-mapped)
```

### 3. Speculative Decoding
```cpp
// Use small draft model for speculation
// - 7B draft model suggests tokens
// - 2T target model verifies
// - 2-3x speedup for acceptance
```

### 4. KV Cache Compression
```cpp
// Compress KV cache for long contexts
// - 8-bit quantization: 2x reduction
// - 4-bit quantization: 4x reduction
// - Critical for 2T models with large contexts
```

## Validation Plan

The 2T simulation work should stay tied to existing smoke surfaces so the conclusions are grounded in the current runtime stack.

### Completion smoke benchmark

Use the existing decode/completion-oriented micro-benchmark target as the quick sanity check that the inference surfaces are still healthy:

```powershell
cmake --build build-ninja --target RawrXD-DecodeMicroBench -j 8
```

That target is intentionally lightweight; it verifies the inference/completion graph remains buildable before heavier soak work.

### Ghost text soak path

For editor-facing validation, use the Win32 IDE with:

1. Ghost text enabled.
2. A loaded GGUF or Titan-backed model.
3. Repeated typing bursts that force timer-driven completion dispatch.

Watch for:

- provider selection logs
- fallback reasons
- Titan latency
- stale request cancellation
- any deadlock or UI starvation under burst input

Headless executable path (no HWND / no rendering dependency):

```powershell
cmake --build build-ghostsoak --target RawrXD-GhostSoak -j 8
.\build-ghostsoak\bin\RawrXD-GhostSoak.exe --iterations 200 --max-inflight 16 --timeout-ms 1500
```

Machine-readable result line:

```text
RAWRXD_GHOST_SOAK_JSON={...}
```

Current baseline sample:

```text
[GhostSoak] provider_available=no requests=200 cancels=0 stale_drops=0 fallbacks=200 avg_latency_us=17 max_latency_us=1230 max_queue_depth=2 max_recursion_depth=1 => PASS
```

### Disk-to-RAM throughput benchmark (paged 2T proxy)

Use the dedicated disk benchmark target to measure sequential, random, and overlapped-async prefetch throughput, then convert that bandwidth to estimated tokens/sec under a paging model:

```powershell
cmake --build build-ninja --target RawrXD-DiskPagedBench -j 8

# Buffered sequential + random + prefetch pipeline (depth 4)
.\build-ninja\bin\RawrXD-DiskPagedBench.exe --model D:\codestral22b.gguf `
  --window-bytes 67108864 --sequential-bytes 4294967296 --random-ops 128 `
  --prefetch-depth 4 --bytes-per-token 1181116006400

# Uncached NVMe ceiling — bypass OS page cache, prefetch only (depth 8)
.\build-ninja\bin\RawrXD-DiskPagedBench.exe --model D:\codestral22b.gguf `
  --window-bytes 67108864 --sequential-bytes 2147483648 `
  --prefetch-depth 8 --no-buffering `
  --skip-sequential --skip-random --bytes-per-token 1181116006400
```

Machine-readable result line:

```text
RAWRXD_DISK_PAGED_JSON={...}
```

Current baseline samples on `D:\codestral22b.gguf`:

```text
Short sanity profile (1 GB sequential + 32 random windows):
[DiskPagedBench] file_gb=11.7909 seq_gbps=1.6002 rnd_gbps=1.3274 est_seq_tps=0.0015 est_rnd_tps=0.0012 wall_ms=1382.8431
RAWRXD_DISK_PAGED_JSON={"success":true,"model_path":"D:\\codestral22b.gguf",...}

Medium sustained profile (4 GB sequential + 128 random windows):
[DiskPagedBench] file_gb=11.7909 seq_gbps=1.1851 rnd_gbps=1.1225 est_seq_tps=0.0011 est_rnd_tps=0.0010 wall_ms=10510.7711
RAWRXD_DISK_PAGED_JSON={..., "sequential_gbps":1.185064,"random_gbps":1.122533,...}

Prefetch pipeline profile — depth=4, buffered (2 GB seq + 32 rnd + 2 GB prefetch):
[DiskPagedBench] file_gb=11.7909 seq_gbps=1.4157 rnd_gbps=1.4891 prefetch_gbps=3.9758 (depth=4) est_prefetch_tps=0.0036 wall_ms=3422.1442
RAWRXD_DISK_PAGED_JSON={..., "prefetch_depth":4,"no_buffering":false,"prefetch_gbps":3.975810,"estimated_prefetch_tps":0.003614,...}

Uncached NVMe ceiling — depth=8, FILE_FLAG_NO_BUFFERING (2 GB prefetch only):
[DiskPagedBench] file_gb=11.7909 prefetch_gbps=2.5013 (depth=8,nobuf) est_prefetch_tps=0.0023 wall_ms=1457.0868
RAWRXD_DISK_PAGED_JSON={..., "prefetch_depth":8,"no_buffering":true,"prefetch_gbps":2.501285,"estimated_prefetch_tps":0.002274,...}
```

### I/O bandwidth comparison matrix

| Mode | GB/s | vs single-thread | Notes |
|------|------|-----------------|-------|
| Sequential buffered (single-thread) | 1.19 – 1.42 | 1× baseline | OS cache + sequential prefetch |
| Random buffered (single-thread) | 1.12 – 1.49 | ~1× | Near-sequential due to large 64 MB windows |
| Overlapped async, depth=4, buffered | **3.98** | **2.8×** | Pipeline hides read latency; OS cache helps |
| Overlapped async, depth=8, no-buffering | 2.50 | 1.76× | True NVMe queue depth; bypasses page cache |

Interpretation:

- `sequential_gbps`: upper-bound streaming layer fetch bandwidth (single-threaded)
- `random_gbps`: paging-churn bandwidth proxy for non-contiguous layer access
- `prefetch_gbps`: overlapped pipeline bandwidth — the practical ceiling achievable by an inference engine that issues multiple layer-fetch requests concurrently
- longer profiles are more representative: the 4 GB/128-op run settled below the short sanity pass
- the **2.8× prefetch uplift** (1.42 → 3.98 GB/s) demonstrates that the single-threaded sync read path, not the NVMe hardware, was the bottleneck; an inference engine implementing async layer pre-fetching can materially reduce the per-token paging penalty
- the **no-buffering ceiling at 2.50 GB/s** is lower than the buffered prefetch 3.98 GB/s because the OS page cache is covering repeat-reads in the buffered case; for a true cold-inference scenario (first token, model not cached) the uncached figure is more realistic
- use this output as a baseline before adding RAM cache tiers or smarter layer pinning

### 2T simulation checkpoints

For a 2T-capable extrapolation model, validate these in order:

1. memory-pressure behavior under synthetic large working sets
2. completion latency under repeated timer-triggered dispatch
3. fallback behavior when Titan/native inference is unavailable
4. queue cancellation when typing outpaces inference
5. model-load state transitions across GGUF, bridge, and deferred startup paths

### Existing runtime harnesses worth reusing

- [`docs/ANALYSIS_AND_AUDIT_QUICKSTART.md`](ANALYSIS_AND_AUDIT_QUICKSTART.md) for the current smoke-entry guidance
- `RawrXD-TpsSmoke` for throughput-style inference validation
- `tests/benchmark_completions.cpp` for a quick build/surface sanity check

## Next Runtime Questions

If the simulation work progresses, the next questions are not "can we allocate 2T parameters" but:

- how much latency budget is lost to paging and cache churn
- whether completion cancellation stays newest-request-wins under burst typing
- whether the provider cascade still converges to Titan/native inference when the model is present
- whether the UI thread remains responsive while the worker thread is busy

## Conclusion

**Current hardware cannot run 2T models in-memory.** The gap is approximately:

- **18x more RAM needed** (64 GB → 1.2 TB)
- **3x more VRAM needed** (16 GB → 48+ GB)
- **Storage capacity is sufficient** (11 TB available), but paging throughput still constrains practical token rate

**Simulation approach:**
1. Use memory-mapped loading with synthetic model files
2. Profile inference pipeline with simulated layer sizes
3. Test KV cache management with large contexts
4. Validate speculative decoding with draft models

**Recommended next steps:**
1. Implement layer-by-layer profiling
2. Add memory pressure simulation mode
3. Test with largest available models (70B-180B)
4. Document scaling characteristics for extrapolation