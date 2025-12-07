# RawrXD Bottleneck Audit - Quick Reference

## 📊 Summary
- **Total Bottlenecks:** 28
- **CRITICAL:** 6 (blocking 10-100x improvements)
- **HIGH:** 4 (major performance impact)
- **MEDIUM:** 7 (moderate impact)
- **LOW:** 11 (minor impact)

---

## 🔴 CRITICAL Bottlenecks (Fix First)

| # | Component | Issue | Impact | Fix Time |
|---|-----------|-------|--------|----------|
| 1 | Agent Coordinator | Lock held during entire plan submission | +15-20ms latency | 4 hrs |
| 2 | Agent Coordinator | O(V+E) cycle detection per submission | +2-50ms for large DAGs | 6 hrs |
| 3 | GGUFServer | Per-request DOM JSON parsing | +5-15ms per request | 8 hrs |
| 4 | InferenceEngine | 100% redundant tensor embeddings in generate loop | +50-150ms per inference | 16 hrs |
| 5 | VulkanCompute | Synchronous fence waits (GPU stalls) | +200-500ms per inference | 24 hrs |
| 6 | GGUFLoader | No memory mapping on large files | +600ms for model load | 12 hrs |

**Combined Impact:** -60% latency, +250% throughput

---

## 🟠 HIGH Priority Bottlenecks

| # | Component | Issue | Impact | Fix Time |
|---|-----------|-------|--------|----------|
| 7 | InferenceEngine | O(n) byte tokenization without vocab cache | +2-5ms per tokenize | 8 hrs |
| 8 | GGUFServer | No HTTP pipelining, per-socket buffering | -70% concurrency | 6 hrs |
| 9 | Agent Coordinator | QJsonObject copying in status queries | +5-10ms per query | 4 hrs |
| 10 | Tests | Model reload per test (6+ seconds waste) | 90% test time overhead | 6 hrs |

**Combined Impact:** -20% latency, +50% throughput

---

## 🟡 MEDIUM Priority (Optimization)

| # | Issue | Impact |
|----|-------|--------|
| 11 | No task prioritization in ready queue | Poor scheduling fairness |
| 12 | Uncompressed JSON responses | 3-5x bandwidth overhead |
| 13 | RNG reinitialized per embedding | +2-3µs per call |
| 14 | std::find_if O(n) tensor lookup | Linear search per tensor |
| 15-20 | [Various minor inefficiencies] | ~5-10% combined |

---

## ⏱️ Performance Targets

| Metric | Current | Target | Improvement |
|--------|---------|--------|-------------|
| Single-request latency (p99) | 350ms | 150ms | -57% |
| Concurrent throughput | 4 RPS | 14 RPS | +250% |
| Memory allocations (per inference) | ~20M | ~2M | -90% |
| Model load time | 630ms | 90ms | -86% |
| GPU utilization | 5% | 50% | +900% |

---

## 🔧 Implementation Priority

### Week 1 (Critical Path)
1. **Bottleneck 1:** Lock contention - Split plan initialization outside critical section
2. **Bottleneck 2:** Cycle detection - Implement color-based DFS with single-pass O(V+E)
3. **Bottleneck 3:** JSON parsing - Integrate streaming JSON parser (nlohmann or custom)

### Week 2 (Core Optimization)
4. **Bottleneck 4:** Tensor caching - Implement KV caching for autoregressive generation
5. **Bottleneck 6:** Memory mapping - Add mmap support for tensor loading

### Week 3 (GPU Integration)
6. **Bottleneck 5:** Vulkan async - Implement callback-based async dispatch queue

### Post-Week 3 (Polish)
7. **Bottleneck 7:** Vocab caching - Cache BPE tokenizer results
8. **Bottleneck 8:** HTTP pipelining - Support Keep-Alive + pipelined requests
9. **Bottleneck 9:** JSON pooling - Reuse QJsonObject allocations
10. **Bottleneck 10:** Shared fixtures - Use singleton model loader for tests

---

## 📈 Expected Performance Curve

```
Before Fixes:
├─ Single request: 350ms
├─ Concurrent (10): 1000ms (serialized)
├─ Model load: 630ms
├─ 100-token gen: 100ms
└─ GPU util: 5%

After Tier 1 (Bottlenecks 1-3):
├─ Single request: 200ms (-43%)
├─ Concurrent (10): 250ms (-75%)
├─ Model load: 630ms (unchanged)
└─ Throughput: 4 RPS → 8 RPS

After Tier 2 (Bottlenecks 4-6):
├─ Single request: 150ms (-57% total)
├─ Concurrent (10): 180ms (-82%)
├─ Model load: 90ms (-86%)
├─ 100-token gen: 15ms (-85%)
└─ Throughput: 4 RPS → 14 RPS

After Tier 3 (Bottlenecks 7-10):
├─ Single request: 145ms (-59% total)
├─ Concurrent (10): 160ms (-84%)
├─ GPU util: 50% (+900%)
└─ Test suite: 5 seconds (from 60)
```

---

## 📋 File References

**Report Location:** `BOTTLENECK_AUDIT_REPORT.md` (full analysis)

**Key Files to Modify:**
- `src/orchestration/agent_coordinator.cpp` - Locks, cycle detection
- `src/inference_engine_stub.cpp` - Tensor caching, tokenization
- `src/qtapp/gguf_server.cpp` - JSON parsing, HTTP pipelining
- `src/gguf_loader.cpp` - Memory mapping, tensor lookup
- `include/vulkan_compute.h` - GPU header completion
- `src/vulkan_compute.cpp` - Async dispatch architecture
- `tests/test_agent_coordinator_integration.cpp` - Shared fixtures

---

## 🎯 Success Metrics

After implementing Tier 1 (Bottlenecks 1-3):
- [ ] Single request latency < 200ms
- [ ] Concurrent throughput > 8 RPS
- [ ] All existing tests pass
- [ ] No memory regressions

After Tier 2 (Bottlenecks 4-6):
- [ ] Single request latency < 150ms
- [ ] Concurrent throughput > 12 RPS
- [ ] Model loading < 100ms
- [ ] 100-token generation < 20ms

After Tier 3 (Bottlenecks 7-10):
- [ ] Full production readiness
- [ ] GPU utilization > 50%
- [ ] Test suite runtime < 10 seconds
- [ ] Sub-100ms p99 latency

---

## ⚡ Quick Wins (Implement First)

1. **Bottleneck 13 (30 min):** Move RNG initialization outside loop
   ```cpp
   // Before: std::mt19937 gen(std::random_device()()); inside loop
   // After: static std::mt19937 gen at class level
   ```

2. **Bottleneck 14 (1 hr):** Add tensor lookup map
   ```cpp
   std::unordered_map<std::string, TensorInfo> tensor_index;
   ```

3. **Bottleneck 1 (2 hrs):** Split lock scope in submitPlan
   ```cpp
   // Build plan outside lock, only critical section for insert
   ```

These three fixes: **~10% performance improvement with 3 hours of work**

---

**Last Updated:** December 5, 2025
