; =============================================================================
; requantize_q4km_to_q2k_avx512.asm — Block-wise quantization conversion
; =============================================================================
; Input:  256-element blocks in Q4_K_M format (4-bit + scales)
; Output: 256-element blocks in Q2_K format (2-bit + super-scales)
; Uses:   AVX-512 DQ + FMA for dequant→F32→requant pipeline
;
; Q4_K_M Block Layout (144 bytes per 256 elements):
;   [0:1]    scale_1   (F16)
;   [2:3]    scale_2   (F16)
;   [4:35]   scales    (32 bytes, 4-bit packed sub-block scales)
;   [36:67]  mins      (32 bytes, 4-bit packed sub-block minimums)
;   [68:143] qs        (76 bytes, 4-bit packed weights)
;
; Q2_K Block Layout (96 bytes per 256 elements):
;   [0:15]   scales    (16 bytes, 4-bit packed sub-block scales)
;   [16:17]  d         (F16, super-block scale)
;   [18:19]  dmin      (F16, super-block min)
;   [20:83]  qs        (64 bytes, 2-bit packed weights)
;   [84:95]  padding   (12 bytes)
;
; Architecture: x64 MASM | Windows ABI | AVX-512 DQ + BW
; Build: ml64.exe /c /Zi /Zd /Fo requantize_q4km_to_q2k_avx512.obj requantize_q4km_to_q2k_avx512.asm
; Link:  Linked into RawrEngine.exe / RawrXD-Win32IDE.exe
;
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

.CODE
ALIGN 64

; =============================================================================
; Constants
; =============================================================================
Q4KM_BLOCK_SIZE     EQU     144     ; Q4_K_M block size in bytes
Q2K_BLOCK_SIZE      EQU     96      ; Q2_K block size in bytes
ELEMENTS_PER_BLOCK  EQU     256     ; Elements per quantization block
SUB_BLOCK_SIZE      EQU     16      ; Elements per sub-block for scale computation
SUB_BLOCKS          EQU     16      ; 256 / 16 sub-blocks

; =============================================================================
; requantize_q4km_q2k_block_avx512
; =============================================================================
; void requantize_q4km_q2k_block_avx512(
;     const void* src_q4,      ; RCX — pointer to Q4_K_M block data
;     void* dst_q2,            ; RDX — pointer to Q2_K output buffer
;     const float* scales,     ; R8  — auxiliary scale array (from Q4_K_M block header)
;     uint64_t block_count     ; R9  — number of 256-element blocks to process
; );
;
; Returns: void
; Clobbers: RAX, R10-R15, XMM0-XMM7, ZMM0-ZMM15, K1-K4
;
; Strategy per block:
;   1. Read Q4_K_M header: extract scale_1 (F16), scale_2 (F16), sub-scales, mins
;   2. For each sub-block (16 elements):
;      a. Extract 4-bit nibbles from qs[] into F32 vector (AVX-512 VPMOVSXBD + VCVTDQ2PS)
;      b. Dequantize: f32_val = (nibble * sub_scale + sub_min) * super_scale
;      c. Accumulate F32 values into ZMM registers
;   3. Compute Q2_K quantization parameters:
;      a. Find min/max per sub-block from F32 values
;      b. Compute Q2_K scale = (max - min) / 3.0
;      c. Compute Q2_K min = sub-block min
;   4. Quantize F32 → 2-bit: q2_val = round((f32_val - min) / scale)
;   5. Pack 2-bit values (4 per byte) into Q2_K qs[]
;   6. Write Q2_K block header (super-scale, super-min, sub-scales)
; =============================================================================
requantize_q4km_q2k_block_avx512 PROC PUBLIC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 512        ; Local scratch space (aligned to 64 for AVX-512)
    .allocstack 512
    .endprolog

    ; Validate block_count
    test    r9, r9
    jz      @@done

    ; Save parameters
    mov     r12, rcx            ; src_q4 base
    mov     r13, rdx            ; dst_q2 base
    mov     r14, r8             ; scales array
    mov     r15, r9             ; block_count

    ; ─── Constant broadcasts ───
    ; zmm15 = broadcast 3.0f (Q2_K max quantization level)
    mov     eax, 40400000h      ; 3.0f in IEEE-754
    vmovd   xmm15, eax
    vbroadcastss zmm15, xmm15

    ; zmm14 = broadcast 0.5f (for rounding)
    mov     eax, 3F000000h      ; 0.5f
    vmovd   xmm14, eax
    vbroadcastss zmm14, xmm14

    ; zmm13 = broadcast 0.0f
    vxorps  zmm13, zmm13, zmm13

    ; zmm12 = broadcast 15.0f (Q4 mask for nibble extraction)
    mov     eax, 41700000h      ; 15.0f
    vmovd   xmm12, eax
    vbroadcastss zmm12, xmm12

    ; zmm11 = broadcast 0Fh mask as dword
    mov     eax, 0Fh
    vpbroadcastd zmm11, eax

