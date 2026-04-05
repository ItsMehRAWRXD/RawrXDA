; RawrXD_Enhancement3_ZeroStoreDequant.asm
; Zero-store style dequant flow kept in registers.

.CODE
Enhancement3_ZeroStoreDequantQ4 PROC FRAME
    ; rcx = packed input
    ; rdx = output zmm scratch pointer
    ; r8  = block count
    push    rsi
    .pushreg rsi
    .endprolog

    mov     rsi, rcx
    xor     r9, r9

L3_loop:
    cmp     r9, r8
    jae     L3_done

    mov     r10, r9
    shl     r10, 6
    vmovdqu8 zmm0, zmmword ptr [rsi + r10]
    vpmovzxbd zmm1, xmm0
    vcvtdq2ps zmm2, zmm1
    vmovups zmmword ptr [rdx], zmm2

    inc     r9
    jmp     L3_loop

L3_done:
    vzeroupper
    pop     rsi
    ret
Enhancement3_ZeroStoreDequantQ4 ENDP

END
