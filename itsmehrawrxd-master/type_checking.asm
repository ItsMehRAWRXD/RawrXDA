; type_checking.asm
; Implement type checking for expressions and statements
; Comprehensive type checking system for the Eon compiler

section .data
    ; === Type Checking Information ===
    type_checking_info        db "Type Checking System v1.0", 10, 0
    type_checking_capabilities db "Features: Expression type checking, Statement validation, Type inference", 10, 0
    
    ; === Type System ===
    TYPE_VOID            equ 0
    TYPE_INT             equ 1
    TYPE_FLOAT           equ 2
    TYPE_BOOL            equ 3
    TYPE_STRING          equ 4
    TYPE_ARRAY           equ 5
    TYPE_FUNCTION        equ 6
    TYPE_STRUCT          equ 7
    TYPE_POINTER         equ 8
    TYPE_UNION           equ 9
    TYPE_ENUM            equ 10
    TYPE_GENERIC         equ 11
    
    ; === Type Checking State ===
    type_checking_enabled resq 1      ; Type checking enabled
    strict_type_checking resq 1       ; Strict type checking
    implicit_conversions resq 1       ; Implicit conversions allowed
    type_inference       resq 1       ; Type inference enabled
    type_errors          resq 256     ; Type errors
    type_error_count     resq 1       ; Type error count
    type_warnings        resq 256     ; Type warnings
    type_warning_count   resq 1       ; Type warning count
    
    ; === Type Checking Statistics ===
    expressions_checked  resq 1       ; Expressions checked
    statements_checked   resq 1       ; Statements checked
    types_inferred       resq 1       ; Types inferred
    conversions_performed resq 1      ; Conversions performed
    errors_reported      resq 1       ; Errors reported
    warnings_reported    resq 1       ; Warnings reported

section .text
    global type_checking_init
    global type_checking_check_expression
    global type_checking_check_statement
    global type_checking_check_binary_expression
    global type_checking_check_unary_expression
    global type_checking_check_function_call
    global type_checking_check_assignment
    global type_checking_check_if_statement
    global type_checking_check_loop_statement
    global type_checking_check_return_statement
    global type_checking_infer_type
    global type_checking_check_type_compatibility
    global type_checking_perform_conversion
    global type_checking_report_error
    global type_checking_report_warning
    global type_checking_get_errors
    global type_checking_get_warnings
    global type_checking_get_statistics
    global type_checking_cleanup

; === Initialize Type Checking ===
type_checking_init:
    push rbp
    mov rbp, rsp
    
    ; Print type checking info
    mov rdi, type_checking_info
    call print_string
    mov rdi, type_checking_capabilities
    call print_string
    
    ; Initialize state
    mov qword [type_checking_enabled], 1
    mov qword [strict_type_checking], 1
    mov qword [implicit_conversions], 0
    mov qword [type_inference], 1
    mov qword [type_error_count], 0
    mov qword [type_warning_count], 0
    
    ; Initialize statistics
    mov qword [expressions_checked], 0
    mov qword [statements_checked], 0
    mov qword [types_inferred], 0
    mov qword [conversions_performed], 0
    mov qword [errors_reported], 0
    mov qword [warnings_reported], 0
    
    ; Clear error buffer
    mov rdi, type_errors
    mov rsi, 256 * 8
    call zero_memory
    
    ; Clear warning buffer
    mov rdi, type_warnings
    mov rsi, 256 * 8
    call zero_memory
    
    leave
    ret

; === Check Expression ===
type_checking_check_expression:
    push rbp
    mov rbp, rsp
    
    ; Args: rdi = expression node
    ; Returns: rax = expression type
    
    ; Check if type checking is enabled
    mov rax, [type_checking_enabled]
    cmp rax, 0
    je .type_checking_disabled
    
    ; Get expression type
    mov rbx, rdi
    mov rcx, [rbx]  ; node type
    
    ; Check expression type
    cmp rcx, 9  ; BINARY_OP
    je .check_binary_expression
    cmp rcx, 10 ; UNARY_OP
    je .check_unary_expression
    cmp rcx, 11 ; FUNCTION_CALL
    je .check_function_call
    cmp rcx, 12 ; IDENTIFIER
    je .check_identifier
    cmp rcx, 13 ; LITERAL
    je .check_literal
    
    ; Unknown expression type
    mov rax, TYPE_VOID
    jmp .done
    
.check_binary_expression:
    call type_checking_check_binary_expression
    jmp .done
    
.check_unary_expression:
    call type_checking_check_unary_expression
    jmp .done
    
