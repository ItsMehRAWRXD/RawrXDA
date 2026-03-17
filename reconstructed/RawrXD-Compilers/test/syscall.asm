; test_syscall.asm - NASM syntax syscall test
; Linux x64 exit(0)

section .text
global _start

_start:
    mov rax, 60      ; sys_exit
    xor rdi, rdi     ; exit code 0
    syscall
