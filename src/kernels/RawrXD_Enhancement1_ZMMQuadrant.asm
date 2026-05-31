; RawrXD_Enhancement1_ZMMQuadrant.asm
; ZMM quadrant-style fused accumulation lane.

EXTERN rawrxd_emit_heartbeat:PROC

.CODE
Enhancement1_ZMMQuadrantFMA PROC FRAME
    ; rcx = a
    ; rdx = b
    ; r8  = out
    ; r9  = vec count (multiple of 16 floats)
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    rbx ; save rbx for metadata
    .pushreg rbx
    sub     rsp, 32 ; shadow space
    .allocstack 32
    .endprolog

    mov     rsi, rcx
    mov     rdi, rdx
    xor     r10, r10
    vxorps  zmm0, zmm0, zmm0

    ; Emit entry heartbeat
    call    rawrxd_emit_heartbeat

L1_loop:
    cmp     r10, r9
    jae     L1_done

    vmovups zmm1, zmmword ptr [rsi + r10*4]
    vmovups zmm2, zmmword ptr [rdi + r10*4]
    vfmadd231ps zmm0, zmm1, zmm2

    add     r10, 16
    jmp     L1_loop

L1_done:
    vmovups zmmword ptr [r8], zmm0
    
    ; Emit exit heartbeat
    call    rawrxd_emit_heartbeat

    vzeroupper
    add     rsp, 32
    pop     rbx
    pop     rdi
    pop     rsi
    ret
Enhancement1_ZMMQuadrantFMA ENDP

END
