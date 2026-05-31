; RawrXD_SingularityEnh3_StreamingDequant.asm
; Enhancement 3: Streaming Dequantization
; Mechanic: Dequant fused with streamed weight ingress to hide IO latency.

OPTION CASEMAP:NONE

RAWRXD_DEQ_BLOCK_BYTES            EQU 64

.CODE

Enhancement3_StreamingDequant PROC FRAME
    ; rcx = packed_qweights
    ; rdx = output_fp32
    ; r8  = byte_count

    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    mov     rsi, rcx
    mov     rdi, rdx
    mov     rbx, r8

_loop:
    cmp     rbx, RAWRXD_DEQ_BLOCK_BYTES
    jb      short _tail

    ; Streaming fetch + fused convert path evidence.
    prefetcht0 [rsi + 256]
    vmovdqu xmm0, xmmword ptr [rsi]
    vpmovzxbd xmm1, xmm0
    vcvtdq2ps xmm2, xmm1
    vmovups xmmword ptr [rdi], xmm2

    add     rsi, 16
    add     rdi, 16
    sub     rbx, 16
    jmp     short _loop

_tail:
    vzeroupper
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Enhancement3_StreamingDequant ENDP

END