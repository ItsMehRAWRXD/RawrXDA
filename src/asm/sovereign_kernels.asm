; ============================================================================
; sovereign_kernels.asm
; Pure x64 MASM (ml64.exe). Zero CRT. AVX2 + F16C.
; Real compute primitives for sovereign CPU inference fallback.
; ============================================================================
.code
OPTION DOTNAME

; ----------------------------------------------------------------------------
; Sovereign_DequantizeRow_Q4_0_AVX2
; RCX = const void* q4_0_blocks
; RDX = float* dst
; R8  = int64_t n  (must be multiple of 32)
; ----------------------------------------------------------------------------
Sovereign_DequantizeRow_Q4_0_AVX2 PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx
    mov     r12, rdx
    mov     r13, r8
    xor     rax, rax
    vpxor   ymm0, ymm0, ymm0
    vmovdqu ymm14, ymmword ptr [mask_0F]

dq_loop:
    cmp     rax, r13
    jge     dq_done

    ; Load delta (half-precision) and broadcast
    vcvtph2ps xmm1, qword ptr [rbx]
    vbroadcastss ymm15, xmm1

    ; Load 32 nibbles (16 bytes)
    vmovdqu xmm1, xmmword ptr [rbx+2]
    vpmovzxbw ymm2, xmm1
    vpsrlw  ymm3, ymm2, 4
    vpand   ymm2, ymm2, ymm14
    vpand   ymm3, ymm3, ymm14

    ; Low 16: unpack to 32-bit, convert to float, subtract 8, multiply by delta
    vpunpcklwd ymm4, ymm2, ymm0
    vpunpckhwd ymm5, ymm2, ymm0
    vcvtdq2ps ymm4, ymm4
    vcvtdq2ps ymm5, ymm5
    vsubps  ymm4, ymm4, ymmword ptr [center_8]
    vsubps  ymm5, ymm5, ymmword ptr [center_8]
    vmulps  ymm4, ymm4, ymm15
    vmulps  ymm5, ymm5, ymm15
    vmovups ymmword ptr [r12], ymm4
    vmovups ymmword ptr [r12+32], ymm5

    ; High 16: same for upper nibble
    vpunpcklwd ymm4, ymm3, ymm0
    vpunpckhwd ymm5, ymm3, ymm0
    vcvtdq2ps ymm4, ymm4
    vcvtdq2ps ymm5, ymm5
    vsubps  ymm4, ymm4, ymmword ptr [center_8]
    vsubps  ymm5, ymm5, ymmword ptr [center_8]
    vmulps  ymm4, ymm4, ymm15
    vmulps  ymm5, ymm5, ymm15
    vmovups ymmword ptr [r12+64], ymm4
    vmovups ymmword ptr [r12+96], ymm5

    add     rbx, 18
    add     r12, 128
    add     rax, 32
    jmp     dq_loop

dq_done:
    vzeroupper
    add     rsp, 40
    pop     r13
    pop     r12
    pop     rbx
    ret
Sovereign_DequantizeRow_Q4_0_AVX2 ENDP

; ----------------------------------------------------------------------------
; Sovereign_RMSNorm_F32_AVX2
; RCX = float* x (in-place)
; RDX = int64_t n
; R8  = const float* weight
; R9  = float eps  (in xmm3 per Windows x64 float ABI)
; ----------------------------------------------------------------------------
Sovereign_RMSNorm_F32_AVX2 PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx
    mov     r12, rdx
    vxorps  ymm0, ymm0, ymm0
    xor     rax, rax

rn_sum:
    cmp     rax, r12
    jge     rn_sum_done
    vmovups ymm1, ymmword ptr [rbx+rax*4]
    vmulps  ymm1, ymm1, ymm1
    vaddps  ymm0, ymm0, ymm1
    add     rax, 8
    jmp     rn_sum

rn_sum_done:
    vextractf128 xmm1, ymm0, 1
    vaddps  xmm0, xmm0, xmm1
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0
    vcvtsi2ss xmm1, xmm1, r12d
    vdivss  xmm1, xmm0, xmm1
    vaddss  xmm1, xmm1, xmm3
    vsqrtss xmm1, xmm1, xmm1
    vmovss  xmm2, dword ptr [one_f]
    vdivss  xmm1, xmm2, xmm1
    vbroadcastss ymm1, xmm1

    xor     rax, rax
rn_apply:
    cmp     rax, r12
    jge     rn_done
    vmovups ymm2, ymmword ptr [rbx+rax*4]
    vmovups ymm3, ymmword ptr [r8+rax*4]
    vmulps  ymm2, ymm2, ymm1
    vmulps  ymm2, ymm2, ymm3
    vmovups ymmword ptr [rbx+rax*4], ymm2
    add     rax, 8
    jmp     rn_apply

rn_done:
    vzeroupper
    add     rsp, 40
    pop     r12
    pop     rbx
    ret
Sovereign_RMSNorm_F32_AVX2 ENDP

; ----------------------------------------------------------------------------
; Sovereign_CopyBuffer_NT_AVX2
; RCX = void* dst
; RDX = const void* src
; R8  = int64_t bytes (multiple of 32)
; ----------------------------------------------------------------------------
Sovereign_CopyBuffer_NT_AVX2 PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, r8
    xor     rax, rax
nt_loop:
    cmp     rax, rbx
    jge     nt_done
    vmovdqa ymm0, ymmword ptr [rdx+rax]
    vmovntdq ymmword ptr [rcx+rax], ymm0
    add     rax, 32
    jmp     nt_loop
nt_done:
    sfence
    vzeroupper
    add     rsp, 40
    pop     rbx
    ret
Sovereign_CopyBuffer_NT_AVX2 ENDP

; ----------------------------------------------------------------------------
; Sovereign_MatVec_F32_AVX2
; RCX = const float* A (row-major)
; RDX = const float* x
; R8  = float* y
; R9  = int64_t n_rows
; [RBP+48] = int64_t n_cols
; ----------------------------------------------------------------------------
Sovereign_MatVec_F32_AVX2 PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
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
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx
    mov     r12, rdx
    mov     r13, r8
    mov     r14, r9
    mov     r15, qword ptr [rbp+48]

    xor     r9, r9

mv_row:
    cmp     r9, r14
    jge     mv_done
    vxorps  ymm0, ymm0, ymm0
    xor     r10, r10

mv_col:
    cmp     r10, r15
    jge     mv_col_done
    vmovups ymm1, ymmword ptr [rbx+r10*4]
    vmovups ymm2, ymmword ptr [r12+r10*4]
    vfmadd231ps ymm0, ymm1, ymm2
    add     r10, 8
    jmp     mv_col

mv_col_done:
    vextractf128 xmm1, ymm0, 1
    vaddps  xmm0, xmm0, xmm1
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0
    vmovss  dword ptr [r13+r9*4], xmm0

    lea     rbx, [rbx+r15*4]
    inc     r9
    jmp     mv_row

mv_done:
    vzeroupper
    lea     rsp, [rbp-40]
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    pop     rbp
    ret
Sovereign_MatVec_F32_AVX2 ENDP

.data
ALIGN 16
mask_0F     db 16 dup(0Fh), 16 dup(0Fh)
center_8    dd 8 dup(8.0)
one_f       dd 1.0

END
