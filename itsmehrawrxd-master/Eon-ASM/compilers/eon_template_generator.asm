; eon_template_generator.asm
; Main template generator that orchestrates the entire process
; Integrates lexer, parser, engine, and data adapters

section .data
    ; Command line argument indices
    %define ARG_TEMPLATE_FILE    1
    %define ARG_DATA_FILE        2
    %define ARG_OUTPUT_FILE      3
    %define ARG_DATA_TYPE        4

    ; File type constants
    %define DATA_TYPE_JSON       1
    %define DATA_TYPE_YAML       2
    %define DATA_TYPE_SQL        3
    %define DATA_TYPE_C_HEADER   4

    ; Error messages
    error_usage           db "Usage: eon_template_generator <template.eont> <data_file> <output.eon> <data_type>", 10, 0
    error_template_file   db "Error: Cannot open template file", 10, 0
    error_data_file       db "Error: Cannot open data file", 10, 0
    error_output_file     db "Error: Cannot create output file", 10, 0
    error_parse_template  db "Error: Failed to parse template", 10, 0
    error_parse_data      db "Error: Failed to parse data file", 10, 0
    error_generate        db "Error: Failed to generate code", 10, 0
    success_message       db "Template generation successful", 10, 0

section .bss
    template_file_path    resq 1      ; Path to template file
    data_file_path        resq 1      ; Path to data file
    output_file_path      resq 1      ; Path to output file
    data_type             resq 1      ; Type of data file
    template_ast          resq 1      ; Template AST
    uim_root              resq 1      ; UIM root node
    generated_code        resq 1      ; Generated code buffer

section .text
    global _start
    global main

; Main entry point
_start:
    call    main
    mov     rdi, rax
    mov     rax, 60  ; sys_exit
    syscall

; Main function
; Args: argc, argv
main:
    push    rbx
    push    rcx
    push    rdx
    push    rsi
    push    rdi
    
    ; Check argument count
    cmp     rdi, 5
    jl      .usage_error
    
    ; Get command line arguments
    mov     rbx, rsi  ; argv
    
    ; Get template file path
    mov     rcx, qword [rbx + ARG_TEMPLATE_FILE * 8]
    mov     qword [template_file_path], rcx
    
    ; Get data file path
    mov     rcx, qword [rbx + ARG_DATA_FILE * 8]
    mov     qword [data_file_path], rcx
    
    ; Get output file path
    mov     rcx, qword [rbx + ARG_OUTPUT_FILE * 8]
    mov     qword [output_file_path], rcx
    
    ; Get data type
    mov     rcx, qword [rbx + ARG_DATA_TYPE * 8]
    call    parse_data_type
    mov     qword [data_type], rax
    
    ; Initialize systems
    call    initialize_systems
    
    ; Parse template file
    call    parse_template_file
    cmp     rax, 0
    je      .parse_template_error
    
    ; Parse data file
    call    parse_data_file
    cmp     rax, 0
    je      .parse_data_error
    
    ; Generate code
    call    generate_code
    cmp     rax, 0
    je      .generate_error
    
    ; Write output file
    call    write_output_file
    cmp     rax, 0
    je      .output_error
    
    ; Success
    mov     rdi, success_message
    call    print_string
    
    ; Cleanup
    call    cleanup_systems
    
    mov     rax, 0  ; Success
    jmp     .done
    
.usage_error:
    mov     rdi, error_usage
    call    print_string
    mov     rax, 1
    jmp     .done
    
.parse_template_error:
    mov     rdi, error_parse_template
    call    print_string
    mov     rax, 1
    jmp     .done
    
.parse_data_error:
    mov     rdi, error_parse_data
    call    print_string
    mov     rax, 1
    jmp     .done
    
.generate_error:
    mov     rdi, error_generate
    call    print_string
    mov     rax, 1
    jmp     .done
    
.output_error:
    mov     rdi, error_output_file
    call    print_string
    mov     rax, 1
    jmp     .done
    
.done:
    pop     rdi
    pop     rsi
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Parse data type string
; Args: rcx = data type string
; Returns: rax = data type constant
parse_data_type:
    push    rbx
    push    rcx
    push    rdx
    
    mov     rbx, rcx
    
    ; Check for "json"
    mov     rdi, rbx
    mov     rsi, json_type_string
    call    compare_strings
    cmp     rax, 1
    je      .json_type
    
    ; Check for "yaml"
    mov     rdi, rbx
    mov     rsi, yaml_type_string
    call    compare_strings
    cmp     rax, 1
    je      .yaml_type
    
    ; Check for "sql"
    mov     rdi, rbx
    mov     rsi, sql_type_string
    call    compare_strings
    cmp     rax, 1
    je      .sql_type
    
    ; Check for "c_header"
    mov     rdi, rbx
    mov     rsi, c_header_type_string
    call    compare_strings
    cmp     rax, 1
    je      .c_header_type
    
    ; Default to JSON
    mov     rax, DATA_TYPE_JSON
    jmp     .done
    
