; test_hello.asm — Sample assembly for rawrxd_asm testing
; Tests: sections, extern, global, labels, instructions, data

section .text

extern ExitProcess
global main

main:
    push rbp
    mov rbp, rsp
    sub rsp, 0x28

    ; Simple computation
    xor eax, eax
    mov ecx, 42
    add eax, ecx
    cmp eax, 42
    jne .error

    ; Clean exit
    xor ecx, ecx
    call ExitProcess

.error:
    mov ecx, 1
    call ExitProcess

section .data
msg db "Hello, RawrXD!", 0
val dd 0x12345678
ptr dq 0
