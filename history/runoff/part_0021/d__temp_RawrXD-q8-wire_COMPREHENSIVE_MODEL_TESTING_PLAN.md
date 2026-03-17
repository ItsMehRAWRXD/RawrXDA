# 🚀 Comprehensive Multi-Model GPU Testing Plan

**Date Started:** December 4, 2025  
**Objective:** Benchmark all available local models on GPU with standardized metrics  
**Scope:** 9 GGUF models + 40+ Ollama models (excluding cloud variants)  
**Hardware:** AMD Radeon RX 7800 XT (16GB VRAM)

---

## Available Models for Testing

### Local GGUF Files (Direct Testing)
| Model | Format | Size | Status |
|:---|:---|:---|:---|
| BigDaddyG Q5_K_M | GGUF | 45.41 GB | ✅ Ready |
| BigDaddyG Q4_K_M (NO-REFUSE) | GGUF | 36.20 GB | ✅ Ready |
| BigDaddyG F32 (from Q4) | GGUF | 36.20 GB | ✅ Ready |
| BigDaddyG UNLEASHED Q4_K_M | GGUF | 36.20 GB | ✅ Ready |
| BigDaddyG Q2_K (CHEETAH) | GGUF | 23.71 GB | ✅ Ready |
| BigDaddyG Q2_K (ULTRA) | GGUF | 23.71 GB | ✅ Ready |
| BigDaddyG Q2_K (Custom) | GGUF | 23.71 GB | ✅ Ready |
| BigDaddyG Q2_K (16GB Pruned) | GGUF | 15.81 GB | ✅ Ready |
| Codestral 22B Q4_K_S | GGUF | 11.79 GB | ✅ Ready |

**Total GGUF Models:** 9

### Ollama Registry Models (via Ollama API)
| Category | Models | Count |
|:---|:---|:---|
| BigDaddyG Variants | bg-ide-agentic, bg-test, bg40, bg40-f32, bg40-f32-from-q4, bg40-unleashed, bigdaddyg, bigdaddyg-16gb, bigdaddyg-agentic, bigdaddyg-cheetah, bigdaddyg-fast, bigdaddyg-fast-agentic, bigdaddyg-personalized, bigdaddyg-personalized-agentic, bigdaddyg-q2-ultra, bigdaddyg-q2k-ultra-productive | 16 |
| Cheetah Variants | bigdaddy-7b-cheetah, cheetah-speed-agentic, cheetah-stealth, cheetah-stealth-agentic, cheetah-stealth-agentic-5gb, cheetah-stealth-agentic-q5km-full | 6 |
| Balanced Variants | bigdaddyg-balanced-agentic, bigdaddy-balanced-agentic | 2 |
| Specialized | quantumide-architect, quantumide-feature, quantumide-performance, quantumide-security | 4 |
| Standard Open Models | llama2, llama3, llama3.2, qwen2.5, qwen3, qwen3-coder, qwen3-vl, phi, gemma3, minimax-m2, gpt-oss, deepseek-v3.1 | 12 |
| Unlocked Variants | unlocked-60M, unlocked-350M, unlocked-1B, unlocked-125M | 4 |
| Comprehensive Agentic | llama3-comprehensive-agentic | 1 |

**Total Ollama Models:** 45  
**Cloud Models (EXCLUDED):** 671b-cloud, 120b-cloud, 20b-cloud, 480b-cloud, 235b-cloud (✗ Skipped)

---

## Testing Phases

### Phase A: GGUF Direct Testing (Days 1-5)
**Objective:** Establish baseline for all 9 GGUF files

**Test Parameters:**
- Prompt: "Explain GPU computing in 256 tokens"
- Batch sizes: 1, 4, 8
- Repetitions: 3 per batch
- Metrics: TPS, latency, VRAM, stability

**Expected Results:**
- Q5_K_M: ~60-70 TPS
- Q4_K_M: ~75-85 TPS
- F32: ~45-55 TPS
- Q2_K variants: ~110-130 TPS
- Codestral Q4_K_S: ~90-110 TPS

### Phase B: Ollama Model Testing (Days 6-12)
**Objective:** Test top 20 Ollama models for compatibility & performance

