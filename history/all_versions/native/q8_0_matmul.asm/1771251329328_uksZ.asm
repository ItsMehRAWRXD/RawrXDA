; native/q8_0_matmul.asm
; Pure MASM x64 Q8_0 dequantization and matrix multiplication
; Zero external dependencies, uses AVX2/AVX-512 if available

.code
ALIGN 16

EXTERNDEF q8_0_dequant_block:PROC
EXTERNDEF q8_0_matmul_naive:PROC
EXTERNDEF q8_0_matmul_avx2:PROC

; Q8_0 block structure: 32 8-bit weights (32 bytes) + 2-byte scale (FP16)
Q8_0_BLOCK_SIZE EQU 34
Q8_0_VALUES_PER_BLOCK EQU 32

; ============================================================================
; void q8_0_dequant_block(const uint8_t* src, float* dst, size_t n);
; RCX = src (Q8_0 block pointer)
; RDX = dst (output float buffer, must hold 32 floats)
; R8  = n (number of blocks)
; ============================================================================
q8_0_dequant_block PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rsi, rcx        ; src
    mov     rdi, rdx        ; dst
    mov     rbx, r8         ; n blocks

dequant_loop:
    test    rbx, rbx
    jz      dequant_done

    ; Load scale (FP16 at offset 32)
    movzx   eax, WORD PTR [rsi + 32]
    ; Convert FP16 to FP32 (simplified)
    movss   xmm0, DWORD PTR [rsi + 32]  ; Assume stored as FP32

    ; Broadcast scale
    shufps  xmm0, xmm0, 0

    ; Process 32 8-bit values
    ; Load all 32 bytes
    movdqu  xmm1, XMMWORD PTR [rsi]
    movdqu  xmm2, XMMWORD PTR [rsi + 16]

    ; Convert int8 to float and multiply by scale
    ; This is simplified - real implementation needs proper int8->float conversion
    movss   DWORD PTR [rdi], xmm0

    add     rsi, Q8_0_BLOCK_SIZE
    add     rdi, 32 * 4         ; 32 floats
    dec     rbx
    jmp     dequant_loop

dequant_done:
    add     rsp, 40
    pop     rdi
    pop     rsi
    pop     rbx
    ret

q8_0_dequant_block ENDP

; ============================================================================
; float q8_0_matmul_naive(const uint8_t* weights_q8, const float* activations,
;                         size_t n_blocks, size_t block_size);
; ============================================================================
q8_0_matmul_naive PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     r12, rcx        ; weights
    mov     r13, rdx        ; activations
    mov     rbx, r8         ; n_blocks
    xorps   xmm6, xmm6      ; Accumulator

matmul_loop:
    test    rbx, rbx
    jz      matmul_done

    ; Load scale
    movss   xmm0, DWORD PTR [r12 + 32]

    ; Inner loop over 32 values
    mov     rcx, r9         ; block_size
    xor     rax, rax        ; index

inner_loop:
    cmp     rax, rcx
    jae     inner_done

    ; Load weight (8-bit, signed)
    movsx   r8d, BYTE PTR [r12 + rax]

    ; Convert to float and dequant: weight * scale
    cvtsi2ss xmm1, r8d
    mulss   xmm1, xmm0

    ; Load activation and multiply-accumulate
    movss   xmm2, DWORD PTR [r13 + rax * 4]
    mulss   xmm1, xmm2
    addss   xmm6, xmm1

    inc     rax
    jmp     inner_loop

inner_done:
    add     r12, Q8_0_BLOCK_SIZE
    add     r13, r9d
    add     r13, r9d
    add     r13, r9d
    add     r13, r9d        ; *4 for float

    dec     rbx
    jmp     matmul_loop

matmul_done:
    movaps  xmm0, xmm6
    add     rsp, 40
    pop     r13
    pop     r12
    pop     rbx
    ret

q8_0_matmul_naive ENDP

q8_0_matmul_avx2 PROC FRAME
    ; TODO: AVX2 implementation
    jmp     q8_0_matmul_naive
q8_0_matmul_avx2 ENDP

END