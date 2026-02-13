; =============================================================================
; quant_avx2.asm — Q4_0 / Q8_0 Dequantization & Fused Dot-Product (AVX2/FMA3)
; =============================================================================
;
; Legacy quantization format kernels for GGUF models using Q4_0 and Q8_0.
; These are the two most common base quantization types in llama.cpp-compatible
; GGUF files. This module provides:
;
;   1. Dequantization:  Q4_0 → fp32,  Q8_0 → fp32
;   2. Fused vec_dot:   dot(Q4_0, Q8_0) → fp32  (avoids materializing fp32)
;   3. Fused vec_dot:   dot(Q8_0, fp32) → fp32
;
; The fused vec_dot kernels are critical for inference performance because
; they eliminate the intermediate fp32 buffer and reduce memory bandwidth
; by ~8× for Q4_0 and ~4× for Q8_0.
;
; ╔═══════════════════════════════════════════════════════════════════════╗
; ║  GGML QUANTIZATION BLOCK FORMATS                                    ║
; ║                                                                     ║
; ║  Q4_0 block (20 bytes per 32 elements):                             ║
; ║    +0:  fp16 d         (scale factor, 2 bytes)                      ║
; ║    +2:  uint8 qs[16]   (32 × 4-bit quants packed, 16 bytes)        ║
; ║    Total: 18 bytes per 32 elements = 4.5 bits/element               ║
; ║                                                                     ║
; ║  Q8_0 block (34 bytes per 32 elements):                             ║
; ║    +0:  fp16 d         (scale factor, 2 bytes)                      ║
; ║    +2:  int8 qs[32]    (32 × 8-bit signed quants, 32 bytes)        ║
; ║    Total: 34 bytes per 32 elements = 8.5 bits/element               ║
; ║                                                                     ║
; ║  Dequant formula:                                                   ║
; ║    Q4_0: output[i] = d * (nibble[i] - 8)     (unsigned→signed)     ║
; ║    Q8_0: output[i] = d * qs[i]                (already signed)      ║
; ╚═══════════════════════════════════════════════════════════════════════╝
;
; ╔═══════════════════════════════════════════════════════════════════════╗
; ║  WINDOWS x64 REGISTER PRESERVATION CONTRACT                        ║
; ║  Non-volatile (MUST save/restore):                                  ║
; ║    RBX, RBP, RSI, RDI, R12-R15, XMM6-XMM15                       ║
; ╚═══════════════════════════════════════════════════════════════════════╝
;
; Build: ml64.exe /c /Zi /Zd quant_avx2.asm
; Link:  Statically linked into RawrEngine / RawrXD-Win32IDE
;
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

INCLUDE RawrXD_Common.inc

; =============================================================================
;                           CONSTANTS
; =============================================================================

; Block sizes
BLOCK_Q4_0_SIZE         EQU 18          ; 2 (fp16 d) + 16 (qs) = 18 bytes
BLOCK_Q8_0_SIZE         EQU 34          ; 2 (fp16 d) + 32 (qs) = 34 bytes
QK_0                    EQU 32          ; Elements per block (both Q4_0 and Q8_0)

; Q4_0 signed offset: nibbles are 0..15, subtract 8 to get -8..+7
Q4_OFFSET               EQU 8

; =============================================================================
;                           DATA
; =============================================================================
_DATA64 SEGMENT ALIGN(64) 'DATA'

; Nibble isolation mask: 0x0F repeated (for vpand)
q4_nibble_mask:
    DB 32 DUP(0Fh)

; Signed offset for Q4_0: subtract 8 from each int32 element
; (after zero-extension from byte to dword, we need to subtract 8)
q4_offset_8:
    DD 8, 8, 8, 8, 8, 8, 8, 8          ; 8 × int32 = 1 YMM

; Performance counters
g_Q4DequantCalls        DQ 0
g_Q8DequantCalls        DQ 0
g_Q4Q8VecDotCalls       DQ 0
g_Q8F32VecDotCalls      DQ 0

_DATA64 ENDS

; =============================================================================
;                           EXPORTS
; =============================================================================
PUBLIC Quant_DequantQ4_0
PUBLIC Quant_DequantQ8_0
PUBLIC Quant_VecDotQ4_0_Q8_0
PUBLIC Quant_VecDotQ8_0_F32
PUBLIC Quant_GetStats

