section .text
global main
extern printf
main:
    push rbp
    mov rbp, rsp
    sub rsp, 1024

    ; let x = 5
    mov rax, 5
    mov [rbp-8], rax
    ; let y = 10
    mov rax, 10
    mov [rbp-16], rax
    ; ret result
    mov rax, [rbp-24]
    leave
    ret
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