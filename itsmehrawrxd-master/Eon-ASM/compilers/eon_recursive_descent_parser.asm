; Eon Recursive Descent Parser Implementation
; Alternative parsing approach using recursive descent method
; Target: NASM x86-64

%include "Eon-ASM/compilers/eon_token_definitions.asm"

section .data
    ; AST Node Types
    AST_NODE_ASSIGNMENT    equ 1
    AST_NODE_TERNARY_OP    equ 2
    AST_NODE_BINARY_OP     equ 3
    AST_NODE_UNARY_OP      equ 4
    AST_NODE_POSTFIX_OP    equ 5
    AST_NODE_PRIMARY       equ 6
    AST_NODE_FUNCTION_CALL equ 7
    AST_NODE_MEMBER_ACCESS equ 8
    AST_NODE_ARRAY_ACCESS  equ 9

section .text
    global parse_expression_recursive
    global parse_assignment
    global parse_ternary
    global parse_logical_or
    global parse_logical_and
    global parse_bitwise_or
    global parse_bitwise_xor
    global parse_bitwise_and
    global parse_equality
    global parse_relational
    global parse_shifts
    global parse_additive
    global parse_multiplicative
    global parse_unary
    global parse_postfix

; Main entry point for recursive descent parsing
parse_expression_recursive:
    push rbp
    mov rbp, rsp
    push rbx
    
    mov rbx, rdi  ; parser
    call parse_assignment
    
    pop rbx
    pop rbp
    ret

; Assignment parsing: a = b, a += b, ...
parse_assignment:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    
    mov rbx, rdi  ; parser
    
    ; Parse left-hand side
    call parse_ternary
    mov r12, rax  ; lhs
    
    ; Check for assignment operators
    mov rdi, rbx
    call get_current_token
    mov rdi, rax
    call get_token_type
    mov rcx, rax
    
    ; Check if it's an assignment operator
    cmp rcx, TOKEN_ASSIGN
    je .parse_assign
    cmp rcx, TOKEN_PLUS_ASSIGN
    je .parse_compound_assign
    cmp rcx, TOKEN_MINUS_ASSIGN
    je .parse_compound_assign
    cmp rcx, TOKEN_MULTIPLY_ASSIGN
    je .parse_compound_assign
    cmp rcx, TOKEN_DIVIDE_ASSIGN
    je .parse_compound_assign
    
    ; Not an assignment, return lhs
    mov rax, r12
    jmp .done
    
.parse_assign:
    ; Get operator token
    mov rdi, rbx
    call get_current_token
    mov r13, rax  ; operator token
    
    ; Advance past operator
    mov rdi, rbx
    call advance_token
    
    ; Parse right-hand side (right-associative)
    mov rdi, rbx
    call parse_assignment
    mov r14, rax  ; rhs
    
    ; Create assignment node
    mov rdi, AST_NODE_ASSIGNMENT
    call create_ast_node
    mov rdx, rax  ; node
    
    ; Set node properties
    mov rdi, rdx
    mov rsi, r13  ; operator token
    call set_node_operator
    
    mov rdi, rdx
    mov rsi, r12  ; lhs
    call set_node_left
    
    mov rdi, rdx
    mov rsi, r14  ; rhs
    call set_node_right
    
    mov rax, rdx
    jmp .done
    
.parse_compound_assign:
    ; Get operator token
    mov rdi, rbx
    call get_current_token
    mov r13, rax  ; operator token
    
    ; Advance past operator
    mov rdi, rbx
    call advance_token
    
    ; Parse right-hand side (right-associative)
    mov rdi, rbx
    call parse_assignment
    mov r14, rax  ; rhs
    
    ; Create assignment node
    mov rdi, AST_NODE_ASSIGNMENT
    call create_ast_node
    mov rdx, rax  ; node
    
    ; Set node properties
    mov rdi, rdx
    mov rsi, r13  ; operator token
    call set_node_operator
    
    mov rdi, rdx
    mov rsi, r12  ; lhs
    call set_node_left
    
    mov rdi, rdx
    mov rsi, r14  ; rhs
    call set_node_right
    
    mov rax, rdx
    jmp .done
    