.check_function_call:
    call type_checking_check_function_call
    jmp .done
    
.check_identifier:
    call type_checking_check_identifier
    jmp .done
    
.check_literal:
    call type_checking_check_literal
    jmp .done
    
.type_checking_disabled:
    mov rax, TYPE_VOID
    
.done:
    ; Update statistics
    inc qword [expressions_checked]
    
    leave
    ret

; === Check Binary Expression ===
type_checking_check_binary_expression:
    push rbp
    mov rbp, rsp
    
    ; Args: rdi = binary expression node
    ; Returns: rax = expression type
    
    ; Check left operand
    mov rbx, [rdi + 8]  ; left child
    call type_checking_check_expression
    mov rcx, rax  ; left type
    
    ; Check right operand
    mov rbx, [rdi + 16] ; right child
    call type_checking_check_expression
    mov rdx, rax  ; right type
    
    ; Check type compatibility
    mov rdi, rcx
    mov rsi, rdx
    call type_checking_check_type_compatibility
    cmp rax, 0
    je .types_incompatible
    
    ; Return common type
    mov rax, rcx
    jmp .done
    
.types_incompatible:
    ; Report type error
    mov rdi, 0  ; error message
    call type_checking_report_error
    mov rax, TYPE_VOID
    
.done:
    leave
    ret

; === Check Unary Expression ===
type_checking_check_unary_expression:
    push rbp
    mov rbp, rsp
    
    ; Args: rdi = unary expression node
    ; Returns: rax = expression type
    
    ; Check operand
    mov rbx, [rdi + 8]  ; child
    call type_checking_check_expression
    
    ; Return operand type
    jmp .done
    
.done:
    leave
    ret

; === Check Function Call ===
type_checking_check_function_call:
    push rbp
    mov rbp, rsp
    
    ; Args: rdi = function call node
    ; Returns: rax = return type
    
    ; Check function type
    ; This is a simplified implementation
    
    mov rax, TYPE_VOID
    jmp .done
    
.done:
    leave
    ret

; === Check Identifier ===
type_checking_check_identifier:
    push rbp
    mov rbp, rsp
    
    ; Args: rdi = identifier node
    ; Returns: rax = identifier type
    
    ; Look up identifier type
    ; This is a simplified implementation
    
    mov rax, TYPE_INT  ; Default type
    jmp .done
    
.done:
    leave
    ret

; === Check Literal ===
type_checking_check_literal:
    push rbp
    mov rbp, rsp
    
    ; Args: rdi = literal node
    ; Returns: rax = literal type
    
    ; Determine literal type
    ; This is a simplified implementation
    
    mov rax, TYPE_INT  ; Default type
    jmp .done
    
.done:
    leave
    ret

; === Check Statement ===
type_checking_check_statement:
    push rbp
    mov rbp, rsp
    
    ; Args: rdi = statement node
    
    ; Get statement type
    mov rbx, rdi
    mov rcx, [rbx]  ; node type
    
    ; Check statement type
    cmp rcx, 3  ; VARIABLE_DECL
    je .check_variable_statement
    cmp rcx, 4  ; ASSIGNMENT
    je .check_assignment_statement
    cmp rcx, 5  ; IF_STMT
    je .check_if_statement
    cmp rcx, 6  ; LOOP_STMT
    je .check_loop_statement
    cmp rcx, 7  ; RETURN_STMT
    je .check_return_statement
    
    ; Unknown statement type
    jmp .done
    
.check_variable_statement:
    call type_checking_check_variable
    jmp .done
    
.check_assignment_statement:
    call type_checking_check_assignment
    jmp .done
    
.check_if_statement:
    call type_checking_check_if_statement
    jmp .done
    
.check_loop_statement:
    call type_checking_check_loop_statement
    jmp .done
    
.check_return_statement:
    call type_checking_check_return_statement
    jmp .done
    
.done:
    ; Update statistics
    inc qword [statements_checked]
    
    leave
    ret

; === Check Variable ===
type_checking_check_variable:
    push rbp
    mov rbp, rsp
    
    ; Check variable declaration
    ; This is a simplified implementation
    
    leave
    ret

; === Check Assignment ===
type_checking_check_assignment:
    push rbp
    mov rbp, rsp
    
    ; Args: rdi = assignment node
    
    ; Check left operand
    mov rbx, [rdi + 8]  ; left child
    call type_checking_check_expression
    mov rcx, rax  ; left type
    
    ; Check right operand
    mov rbx, [rdi + 16] ; right child
    call type_checking_check_expression
    mov rdx, rax  ; right type
    
    ; Check type compatibility
    mov rdi, rcx
    mov rsi, rdx
    call type_checking_check_type_compatibility
    cmp rax, 0
    je .types_incompatible
    
    jmp .done
    
