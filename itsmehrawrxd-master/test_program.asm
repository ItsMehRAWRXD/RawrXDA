section .data


section .text
global _start

_start:
    call main
    mov rax, 60
    mov rdi, 0
    syscall

main:
    push rbp
    mov rbp, rsp
    push 5
    pop rax
    mov [x], rax
    push 10
    pop rax
    mov [y], rax
    push x
    push y
    pop rbx
    pop rax
    add rax, rbx
    push rax
    pop rax
    mov [result], rax
    push result
    pop rax
    ret
    pop rbp
    ret