**Priority Order:**
1. BigDaddyG variants (most similar to GGUF files)
2. Cheetah variants (speed optimized)
3. Standard models (llama3, qwen2.5)
4. Specialized QuantumIDE variants

**Test Parameters:**
- Prompt: Standard benchmark prompt
- Single batch: Most Ollama models are pull-based
- 2 repetitions per model
- Metrics: TPS, latency, memory

### Phase C: Stability & Concurrency (Days 13-14)
**Objective:** Test top 3 performers under load

**Test Scenarios:**
- Concurrent requests (5, 10, 15 threads)
- Long-running tests (30min sustained)
- Memory stability monitoring
- Thermal monitoring

---

## Testing Framework

### Python Test Harness
```python
# Location: gpu_multi_model_tester.py
# Features:
# - Auto-detect available models
# - Standardized prompt generation
# - Metrics collection (TPS, latency, VRAM)
# - CSV result export
# - Graph generation
# - Statistical analysis (mean, std, P95)
```

### Test Categories

#### A. Performance Metrics
- **Throughput (TPS):** tokens/second
- **Latency (ms):** time per token
- **Batch Throughput:** aggregate tokens/sec
- **VRAM Usage:** peak and sustained
- **Bandwidth:** VRAM bandwidth saturation %

#### B. Stability Metrics
- **Crash Count:** 0 target
- **Memory Leak:** VRAM drift < 50MB/hour
- **Temperature:** < 85°C sustained
- **Error Rate:** 0%

#### C. Compatibility Metrics
- **Load Time:** seconds to ready
- **Model Detection:** automatic
- **Format Support:** GGUF vs Ollama API

---

## Test Execution Schedule

| Phase | Dates | Models | Tasks | Status |
|:---|:---|:---|:---|:---|
| A | Dec 5-6 | 9 GGUF | Direct GGUF testing | ⏳ Pending |
| B | Dec 7-11 | 20 Ollama | Primary Ollama models | ⏳ Pending |
| C | Dec 12-14 | Top 3 | Concurrency & stability | ⏳ Pending |
| D | Dec 15 | All | Results consolidation | ⏳ Pending |

---

## Success Criteria

### Phase A (GGUF)
- [ ] All 9 models load successfully
- [ ] Performance within expected ranges
- [ ] Zero crashes across 27 test runs
- [ ] VRAM stability confirmed
- [ ] CSV results exported

### Phase B (Ollama)
- [ ] 20+ Ollama models tested
- [ ] Performance baseline established
- [ ] Compatibility matrix created
- [ ] Top 3 performers identified
- [ ] Results committed to GitHub

### Phase C (Stability)
- [ ] Concurrent load test: P95 < 50ms
- [ ] 30-minute sustained run: zero crashes
- [ ] Memory drift: < 50MB/hour
- [ ] Thermal stability confirmed
- [ ] Final report generated

### Phase D (Consolidation)
- [ ] Performance comparison chart created
- [ ] Executive summary written
- [ ] All results pushed to GitHub
- [ ] Roadmap updated with findings

---

## Key Metrics Dashboard

**GGUF Performance Summary (Expected)**
| Model | Expected TPS | VRAM (GB) | Variant |
|:---|:---|:---|:---|
| BigDaddyG Q5_K_M | 65 | 14 | Balanced |
| BigDaddyG Q4_K_M | 80 | 13 | Standard |
| BigDaddyG F32 | 50 | 18 | Precision |
| Codestral Q4_K_S | 100 | 12 | Speed |
| BigDaddyG Q2_K CHEETAH | 120 | 9 | Speed |
| BigDaddyG Q2_K ULTRA | 118 | 9 | Speed |
| BigDaddyG Q2_K Custom | 115 | 9 | Speed |
| BigDaddyG Q2_K 16GB | 125 | 8 | Ultra-Speed |
| BigDaddyG UNLEASHED Q4_K | 78 | 13 | Custom |

---

## Testing Tools & Infrastructure

### Tools Available
- `gpu_inference_benchmark.exe` - Baseline testing
- `simple_gpu_test.exe` - GPU detection
- Ollama CLI - Model pulling & execution
- Python benchmark harness - Multi-model coordination

