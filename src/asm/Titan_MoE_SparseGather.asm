; ============================================================================
; MSVC x64 MoeConfig field offsets
; The MASM MOE_CONFIG STRUCT above has no padding (MASM default packs tightly),
; but MSVC inserts 4 bytes of padding at offset +20 to align expert_size_bytes
; to 8. Use these EQU constants instead of the struct for all field access.
; ============================================================================
MOECONFIG_TOP_K        EQU 4   ; DWORD @  +4
MOECONFIG_HIDDEN_DIM   EQU 8   ; DWORD @  +8
MOECONFIG_EXPERT_BYTES EQU 24  ; QWORD @ +24  (+4 pad after weight_dtype)
MOECONFIG_WEIGHTS_BASE EQU 32  ; QWORD @ +32
.code
; ============================================================================
; SparseGather_Initialize
;   In:  RCX = MoeConfig* config,  RDX = telemetry_handle (unused)
;   Out: RAX = context handle (= config pointer)
; ============================================================================
SparseGather_Initialize PROC EXPORT
    mov     rax, rcx
    ret
SparseGather_Initialize ENDP

; ============================================================================
; SparseGather_Execute
;   In:  RCX = void* context (MoeConfig*),  RDX = float* input
;        R8  = float* router_logits,          R9  = float* output
;        [rsp+28h] = uint32_t layer_index  (unused)
;   Out: RAX = 1 (success)  or  0 (null / misconfigured)
;
;   Computes (float32 AVX-512 FMA sparse weighted gather):
;     output[0..hidden-1] = 0
;     for k = 0..top_k-1:
;       logit = router_logits[k]
;       for i = 0..hidden-1 step 16:
;         output[i:+16] += logit * weights_base[k*expert_bytes + i:+16]
;                                   * input[i:+16]
;
;   Register map
;   ── Non-volatile (saved/restored): ─────────────────────────────────────
;     RBX = MoeConfig*          RSI = router_logits
;     RDI = input               R12 = output
;     R13 = top_k               R14 = hidden_dim (DWORD)
;     R15 = weights_base        R11 = expert_size_bytes (intermediate)
;   ── Volatile (no save needed): ─────────────────────────────────────────
;     RCX = k (outer loop)      RDX = i (inner loop, float index)
;     RAX = expert_ptr          R10 = scratch
;   ── ZMM — only volatile registers ZMM0-ZMM5 are used: ─────────────────
;   (Win64 requires saving XMM/YMM/ZMM6-15; using only 0-5 avoids that)
;     ZMM0 = weights chunk      ZMM1 = input chunk
;     ZMM2 = output accumul.    ZMM3 = elem product (weight*input)
;     ZMM4 = logit broadcast    ZMM5 = zero
;
;   Stack frame (entry RSP%16==8):
;     push r15/r14/r13/r12/rsi/rdi/rbx  = 7×8 = 56 bytes  → RSP%16 == 0
;     sub rsp, 40                        = 40 bytes         → RSP%16 == 8
;     (40 = 32 shadow + 8 alignment pad to keep RSP 16-aligned at any call)
; ============================================================================
SparseGather_Execute PROC EXPORT
    push    r15
    push    r14
    push    r13
    push    r12
    push    rsi
    push    rdi
    push    rbx
    sub     rsp, 40         ; 32-byte shadow + 8 pad (RSP stays 16-aligned)

    ; null-check context
    test    rcx, rcx
    jz      SG_fail

    ; save Win64 arg regs to non-volatiles
    mov     rbx, rcx        ; rbx = MoeConfig*
    mov     rdi, rdx        ; rdi = float* input
    mov     rsi, r8         ; rsi = float* router_logits
    mov     r12, r9         ; r12 = float* output

    ; load config
    mov     r15, QWORD PTR [rbx + MOECONFIG_WEIGHTS_BASE]
    test    r15, r15
    jz      SG_fail

    mov     r13d, DWORD PTR [rbx + MOECONFIG_TOP_K]
    test    r13d, r13d
    jz      SG_fail

    mov     r14d, DWORD PTR [rbx + MOECONFIG_HIDDEN_DIM]
    test    r14d, r14d
    jz      SG_fail

    mov     r11, QWORD PTR [rbx + MOECONFIG_EXPERT_BYTES]

    ; ── zero output buffer ──────────────────────────────────────────────
    vpxord  zmm5, zmm5, zmm5
    xor     eax, eax
