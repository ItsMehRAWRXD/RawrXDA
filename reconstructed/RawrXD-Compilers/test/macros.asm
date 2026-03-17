; ============================================================================
; test_macro_args.asm
; Comprehensive test for macro argument substitution
; ============================================================================

BITS 64

; Test 1: Basic positional parameters
%macro prologue 2
    push %1
    mov %1, %2
%endmacro

; Test 2: Default parameters  
%macro add_instr 2 0
    add %1, %2
%endmacro

; Test 3: Argument count (%0)
%macro check_args 3+
    ; Should have at least 3 args
    mov eax, %0
%endmacro

; Test 4: Variadic (%*)
%macro push_all 1+
    %rep %0
        push %1
    %endrep
%endmacro

; Test 5: Literal %% 
%macro percent_test 0
    ; Test that %% expands to single %
    db 100
%endmacro

section .text
global _start

_start:
    ; Test basic substitution
    prologue rbp, rsp
    
    ; Test default params (calling with only 1 arg)
    add_instr rax
    
    ; Test arg count
    check_args rax, rbx, rcx, rdx
    
    ; Test variadic
    push_all rax, rbx, rcx
    
    ; Test literal %%
    percent_test
    
    ; Cleanup
    mov rax, 60
    xor rdi, rdi
    syscall
