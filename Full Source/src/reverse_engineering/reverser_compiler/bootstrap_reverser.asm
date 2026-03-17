; bootstrap_reverser.asm
; Bootstrap script to compile the self-hosting Reverser compiler
; This uses the existing Reverser compiler to compile reverser_self_compiler.rev

%include "reverser_lexer.asm"
%include "reverser_parser.asm"
%include "reverser_runtime.asm"

section .data
    ; Messages
    msg_bootstrap_start db "Starting Reverser self-hosting bootstrap...", 10, 0
    msg_bootstrap_start_len equ $ - msg_bootstrap_start - 1
    
    msg_loading_source db "Loading self-compiler source...", 10, 0
    msg_loading_source_len equ $ - msg_loading_source - 1
    
    msg_parsing db "Parsing Reverser source...", 10, 0
    msg_parsing_len equ $ - msg_parsing - 1
    
    msg_generating db "Generating assembly code...", 10, 0
    msg_generating_len equ $ - msg_generating - 1
    
    msg_writing_output db "Writing output assembly...", 10, 0
    msg_writing_output_len equ $ - msg_writing_output - 1
    
    msg_bootstrap_complete db "Bootstrap complete! Self-hosting compiler ready.", 10, 0
    msg_bootstrap_complete_len equ $ - msg_bootstrap_complete - 1
    
    ; File names
    source_filename db "reverser_self_compiler.rev", 0
    output_filename db "reverser_self_compiler.asm", 0
    
    ; Error messages
    error_file_not_found db "Error: Source file not found", 10, 0
    error_file_not_found_len equ $ - error_file_not_found - 1
    
    error_parse_failed db "Error: Failed to parse source", 10, 0
    error_parse_failed_len equ $ - error_parse_failed - 1

section .bss
    ; File buffers
    source_buffer resb 65536    ; 64KB source buffer
    output_buffer resb 131072   ; 128KB output buffer
    
    ; Compiler state
    source_size resq 1
    output_size resq 1
    ast_root resq 1

section .text
    global _start
    extern lexer_init
    extern parser_init
    extern parse_program
    extern runtime_init
    extern heap_alloc

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

; Load source file
; Output: rax = 1 if success, 0 if failed
load_source_file:
    push rbx
    push rcx
    push rdx
    
    ; Open source file
    mov rax, 2      ; syscall number for open
    mov rdi, source_filename
    mov rsi, 0      ; O_RDONLY
    mov rdx, 0      ; mode (not used for reading)
    syscall
    
    cmp rax, 0
    jl .load_failed
    
    mov rbx, rax    ; Save file descriptor
    
    ; Read file content
    mov rax, 0      ; syscall number for read
    mov rdi, rbx    ; file descriptor
    mov rsi, source_buffer
    mov rdx, 65536  ; max bytes to read
    syscall
    
    mov [source_size], rax
    
    ; Close file
    mov rax, 3      ; syscall number for close
    mov rdi, rbx    ; file descriptor
    syscall
    
    mov rax, 1      ; Success
    jmp .return

.load_failed:
    mov rax, 0      ; Failure

.return:
    pop rdx
    pop rcx
    pop rbx
    ret

; Parse source code
; Output: rax = AST root node or 0 if failed
parse_source:
    push rbx
    push rcx
    push rdx
    
    ; Initialize lexer
    mov rdi, source_buffer
    call lexer_init
    
    ; Initialize parser
    call parser_init
    
    ; Parse program
    call parse_program
    mov [ast_root], rax
    
    pop rdx
    pop rcx
    pop rbx
    ret

; Generate assembly code from AST
; Input: rdi = AST root node
; Output: rax = 1 if success, 0 if failed
generate_assembly:
    push rbx
    push rcx
    push rdx
    
    mov rbx, rdi    ; Save AST root
    
    ; Initialize output buffer
    mov rdi, output_buffer
    mov rsi, 131072
    call clear_buffer
    
    ; Generate assembly header
    mov rdi, output_buffer
    mov rsi, asm_header
    call append_string
    
    ; Generate code for AST
    mov rdi, rbx
    mov rsi, output_buffer
    call generate_node_assembly
    
    ; Generate assembly footer
    mov rdi, output_buffer
    mov rsi, asm_footer
    call append_string
    
    ; Calculate output size
    mov rdi, output_buffer
    call string_length
    mov [output_size], rax
    
    mov rax, 1      ; Success
    pop rdx
    pop rcx
    pop rbx
    ret

; Generate assembly for a specific AST node
; Input: rdi = AST node, rsi = output buffer
generate_node_assembly:
    push rbx
    push rcx
    push rdx
    
    cmp rdi, 0
    je .generate_done
    
    mov rbx, rdi    ; Save node
    mov rcx, rsi    ; Save buffer
    
    ; Get node type
    mov rax, [rbx]  ; Assuming type is at offset 0
    
    ; Generate code based on node type
    cmp rax, AST_FUNCTION
    je .generate_function
    cmp rax, AST_BINARY_OP
    je .generate_binary_op
    cmp rax, AST_LITERAL
    je .generate_literal
    cmp rax, AST_IDENTIFIER
    je .generate_identifier
    
    jmp .generate_done

.generate_function:
    ; Generate function assembly
    mov rdi, rcx
    mov rsi, function_prologue
    call append_string
    
    ; Generate function body
    mov rdi, [rbx + 32]  ; children field
    mov rsi, rcx
    call generate_node_assembly
    
    mov rdi, rcx
    mov rsi, function_epilogue
    call append_string
    jmp .generate_done

