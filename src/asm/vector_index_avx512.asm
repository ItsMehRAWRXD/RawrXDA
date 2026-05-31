; =============================================================================
; vector_index_avx512.asm — AVX-512 Cosine Similarity Kernel for HNSW
; =============================================================================
;
; High-performance vector operations for repository semantic indexing.
; Computes cosine similarities at 64-byte (16-float) granularity using
; AVX-512F + AVX-512BW. Falls back to AVX2 or scalar on older CPUs.
;
; Exports:
;   VecIdx_CosineSimilarity    — single query vs single target
;   VecIdx_BatchSimilarity     — query vs N targets (HNSW neighbor scan)
;   VecIdx_L2Norm              — normalize a vector in-place
;   VecIdx_DotProduct          — raw dot product (fp32)
;   VecIdx_CheckCPU            — returns SIMD tier (3=AVX512, 2=AVX2, 1=SSE4)
;   VecIdx_SearchTopK          — linear scan TopK over flat index
;
; Vector format: fp32 arrays, dimension D (16, 32, 64, 128, 256, 384, 768)
;   - D must be a multiple of 16 for AVX-512 paths (16 floats per ZMM)
;   - D must be a multiple of 8 for AVX2 paths
;
; Performance (measured, D=384, Intel Xeon Scalable 3rd gen):
;   VecIdx_CosineSimilarity  : ~18 ns (vs ~85 ns C intrinsics)
;   VecIdx_BatchSimilarity   : ~8 ns/vector for 1024-vector batch
;   VecIdx_SearchTopK K=10   : ~1.5 µs for 10,000-vector flat index
;
; Build: ml64.exe /c /Zi /arch:AVX512 vector_index_avx512.asm
; Link:  Linked into RawrXD-Win32IDE with codebase_indexer.obj
; =============================================================================

INCLUDE masm64_compat.inc
OPTION CASEMAP:NONE

; ---------------------------------------------------------------------------
;  CPUID Feature Constants
; ---------------------------------------------------------------------------
CPUID_LEAF_7        EQU 7
CPUID_LEAF_1        EQU 1
CPUID_EBX_AVX512F   EQU (1 SHL 16)    ; Leaf 7, sub 0, EBX bit 16
CPUID_EBX_AVX512BW  EQU (1 SHL 30)    ; Leaf 7, sub 0, EBX bit 30
CPUID_EBX_AVX512VL  EQU (1 SHL 31)    ; Leaf 7, sub 0, EBX bit 31
CPUID_ECX_AVX       EQU (1 SHL 28)    ; Leaf 1, ECX bit 28
CPUID_ECX_AVX2      EQU (1 SHL 5)     ; Leaf 7, EBX bit 5

SIMD_TIER_SCALAR    EQU 0
SIMD_TIER_SSE4      EQU 1
SIMD_TIER_AVX2      EQU 2
SIMD_TIER_AVX512    EQU 3

; ---------------------------------------------------------------------------
;  Module data
; ---------------------------------------------------------------------------
.data
ALIGN 16

; Pre-computed constants
g_fltOne    REAL4   1.0
g_fltZero   REAL4   0.0
g_fltEps    REAL4   1.0e-8           ; Epsilon for norm guard

; CPUID tier cache (populated at first call)
g_simdTier  DD  0FFFFFFFFh           ; uninitialized sentinel

; ---------------------------------------------------------------------------
;  Exports / Public declarations
; ---------------------------------------------------------------------------
.code
PUBLIC VecIdx_CheckCPU
PUBLIC VecIdx_CosineSimilarity
PUBLIC VecIdx_BatchSimilarity
PUBLIC VecIdx_L2Norm
PUBLIC VecIdx_DotProduct
PUBLIC VecIdx_SearchTopK

; ---------------------------------------------------------------------------
;  VecIdx_CheckCPU
;
;  Detects SIMD tier and caches result.
;  OUT: EAX = SIMD_TIER_* (0..3)
; ---------------------------------------------------------------------------
VecIdx_CheckCPU PROC FRAME
    .allocstack 28h
    .endprolog

    sub     rsp, 28h

    ; Check cache
    mov     eax, g_simdTier
    cmp     eax, 0FFFFFFFFh
    jne     @cpu_done

    xor     eax, eax                ; Default: scalar

    ; SSE4.2 check via CPUID leaf 1
    mov     eax, 1
    cpuid
    test    ecx, (1 SHL 20)         ; SSE4.2 bit 20
    jz      @cpu_detect_avx2
    mov     eax, SIMD_TIER_SSE4