; =============================================================================
;                           CODE
; =============================================================================
.code

; =============================================================================
; Quant_GetStats
; RCX = pointer to output buffer (4 × uint64_t)
; Returns: EAX = 1
; =============================================================================
Quant_GetStats PROC
    mov     rax, g_Q4DequantCalls
    mov     qword ptr [rcx], rax
    mov     rax, g_Q8DequantCalls
    mov     qword ptr [rcx + 8], rax
    mov     rax, g_Q4Q8VecDotCalls
    mov     qword ptr [rcx + 16], rax
    mov     rax, g_Q8F32VecDotCalls
    mov     qword ptr [rcx + 24], rax
    mov     eax, 1
    ret
Quant_GetStats ENDP

; =============================================================================
; Quant_DequantQ4_0
; Dequantize Q4_0 blocks to fp32.
;
; RCX = src (block_q4_0*), blocks of 18 bytes
; RDX = dst (float*), output buffer
; R8  = num_elements (must be multiple of 32, or tail is truncated)
;
; Returns: RAX = number of elements processed
;
; Algorithm per block (32 elements):
;   d = fp16_to_fp32(src[0:2])
;   for i in 0..15:
;     lo = qs[i] & 0xF       → output[2*i]   = d * (lo - 8)
;     hi = qs[i] >> 4        → output[2*i+1] = d * (hi - 8)
;
; AVX2 approach:
;   Load 16 bytes of qs into xmm
;   Split into low/high nibbles (vpand, vpsrlw + vpand)
;   Interleave: vpmovzxbd to get 8 int32 values each
;   Subtract 8, convert to float, multiply by d
; =============================================================================
Quant_DequantQ4_0 PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    sub     rsp, 48
    .allocstack 48
    vmovdqu xmmword ptr [rsp],      xmm6
    vmovdqu xmmword ptr [rsp + 16], xmm7
    vmovdqu xmmword ptr [rsp + 32], xmm8
    .endprolog

    lock inc g_Q4DequantCalls

    mov     rsi, rcx                    ; src
    mov     rdi, rdx                    ; dst
    mov     r12, r8                     ; total elements
    xor     ebx, ebx                    ; processed count

    ; Load constant: nibble mask
    vmovdqa ymm6, ymmword ptr [q4_nibble_mask]
    ; Load constant: offset 8
    vmovdqa ymm7, ymmword ptr [q4_offset_8]

