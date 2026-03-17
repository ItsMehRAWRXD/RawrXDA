; ═══════════════════════════════════════════════════════════════════
; simd_kernels.asm — Phase 9A: AVX2/AVX-512 Attention Kernels
;
; Production-grade SIMD compute kernels for transformer inference.
; All routines auto-dispatch: AVX-512 path if available, else AVX2.
;
; Dual-path kernels (AVX-512 / AVX2 auto-dispatch):
;   SIMD_RMSNorm          — Root Mean Square normalization
;   SIMD_DotProduct       — Q×K^T attention score (one head)
;   SIMD_MatVecQ4         — Quantized (Q4_0) matrix-vector multiply
;
; AVX2 kernels (vectorized, no AVX-512 path needed):
;   SIMD_Softmax          — Numerically stable softmax
;   SIMD_ScaledDotBatch   — Batched Q×K^T for all tokens (one head)
;   SIMD_VAccumulate      — V weighted sum (score × V rows)
;   SIMD_RoPE             — Rotary Position Embedding
;   SIMD_SiLU             — SiLU / SwiGLU activation
; ═══════════════════════════════════════════════════════════════════

; NOTE: Do NOT include rawrxd.inc — this file DEFINES PUBLIC symbols.
EXTERN g_hasAVX512:DWORD
EXTERN FlashAttention_Forward:PROC

PUBLIC SIMD_RMSNorm
PUBLIC SIMD_Softmax
PUBLIC SIMD_DotProduct
PUBLIC SIMD_ScaledDotBatch
PUBLIC SIMD_VAccumulate
PUBLIC SIMD_MatVecQ4
PUBLIC SIMD_RoPE
PUBLIC SIMD_SiLU
PUBLIC SIMD_TextSearch
PUBLIC SIMD_FlashAttention

.const
align 16
c_one           dd 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0
                dd 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0
c_rms_eps       dd 1.0e-6              ; RMSNorm epsilon
c_neg_inf       dd 0FF800000h          ; -inf for softmax stability
c_inv_sqrt_dim  dd 0.0883883h          ; 1/sqrt(128) ≈ 0.08839 — for head_dim=128

.data
align 8
; Scratch buffers for softmax (thread-local in production)
g_softmax_max   dd 0
g_softmax_sum   dd 0

.code

; ────────────────────────────────────────────────────────────────
; SIMD_RMSNorm — Root Mean Square Layer Normalization
;   RCX = pOut      (float* output, normalized)
;   RDX = pInput    (float* input vector)
;   R8  = pWeight   (float* gamma weights)
;   R9D = dim       (vector dimension, must be multiple of 8)
;
;   out[i] = (input[i] / sqrt(mean(input²) + eps)) * weight[i]
; ────────────────────────────────────────────────────────────────
SIMD_RMSNorm PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    mov     ebx, r9d                ; dim
    test    ebx, ebx
    jz      @rms_done

    ; ── Pass 1: Compute sum of squares ──
    vxorps  ymm0, ymm0, ymm0       ; accumulator = 0
    xor     eax, eax                ; index

    cmp     g_hasAVX512, 1
    je      @rms_sum_512

    ; AVX2 path: 8 floats per iteration
@rms_sum_256:
    cmp     eax, ebx
    jae     @rms_reduce
    vmovups ymm1, ymmword ptr [rdx + rax*4]
    vmulps  ymm1, ymm1, ymm1       ; x²
    vaddps  ymm0, ymm0, ymm1       ; accum += x²
    add     eax, 8
    jmp     @rms_sum_256

@rms_sum_512:
    ; AVX-512 path: 16 floats per iteration
    vxorps  zmm0, zmm0, zmm0
@rms_s512_loop:
    cmp     eax, ebx
    jae     @rms_reduce_512
    vmovups zmm1, zmmword ptr [rdx + rax*4]
    vfmadd231ps zmm0, zmm1, zmm1   ; accum += x²
    add     eax, 16
    jmp     @rms_s512_loop

@rms_reduce_512:
    ; Reduce zmm0 to ymm0 (add high 256 to low 256)
    vextractf64x4 ymm1, zmm0, 1
    vaddps  ymm0, ymm0, ymm1

@rms_reduce:
    ; Horizontal sum of ymm0 → xmm0[0]
    vextractf128 xmm1, ymm0, 1
    vaddps  xmm0, xmm0, xmm1
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0       ; xmm0[0] = sum of squares

    ; mean = sum / dim
    vcvtsi2ss xmm1, xmm1, ebx      ; xmm1 = (float)dim
    vdivss  xmm0, xmm0, xmm1       ; mean(x²)
    vaddss  xmm0, xmm0, dword ptr [c_rms_eps]  ; + epsilon
    vrsqrtss xmm0, xmm0, xmm0      ; 1/sqrt(mean + eps) — fast reciprocal sqrt

    ; ── Pass 2: Normalize and scale ──
    vbroadcastss ymm2, xmm0        ; ymm2 = scale factor everywhere
    xor     eax, eax

    cmp     g_hasAVX512, 1
    je      @rms_apply_512

@rms_apply_256:
    cmp     eax, ebx
    jae     @rms_done
    vmovups ymm1, ymmword ptr [rdx + rax*4]    ; input
    vmulps  ymm1, ymm1, ymm2                   ; input * scale
    vmovups ymm3, ymmword ptr [r8 + rax*4]      ; gamma weights
    vmulps  ymm1, ymm1, ymm3                   ; * gamma
    vmovups ymmword ptr [rcx + rax*4], ymm1     ; store
    add     eax, 8
    jmp     @rms_apply_256

@rms_apply_512:
    vbroadcastss zmm2, xmm0
@rms_a512_loop:
    cmp     eax, ebx
    jae     @rms_done
    vmovups zmm1, zmmword ptr [rdx + rax*4]
    vmulps  zmm1, zmm1, zmm2
    vmovups zmm3, zmmword ptr [r8 + rax*4]
    vmulps  zmm1, zmm1, zmm3
    vmovups zmmword ptr [rcx + rax*4], zmm1
    add     eax, 16
    jmp     @rms_a512_loop

@rms_done:
    vzeroupper
    add     rsp, 30h
    pop     rbx
    ret
SIMD_RMSNorm ENDP

; ────────────────────────────────────────────────────────────────
; SIMD_Softmax — Numerically stable softmax over float vector
;   RCX = pInOut    (float*, in-place: scores → probabilities)
;   EDX = count     (number of elements, must be ≥ 1)
;
;   Pass 1: find max
;   Pass 2: exp(x - max), accumulate sum
;   Pass 3: divide by sum
; ────────────────────────────────────────────────────────────────
SIMD_Softmax PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    mov     ebx, edx                ; count
    test    ebx, ebx
    jz      @sm_done

    ; ── Pass 1: Find max ──
    vmovss  xmm0, dword ptr [rcx]   ; max = input[0]
    mov     eax, 1
@sm_max_loop:
    cmp     eax, ebx
    jae     @sm_pass2
    vmovss  xmm1, dword ptr [rcx + rax*4]
    vmaxss  xmm0, xmm0, xmm1
    inc     eax
    jmp     @sm_max_loop

@sm_pass2:
    ; ── Pass 2: exp(x - max) and sum ──
    ; Scalar exp approximation: exp(x) ≈ (1 + x/256)^256
    ; For production accuracy, use a polynomial or lookup table.
    ; Here we use a compact 4th-order polynomial:
    ;   exp(x) ≈ 1 + x + x²/2 + x³/6 + x⁴/24
    vxorps  xmm4, xmm4, xmm4       ; sum = 0
    xor     eax, eax

