; =============================================================================
; RawrXD_AVX512_Dequant_BPE.asm
; Lane A skeleton: AVX-512 dequant fusion + MASM BPE tokenizer entrypoints.
; Safe baseline implementation (scalar-compatible) with ABI-stable symbols.
; =============================================================================

option casemap:none

PUBLIC RawrXD_AVX512_DequantFusion
PUBLIC RawrXD_MASM_BPETokenize
PUBLIC RawrXD_ASMToolDispatchFastPath

.code

; uint64_t RawrXD_AVX512_DequantFusion(
;   const uint8_t* src_q,
;   const float* scales,
;   float* dst_f32,
;   uint64_t count)
;
; RCX=src_q, RDX=scales, R8=dst_f32, R9=count
; Returns processed element count.
RawrXD_AVX512_DequantFusion PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    xor     rax, rax
    test    rcx, rcx
    jz      @done
    test    r8, r8
    jz      @done
    test    r9, r9
    jz      @done

    mov     rsi, rcx                ; src_q
    mov     rbx, rdx                ; scales (optional)
    mov     rdi, r8                 ; dst_f32
    mov     rcx, r9                 ; loop counter

    ; Baseline lane: q -> float, optional global scale[0] multiply.
    ; Keeps ABI stable while AVX-512 fused kernel is iterated.
@loop:
    movzx   eax, byte ptr [rsi]
    cvtsi2ss xmm0, eax
    test    rbx, rbx
    jz      @store
    mulss   xmm0, dword ptr [rbx]
@store:
    movss   dword ptr [rdi], xmm0

    inc     rsi
    add     rdi, 4
    dec     rcx
    jnz     @loop

    mov     rax, r9

@done:
    pop     rdi
    pop     rsi
    pop     rbx
    ret
RawrXD_AVX512_DequantFusion ENDP

; uint64_t RawrXD_MASM_BPETokenize(
;   const char* text,
;   uint64_t text_len,
;   uint32_t* out_token_ids,
;   uint64_t max_tokens)
;
; RCX=text, RDX=text_len, R8=out_ids, R9=max_tokens
; Returns token count written.
RawrXD_MASM_BPETokenize PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    xor     rax, rax
    test    rcx, rcx
    jz      @tok_done
    test    r8, r8
    jz      @tok_done
    test    r9, r9
    jz      @tok_done

    mov     rsi, rcx                ; text
    mov     rcx, rdx                ; text_len remaining
    mov     rdi, r8                 ; out ids
    mov     rbx, r9                 ; max tokens

@tok_loop:
    test    rcx, rcx
    jz      @tok_done
    test    rbx, rbx
    jz      @tok_done

    movzx   edx, byte ptr [rsi]
    cmp     edx, 20h                ; skip spaces as token boundaries
    je      @next_char
    mov     dword ptr [rdi], edx    ; byte value as token id skeleton
    add     rdi, 4
    inc     rax
    dec     rbx

@next_char:
    inc     rsi
    dec     rcx
    jmp     @tok_loop

@tok_done:
    pop     rdi
    pop     rsi
    pop     rbx
    ret
RawrXD_MASM_BPETokenize ENDP

; uint64_t RawrXD_ASMToolDispatchFastPath(
;   uint32_t opcode,
;   const void* in_payload,
;   void* out_payload,
;   uint64_t payload_bytes)
;
; RCX=opcode, RDX=in_payload, R8=out_payload, R9=payload_bytes
; Returns 1 on fast-path success, 0 on fallback required.
RawrXD_ASMToolDispatchFastPath PROC FRAME
    .endprolog
    test    r8, r8
    jz      @fail
    test    rdx, rdx
    jz      @fail
    test    r9, r9
    jz      @ok

    ; Skeleton fast path: direct payload mirror for opcodes currently mapped.
    ; Full opcode routing lands in Lane F completion phase.
    mov     r10, r9
@copy_loop:
    mov     al, byte ptr [rdx]
    mov     byte ptr [r8], al
    inc     rdx
    inc     r8
    dec     r10
    jnz     @copy_loop

@ok:
    mov     rax, 1
    ret

@fail:
    xor     rax, rax
    ret
RawrXD_ASMToolDispatchFastPath ENDP

END