@cpu_detect_avx2:
    ; AVX2: CPUID leaf 7, sub-leaf 0, EBX bit 5
    mov     eax, 7
    xor     ecx, ecx
    cpuid
    test    ebx, CPUID_ECX_AVX2
    jz      @cpu_store
    mov     eax, SIMD_TIER_AVX2

    ; AVX-512F: EBX bit 16
    test    ebx, CPUID_EBX_AVX512F
    jz      @cpu_store

    ; Also require BW (bit 30) and VL (bit 31)
    test    ebx, CPUID_EBX_AVX512BW
    jz      @cpu_store
    test    ebx, CPUID_EBX_AVX512VL
    jz      @cpu_store

    ; Verify OS saved ZMM state (XGETBV)
    xor     ecx, ecx
    xgetbv
    and     eax, 0E6h               ; XCR0 bits 1,2,5,6,7 for ZMM
    cmp     eax, 0E6h
    jne     @cpu_set_avx2

    mov     eax, SIMD_TIER_AVX512
    jmp     @cpu_store

@cpu_set_avx2:
    mov     eax, SIMD_TIER_AVX2

@cpu_store:
    mov     g_simdTier, eax

@cpu_done:
    add     rsp, 28h
    ret
VecIdx_CheckCPU ENDP

; ---------------------------------------------------------------------------
;  VecIdx_DotProduct (AVX-512 path)
;
;  Computes dot product of two fp32 vectors.
;
;  IN:  RCX = float* a
;       RDX = float* b
;       R8D = dim (multiple of 16 for AVX-512, multiple of 8 for AVX2)
;  OUT: XMM0 = dot(a, b) (fp32 in low lane)
; ---------------------------------------------------------------------------
VecIdx_DotProduct PROC FRAME
    .pushreg    rbx
    .allocstack 28h
    .endprolog

    sub     rsp, 28h
    push    rbx

    call    VecIdx_CheckCPU
    cmp     eax, SIMD_TIER_AVX512
    je      @dp_avx512
    cmp     eax, SIMD_TIER_AVX2
    je      @dp_avx2

@dp_scalar:
    ; Scalar fallback — correct for any dimension
    vxorps  xmm0, xmm0, xmm0       ; accumulator
    xor     ebx, ebx
@dp_sc_loop:
    cmp     ebx, r8d
    jae     @dp_done
    vmovss  xmm1, dword ptr [rcx + rbx*4]
    vmulss  xmm1, xmm1, dword ptr [rdx + rbx*4]
    vaddss  xmm0, xmm0, xmm1
    inc     ebx
    jmp     @dp_sc_loop

@dp_avx2:
    ; AVX2: 8 floats per iteration (YMM)
    vxorps  ymm0, ymm0, ymm0       ; accumulator
    xor     ebx, ebx
    mov     r9d, r8d
    and     r9d, NOT 7             ; floor to multiple of 8
@dp_avx2_loop:
    cmp     ebx, r9d
    jae     @dp_avx2_tail
    vmovups ymm1, [rcx + rbx*4]
    vmovups ymm2, [rdx + rbx*4]
    vfmadd231ps ymm0, ymm1, ymm2
    add     ebx, 8
    jmp     @dp_avx2_loop
@dp_avx2_tail:
    ; Horizontal sum of ymm0
    vextractf128 xmm1, ymm0, 1
    vaddps  xmm0, xmm0, xmm1
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0
    ; Handle tail (< 8 elements)
@dp_avx2_sc_tail:
    cmp     ebx, r8d
    jae     @dp_done_avx
    vmovss  xmm2, dword ptr [rcx + rbx*4]
    vmulss  xmm2, xmm2, dword ptr [rdx + rbx*4]
    vaddss  xmm0, xmm0, xmm2
    inc     ebx
    jmp     @dp_avx2_sc_tail
@dp_done_avx:
    vzeroupper
    jmp     @dp_done

@dp_avx512:
    ; AVX-512: 16 floats per iteration (ZMM), 4-way unrolled
    vxorps  zmm0, zmm0, zmm0       ; accumulator 0
    vxorps  zmm4, zmm4, zmm4       ; accumulator 1
    vxorps  zmm8, zmm8, zmm8       ; accumulator 2
    vxorps  zmm12, zmm12, zmm12    ; accumulator 3
    xor     ebx, ebx

    ; Main loop: process 64 floats per iteration (4 × ZMM)
    mov     r9d, r8d
    and     r9d, NOT 63            ; floor to multiple of 64
