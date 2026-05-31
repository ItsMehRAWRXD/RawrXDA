; RawrXD_Enhancement7_NonTemporalWrite.asm
; Non-temporal writes with required fencing.

.CODE
Enhancement7_NonTemporalWrite PROC FRAME
    ; rcx = dst
    ; rdx = src
    ; r8  = bytes (multiple of 64)
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    mov     rdi, rcx
    mov     rsi, rdx
    mov     r9, r8

L7_loop:
    cmp     r9, 64
    jb      L7_done

    vmovdqu64 zmm0, zmmword ptr [rsi]
    vmovntdq zmmword ptr [rdi], zmm0

    add     rsi, 64
    add     rdi, 64
    sub     r9, 64
    jmp     L7_loop

L7_done:
    sfence
    vzeroupper
    pop     rdi
    pop     rsi
    ret
Enhancement7_NonTemporalWrite ENDP

END
