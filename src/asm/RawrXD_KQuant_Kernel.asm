; =============================================================================
; RawrXD_KQuant_Kernel.asm — Bare-Metal Q4_K Dequantization (AVX2 + AVX-512)
; =============================================================================
;
; Optimized, register-pressure-minimized dequantization kernels for GGML
; K-Quant Q4_K super-blocks. Reverse-engineered from gguf_dml_bridge.cpp
; into pure x64 MASM for maximum throughput during inference.
;
; Q4_K Super-Block Layout (144 bytes per 256 elements):
;   +0:   d (fp16, 2 bytes) — super-block scale
;   +2:   dmin (fp16, 2 bytes) — super-block minimum
;   +4:   scales[12] — packed 6-bit scale/min pairs for 8 sub-blocks
;   +16:  qs[128] — packed 4-bit quants (2 per byte, 256 elements)
;
; Formula: output[i] = d * sc_j * quant_i - dmin * m_j
;   where sc_j/m_j are sub-block scale/min from scales[]
;
; Exports:
;   asm_dequant_q4_k_avx2     — Per-block AVX2 dequantizer (256 elements)
;   asm_dequant_q4_k_avx512   — Per-block AVX-512 dequantizer (256 elements)
;   asm_dequant_q4_k_batch    — Multi-block wrapper with auto dispatch
;   asm_kquant_cpuid_check     — Runtime AVX-512 feature detection
;
; Architecture: x64 MASM64 | Windows x64 ABI | No CRT | No exceptions
; Build: ml64.exe /c /Zi /Zd /Fo RawrXD_KQuant_Kernel.obj
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

option casemap:none

; =============================================================================
;                    CONSTANTS
; =============================================================================

QK_K                    EQU     256         ; Elements per super-block
BLOCK_Q4_K_SIZE         EQU     144         ; Bytes per Q4_K super-block
Q4K_D_OFFSET            EQU     0           ; fp16 d at offset 0
Q4K_DMIN_OFFSET         EQU     2           ; fp16 dmin at offset 2
Q4K_SCALES_OFFSET       EQU     4           ; scales[12] at offset 4
Q4K_QUANTS_OFFSET       EQU     16          ; qs[128] at offset 16
Q4K_NUM_SUBBLOCKS       EQU     8           ; 8 sub-blocks of 32 elements

; CPUID feature bits (leaf 7, subleaf 0, EBX)
CPUID_AVX512F_BIT       EQU     16          ; AVX-512 Foundation

; =============================================================================
;                    EXPORTS
; =============================================================================

PUBLIC asm_dequant_q4_k_avx2
PUBLIC asm_dequant_q4_k_avx512
PUBLIC asm_dequant_q4_k_batch
PUBLIC asm_kquant_cpuid_check

; =============================================================================
;                    DATA SECTION (64-byte aligned for ZMM loads)
; =============================================================================

_DATA64 SEGMENT ALIGN(64) 'DATA'

; Nibble isolation mask: 0x0F repeated (for AVX2 = 32 bytes, AVX-512 = 64 bytes)
ALIGN 64
kq_mask_0F_ymm:
    DB  0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh
    DB  0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh
    DB  0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh
    DB  0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh

ALIGN 64
kq_mask_0F_zmm:
    DB  0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh
    DB  0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh
    DB  0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh
    DB  0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh
    DB  0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh
    DB  0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh
    DB  0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh
    DB  0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh

; 6-bit scale isolation mask (0x3F)
ALIGN 32
kq_mask_3F:
    DD  03Fh, 03Fh, 03Fh, 03Fh, 03Fh, 03Fh, 03Fh, 03Fh

_DATA64 ENDS

; =============================================================================
;                    BSS
; =============================================================================

.data?

; CPUID cache (avoid repeated CPUID calls)
kq_cpuid_checked        DD  ?               ; 0 = not checked, 1 = checked
kq_avx512_available     DD  ?               ; 0 = no, 1 = AVX-512F present

; =============================================================================
;                    CODE SECTION
; =============================================================================

.code

