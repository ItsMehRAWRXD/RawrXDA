# v0.3.0 Sprint: AVX2 + Q4_0 Kernels

**Target**: <400 KB binary, ≥1.8× scalar tok/s, zero regression in GQA+RoPE  
**Baseline**: v0.2.0 (386 KB, ~200 tok/s scalar)  
**Branch**: `avx2-q4-v0.3.0`

---

## Sprint Checklist

### Phase 1: AVX2 GEMM (Commit 1)
- [ ] Implement `matmul_avx2()` in `GGUFRunner.cpp` using `_mm256_*` intrinsics
- [ ] Add runtime CPU detection for AVX2 (use existing `context_.hasAVX2`)
- [ ] Dispatch: `if (hasAVX2) matmul_avx2() else matmul()`
- [ ] Validate with Python reference (same logits as scalar)
- [ ] Binary size check: expect +5-8 KB (intrinsics code)
- **Acceptance**: Binary ≤395 KB, bit-exact match with scalar

### Phase 2: Q4_0 Quantized Dot-Product (Commit 2)
- [ ] Add `dotProductQ4_0()` for attention scores (Q·K^T)
- [ ] Dequantize Q4_0 blocks on-the-fly during dot-product
- [ ] Use existing `dequantizeRowQ4_0_scalar()` as reference
- [ ] Benchmark: compare Q4_0 vs F32 attention throughput
- **Acceptance**: ≥1.5× faster than scalar Q4_0, <2% accuracy loss

### Phase 3: Q8_0 Activation Cache (Commit 3)
- [ ] Quantize activations to Q8_0 before storing in KV-cache
- [ ] Reduces KV-cache memory by 4× (F32 → Q8_0)
- [ ] Modify `attentionForward()` to quantize K/V on write
- [ ] Dequantize on read during attention computation
- **Acceptance**: KV-cache 4 KB/layer (was 16 KB), same quality

### Phase 4: Benchmark Integration (Commit 4)
- [ ] Extend `scripts/bench_gqa_rope.ps1` to measure AVX2 vs scalar
- [ ] Add `--avx2` flag to testbench for explicit path selection
- [ ] Report: tok/s, binary size, KV-cache bytes, canary validation
- **Acceptance**: Automated CI comparison (AVX2 must be ≥1.8× scalar)

### Phase 5: CI Validation Gate (Commit 5)
- [ ] Add AVX2 build to `.github/workflows/release.yml`
- [ ] Run both scalar and AVX2 benchmarks in CI
- [ ] Assert: AVX2 tok/s ≥ 1.8× scalar tok/s
- [ ] Assert: Binary size <400 KB
- [ ] Assert: Canary value matches (7.891234)
- **Acceptance**: CI passes, both artifacts signed

---

## Performance Targets

| Metric | v0.2.0 (Scalar) | v0.3.0 (AVX2) Target | Status |
|--------|-----------------|----------------------|--------|
| Binary Size | 386 KB | <400 KB (+<5%) | ☐ |
| Throughput (128 tok) | ~200 tok/s | ≥360 tok/s (1.8×) | ☐ |
| KV-Cache (per layer) | 16 KB (F32) | 4 KB (Q8_0, 4×) | ☐ |
| Canary Validation | 7.891234 | 7.891234 (exact) | ☐ |

---

## Build Commands

```powershell
# Scalar baseline (v0.2.0 validation)
cmake --preset win-rel-scalar
cmake --build build-scalar --config Release
.\build-scalar\bin\Release\RawrXD-QtShell.exe --gen 128

# AVX2 optimized (v0.3.0)
cmake --preset win-rel-avx2
cmake --build build-avx2 --config Release
.\build-avx2\bin\Release\RawrXD-QtShell.exe --gen 128 --avx2

# Benchmark comparison
.\scripts\bench_gqa_rope.ps1 -Scalar .\build-scalar\bin\Release\RawrXD-QtShell.exe `
                             -Avx2 .\build-avx2\bin\Release\RawrXD-QtShell.exe
```

---

## Commit Strategy

Each phase = 1 commit with atomic changes:

1. **Commit 1**: `feat(avx2): add AVX2 matmul kernel with runtime dispatch`
2. **Commit 2**: `feat(q4): quantized dot-product for attention scores`
3. **Commit 3**: `feat(q8): Q8_0 activation cache (4× memory reduction)`
4. **Commit 4**: `chore(bench): integrate AVX2 vs scalar benchmarks`
5. **Commit 5**: `ci: add AVX2 validation gate (1.8× throughput assert)`

Each commit:
- ✅ Builds clean (no warnings)
- ✅ Passes Python reference validation
- ✅ Binary size ≤400 KB
- ✅ Canary value 7.891234

---

## Notes

- **Keep GQA+RoPE untouched**: All optimizations apply *after* attention computation
- **Scalar fallback**: If AVX2 unavailable at runtime, use existing scalar path
- **No new dependencies**: Only `<immintrin.h>` (already in MSVC)
- **Preserve KV-cache layout**: [nLayers×nKVHeads×maxTokens×headDim] unchanged

---

## Ready State

- ✅ v0.2.0 tagged and shipped
- ✅ Validation suite in place (Python reference, CI gate)
- ✅ Dead code removed (stub cleanup)
- ✅ Feature branch created (`avx2-q4-v0.3.0`)
- ✅ CMakePresets.json configured (`win-rel-avx2`)
- ☐ Draft PR opened
- ☐ Baseline benchmark run

**Next**: Open draft PR, run scalar baseline, implement Phase 1 (AVX2 matmul).
