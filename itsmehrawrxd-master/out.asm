section .text
global main
extern printf
main:
    push rbp
    mov rbp, rsp
    sub rsp, 1024
    mov rax, 5
    push rax
    pop rax
    mov [rbp-8], rax
print_int:
    push rbp
    mov rbp, rsp
    mov rsi, rdi
    mov rdi, print_fmt
    mov rax, 0
    call printf
    leave
    ret
section .data
print_fmt: db "%d", 10, 0