; =============================================================================
; asm_kquant_cpuid_check — Runtime AVX-512 feature detection
;
; Checks CPUID leaf 7, subleaf 0 for AVX-512F support.
; Result is cached in BSS for subsequent calls.
;
; Returns: EAX = 1 if AVX-512F available, 0 otherwise
; =============================================================================
ALIGN 16
asm_kquant_cpuid_check PROC FRAME
    push    rbx
    .pushreg rbx
    .endprolog

    ; Check cache first
    cmp     DWORD PTR [kq_cpuid_checked], 1
    je      @@cpuid_cached

    ; CPUID leaf 7, subleaf 0
    mov     eax, 7
    xor     ecx, ecx
    cpuid

    ; Test EBX bit 16 (AVX-512F)
    bt      ebx, CPUID_AVX512F_BIT
    jnc     @@cpuid_no_avx512

    ; ── XGETBV gate: verify OS has enabled AVX-512 XSTATE ────────────
    ; CPU may advertise AVX-512F but OS might not have enabled ZMM
    ; state in XCR0 (bits 5/6/7).  Without this check, executing a
    ; ZMM instruction would #UD on such a machine.
    xor     ecx, ecx                    ; XCR0 selector
    xgetbv                              ; EDX:EAX = XCR0
    and     eax, 0E0h                   ; bits 5+6+7 = opmask + ZMM_hi256 + hi16_ZMM
    cmp     eax, 0E0h
    jne     @@cpuid_no_avx512           ; OS didn't enable AVX-512 state

    mov     DWORD PTR [kq_avx512_available], 1
    jmp     @@cpuid_done

@@cpuid_no_avx512:
    mov     DWORD PTR [kq_avx512_available], 0

@@cpuid_done:
    mov     DWORD PTR [kq_cpuid_checked], 1

@@cpuid_cached:
    mov     eax, DWORD PTR [kq_avx512_available]

    pop     rbx
    ret
asm_kquant_cpuid_check ENDP

; =============================================================================
; asm_dequant_q4_k_avx2 — Q4_K to FP32 dequantization (AVX2 + F16C)
;
; Dequantizes one Q4_K super-block (256 elements) from 144 bytes to 256 floats.
; Processes 8 sub-blocks of 32 elements each using AVX2 SIMD.
;
; RCX = pOutput (float*, must be 32-byte aligned for optimal perf)
; RDX = pInput  (block_q4_k*, 144-byte super-block)
; Returns: RAX = 256 (elements dequantized)
;
; Register allocation:
;   ymm12 = broadcast d (super-scale)
;   ymm13 = broadcast dmin (super-minimum)
;   ymm15 = nibble mask (0x0F)
;   r8    = quants pointer (advances per sub-block)
;   r9    = scales pointer
;   ebx   = sub-block counter
; =============================================================================
ALIGN 16
asm_dequant_q4_k_avx2 PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    mov     rsi, rdx                    ; pInput (Q4_K block)
    mov     rdi, rcx                    ; pOutput (float*)

    ; Load nibble mask
    vmovdqa ymm15, ymmword ptr [kq_mask_0F_ymm]

    ; ---- Read super-block header ----
    ; d = fp16 at [rsi+0], dmin = fp16 at [rsi+2]
    movzx   eax, word ptr [rsi + Q4K_D_OFFSET]
    vmovd   xmm0, eax
    vcvtph2ps xmm0, xmm0               ; d -> fp32
    vbroadcastss ymm12, xmm0           ; ymm12 = d (all 8 lanes)

    movzx   eax, word ptr [rsi + Q4K_DMIN_OFFSET]
    vmovd   xmm1, eax
    vcvtph2ps xmm1, xmm1               ; dmin -> fp32
    vbroadcastss ymm13, xmm1           ; ymm13 = dmin (all 8 lanes)

    ; Pointers to scales and quants within the super-block
    lea     r9, [rsi + Q4K_SCALES_OFFSET]   ; r9 = scales[12]
    lea     r8, [rsi + Q4K_QUANTS_OFFSET]   ; r8 = quants[128]

    xor     ebx, ebx                    ; sub-block index 0..7

@@avx2_subblock:
    cmp     ebx, Q4K_NUM_SUBBLOCKS
    jae     @@avx2_done

    ; ---- Unpack 6-bit scale (sc) and min (m) for this sub-block ----
    ; scales[12] packing for Q4_K:
    ;   For sub-block j (0..7):
    ;     sc = (scales[j/2] >> ((j%2)*4)) & 0x3F    (bottom half)
    ;     m  = (scales[j/2 + 6] >> ((j%2)*4)) & 0x3F  (top half)
    mov     ecx, ebx
    shr     ecx, 1                      ; j/2
    movzx   eax, byte ptr [r9 + rcx]   ; scales[j/2] for sc
    test    ebx, 1
    jz      @@avx2_sc_even
    shr     eax, 4                      ; High nibble for odd j
@@avx2_sc_even:
    and     eax, 3Fh                    ; 6-bit scale

    movzx   edx, byte ptr [r9 + rcx + 6]  ; scales[j/2 + 6] for m
    test    ebx, 1
    jz      @@avx2_m_even
    shr     edx, 4