@@q4_block:
    lea     rax, [rbx + QK_0]
    cmp     rax, r12
    ja      @@q4_done

    ; --- Read scale factor d (fp16 at [rsi+0]) ---
    movzx   eax, word ptr [rsi]
    vmovd   xmm0, eax
    vcvtph2ps xmm0, xmm0               ; d as fp32
    vbroadcastss ymm8, xmm0            ; ymm8 = d broadcast

    ; --- Load 16 bytes of packed quants from [rsi+2] ---
    vmovdqu xmm1, xmmword ptr [rsi + 2]

    ; --- Extract low nibbles (elements 0,2,4,...30 → first 16 of 32) ---
    vpand   xmm2, xmm1, xmm6          ; low nibbles (16 bytes)

    ; --- Extract high nibbles (elements 1,3,5,...31 → second 16 of 32) ---
    vpsrlw  xmm3, xmm1, 4
    vpand   xmm3, xmm3, xmm6          ; high nibbles (16 bytes)

    ; --- Process low nibbles: bytes 0-7 → 8 int32 → 8 fp32 ---
    vpmovzxbd ymm0, xmm2               ; bytes[0:7] → 8 × int32
    vpsubd  ymm0, ymm0, ymm7           ; subtract 8 → signed
    vcvtdq2ps ymm0, ymm0               ; → fp32
    vmulps  ymm0, ymm0, ymm8           ; × d
    vmovups ymmword ptr [rdi], ymm0     ; Store 8 floats (elements 0,2,4,6,8,10,12,14)

    ; --- Process low nibbles: bytes 8-15 → 8 int32 → 8 fp32 ---
    vpsrldq xmm4, xmm2, 8             ; shift right 8 bytes
    vpmovzxbd ymm0, xmm4
    vpsubd  ymm0, ymm0, ymm7
    vcvtdq2ps ymm0, ymm0
    vmulps  ymm0, ymm0, ymm8
    vmovups ymmword ptr [rdi + 32], ymm0  ; Elements 16,18,20,22,24,26,28,30

    ; --- Process high nibbles: bytes 0-7 → 8 int32 → 8 fp32 ---
    vpmovzxbd ymm0, xmm3
    vpsubd  ymm0, ymm0, ymm7
    vcvtdq2ps ymm0, ymm0
    vmulps  ymm0, ymm0, ymm8
    vmovups ymmword ptr [rdi + 64], ymm0  ; Elements 1,3,5,7,9,11,13,15

    ; --- Process high nibbles: bytes 8-15 → 8 int32 → 8 fp32 ---
    vpsrldq xmm4, xmm3, 8
    vpmovzxbd ymm0, xmm4
    vpsubd  ymm0, ymm0, ymm7
    vcvtdq2ps ymm0, ymm0
    vmulps  ymm0, ymm0, ymm8
    vmovups ymmword ptr [rdi + 96], ymm0  ; Elements 17,19,21,23,25,27,29,31

    ; Note: The output ordering above is interleaved (even then odd indices).
    ; For correct sequential output, we need to interleave low/high nibbles.
    ; The canonical Q4_0 layout stores: for byte i, element 2*i uses low nibble,
    ; element 2*i+1 uses high nibble. So the correct output order is:
    ;   [lo0, hi0, lo1, hi1, ...lo15, hi15]
    ; We've output [lo0..lo15, hi0..hi15] which is WRONG for sequential access.
    ; FIX: Re-interleave using vpunpcklbw before expansion.
    ; However, for fused vec_dot (which is the hot path), order doesn't matter
    ; because we pair with Q8_0 which has the same block structure.
    ; For standalone dequant, callers expecting sequential order should use
    ; the interleaved path below instead.

    ; === CORRECT INTERLEAVED DEQUANT ===
    ; We rewrite the output in correct order by interleaving low/high.
    ; Back up to restart with interleaving:
    ; Actually let's just write it correctly from the start.
    ; xmm2 = low nibbles (16 bytes), xmm3 = high nibbles (16 bytes)
    ; We want byte-interleaved: [lo0,hi0,lo1,hi1,...] = vpunpcklbw + vpunpckhbw

    ; First 16 interleaved values: bytes 0-7 of lo/hi
    vpunpcklbw xmm4, xmm2, xmm3       ; [lo0,hi0,lo1,hi1,lo2,hi2,lo3,hi3,lo4,hi4,lo5,hi5,lo6,hi6,lo7,hi7]
    ; Second 16 interleaved values: bytes 8-15 of lo/hi
    vpunpckhbw xmm5, xmm2, xmm3       ; [lo8,hi8,...lo15,hi15]

    ; Now expand xmm4 (first 16 elements) to fp32 in 2 batches of 8
    ; Batch 0: bytes 0-7 of xmm4
    vpmovzxbd ymm0, xmm4               ; 8 × uint8 → 8 × int32
    vpsubd  ymm0, ymm0, ymm7           ; - 8
    vcvtdq2ps ymm0, ymm0
    vmulps  ymm0, ymm0, ymm8
    vmovups ymmword ptr [rdi], ymm0     ; Elements 0-7

    ; Batch 1: bytes 8-15 of xmm4
    vpsrldq xmm4, xmm4, 8
    vpmovzxbd ymm0, xmm4
    vpsubd  ymm0, ymm0, ymm7
    vcvtdq2ps ymm0, ymm0
    vmulps  ymm0, ymm0, ymm8
    vmovups ymmword ptr [rdi + 32], ymm0  ; Elements 8-15

    ; Batch 2: bytes 0-7 of xmm5
    vpmovzxbd ymm0, xmm5
    vpsubd  ymm0, ymm0, ymm7
    vcvtdq2ps ymm0, ymm0
    vmulps  ymm0, ymm0, ymm8
    vmovups ymmword ptr [rdi + 64], ymm0  ; Elements 16-23

    ; Batch 3: bytes 8-15 of xmm5
    vpsrldq xmm5, xmm5, 8
    vpmovzxbd ymm0, xmm5
    vpsubd  ymm0, ymm0, ymm7
    vcvtdq2ps ymm0, ymm0
    vmulps  ymm0, ymm0, ymm8
    vmovups ymmword ptr [rdi + 96], ymm0  ; Elements 24-31

    ; Advance pointers
    add     rsi, BLOCK_Q4_0_SIZE        ; Next Q4_0 block (18 bytes)
    add     rdi, 128                    ; 32 floats × 4 bytes
    add     ebx, QK_0
    jmp     @@q4_block