.done:
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; Ternary operator parsing: cond ? a : b
parse_ternary:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    
    mov rbx, rdi  ; parser
    
    ; Parse condition
    call parse_logical_or
    mov r12, rax  ; condition
    
    ; Check for '?'
    mov rdi, rbx
    call get_current_token
    mov rdi, rax
    call get_token_type
    cmp rax, TOKEN_QUESTION
    jne .not_ternary
    
    ; Advance past '?'
    mov rdi, rbx
    call advance_token
    
    ; Parse true branch
    mov rdi, rbx
    call parse_expression_recursive
    mov r13, rax  ; true branch
    
    ; Expect ':'
    mov rdi, rbx
    call get_current_token
    mov rdi, rax
    call get_token_type
    cmp rax, TOKEN_COLON
    jne .ternary_error
    
    ; Advance past ':'
    mov rdi, rbx
    call advance_token
    
    ; Parse false branch
    mov rdi, rbx
    call parse_expression_recursive
    mov r14, rax  ; false branch
    
    ; Create ternary node
    mov rdi, AST_NODE_TERNARY_OP
    call create_ast_node
    mov rdx, rax  ; node
    
    ; Set node properties
    mov rdi, rdx
    mov rsi, r12  ; condition
    call set_node_condition
    
    mov rdi, rdx
    mov rsi, r13  ; true branch
    call set_node_left
    
    mov rdi, rdx
    mov rsi, r14  ; false branch
    call set_node_right
    
    mov rax, rdx
    jmp .done
    
.not_ternary:
    mov rax, r12
    jmp .done
    
.ternary_error:
    mov rdi, rbx
    call report_parse_error
    xor rax, rax
    
.done:
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; Logical OR parsing: a || b
parse_logical_or:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    
    mov rbx, rdi  ; parser
    
    ; Parse left operand
    call parse_logical_and
    mov r12, rax  ; left
    
    ; Parse right operands in loop
    mov r13, r12  ; accumulator
    
.loop:
    ; Check for LOGICAL_OR
    mov rdi, rbx
    call get_current_token
    mov rdi, rax
    call get_token_type
    cmp rax, TOKEN_LOGICAL_OR
    jne .done
    
    ; Get operator token
    mov rdi, rbx
    call get_current_token
    mov r14, rax  ; operator
    
    ; Advance past operator
    mov rdi, rbx
    call advance_token
    
    ; Parse right operand
    call parse_logical_and
    mov rdx, rax  ; right
    
    ; Create binary node
    mov rdi, AST_NODE_BINARY_OP
    call create_ast_node
    mov rcx, rax  ; node
    
    ; Set node properties
    mov rdi, rcx
    mov rsi, r14  ; operator
    call set_node_operator
    
    mov rdi, rcx
    mov rsi, r13  ; left
    call set_node_left
    
    mov rdi, rcx
    mov rsi, rdx  ; right
    call set_node_right
    
    mov r13, rcx  ; update accumulator
    jmp .loop
    
.done:
    mov rax, r13
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; Logical AND parsing: a && b
parse_logical_and:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    
    mov rbx, rdi  ; parser
    
    ; Parse left operand
    call parse_bitwise_or
    mov r12, rax  ; left
    
    ; Parse right operands in loop
    mov r13, r12  ; accumulator
    
.loop:
    ; Check for LOGICAL_AND
    mov rdi, rbx
    call get_current_token
    mov rdi, rax
    call get_token_type
    cmp rax, TOKEN_LOGICAL_AND
    jne .done
    
    ; Get operator token
    mov rdi, rbx
    call get_current_token
    mov r14, rax  ; operator
    
    ; Advance past operator
    mov rdi, rbx
    call advance_token
    
    ; Parse right operand
    call parse_bitwise_or
    mov rdx, rax  ; right
    
    ; Create binary node
    mov rdi, AST_NODE_BINARY_OP
    call create_ast_node
    mov rcx, rax  ; node
    
    ; Set node properties
    mov rdi, rcx
    mov rsi, r14  ; operator
    call set_node_operator
    
    mov rdi, rcx
    mov rsi, r13  ; left
    call set_node_left
    
    mov rdi, rcx
    mov rsi, rdx  ; right
    call set_node_right
    
    mov r13, rcx  ; update accumulator
    jmp .loop
    
