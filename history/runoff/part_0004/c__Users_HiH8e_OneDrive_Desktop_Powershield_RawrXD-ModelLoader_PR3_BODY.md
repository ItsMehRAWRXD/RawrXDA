Centralises the FP32 matmul in GGUFRunner to reuse the existing
matmul_kernel_avx2 (64×64 tiles, 16 KB scratch).

- Scalar path unchanged (fallback intact).
- Zero functional change for FP32 models.
- Prepares the call-site for the upcoming Q4_0 dispatcher swap.
- Diff ≤ 20 lines.

Next PR will retain raw Q4_0 weights and call ggml_gemm_q4_0(...)
where appropriate, giving ~5× tok/s on quantized models.
