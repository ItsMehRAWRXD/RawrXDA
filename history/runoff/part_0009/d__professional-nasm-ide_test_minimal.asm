; Minimal test to verify Windows GUI executable works
bits 64
default rel

extern MessageBoxA
extern ExitProcess

global main

section .data
    msg db "IDE Test - If you see this, the executable works!", 0
    title db "Test", 0

section .text
main:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; MessageBoxA(NULL, msg, title, MB_OK)
    xor ecx, ecx
    lea rdx, [msg]
    lea r8, [title]
    xor r9d, r9d
    call MessageBoxA
    
    ; Exit with return code 0
    xor eax, eax
    
    add rsp, 32
    pop rbp
    ret
