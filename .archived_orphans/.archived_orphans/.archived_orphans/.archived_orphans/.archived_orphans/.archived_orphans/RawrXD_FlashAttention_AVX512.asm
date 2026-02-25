; RawrXD_FlashAttention_AVX512.asm
; Flash Attention v2, tiled, online softmax
; RCX=Q, RDX=K, R8=V, R9=O, [RSP+40]=N, [RSP+48]=d, [RSP+56]=scale

.code
OPTION CASEMAP:NONE
align 16

PUBLIC RawrXD_FlashAttention_AVX512

RawrXD_FlashAttention_AVX512 PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 256
    .allocstack 256
    .endprolog

    mov r10d, [rsp+296]
    mov r11d, [rsp+304]
    vbroadcastss zmm15, real4 ptr [rsp+312]

    xor r12d, r12d
row_tile_loop:
    cmp r12d, r10d
    jge fa_done

    vxorps zmm0, zmm0, zmm0
    vxorps zmm4, zmm4, zmm4

    xor r13d, r13d
kv_loop:
    cmp r13d, r10d
    jge next_row

    vmovaps zmm8, zmmword ptr [rcx+r12*256]
    vmovaps zmm9, zmmword ptr [rdx+r13*256]

    vmulps zmm10, zmm8, zmm9
    vmulps zmm10, zmm10, zmm15

    vmaxps zmm11, zmm0, zmm10
    vsubps zmm12, zmm10, zmm11
    vexp2ps zmm13, zmm12

    vsubps zmm14, zmm0, zmm11
    vexp2ps zmm14, zmm14
    vmulps zmm4, zmm4, zmm14
    vaddps zmm4, zmm4, zmm13

    vmovaps zmm0, zmm11

    vmovaps zmm8, zmmword ptr [r8+r13*256]
    vfmadd231ps zmm1, zmm13, zmm8

    add r13d, 16
    jmp kv_loop

next_row:
    vdivps zmm1, zmm1, zmm4
    vmovaps zmmword ptr [r9+r12*256], zmm1

    add r12d, 16
    jmp row_tile_loop

fa_done:
    vzeroupper
    add rsp, 256
    pop rbp
    ret
RawrXD_FlashAttention_AVX512 ENDP

END