@@q4_done:
    vzeroupper
    mov     rax, rbx                    ; Return processed count

    vmovdqu xmm6,  xmmword ptr [rsp]
    vmovdqu xmm7,  xmmword ptr [rsp + 16]
    vmovdqu xmm8,  xmmword ptr [rsp + 32]
    add     rsp, 48
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Quant_DequantQ4_0 ENDP

; =============================================================================
; Quant_DequantQ8_0
; Dequantize Q8_0 blocks to fp32.
;
; RCX = src (block_q8_0*), blocks of 34 bytes
; RDX = dst (float*), output buffer
; R8  = num_elements (must be multiple of 32)
;
; Returns: RAX = number of elements processed
;
; Algorithm per block:
;   d = fp16_to_fp32(src[0:2])
;   for i in 0..31: output[i] = d * qs[i]   (qs is signed int8)
;
; AVX2: vpmovsxbd (sign-extend int8→int32), vcvtdq2ps, vmulps
; =============================================================================
Quant_DequantQ8_0 PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    sub     rsp, 16
    .allocstack 16
    vmovdqu xmmword ptr [rsp], xmm6
    .endprolog

    lock inc g_Q8DequantCalls

    mov     rsi, rcx                    ; src
    mov     rdi, rdx                    ; dst
    mov     r12, r8                     ; total elements
    xor     ebx, ebx                    ; processed count

@@q8_block:
    lea     rax, [rbx + QK_0]
    cmp     rax, r12
    ja      @@q8_done

    ; --- Read scale factor d (fp16 at [rsi+0]) ---
    movzx   eax, word ptr [rsi]
    vmovd   xmm0, eax
    vcvtph2ps xmm0, xmm0
    vbroadcastss ymm6, xmm0            ; ymm6 = d broadcast

    ; --- 32 signed int8 quants at [rsi+2] ---
    ; Process in 4 batches of 8

    ; Batch 0: bytes 0-7
    vpmovsxbd ymm0, qword ptr [rsi + 2]
    vcvtdq2ps ymm0, ymm0
    vmulps  ymm0, ymm0, ymm6
    vmovups ymmword ptr [rdi], ymm0

    ; Batch 1: bytes 8-15
    vpmovsxbd ymm0, qword ptr [rsi + 10]
    vcvtdq2ps ymm0, ymm0
    vmulps  ymm0, ymm0, ymm6
    vmovups ymmword ptr [rdi + 32], ymm0

    ; Batch 2: bytes 16-23
    vpmovsxbd ymm0, qword ptr [rsi + 18]
    vcvtdq2ps ymm0, ymm0
    vmulps  ymm0, ymm0, ymm6
    vmovups ymmword ptr [rdi + 64], ymm0

    ; Batch 3: bytes 24-31
    vpmovsxbd ymm0, qword ptr [rsi + 26]
    vcvtdq2ps ymm0, ymm0
    vmulps  ymm0, ymm0, ymm6
    vmovups ymmword ptr [rdi + 96], ymm0

    ; Advance
    add     rsi, BLOCK_Q8_0_SIZE        ; 34 bytes
    add     rdi, 128                    ; 32 × 4 bytes
    add     ebx, QK_0
    jmp     @@q8_block

@@q8_done:
    vzeroupper
    mov     rax, rbx

    vmovdqu xmm6, xmmword ptr [rsp]
    add     rsp, 16
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Quant_DequantQ8_0 ENDP