.done:
    mov rax, r13
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; Bitwise OR parsing: a | b
parse_bitwise_or:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    
    mov rbx, rdi  ; parser
    
    ; Parse left operand
    call parse_bitwise_xor
    mov r12, rax  ; left
    
    ; Parse right operands in loop
    mov r13, r12  ; accumulator
    
.loop:
    ; Check for BITWISE_OR
    mov rdi, rbx
    call get_current_token
    mov rdi, rax
    call get_token_type
    cmp rax, TOKEN_BITWISE_OR
    jne .done
    
    ; Get operator token
    mov rdi, rbx
    call get_current_token
    mov r14, rax  ; operator
    
    ; Advance past operator
    mov rdi, rbx
    call advance_token
    
    ; Parse right operand
    call parse_bitwise_xor
    mov rdx, rax  ; right
    
    ; Create binary node
    mov rdi, AST_NODE_BINARY_OP
    call create_ast_node
    mov rcx, rax  ; node
    
    ; Set node properties
    mov rdi, rcx
    mov rsi, r14  ; operator
    call set_node_operator
    
    mov rdi, rcx
    mov rsi, r13  ; left
    call set_node_left
    
    mov rdi, rcx
    mov rsi, rdx  ; right
    call set_node_right
    
    mov r13, rcx  ; update accumulator
    jmp .loop
    
.done:
    mov rax, r13
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; Bitwise XOR parsing: a ^ b
parse_bitwise_xor:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    
    mov rbx, rdi  ; parser
    
    ; Parse left operand
    call parse_bitwise_and
    mov r12, rax  ; left
    
    ; Parse right operands in loop
    mov r13, r12  ; accumulator
    
.loop:
    ; Check for BITWISE_XOR
    mov rdi, rbx
    call get_current_token
    mov rdi, rax
    call get_token_type
    cmp rax, TOKEN_BITWISE_XOR
    jne .done
    
    ; Get operator token
    mov rdi, rbx
    call get_current_token
    mov r14, rax  ; operator
    
    ; Advance past operator
    mov rdi, rbx
    call advance_token
    
    ; Parse right operand
    call parse_bitwise_and
    mov rdx, rax  ; right
    
    ; Create binary node
    mov rdi, AST_NODE_BINARY_OP
    call create_ast_node
    mov rcx, rax  ; node
    
    ; Set node properties
    mov rdi, rcx
    mov rsi, r14  ; operator
    call set_node_operator
    
    mov rdi, rcx
    mov rsi, r13  ; left
    call set_node_left
    
    mov rdi, rcx
    mov rsi, rdx  ; right
    call set_node_right
    
    mov r13, rcx  ; update accumulator
    jmp .loop
    
.done:
    mov rax, r13
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; Bitwise AND parsing: a & b
parse_bitwise_and:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    
    mov rbx, rdi  ; parser
    
    ; Parse left operand
    call parse_equality
    mov r12, rax  ; left
    
    ; Parse right operands in loop
    mov r13, r12  ; accumulator
    
.loop:
    ; Check for BITWISE_AND
    mov rdi, rbx
    call get_current_token
    mov rdi, rax
    call get_token_type
    cmp rax, TOKEN_BITWISE_AND
    jne .done
    
    ; Get operator token
    mov rdi, rbx
    call get_current_token
    mov r14, rax  ; operator
    
    ; Advance past operator
    mov rdi, rbx
    call advance_token
    
    ; Parse right operand
    call parse_equality
    mov rdx, rax  ; right
    
    ; Create binary node
    mov rdi, AST_NODE_BINARY_OP
    call create_ast_node
    mov rcx, rax  ; node
    
    ; Set node properties
    mov rdi, rcx
    mov rsi, r14  ; operator
    call set_node_operator
    
    mov rdi, rcx
    mov rsi, r13  ; left
    call set_node_left
    
    mov rdi, rcx
    mov rsi, rdx  ; right
    call set_node_right
    
    mov r13, rcx  ; update accumulator
    jmp .loop
    
.done:
    mov rax, r13
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; Equality parsing: a == b, a != b
parse_equality:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    
    mov rbx, rdi  ; parser
    
    ; Parse left operand
    call parse_relational
    mov r12, rax  ; left
    
    ; Parse right operands in loop
    mov r13, r12  ; accumul                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               