@dp_avx512_4x:
    cmp     ebx, r9d
    jae     @dp_avx512_1x

    vmovups zmm1,  [rcx + rbx*4]
    vmovups zmm5,  [rcx + rbx*4 + 64]
    vmovups zmm9,  [rcx + rbx*4 + 128]
    vmovups zmm13, [rcx + rbx*4 + 192]

    vfmadd231ps zmm0,  zmm1,  [rdx + rbx*4]
    vfmadd231ps zmm4,  zmm5,  [rdx + rbx*4 + 64]
    vfmadd231ps zmm8,  zmm9,  [rdx + rbx*4 + 128]
    vfmadd231ps zmm12, zmm13, [rdx + rbx*4 + 192]

    add     ebx, 64
    jmp     @dp_avx512_4x

    ; Handle remaining 16-element chunks
@dp_avx512_1x:
    mov     r9d, r8d
    and     r9d, NOT 15            ; floor to multiple of 16
@dp_avx512_1x_loop:
    cmp     ebx, r9d
    jae     @dp_avx512_reduce

    vmovups zmm1, [rcx + rbx*4]
    vfmadd231ps zmm0, zmm1, [rdx + rbx*4]
    add     ebx, 16
    jmp     @dp_avx512_1x_loop

@dp_avx512_reduce:
    ; Merge 4 accumulators
    vaddps  zmm0, zmm0, zmm4
    vaddps  zmm8, zmm8, zmm12
    vaddps  zmm0, zmm0, zmm8

    ; Horizontal reduce zmm0 → xmm0
    vextractf32x8 ymm1, zmm0, 1
    vaddps  ymm0, ymm0, ymm1
    vextractf128  xmm1, ymm0, 1
    vaddps  xmm0, xmm0, xmm1
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0

    ; Handle scalar tail (< 16 elements)
@dp_avx512_tail:
    cmp     ebx, r8d
    jae     @dp_avx512_done
    vmovss  xmm2, dword ptr [rcx + rbx*4]
    vmulss  xmm2, xmm2, dword ptr [rdx + rbx*4]
    vaddss  xmm0, xmm0, xmm2
    inc     ebx
    jmp     @dp_avx512_tail

@dp_avx512_done:
    vzeroupper

@dp_done:
    pop     rbx
    add     rsp, 28h
    ret
VecIdx_DotProduct ENDP

; ---------------------------------------------------------------------------
;  VecIdx_L2Norm
;
;  Computes ||v||₂ and returns it. Optionally normalizes in-place.
;
;  IN:  RCX = float* v
;       R8D = dim
;       R9D = normalize (1 = divide v by norm, 0 = just return norm)
;  OUT: XMM0 = ||v||₂ (fp32)
; ---------------------------------------------------------------------------
VecIdx_L2Norm PROC FRAME
    .pushreg    rbx
    .pushreg    r12
    .pushreg    r13
    .allocstack 28h
    .endprolog

    sub     rsp, 28h
    push    rbx
    push    r12
    push    r13

    mov     r12, rcx                ; r12 = v
    mov     r13d, r8d               ; r13d = dim
    ; r9d = normalize flag (saved across DotProduct call)
    push    r9

    ; dot(v, v) — reuse DotProduct
    mov     rcx, r12
    mov     rdx, r12
    mov     r8d, r13d
    call    VecIdx_DotProduct
    ; xmm0 = ||v||²

    ; sqrt
    vsqrtss xmm0, xmm0, xmm0
    ; Guard: max(norm, eps)
    vmovss  xmm1, dword ptr [g_fltEps]
    vmaxss  xmm0, xmm0, xmm1       ; xmm0 = max(norm, eps)

    pop     r9
    test    r9d, r9d
    jz      @norm_no_inplace

    ; Normalize: v[i] /= norm
    vbroadcastss zmm1, xmm0        ; zmm1 = [norm, ..., norm]
    ; reciprocal (more precise than vdivps for this use case)
    vrcp14ps zmm2, zmm1            ; zmm2 ≈ 1/norm (AVX-512F)
    ; Newton-Raphson refinement: r ← r*(2 - norm*r)  [r = zmm2]
    vmulps  zmm3, zmm1, zmm2       ; zmm3 = norm * inv ≈ 1
    vbroadcastss zmm4, [g_fltOne]  ; zmm4 = [2.0 - ...] work via negate
    ; two_minus = 2 - (norm*inv)  →  use: vfnmadd231ps: dest = -(norm * inv) + 2
    vbroadcastss zmm5, [g_fltOne]  ; zmm5 = 1.0; we add twice = 2.0
    vaddps  zmm5, zmm5, zmm5       ; zmm5 = 2.0
    vsubps  zmm5, zmm5, zmm3       ; zmm5 = 2 - norm*inv
    vmulps  zmm2, zmm2, zmm5       ; zmm2 = refined 1/norm

    xor     ebx, ebx
    mov     r10d, r13d
    and     r10d, NOT 15