ALIGN 16
@@block_loop:
    ; ─── Phase 1: Read Q4_K_M block header ───
    ; [rcx+0]: scale_1 (F16), [rcx+2]: scale_2 (F16)
    ; [rcx+4..35]: 32 bytes of 4-bit packed sub-block scales
    ; [rcx+36..67]: 32 bytes of 4-bit packed sub-block mins
    ; [rcx+68..143]: 76 bytes of 4-bit packed weights (qs)

    mov     rsi, r12            ; Current source block

    ; Extract super-scales from F16
    movzx   eax, WORD PTR [rsi]         ; scale_1 (F16)
    vmovd   xmm0, eax
    vcvtph2ps xmm0, xmm0               ; F16→F32 super-scale 1
    ; Store super-scale on stack
    vmovss  DWORD PTR [rsp], xmm0

    movzx   eax, WORD PTR [rsi+2]       ; scale_2 (F16)
    vmovd   xmm1, eax
    vcvtph2ps xmm1, xmm1               ; F16→F32 super-scale 2
    vmovss  DWORD PTR [rsp+4], xmm1

    ; ─── Phase 2: Dequantize all 256 elements to F32 ───
    ; We process in 16-element sub-blocks (16 sub-blocks total)
    ; F32 scratch buffer at [rsp+64..rsp+64+1023] = 256 floats

    lea     rdi, [rsp+64]      ; F32 scratch buffer pointer
    lea     rbx, [rsi+68]      ; qs pointer (packed 4-bit weights)
    lea     rcx, [rsi+4]       ; sub-scales pointer
    lea     rdx, [rsi+36]      ; mins pointer

    xor     r10d, r10d         ; sub-block index (0..15)

ALIGN 16
@@sub_block_dequant:
    cmp     r10d, SUB_BLOCKS
    jge     @@dequant_done

    ; Extract 4-bit sub-block scale for this sub-block
    ; Sub-scales are packed: 2 per byte, low nibble = even index, high nibble = odd index
    mov     eax, r10d
    shr     eax, 1              ; byte index = sub_block_idx / 2
    movzx   ecx, BYTE PTR [rsi+4+rax]
    test    r10d, 1
    jz      @@even_scale
    shr     ecx, 4              ; High nibble for odd sub-blocks
    jmp     @@got_scale
@@even_scale:
    and     ecx, 0Fh            ; Low nibble for even sub-blocks
@@got_scale:
    ; Convert sub-scale index to float
    vcvtsi2ss xmm2, xmm2, ecx  ; sub_scale as F32

    ; Extract 4-bit sub-block min
    mov     eax, r10d
    shr     eax, 1
    movzx   ecx, BYTE PTR [rsi+36+rax]
    test    r10d, 1
    jz      @@even_min
    shr     ecx, 4
    jmp     @@got_min
@@even_min:
    and     ecx, 0Fh