SG_zero:
    mov     r8d, r14d
    sub     r8d, eax
    cmp     r8d, 16
    jge     SG_zero_full

    ; Tail zeroing with mask
    mov     ecx, r8d
    mov     r10d, 1
    shl     r10d, cl
    dec     r10d
    kmovw   k1, r10d
    vmovups ZMMWORD PTR [r12 + rax*4]{k1}, zmm5
    jmp     SG_zero_done

SG_zero_full:
    vmovups ZMMWORD PTR [r12 + rax*4], zmm5
    add     eax, 16
    cmp     eax, r14d
    jl      SG_zero
SG_zero_done:

    ; ── outer loop: k = 0..top_k-1 ─────────────────────────────────────
    xor     r10d, r10d      ; r10d = outer loop counter k
SG_k_loop:
    cmp     r10d, r13d
    jge     SG_done

    ; broadcast router_logits[k] into all 16 lanes of zmm4
    vbroadcastss zmm4, DWORD PTR [rsi + r10*4]

    ; expert_ptr = weights_base + k * expert_size_bytes
    mov     rax, r10
    imul    rax, r11
    add     rax, r15        ; rax = &weights[k][0]

    ; ── inner loop: i = 0..hidden-1, step 16 ───────────────────────────
    xor     edx, edx
SG_i_loop:
    mov     r8d, r14d
    sub     r8d, edx
    cmp     r8d, 16
    jge     SG_i_full

    ; Tail processing with mask
    mov     ecx, r8d
    mov     r9d, 1
    shl     r9d, cl
    dec     r9d
    kmovw   k1, r9d
    
    vmovups zmm0{k1}{z}, ZMMWORD PTR [rax + rdx*4]
    vmovups zmm1{k1}{z}, ZMMWORD PTR [rdi + rdx*4]
    vmovups zmm2, ZMMWORD PTR [r12 + rdx*4]    ; load existing
    vmulps  zmm3, zmm0, zmm1
    vfmadd231ps zmm2, zmm3, zmm4
    vmovups ZMMWORD PTR [r12 + rdx*4]{k1}, zmm2
    jmp     SG_i_next_k

SG_i_full:
    vmovups zmm0, ZMMWORD PTR [rax + rdx*4]    ; weights[k][i:+16]
    vmovups zmm1, ZMMWORD PTR [rdi + rdx*4]    ; input[i:+16]
    vmovups zmm2, ZMMWORD PTR [r12 + rdx*4]    ; output[i:+16]
    vmulps  zmm3, zmm0, zmm1                   ; elem  = weight * input
    vfmadd231ps zmm2, zmm3, zmm4               ; output += elem * logit
    vmovups ZMMWORD PTR [r12 + rdx*4], zmm2
    add     edx, 16
    cmp     edx, r14d
    jl      SG_i_loop

SG_i_next_k:
    inc     r10d
    jmp     SG_k_loop

SG_done:
    add     rsp, 40
    pop     rbx
    pop     rdi
    pop     rsi
    pop     r12
    pop     r13
    pop     r14
    pop     r15
    mov     eax, 1
    ret

SG_fail:
    add     rsp, 40
    pop     rbx
    pop     rdi
    pop     rsi
    pop     r12
    pop     r13
    pop     r14
    pop     r15
    xor     eax, eax
    ret

SparseGather_Execute ENDP

; ============================================================================
; SparseGather_FlushCache — no persistent cache
; ============================================================================
SparseGather_FlushCache PROC EXPORT
    ret
SparseGather_FlushCache ENDP

; ============================================================================
; SparseGather_GetStats
;   RCX = context,  RDX = uint64_t* loaded,  R8 = uint64_t* skipped
; ============================================================================
SparseGather_GetStats PROC EXPORT
    test    rdx, rdx
    jz      SG_gs_done
    mov     QWORD PTR [rdx], 0
    test    r8, r8
    jz      SG_gs_done
    mov     QWORD PTR [r8], 0
SG_gs_done:
    ret
SparseGather_GetStats ENDP

END