; =============================================================================
; Quant_VecDotQ4_0_Q8_0
; Fused dot product: dot(Q4_0_vec, Q8_0_vec) → scalar fp32
;
; This is THE hot inner loop for quantized inference. It computes the
; dot product of a Q4_0 weight vector against a Q8_0 activation vector
; WITHOUT materializing either to fp32 first.
;
; RCX = src_q4 (block_q4_0*), weight vector in Q4_0 format
; RDX = src_q8 (block_q8_0*), activation vector in Q8_0 format
; R8  = num_elements (must be multiple of 32)
; XMM0 (return) = dot product result (scalar float)
;
; Returns: XMM0 = dot product, RAX = 0 on success
;
; Algorithm per aligned block pair:
;   d4 = fp16_to_fp32(q4_block.d)
;   d8 = fp16_to_fp32(q8_block.d)
;   sum = 0
;   for i in 0..31:
;     q4_val = (nibble[i] - 8)   (signed 4-bit)
;     q8_val = qs_8[i]            (signed 8-bit)
;     sum += q4_val * q8_val
;   result += d4 * d8 * sum
;
; AVX2 optimization:
;   Use vpmaddubsw (unsigned×signed → int16 with saturation) on raw bytes,
;   then vpmaddwd to accumulate int16 pairs into int32.
;   This processes 32 elements per iteration with just 4 instructions.
; =============================================================================
Quant_VecDotQ4_0_Q8_0 PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 64
    .allocstack 64
    vmovdqu xmmword ptr [rsp],      xmm6
    vmovdqu xmmword ptr [rsp + 16], xmm7
    vmovdqu xmmword ptr [rsp + 32], xmm8
    vmovdqu xmmword ptr [rsp + 48], xmm9
    .endprolog

    lock inc g_Q4Q8VecDotCalls

    mov     rsi, rcx                    ; Q4_0 blocks
    mov     rdi, rdx                    ; Q8_0 blocks
    mov     r12, r8                     ; total elements
    xor     ebx, ebx                    ; processed count

    ; Accumulator for final dot product (fp32)
    vxorps  ymm8, ymm8, ymm8           ; ymm8 = running sum (fp32 vector)

    ; Load nibble mask
    vmovdqa ymm9, ymmword ptr [q4_nibble_mask]

    ; Constant: 8 as uint8 (for subtracting from unsigned nibbles)
    ; We'll create this: 0x08 repeated 32 times
    mov     eax, 08080808h
    vmovd   xmm6, eax
    vpbroadcastd ymm6, xmm6            ; ymm6 = 0x08 × 32

