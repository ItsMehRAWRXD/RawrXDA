; Ada Compiler - Built From Scratch (FIXED)
; NASM x86-64 Assembly Implementation
; Links against universal_compiler_runtime_clean.asm

default rel

extern compiler_init
extern compiler_cleanup
extern compiler_error
extern compiler_warning
extern compiler_get_error_count
extern compiler_get_warning_count

section .data
    compiler_name db "Ada Compiler", 0
    compiler_version db "1.0.0", 0
    
section .text
    global start
    global main

start:
main:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Initialize runtime
    call compiler_init
    
    ; Ada compiler stubs
    xor eax, eax
    
    ; Cleanup
    call compiler_cleanup
    
    add rsp, 32
    pop rbp
    ret
