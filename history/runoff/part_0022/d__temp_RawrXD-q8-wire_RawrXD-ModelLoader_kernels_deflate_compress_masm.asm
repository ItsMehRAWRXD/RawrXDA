; deflate_compress_masm.asm - RLE compression in pure MASM
; ml64 /c /Fo deflate_compress_masm.obj deflate_compress_masm.asm
OPTION casemap:none
PUBLIC deflate_compress_masm
EXTERN malloc:PROC

.code
deflate_compress_masm PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 64

    mov     rsi, rcx
    mov     rbx, rdx
    mov     r14, r8

    ; Allocate output
    mov     rcx, rbx
    shr     rcx, 3
    add     rcx, rbx
    add     rcx, 1024
    sub     rsp, 32
    call    malloc
    add     rsp, 32
    test    rax, rax
    jz      exit_fail

    mov     r15, rax
    mov     rdi, rax

    ; Gzip header
    mov     ax, 01F8Bh
    mov     word ptr [rdi], ax
    add     rdi, 2
    mov     byte ptr [rdi], 08h
    inc     rdi
    xor     eax, eax
    mov     qword ptr [rdi], rax
    add     rdi, 7

    ; DEFLATE block
    mov     byte ptr [rdi], 03h
    inc     rdi

    ; RLE compression
    xor     r12, r12
    xor     r13d, r13d

compress_start:
    cmp     r12, rbx
    jge     compress_end

    movzx   eax, byte ptr [rsi + r12]
    mov     cl, al

    ; Update CRC32
    xor     r13d, eax
    mov     edx, 8
crc_loop:
    mov     eax, r13d
    and     eax, 1
    shr     r13d, 1
    test    eax, eax
    jz      crc_skip
    xor     r13d, 0EDB88320h
crc_skip:
    dec     edx
    jnz     crc_loop

    ; Count run
    mov     edx, 1
    inc     r12

count_loop:
    cmp     r12, rbx
    jge     run_done
    cmp     edx, 258
    jge     run_done
    movzx   eax, byte ptr [rsi + r12]
    cmp     al, cl
    jne     run_done
    inc     edx
    inc     r12
    jmp     count_loop

run_done:
    cmp     edx, 4
    jl      write_literal

    ; RLE encode
    mov     byte ptr [rdi], 0FFh
    inc     rdi
    mov     byte ptr [rdi], cl
    inc     rdi
    mov     word ptr [rdi], dx
    add     rdi, 2
    jmp     compress_start

write_literal:
    mov     byte ptr [rdi], cl
    inc     rdi
    dec     edx
    test    edx, edx
    jnz     write_literal
    jmp     compress_start

compress_end:
    mov     byte ptr [rdi], 00h
    inc     rdi

    not     r13d
    mov     dword ptr [rdi], r13d
    add     rdi, 4
    mov     eax, ebx
    mov     dword ptr [rdi], eax
    add     rdi, 4

    mov     rax, rdi
    sub     rax, r15
    mov     qword ptr [r14], rax

    mov     rax, r15
    add     rsp, 64
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

exit_fail:
    xor     rax, rax
    add     rsp, 64
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

deflate_compress_masm ENDP
END
