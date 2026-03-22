; deflate_brutal_masm.asm — stored-block gzip (DEFLATE type 0) for RawrXD
; MSVC x64 ABI: RCX=src, RDX=len, R8=out_len*  -> returns RAX = malloc buffer or 0
; Mirrors RawrXD-ModelLoader/kernels/deflate_brutal_masm.asm (production path).
OPTION casemap:none
PUBLIC deflate_brutal_masm
EXTERN malloc:PROC

.code
deflate_brutal_masm PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 64

    mov     rsi, rcx        ; src
    mov     rbx, rdx        ; len
    mov     r14, r8         ; out_len*

    mov     rax, rbx
    add     rax, 65534
    mov     rcx, 65535
    xor     rdx, rdx
    div     rcx
    mov     rcx, rax
    imul    rcx, 5
    add     rcx, rbx
    add     rcx, 18

    sub     rsp, 32
    call    malloc
    add     rsp, 32
    test    rax, rax
    jz      _fail

    mov     r15, rax
    mov     rdi, rax

    ; Gzip header (10 bytes) — CM=8 deflate, FLG=0
    mov     word ptr [rdi], 08B1Fh
    mov     byte ptr [rdi+2], 08h
    mov     dword ptr [rdi+4], 0
    mov     word ptr [rdi+8], 0003h
    add     rdi, 10

    mov     r12, rbx
_block_loop:
    test    r12, r12
    jz      _after_blocks

    mov     rax, r12
    cmp     rax, 65535
    jbe     _chunk_ok
    mov     rax, 65535
_chunk_ok:
    mov     rcx, r12
    sub     rcx, rax
    cmp     rcx, 0
    jne     _not_final
    mov     byte ptr [rdi], 1
    jmp     _hdr_done
_not_final:
    mov     byte ptr [rdi], 0
_hdr_done:
    inc     rdi
    mov     cx, ax
    mov     word ptr [rdi], cx
    not     cx
    mov     word ptr [rdi+2], cx
    add     rdi, 4

    mov     rcx, rax
    rep     movsb

    sub     r12, rax
    jmp     _block_loop

_after_blocks:
    mov     dword ptr [rdi], 0
    add     rdi, 4
    mov     eax, ebx
    mov     dword ptr [rdi], eax
    add     rdi, 4

    mov     rax, rdi
    sub     rax, r15
    mov     qword ptr [r14], rax

    mov     rax, r15
    jmp     _exit

_fail:
    xor     rax, rax

_exit:
    add     rsp, 64
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
deflate_brutal_masm ENDP
END
