; RawrXD_SingularityEnh6_WriteBackEviction.asm
; Enhancement 6: Write-Back Weight Eviction
; Mechanic: Non-temporal write-back path to NVMe staging aperture.

OPTION CASEMAP:NONE

RAWRXD_EVICT_LINE_BYTES           EQU 64

.CODE

Enhancement6_WriteBackEviction PROC FRAME
    ; rcx = dst_aperture
    ; rdx = src_weights
    ; r8  = bytes

    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    mov     rdi, rcx
    mov     rsi, rdx
    mov     rbx, r8

_loop:
    cmp     rbx, RAWRXD_EVICT_LINE_BYTES
    jb      short _fence

    vmovdqu xmm0, xmmword ptr [rsi]
    vmovntdq xmmword ptr [rdi], xmm0
    add     rsi, 16
    add     rdi, 16
    sub     rbx, 16
    jmp     short _loop

_fence:
    sfence
    vzeroupper
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Enhancement6_WriteBackEviction ENDP

END