### Environment Setup
```powershell
# Ensure GPU backend enabled
$env:CUDA_VISIBLE_DEVICES = "0"  # or ROCm equivalent

# Verify VRAM available
nvidia-smi  # or equivalent AMD tool
# Expected: 16GB VRAM available

# Verify Ollama running
ollama serve  # in background

# Verify models available
ollama list
```

---

## Test Script Templates

### GGUF Direct Test
```powershell
# Test: BigDaddyG-Q2_K-PRUNED-16GB.gguf
# Batches: 1, 4, 8
# Reps: 3 each
# Expected TPS: ~125
```

### Ollama Model Test
```powershell
# Test: bigdaddyg-fast
# Batches: single (Ollama API constraint)
# Reps: 2
# Expected TPS: varies
```

### Stability Test
```powershell
# Model: Top performer from Phase A & B
# Duration: 30 minutes minimum
# Threads: 5, 10, 15 concurrent
# Metrics: Memory, temp, throughput over time
```

---

## Expected Outcomes

### Best Performers
1. **Speed Leader:** BigDaddyG Q2_K 16GB (~125 TPS)
2. **Balanced Leader:** BigDaddyG Q4_K_M (~80 TPS, high quality)
3. **Precision Leader:** BigDaddyG F32 (~50 TPS, maximum quality)

### Use Case Recommendations
- **Real-time Interaction:** Q2_K variants (120+ TPS)
- **High Quality Inference:** Q4_K_M (80 TPS, good balance)
- **Maximum Precision:** F32 (50 TPS, best quality)
- **Specialized Tasks:** Codestral 22B (100 TPS)

---

## Deliverables

### By Dec 7 (Phase A Complete)
- [ ] GGUF_BENCHMARK_RESULTS.csv
- [ ] GGUF_PERFORMANCE_ANALYSIS.md
- [ ] Graphs: TPS comparison, VRAM usage, latency distribution
- [ ] Commit to GitHub

### By Dec 11 (Phase B Complete)
- [ ] OLLAMA_MODEL_COMPATIBILITY_MATRIX.csv
- [ ] OLLAMA_PERFORMANCE_RANKING.md
- [ ] Top 3 performers identified
- [ ] Commit to GitHub

### By Dec 14 (Phase C Complete)
- [ ] CONCURRENCY_TEST_RESULTS.md
- [ ] STABILITY_VALIDATION_REPORT.md
- [ ] 30-minute test logs
- [ ] Thermal data collected

### By Dec 15 (Phase D Complete)
- [ ] COMPREHENSIVE_MODEL_TESTING_REPORT.md
- [ ] Executive summary with recommendations
- [ ] All raw data and graphs
- [ ] Final GitHub push

---

## Risk Mitigation

| Risk | Mitigation |
|:---|:---|
| VRAM exhaustion | Monitor continuously, pause if > 90% |
| Thermal throttling | Stop if temp > 85°C, cool down 30 min |
| Model load failures | Skip and log, continue with next |
| Ollama API issues | Restart service, retry up to 3x |
| Concurrency crashes | Reduce thread count, log error |

---

## Quick Start Commands

```powershell
# 1. Verify environment
Write-Host "VRAM Available:"
nvidia-smi  # Check free memory

# 2. Start Ollama
ollama serve  # Terminal 1

# 3. Run GGUF tests
cd "d:\temp\RawrXD-q8-wire"
.\gpu_multi_model_tester.py --phase gguf

# 4. Run Ollama tests
.\gpu_multi_model_tester.py --phase ollama

# 5. Run stability tests
.\gpu_multi_model_tester.py --phase stability

# 6. Generate final report
.\gpu_multi_model_tester.py --report comprehensive
```

---

## Status Tracking

```
Phase A (GGUF): ⏳ NOT STARTED
Phase B (Ollama): ⏳ NOT STARTED
Phase C (Stability): ⏳ NOT STARTED
Phase D (Report): ⏳ NOT STARTED

Overall: ⏳ Ready to begin
```

---

**Created:** December 4, 2025  
**Repository:** github.com/ItsMehRAWRXD/rawrxd-recovery-docs  
**Next Action:** Execute Phase A on December 5  

**Phase A Kickoff:** Begin GGUF direct testing with comprehensive metrics collection
