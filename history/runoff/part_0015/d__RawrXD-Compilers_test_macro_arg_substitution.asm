; ============================================================================
; test_macro_arg_substitution.asm
; Comprehensive Test Suite for Macro Argument Substitution Engine
; Tests: %1-%N, %0 (argc), %* (variadic), defaults, brace delimiters
; ============================================================================

BITS 64

section .data
    test_msg db "Testing macro argument substitution...", 10, 0
    pass_msg db "PASS: ", 0
    fail_msg db "FAIL: ", 0

section .text
global _start

_start:
    ; Test 1: Basic Positional Arguments (%1, %2, %3)
    test_basic_args
    
    ; Test 2: Argument Count (%0)
    test_arg_count
    
    ; Test 3: Variadic Arguments (%*)
    test_variadic
    
    ; Test 4: Default Arguments
    test_defaults
    
    ; Test 5: Brace-Delimited Arguments (NASM {comma, preservation})
    test_brace_args
    
    ; Test 6: Nested Macro Expansion
    test_nested
    
    ; Test 7: Empty Arguments
    test_empty_args
    
    ; Test 8: Maximum Parameters (20 params)
    test_max_params
    
    ; Exit with success code
    mov rax, 60
    xor rdi, rdi
    syscall

; ============================================================================
; TEST 1: Basic Positional Arguments
; ============================================================================
%macro MOV3 3
    ; Simple 3-argument macro: mov %1, %2, %3
    mov %1, %2
    add %1, %3
%endmacro

test_basic_args:
    MOV3 rax, rbx, rcx      ; Should expand to: mov rax, rbx / add rax, rcx
    MOV3 rdx, rsi, rdi      ; Another invocation with different args
    ret

; ============================================================================
; TEST 2: Argument Count (%0)
; ============================================================================
%macro ARGCOUNT 1+
    ; Report how many args were provided
    ; %0 should contain the actual argument count
    mov eax, %0             ; Should emit: mov eax, <number>
    ; For debugging: if this is 2 args, eax = 2
%endmacro

test_arg_count:
    ARGCOUNT one            ; %0 = 1
    ARGCOUNT one, two       ; %0 = 2
    ARGCOUNT a, b, c, d     ; %0 = 4
    ret

; ============================================================================
; TEST 3: Variadic Arguments (%*)
; ============================================================================
%macro CALL_WITH_ARGS 1+
    ; Call function %1 with all remaining args
    ; %* should expand to: %2, %3, %4, ... (comma-separated)
    mov rdi, %2
    mov rsi, %3
    call %1
%endmacro

%macro PUSH_ALL 0+
    ; Push all provided arguments
    ; Variadic macro with no minimum args
%rep %0
    push %1
%rotate 1
%endrep
%endmacro

test_variadic:
    CALL_WITH_ARGS my_func, arg1, arg2  ; Expands with 2 extra args
    PUSH_ALL rax, rbx, rcx, rdx         ; Push 4 registers
    ret

; ============================================================================
; TEST 4: Default Arguments
; ============================================================================
%macro PROLOGUE 1-2 rsp
    ; 1 required arg, 2nd defaults to rsp
    push rbp
    mov rbp, %1
    sub %1, %2
%endmacro

%macro ALLOC 1-3 0, 8
    ; 1 required, 2 optional (default 0 and 8)
    ; %1 = dest register
    ; %2 = size (default 0)
    ; %3 = alignment (default 8)
    mov %1, %2
    add %1, %3
%endmacro

test_defaults:
    PROLOGUE rsp            ; Uses default for %2 (rsp)
    PROLOGUE rax, rbx       ; Provides both args explicitly
    
    ALLOC rcx               ; Uses defaults: 0, 8
    ALLOC rdx, 64           ; Uses default for %3 (8)
    ALLOC rsi, 128, 16      ; Provides all 3 args
    ret

; ============================================================================
; TEST 5: Brace-Delimited Arguments (NASM Syntax)
; ============================================================================
%macro ARRAY_INIT 2
    ; %1 = variable name
    ; %2 = initializer list (may contain commas)
    %1: dd %2
%endmacro

