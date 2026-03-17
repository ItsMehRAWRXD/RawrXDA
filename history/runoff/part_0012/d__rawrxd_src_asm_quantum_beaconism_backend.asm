; =============================================================================
; quantum_beaconism_backend.asm — MASM x64 kernels for beacon fusion backend
; =============================================================================
; Exports:
;   qb_masm_normalize_qubit(float* alpha, float* beta)
;   qb_masm_weighted_fitness(const float* values, const float* weights, uint32_t count)
;   qb_masm_entangle_pair(float* betaA, float* betaB, float strength, uint32_t antiCorrelated)
;   qb_masm_abs_dot2_ptr(const float* lhs2, const float* rhs2)
; =============================================================================

PUBLIC qb_masm_normalize_qubit
PUBLIC qb_masm_weighted_fitness
PUBLIC qb_masm_entangle_pair
PUBLIC qb_masm_abs_dot2_ptr

.data
ALIGN 16
qb_one          REAL4 1.0
qb_zero         REAL4 0.0
qb_point_one    REAL4 0.1
qb_inv_sqrt2    REAL4 0.70710678
qb_abs_mask     DWORD 07FFFFFFFh,07FFFFFFFh,07FFFFFFFh,07FFFFFFFh

.code

; void qb_masm_normalize_qubit(float* alpha, float* beta)
; RCX = alpha, RDX = beta
qb_masm_normalize_qubit PROC FRAME
    .endprolog

    test    rcx, rcx
    jz      short @@done
    test    rdx, rdx
    jz      short @@done

    movss   xmm0, DWORD PTR [rcx]
    movss   xmm1, DWORD PTR [rdx]
    mulss   xmm0, xmm0
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    sqrtss  xmm0, xmm0

    movss   xmm1, DWORD PTR [qb_zero]
    ucomiss xmm0, xmm1
    jbe     short @@fallback

    movss   xmm1, DWORD PTR [rcx]
    divss   xmm1, xmm0
    movss   DWORD PTR [rcx], xmm1

    movss   xmm1, DWORD PTR [rdx]
    divss   xmm1, xmm0
    movss   DWORD PTR [rdx], xmm1
    jmp     short @@done

@@fallback:
    movss   xmm0, DWORD PTR [qb_inv_sqrt2]
    movss   DWORD PTR [rcx], xmm0
    movss   DWORD PTR [rdx], xmm0

@@done:
    ret
qb_masm_normalize_qubit ENDP

; float qb_masm_weighted_fitness(const float* values, const float* weights, uint32_t count)
; RCX = values, RDX = weights, R8D = count
; Returns XMM0 = weighted average
qb_masm_weighted_fitness PROC FRAME
    .endprolog

    xorps   xmm0, xmm0    ; weighted sum
    xorps   xmm1, xmm1    ; weight sum

    test    rcx, rcx
    jz      short @@ret_zero
    test    rdx, rdx
    jz      short @@ret_zero
    test    r8d, r8d
    jz      short @@ret_zero

    xor     r9d, r9d
@@loop:
    cmp     r9d, r8d
    jae     short @@finish

    movss   xmm2, DWORD PTR [rcx + r9*4]
    movss   xmm3, DWORD PTR [rdx + r9*4]
    mulss   xmm2, xmm3
    addss   xmm0, xmm2
    addss   xmm1, xmm3

    inc     r9d
    jmp     short @@loop

@@finish:
    movss   xmm2, DWORD PTR [qb_zero]
    ucomiss xmm1, xmm2
    jbe     short @@ret_zero

    divss   xmm0, xmm1
    ret

@@ret_zero:
    xorps   xmm0, xmm0
    ret
qb_masm_weighted_fitness ENDP

; void qb_masm_entangle_pair(float* betaA, float* betaB, float strength, uint32_t antiCorrelated)
; RCX = betaA, RDX = betaB, XMM2 = strength, R9D = antiCorrelated
qb_masm_entangle_pair PROC FRAME
    .endprolog

    test    rcx, rcx
    jz      short @@done
    test    rdx, rdx
    jz      short @@done

    movss   xmm0, DWORD PTR [rcx]    ; betaA
    movss   xmm1, DWORD PTR [rdx]    ; betaB

    movss   xmm3, DWORD PTR [qb_point_one]
    mulss   xmm2, xmm3               ; strength *= 0.1

    test    r9d, r9d
    jz      short @@correlated

    movss   xmm4, DWORD PTR [qb_one]
    subss   xmm4, xmm1               ; (1 - betaB)
    subss   xmm4, xmm0               ; (1 - betaB) - betaA
    jmp     short @@delta

@@correlated:
    movss   xmm4, xmm1
    subss   xmm4, xmm0               ; betaB - betaA

@@delta:
    mulss   xmm4, xmm2               ; delta
    addss   xmm0, xmm4               ; betaA += delta
    subss   xmm1, xmm4               ; betaB -= delta

    maxss   xmm0, DWORD PTR [qb_zero]
    minss   xmm0, DWORD PTR [qb_one]
    maxss   xmm1, DWORD PTR [qb_zero]
    minss   xmm1, DWORD PTR [qb_one]

    movss   DWORD PTR [rcx], xmm0
    movss   DWORD PTR [rdx], xmm1

@@done:
    ret
qb_masm_entangle_pair ENDP

; float qb_masm_abs_dot2_ptr(const float* lhs2, const float* rhs2)
; RCX = lhs2[2], RDX = rhs2[2]
; Returns XMM0 = abs(lhs2[0]*rhs2[0] + lhs2[1]*rhs2[1])
qb_masm_abs_dot2_ptr PROC FRAME
    .endprolog

    xorps   xmm0, xmm0
    test    rcx, rcx
    jz      short @@done
    test    rdx, rdx
    jz      short @@done

    movss   xmm0, DWORD PTR [rcx]
    movss   xmm1, DWORD PTR [rdx]
    mulss   xmm0, xmm1

    movss   xmm2, DWORD PTR [rcx + 4]
    movss   xmm3, DWORD PTR [rdx + 4]
    mulss   xmm2, xmm3

    addss   xmm0, xmm2
    andps   xmm0, XMMWORD PTR [qb_abs_mask]

@@done:
    ret
qb_masm_abs_dot2_ptr ENDP

END
