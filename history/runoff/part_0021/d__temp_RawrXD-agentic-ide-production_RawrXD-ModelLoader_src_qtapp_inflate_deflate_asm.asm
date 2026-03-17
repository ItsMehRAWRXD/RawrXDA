OPTION casemap:none

PUBLIC AsmInflate
PUBLIC AsmDeflate

EXTERN malloc:PROC
EXTERN memcpy:PROC
EXTERN free:PROC

.code

; RCX = src, RDX = src_len, R8 = dst, R9 = dst_max_len
; returns unpacked len in RAX, 0 on error
AsmInflate PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 64
    
    mov     rsi, rcx        ; src
    mov     rbx, rdx        ; src_len
    mov     rdi, r8         ; dst
    mov     r14, r9         ; dst_max_len
    
    ; Check for gzip header
    cmp     rbx, 18
    jl      _inflate_fail
    
    mov     ax, word ptr [rsi]
    cmp     ax, 08B1Fh      ; gzip magic
    jne     _inflate_fail
    
    ; Skip gzip header (10 bytes minimum)
    add     rsi, 10
    sub     rbx, 18         ; header + footer
    
    ; Process stored blocks
    xor     r15, r15        ; output counter
    
_block_loop:
    cmp     rbx, 5
    jl      _inflate_fail
    
    ; Read block header
    mov     al, byte ptr [rsi]
    inc     rsi
    dec     rbx
    
    ; Read block length
    mov     cx, word ptr [rsi]
    add     rsi, 2
    sub     rbx, 2
    
    ; Verify length complement
    mov     dx, word ptr [rsi]
    add     rsi, 2
    sub     rbx, 2
    
    not     dx
    cmp     cx, dx
    jne     _inflate_fail
    
    ; Check output buffer size
    mov     rax, r15
    add     rax, rcx
    cmp     rax, r14
    jg      _inflate_fail
    
    ; Copy data
    mov     r12, rcx
    rep     movsb
    add     r15, r12
    sub     rbx, r12
    
    ; Check if final block
    test    al, 1
    jnz     _inflate_done
    
    jmp     _block_loop
    
_inflate_done:
    mov     rax, r15
    jmp     _inflate_exit
    
_inflate_fail:
    xor     rax, rax
    
_inflate_exit:
    add     rsp, 64
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
AsmInflate ENDP

; RCX = src, RDX = src_len, R8 = dst, R9 = dst_max_len
; returns compressed len in RAX, 0 on error
AsmDeflate PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 64
    
    mov     rsi, rcx        ; src
    mov     rbx, rdx        ; src_len
    mov     rdi, r8         ; dst
    mov     r14, r9         ; dst_max_len
    
    ; Check output buffer size (header + blocks + footer)
    mov     rax, rbx
    add     rax, 65534
    mov     rcx, 65535
    xor     rdx, rdx
    div     rcx
    imul    rax, 5
    add     rax, rbx
    add     rax, 18
    cmp     rax, r14
    jg      _deflate_fail
    
    ; Write gzip header
    mov     word ptr [rdi], 08B1Fh
    mov     byte ptr [rdi+2], 08h
    mov     dword ptr [rdi+4], 0
    mov     word ptr [rdi+8], 0003h
    add     rdi, 10
    
    ; Process blocks
    mov     r12, rbx        ; remaining
    xor     r15, r15        ; output counter
    add     r15, 10
    
_block_loop_deflate:
    test    r12, r12
    jz      _deflate_footer
    
    mov     rax, r12
    cmp     rax, 65535
    jbe     _chunk_ok_deflate
    mov     rax, 65535
    
_chunk_ok_deflate:
    ; Write block header
    mov     rcx, r12
    sub     rcx, rax
    cmp     rcx, 0
    jne     _not_final_deflate
    mov     byte ptr [rdi], 1
    jmp     _hdr_done_deflate
    
_not_final_deflate:
    mov     byte ptr [rdi], 0
    
_hdr_done_deflate:
    inc     rdi
    inc     r15
    
    mov     cx, ax
    mov     word ptr [rdi], cx
    not     cx
    mov     word ptr [rdi+2], cx
    add     rdi, 4
    add     r15, 4
    
    ; Copy data
    mov     rcx, rax
    rep     movsb
    add     r15, rax
    sub     r12, rax
    
    jmp     _block_loop_deflate
    
_deflate_footer:
    ; Write footer
    mov     dword ptr [rdi], 0
    add     rdi, 4
    mov     eax, ebx
    mov     dword ptr [rdi], eax
    add     rdi, 4
    add     r15, 8
    
    mov     rax, r15
    jmp     _deflate_exit
    
_deflate_fail:
    xor     rax, rax
    
_deflate_exit:
    add     rsp, 64
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
AsmDeflate ENDP

END
