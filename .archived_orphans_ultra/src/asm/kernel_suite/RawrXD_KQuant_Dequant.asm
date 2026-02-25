; RawrXD_KQuant_Dequant.asm
; Q4_0, Q4_K dequant hot loops. RCX=blocks, RDX=output F32, R8d=n_blocks

.code
OPTION CASEMAP:NONE
align 16

PUBLIC RawrXD_Dequant_Q4_0_AVX2
PUBLIC RawrXD_Dequant_Q4_K_AVX2

RawrXD_Dequant_Q4_0_AVX2 PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 64
    .allocstack 64
    .endprolog

    mov rbx, rcx
    mov r10, rdx
    mov r11d, r8d

    vmovdqa ymm14, ymmword ptr [shuffle_nibble_mask]
    vbroadcastss ymm8, real4 ptr [ymm8_init]
    vmovdqa ymm15, ymmword ptr [mask_0f]

block_loop_q40:
    test r11d, r11d
    jz dequant_q40_done

    vbroadcastss ymm0, real4 ptr [rbx]
    add rbx, 4

    vpmovzxbw ymm1, xmmword ptr [rbx]
    vpsrlw ymm2, ymm1, 4
    vpand ymm1, ymm1, ymm15
    vpand ymm2, ymm2, ymm15

    vpmovzxbd ymm3, xmm1
    vpmovzxbd ymm4, xmm2

    vcvtdq2ps ymm3, ymm3
    vcvtdq2ps ymm4, ymm4

    vsubps ymm3, ymm3, ymm8
    vsubps ymm4, ymm4, ymm8
    vmulps ymm3, ymm3, ymm0
    vmulps ymm4, ymm4, ymm0

    vmovups ymmword ptr [r10], ymm3
    vmovups ymmword ptr [r10+32], ymm4

    add rbx, 16
    add r10, 64
    dec r11d
    jmp block_loop_q40

dequant_q40_done:
    vzeroupper
    add rsp, 64
    pop rbx
    ret
RawrXD_Dequant_Q4_0_AVX2 ENDP

RawrXD_Dequant_Q4_K_AVX2 PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    ; Stub: full Q4_K layout implementation TBD
    xor eax, eax
    pop rbx
    ret
RawrXD_Dequant_Q4_K_AVX2 ENDP

.data
align 32
shuffle_nibble_mask byte 16 dup(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15)
ymm8_init real4 8.0
mask_0f dd 8 dup(0Fh)

END