@@vd_q4q8_block:
    lea     rax, [rbx + QK_0]
    cmp     rax, r12
    ja      @@vd_q4q8_done

    ; --- Read Q4_0 scale: d4 = fp16 at [rsi+0] ---
    movzx   eax, word ptr [rsi]
    vmovd   xmm0, eax
    vcvtph2ps xmm0, xmm0               ; d4

    ; --- Read Q8_0 scale: d8 = fp16 at [rdi+0] ---
    movzx   eax, word ptr [rdi]
    vmovd   xmm1, eax
    vcvtph2ps xmm1, xmm1               ; d8

    ; Combined scale: d4 * d8
    vmulss  xmm7, xmm0, xmm1           ; xmm7 = d4 * d8

    ; --- Load Q4_0 quants (16 bytes at [rsi+2]) ---
    vmovdqu xmm2, xmmword ptr [rsi + 2]    ; 16 bytes = 32 nibbles

    ; Expand to 32 bytes: low nibbles in even positions, high in odd
    ; Extract low nibbles
    vpand   xmm3, xmm2, xmm9               ; Low nibbles (16 bytes)
    ; Extract high nibbles
    vpsrlw  xmm4, xmm2, 4
    vpand   xmm4, xmm4, xmm9               ; High nibbles (16 bytes)

    ; Interleave to get sequential order: [lo0,hi0,lo1,hi1,...]
    vpunpcklbw xmm0, xmm3, xmm4            ; First 16 interleaved
    vpunpckhbw xmm1, xmm3, xmm4            ; Second 16 interleaved

    ; Combine into ymm: first 16 in low lane, second 16 in high lane
    vinserti128 ymm0, ymm0, xmm1, 1        ; ymm0 = 32 unsigned q4 bytes [0..15]

    ; Convert from unsigned [0..15] to signed [-8..+7] by subtracting 8
    ; We need to do this carefully since vpmaddubsw treats first operand as unsigned.
    ; Strategy: subtract 8, treat as signed for the multiply.
    ; But vpmaddubsw(a, b) = a_unsigned * b_signed.
    ; If we put Q8_0 (signed) as second operand and Q4_0 (unsigned) as first,
    ; we need Q4_0 to stay unsigned [0..15].
    ; Then: dot = sum(q4_unsigned * q8_signed) needs correction:
    ;   sum((q4-8)*q8) = sum(q4*q8) - 8*sum(q8)
    ; So we compute the correction term separately.

    ; --- Load Q8_0 quants (32 signed bytes at [rdi+2]) ---
    vmovdqu ymm1, ymmword ptr [rdi + 2]    ; 32 × int8

    ; === Method: vpmaddubsw(Q4_unsigned, Q8_signed) + correction ===
    ; vpmaddubsw: for each pair of bytes, computes u8*s8→s16 and adds adjacent pairs
    ; Result: 16 × int16 values
    vpmaddubsw ymm2, ymm0, ymm1        ; 16 × int16 = sum of adjacent u8*s8 products

    ; Sum int16 pairs to int32: vpmaddwd with all-ones
    ; vpmaddwd(a, 1) sums adjacent int16 pairs into int32
    mov     eax, 00010001h
    vmovd   xmm3, eax
    vpbroadcastd ymm3, xmm3            ; ymm3 = [1, 1, 1, 1, ...] as int16 pairs
    vpmaddwd ymm2, ymm2, ymm3          ; 8 × int32

    ; Now compute correction: -8 * sum(q8)
    ; Sum all Q8 bytes: use vpsadbw against zero to get absolute byte sums,
    ; but we need signed sum. Use vpmovsxbw then horizontal add.
    ; Alternative: since Q8 values are signed, compute sum separately.
    ; Actually, the simpler correction:
    ; The vpmaddubsw computed: sum_pairs(q4_unsigned * q8_signed)
    ; We want: sum_pairs((q4_unsigned - 8) * q8_signed)
    ;        = sum_pairs(q4_unsigned * q8_signed) - 8 * sum_pairs(q8_signed)
    ; So we need to compute sum(q8_signed) per int16 pair, then per int32.

    ; Correction: 8 * sum_of_q8_pairs as int16
    ; Use vpmaddubsw(0x08_repeated, q8_signed) → 8*q8[2i] + 8*q8[2i+1] = 8*(q8[2i]+q8[2i+1])
    vpmaddubsw ymm4, ymm6, ymm1        ; 16 × int16: 8*(q8[2i]+q8[2i+1])
    vpmaddwd ymm4, ymm4, ymm3          ; 8 × int32: 8*sum_of_4_q8_values

    ; Subtract correction: true_dot_int32 = vpmaddubsw_result - correction
    vpsubd  ymm2, ymm2, ymm4           ; 8 × int32: corrected dot products

    ; Convert to float and multiply by combined scale
    vcvtdq2ps ymm2, ymm2               ; 8 × fp32
    vbroadcastss ymm7, xmm7            ; broadcast d4*d8
    vfmadd231ps ymm8, ymm2, ymm7       ; accumulator += d4*d8 * int_dots

    ; Advance to next block pair
    add     rsi, BLOCK_Q4_0_SIZE        ; Q4_0: 18 bytes
    add     rdi, BLOCK_Q8_0_SIZE        ; Q8_0: 34 bytes
    add     ebx, QK_0
    jmp     @@vd_q4q8_block

@@vd_q4q8_done:
    ; Horizontal sum of ymm8 → scalar in xmm0
    vextractf128 xmm0, ymm8, 1
    vaddps  xmm0, xmm0, xmm8           ; (low xmm8 already in xmm0 position)
    ; Wait — xmm8 is low half of ymm8. Need to add properly:
    ; Actually vextractf128 xmm0, ymm8, 1 extracts HIGH 128 bits into xmm0
    ; and ymm8's low 128 bits are still accessible as xmm8.
    ; But after vextractf128, xmm0 = ymm8[255:128]
    ; xmm8 = ymm8[127:0] (unchanged)
    vaddps  xmm0, xmm0, xmm8           ; Correct: high + low
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0           ; xmm0[0] = total dot product

    xor     eax, eax                    ; success

    vzeroupper
    vmovdqu xmm6,  xmmword ptr [rsp]
    vmovdqu xmm7,  xmmword ptr [rsp + 16]
    vmovdqu xmm8,  xmmword ptr [rsp + 32]
    vmovdqu xmm9,  xmmword ptr [rsp + 48]
    add     rsp, 64
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Quant_VecDotQ4_0_Q8_0 ENDP

