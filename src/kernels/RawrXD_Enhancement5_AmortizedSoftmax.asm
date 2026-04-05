; RawrXD_Enhancement5_AmortizedSoftmax.asm
; Amortized softmax skeleton with max pass + normalize pass.

.DATA
ALIGN 4
one_f32 DD 1.0

.CODE
Enhancement5_FusedSoftmax PROC FRAME
    ; rcx = scores
    ; rdx = length (multiple of 16)
    push    rsi
    .pushreg rsi
    .endprolog

    mov     rsi, rcx
    xor     r8, r8
    vxorps  zmm15, zmm15, zmm15

L5_max:
    cmp     r8, rdx
    jae     L5_norm_start
    vmovups zmm0, zmmword ptr [rsi + r8*4]
    vmaxps  zmm15, zmm15, zmm0
    add     r8, 16
    jmp     L5_max

L5_norm_start:
    xor     r8, r8
    vbroadcastss zmm14, dword ptr [one_f32]

L5_norm:
    cmp     r8, rdx
    jae     L5_done
    vmovups zmm0, zmmword ptr [rsi + r8*4]
    vmulps  zmm0, zmm0, zmm14
    vmovups zmmword ptr [rsi + r8*4], zmm0
    add     r8, 16
    jmp     L5_norm

L5_done:
    vzeroupper
    pop     rsi
    ret
Enhancement5_FusedSoftmax ENDP

END