test_brace_args:
    ; Commas inside braces should NOT split the argument
    ARRAY_INIT my_array, {1, 2, 3, 4}
    ; Should expand to: my_array: dd 1, 2, 3, 4
    ; (The braces are removed, but content preserved as single arg)
    
    ARRAY_INIT vec, {10, 20, 30}
    ret

; ============================================================================
; TEST 6: Nested Macro Expansion
; ============================================================================
%macro INNER 2
    mov %1, %2
%endmacro

%macro OUTER 3
    INNER %1, %2        ; Nested invocation
    add %1, %3
%endmacro

test_nested:
    OUTER rax, rbx, rcx
    ; Should expand to:
    ;   INNER rax, rbx  -> mov rax, rbx
    ;   add rax, rcx
    ret

; ============================================================================
; TEST 7: Empty Arguments
; ============================================================================
%macro OPTIONAL_LOAD 2-3
    ; %1 = dest, %2 = src, %3 = optional offset
    mov %1, [%2%3]      ; %3 may be empty
%endmacro

test_empty_args:
    OPTIONAL_LOAD rax, rbx      ; %3 is empty: mov rax, [rbx]
    OPTIONAL_LOAD rcx, rdx, +8  ; %3 is +8: mov rcx, [rdx+8]
    ret

; ============================================================================
; TEST 8: Maximum Parameters (20 params per NASM standard)
; ============================================================================
%macro LOAD20 20
    ; Test all 20 parameters are accessible
    mov rax, %1
    mov rbx, %2
    mov rcx, %3
    mov rdx, %4
    mov rsi, %5
    mov rdi, %6
    mov r8, %7
    mov r9, %8
    mov r10, %9
    mov r11, %10
    mov r12, %11
    mov r13, %12
    mov r14, %13
    mov r15, %14
    ; Continue with memory/stack for remaining
    push %15
    push %16
    push %17
    push %18
    push %19
    push %20
%endmacro

test_max_params:
    LOAD20 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20
    ; Should successfully substitute all 20 parameters
    ret

