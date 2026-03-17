; Phase 150A: AVX-512 FMA Unrolled Microkernel
; Feature Request: SwarmV150_AVX512_FMA_Unroll

.code
SwarmV150_AVX512_FMA_Unroll proc
    ; RCX = Ptr to output accumulation array
    ; RDX = Ptr to Weight Matrix (A)
    ; R8  = Ptr to Vector (X)
    ; R9  = Loop Count (N / 128) -> Assumes pre-calculated tile depth

    ; Zero out exactly 8 full ZMM registers (8 * 16 floats = 128 parallel accumulators)
    vxorps zmm0, zmm0, zmm0
    vxorps zmm1, zmm1, zmm1
    vxorps zmm2, zmm2, zmm2
    vxorps zmm3, zmm3, zmm3
    vxorps zmm4, zmm4, zmm4
    vxorps zmm5, zmm5, zmm5
    vxorps zmm6, zmm6, zmm6
    vxorps zmm7, zmm7, zmm7

ALIGN 16
@@UnrollLoop:
    ; Hardware Prefetch: NTA (Non-Temporal Access) keeps L1 clean
    prefetchnta [rdx + 512]
    prefetchnta [r8 + 512]

    ; Load X vector into ZMM8..ZMM15
    vmovups zmm8,  [r8]
    vmovups zmm9,  [r8 + 64]
    vmovups zmm10, [r8 + 128]
    vmovups zmm11, [r8 + 192]
    vmovups zmm12, [r8 + 256]
    vmovups zmm13, [r8 + 320]
    vmovups zmm14, [r8 + 384]
    vmovups zmm15, [r8 + 448]

    ; Fused Multiply-Add (FMA): A * X + Accumulator
    vfmadd231ps zmm0, zmm8,  [rdx]
    vfmadd231ps zmm1, zmm9,  [rdx + 64]
    vfmadd231ps zmm2, zmm10, [rdx + 128]
    vfmadd231ps zmm3, zmm11, [rdx + 192]
    vfmadd231ps zmm4, zmm12, [rdx + 256]
    vfmadd231ps zmm5, zmm13, [rdx + 320]
    vfmadd231ps zmm6, zmm14, [rdx + 384]
    vfmadd231ps zmm7, zmm15, [rdx + 448]

    ; Increment pointers by 512 bytes (8 * 64)
    add rdx, 512
    add r8, 512

    dec r9
    jnz @@UnrollLoop

    ; Store accumulation back out
    vmovups [rcx],       zmm0
    vmovups [rcx + 64],  zmm1
    vmovups [rcx + 128], zmm2
    vmovups [rcx + 192], zmm3
    vmovups [rcx + 256], zmm4
    vmovups [rcx + 320], zmm5
    vmovups [rcx + 384], zmm6
    vmovups [rcx + 448], zmm7

    ; Clears Upper half of registers. Mandatory for mixed SSE/AVX state machine transitions.
    vzeroupper
    ret

SwarmV150_AVX512_FMA_Unroll endp
end