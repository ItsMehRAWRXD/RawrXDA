; =============================================================================
; RawrXD_VectorIndex.asm - AVX-512 Accelerated Cosine Similarity
; =============================================================================
; Phase 47: AVX-512 Semantic Vector Search
; Production cosine similarity for float32 embeddings.
;
; Exports:
;   Vector_DotProduct(RCX=A, RDX=B, R8=dims) -> XMM0
;   Vector_MagnitudeSq(RCX=V, RDX=dims)       -> XMM0
;   Vector_CosineSimilarity(RCX=A, RDX=B, R8=dims) -> XMM0  range [-1,1]
;   Vector_ComputeSimilarity - legacy alias
;
; dims must be multiple of 16.  Zero denominator returns 0.0.

.DATA
; Use explicit IEEE-754 bit patterns to avoid assembler parsing ambiguity.
VecOne  DWORD 03F800000h
VecNOne DWORD 0BF800000h

.CODE

; ============================================================================
; VectorHSum512 - internal helper, no frame
; zmm0 in -> xmm0 scalar sum out
; ============================================================================
VectorHSum512 PROC
    ; Use only volatile XMM/YMM registers (xmm0-xmm5) to satisfy Win64 ABI.
    vextractf32x8 ymm5, zmm0, 1
    vaddps ymm0, ymm0, ymm5
    vextractf128 xmm5, ymm0, 1
    vaddps xmm0, xmm0, xmm5
    vshufps xmm4, xmm0, xmm0, 4Eh
    vaddps xmm0, xmm0, xmm4
    vshufps xmm4, xmm0, xmm0, 11h
    vaddss xmm0, xmm0, xmm4
    ret
VectorHSum512 ENDP

; ============================================================================
; Vector_DotProduct
; ============================================================================
Vector_DotProduct PROC FRAME
    .PUSHREG rbx
    push rbx
    .ENDPROLOG
    vpxord zmm0, zmm0, zmm0
    xor    ebx, ebx
DP_Loop:
    cmp  ebx, r8d
    jge  DP_Done
    vmovups zmm1, zmmword ptr [rcx + rbx*4]
    vmovups zmm2, zmmword ptr [rdx + rbx*4]
    vfmadd231ps zmm0, zmm1, zmm2
    add  ebx, 16
    jmp  DP_Loop
DP_Done:
    call VectorHSum512
    pop  rbx
    ret
Vector_DotProduct ENDP

; ============================================================================
; Vector_MagnitudeSq
; ============================================================================
Vector_MagnitudeSq PROC FRAME
    .PUSHREG rbx
    push rbx
    .ENDPROLOG
    vpxord zmm0, zmm0, zmm0
    xor    ebx, ebx
MS_Loop:
    cmp  ebx, edx
    jge  MS_Done
    vmovups zmm1, zmmword ptr [rcx + rbx*4]
    vfmadd231ps zmm0, zmm1, zmm1
    add  ebx, 16
    jmp  MS_Loop
MS_Done:
    call VectorHSum512
    pop  rbx
    ret
Vector_MagnitudeSq ENDP

; ============================================================================
; Vector_CosineSimilarity
; ============================================================================
Vector_CosineSimilarity PROC FRAME
    .PUSHREG rbx
    .PUSHREG rsi
    .PUSHREG rdi
    .ALLOCSTACK 48
    push rbx
    push rsi
    push rdi
    sub  rsp, 48
    .ENDPROLOG

    mov  rsi, rcx
    mov  rdi, rdx
    mov  rbx, r8

    ; Compute magnitudes first and keep them in local slots.
    mov  rcx, rsi
    mov  rdx, rbx
    call Vector_MagnitudeSq
    vmovss dword ptr [rsp+36], xmm0     ; mag_a2

    mov  rcx, rdi
    mov  rdx, rbx
    call Vector_MagnitudeSq
    vmovss dword ptr [rsp+40], xmm0     ; mag_b2

    mov  rcx, rsi
    mov  rdx, rdi
    mov  r8,  rbx
    call Vector_DotProduct               ; xmm0 = dot_ab

    vmovss xmm1, dword ptr [rsp+40]
    vmulss xmm1, xmm1, dword ptr [rsp+36]
    vsqrtss xmm2, xmm2, xmm1

    vxorps xmm3, xmm3, xmm3
    vucomiss xmm2, xmm3
    jbe  CS_Zero

    vdivss xmm0, xmm0, xmm2

    mov eax, 03F800000h                ; +1.0f
    movd xmm3, eax
    vucomiss xmm0, xmm3
    jbe  CS_CheckLow
    vmovaps xmm0, xmm3
    jmp  CS_Done
CS_CheckLow:
    mov eax, 0BF800000h                ; -1.0f
    movd xmm3, eax
    vucomiss xmm0, xmm3
    jae  CS_Done
    vmovaps xmm0, xmm3
    jmp  CS_Done
CS_Zero:
    vxorps xmm0, xmm0, xmm0
CS_Done:
    add  rsp, 48
    pop  rdi
    pop  rsi
    pop  rbx
    ret
Vector_CosineSimilarity ENDP

; Legacy 2-arg alias — hardcodes 768 dims then tail-calls CosineSimilarity
; Signature: float Vector_ComputeSimilarity(const float* q, const float* t)
Vector_ComputeSimilarity PROC
    mov r8, 768
    jmp Vector_CosineSimilarity
Vector_ComputeSimilarity ENDP

END