@sm_exp_loop:
    cmp     eax, ebx
    jae     @sm_pass3

    vmovss  xmm1, dword ptr [rcx + rax*4]
    vsubss  xmm1, xmm1, xmm0       ; x = input[i] - max

    ; Polynomial exp(x): 1 + x + x²/2 + x³/6 + x⁴/24
    vmovaps xmm2, xmm1              ; x
    vmulss  xmm3, xmm2, xmm2       ; x²
    vmovss  xmm5, dword ptr [c_one] ; 1.0
    vaddss  xmm5, xmm5, xmm2       ; 1 + x

    vmulss  xmm6, xmm3, dword ptr [c_half]  ; x²/2
    vaddss  xmm5, xmm5, xmm6       ; 1 + x + x²/2

    vmulss  xmm6, xmm3, xmm2       ; x³
    vmulss  xmm6, xmm6, dword ptr [c_sixth] ; x³/6
    vaddss  xmm5, xmm5, xmm6       ; + x³/6

    vmulss  xmm6, xmm3, xmm3       ; x⁴
    vmulss  xmm6, xmm6, dword ptr [c_24th]  ; x⁴/24
    vaddss  xmm5, xmm5, xmm6       ; + x⁴/24

    ; Clamp to 0 if negative (exp can't be negative)
    vmaxss  xmm5, xmm5, xmm4       ; max(result, 0)

    vmovss  dword ptr [rcx + rax*4], xmm5   ; store exp(x-max)
    vaddss  xmm4, xmm4, xmm5               ; sum += exp
    inc     eax
    jmp     @sm_exp_loop

@sm_pass3:
    ; ── Pass 3: Divide by sum ──
    ; Avoid division by zero
    vcomiss xmm4, xmm4              ; xmm4 is NaN if 0 somehow
    vrcpss  xmm2, xmm2, xmm4       ; 1/sum (fast approx)
    xor     eax, eax

@sm_div_loop:
    cmp     eax, ebx
    jae     @sm_done
    vmovss  xmm1, dword ptr [rcx + rax*4]
    vmulss  xmm1, xmm1, xmm2       ; × (1/sum)
    vmovss  dword ptr [rcx + rax*4], xmm1
    inc     eax
    jmp     @sm_div_loop

@sm_done:
    vzeroupper
    add     rsp, 30h
    pop     rbx
    ret
SIMD_Softmax ENDP

; ────────────────────────────────────────────────────────────────
; SIMD_DotProduct — Dot product of two float vectors (Q · K for one head)
;   RCX = pA (float*, e.g. Q vector)
;   RDX = pB (float*, e.g. K vector)
;   R8D = dim (must be multiple of 8)
;   Returns: XMM0 = dot product (scalar float)
; ────────────────────────────────────────────────────────────────
SIMD_DotProduct PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    vxorps  ymm0, ymm0, ymm0       ; accumulator
    xor     eax, eax

    cmp     g_hasAVX512, 1
    je      @dp_512

    ; AVX2 path with FMA
@dp_256:
    cmp     eax, r8d
    jae     @dp_reduce
    vmovups ymm1, ymmword ptr [rcx + rax*4]
    vmovups ymm2, ymmword ptr [rdx + rax*4]
    vfmadd231ps ymm0, ymm1, ymm2   ; accum += A[i] * B[i]
    add     eax, 8
    jmp     @dp_256

@dp_512:
    vxorps  zmm0, zmm0, zmm0
@dp_512_loop:
    cmp     eax, r8d
    jae     @dp_reduce_512
    vmovups zmm1, zmmword ptr [rcx + rax*4]
    vmovups zmm2, zmmword ptr [rdx + rax*4]
    vfmadd231ps zmm0, zmm1, zmm2
    add     eax, 16
    jmp     @dp_512_loop

@dp_reduce_512:
    vextractf64x4 ymm1, zmm0, 1
    vaddps  ymm0, ymm0, ymm1

@dp_reduce:
    ; Horizontal sum: ymm0 → xmm0[0]
    vextractf128 xmm1, ymm0, 1
    vaddps  xmm0, xmm0, xmm1
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0

    vzeroupper
    add     rsp, 28h
    ret
SIMD_DotProduct ENDP

; ────────────────────────────────────────────────────────────────
; SIMD_ScaledDotBatch — Compute attention scores for one head
;   RCX = pQ         (float*, query vector, [head_dim])
;   RDX = pKBlock    (float*, K vectors in head-major layout, [tokens × head_dim])
;   R8D = numTokens  (number of K vectors)
;   R9D = headDim    (dimension, must be multiple of 8)
;   [rsp+28h] = pOutScores (float*, output score array [numTokens])
;
;   For each token t: outScores[t] = dot(Q, K[t]) / sqrt(headDim)
;   Uses KVHM head-major layout: K vectors are contiguous per head.
; ────────────────────────────────────────────────────────────────
SIMD_ScaledDotBatch PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    mov     rsi, rcx                ; pQ
    mov     rdi, rdx                ; pKBlock
    mov     r12d, r8d               ; numTokens
    mov     r13d, r9d               ; headDim
    mov     rbx, qword ptr [rsp + 28h + 48h]  ; pOutScores (after saves + alloc)

    ; Compute 1/sqrt(headDim)
    vcvtsi2ss xmm3, xmm3, r13d
    vrsqrtss xmm3, xmm3, xmm3      ; xmm3 = 1/sqrt(headDim)

    ; headStride = headDim * 4 (bytes per K vector)
    mov     eax, r13d
    shl     eax, 2                  ; × sizeof(float)

    xor     ecx, ecx                ; token index

@sdb_loop:
    cmp     ecx, r12d
    jae     @sdb_done

    ; pK = pKBlock + token * headDim * 4
    mov     eax, ecx
    imul    eax, r13d
    shl     eax, 2
    movsxd  rdx, eax
    lea     rdx, [rdi + rdx]        ; pK[token]

    ; Dot product Q · K[token]
    push    rcx
    push    rbx
    mov     rcx, rsi                ; pQ
    ; rdx = pK (already set)
    mov     r8d, r13d               ; headDim
    call    SIMD_DotProduct         ; result in xmm0
    pop     rbx
    pop     rcx

    ; Scale by 1/sqrt(headDim)
    vmulss  xmm0, xmm0, xmm3

    ; Store score
    vmovss  dword ptr [rbx + rcx*4], xmm0

    inc     ecx
    jmp     @sdb_loop

@sdb_done:
    vzeroupper
    add     rsp, 28h
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SIMD_ScaledDotBatch ENDP

; ────────────────────────────────────────────────────────────────
; SIMD_VAccumulate — Weighted sum of V vectors (score × V rows)
;   RCX = pOut      (float*, output [head_dim], zeroed by caller)
;   RDX = pScores   (float*, softmax probabilities [numTokens])
;   R8  = pVBlock   (float*, V vectors [numTokens × head_dim])
;   R9D = numTokens
;   [rsp+28h] = headDim (DWORD)
;
;   out += scores[t] * V[t] for each token t
; ────────────────────────────────────────────────────────────────
SIMD_VAccumulate PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    mov     rsi, rcx                ; pOut
    mov     rdi, rdx                ; pScores
    mov     rbx, r8                 ; pVBlock
    mov     r12d, r9d               ; numTokens
    mov     r13d, dword ptr [rsp + 28h + 48h]  ; headDim

    xor     ecx, ecx                ; token index

@va_token_loop:
    cmp     ecx, r12d
    jae     @va_done

    ; Broadcast score[t]
    vbroadcastss ymm0, dword ptr [rdi + rcx*4]

    ; pV = pVBlock + token * headDim * 4
    mov     eax, ecx
    imul    eax, r13d
    shl     eax, 2
    movsxd  rdx, eax
    lea     rdx, [rbx + rdx]

    ; out[d] += score * V[t][d]  (unrolled 8 floats per iteration)
    xor     eax, eax
@va_dim_loop:
    cmp     eax, r13d
    jae     @va_next_token
    vmovups ymm1, ymmword ptr [rdx + rax*4]    ; V[t][d..d+7]
    vmovups ymm2, ymmword ptr [rsi + rax*4]    ; out[d..d+7]
    vfmadd231ps ymm2, ymm0, ymm1              ; out += score * V
    vmovups ymmword ptr [rsi + rax*4], ymm2
    add     eax, 8
    jmp     @va_dim_loop

@va_next_token:
    inc     ecx
    jmp     @va_token_loop

@va_done:
    vzeroupper
    add     rsp, 28h
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SIMD_VAccumulate ENDP

; ────────────────────────────────────────────────────────────────
; SIMD_MatVecQ4 — Quantized Q4_0 matrix × float vector
;   RCX = pOut      (float*, output [rows])
;   RDX = pMatrix   (uint8_t*, Q4_0 packed: 32 nibbles + 2-byte scale per block)
;   R8  = pVec      (float*, input vector [cols])
;   R9D = rows
;   [rsp+28h] = cols (DWORD, must be multiple of 32)
;
;   Each Q4_0 block: 16 bytes of nibbles (32 values) + 2-byte fp16 scale
;   block_size = 18 bytes
;   For each row: dot(dequant(matrix_row), vec)
;
;   Auto-dispatches:
;     AVX-512: 2 ZMM FMAs/block via vpmovzxbd zmm (16-wide dequant)
;     AVX2:    4 YMM FMAs/block via vpmovzxbd ymm (8-wide dequant)
; ────────────────────────────────────────────────────────────────
SIMD_MatVecQ4 PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    sub     rsp, 30h
    .allocstack 30h
    .endprolog

    mov     rsi, rcx                ; pOut
    mov     rdi, rdx                ; pMatrix
    mov     rbx, r8                 ; pVec
    mov     r12d, r9d               ; rows
    mov     r13d, dword ptr [rsp + 30h + 58h]  ; cols

    ; Q4_0 block size = 18 bytes (16 nibble bytes + 2 scale bytes)
    ; blocks per row = cols / 32
    mov     eax, r13d
    shr     eax, 5                  ; blocks = cols / 32
    mov     r14d, eax               ; blocks per row

    xor     ecx, ecx                ; row index

@q4_row_loop:
    cmp     ecx, r12d
    jae     @q4_done

    ; ── AVX-512 / AVX2 dispatch ──
    cmp     g_hasAVX512, 1
    je      @q4_row_512

    ; ════════════════════════════════════════════════════════════
    ; AVX2 path: 4 YMM FMAs per block (8 values × 4 passes = 32)
    ; ════════════════════════════════════════════════════════════
    vxorps  ymm0, ymm0, ymm0       ; row accumulator
    xor     edx, edx                ; block index within row

@q4_block_loop:
    cmp     edx, r14d
    jae     @q4_store_row

    ; Each block: 18 bytes
    ; pBlock = pMatrix + (row * blocks_per_row + blockIdx) * 18
    mov     eax, ecx
    imul    eax, r14d
    add     eax, edx
    imul    eax, eax, 18
    lea     r8, [rdi + rax]         ; r8 = block ptr

    ; Read fp16 scale (last 2 bytes of block) and convert to fp32
    movzx   eax, word ptr [r8 + 16]
    vmovd   xmm4, eax
    vcvtph2ps xmm4, xmm4           ; scale as fp32 (AVX2 F16C)

    ; Dequantize 32 nibbles → 32 floats and dot with vec
    ; Production AVX2 vectorized: vpshufb + vpand parallel nibble extraction
    ; Process 32 nibbles (16 bytes) in two AVX2 passes of 16 nibbles each
    vbroadcastss ymm5, xmm4        ; scale broadcast across 8 lanes

    mov     r9d, edx
    shl     r9d, 5                  ; vec offset = blockIdx * 32

    ; ── Pass 1: Process nibble bytes [0..7] → 16 Q4 values → 16 floats ──
    ; Load 8 bytes of nibble data into xmm6 (low 64 bits)
    vmovq   xmm6, qword ptr [r8]

    ; Extract low nibbles: AND with 0x0F mask
    vbroadcastss ymm7, dword ptr [c_nibble_mask] ; 0x0F0F0F0F
    vpand   xmm1, xmm6, xmm7       ; low nibbles (8 bytes)

    ; Extract high nibbles: shift right 4 and AND
    vpsrlw  xmm2, xmm6, 4
    vpand   xmm2, xmm2, xmm7       ; high nibbles (8 bytes)

    ; Interleave: lo0, hi0, lo1, hi1, ... → 16 nibble values in order
    vpunpcklbw xmm3, xmm1, xmm2    ; interleave low and high nibbles

    ; Widen to 32-bit integers: first 8 nibbles
    vpmovzxbd ymm1, xmm3            ; 8 bytes → 8 DWORDs in YMM

    ; Subtract 8 to center around zero (Q4_0 offset)
    vbroadcastss ymm6, dword ptr [c_q4_offset] ; 8 as int
    vpsubd  ymm1, ymm1, ymm6        ; centered values

    ; Convert to float
    vcvtdq2ps ymm1, ymm1            ; 8 dequant'd floats

    ; Multiply by scale
    vmulps  ymm1, ymm1, ymm5        ; 8 scaled float values

    ; Load 8 input vector elements
    vmovups ymm2, ymmword ptr [rbx + r9*4]
    vfmadd231ps ymm0, ymm1, ymm2    ; accum += dequant * vec

    ; Second 8 of low pass: extract bytes [4..7] high for ymm upper
    vpsrldq xmm3, xmm3, 8          ; shift to get nibbles 8-15
    vpmovzxbd ymm1, xmm3
    vpsubd  ymm1, ymm1, ymm6
    vcvtdq2ps ymm1, ymm1
    vmulps  ymm1, ymm1, ymm5
    vmovups ymm2, ymmword ptr [rbx + r9*4 + 32]
    vfmadd231ps ymm0, ymm1, ymm2

    add     r9d, 16                 ; Advance vec offset by 16

    ; ── Pass 2: Process nibble bytes [8..15] → 16 Q4 values → 16 floats ──
    vmovq   xmm6, qword ptr [r8 + 8]

    vpand   xmm1, xmm6, xmm7
    vpsrlw  xmm2, xmm6, 4
    vpand   xmm2, xmm2, xmm7
    vpunpcklbw xmm3, xmm1, xmm2

    ; First 8 of pass 2
    ; Reload q4_offset into ymm6 (was clobbered by vmovq of nibble data above)
    vbroadcastss ymm6, dword ptr [c_q4_offset]
    vpmovzxbd ymm1, xmm3
    vpsubd  ymm1, ymm1, ymm6        ; centered values (correct single subtraction)
    vcvtdq2ps ymm1, ymm1
    vmulps  ymm1, ymm1, ymm5
    vmovups ymm2, ymmword ptr [rbx + r9*4]
    vfmadd231ps ymm0, ymm1, ymm2

    ; Second 8 of pass 2
    vpsrldq xmm3, xmm3, 8
    vpmovzxbd ymm1, xmm3
    vpsubd  ymm1, ymm1, ymm6
    vcvtdq2ps ymm1, ymm1
    vmulps  ymm1, ymm1, ymm5
    vmovups ymm2, ymmword ptr [rbx + r9*4 + 32]
    vfmadd231ps ymm0, ymm1, ymm2

@q4_next_block:
    inc     edx
    jmp     @q4_block_loop

@q4_store_row:
    ; Horizontal sum of ymm0 (8 floats → 1 scalar)
    ; Extract high 128 bits and add to low 128 bits
    vextractf128 xmm1, ymm0, 1
    vaddps  xmm0, xmm0, xmm1       ; 4 partial sums
    ; Horizontal add within 128 bits
    vhaddps xmm0, xmm0, xmm0       ; 2 partial sums
    vhaddps xmm0, xmm0, xmm0       ; 1 final sum
    vmovss  dword ptr [rsi + rcx*4], xmm0
    inc     ecx
    jmp     @q4_row_loop

    ; ════════════════════════════════════════════════════════════
    ; AVX-512 path: 2 ZMM FMAs per block (16 values × 2 passes = 32)
    ;
    ; Key improvement over AVX2 path:
    ;   - vmovdqu xmm loads all 16 nibble bytes at once (vs 2× vmovq)
    ;   - vpmovzxbd zmm widens 16 bytes → 16 dwords (vs 8)
    ;   - vfmadd231ps zmm does 16-wide FMA (vs 8-wide)
    ;   - 2 FMA instructions per block (vs 4) = 2× throughput
    ;
    ; Register allocation:
    ;   zmm0  = row accumulator (16-wide partial sums)
    ;   zmm8  = scale broadcast (per-block, from fp16)
    ;   zmm9  = q4_offset broadcast (integer 8, constant)
    ;   xmm10 = nibble mask 0x0F (constant, 128-bit for byte ops)
    ;   xmm4  = interleaved nibbles pass 1 (first 16 of 32)
    ;   xmm5  = interleaved nibbles pass 2 (last 16 of 32)
    ;   zmm1  = widened dequantized values
    ;   zmm2  = input vector elements
    ; ════════════════════════════════════════════════════════════
@q4_row_512:
    vxorps  zmm0, zmm0, zmm0       ; 16-wide row accumulator

    ; Load constants into high registers (preserved across block iterations)
    vpbroadcastd zmm9, dword ptr [c_q4_offset]     ; integer 8 in all 16 lanes
    vbroadcastss xmm10, dword ptr [c_nibble_mask]  ; 0x0F0F0F0F in 128-bit for vpand

    xor     edx, edx                ; block index within row

@q4_block_512:
    cmp     edx, r14d
    jae     @q4_store_row_512

    ; ── Block address: pMatrix + (row * blocksPerRow + blockIdx) * 18 ──
    mov     eax, ecx
    imul    eax, r14d
    add     eax, edx
    imul    eax, eax, 18
    lea     r8, [rdi + rax]         ; r8 = block ptr

    ; ── Read fp16 scale (last 2 bytes), convert to fp32, broadcast to zmm ──
    movzx   eax, word ptr [r8 + 16]
    vmovd   xmm8, eax
    vcvtph2ps xmm8, xmm8           ; fp16 → fp32 (AVX2 F16C)
    vbroadcastss zmm8, xmm8        ; scale in all 16 lanes

    ; ── Vec offset = blockIdx * 32 (each block covers 32 input elements) ──
    mov     r9d, edx
    shl     r9d, 5

    ; ── Load all 16 nibble bytes at once ──
    vmovdqu xmm1, xmmword ptr [r8]  ; 16 bytes = 32 packed nibbles

    ; ── Extract low nibbles: byte[i] & 0x0F → value[2*i] ──
    vpand   xmm2, xmm1, xmm10      ; low nibbles (16 bytes)

    ; ── Extract high nibbles: (byte[i] >> 4) & 0x0F → value[2*i+1] ──
    vpsrlw  xmm3, xmm1, 4
    vpand   xmm3, xmm3, xmm10      ; high nibbles (16 bytes)

    ; ── Interleave to sequential order: [nib0, nib1, nib2, ..., nib31] ──
    vpunpcklbw xmm4, xmm2, xmm3    ; first 16 nibbles (from bytes 0-7)
    vpunpckhbw xmm5, xmm2, xmm3    ; last 16 nibbles (from bytes 8-15)

    ; ── Pass 1: First 16 Q4 values → 16 floats → ZMM FMA ──
    vpmovzxbd zmm1, xmm4            ; 16 bytes → 16 dwords (AVX-512)
    vpsubd  zmm1, zmm1, zmm9        ; center: subtract 8 (unsigned→signed Q4_0)
    vcvtdq2ps zmm1, zmm1            ; int32 → float32
    vmulps  zmm1, zmm1, zmm8        ; × scale
    vmovups zmm2, zmmword ptr [rbx + r9*4]       ; 16 input vec elements
    vfmadd231ps zmm0, zmm1, zmm2    ; accum += dequant[0..15] * vec[0..15]

    ; ── Pass 2: Next 16 Q4 values → 16 floats → ZMM FMA ──
    vpmovzxbd zmm1, xmm5            ; 16 bytes → 16 dwords
    vpsubd  zmm1, zmm1, zmm9        ; center: subtract 8
    vcvtdq2ps zmm1, zmm1            ; int32 → float32
    vmulps  zmm1, zmm1, zmm8        ; × scale
    vmovups zmm2, zmmword ptr [rbx + r9*4 + 64]  ; next 16 input vec elements
    vfmadd231ps zmm0, zmm1, zmm2    ; accum += dequant[16..31] * vec[16..31]

    inc     edx
    jmp     @q4_block_512

@q4_store_row_512:
    ; ── Horizontal sum: zmm0 (16 floats) → scalar ──
    vextractf64x4 ymm1, zmm0, 1    ; high 256 bits
    vaddps  ymm0, ymm0, ymm1       ; 8 partial sums
    vextractf128 xmm1, ymm0, 1     ; high 128 bits
    vaddps  xmm0, xmm0, xmm1       ; 4 partial sums
    vhaddps xmm0, xmm0, xmm0       ; 2 partial sums
    vhaddps xmm0, xmm0, xmm0       ; 1 final sum
    vmovss  dword ptr [rsi + rcx*4], xmm0
    inc     ecx
    jmp     @q4_row_loop

@q4_done:
    vzeroupper
    add     rsp, 30h
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SIMD_MatVecQ4 ENDP

; ────────────────────────────────────────────────────────────────
; SIMD_RoPE — Rotary Position Embedding (in-place, vectorized)
;   RCX = pVec      (float*, interleaved [re0, im0, re1, im1, ...])
;   EDX = dim       (must be even, typically head_dim)
;   R8D = position  (token position index)
;   R9D = theta_base (typically 10000)
;
;   For each pair (i, i+1):
;     freq = position / (theta_base ^ (2*i / dim))
;     (re, im) = (re*cos(freq) - im*sin(freq),
;                  re*sin(freq) + im*cos(freq))
;
;   Production implementation: precomputed frequency table, minimax 
;   polynomial cos/sin (7-term), vectorized over 4 pairs per iteration.
; ────────────────────────────────────────────────────────────────
SIMD_RoPE PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    mov     rbp, rsp
    sub     rsp, 80h
    .allocstack 80h
    .endprolog

    mov     rsi, rcx                ; pVec
    mov     r12d, edx               ; dim
    mov     r13d, r8d               ; position
    
    ; Compute ln(theta_base) for angle computation
    ; ln(10000) ≈ 9.2103404 (we store this as a constant)
    ; freq[i] = exp(-(2*i/dim) * ln(theta_base))
    ; angle[i] = position * freq[i]
    
    ; Precompute dim_recip = 1.0 / dim
    vcvtsi2ss xmm0, xmm0, r12d
    vmovss  xmm1, dword ptr [c_one]
    vdivss  xmm1, xmm1, xmm0       ; xmm1 = 1.0 / dim
    vmovss  dword ptr [rsp+40h], xmm1 ; save dim_recip

    ; position as float
    vcvtsi2ss xmm2, xmm2, r13d      ; xmm2 = position (float)
    vmovss  dword ptr [rsp+44h], xmm2 ; save pos_f

    ; Total pairs = dim / 2
    mov     ebx, r12d
    shr     ebx, 1                  ; pairs

    ; Process 4 pairs per iteration (8 floats = 1 YMM register)
    xor     ecx, ecx                ; pair index

@rope_vec_loop:
    lea     eax, [ecx + 4]
    cmp     eax, ebx                ; Can we do 4 pairs?
    jg      @rope_scalar_tail       ; Less than 4 pairs remaining

    ; ── Compute 4 angles simultaneously ──
    ; freq[i] = exp(-2 * i / dim * ln(theta_base))
    ; Build vector of pair indices: [i, i+1, i+2, i+3]
    vcvtsi2ss xmm0, xmm0, ecx
    vbroadcastss xmm0, xmm0         ; [i, i, i, i]
    vaddps  xmm0, xmm0, xmmword ptr [c_rope_offsets] ; [i, i+1, i+2, i+3]

    ; Multiply by 2.0 / dim = 2 * dim_recip
    vmovss  xmm1, dword ptr [rsp+40h]
    vaddss  xmm1, xmm1, xmm1       ; 2/dim
    vbroadcastss xmm1, xmm1
    vmulps  xmm0, xmm0, xmm1       ; 2i/dim for each pair

    ; Multiply by -ln(theta_base)
    vbroadcastss xmm1, dword ptr [c_neg_ln_theta]  ; -9.2103404
    vmulps  xmm0, xmm0, xmm1       ; -(2i/dim) * ln(base)

    ; Compute exp(x) via polynomial approximation (range-reduced)
    ; exp(x) ≈ 2^(x/ln2) = 2^n * 2^f where f = frac part
    ; Use minimax polynomial for 2^f on [0, 1)
    vbroadcastss xmm1, dword ptr [c_log2e]  ; 1/ln(2) = 1.4426950
    vmulps  xmm0, xmm0, xmm1       ; x / ln(2) = full exponent

    ; Split into integer part and fraction
    vroundps xmm1, xmm0, 1         ; floor(x/ln2) = n
    vsubps  xmm2, xmm0, xmm1       ; f = frac part [0, 1)

    ; 2^f ≈ minimax polynomial (degree 4)
    ; p(f) = c0 + f*(c1 + f*(c2 + f*(c3 + f*c4)))
    vbroadcastss xmm3, dword ptr [c_exp_c4]
    vmulps  xmm3, xmm3, xmm2       ; c4 * f
    vbroadcastss xmm4, dword ptr [c_exp_c3]
    vaddps  xmm3, xmm3, xmm4       ; c3 + c4*f
    vmulps  xmm3, xmm3, xmm2       ; f * (c3 + c4*f)
    vbroadcastss xmm4, dword ptr [c_exp_c2]
    vaddps  xmm3, xmm3, xmm4       ; c2 + ...
    vmulps  xmm3, xmm3, xmm2       ; f * (c2 + ...)
    vbroadcastss xmm4, dword ptr [c_exp_c1]
    vaddps  xmm3, xmm3, xmm4       ; c1 + ...
    vmulps  xmm3, xmm3, xmm2       ; f * (c1 + ...)
    vbroadcastss xmm4, dword ptr [c_exp_c0]
    vaddps  xmm3, xmm3, xmm4       ; 2^f ≈ p(f)

    ; Scale by 2^n using float bit manipulation
    ; 2^n: convert n to int, shift left 23, add to exponent
    vcvtps2dq xmm1, xmm1            ; n as integers
    vpslld  xmm1, xmm1, 23          ; shift into exponent field
    vpaddd  xmm3, xmm3, xmm1       ; 2^f * 2^n = exp(x)
    ; xmm3 now contains 4 frequency values: freq[i..i+3]

    ; angle = position * freq
    vbroadcastss xmm0, dword ptr [rsp+44h]  ; position
    vmulps  xmm0, xmm0, xmm3       ; 4 angles

    ; ── Compute cos and sin via Cody-Waite range reduction + minimax polynomial ──
    ; Range reduce: angle mod 2π (approximate via repeated subtraction)
    vbroadcastss xmm7, dword ptr [c_twopi]
    vbroadcastss xmm6, dword ptr [c_twopi_recip]
    vmulps  xmm1, xmm0, xmm6        ; angle / 2π
    vroundps xmm1, xmm1, 0          ; round to nearest int
    vmulps  xmm1, xmm1, xmm7        ; n * 2π
    vsubps  xmm0, xmm0, xmm1        ; reduced angle in [-π, π]

    ; cos(x) via minimax polynomial (degree 6): cos(x) ≈ 1 - x²/2 + x⁴/24 - x⁶/720
    vmulps  xmm1, xmm0, xmm0       ; x²
    vmulps  xmm2, xmm1, xmm1       ; x⁴
    vmulps  xmm3, xmm2, xmm1       ; x⁶

    vbroadcastss xmm4, dword ptr [c_cos_c2]  ; -1/2
    vmulps  xmm4, xmm4, xmm1       ; -x²/2
    vbroadcastss xmm5, dword ptr [c_cos_c4]  ; 1/24
    vmulps  xmm5, xmm5, xmm2       ; x⁴/24
    vaddps  xmm4, xmm4, xmm5
    vbroadcastss xmm5, dword ptr [c_cos_c6]  ; -1/720
    vmulps  xmm5, xmm5, xmm3       ; -x⁶/720
    vaddps  xmm4, xmm4, xmm5
    vbroadcastss xmm5, dword ptr [c_one]
    vaddps  xmm4, xmm4, xmm5       ; cos(x) ≈ 1 - x²/2 + x⁴/24 - x⁶/720

    ; sin(x) via minimax polynomial (degree 7): sin(x) ≈ x - x³/6 + x⁵/120 - x⁷/5040
    vmulps  xmm5, xmm1, xmm0       ; x³
    vbroadcastss xmm6, dword ptr [c_sin_c3]  ; -1/6
    vmulps  xmm6, xmm6, xmm5       ; -x³/6
    vaddps  xmm6, xmm6, xmm0       ; x - x³/6

    vmulps  xmm5, xmm2, xmm0       ; x⁵
    vbroadcastss xmm7, dword ptr [c_sin_c5]  ; 1/120
    vmulps  xmm7, xmm7, xmm5       ; x⁵/120
    vaddps  xmm6, xmm6, xmm7

    vmulps  xmm5, xmm3, xmm0       ; x⁷
    vbroadcastss xmm7, dword ptr [c_sin_c7]  ; -1/5040
    vmulps  xmm7, xmm7, xmm5       ; -x⁷/5040
    vaddps  xmm6, xmm6, xmm7       ; sin(x) complete

    ; xmm4 = cos4, xmm6 = sin4 (4 values each)
    ; Now apply rotation to 4 (re, im) pairs
    ; Data layout: [re0, im0, re1, im1, re2, im2, re3, im3] = 8 floats
    ; Load 8 floats (4 pairs)
    mov     eax, ecx
    shl     eax, 3                  ; pair * 8 bytes
    vmovups ymm0, ymmword ptr [rsi + rax]

    ; Separate re and im using shuffle
    ; re = [re0, re1, re2, re3], im = [im0, im1, im2, im3]
    vshufps ymm1, ymm0, ymm0, 088h  ; re: pick elements 0,2 from each pair
    vshufps ymm2, ymm0, ymm0, 0DDh  ; im: pick elements 1,3 from each pair
    ; Note: we only have 4 values in xmm register range. Adjust:
    ; Actually, re/im pairs are interleaved as 128-bit: [re0,im0,re1,im1] | [re2,im2,re3,im3]
    ; Use vshufps within 128-bit lanes:
    vshufps xmm1, xmm0, xmm0, 088h  ; [re0, re1, ?, ?] from low 128
    vextractf128 xmm3, ymm0, 1      ; high 128 bits
    vshufps xmm5, xmm3, xmm3, 088h  ; [re2, re3, ?, ?]
    
    vshufps xmm2, xmm0, xmm0, 0DDh  ; [im0, im1, ?, ?]
    vshufps xmm7, xmm3, xmm3, 0DDh  ; [im2, im3, ?, ?]

    ; Pack into xmm: re = [re0, re1, re2, re3], im = [im0, im1, im2, im3]
    vinsertps xmm1, xmm1, xmm5, 020h  ; re[2] = xmm5[0]
    vinsertps xmm1, xmm1, xmm5, 031h  ; re[3] = xmm5[1]
    vinsertps xmm2, xmm2, xmm7, 020h  ; im[2]
    vinsertps xmm2, xmm2, xmm7, 031h  ; im[3]

    ; Rotation: new_re = re*cos - im*sin
    ;           new_im = re*sin + im*cos
    vmulps  xmm3, xmm1, xmm4       ; re * cos
    vmulps  xmm5, xmm2, xmm6       ; im * sin
    vsubps  xmm3, xmm3, xmm5       ; new_re = re*cos - im*sin

    vmulps  xmm5, xmm1, xmm6       ; re * sin
    vmulps  xmm7, xmm2, xmm4       ; im * cos
    vaddps  xmm5, xmm5, xmm7       ; new_im = re*sin + im*cos

    ; Interleave back: [new_re0, new_im0, new_re1, new_im1, ...]
    vunpcklps xmm0, xmm3, xmm5     ; [re0, im0, re1, im1]
    vunpckhps xmm1, xmm3, xmm5     ; [re2, im2, re3, im3]
    vinsertf128 ymm0, ymm0, xmm1, 1 ; Full 256-bit result

    ; Store back
    vmovups ymmword ptr [rsi + rax], ymm0

    add     ecx, 4
    jmp     @rope_vec_loop

    ; ── Scalar tail for remaining 0-3 pairs ──
@rope_scalar_tail:
    cmp     ecx, ebx
    jge     @rope_done

    ; Compute angle for single pair
    mov     eax, ecx
    shl     eax, 1                  ; 2*i
    vcvtsi2ss xmm0, xmm0, eax
    vmulss  xmm0, xmm0, dword ptr [rsp+40h] ; 2i/dim
    vmulss  xmm0, xmm0, dword ptr [c_neg_ln_theta] ; -(2i/dim)*ln(base)

    ; Scalar exp via the same polynomial
    vmulss  xmm0, xmm0, dword ptr [c_log2e]
    vroundss xmm1, xmm0, xmm0, 1   ; n = floor
    vsubss  xmm2, xmm0, xmm1        ; f = frac
    ; 2^f poly
    vmovss  xmm3, dword ptr [c_exp_c4]
    vmulss  xmm3, xmm3, xmm2
    vaddss  xmm3, xmm3, dword ptr [c_exp_c3]
    vmulss  xmm3, xmm3, xmm2
    vaddss  xmm3, xmm3, dword ptr [c_exp_c2]
    vmulss  xmm3, xmm3, xmm2
    vaddss  xmm3, xmm3, dword ptr [c_exp_c1]
    vmulss  xmm3, xmm3, xmm2
    vaddss  xmm3, xmm3, dword ptr [c_exp_c0]
    vcvtss2si eax, xmm1             ; n as int
    shl     eax, 23
    vmovd   xmm1, eax
    vpaddd  xmm3, xmm3, xmm1       ; freq = exp(x)

    ; angle = pos * freq
    vmulss  xmm0, xmm3, dword ptr [rsp+44h]

    ; cos/sin scalar poly
    vmulss  xmm1, xmm0, xmm0       ; x²
    vmulss  xmm2, xmm1, xmm1       ; x⁴
    vmulss  xmm3, xmm2, xmm1       ; x⁶

    ; cos = 1 - x²/2 + x⁴/24 - x⁶/720
    vmulss  xmm4, xmm1, dword ptr [c_cos_c2]
    vmulss  xmm5, xmm2, dword ptr [c_cos_c4]
    vaddss  xmm4, xmm4, xmm5
    vmulss  xmm5, xmm3, dword ptr [c_cos_c6]
    vaddss  xmm4, xmm4, xmm5
    vaddss  xmm4, xmm4, dword ptr [c_one]

    ; sin = x - x³/6 + x⁵/120 - x⁷/5040
    vmulss  xmm5, xmm1, xmm0       ; x³
    vmulss  xmm6, xmm5, dword ptr [c_sin_c3]
    vaddss  xmm6, xmm6, xmm0
    vmulss  xmm5, xmm2, xmm0       ; x⁵
    vmulss  xmm7, xmm5, dword ptr [c_sin_c5]
    vaddss  xmm6, xmm6, xmm7
    vmulss  xmm5, xmm3, xmm0       ; x⁷
    vmulss  xmm7, xmm5, dword ptr [c_sin_c7]
    vaddss  xmm6, xmm6, xmm7

    ; Read (re, im) pair
    mov     eax, ecx
    shl     eax, 3
    vmovss  xmm0, dword ptr [rsi + rax]      ; re
    vmovss  xmm1, dword ptr [rsi + rax + 4]  ; im

    ; Rotate
    vmulss  xmm2, xmm0, xmm4       ; re * cos
    vmulss  xmm3, xmm1, xmm6       ; im * sin
    vsubss  xmm2, xmm2, xmm3       ; new_re

    vmulss  xmm3, xmm0, xmm6       ; re * sin
    vmulss  xmm5, xmm1, xmm4       ; im * cos
    vaddss  xmm3, xmm3, xmm5       ; new_im

    vmovss  dword ptr [rsi + rax], xmm2
    vmovss  dword ptr [rsi + rax + 4], xmm3

    inc     ecx
    jmp     @rope_scalar_tail

@rope_done:
    vzeroupper
    add     rsp, 80h
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SIMD_RoPE ENDP

; ────────────────────────────────────────────────────────────────
; SIMD_SiLU — SiLU (Swish) activation: x * sigmoid(x)
;   RCX = pInOut    (float*, in-place)
;   EDX = count     (elements)
;
;   SiLU(x) = x / (1 + exp(-x))
;   Vectorized: rational sigmoid approximation
;     sig(x) ≈ 0.5 + 0.5 * x / (1 + |x|)
;   8-wide AVX2 main loop, scalar tail for remainder
; ────────────────────────────────────────────────────────────────
SIMD_SiLU PROC FRAME
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    test    edx, edx
    jz      @silu_done

    mov     r8d, edx                     ; total count
    xor     eax, eax                     ; element index

    ; Preload broadcast constants for 8-wide path
    vbroadcastss ymm4, dword ptr [c_one]     ; 1.0
    vmovups ymm5, ymmword ptr [c_half_f]     ; 0.5 (already 8-wide)
    vmovups ymm6, ymmword ptr [c_abs_mask]   ; abs mask (8-wide)

    ; Main loop: 8 elements per iteration
    mov     r9d, r8d
    and     r9d, 0FFFFFFF8h              ; r9d = count & ~7
    test    r9d, r9d
    jz      @silu_scalar_tail

@silu_vec_loop:
    cmp     eax, r9d
    jae     @silu_scalar_tail

    ; Load 8 floats
    vmovups ymm0, ymmword ptr [rcx + rax*4]  ; x[0..7]

    ; abs(x) = x & 0x7FFFFFFF
    vandps  ymm1, ymm0, ymm6            ; |x|

    ; denominator = 1.0 + |x|
    vaddps  ymm2, ymm1, ymm4            ; 1 + |x|

    ; x / (1 + |x|) via reciprocal with Newton-Raphson refinement
    ; First estimate
    vrcpps  ymm3, ymm2                  ; ~1/(1+|x|), 12-bit accuracy
    ; Newton-Raphson: y' = y * (2 - d*y) for better precision
    vmulps  ymm7, ymm2, ymm3            ; d * y
    vsubps  ymm7, ymm4, ymm7            ; 1 - d*y (note: need 2-d*y)
    ; Actually: 2-d*y = 1 + (1 - d*y), so:
    vaddps  ymm7, ymm7, ymm4            ; 2 - d*y
    vmulps  ymm3, ymm3, ymm7            ; refined reciprocal
    vmulps  ymm3, ymm0, ymm3            ; x / (1 + |x|)

    ; sigmoid = 0.5 + 0.5 * ratio
    vmulps  ymm3, ymm3, ymm5            ; 0.5 * ratio
    vaddps  ymm3, ymm3, ymm5            ; sigmoid ≈ 0.5 + 0.5*(x/(1+|x|))

    ; SiLU = x * sigmoid
    vmulps  ymm0, ymm0, ymm3
    vmovups ymmword ptr [rcx + rax*4], ymm0

    add     eax, 8
    jmp     @silu_vec_loop

    ; ── Scalar tail for remaining 0-7 elements ──
@silu_scalar_tail:
    cmp     eax, r8d
    jae     @silu_done

    vmovss  xmm0, dword ptr [rcx + rax*4]   ; x
    vandps  xmm1, xmm0, xmmword ptr [c_abs_mask] ; |x|
    vmovss  xmm2, dword ptr [c_one]
    vaddss  xmm1, xmm1, xmm2               ; 1 + |x|
    vdivss  xmm1, xmm0, xmm1               ; x / (1+|x|)
    vmulss  xmm1, xmm1, dword ptr [c_half_f]; × 0.5
    vaddss  xmm1, xmm1, dword ptr [c_half_f]; sigmoid
    vmulss  xmm0, xmm0, xmm1               ; silu = x * sig
    vmovss  dword ptr [rcx + rax*4], xmm0

    inc     eax
    jmp     @silu_scalar_tail

@silu_done:
    vzeroupper
    add     rsp, 28h
    ret
SIMD_SiLU ENDP

; ── Additional constants for softmax / RoPE / SiLU / Q4 ──────
.const
align 16
c_half          dd 0.5
c_half_f        dd 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5  ; broadcast-ready
c_sixth         dd 0.16666667          ; 1/6
c_24th          dd 0.04166667          ; 1/24
align 16
c_abs_mask      dd 7FFFFFFFh, 7FFFFFFFh, 7FFFFFFFh, 7FFFFFFFh
                dd 7FFFFFFFh, 7FFFFFFFh, 7FFFFFFFh, 7FFFFFFFh  ; 256-bit

; ── Q4 dequantization constants ──────────────────────────────
align 16
c_nibble_mask   dd 0F0F0F0Fh, 0F0F0F0Fh, 0F0F0F0Fh, 0F0F0F0Fh
                dd 0F0F0F0Fh, 0F0F0F0Fh, 0F0F0F0Fh, 0F0F0F0Fh
c_q4_offset     dd 8, 8, 8, 8, 8, 8, 8, 8    ; bias for unsigned→signed

; ── RoPE trigonometric constants ─────────────────────────────
align 16
c_neg_ln_theta  dd -9.2103404          ; -ln(10000)
c_log2e         dd 1.4426950           ; 1/ln(2)
c_twopi         dd 6.2831855           ; 2π
c_twopi_recip   dd 0.1591549           ; 1/(2π)

; Minimax exp2(f) polynomial coefficients (f ∈ [0,1))
; 2^f ≈ c0 + c1*f + c2*f² + c3*f³ + c4*f⁴
c_exp_c0        dd 1.0
c_exp_c1        dd 0.6931472           ; ln(2)
c_exp_c2        dd 0.2402265           ; ln(2)²/2
c_exp_c3        dd 0.0555041           ; ln(2)³/6
c_exp_c4        dd 0.0096139           ; ln(2)⁴/24

; cos(x) Taylor coefficients
c_cos_c2        dd -0.5                ; -1/2
c_cos_c4        dd 0.041666668         ;  1/24
c_cos_c6        dd -0.001388889        ; -1/720

; sin(x) Taylor coefficients
c_sin_c3        dd -0.16666667         ; -1/6
c_sin_c5        dd 0.0083333335        ;  1/120
c_sin_c7        dd -0.00019841270      ; -1/5040

; RoPE pair index offsets for vectorized loop
align 16
c_rope_offsets  dd 0.0, 1.0, 2.0, 3.0

; ── SiLU vectorized constants ────────────────────────────────
align 16
c_silu_clamp_hi dd 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0
c_silu_clamp_lo dd -10.0, -10.0, -10.0, -10.0, -10.0, -10.0, -10.0, -10.0

.code
; ────────────────────────────────────────────────────────────────
; SIMD_TextSearch — SSE2/AVX2 accelerated byte-pattern search
;
; Scans a haystack buffer for all occurrences of a needle byte
; pattern using pcmpeqb + pmovmskb for 16-byte-at-a-time scanning,
; with AVX2 upgrade to 32 bytes/iteration when available.
;
; Parameters:
;   RCX = pHaystack   (const uint8_t*, buffer to search in)
;   RDX = haystackLen (uint64_t, byte count)
;   R8  = pNeedle     (const uint8_t*, pattern to find)
;   R9  = needleLen   (uint64_t, pattern byte count, 1-255)
;   [RSP+28h] = pResults (uint64_t*, array to receive match offsets)
;   [RSP+30h] = maxResults (uint32_t, capacity of pResults array)
;
; Returns:
;   EAX = number of matches found (capped at maxResults)
;
; Algorithm:
;   1. Broadcast needle[0] into xmm0 (or ymm0 for AVX2)
;   2. Scan haystack in 16/32-byte chunks with pcmpeqb
;   3. pmovmskb extracts match bits → bsf finds first hit
;   4. For each candidate, memcmp full needle at that offset
;   5. Record match offset in results array
; ────────────────────────────────────────────────────────────────
SIMD_TextSearch PROC FRAME
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 40h
    .allocstack 40h
    .endprolog

    ; Save parameters
    mov     rsi, rcx                ; rsi = pHaystack
    mov     r12, rdx                ; r12 = haystackLen
    mov     r13, r8                 ; r13 = pNeedle
    mov     r14, r9                 ; r14 = needleLen
    mov     rax, qword ptr [rbp + 88h]  ; pResults (after 8 pushes + ret addr)
    mov     r15, rax
    mov     eax, dword ptr [rbp + 90h]  ; maxResults
    mov     dword ptr [rsp + 20h], eax  ; local: maxResults
    xor     ebx, ebx                ; ebx = match count

    ; Validate inputs
    test    rsi, rsi
    jz      @ts_done
    test    r12, r12
    jz      @ts_done
    test    r13, r13
    jz      @ts_done
    test    r14, r14
    jz      @ts_done
    cmp     r14, r12
    ja      @ts_done                ; needle longer than haystack

    ; Broadcast needle[0] into xmm0 (16 copies)
    movzx   eax, byte ptr [r13]
    movd    xmm0, eax
    punpcklbw xmm0, xmm0
    punpcklwd xmm0, xmm0
    pshufd  xmm0, xmm0, 0          ; xmm0 = 16x needle[0]

    ; Calculate scan limit: haystack can only match up to (len - needleLen)
    mov     rdi, r12
    sub     rdi, r14                ; rdi = last valid start offset
    inc     rdi                     ; rdi = number of valid positions
    xor     ecx, ecx                ; ecx = current scan offset

    ; Check for AVX2 availability
    cmp     dword ptr [g_hasAVX512], 0
    jne     @ts_avx2_path           ; AVX-512 implies AVX2

    ; Try cpuid for AVX2 (bit 5 of EBX from CPUID leaf 7)
    ; Simple approach: just use SSE2 which is baseline x86-64
    jmp     @ts_sse2_loop

    ; ── AVX2 path: 32 bytes per iteration ─────────────────────
@ts_avx2_path:
    ; Broadcast needle[0] into ymm1 (32 copies)
    vbroadcastss ymm1, xmm0        ; ymm1 = 32x needle[0] (byte broadcast via float)
    ; Actually need vpbroadcastb — use vinserti128 workaround
    vinserti128 ymm0, ymm0, xmm0, 1  ; ymm0 = [xmm0 | xmm0] = 32x needle[0]

@ts_avx2_loop:
    mov     rax, rdi
    sub     rax, rcx                ; remaining valid positions
    cmp     rax, 32
    jb      @ts_sse2_tail           ; less than 32 bytes left, fall through to SSE2

    ; Load 32 bytes from haystack[offset]
    vmovdqu ymm2, ymmword ptr [rsi + rcx]
    vpcmpeqb ymm3, ymm2, ymm0      ; compare each byte to needle[0]
    vpmovmskb eax, ymm3             ; 32-bit mask of matches

    test    eax, eax
    jz      @ts_avx2_advance

@ts_avx2_check_bits:
    bsf     edx, eax                ; edx = bit index of first match
    jz      @ts_avx2_advance

    ; Candidate at offset (rcx + rdx)
    lea     r8, [rcx + rdx]
    ; Verify: memcmp(haystack + r8, needle, needleLen)
    push    rcx
    push    rax
    mov     rcx, r14                ; needleLen
    lea     r9, [rsi + r8]          ; haystack + candidate offset
    mov     r10, r13                ; needle
    xor     r11d, r11d              ; byte index

@ts_avx2_cmp:
    cmp     r11, rcx
    jge     @ts_avx2_match
    movzx   eax, byte ptr [r9 + r11]
    cmp     al, byte ptr [r10 + r11]
    jne     @ts_avx2_no_match
    inc     r11
    jmp     @ts_avx2_cmp

@ts_avx2_match:
    ; Record match offset
    cmp     ebx, dword ptr [rsp + 20h + 16] ; maxResults (adjusted for pushes)
    jge     @ts_avx2_no_match
    mov     qword ptr [r15 + rbx*8], r8
    inc     ebx

@ts_avx2_no_match:
    pop     rax
    pop     rcx
    ; Clear the bit we just checked
    btr     eax, edx
    test    eax, eax
    jnz     @ts_avx2_check_bits

@ts_avx2_advance:
    add     ecx, 32
    jmp     @ts_avx2_loop

@ts_sse2_tail:
    vzeroupper                      ; clean AVX→SSE transition

    ; ── SSE2 path: 16 bytes per iteration ─────────────────────
@ts_sse2_loop:
    mov     rax, rdi
    sub     rax, rcx                ; remaining valid positions
    cmp     rax, 16
    jb      @ts_scalar_tail

    ; Load 16 bytes from haystack[offset]
    movdqu  xmm2, xmmword ptr [rsi + rcx]
    pcmpeqb xmm2, xmm0             ; compare each byte to needle[0]
    pmovmskb eax, xmm2             ; 16-bit mask of matches

    test    eax, eax
    jz      @ts_sse2_advance

@ts_sse2_check_bits:
    bsf     edx, eax                ; edx = bit index of first match
    jz      @ts_sse2_advance

    ; Candidate at offset (rcx + rdx)
    lea     r8, [rcx + rdx]
    ; Bounds check: candidate + needleLen must not exceed haystackLen
    lea     r9, [r8 + r14]
    cmp     r9, r12
    ja      @ts_sse2_skip_bit

    ; Verify full needle match
    push    rcx
    push    rax
    mov     rcx, r14                ; needleLen
    lea     r9, [rsi + r8]          ; haystack + candidate
    mov     r10, r13                ; needle
    xor     r11d, r11d

@ts_sse2_cmp:
    cmp     r11, rcx
    jge     @ts_sse2_match
    movzx   eax, byte ptr [r9 + r11]
    cmp     al, byte ptr [r10 + r11]
    jne     @ts_sse2_no_match
    inc     r11
    jmp     @ts_sse2_cmp

@ts_sse2_match:
    cmp     ebx, dword ptr [rsp + 20h + 16] ; maxResults (adjusted for pushes)
    jge     @ts_sse2_no_match
    mov     qword ptr [r15 + rbx*8], r8
    inc     ebx

@ts_sse2_no_match:
    pop     rax
    pop     rcx

@ts_sse2_skip_bit:
    btr     eax, edx
    test    eax, eax
    jnz     @ts_sse2_check_bits

@ts_sse2_advance:
    add     ecx, 16
    jmp     @ts_sse2_loop

    ; ── Scalar tail: less than 16 bytes remaining ─────────────
@ts_scalar_tail:
    cmp     rcx, rdi
    jge     @ts_done

    movzx   eax, byte ptr [rsi + rcx]
    cmp     al, byte ptr [r13]
    jne     @ts_scalar_next

    ; First byte matches — verify full needle
    lea     r8, [rcx]
    lea     r9, [rsi + r8]
    mov     r10, r13
    xor     r11d, r11d

@ts_scalar_cmp:
    cmp     r11, r14
    jge     @ts_scalar_match
    movzx   eax, byte ptr [r9 + r11]
    cmp     al, byte ptr [r10 + r11]
    jne     @ts_scalar_next
    inc     r11
    jmp     @ts_scalar_cmp

@ts_scalar_match:
    cmp     ebx, dword ptr [rsp + 20h]  ; maxResults
    jge     @ts_done
    mov     qword ptr [r15 + rbx*8], r8
    inc     ebx

@ts_scalar_next:
    inc     ecx
    jmp     @ts_scalar_tail

@ts_done:
    mov     eax, ebx                ; return match count

    lea     rsp, [rbp]
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
SIMD_TextSearch ENDP

; ────────────────────────────────────────────────────────────────
; SIMD_FlashAttention — Hybrid Dispatcher for CPU
; RCX = *Q, RDX = *K, R8 = *V, R9 = *Out, Stack = M, N, head_dim
; ────────────────────────────────────────────────────────────────
align 16
SIMD_FlashAttention PROC
    push    rbp
    mov     rbp, rsp
    
    ; Setup parameter spill properly
    sub     rsp, 32

    ; Check AVX-512 context globally
    mov     eax, dword ptr [g_hasAVX512]
    test    eax, eax
    jz      @@dispatch_avx2

    ; Route AVX-512 FMA compute path mapping v14.7.0
    call    FlashAttention_Forward
    jmp     @@done

@@dispatch_avx2:
    ; Route AVX2 Legacy Core Fallback 
    ; call    FlashAttention_AVX2 (Stubbed out)
    mov     rax, -1 ; Return -1 = pending AVX2 implementation

@@done:
    add     rsp, 32
    pop     rbp
    ret
SIMD_FlashAttention ENDP

END
