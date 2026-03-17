; kv_cache.asm
; MASM64 sliding window KV cache for GGML
; Efficient memory management for Transformer Key/Value tensors

.code

; ==================================================================================================
; void ggml_masm_kv_cache_copy(float* dst, const float* src, int64_t n_head, int64_t head_size, int64_t pos)
; RCX: dst, RDX: src, R8: n_head, R9: head_size, [rsp+40]: pos
; ==================================================================================================
ggml_masm_kv_cache_copy PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi

    mov r10, [rbp + 48] ; pos
    
    ; Total floats to copy = n_head * head_size
    mov rax, r8
    imul rax, r9
    
    ; dst_offset = pos * n_head * head_size
    mov r11, r10
    imul r11, rax
    
    lea rdi, [rcx + r11 * 4]
    mov rsi, rdx
    
    ; Fast SIMD copy
    mov rcx, rax ; count
    shr rcx, 3    ; 8 floats per YMM
copy_loop:
    test rcx, rcx
    jz copy_tail
    vmovups ymm0, ymmword ptr [rsi]
    vmovups ymmword ptr [rdi], ymm0
    add rsi, 32
    add rdi, 32
    dec rcx
    jmp copy_loop

copy_tail:
    mov rcx, rax
    and rcx, 7
tail_loop:
    test rcx, rcx
    jz done
    vmovss xmm0, dword ptr [rsi]
    vmovss dword ptr [rdi], xmm0
    add rsi, 4
    add rdi, 4
    dec rcx
    jmp tail_loop

done:
    vzeroupper
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
ggml_masm_kv_cache_copy ENDP

END
