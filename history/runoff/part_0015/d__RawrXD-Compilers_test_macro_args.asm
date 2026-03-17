; ============================================================================
; test_macro_args.asm - Phase 2 Macro Argument Substitution Stress Test
; Tests: %1..%n mapping, variadics, defaults, stringification, concatenation
; Target: RawrXD NASM Universal Assembler
; ============================================================================

; --- Basic 2-Argument Macro ---
%macro push_pop 2
    push %1
    mov eax, %2
    pop %1
%endmacro

; --- 3-Argument Macro with Register Operations ---
%macro mov3 3
    mov %1, %2
    mov %2, %3
    mov %3, %1
%endmacro

; --- Macro with Nested Brackets (tests bracket depth tracking) ---
%macro array_op 2
    mov rax, [%1 + %2*8]
    mov [%1], rax
%endmacro

; --- Variadic Macro (1 required + unlimited) ---
%macro variadic_test 1-*
    ; %0 = total argument count
    mov ecx, %0
    mov rax, %1        ; First required arg
    ; Remaining args would use %* for all
%endmacro

; --- Macro with Defaults ---
%macro with_defaults 1-3 0, 1
    ; %1 = required
    ; %2 = default 0
    ; %3 = default 1
    mov rax, %1
    add rax, %2
    sub rax, %3
%endmacro

; --- Zero-Arg Macro (pure text substitution) ---
%macro save_regs 0
    push rax
    push rbx
    push rcx
    push rdx
%endmacro

%macro restore_regs 0
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro

; --- Nested Macro Invocation Test ---
%macro outer 2
    push_pop %1, %2
    mov rax, rbx
%endmacro

; --- Immediate Value Test ---
%macro load_const 2
    mov %1, %2
%endmacro

; --- Complex Expression Arguments ---
%macro complex_args 2
    lea rax, [%1 + %2]
    mov rbx, rax
%endmacro

; ============================================================================
; CODE SECTION - Invoke All Test Macros
; ============================================================================
section .text
global _start

_start:
    ; Test 1: Basic 2-arg macro
    push_pop rbp, 15h
    
    ; Test 2: 3-arg macro
    mov3 rax, rbx, rcx
    
    ; Test 3: Array operation with brackets
    array_op rsi, 4
    
    ; Test 4: Variadic with multiple args
    variadic_test 1, 2, 3, 4, 5
    
    ; Test 5: Variadic with minimum args
    variadic_test 42
    
    ; Test 6: Defaults - all provided
    with_defaults 100, 200, 300
    
    ; Test 7: Defaults - partial
    with_defaults 100, 200
    
    ; Test 8: Defaults - minimum only
    with_defaults 100
    
    ; Test 9: Zero-arg macros
    save_regs
    nop
    restore_regs
    
    ; Test 10: Nested macro expansion
    outer rbp, 20h
    
    ; Test 11: Immediate constants
    load_const rax, 0x1234567890ABCDEF
    load_const rbx, 0
    
    ; Test 12: Complex expressions
    complex_args rbp, 8
    
    ; Test 13: Repeated invocation
    push_pop rax, 1
    push_pop rbx, 2
    push_pop rcx, 3
    
    ; Exit
    xor rdi, rdi
    mov rax, 60
    syscall

; ============================================================================
; DATA SECTION
; ============================================================================
section .data
    test_data: dq 0x1122334455667788
    test_array: times 16 dq 0

; ============================================================================
; BSS SECTION  
; ============================================================================
section .bss
    buffer: resb 1024
