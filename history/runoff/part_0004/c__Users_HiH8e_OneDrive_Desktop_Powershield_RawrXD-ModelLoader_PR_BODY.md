Summary
- Runtime-dispatched Q4_0 GEMM with AVX2 path and scalar fallback.
- Batch dequantization of 64×64 tiles (16 KB scratch) + blocked AVX2 GEMM.
- Preserves portability; automatically picks AVX2 when available at runtime.

Benchmarks
- bench_q4_0_end2end (64×128 × 128×64 slice, 100 iters): Scalar 33.93 ms → AVX2 6.65 ms (5.10× speedup). Gate ≥1.8×: PASS.
- gguf_inference_cli (TinyLlama Q4_0, 50 tokens): 28.7 → 212.8 tokens/sec (~7.4× uplift). Apples-to-apples hidden × Q4_0 hot path.

Implementation
- kernels/q4_0_gemm_avx2.cc: Dispatcher + 64×64 tiling; uses `q4_0_unpack_64x64` (16 KB scratch) → `matmul_kernel_avx2`.
- kernels/q4_0_unpack_avx2.cc: Batch unpacker (Q4_0 → FP32) tuned for cache locality.
- kernels/matmul_kernel_avx2.cc: Minimal AVX2 micro-kernel for blocked GEMM (scalar fallback included).
- tests/bench_q4_0_end2end.cpp: End-to-end micro-bench harness and gate (≥1.8×).
- tests/gguf_inference_cli.cpp: Simple CLI to measure tok/s on the exact hot path.

Footprint
- Binary delta ~+2 KB; total remains < 400 KB.
- AVX2-only code is guarded; scalar path unchanged on CPUs without AVX2.

Notes
- Dequantization is amortized by unpacking full 64×64 tiles once and reusing during GEMM, which avoids per-nibble overhead.
- The blocked 64×64 layout fits L1 and leverages FMA for high throughput.

Next Steps (follow-up PR)
- Wire the optimized Q4_0 GEMM into the real forward-pass call-site in GGUFRunner (GGML_TYPE_Q4_0 branch) with runtime dispatch.
- Additional optimizations: Q8_0 kernels, KV-cache improvements, flash-attention, and ARM NEON path.
