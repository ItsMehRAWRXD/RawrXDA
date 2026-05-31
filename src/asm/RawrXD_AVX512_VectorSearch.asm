; ==============================================================================
; RawrXD-AVX512-VectorSearch.asm - Optimized Similarity Search
; ==============================================================================
; Highly optimized cosine similarity and dot product for vector indexing.
; Surpasses Cursor/Windsurf (Python/Node) by using raw metal performance.
; Hand-written for the Moat Feature (Phase 15+ Integration)
; ==============================================================================

PUBLIC cosine_similarity_avx512

.code

; float cosine_similarity_avx512(const float* a, const float* b, size_t n)
; rcx = a (ptr), rdx = b (ptr), r8 = n (size)
; Returns similarity in xmm0

cosine_similarity_avx512 PROC
    push rbp
    mov rbp, rsp

    ; Initial state: sum_ab=0, sum_a2=0, sum_b2=0
    vpxord zmm0, zmm0, zmm0      ; sum_ab
    vpxord zmm1, zmm1, zmm1      ; sum_a2
    vpxord zmm2, zmm2, zmm2      ; sum_b2
    
    xor rax, rax                 ; index

    ; Main loop: 16 floats per iteration (AVX-512 = 512 bits = 16 x float32)
    mov r9, r8
    and r9, 0FFFFFFFFFFFFFFF0h   ; r9 = n & ~15 (aligned count)

vector_loop:
    cmp rax, r9
    jge scalar_tail
    
    vmovups zmm3, [rcx + rax*4]  ; Load 16 floats from a
    vmovups zmm4, [rdx + rax*4]  ; Load 16 floats from b
    
    vfmadd231ps zmm0, zmm3, zmm4 ; sum_ab += a * b
    vfmadd231ps zmm1, zmm3, zmm3 ; sum_a2 += a * a
    vfmadd231ps zmm2, zmm4, zmm4 ; sum_b2 += b * b
    
    add rax, 16
    jmp vector_loop

scalar_tail:
    ; Handle remaining elements (n % 16) with scalar SSE
    cmp rax, r8
    jge reduction
    vmovss xmm3, dword ptr [rcx + rax*4]
    vmovss xmm4, dword ptr [rdx + rax*4]
    vfmadd231ss xmm0, xmm3, xmm4
    vfmadd231ss xmm1, xmm3, xmm3
    vfmadd231ss xmm2, xmm4, xmm4
    inc rax
    jmp scalar_tail

reduction:
    ; Horizontal reduction for sum_ab (zmm0 -> xmm0[0])
    vextractf32x8 ymm3, zmm0, 1
    vaddps ymm0, ymm0, ymm3
    vextractf128 xmm3, ymm0, 1
    vaddps xmm0, xmm0, xmm3
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0     ; xmm0[0] = dot_ab

    ; Horizontal reduction for sum_a2 (zmm1 -> xmm1[0])
    vextractf32x8 ymm3, zmm1, 1
    vaddps ymm1, ymm1, ymm3
    vextractf128 xmm3, ymm1, 1
    vaddps xmm1, xmm1, xmm3
    vhaddps xmm1, xmm1, xmm1
    vhaddps xmm1, xmm1, xmm1     ; xmm1[0] = sum_a2

    ; Horizontal reduction for sum_b2 (zmm2 -> xmm2[0])
    vextractf32x8 ymm3, zmm2, 1
    vaddps ymm2, ymm2, ymm3
    vextractf128 xmm3, ymm2, 1
    vaddps xmm2, xmm2, xmm3
    vhaddps xmm2, xmm2, xmm2
    vhaddps xmm2, xmm2, xmm2     ; xmm2[0] = sum_b2

    ; result = dot_ab / sqrt(sum_a2 * sum_b2)
    vmulss xmm1, xmm1, xmm2     ; xmm1 = sum_a2 * sum_b2
    vsqrtss xmm1, xmm1, xmm1    ; xmm1 = sqrt(sum_a2 * sum_b2)

    ; Guard: if denominator is zero, return 0.0
    vxorps xmm5, xmm5, xmm5
    vucomiss xmm1, xmm5
    je return_zero

    vdivss xmm0, xmm0, xmm1     ; xmm0 = dot_ab / sqrt(sum_a2 * sum_b2)
    jmp done

return_zero:
    vxorps xmm0, xmm0, xmm0

done:
    vzeroupper
    pop rbp
    ret
cosine_similarity_avx512 ENDP

END