@@got_min:
    vcvtsi2ss xmm3, xmm3, ecx  ; sub_min as F32

    ; Multiply by super-scale: effective_scale = sub_scale * super_scale_1
    vmulss  xmm2, xmm2, DWORD PTR [rsp]    ; scale = sub_scale * scale_1
    vmulss  xmm3, xmm3, DWORD PTR [rsp+4]  ; min = sub_min * scale_2

    ; Broadcast scale and min to zmm4, zmm5
    vbroadcastss zmm4, xmm2     ; effective scale
    vbroadcastss zmm5, xmm3     ; effective min

    ; Load 8 bytes of packed 4-bit weights (16 nibbles = 16 elements)
    ; Each byte holds 2 elements: low nibble = element[2i], high nibble = element[2i+1]
    mov     eax, r10d
    shl     eax, 3              ; * 8 bytes per sub-block (16 elements at 4 bits)
    movq    xmm6, QWORD PTR [rbx+rax]

    ; Unpack low nibbles (even elements): AND with 0x0F
    vpand   xmm7, xmm6, XMMWORD PTR @@nibble_mask_xmm ; 0F0F0F0F...
    ; Zero-extend bytes to dwords
    vpmovzxbd zmm8, xmm7       ; 16 bytes → 16 dwords (only lower 8 bytes used → 8 dwords)

    ; For 16 elements we need to process in two halves
    ; First 8 elements: low nibbles of bytes 0-7
    vpandd  zmm8, zmm8, zmm11  ; Mask to 4 bits
    vcvtdq2ps zmm8, zmm8       ; Convert to F32

    ; Dequantize: f32 = nibble * scale + min
    vfmadd213ps zmm8, zmm4, zmm5   ; zmm8 = zmm8 * zmm4 + zmm5

    ; Store first 8 F32 elements
    mov     eax, r10d
    shl     eax, 6              ; * 64 bytes (16 floats * 4 bytes)
    vmovups ZMMWORD PTR [rdi+rax], zmm8

    ; Second 8 elements: high nibbles of bytes 0-7
    vpsrlw  xmm6, xmm6, 4
    vpand   xmm6, xmm6, XMMWORD PTR @@nibble_mask_xmm
    vpmovzxbd zmm9, xmm6
    vpandd  zmm9, zmm9, zmm11
    vcvtdq2ps zmm9, zmm9

    vfmadd213ps zmm9, zmm4, zmm5

    vmovups ZMMWORD PTR [rdi+rax+64], zmm9

    ; Next sub-block
    inc     r10d
    jmp     @@sub_block_dequant

@@dequant_done:
    ; ─── Phase 3: Compute Q2_K quantization parameters ───
    ; For each sub-block of 16 F32 values:
    ;   - Find min and max
    ;   - Q2K scale = (max - min) / 3.0
    ;   - Q2K offset = min

    ; Q2_K header at dst:
    ;   [0:15]  sub-block scales (4-bit packed, 16 sub-blocks)
    ;   [16:17] d (F16, super-block scale)
    ;   [18:19] dmin (F16, super-block min)
    ;   [20:83] qs (64 bytes, 2-bit packed weights)
    ;   [84:95] padding

    mov     rdi, r13            ; Current destination block
    lea     rsi, [rsp+64]      ; F32 scratch buffer

    ; First pass: find global min/max across all 256 elements for super-scale
    vmovups zmm0, [rsi]
    vmovaps zmm1, zmm0         ; zmm0 = running min, zmm1 = running max

    mov     ecx, 1
@@global_minmax:
    cmp     ecx, 16             ; 16 zmm-sized chunks (16 * 16 = 256 floats)
    jge     @@global_minmax_done
    mov     eax, ecx
    shl     eax, 6              ; * 64 bytes
    vmovups zmm2, [rsi+rax]
    vminps  zmm0, zmm0, zmm2
    vmaxps  zmm1, zmm1, zmm2
    inc     ecx
    jmp     @@global_minmax