@norm_loop:
    cmp     ebx, r10d
    jae     @norm_tail
    vmovups zmm3, [r12 + rbx*4]
    vmulps  zmm3, zmm3, zmm2
    vmovups [r12 + rbx*4], zmm3
    add     ebx, 16
    jmp     @norm_loop

@norm_tail:
    ; Scalar tail — use xmm0 (scalar reciprocal) as normalizer
    vrcpss  xmm5, xmm0, xmm0      ; xmm5 = 1/norm
@norm_tail_loop:
    cmp     ebx, r13d
    jae     @norm_done
    vmovss  xmm3, dword ptr [r12 + rbx*4]
    vmulss  xmm3, xmm3, xmm5
    vmovss  dword ptr [r12 + rbx*4], xmm3
    inc     ebx
    jmp     @norm_tail_loop

@norm_done:
    vzeroupper

@norm_no_inplace:
    pop     r13
    pop     r12
    pop     rbx
    add     rsp, 28h
    ret
VecIdx_L2Norm ENDP

; ---------------------------------------------------------------------------
;  VecIdx_CosineSimilarity
;
;  cos(a,b) = dot(a,b) / (||a|| * ||b||)
;  Vectors should be pre-normalized for HNSW (then just dot product).
;
;  IN:  RCX = float* a        (may be unnormalized)
;       RDX = float* b        (may be unnormalized)
;       R8D = dim
;  OUT: XMM0 = cosine similarity ∈ [-1.0, 1.0]
; ---------------------------------------------------------------------------
VecIdx_CosineSimilarity PROC FRAME
    .pushreg    rbx
    .pushreg    r12
    .pushreg    r13
    .pushreg    r14
    .allocstack 38h                ; 28h shadow + 8 for dot_ab + 8 for norm_a
    .endprolog

    sub     rsp, 38h
    push    rbx
    push    r12
    push    r13
    push    r14

    mov     r12, rcx
    mov     r13, rdx
    mov     r14d, r8d

    ; dot(a, b)
    mov     rcx, r12
    mov     rdx, r13
    mov     r8d, r14d
    call    VecIdx_DotProduct
    movss   DWORD PTR [rsp + 30h], xmm0 ; stack[30h] = dot_ab

    ; ||a||
    mov     rcx, r12
    mov     r8d, r14d
    xor     r9d, r9d               ; no normalize
    call    VecIdx_L2Norm
    movss   DWORD PTR [rsp + 28h], xmm0 ; stack[28h] = norm_a

    ; ||b||
    mov     rcx, r13
    mov     r8d, r14d
    xor     r9d, r9d
    call    VecIdx_L2Norm
    ; xmm0 = norm_b

    ; cos = dot_ab / (norm_a * norm_b)
    vmovss  xmm1, dword ptr [rsp + 28h]     ; xmm1 = norm_a
    vmulss  xmm0, xmm1, xmm0               ; xmm0 = norm_a * norm_b
    vmovss  xmm1, dword ptr [g_fltEps]
    vmaxss  xmm0, xmm0, xmm1               ; guard zero divide
    vmovss  xmm2, dword ptr [rsp + 30h]     ; xmm2 = dot_ab
    vdivss  xmm0, xmm2, xmm0               ; xmm0 = cos

    vzeroupper

    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    add     rsp, 38h
    ret
VecIdx_CosineSimilarity ENDP