@@avx2_m_even:
    and     edx, 3Fh                    ; 6-bit min

    ; Broadcast sc and m as floats
    vcvtsi2ss xmm2, xmm2, eax
    vbroadcastss ymm2, xmm2            ; ymm2 = sc (float)
    vcvtsi2ss xmm3, xmm3, edx
    vbroadcastss ymm3, xmm3            ; ymm3 = m (float)

    ; Compute d*sc and dmin*m
    vmulps  ymm10, ymm12, ymm2         ; ymm10 = d * sc
    vmulps  ymm11, ymm13, ymm3         ; ymm11 = dmin * m

    ; ---- Load 16 bytes of quants (32 x 4-bit values) ----
    vmovdqu xmm4, xmmword ptr [r8]

    ; Extract low nibbles (elements 0,2,4,...,30 → first 16 values)
    vpand   xmm5, xmm4, xmm15          ; Low nibbles (0x0F mask)

    ; Extract high nibbles (elements 1,3,5,...,31 → next 16 values)
    vpsrlw  xmm6, xmm4, 4
    vpand   xmm6, xmm6, xmm15

    ; ---- Process 8 low-nibble quants (first 8 bytes → 8 floats) ----
    vpmovzxbd ymm7, xmm5               ; 8 bytes → 8 dwords (zero-extend)
    vcvtdq2ps ymm7, ymm7               ; → 8 floats
    vmulps  ymm7, ymm7, ymm10          ; d * sc * q
    vsubps  ymm7, ymm7, ymm11          ; - dmin * m
    vmovups ymmword ptr [rdi], ymm7     ; Store 8 floats (32 bytes)

    ; ---- Process next 8 low-nibble quants (bytes 8-15 → 8 floats) ----
    vpsrldq xmm8, xmm5, 8             ; Shift right 8 bytes in xmm
    vpmovzxbd ymm8, xmm8
    vcvtdq2ps ymm8, ymm8
    vmulps  ymm8, ymm8, ymm10
    vsubps  ymm8, ymm8, ymm11
    vmovups ymmword ptr [rdi + 32], ymm8

    ; ---- Process 8 high-nibble quants (first 8 bytes → 8 floats) ----
    vpmovzxbd ymm7, xmm6
    vcvtdq2ps ymm7, ymm7
    vmulps  ymm7, ymm7, ymm10
    vsubps  ymm7, ymm7, ymm11
    vmovups ymmword ptr [rdi + 64], ymm7

    ; ---- Process next 8 high-nibble quants (bytes 8-15 → 8 floats) ----
    vpsrldq xmm8, xmm6, 8
    vpmovzxbd ymm8, xmm8
    vcvtdq2ps ymm8, ymm8
    vmulps  ymm8, ymm8, ymm10
    vsubps  ymm8, ymm8, ymm11
    vmovups ymmword ptr [rdi + 96], ymm8

    ; Advance pointers
    add     r8, 16                      ; 16 bytes of quants consumed
    add     rdi, 128                    ; 32 floats × 4 bytes = 128 bytes
    inc     ebx
    jmp     @@avx2_subblock

@@avx2_done:
    vzeroupper                          ; Transition penalty prevention
    mov     eax, QK_K                   ; Return 256 (elements processed)
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_dequant_q4_k_avx2 ENDP

; =============================================================================
; asm_dequant_q4_k_avx512 — Q4_K to FP32 dequantization (AVX-512F + F16C)
;
; Same logic as AVX2 but uses 512-bit ZMM registers for 16-wide float ops.
; Each sub-block of 32 elements is processed in two ZMM iterations (16+16)
; instead of four YMM iterations (8+8+8+8), halving loop overhead.
;
; RCX = pOutput (float*, 64-byte aligned recommended)
; RDX = pInput  (block_q4_k*, 144-byte super-block)
; Returns: RAX = 256 (elements dequantized)
;
; NOTE: Caller MUST verify AVX-512 support via asm_kquant_cpuid_check first.
;       Executing this on a non-AVX-512 CPU triggers #UD (Invalid Opcode).
; =============================================================================
ALIGN 16
asm_dequant_q4_k_avx512 PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    mov     rsi, rdx                    ; pInput
    mov     rdi, rcx                    ; pOutput

    ; ---- Read super-block header ----
    movzx   eax, word ptr [rsi + Q4K_D_OFFSET]
    vmovd   xmm0, eax
    vcvtph2ps xmm0, xmm0
    vbroadcastss zmm12, xmm0           ; zmm12 = d (16 lanes)

    movzx   eax, word ptr [rsi + Q4K_DMIN_OFFSET]
    vmovd   xmm1, eax
    vcvtph2ps xmm1, xmm1
    vbroadcastss zmm13, xmm1           ; zmm13 = dmin (16 lanes)

    ; Load 64-byte nibble mask for AVX-512
    vmovdqu64 zmm15, zmmword ptr [kq_mask_0F_zmm]

    lea     r9, [rsi + Q4K_SCALES_OFFSET]
    lea     r8, [rsi + Q4K_QUANTS_OFFSET]

    xor     ebx, ebx