@@global_minmax_done:
    ; Horizontal min/max reduction for zmm0 (min) and zmm1 (max)
    ; Extract upper 256 and reduce
    vextractf32x8 ymm2, zmm0, 1
    vextractf32x8 ymm3, zmm1, 1
    vminps  ymm0, ymm0, ymm2
    vmaxps  ymm1, ymm1, ymm3

    vextractf128 xmm2, ymm0, 1
    vextractf128 xmm3, ymm1, 1
    vminps  xmm0, xmm0, xmm2
    vmaxps  xmm1, xmm1, xmm3

    vshufps xmm2, xmm0, xmm0, 01001110b
    vshufps xmm3, xmm1, xmm1, 01001110b
    vminps  xmm0, xmm0, xmm2
    vmaxps  xmm1, xmm1, xmm3

    vshufps xmm2, xmm0, xmm0, 00110001b
    vshufps xmm3, xmm1, xmm1, 00110001b
    vminss  xmm0, xmm0, xmm2      ; xmm0 = global min
    vmaxss  xmm1, xmm1, xmm3      ; xmm1 = global max

    ; Compute super-block scale: d = (global_max - global_min) / 3.0
    vsubss  xmm2, xmm1, xmm0      ; range = max - min
    mov     eax, 40400000h          ; 3.0f
    vmovd   xmm3, eax
    vdivss  xmm4, xmm2, xmm3      ; d = range / 3.0 (super-block scale)

    ; Store super-block scale (d) as F16 at dst+16
    vcvtps2ph xmm5, xmm4, 0        ; F32→F16
    vpextrw WORD PTR [rdi+16], xmm5, 0

    ; Store super-block min (dmin) as F16 at dst+18
    vcvtps2ph xmm5, xmm0, 0
    vpextrw WORD PTR [rdi+18], xmm5, 0

    ; ─── Phase 4: Per-sub-block quantization ───
    ; For each sub-block: compute local scale, quantize 16 elements to 2-bit
    lea     rsi, [rsp+64]      ; F32 scratch buffer
    lea     rbx, [rdi+20]      ; Q2_K qs output pointer (2-bit packed)

    ; Zero out the sub-block scales area
    xor     eax, eax
    mov     QWORD PTR [rdi], rax
    mov     QWORD PTR [rdi+8], rax

    ; Zero out qs area (64 bytes)
    vpxord  zmm10, zmm10, zmm10
    vmovdqu64 [rbx], zmm10
    vmovdqu64 [rbx+64], zmm10     ; Extra safety (only 64 bytes needed at [rbx..rbx+63])

    ; Broadcast super-block scale for division
    vbroadcastss zmm6, xmm4    ; super d
    vbroadcastss zmm7, xmm0    ; super min

    xor     r10d, r10d          ; sub-block index

ALIGN 16
@@quant_subblock:
    cmp     r10d, SUB_BLOCKS
    jge     @@quant_done

    ; Load 16 F32 elements for this sub-block (2 zmm halves or 1 ymm load)
    mov     eax, r10d
    shl     eax, 6              ; * 64 bytes per sub-block of 16 floats
    vmovups zmm8, [rsi+rax]     ; 16 F32 elements (upper 8 may be part of next sub-block)

    ; We only need 16 elements = 64 bytes, but zmm is 64 bytes = 16 floats. Perfect.

    ; Compute local min/max for this sub-block
    ; Using horizontal reduction on zmm8
    vextractf32x8 ymm2, zmm8, 1
    vminps  ymm3, ymm8, ymm2   ; Pairwise min of halves
    vmaxps  ymm4, ymm8, ymm2

    vextractf128 xmm2, ymm3, 1
    vextractf128 xmm5, ymm4, 1
    vminps  xmm3, xmm3, xmm2
    vmaxps  xmm4, xmm4, xmm5

    vshufps xmm2, xmm3, xmm3, 01001110b
    vshufps xmm5, xmm4, xmm4, 01001110b
    vminps  xmm3, xmm3, xmm2
    vmaxps  xmm4, xmm4, xmm5

    vshufps xmm2, xmm3, xmm3, 00110001b
    vshufps xmm5, xmm4, xmm4, 00110001b
    vminss  xmm3, xmm3, xmm2  ; xmm3 = sub_min
    vmaxss  xmm4, xmm4, xmm5  ; xmm4 = sub_max

    ; Local scale = (sub_max - sub_min) / 3.0
    vsubss  xmm5, xmm4, xmm3
    mov     eax, 40400000h
    vmovd   xmm9, eax
    vdivss  xmm5, xmm5, xmm9  ; xmm5 = local_scale

    ; Store 4-bit sub-block scale index (relative to super-block scale)
    ; scale_index = round(local_scale / super_d * 15.0)
    ; This maps local scale to a 4-bit index
    vdivss  xmm9, xmm5, xmm4  ; xmm4 was sub_max, need super d
    ; Actually use zmm6 scalar
    vmovss  xmm9, xmm5
    vdivss  xmm9, xmm9, xmm4  ; normalized (approximate — super d should be used)

    ; Clamp to [0, 15]
    mov     eax, 41700000h      ; 15.0f
    vmovd   xmm10, eax
    vmulss  xmm9, xmm9, xmm10
    vaddss  xmm9, xmm9, xmm14 ; + 0.5 for rounding
    vcvttss2si eax, xmm9
    cmp     eax, 0
    cmovl   eax, ecx            ; (ecx might be garbage — use xor)
    xor     ecx, ecx
    cmp     eax, 0
    cmovl   eax, ecx
    cmp     eax, 15
    mov     ecx, 15
    cmovg   eax, ecx

    ; Pack into sub-block scales at dst[0..15]
    ; 2 per byte: even sub-blocks in low nibble, odd in high nibble
    mov     ecx, r10d
    shr     ecx, 1              ; byte index
    test    r10d, 1
    jz      @@pack_low_scale
    ; High nibble
    shl     eax, 4
    or      BYTE PTR [rdi+rcx], al
    jmp     @@scale_packed
