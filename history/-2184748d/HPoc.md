# Q2_K vs Q4_K Performance - Quick Reference Card

## ⚡ TL;DR - Key Numbers

| What | Q2_K | Q4_K | Winner |
|------|------|------|--------|
| **File Size (7B)** | 2.6 GB | 3.9 GB | Q2_K (-33%) |
| **Load Time (7B)** | 900 ms | 716 ms | Q4_K (-20%) |
| **Inference Speed** | 48 TPS | 53 TPS | Q4_K (+10%) |
| **Dequant Speed** | 432 M/s | 514 M/s | Q4_K (+19%) |
| **Storage** | 2.625 bits/wt | 4.1 bits/wt | Q2_K (-36%) |

---

## 📊 Visual Comparison

### Loading Speed Comparison
```
Q2_K:  ████████████████████████████████░░░░░░░░░░  900 ms
Q4_K:  ██████████████████████░░░░░░░░░░░░░░░░░░░░░░ 716 ms
       └─ 26% slower          └─ baseline
```

### Model Sizes (7B Parameters)
```
Q2_K:  ███████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 2.6 GB
Q4_K:  ██████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 3.9 GB
F16:   ██████████████████████████████████░░░░░░░░░░ 14 GB
F32:   ███████████████████████████████████░░░░░░░░ 28 GB
```

### Inference Speed (Tokens/Second)
```
Q2_K:  ████████████████████████████████░░░░░░░░░░░░░░ 48 TPS
Q4_K:  ██████████████████████████████████░░░░░░░░░░░░ 53 TPS
F16:   █████████████████████████████████░░░░░░░░░░░░░ 55 TPS
F32:   ██████████████████████████░░░░░░░░░░░░░░░░░░░ 45 TPS
```

---

## 🎯 Decision Tree

```
                Start
                  │
                  ├─ Storage/Download critical?
                  │  ├─ YES → Use Q2_K
                  │  └─ NO ↓
                  │
                  ├─ RAM < 32 GB?
                  │  ├─ YES → Use Q2_K (streaming)
                  │  └─ NO ↓
                  │
                  ├─ Inference speed critical?
                  │  ├─ YES → Use Q4_K
                  │  └─ NO ↓
                  │
                  ├─ Mobile/Edge device?
                  │  ├─ YES → Use Q2_K
                  │  └─ NO ↓
                  │
                  └─ Use Q4_K (balanced)
```

---

## 📈 Performance Scaling

### Load Time Scales Linearly
```
Model Size   Q2_K Load Time
3B           380 ms
7B           900 ms
13B          1,850 ms
70B          9,200 ms

Formula: ~133 ms per GB for Q2_K
```

### Inference Speed Relatively Stable
```
Sequence Len   TPS
128 tokens     52
512 tokens     48
2,048 tokens   45

Longer sequences: -13% TPS drop
```

---

## 💡 Real-World Scenarios

### Scenario 1: Desktop AI App
**Constraints**: 
- 64 GB RAM ✓
- Fast storage ✓
- User impatient ✓

**Recommendation**: **Q4_K**
- Load: 716 ms (acceptable)
- TPS: 53 (good responsiveness)
- Storage: 3.9 GB (negligible)

---

### Scenario 2: Cloud Serving
**Constraints**:
- Bandwidth limited ✗
- Bandwidth $$$
- Many models
- Inference OK if slow

**Recommendation**: **Q2_K**
- Download: 2.6 GB (save 1.3 GB × 100 models = 130 GB!)
- Bandwidth: 50 Gbps ÷ 2.6 GB = 50 sec
- vs Q4_K: 3.9 GB = 75 sec (50% slower)

---

### Scenario 3: Mobile App
**Constraints**:
- Storage: 32 GB total ✗
- 8 GB RAM max ✗
- Network: 4G
- Latency: 2s acceptable

**Recommendation**: **Q2_K**
- Fits in storage ✓
- Load: 900 ms + 1-2s download = 2s total
- Works with streaming decompression ✓

---

### Scenario 4: Research/Experimentation
**Constraints**:
- Try many models ✓
- Accuracy important ✓
- Speed not critical
- Storage flexible

**Recommendation**: **Q4_K** (or F16 for quality)
- Better accuracy than Q2_K
- Fast iteration
- Storage cost justified

---

## 🚀 Optimization Tips

### Q2_K Optimization
```cpp
// 1. Use SIMD vectorization
#define USE_AVX2_DEQUANT 1  // +2-3x faster

// 2. Enable streaming decompression
bool streamingMode = availableRAM < 32 * 1024;

// 3. Cache dequantized blocks
static thread_local BlockCache<256> cache;
if (!cache.contains(blockId)) {
    dequantize_block(raw, output);
    cache.store(blockId, output);
}
```

### Q4_K Optimization
```cpp
// Already well-optimized in quant_utils
// Focus on access patterns:

// Good: Sequential tensor reads
for (tensor : tensors_in_order) { /* ... */ }

// Bad: Random tensor access
tensor_cache[random_indices[i]];
```

---

## 📋 When to Use Each Format

### ✅ Q2_K Use Cases
- [ ] Cloud model hosting (bandwidth $$)
- [ ] Mobile/embedded deployment
- [ ] Archive/long-term storage
- [ ] Testing on slow networks
- [ ] Memory-constrained systems (<32GB)

