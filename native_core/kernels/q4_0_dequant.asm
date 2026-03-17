; native_core/kernels/q4_0_dequant.asm
; Q4_0 Dequantization: 32 weights (4-bit) + 2-byte scale -> 32 floats
; Zero external deps — pure AVX2 MASM64


; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

.data
ALIGN 16
low_nibble_mask BYTE 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh
                BYTE 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh

; Q4_0 offset constant: subtract 8 to center signed range [-8..7]
q4_offset_f32   REAL4 -8.0

.code

PUBLIC Q4_0_DequantBlock_AVX2
PUBLIC Q4_0_DequantBatch_AVX2

; ============================================================================
; Q4_0_DequantBlock_AVX2 — Dequantize one Q4_0 block (32 weights)
; RCX = input ptr  (Q4_0 block: 2 bytes scale_f16, 16 bytes nibbles = 18 bytes)
; RDX = output ptr (32 x float = 128 bytes)
; ============================================================================
Q4_0_DequantBlock_AVX2 PROC FRAME
    push        rbp
    .pushreg    rbp
    mov         rbp, rsp
    .setframe   rbp, 0
    sub         rsp, 32
    .allocstack 32
    .endprolog

    ; --- Load scale (f16 at [rcx], 2 bytes) and convert to f32 ---
    ; ml64 does not support vcvtph2ps from memory WORD PTR directly
    ; Load the 2-byte f16 into a dword register, then use vcvtph2ps
    movzx       eax, word ptr [rcx]
    movd        xmm0, eax
    vcvtph2ps   xmm0, xmm0              ; xmm0[0] = scale as f32
    vbroadcastss ymm0, xmm0             ; ymm0 = scale broadcast

    ; --- Load offset (-8.0) for centering signed nibbles ---
    vbroadcastss ymm5, dword ptr [q4_offset_f32]

    ; --- Load 16 bytes of nibble data (32 x 4-bit values) ---
    vmovdqu     xmm1, xmmword ptr [rcx+2]

    ; --- Extract low nibbles (bits 0..3 of each byte) ---
    vpand       xmm2, xmm1, xmmword ptr [low_nibble_mask]

    ; --- Extract high nibbles (bits 4..7 of each byte) ---
    vpsrlw      xmm3, xmm1, 4
    vpand       xmm3, xmm3, xmmword ptr [low_nibble_mask]

    ; --- Interleave: low/high nibble pairs -> 16 bytes each half ---
    vpunpcklbw  xmm4, xmm2, xmm3       ; bytes 0..7: interleaved low+high
    vpunpckhbw  xmm6, xmm2, xmm3       ; bytes 8..15: interleaved low+high

    ; --- Convert first 8 values to dwords then floats ---
    vpmovzxbd   ymm1, xmm4
    vcvtdq2ps   ymm1, ymm1
    vaddps      ymm1, ymm1, ymm5        ; center: val - 8
    vmulps      ymm1, ymm1, ymm0        ; scale
    vmovups     ymmword ptr [rdx], ymm1

    ; --- Convert second 8 values ---
    ; Shift xmm4 right to get upper 8 bytes
    vpsrldq     xmm7, xmm4, 8
    vpmovzxbd   ymm1, xmm7
    vcvtdq2ps   ymm1, ymm1
    vaddps      ymm1, ymm1, ymm5
    vmulps      ymm1, ymm1, ymm0
    vmovups     ymmword ptr [rdx+32], ymm1

    ; --- Convert third 8 values ---
    vpmovzxbd   ymm1, xmm6
    vcvtdq2ps   ymm1, ymm1
    vaddps      ymm1, ymm1, ymm5
    vmulps      ymm1, ymm1, ymm0
    vmovups     ymmword ptr [rdx+64], ymm1

    ; --- Convert fourth 8 values ---
    vpsrldq     xmm7, xmm6, 8
    vpmovzxbd   ymm1, xmm7
    vcvtdq2ps   ymm1, ymm1
    vaddps      ymm1, ymm1, ymm5
    vmulps      ymm1, ymm1, ymm0
    vmovups     ymmword ptr [rdx+96], ymm1

    vzeroupper
    add         rsp, 32
    pop         rbp
    ret
Q4_0_DequantBlock_AVX2 ENDP

; ============================================================================
; Q4_0_DequantBatch_AVX2 — Dequantize N blocks sequentially
; RCX = input ptr  (array of Q4_0 blocks, 18 bytes each)
; RDX = output ptr (float array)
; R8  = block count
; ============================================================================
Q4_0_DequantBatch_AVX2 PROC FRAME
    push        rbp
    .pushreg    rbp
    mov         rbp, rsp
    .setframe   rbp, 0
    push        rbx
    .pushreg    rbx
    push        rsi
    .pushreg    rsi
    push        rdi
    .pushreg    rdi
    sub         rsp, 32
    .allocstack 32
    .endprolog

    mov         rsi, rcx                 ; src
    mov         rdi, rdx                 ; dst
    mov         rbx, r8                  ; count

    test        rbx, rbx
    jz          batch_done

batch_loop:
    mov         rcx, rsi
    mov         rdx, rdi
    call        Q4_0_DequantBlock_AVX2

    add         rsi, 18                  ; next Q4_0 block (2 + 16)
    add         rdi, 128                 ; next 32 floats (32 * 4)
    dec         rbx
    jnz         batch_loop

batch_done:
    vzeroupper
    add         rsp, 32
    pop         rdi
    pop         rsi
    pop         rbx
    pop         rbp
    ret
Q4_0_DequantBatch_AVX2 ENDP

END