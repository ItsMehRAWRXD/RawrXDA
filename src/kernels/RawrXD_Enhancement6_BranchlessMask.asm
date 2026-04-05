; RawrXD_Enhancement6_BranchlessMask.asm
; Branchless causal mask using AVX-512 opmask compare/blend.

.CODE
Enhancement6_CausalMaskOpmask PROC FRAME
    ; rcx = scores
    ; rdx = length (multiple of 16)
    ; r8d = threshold
    push    rsi
    .pushreg rsi
    .endprolog

    mov     rsi, rcx
    xor     r9, r9
    vpbroadcastd zmm2, r8d
    vpxord  zmm3, zmm3, zmm3

L6_loop:
    cmp     r9, rdx
    jae     L6_done

    vmovups zmm0, zmmword ptr [rsi + r9*4]
    vpbroadcastd zmm1, r9d
    vpcmpd  k1, zmm1, zmm2, 6
    vmovaps zmm0 {k1}, zmm3
    vmovups zmmword ptr [rsi + r9*4], zmm0

    add     r9, 16
    jmp     L6_loop

L6_done:
    vzeroupper
    pop     rsi
    ret
Enhancement6_CausalMaskOpmask ENDP

END