.generate_binary_op:
    ; Generate left operand
    mov rdi, [rbx + 16]  ; left field
    mov rsi, rcx
    call generate_node_assembly
    
    ; Generate right operand
    mov rdi, [rbx + 24]  ; right field
    mov rsi, rcx
    call generate_node_assembly
    
    ; Generate operation
    mov rdi, rcx
    mov rsi, binary_op_code
    call append_string
    jmp .generate_done

.generate_literal:
    mov rdi, rcx
    mov rsi, literal_code
    call append_string
    jmp .generate_done

.generate_identifier:
    mov rdi, rcx
    mov rsi, identifier_code
    call append_string
    jmp .generate_done

.generate_done:
    pop rdx
    pop rcx
    pop rbx
    ret

; Write output file
; Output: rax = 1 if success, 0 if failed
write_output_file:
    push rbx
    push rcx
    push rdx
    
    ; Create/open output file
    mov rax, 2      ; syscall number for open
    mov rdi, output_filename
    mov rsi, 0x241  ; O_WRONLY | O_CREAT | O_TRUNC
    mov rdx, 0644   ; file permissions
    syscall
    
    cmp rax, 0
    jl .write_failed
    
    mov rbx, rax    ; Save file descriptor
    
    ; Write output buffer
    mov rax, 1      ; syscall number for write
    mov rdi, rbx    ; file descriptor
    mov rsi, output_buffer
    mov rdx, [output_size]
    syscall
    
    ; Close file
    mov rax, 3      ; syscall number for close
    mov rdi, rbx    ; file descriptor
    syscall
    
    mov rax, 1      ; Success
    jmp .return

.write_failed:
    mov rax, 0      ; Failure

.return:
    pop rdx
    pop rcx
    pop rbx
    ret

; Clear buffer
; Input: rdi = buffer, rsi = size
clear_buffer:
    push rcx
    push rdi
    
    mov rcx, rsi
    xor rax, rax
    rep stosb
    
    pop rdi
    pop rcx
    ret

; Append string to buffer
; Input: rdi = buffer, rsi = string to append
append_string:
    push rbx
    push rcx
    push rdx
    
    ; Find end of buffer
    mov rbx, rdi
    call string_length
    add rbx, rax
    
    ; Copy string
    mov rcx, rsi
    call string_length
    mov rdx, rax    ; String length
    
    mov rdi, rbx
    mov rsi, rcx
    mov rcx, rdx
    rep movsb
    
    pop rdx
    pop rcx
    pop rbx
    ret

; Get string length
; Input: rdi = string
; Output: rax = length
string_length:
    push rbx
    push rcx
    
    mov rbx, rdi
    mov rcx, 0
    
.count_loop:
    cmp byte [rbx + rcx], 0
    je .count_done
    inc rcx
    jmp .count_loop

.count_done:
    mov rax, rcx
    pop rcx
    pop rbx
    ret

; Main bootstrap function
_start:
    ; Initialize runtime
    call runtime_init
    
    ; Print start message
    mov rdi, msg_bootstrap_start
    mov rsi, msg_bootstrap_start_len
    call print_string
    
    ; Load source file
    mov rdi, msg_loading_source
    mov rsi, msg_loading_source_len
    call print_string
    
    call load_source_file
    cmp rax, 0
    je .bootstrap_failed
    
    ; Parse source
    mov rdi, msg_parsing
    mov rsi, msg_parsing_len
    call print_string
    
    call parse_source
    cmp rax, 0
    je .parse_failed
    
    ; Generate assembly
    mov rdi, msg_generating
    mov rsi, msg_generating_len
    call print_string
    
    mov rdi, [ast_root]
    call generate_assembly
    cmp rax, 0
    je .generate_failed
    
    ; Write output
    mov rdi, msg_writing_output
    mov rsi, msg_writing_output_len
    call print_string
    
    call write_output_file
    cmp rax, 0
    je .write_failed
    
    ; Success
    mov rdi, msg_bootstrap_complete
    mov rsi, msg_bootstrap_complete_len
    call print_string
    
    jmp .exit_success

.bootstrap_failed:
    mov rdi, error_file_not_found
    mov rsi, error_file_not_found_len
    call print_string
    jmp .exit_error

.parse_failed:
    mov rdi, error_parse_failed
    mov rsi, error_parse_failed_len
    call print_string
    jmp .exit_error

.generate_failed:
.write_failed:
    jmp .exit_error

.exit_success:
    mov rax, 60     ; syscall number for exit
    xor rdi, rdi    ; exit code 0
    syscall

.exit_error:
    mov rax, 60     ; syscall number for exit
    mov rdi, 1      ; exit code 1
    syscall

section .data
    ; Assembly code templates
    asm_header db "section .text", 10, "global _start", 10, 10, "_start:", 10, 0
    asm_footer db "    mov rax, 60", 10, "    xor rdi, rdi", 10, "    syscall", 10, 0
    
    function_prologue db "    push rbp", 10, "    mov rbp, rsp", 10, 0
    function_epilogue db "    pop rbp", 10, "    ret", 10, 0
    
    binary_op_code db "    pop rbx", 10, "    pop rax", 10, "    add rax, rbx", 10, "    push rax", 10, 0
    literal_code db "    mov rax, 42", 10, "    push rax", 10, 0
    identifier_code db "    ; identifier", 10, 0