@@avx512_subblock:
    cmp     ebx, Q4K_NUM_SUBBLOCKS
    jae     @@avx512_done

    ; ---- Unpack scale/min (same 6-bit logic as AVX2) ----
    mov     ecx, ebx
    shr     ecx, 1
    movzx   eax, byte ptr [r9 + rcx]
    test    ebx, 1
    jz      @@avx512_sc_even
    shr     eax, 4
@@avx512_sc_even:
    and     eax, 3Fh

    movzx   edx, byte ptr [r9 + rcx + 6]
    test    ebx, 1
    jz      @@avx512_m_even
    shr     edx, 4
@@avx512_m_even:
    and     edx, 3Fh

    ; Broadcast to ZMM (16 lanes)
    vcvtsi2ss xmm2, xmm2, eax
    vbroadcastss zmm2, xmm2            ; zmm2 = sc
    vcvtsi2ss xmm3, xmm3, edx
    vbroadcastss zmm3, xmm3            ; zmm3 = m

    vmulps  zmm10, zmm12, zmm2         ; d * sc
    vmulps  zmm11, zmm13, zmm3         ; dmin * m

    ; ---- Load 16 bytes of quants ----
    vmovdqu xmm4, xmmword ptr [r8]

    ; ---- Low nibbles: 16 values processed in one ZMM pass ----
    vpandd  xmm5, xmm4, xmm15          ; Isolate low nibbles (16 bytes)
    vpmovzxbd zmm7, xmm5               ; 16 bytes → 16 dwords in ZMM
    vcvtdq2ps zmm7, zmm7               ; → 16 floats
    vmulps  zmm7, zmm7, zmm10          ; d * sc * q
    vsubps  zmm7, zmm7, zmm11          ; - dmin * m
    vmovups zmmword ptr [rdi], zmm7     ; Store 16 floats (64 bytes)

    ; ---- High nibbles: 16 values processed in one ZMM pass ----
    vpsrlw  xmm6, xmm4, 4
    vpandd  xmm6, xmm6, xmm15
    vpmovzxbd zmm8, xmm6               ; 16 bytes → 16 dwords
    vcvtdq2ps zmm8, zmm8
    vmulps  zmm8, zmm8, zmm10
    vsubps  zmm8, zmm8, zmm11
    vmovups zmmword ptr [rdi + 64], zmm8 ; Store next 16 floats

    ; Advance
    add     r8, 16
    add     rdi, 128                    ; 32 floats × 4 bytes
    inc     ebx
    jmp     @@avx512_subblock

@@avx512_done:
    vzeroupper
    mov     eax, QK_K
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_dequant_q4_k_avx512 ENDP

; =============================================================================
; asm_dequant_q4_k_batch — Multi-block Q4_K dequantization with auto-dispatch
;
; Processes multiple Q4_K super-blocks, auto-selecting AVX-512 or AVX2
; based on CPUID detection. This is the primary entry point for the
; inference engine's tensor materialization path.
;
; RCX = pOutput (float*, numElements floats)
; RDX = pInput  (block_q4_k*, array of super-blocks)
; R8  = numElements (must be multiple of 256)
; Returns: RAX = total elements dequantized
; =============================================================================
ALIGN 16
asm_dequant_q4_k_batch PROC FRAME
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

    mov     r12, rcx                    ; pOutput
    mov     r13, rdx                    ; pInput
    mov     r14, r8                     ; total elements
    xor     r15d, r15d                  ; processed count

    ; Determine which kernel to use
    call    asm_kquant_cpuid_check
    mov     ebx, eax                    ; ebx = 1 if AVX-512, 0 for AVX2

@@batch_loop:
    cmp     r15, r14
    jae     @@batch_done

    ; Call per-block kernel
    mov     rcx, r12                    ; current output pointer
    mov     rdx, r13                    ; current input pointer

    test    ebx, ebx
    jz      @@batch_avx2
    call    asm_dequant_q4_k_avx512
    jmp     @@batch_advance
@@batch_avx2:
    call    asm_dequant_q4_k_avx2

@@batch_advance:
    ; Advance pointers by one super-block
    add     r13, BLOCK_Q4_K_SIZE        ; +144 bytes input
    add     r12, QK_K * 4               ; +256 floats × 4 bytes = +1024 bytes output
    add     r15, QK_K                   ; +256 elements
    jmp     @@batch_loop

@@batch_done:
    mov     rax, r15                    ; return total processed
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret
asm_dequant_q4_k_batch ENDP

; =============================================================================
; End
; =============================================================================
END
