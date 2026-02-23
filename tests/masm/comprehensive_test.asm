; ============================================================================
; comprehensive_test.asm - Comprehensive MASM Feature Test
; Tests all major features of the MASM compiler
; ============================================================================

.data
    ; String data
    welcome     db 'MASM Compiler Comprehensive Test', 13, 10, 0
    testMsg1    db 'Testing arithmetic operations...', 13, 10, 0
    testMsg2    db 'Testing memory operations...', 13, 10, 0
    testMsg3    db 'Testing control flow...', 13, 10, 0
    testMsg4    db 'Testing procedure calls...', 13, 10, 0
    doneMsg     db 'All tests completed!', 13, 10, 0
    
    ; Numeric data
    num1        dq 42
    num2        dq 17
    array       dq 1, 2, 3, 4, 5, 6, 7, 8, 9, 10
    arraySize   equ 10
    
    ; Results
    addResult   dq 0
    subResult   dq 0
    mulResult   dq 0
    arraySum    dq 0

.code
    extern GetStdHandle
    extern WriteFile
    extern ExitProcess

; ============================================================================
; Print String Helper
; Input: rdx = pointer to string
; ============================================================================
print_string proc
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    sub rsp, 48
    
    mov rsi, rdx                    ; Save string pointer
    
    ; Get stdout
    mov rcx, -11
    call GetStdHandle
    mov rbx, rax
    
    ; Calculate string length
    mov rcx, rsi
    call string_length
    mov r8, rax                     ; Length in r8
    
    ; Write to stdout
    mov rcx, rbx
    mov rdx, rsi
    lea r9, [rsp+48]
    mov qword ptr [rsp+32], 0
    call WriteFile
    
    add rsp, 48
    pop rsi
    pop rbx
    pop rbp
    ret
print_string endp

; ============================================================================
; String Length Helper
; Input: rcx = pointer to string
; Output: rax = length
; ============================================================================
string_length proc
    push rdi
    mov rdi, rcx
    xor rax, rax
    
.loop:
    cmp byte ptr [rdi], 0
    je .done
    inc rdi
    inc rax
    jmp .loop
    
.done:
    pop rdi
    ret
string_length endp

; ============================================================================
; Test Arithmetic Operations
; ============================================================================
test_arithmetic proc
    push rbp
    mov rbp, rsp
    
    ; Addition
    mov rax, [num1]
    add rax, [num2]
    mov [addResult], rax
    
    ; Subtraction
    mov rax, [num1]
    sub rax, [num2]
    mov [subResult], rax
    
    ; Multiplication
    mov rax, [num1]
    imul rax, [num2]
    mov [mulResult], rax
    
    pop rbp
    ret
test_arithmetic endp

; ============================================================================
; Test Memory Operations
; ============================================================================
test_memory proc
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    
    ; Sum array
    lea rsi, [array]
    xor rbx, rbx                    ; Sum = 0
    mov rcx, arraySize              ; Counter
    
.loop:
    mov rax, [rsi]
    add rbx, rax
    add rsi, 8
    loop .loop
    
    mov [arraySum], rbx
    
    pop rsi
    pop rbx
    pop rbp
    ret
test_memory endp

; ============================================================================
; Test Control Flow
; ============================================================================
test_control_flow proc
    push rbp
    mov rbp, rsp
    
    ; Test conditional jumps
    mov rax, [num1]
    cmp rax, [num2]
    jg .greater
    jl .less
    je .equal
    
.greater:
    ; num1 > num2
    jmp .done
    
.less:
    ; num1 < num2
    jmp .done
    
.equal:
    ; num1 == num2
    
.done:
    pop rbp
    ret
test_control_flow endp

; ============================================================================
; Fibonacci Function (Recursive)
; Input: rcx = n
; Output: rax = fib(n)
; ============================================================================
fibonacci proc
    push rbp
    mov rbp, rsp
    push rbx
    
    ; Base cases
    cmp rcx, 0
    je .zero
    cmp rcx, 1
    je .one
    
    ; Recursive case: fib(n-1) + fib(n-2)
    mov rbx, rcx                    ; Save n
    
    dec rcx
    call fibonacci                  ; fib(n-1)
    push rax                        ; Save result
    
    mov rcx, rbx
    sub rcx, 2
    call fibonacci                  ; fib(n-2)
    
    pop rbx                         ; Get fib(n-1)
    add rax, rbx                    ; fib(n-1) + fib(n-2)
    jmp .done
    
.zero:
    xor rax, rax
    jmp .done
    
.one:
    mov rax, 1
    
.done:
    pop rbx
    pop rbp
    ret
fibonacci endp

; ============================================================================
; Main Entry Point
; ============================================================================
main proc
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Print welcome message
    lea rdx, [welcome]
    call print_string
    
    ; Test 1: Arithmetic
    lea rdx, [testMsg1]
    call print_string
    call test_arithmetic
    
    ; Test 2: Memory operations
    lea rdx, [testMsg2]
    call print_string
    call test_memory
    
    ; Test 3: Control flow
    lea rdx, [testMsg3]
    call print_string
    call test_control_flow
    
    ; Test 4: Procedure calls (Fibonacci)
    lea rdx, [testMsg4]
    call print_string
    mov rcx, 10
    call fibonacci
    
    ; Print completion message
    lea rdx, [doneMsg]
    call print_string
    
    ; Exit with success
    xor rcx, rcx
    add rsp, 32
    pop rbp
    call ExitProcess
    
    ret
main endp

end main