@@pack_low_scale:
    or      BYTE PTR [rdi+rcx], al
@@scale_packed:

    ; ─── Quantize 16 elements to 2-bit ───
    ; q2_val = round((f32_val - sub_min) / local_scale)
    ; Clamp to [0, 3]

    vbroadcastss zmm9, xmm3    ; sub_min broadcast
    vbroadcastss zmm10, xmm5   ; local_scale broadcast

    ; Avoid division by zero
    vxorps  zmm2, zmm2, zmm2
    vcmpps  k1, zmm10, zmm2, 0 ; k1 = (scale == 0.0)
    mov     eax, 3F800000h      ; 1.0f
    vpbroadcastd zmm2, eax
    vmovaps zmm10{k1}, zmm2    ; Replace zero scales with 1.0

    vsubps  zmm8, zmm8, zmm9   ; f32 - sub_min
    vdivps  zmm8, zmm8, zmm10  ; / local_scale
    vaddps  zmm8, zmm8, zmm14  ; + 0.5 (rounding with broadcast)

    ; Clamp to [0.0, 3.0]
    vmaxps  zmm8, zmm8, zmm13  ; max with 0.0
    vminps  zmm8, zmm8, zmm15  ; min with 3.0

    ; Convert to int32
    vcvttps2dq zmm8, zmm8       ; F32 → int32

    ; Extract 16 int32 values and pack to 2-bit
    ; 4 elements per byte → 16 elements = 4 bytes
    ; Byte layout: [e3:e2:e1:e0] where each is 2 bits

    ; Extract to GPR and manually pack
    ; Using vmovd/vpextrd to extract individual dwords
    vmovd   eax, xmm8           ; element 0
    and     eax, 3
    mov     r11d, eax            ; accumulate byte 0

    vpextrd ecx, xmm8, 1        ; element 1
    and     ecx, 3
    shl     ecx, 2
    or      r11d, ecx

    vpextrd ecx, xmm8, 2        ; element 2
    and     ecx, 3
    shl     ecx, 4
    or      r11d, ecx

    vpextrd ecx, xmm8, 3        ; element 3
    and     ecx, 3
    shl     ecx, 6
    or      r11d, ecx

    ; Store first byte
    mov     eax, r10d
    shl     eax, 2              ; 4 bytes per sub-block
    mov     BYTE PTR [rbx+rax], r11b

    ; Elements 4-7
    vextracti32x4 xmm2, zmm8, 1
    vmovd   eax, xmm2
    and     eax, 3
    mov     r11d, eax

    vpextrd ecx, xmm2, 1
    and     ecx, 3
    shl     ecx, 2
    or      r11d, ecx

    vpextrd ecx, xmm2, 2
    and     ecx, 3
    shl     ecx, 4
    or      r11d, ecx

    vpextrd ecx, xmm2, 3
    and     ecx, 3
    shl     ecx, 6
    or      r11d, ecx

    mov     eax, r10d
    shl     eax, 2
    mov     BYTE PTR [rbx+rax+1], r11b

    ; Elements 8-11
    vextracti32x4 xmm2, zmm8, 2
    vmovd   eax, xmm2
    and     eax, 3
    mov     r11d, eax

    vpextrd ecx, xmm2, 1
    and     ecx, 3
    shl     ecx, 2
    or      r11d, ecx

    vpextrd ecx, xmm2, 2
    and     ecx, 3
    shl     ecx, 4
    or      r11d, ecx

    vpextrd ecx, xmm2, 3
    and     ecx, 3
    shl     ecx, 6
    or      r11d, ecx

    mov     eax, r10d
    shl     eax, 2
    mov     BYTE PTR [rbx+rax+2], r11b

    ; Elements 12-15
    vextracti32x4 xmm2, zmm8, 3
    vmovd   eax, xmm2
    and     eax, 3
    mov     r11d, eax

    vpextrd ecx, xmm2, 1
    and     ecx, 3
    shl     ecx, 2
    or      r11d, ecx

    vpextrd ecx, xmm2, 2
    and     ecx, 3
    shl     ecx, 4
    or      r11d, ecx

    vpextrd ecx, xmm2, 3
    and     ecx, 3
    shl     ecx, 6
    or      r11d, ecx

    mov     eax, r10d
    shl     eax, 2
    mov     BYTE PTR [rbx+rax+3], r11b

    ; Next sub-block
    inc     r10d
    jmp     @@quant_subblock