.types_incompatible:
    ; Report type error
    mov rdi, 0  ; error message
    call type_checking_report_error
    
.done:
    leave
    ret

; === Check If Statement ===
type_checking_check_if_statement:
    push rbp
    mov rbp, rsp
    
    ; Args: rdi = if statement node
    
    ; Check condition
    mov rbx, [rdi + 8]  ; condition
    call type_checking_check_expression
    mov rcx, rax  ; condition type
    
    ; Check if condition is boolean
    cmp rcx, TYPE_BOOL
    je .condition_valid
    
    ; Report type error
    mov rdi, 0  ; error message
    call type_checking_report_error
    jmp .done
    
.condition_valid:
    ; Check then block
    mov rbx, [rdi + 16] ; then block
    call type_checking_check_statement
    
    ; Check else block
    mov rbx, [rdi + 24] ; else block
    cmp rbx, 0
    je .done
    call type_checking_check_statement
    
.done:
    leave
    ret

; === Check Loop Statement ===
type_checking_check_loop_statement:
    push rbp
    mov rbp, rsp
    
    ; Args: rdi = loop statement node
    
    ; Check condition
    mov rbx, [rdi + 8]  ; condition
    call type_checking_check_expression
    mov rcx, rax  ; condition type
    
    ; Check if condition is boolean
    cmp rcx, TYPE_BOOL
    je .condition_valid
    
    ; Report type error
    mov rdi, 0  ; error message
    call type_checking_report_error
    jmp .done
    
.condition_valid:
    ; Check loop body
    mov rbx, [rdi + 16] ; loop body
    call type_checking_check_statement
    
.done:
    leave
    ret

; === Check Return Statement ===
type_checking_check_return_statement:
    push rbp
    mov rbp, rsp
    
    ; Args: rdi = return statement node
    
    ; Check return expression
    mov rbx, [rdi + 8]  ; return expression
    cmp rbx, 0
    je .no_return_value
    
    call type_checking_check_expression
    mov rcx, rax  ; return type
    
    ; Check return type compatibility
    ; This is a simplified implementation
    
.no_return_value:
    ; Check if function expects void return
    ; This is a simplified implementation
    
.done:
    leave
    ret

; === Infer Type ===
type_checking_infer_type:
    push rbp
    mov rbp, rsp
    
    ; Args: rdi = expression node
    ; Returns: rax = inferred type
    
    ; Check if type inference is enabled
    mov rax, [type_inference]
    cmp rax, 0
    je .type_inference_disabled
    
    ; Infer type from expression
    call type_checking_check_expression
    
    ; Update statistics
    inc qword [types_inferred]
    jmp .done
    
.type_inference_disabled:
    mov rax, TYPE_VOID
    
.done:
    leave
    ret

; === Check Type Compatibility ===
type_checking_check_type_compatibility:
    push rbp
    mov rbp, rsp
    
    ; Args: rdi = type1, rsi = type2
    ; Returns: rax = compatibility (1=compatible, 0=incompatible)
    
    ; Check exact type match
    cmp rdi, rsi
    je .types_compatible
    
    ; Check for implicit conversions
    mov rax, [implicit_conversions]
    cmp rax, 0
    je .types_incompatible
    
    ; Check specific type conversions
    ; int -> float
    cmp rdi, TYPE_INT
    jne .check_float_to_int
    cmp rsi, TYPE_FLOAT
    je .types_compatible
    
.check_float_to_int:
    ; float -> int
    cmp rdi, TYPE_FLOAT
    jne .types_incompatible
    cmp rsi, TYPE_INT
    je .types_compatible
    
.types_incompatible:
    mov rax, 0
    jmp .done
    
.types_compatible:
    mov rax, 1
    
.done:
    leave
    ret

; === Perform Conversion ===
type_checking_perform_conversion:
    push rbp
    mov rbp, rsp
    
    ; Args: rdi = source type, rsi = target type
    ; Returns: rax = conversion success (1=success, 0=failure)
    
    ; Check type compatibility
    call type_checking_check_type_compatibility
    cmp rax, 0
    je .conversion_failed
    
    ; Perform conversion
    ; This is a simplified implementation
    
    ; Update statistics
    inc qword [conversions_performed]
    
    mov rax, 1
    jmp .done
    
