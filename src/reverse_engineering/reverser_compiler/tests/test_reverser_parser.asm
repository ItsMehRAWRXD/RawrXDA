; test_reverser_parser.asm
; Test program for the Reverser language parser
; Demonstrates AST generation from Reverser source code

%include "reverser_parser.asm"

section .data
    ; Test Reverser source code
    test_source db 'cnuf main() -> tni { ter 42 }', 0
    test_source_len equ $ - test_source - 1
    
    ; Output messages
    msg_parsing db 'Parsing Reverser source:', 10, 0
    msg_ast_root db 'AST Root Node:', 10, 0
    msg_node_type db 'Node Type: ', 0
    msg_node_value db 'Value: ', 0
    msg_children db 'Children: ', 0
    msg_newline db 10, 0
    msg_separator db '---', 10, 0
    msg_success db 'Parser test completed successfully.', 10, 0
    msg_error db 'Parser test failed.', 10, 0

section .bss
    ; Parser state
    parser_root_node resq 1
    test_result      resq 1

section .text
    global _start
    extern parser_init
    extern parser_parse
    extern ast_get_type
    extern ast_get_child
    extern ast_get_value

; Print a string
; Input: rdi = pointer to string, rsi = string length
print_string:
    push rax
    push rdx
    push rcx
    
    mov rax, 1      ; syscall number for write
    mov rdx, rsi    ; string length
    mov rsi, rdi    ; string pointer
    mov rdi, 1      ; file descriptor (stdout)
    syscall
    
    pop rcx
    pop rdx
    pop rax
    ret

; Print a number
; Input: rdi = number to print
print_number:
    push rbx
    push rcx
    push rdx
    push rsi
    
    mov rbx, rdi    ; Save number
    mov rcx, 0      ; Digit count
    mov rsi, 10     ; Base
    
    ; Convert to string
    mov rax, rbx
.convert_loop:
    cmp rax, 0
    je .print_loop
    xor rdx, rdx
    div rsi
    add rdx, '0'
    push rdx
    inc rcx
    jmp .convert_loop

.print_loop:
    cmp rcx, 0
    je .done
    pop rdx
    mov [test_result], rdx
    mov rdi, test_result
    mov rsi, 1
    call print_string
    dec rcx
    jmp .print_loop

.done:
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

; Print AST node information
; Input: rdi = node pointer
print_ast_node:
    push rbx
    push rcx
    push rdx
    push rsi
    
    mov rbx, rdi    ; Save node pointer
    
    ; Print node type
    mov rdi, msg_node_type
    mov rsi, 11
    call print_string
    
    mov rdi, rbx
    call ast_get_type
    mov rdi, rax
    call print_number
    
    mov rdi, msg_newline
    mov rsi, 1
    call print_string
    
    ; Print node value if it exists
    mov rdi, rbx
    call ast_get_value
    cmp rax, 0
    je .no_value
    
    mov rdi, msg_node_value
    mov rsi, 7
    call print_string
    
    ; Print value (simplified)
    mov rdi, rax
    mov rsi, rdx
    call print_string
    
    mov rdi, msg_newline
    mov rsi, 1
    call print_string

.no_value:
    ; Print children count
    mov rdi, msg_children
    mov rsi, 10
    call print_string
    
    mov rdi, rbx
    mov rsi, 0
    call ast_get_child
    cmp rax, 0
    je .no_children
    
    ; Print first child (simplified)
    mov rdi, rax
    call print_ast_node

.no_children:
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

; Main entry point
_start:
    ; Print test header
    mov rdi, msg_parsing
    mov rsi, 25
    call print_string
    
    ; Print source code
    mov rdi, test_source
    mov rsi, test_source_len
    call print_string
    
    mov rdi, msg_newline
    mov rsi, 1
    call print_string
    
    ; Initialize parser
    mov rdi, test_source
    mov rsi, test_source_len
    call parser_init
    
    ; Parse the source
    call parser_parse
    mov [parser_root_node], rax
    
    ; Check if parsing succeeded
    cmp rax, 0
    je .error
    
    ; Print AST information
    mov rdi, msg_ast_root
    mov rsi, 14
    call print_string
    
    mov rdi, [parser_root_node]
    call print_ast_node
    
    ; Print success message
    mov rdi, msg_success
    mov rsi, 35
    call print_string
    
    jmp .exit

.error:
    ; Print error message
    mov rdi, msg_error
    mov rsi, 20
    call print_string

.exit:
    ; Exit
    mov rax, 60     ; syscall number for exit
    xor rdi, rdi    ; exit code 0
    syscall
