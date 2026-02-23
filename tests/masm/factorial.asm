; ============================================================================
; factorial.asm - Factorial Calculation Test Program
; Tests arithmetic operations, procedure calls, and recursion
; ============================================================================

.data
    result      dq 0
    testValue   dq 10
    message     db 'Calculating factorial of 10...', 13, 10, 0
    msgLen      equ $ - message

.code
    extern GetStdHandle
    extern WriteFile
    extern ExitProcess

; ============================================================================
; Factorial Function (Recursive)
; Input:  rcx = n
; Output: rax = n!
; ============================================================================
factorial proc
    push rbp
    mov rbp, rsp
    push rbx
    
    ; Base case: if n <= 1, return 1
    cmp rcx, 1
    jg .recursive
    
    mov rax, 1                      ; Return 1
    jmp .done
    
.recursive:
    ; Save n
    mov rbx, rcx
    
    ; Calculate (n-1)!
    dec rcx
    call factorial
    
    ; Multiply result by n
    imul rax, rbx
    
.done:
    pop rbx
    pop rbp
    ret
factorial endp

; ============================================================================
; Main Entry Point
; ============================================================================
main proc
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    ; Print message
    mov rcx, -11
    call GetStdHandle
    mov rbx, rax
    
    mov rcx, rbx
    lea rdx, [message]
    mov r8, msgLen
    lea r9, [rsp+48]
    mov qword ptr [rsp+32], 0
    call WriteFile
    
    ; Calculate factorial(10)
    mov rcx, [testValue]
    call factorial
    
    ; Store result
    mov [result], rax
    
    ; Exit with success
    xor rcx, rcx
    add rsp, 64
    pop rbp
    call ExitProcess
    
    ret
main endp

end main
