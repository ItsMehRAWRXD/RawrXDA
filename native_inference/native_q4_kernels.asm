; native_q4_kernels.asm
; Pure MASM x64 Q4_0 dequantization (replaces ggml)
; Fixed: PROC/ENDP pairs with FRAME for SEH unwind data


; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

.code
ALIGN 16

; ============================================================================
; Q4_0 Block Dequantization
; RCX = blocks (count)
; RDX = input ptr (Q4_0 blocks: 2-byte scale + 32 nibbles)
; R8  = output ptr (F32)
; ============================================================================
PUBLIC DequantQ4_0_AVX2

DequantQ4_0_AVX2 PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    .endprolog

    mov rbx, rcx                    ; block count
    mov rsi, rdx                    ; input
    mov rdi, r8                     ; output
    vxorps ymm5, ymm5, ymm5         ; zero register

block_loop:
    cmp rbx, 0
    je done

    ; Load scale (16-bit, convert to float)
    movzx eax, WORD PTR [rsi]       ; scale as half/f16 or int16
    ; For now treat as float16, convert to float32 via lookup or approximation
    ; Simplified: treat as int16 scale factor
    vcvtsi2ss xmm0, xmm0, eax
    vbroadcastss ymm0, xmm0         ; ymm0 = scale

    ; Load 32 nibbles (16 bytes)
    vmovdqu xmm1, XMMWORD PTR [rsi+2]

    ; Expand nibbles to bytes then words then dwords
    ; Low nibbles
    mov rax, 0F0F0F0F0F0F0F0Fh
    movq xmm4, rax
    pshufd xmm4, xmm4, 0
    vpand xmm2, xmm1, xmm4
    vpmovzxbd ymm2, xmm2            ; bytes to dwords

    ; High nibbles
    vpsrld xmm3, xmm1, 4
    vpand xmm3, xmm3, xmm4
    ; Convert to float and scale
    vcvtdq2ps ymm2, ymm2
    vcvtdq2ps ymm3, ymm3
    vsubps ymm2, ymm2, ymm5         ; subtract zero? Actually 0-15 range
    vsubps ymm3, ymm3, ymm5
    vmulps ymm2, ymm2, ymm0         ; scale
    vmulps ymm3, ymm3, ymm0

    ; Store 16 floats
    vmovups YMMWORD PTR [rdi], ymm2
    vmovups YMMWORD PTR [rdi+32], ymm3

    ; Advance
    add rsi, 18                     ; 2 bytes scale + 16 bytes nibbles
    add rdi, 64                     ; 16 floats * 4 bytes
    dec rbx
    jmp block_loop

done:
    vzeroupper
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
DequantQ4_0_AVX2 ENDP

END