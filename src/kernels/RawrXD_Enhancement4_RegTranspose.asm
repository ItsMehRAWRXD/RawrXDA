; RawrXD_Enhancement4_RegTranspose.asm
; Register-level unpack transpose primitive.

.CODE
Enhancement4_Transpose16x16 PROC FRAME
    ; rcx = source rows
    ; rdx = destination rows
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    mov     rsi, rcx
    mov     rdi, rdx

    vmovups zmm0, zmmword ptr [rsi]
    vmovups zmm1, zmmword ptr [rsi + 64]
    vunpcklps zmm2, zmm0, zmm1
    vunpckhps zmm3, zmm0, zmm1
    vmovups zmmword ptr [rdi], zmm2
    vmovups zmmword ptr [rdi + 64], zmm3

    vzeroupper
    pop     rdi
    pop     rsi
    ret
Enhancement4_Transpose16x16 ENDP

END