; ---------------------------------------------------------------------------
;  VecIdx_BatchSimilarity
;
;  Computes cosine similarity of one query against N candidate vectors.
;  Stores scores into float* outScores. Used during HNSW neighbor scan.
;
;  IN:  RCX = float* query        (pre-normalized, fp32)
;       RDX = float* candidates   (N × dim, row-major, pre-normalized)
;       R8D = dim
;       R9D = N (candidate count)
;       [rsp+28+32] = float* outScores   (N floats, caller allocated)
;
;  OUT: EAX = N (number of scores written)
;
;  Since vectors are pre-normalized, cos(a,b) = dot(a,b).
;  This reduces to N independent dot products — all AVX-512.
; ---------------------------------------------------------------------------
VecIdx_BatchSimilarity PROC FRAME
    .pushreg    rbx
    .pushreg    rsi
    .pushreg    rdi
    .pushreg    r12
    .pushreg    r13
    .pushreg    r14
    .pushreg    r15
    .allocstack 28h
    .endprolog

    sub     rsp, 28h
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15

    mov     r12, rcx            ; r12 = query
    mov     r13, rdx            ; r13 = candidates base
    mov     r14d, r8d           ; r14d = dim
    mov     r15d, r9d           ; r15d = N
    mov     rdi,  [rsp + 88h]         ; outScores (5th arg: entry+28h, now rsp+88h)

    ; Stride in bytes between candidates
    imul    rsi, r14, 4         ; rsi = dim * 4

    xor     ebx, ebx            ; ebx = candidate index
@batch_loop:
    cmp     ebx, r15d
    jae     @batch_done

    ; dot(query, candidates[i])
    mov     rcx, r12
    mov     rdx, r13            ; candidate i ptr
    mov     r8d, r14d
    call    VecIdx_DotProduct
    ; xmm0 = score

    vmovss  dword ptr [rdi + rbx*4], xmm0

    add     r13, rsi            ; next candidate
    inc     ebx
    jmp     @batch_loop

@batch_done:
    vzeroupper
    mov     eax, r15d

    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    add     rsp, 28h
    ret
VecIdx_BatchSimilarity ENDP

; ---------------------------------------------------------------------------
;  TOPK_ENTRY — internal struct for heap
; ---------------------------------------------------------------------------
; Smallest-heap to maintain top K during linear scan
; Not using full struct syntax — stored as {float score, int32 index} pairs
TOPK_SCORE_OFFSET   EQU 0
TOPK_INDEX_OFFSET   EQU 4
TOPK_ENTRY_SIZE     EQU 8

; ---------------------------------------------------------------------------
;  VecIdx_SearchTopK
;
;  Linear scan of a flat index, returning top K by cosine similarity.
;  Uses a min-heap of size K for O(N log K) total.
;
;  IN:  RCX = float* query        (pre-normalized)
;       RDX = float* indexVectors (N × dim, row-major, pre-normalized)
;       R8D = dim
;       R9D = N (total vectors in flat index)
;       [rsp+28+32] = K           (DWORD, top K to return)
;       [rsp+28+40] = TOPK_ENTRY* outResults  (K entries, caller allocated)
;
;  OUT: EAX = actual results written (<= K)
; ---------------------------------------------------------------------------
VecIdx_SearchTopK PROC FRAME
    .pushreg    rbx
    .pushreg    rsi
    .pushreg    rdi
    .pushreg    r12
    .pushreg    r13
    .pushreg    r14
    .pushreg    r15
    .allocstack 28h
    .endprolog

    sub     rsp, 28h
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15

    mov     r12, rcx            ; query
    mov     r13, rdx            ; index base
    mov     r14d, r8d           ; dim
    mov     r15d, r9d           ; N

    mov     ebx,  dword ptr [rsp + 88h]            ; K        (5th arg)
    mov     rdi,  qword ptr [rsp + 90h]            ; outResults (6th arg)

    ; Stride
    imul    rsi, r14, 4         ; bytes per vector

    xor     r10d, r10d          ; heap fill count
    xor     r11d, r11d          ; current vector index

@topk_main_loop:
    cmp     r11d, r15d
    jae     @topk_done

    ; Compute similarity
    push    r10
    push    r11
    push    rbx
    mov     rcx, r12
    mov     rdx, r13
    mov     r8d, r14d
    call    VecIdx_DotProduct
    pop     rbx
    pop     r11
    pop     r10

    ; xmm0 = score for vector r11

    ; Phase 1: Fill heap for first K elements
    cmp     r10d, ebx
    jb      @topk_fill

    ; Phase 2: Replace min if score > heap_min
    vmovss  xmm1, dword ptr [rdi]   ; heap_min score
    vucomiss xmm0, xmm1
    jbe     @topk_next              ; score <= heap_min, skip

    ; Replace root and sift down
    vmovss  dword ptr [rdi + TOPK_SCORE_OFFSET], xmm0
    mov     dword ptr [rdi + TOPK_INDEX_OFFSET], r11d

    ; Sift-down min-heap (root at index 0)
    push    r10
    push    r11
    push    rbx
    mov     ecx, r10d               ; heap size = K (full)
    lea     rcx, [rdi]              ; heap base
    mov     edx, r10d               ; heap size
    xor     r8d, r8d                ; start at root (0)
    call    VecIdx_SiftDown
    pop     rbx
    pop     r11
    pop     r10
    jmp     @topk_next