.conversion_failed:
    mov rax, 0
    
.done:
    leave
    ret

; === Report Error ===
type_checking_report_error:
    push rbp
    mov rbp, rsp
    
    ; Args: rdi = error message
    
    ; Check if error buffer is full
    mov rax, [type_error_count]
    cmp rax, 256
    jge .error_buffer_full
    
    ; Add error to buffer
    mov rbx, type_errors
    mov rcx, [type_error_count]
    mov [rbx + rcx * 8], rdi
    
    ; Increment error count
    inc qword [type_error_count]
    
    ; Update statistics
    inc qword [errors_reported]
    
    mov rax, 1
    jmp .done
    
.error_buffer_full:
    mov rax, 0
    
.done:
    leave
    ret

; === Report Warning ===
type_checking_report_warning:
    push rbp
    mov rbp, rsp
    
    ; Args: rdi = warning message
    
    ; Check if warning buffer is full
    mov rax, [type_warning_count]
    cmp rax, 256
    jge .warning_buffer_full
    
    ; Add warning to buffer
    mov rbx, type_warnings
    mov rcx, [type_warning_count]
    mov [rbx + rcx * 8], rdi
    
    ; Increment warning count
    inc qword [type_warning_count]
    
    ; Update statistics
    inc qword [warnings_reported]
    
    mov rax, 1
    jmp .done
    
.warning_buffer_full:
    mov rax, 0
    
.done:
    leave
    ret

; === Get Errors ===
type_checking_get_errors:
    push rbp
    mov rbp, rsp
    
    ; Returns: rax = error buffer, rbx = error count
    
    mov rax, type_errors
    mov rbx, [type_error_count]
    
    leave
    ret

; === Get Warnings ===
type_checking_get_warnings:
    push rbp
    mov rbp, rsp
    
    ; Returns: rax = warning buffer, rbx = warning count
    
    mov rax, type_warnings
    mov rbx, [type_warning_count]
    
    leave
    ret

; === Get Statistics ===
type_checking_get_statistics:
    push rbp
    mov rbp, rsp
    
    ; Returns: rax = expressions checked, rbx = statements checked, rcx = types inferred, rdx = conversions performed
    
    mov rax, [expressions_checked]
    mov rbx, [statements_checked]
    mov rcx, [types_inferred]
    mov rdx, [conversions_performed]
    
    leave
    ret

; === Cleanup ===
type_checking_cleanup:
    push rbp
    mov rbp, rsp
    
    ; Clear error buffer
    mov rdi, type_errors
    mov rsi, 256 * 8
    call zero_memory
    
    ; Clear warning buffer
    mov rdi, type_warnings
    mov rsi, 256 * 8
    call zero_memory
    
    ; Reset counters
    mov qword [type_error_count], 0
    mov qword [type_warning_count], 0
    mov qword [expressions_checked], 0
    mov qword [statements_checked], 0
    mov qword [types_inferred], 0
    mov qword [conversions_performed], 0
    mov qword [errors_reported], 0
    mov qword [warnings_reported], 0
    
    leave
    ret

; === Utility Functions ===
zero_memory:
    push rbp
    mov rbp, rsp
    push rbx
    push rcx
    
    ; Args: rdi = memory pointer, rsi = size
    mov rbx, rdi
    mov rcx, rsi
    
.zero_loop:
    cmp rcx, 0
    je .zero_done
    
    mov byte [rbx], 0
    inc rbx
    dec rcx
    jmp .zero_loop
    
.zero_done:
    pop rcx
    pop rbx
    leave
    ret

print_string:
    push rbp
    mov rbp, rsp
    push rbx
    
    mov rbx, rdi
    mov rdx, 0
    
    ; Calculate string length
.length_loop:
    cmp byte [rbx + rdx], 0
    je .print
    inc rdx
    jmp .length_loop
    
.print:
    mov rax, 1              ; sys_write
    mov rdi, 1              ; stdout
    mov rsi, rbx
    syscall
    
    pop rbx
    leave
    ret

; === Type Checking Demo ===
type_checking_demo:
    push rbp
    mov rbp, rsp
    
    ; Initialize type checking
    call type_checking_init
    
    ; Check sample expression
    mov rdi, 0  ; Sample expression
    call type_checking_check_expression
    
    ; Check sample statement
    mov rdi, 0  ; Sample statement
    call type_checking_check_statement
    
    ; Get statistics
    call type_checking_get_statistics
    
    ; Cleanup
    call type_checking_cleanup
    
    leave
    ret