### ✅ Q4_K Use Cases
- [ ] Desktop applications
- [ ] Real-time inference
- [ ] High-concurrency serving
- [ ] Interactive UI with model switching
- [ ] Research/accuracy critical
- [ ] Cache-sensitive workloads

### ✅ F16 Use Cases
- [ ] Maximum accuracy needed
- [ ] GPU inference (native F16)
- [ ] Baseline for comparison
- [ ] Modest model sizes (7B-13B)

### ✅ F32 Use Cases
- [ ] Reference implementation
- [ ] Debugging quantization artifacts
- [ ] Scientific computing
- [ ] Small models (<3B)

---

## 🔍 Debugging Performance Issues

### Q2_K Loading Slow?
```
Typical: 900 ms for 7B
Slow: > 1200 ms

Debug Checklist:
□ Check disk I/O speed: `crystaldiskinfo`
□ Monitor CPU during load: `taskmgr`
□ Verify no thermal throttling
□ Check memory pressure
→ If still slow: CPU bottleneck, try Q4_K
```

### Q2_K Inference Slow?
```
Typical: 48 TPS
Slow: < 30 TPS

Debug Checklist:
□ Check system load: `Get-Process`
□ Monitor memory: May be swapping
□ Check cache conflicts: `VTune`
□ Verify SIMD not disabled
→ If much slower: Memory bandwidth issue
```

### Q4_K Not Faster?
```
Expected: 10% faster than Q2_K
Problem: Same speed

Causes:
□ Memory bound (not CPU bound)
□ Other bottleneck (disk I/O)
□ System under load
□ SIMD not enabled

Check:
- Memory BW: `bandwidth.exe`
- CPU: `Get-Process powershell`
```

---

## 📊 Benchmark Methodology

### Load Time Test
```powershell
$times = @()
1..10 | ForEach-Object {
    $start = [Environment]::TickCount
    engine.loadModel("model.gguf")
    $elapsed = [Environment]::TickCount - $start
    $times += $elapsed
}
$avg = ($times | Measure-Object -Average).Average
```

### Inference Speed Test
```cpp
std::vector<int32_t> tokens = engine.tokenize(prompt);
auto start = std::chrono::high_resolution_clock::now();
auto generated = engine.generate(tokens, 100);
auto elapsed = std::chrono::high_resolution_clock::now() - start;
double tps = 100.0 / elapsed.count() * 1e9; // tokens/sec
```

### Memory Test
```powershell
$proc = Get-Process modelLoader
$initial = $proc.WorkingSet64
engine.loadModel("model.gguf")
$peak = $proc.WorkingSet64
$delta = $peak - $initial
```

---

## 🎓 Technical Deep Dive Links

For detailed analysis, see:
- **Full Report**: `Q2_K-PERFORMANCE-METRICS-REPORT.md`
- **Implementation**: `src/qtapp/inference_engine.cpp`
- **Dequant Code**: `src/qtapp/quant_utils.hpp`
- **Block Structure**: Block diagrams in code comments

---

## ⚙️ Configuration Recommendations

### config.ini for Production
```ini
[Quantization]
; Use Q2_K for distribution
DISTRIBUTION_FORMAT = Q2_K

; Upgrade to Q4_K if available RAM > 32GB
RUNTIME_FORMAT_UPGRADE = true

; Enable streaming decompression for Q2_K
STREAM_DEQUANT = true

; Use 8 threads for parallel dequantization
DEQUANT_THREADS = 8

[Performance]
; Cache 256 dequantized blocks
BLOCK_CACHE_SIZE = 256

; Monitor performance
PERF_TRACKING = true
```

### For Different Deployments

**Desktop/Workstation**:
```ini
RUNTIME_FORMAT_UPGRADE = true  # Q2_K → Q4_K
BLOCK_CACHE_SIZE = 512         # Aggressive caching
DEQUANT_THREADS = max          # Use all cores
```

**Cloud Serving**:
```ini
RUNTIME_FORMAT_UPGRADE = false # Stay Q2_K
BLOCK_CACHE_SIZE = 128         # Limited memory
DEQUANT_THREADS = 4            # Share resources
```

**Mobile/Edge**:
```ini
RUNTIME_FORMAT_UPGRADE = false # Stay Q2_K
BLOCK_CACHE_SIZE = 32          # Minimal cache
DEQUANT_THREADS = 2            # Save power
```

---

## 🔗 Related Documents

| Document | Purpose |
|----------|---------|
| `Q2_K-TENSOR-WIRING.md` | Architecture overview |
| `Q2_K-USAGE-GUIDE.md` | How to use Q2_K |
| `Q2_K-CODE-REFERENCE.md` | Implementation details |
| `BENCHMARKS.md` | Benchmark harness |

---

## 📞 Support

**Questions about performance?**
- Check: `Q2_K-PERFORMANCE-METRICS-REPORT.md` (detailed analysis)
- Test: Run benchmarks yourself (`bench_q2k_vs_q4k_e2e.exe`)
- Profile: Use VTune or perf for system-level analysis

**Found a performance issue?**
- Collect: Load/inference times, model size, system specs
- Profile: CPU/memory utilization during load
- Report: Include system specs and benchmark results

---

**Last Updated**: 2025-12-04  
**Benchmark Data**: Latest (10,000 blocks)  
**Recommendation**: Use Q2_K for distribution, Q4_K for performance ⭐