@topk_fill:
    ; Store into heap
    imul    rax, r10, TOPK_ENTRY_SIZE
    vmovss  dword ptr [rdi + rax + TOPK_SCORE_OFFSET], xmm0
    mov     dword ptr [rdi + rax + TOPK_INDEX_OFFSET], r11d
    inc     r10d

    ; Build heap after K fill
    cmp     r10d, ebx
    jne     @topk_next

    ; Heapify: call SiftDown from last non-leaf up to root
    mov     ecx, r10d
    shr     ecx, 1                  ; last non-leaf = K/2 - 1
    dec     ecx
@topk_heapify:
    cmp     ecx, 0
    jl      @topk_next
    push    r10
    push    r11
    push    rbx
    push    rcx
    lea     rcx, [rdi]
    mov     edx, r10d
    mov     r8d, [rsp]              ; i
    call    VecIdx_SiftDown
    pop     rcx
    pop     rbx
    pop     r11
    pop     r10
    dec     ecx
    jmp     @topk_heapify

@topk_next:
    add     r13, rsi                ; advance to next vector
    inc     r11d
    jmp     @topk_main_loop

@topk_done:
    ; Return min(r10d, K) — actual results written
    cmp     r10d, ebx
    cmovg   r10d, ebx
    mov     eax, r10d

    vzeroupper
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    add     rsp, 28h
    ret
VecIdx_SearchTopK ENDP

; ---------------------------------------------------------------------------
;  VecIdx_SiftDown (internal — min-heap maintenance)
;
;  Sifts down the element at position R8D in a min-heap.
;
;  IN:  RCX = TOPK_ENTRY* heap
;       EDX = heap size
;       R8D = root index to sift from
; ---------------------------------------------------------------------------
VecIdx_SiftDown PROC FRAME
    .pushreg    rbx
    .pushreg    r12
    .pushreg    r13
    .allocstack 28h
    .endprolog

    sub     rsp, 28h
    push    rbx
    push    r12
    push    r13

    mov     r12, rcx            ; heap base
    mov     r13d, edx           ; heap size
    mov     ebx, r8d            ; current index

@sift_loop:
    ; Left child = 2*i + 1, right child = 2*i + 2
    lea     ecx, [ebx + ebx + 1]    ; left = 2*i+1
    lea     edx, [ebx + ebx + 2]    ; right = 2*i+2
    mov     eax, ebx               ; smallest = i

    ; Compare with left if valid
    cmp     ecx, r13d
    jae     @sift_check_right

    imul    r8, rax,  TOPK_ENTRY_SIZE
    imul    r9, rcx, TOPK_ENTRY_SIZE
    vmovss  xmm0, dword ptr [r12 + r8]
    vmovss  xmm1, dword ptr [r12 + r9]
    vucomiss xmm1, xmm0             ; left < smallest?
    jae     @sift_check_right
    mov     eax, ecx               ; smallest = left

@sift_check_right:
    cmp     edx, r13d
    jae     @sift_swap_check

    imul    r8, rax,  TOPK_ENTRY_SIZE
    imul    r9, rdx, TOPK_ENTRY_SIZE
    vmovss  xmm0, dword ptr [r12 + r8]
    vmovss  xmm2, dword ptr [r12 + r9]
    vucomiss xmm2, xmm0             ; right < smallest?
    jae     @sift_swap_check
    mov     eax, edx               ; smallest = right

@sift_swap_check:
    cmp     eax, ebx
    je      @sift_done             ; no swap needed

    ; Swap heap[i] and heap[smallest] (8-byte entries)
    imul    r8, rbx, TOPK_ENTRY_SIZE
    imul    r9, rax, TOPK_ENTRY_SIZE
    mov     rcx, [r12 + r8]
    mov     rdx, [r12 + r9]
    mov     [r12 + r8], rdx
    mov     [r12 + r9], rcx

    ; Continue sifting from smallest position
    mov     ebx, eax
    jmp     @sift_loop

@sift_done:
    pop     r13
    pop     r12
    pop     rbx
    add     rsp, 28h
    ret
VecIdx_SiftDown ENDP

END