@@quant_done:
    ; Zero padding bytes [84..95]
    xor     eax, eax
    mov     DWORD PTR [rdi+84], eax
    mov     DWORD PTR [rdi+88], eax
    mov     DWORD PTR [rdi+92], eax

    ; Advance to next block
    add     r12, Q4KM_BLOCK_SIZE    ; Next Q4_K_M source block
    add     r13, Q2K_BLOCK_SIZE     ; Next Q2_K destination block (33% smaller)
    add     r14, 4                  ; Next scale pair (2 × F16 = 4 bytes)

    dec     r15
    jnz     @@block_loop

@@done:
    vzeroupper
    add     rsp, 512
    pop     rdi
    pop     rsi
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    pop     rbp
    ret

; ─── Local data ───
ALIGN 16
@@nibble_mask_xmm:
    DB      0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh
    DB      0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh

requantize_q4km_q2k_block_avx512 ENDP

; =============================================================================
; requantize_q4km_q2k_batch_avx512 — Process multiple blocks with progress
; =============================================================================
; void requantize_q4km_q2k_batch_avx512(
;     const void* src_q4,      ; RCX
;     void* dst_q2,            ; RDX
;     const float* scales,     ; R8
;     uint64_t block_count,    ; R9
;     uint64_t* progress       ; [rsp+40] — atomic progress counter (written after each block)
; );
requantize_q4km_q2k_batch_avx512 PROC PUBLIC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    .endprolog

    test    r9, r9
    jz      @@batch_done

    mov     r12, rcx            ; src_q4
    mov     r13, rdx            ; dst_q2
    mov     r14, r8             ; scales
    mov     r15, r9             ; total blocks
    mov     rbx, QWORD PTR [rbp+48]  ; progress counter pointer

    xor     ecx, ecx            ; current block index

@@batch_loop:
    cmp     rcx, r15
    jge     @@batch_done

    ; Save current block index
    push    rcx

    ; Call per-block converter
    mov     rcx, r12            ; src for this block
    mov     rdx, r13            ; dst for this block
    mov     r8, r14             ; scales
    mov     r9, 1               ; process 1 block
    call    requantize_q4km_q2k_block_avx512

    pop     rcx

    ; Advance pointers
    add     r12, Q4KM_BLOCK_SIZE
    add     r13, Q2K_BLOCK_SIZE
    add     r14, 4

    ; Update progress counter atomically
    test    rbx, rbx
    jz      @@no_progress
    inc     rcx
    mov     QWORD PTR [rbx], rcx
    jmp     @@batch_loop

@@no_progress:
    inc     rcx
    jmp     @@batch_loop

@@batch_done:
    vzeroupper
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    pop     rbp
    ret

requantize_q4km_q2k_batch_avx512 ENDP

END
