; native_core/kernels/q4_0_dequant.asm
; Q4_0 Dequantization: 32 weights (4-bit) + 2-byte scale → 32 floats
; Zero external deps—pure AVX2/AVX-512

.code
ALIGN 64

PUBLIC Q4_0_DequantBlock_AVX512
PUBLIC Q4_0_DequantBlock_AVX2

; Structure: | scale (f16) | zero (f16) | 32x nibbles (16 bytes) |
; Output: 32 floats in ymm/zmm register(s)

; ============================================================================
; AVX-512 Version: Process 64 weights at once (2 blocks interleaved)
; RCX = input ptr (Q4_0 block)
; RDX = output ptr (32 floats)
; ============================================================================
Q4_0_DequantBlock_AVX512 PROC FRAME
    push        rbx
    .pushreg    rbx
    sub         rsp, 64
    .allocstack 64
    .endprolog

    ; Load scale (half-precision at [rcx])
    vpxor       xmm0, xmm0, xmm0
    vcvtph2ps   xmm0, WORD PTR [rcx]    ; scale in xmm0[0]
    vbroadcastss zmm0, xmm0             ; broadcast to all 16 elements

    ; Load 32 nibbles (16 bytes)
    vmovdqu     xmm1, XMMWORD PTR [rcx+4]  ; Skip 4 bytes (scale+zero)

    ; Expand nibbles to bytes: low nibbles
    vpand       xmm2, xmm1, XMMWORD PTR [low_nibble_mask]

    ; Expand nibbles to bytes: high nibbles (shift right 4)
    vpsrlw      xmm3, xmm1, 4
    vpand       xmm3, xmm3, XMMWORD PTR [low_nibble_mask]

    ; Interleave and convert to 32-bit integers
    vpunpcklbw  xmm1, xmm2, xmm3        ; Interleave low bytes
    vpunpckhbw  xmm4, xmm2, xmm3        ; Interleave high bytes

    ; Convert to 32-bit integers
    vpmovzxbd   ymm1, xmm1              ; Low 8 to dwords
    vpmovzxbd   ymm2, xmm4              ; High 8 to dwords

    ; Convert to float and scale
    vcvtdq2ps   ymm1, ymm1
    vcvtdq2ps   ymm2, ymm2

    vmulps      ymm1, ymm1, ymm0        ; * scale
    vmulps      ymm2, ymm2, ymm0

    ; Store
    vmovups     YMMWORD PTR [rdx], ymm1
    vmovups     YMMWORD PTR [rdx+32], ymm2

    vzeroupper
    add         rsp, 64
    pop         rbx
    ret

Q4_0_DequantBlock_AVX512 ENDP

; ============================================================================
; AVX2 Version: Process 32 weights
; RCX = input ptr
; RDX = output ptr
; ============================================================================
Q4_0_DequantBlock_AVX2 PROC FRAME
    push        rbx
    .pushreg    rbx
    sub         rsp, 40
    .allocstack 40
    .endprolog

    ; Load scale (f16→f32)
    vpxor       xmm0, xmm0, xmm0
    vcvtph2ps   xmm0, WORD PTR [rcx]
    vbroadcastss ymm0, xmm0

    ; Load 16 bytes of nibbles
    vmovdqu     xmm1, [rcx+4]

    ; Extract low nibbles
    vmovdqa     xmm2, xmm1
    vpand       xmm2, xmm2, [low_nibble_mask]

    ; Extract high nibbles
    vpsrlw      xmm3, xmm1, 4
    vpand       xmm3, xmm3, [low_nibble_mask]

    ; Pack into bytes (interleaved)
    vpunpcklbw  xmm1, xmm2, xmm3

    ; Zero-extend to 32-bit
    vpmovzxbd   ymm1, xmm1
    vcvtdq2ps   ymm1, ymm1
    vmulps      ymm1, ymm1, ymm0

    vmovups     [rdx], ymm1

    vzeroupper
    add         rsp, 40
    pop         rbx
    ret

Q4_0_DequantBlock_AVX2 ENDP

.data
ALIGN 64
low_nibble_mask BYTE 16 DUP(0x0F)

END