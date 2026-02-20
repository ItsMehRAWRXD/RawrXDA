; RawrXD Universal Compiler - x86_64 Assembly Test
section .data
    msg db "RawrXD ASM Test - Hello from Assembly!", 10, 0
    pass db "All Assembly tests passed!", 10, 0

section .text
    global _start, main

_start:
main:
    ; Print message
    mov rax, 1          ; sys_write
    mov rdi, 1          ; stdout
    lea rsi, [msg]
    mov rdx, 40
    syscall
    
    ; Print pass
    mov rax, 1
    mov rdi, 1
    lea rsi, [pass]
    mov rdx, 27
    syscall
    
    ; Exit
    mov rax, 60
    xor rdi, rdi
    syscall