.json_type:
    mov     rax, DATA_TYPE_JSON
    jmp     .done
    
.yaml_type:
    mov     rax, DATA_TYPE_YAML
    jmp     .done
    
.sql_type:
    mov     rax, DATA_TYPE_SQL
    jmp     .done
    
.c_header_type:
    mov     rax, DATA_TYPE_C_HEADER
    jmp     .done
    
.done:
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Initialize all systems
initialize_systems:
    push    rbx
    push    rcx
    
    ; Initialize template lexer
    call    template_lexer_init
    
    ; Initialize template parser
    call    template_parser_init
    
    ; Initialize template engine
    call    template_engine_init
    
    ; Initialize data adapter
    call    data_adapter_init
    
    ; Initialize UIM
    call    uim_init
    
    pop     rcx
    pop     rbx
    ret

; Parse template file
parse_template_file:
    push    rbx
    push    rcx
    push    rdx
    
    ; Read template file
    mov     rdi, qword [template_file_path]
    call    read_file_to_memory
    cmp     rax, 0
    je      .parse_error
    
    ; Parse template
    mov     rdi, qword [source_file_data]
    mov     rsi, qword [source_file_size]
    call    template_parser_parse
    cmp     rax, 0
    je      .parse_error
    
    ; Get AST
    call    template_parser_get_ast
    mov     qword [template_ast], rax
    
    mov     rax, 1  ; Success
    jmp     .done
    
.parse_error:
    mov     rax, 0  ; Error
    
.done:
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Parse data file
parse_data_file:
    push    rbx
    push    rcx
    push    rdx
    
    ; Parse data file using adapter
    mov     rdi, qword [data_file_path]
    mov     rsi, qword [data_type]
    call    data_adapter_parse_file
    cmp     rax, 0
    je      .parse_error
    
    ; Get UIM root
    call    data_adapter_get_uim_root
    mov     qword [uim_root], rax
    
    mov     rax, 1  ; Success
    jmp     .done
    
.parse_error:
    mov     rax, 0  ; Error
    
.done:
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Generate code from template and data
generate_code:
    push    rbx
    push    rcx
    push    rdx
    
    ; Set data in template engine
    mov     rdi, qword [uim_root]
    call    template_engine_set_data
    
    ; Generate code
    mov     rdi, qword [template_ast]
    call    template_engine_generate
    cmp     rax, 0
    je      .generate_error
    
    ; Get generated code
    call    template_engine_get_output
    mov     qword [generated_code], rax
    
    mov     rax, 1  ; Success
    jmp     .done
    
.generate_error:
    mov     rax, 0  ; Error
    
.done:
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Write output file
write_output_file:
    push    rbx
    push    rcx
    push    rdx
    
    ; Open output file
    mov     rax, 2  ; sys_open
    mov     rdi, qword [output_file_path]
    mov     rsi, 0x241  ; O_CREAT | O_WRONLY | O_TRUNC
    mov     rdx, 0644  ; permissions
    syscall
    
    cmp     rax, 0
    jl      .write_error
    
    mov     rbx, rax  ; file descriptor
    
    ; Write generated code
    mov     rax, 1  ; sys_write
    mov     rdi, rbx
    mov     rsi, qword [generated_code]
    mov     rdx, 65536  ; max size
    syscall
    
    ; Close file
    mov     rax, 3  ; sys_close
    mov     rdi, rbx
    syscall
    
    mov     rax, 1  ; Success
    jmp     .done
    
.write_error:
    mov     rax, 0  ; Error
    
.done:
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Cleanup all systems
cleanup_systems:
    push    rbx
    push    rcx
    
    ; Cleanup template parser
    call    template_parser_cleanup
    
    ; Cleanup template engine
    call    template_engine_cleanup
    
    ; Cleanup data adapter
    call    data_adapter_cleanup
    
    ; Cleanup UIM
    call    uim_cleanup
    
    pop     rcx
    pop     rbx
    ret

; Print string to stdout
; Args: rdi = string pointer
print_string:
    push    rbx
    push    rcx
    push    rdx
    
    mov     rbx, rdi
    mov     rcx, 0
    
    ; Calculate string length
.length_loop:
    cmp     byte [rbx + rcx], 0
    je      .length_done
    inc     rcx
    jmp     .length_loop
    
.length_done:
    ; Write string
    mov     rax, 1  ; sys_write
    mov     rdi, 1  ; stdout
    mov     rsi, rbx
    mov     rdx, rcx
    syscall
    
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Read file to memory (from data_source_adapters.asm)
read_file_to_memory:
    ; This function is already implemented in data_source_adapters.asm
    ; We'll call it from there
    jmp     read_file_to_memory

; Compare strings (from uim_model.asm)
compare_strings:
    ; This function is already implemented in uim_model.asm
    ; We'll call it from there
    jmp     compare_strings

; String constants
json_type_string       db "json", 0
yaml_type_string       db "yaml", 0
sql_type_string        db "sql", 0
c_header_type_string   db "c_header", 0