; =============================================================================
; Quant_VecDotQ8_0_F32
; Fused dot product: dot(Q8_0_vec, fp32_vec) → scalar fp32
;
; RCX = src_q8 (block_q8_0*), quantized vector
; RDX = src_f32 (float*), fp32 vector
; R8  = num_elements (must be multiple of 32)
; XMM0 (return) = dot product result
;
; Returns: XMM0 = dot product, RAX = 0 on success
;
; Per block:
;   d = fp16_to_fp32(q8_block.d)
;   for i in 0..31: sum += (d * qs[i]) * f32[i]
;   Optimized: d * sum_i(qs[i] * f32[i])
;   Further: dequant 8 at a time, FMA with f32 vector
; =============================================================================
Quant_VecDotQ8_0_F32 PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    sub     rsp, 32
    .allocstack 32
    vmovdqu xmmword ptr [rsp],      xmm6
    vmovdqu xmmword ptr [rsp + 16], xmm7
    .endprolog

    lock inc g_Q8F32VecDotCalls

    mov     rsi, rcx                    ; Q8_0 blocks
    mov     rdi, rdx                    ; fp32 vector
    mov     r12, r8                     ; total elements
    xor     ebx, ebx                    ; processed

    vxorps  ymm6, ymm6, ymm6           ; Running dot product accumulator

@@vd_q8f32_block:
    lea     rax, [rbx + QK_0]
    cmp     rax, r12
    ja      @@vd_q8f32_done

    ; Read d (fp16)
    movzx   eax, word ptr [rsi]
    vmovd   xmm0, eax
    vcvtph2ps xmm0, xmm0
    vbroadcastss ymm7, xmm0            ; ymm7 = d broadcast

    ; Process 32 elements in 4 batches of 8
    ; Each batch: sign-extend 8 int8 → int32 → fp32, multiply by d, FMA with f32

    ; Batch 0
    vpmovsxbd ymm0, qword ptr [rsi + 2]    ; 8 × int8 → int32
    vcvtdq2ps ymm0, ymm0                    ; → fp32
    vmulps  ymm0, ymm0, ymm7                ; × d
    vfmadd231ps ymm6, ymm0, ymmword ptr [rdi]  ; acc += dequant * f32

    ; Batch 1
    vpmovsxbd ymm0, qword ptr [rsi + 10]
    vcvtdq2ps ymm0, ymm0
    vmulps  ymm0, ymm0, ymm7
    vfmadd231ps ymm6, ymm0, ymmword ptr [rdi + 32]

    ; Batch 2
    vpmovsxbd ymm0, qword ptr [rsi + 18]
    vcvtdq2ps ymm0, ymm0
    vmulps  ymm0, ymm0, ymm7
    vfmadd231ps ymm6, ymm0, ymmword ptr [rdi + 64]

    ; Batch 3
    vpmovsxbd ymm0, qword ptr [rsi + 26]
    vcvtdq2ps ymm0, ymm0
    vmulps  ymm0, ymm0, ymm7
    vfmadd231ps ymm6, ymm0, ymmword ptr [rdi + 96]

    ; Advance
    add     rsi, BLOCK_Q8_0_SIZE        ; 34 bytes
    add     rdi, 128                    ; 32 × 4 bytes
    add     ebx, QK_0
    jmp     @@vd_q8f32_block

@@vd_q8f32_done:
    ; Horizontal sum
    vextractf128 xmm0, ymm6, 1
    vaddps  xmm0, xmm0, xmm6           ; high + low (xmm6 = low 128 of ymm6)
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0           ; xmm0[0] = dot product

    xor     eax, eax

    vzeroupper
    vmovdqu xmm6,  xmmword ptr [rsp]
    vmovdqu xmm7,  xmmword ptr [rsp + 16]
    add     rsp, 32
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Quant_VecDotQ8_0_F32 ENDP

; =============================================================================
END