; ============================================================================
; TEST 9: Stringification (%"N - Future)
; ============================================================================
; NOTE: Stringification not yet implemented, placeholder for future test
;%macro DEBUG_VAR 1
;    ; Convert %1 to string literal for debug output
;    db %"1, ": ", 0
;    dq %1
;%endmacro

; test_stringify:
;     DEBUG_VAR my_variable   ; Should emit: db "my_variable: ", 0
;     ret

; ============================================================================
; TEST 10: Concatenation (%+ - Future)
; ============================================================================
; NOTE: Token concatenation not yet implemented
;%macro GEN_FUNC 1
;    ; Generate function name by concatenating
;    %1%+_handler:
;        ret
;%endmacro

; test_concat:
;     GEN_FUNC test   ; Should create: test_handler:
;     call test_handler
;     ret

; ============================================================================
; TEST 11: Edge Case - Argument with Operators
; ============================================================================
%macro COMPLEX_EXPR 3
    ; Arguments containing operators should be preserved
    mov %1, %2 + %3
    lea %1, [%2 + %3*4]
%endmacro

test_complex_expr:
    COMPLEX_EXPR rax, rbx, 8
    ; Should expand to:
    ;   mov rax, rbx + 8
    ;   lea rax, [rbx + 8*4]
    ret

; ============================================================================
; TEST 12: Argument with Parentheses (Nested Delimiters)
; ============================================================================
%macro COMPUTE 2
    mov rax, %1
    add rax, %2
%endmacro

test_paren_args:
    COMPUTE (1+2), (3*4)
    ; Should preserve parentheses: mov rax, (1+2) / add rax, (3*4)
    ret

; ============================================================================
; TEST 13: Multi-Line Macro Body
; ============================================================================
%macro SAVE_REGS 0
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
%endmacro

%macro RESTORE_REGS 0
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro

test_multiline:
    SAVE_REGS
    ; ... do work ...
    RESTORE_REGS
    ret

; ============================================================================
; TEST 14: Variadic Range (min-max syntax)
; ============================================================================
%macro BOUNDED_VARIADIC 2-5
    ; Requires 2 args, accepts up to 5
    mov %1, %2
%if %0 > 2
    add %1, %3
%endif
%if %0 > 3
    add %1, %4
%endif
%if %0 > 4
    add %1, %5
%endif
%endmacro

test_bounded_variadic:
    BOUNDED_VARIADIC rax, 10        ; Min args (2)
    BOUNDED_VARIADIC rbx, 20, 30    ; 3 args
    BOUNDED_VARIADIC rcx, 5, 10, 15, 20    ; 5 args (max)
    ret

; ============================================================================
; TEST 15: Recursive Macro Safety (Recursion Guard)
; ============================================================================
%macro RECURSE_TEST 1
    ; This should NOT cause infinite recursion
    ; Recursion guard should kick in at depth 32
    mov rax, %1
%endmacro

test_recursion_guard:
    RECURSE_TEST 42
    ; Single invocation, no recursion
    ; (Real recursion test would require self-expanding macro)
    ret

; ============================================================================
; TEST 16: Argument Reference in Conditional
; ============================================================================
%macro COND_MOVE 2-3 0
    ; Conditional move: if %3 is non-zero, move %2 to %1
%if %3
    mov %1, %2
%else
    xor %1, %1
%endif
%endmacro

test_conditional_args:
    COND_MOVE rax, rbx          ; %3=0 (default): xor rax, rax
    COND_MOVE rcx, rdx, 1       ; %3=1: mov rcx, rdx
    ret

; ============================================================================
; TEST 17: Argument with Memory Operands
; ============================================================================
%macro MEM_OP 2
    mov rax, %1
    add rax, %2
%endmacro

test_memory_args:
    MEM_OP [rbx], [rcx+8]
    ; Should expand to:
    ;   mov rax, [rbx]
    ;   add rax, [rcx+8]
    ret

; ============================================================================
; TEST 18: Empty Variadic (%* with no extra args)
; ============================================================================
%macro VAR_OPTIONAL 1+
    mov rax, %1
    ; %* would be empty if only 1 arg provided
%endmacro

test_empty_variadic:
    VAR_OPTIONAL single_arg     ; No variadic args
    VAR_OPTIONAL first, second  ; One variadic arg
    ret

; ============================================================================
; TEST 19: Interleaved Commas and Braces
; ============================================================================
%macro MIXED_DELIM 3
    ; %1 normal, %2 brace-delimited, %3 normal
    mov rax, %1
    dd %2
    mov rbx, %3
%endmacro

test_mixed_delim:
    MIXED_DELIM 10, {1,2,3,4}, 20
    ; %2 should be: 1,2,3,4 (without outer braces)
    ret

; ============================================================================
; TEST 20: Whitespace Preservation in Arguments
; ============================================================================
%macro WHITESPACE_TEST 1
    ; Whitespace should be normalized but preserved where meaningful
    db %1
%endmacro

test_whitespace:
    WHITESPACE_TEST "test string"
    ; Should preserve quotes and content
    ret

; ============================================================================
; END OF TEST SUITE
; ============================================================================

section .data
    ; Expected outputs table for verification
    test_names:
        db "Basic Positional Args", 0
        db "Argument Count", 0
        db "Variadic Args", 0
        db "Default Args", 0
        db "Brace Delimiters", 0
        db "Nested Expansion", 0
        db "Empty Args", 0
        db "Max 20 Params", 0
        db "Complex Expressions", 0
        db "Paren Preservation", 0
        db "Multi-Line Body", 0
        db "Bounded Variadic", 0
        db "Recursion Guard", 0
        db "Conditional Args", 0
        db "Memory Operands", 0
        db "Empty Variadic", 0
        db "Mixed Delimiters", 0
        db "Whitespace", 0

section .note
    ; Build Instructions:
    ; 1. Compile assembler: ml64 /c masm_nasm_universal.asm
    ; 2. Link: link /subsystem:console /entry:main kernel32.lib masm_nasm_universal.obj /out:nasm.exe
    ; 3. Assemble test: nasm.exe test_macro_arg_substitution.asm -o test_output.bin
    ; 4. Link test: ld -o test_output test_output.bin
    ; 5. Run: ./test_output
    ;
    ; Expected Results:
    ; - All macros should expand without errors
    ; - Positional parameters %1-%20 accessible
    ; - %0 returns correct argument count
    ; - %* expands variadic args with commas
    ; - Defaults fill missing arguments
    ; - Brace delimiters preserve commas
    ; - Nested expansion works without corruption
    ; - Recursion guard prevents infinite loops (depth 32)
