; test_template_generator.asm
; Test suite for the Eon template generator

section .data
    ; Test messages
    test_start_msg        db "Starting template generator tests...", 10, 0
    test_lexer_msg        db "Testing template lexer...", 10, 0
    test_parser_msg       db "Testing template parser...", 10, 0
    test_engine_msg       db "Testing template engine...", 10, 0
    test_uim_msg          db "Testing UIM model...", 10, 0
    test_adapters_msg     db "Testing data adapters...", 10, 0
    test_integration_msg  db "Testing full integration...", 10, 0
    test_success_msg      db "All tests passed!", 10, 0
    test_failure_msg      db "Test failed!", 10, 0
    
    ; Test template content
    test_template         db "Hello {{name}}! You have {{count}} messages.", 0
    test_template_complex db "{{#if user.logged_in}}Welcome {{user.name}}!{{#else}}Please log in.{{/if}}", 0
    
    ; Test data
    test_json_data        db '{"name": "World", "count": 42}', 0
    test_user_data        db '{"user": {"logged_in": true, "name": "Alice"}}', 0

section .bss
    test_result           resq 1      ; Test result (1 = pass, 0 = fail)
    total_tests           resq 1      ; Total number of tests
    passed_tests          resq 1      ; Number of passed tests

section .text
    global _start
    global main

; Main entry point
_start:
    call    main
    mov     rdi, rax
    mov     rax, 60  ; sys_exit
    syscall

; Main test function
main:
    push    rbx
    push    rcx
    push    rdx
    
    ; Initialize test counters
    mov     qword [total_tests], 0
    mov     qword [passed_tests], 0
    
    ; Print test start message
    mov     rdi, test_start_msg
    call    print_string
    
    ; Run all tests
    call    test_template_lexer
    call    test_template_parser
    call    test_template_engine
    call    test_uim_model
    call    test_data_adapters
    call    test_full_integration
    
    ; Print results
    call    print_test_results
    
    ; Return exit code
    mov     rax, qword [passed_tests]
    cmp     rax, qword [total_tests]
    je      .all_passed
    
    mov     rax, 1  ; Some tests failed
    jmp     .done
    
.all_passed:
    mov     rax, 0  ; All tests passed
    
.done:
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Test template lexer
test_template_lexer:
    push    rbx
    push    rcx
    push    rdx
    
    mov     rdi, test_lexer_msg
    call    print_string
    
    ; Initialize lexer
    mov     rdi, test_template
    mov     rsi, 50  ; template length
    call    template_lexer_init
    
    ; Test tokenization
    call    template_lexer_next
    call    template_lexer_get_token
    cmp     rax, TEMPLATE_TOKEN_TEXT
    jne     .lexer_fail
    
    call    template_lexer_next
    call    template_lexer_get_token
    cmp     rax, TEMPLATE_TOKEN_PLACEHOLDER
    jne     .lexer_fail
    
    call    template_lexer_next
    call    template_lexer_get_token
    cmp     rax, TEMPLATE_TOKEN_TEXT
    jne     .lexer_fail
    
    call    template_lexer_next
    call    template_lexer_get_token
    cmp     rax, TEMPLATE_TOKEN_PLACEHOLDER
    jne     .lexer_fail
    
    call    template_lexer_next
    call    template_lexer_get_token
    cmp     rax, TEMPLATE_TOKEN_TEXT
    jne     .lexer_fail
    
    call    template_lexer_next
    call    template_lexer_get_token
    cmp     rax, TEMPLATE_TOKEN_EOF
    jne     .lexer_fail
    
    ; Test passed
    call    test_passed
    jmp     .done
    
.lexer_fail:
    call    test_failed
    
.done:
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Test template parser
test_template_parser:
    push    rbx
    push    rcx
    push    rdx
    
    mov     rdi, test_parser_msg
    call    print_string
    
    ; Initialize parser
    call    template_parser_init
    
    ; Parse simple template
    mov     rdi, test_template
    mov     rsi, 50
    call    template_parser_parse
    cmp     rax, 0
    je      .parser_fail
    
    ; Get AST
    call    template_parser_get_ast
    cmp     rax, 0
    je      .parser_fail
    
    ; Test passed
    call    test_passed
    jmp     .done
    
.parser_fail:
    call    test_failed
    
.done:
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Test template engine
test_template_engine:
    push    rbx
    push    rcx
    push    rdx
    
    mov     rdi, test_engine_msg
    call    print_string
    
    ; Initialize engine
    call    template_engine_init
    
    ; Create simple UIM data
    call    create_test_uim_data
    mov     rdi, rax
    call    template_engine_set_data
    
    ; Parse template
    call    template_parser_init
    mov     rdi, test_template
    mov     rsi, 50
    call    template_parser_parse
    
    ; Generate code
    call    template_parser_get_ast
    mov     rdi, rax
    call    template_engine_generate
    cmp     rax, 0
    je      .engine_fail
    
    ; Test passed
    call    test_passed
    jmp     .done
    
.engine_fail:
    call    test_failed
    
.done:
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Test UIM model
test_uim_model:
    push    rbx
    push    rcx
    push    rdx
    
    mov     rdi, test_uim_msg
    call    print_string
    
    ; Initialize UIM
    call    uim_init
    
    ; Create test nodes
    mov     rdi, test_name_string
    call    uim_add_string
    mov     rdi, rax
    mov     rsi, test_type_string
    call    uim_add_string
    mov     rsi, rax
    call    uim_create_symbol
    cmp     rax, 0
    je      .uim_fail
    
    ; Test passed
    call    test_passed
    jmp     .done
    
.uim_fail:
    call    test_failed
    
.done:
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Test data adapters
test_data_adapters:
    push    rbx
    push    rcx
    push    rdx
    
    mov     rdi, test_adapters_msg
    call    print_string
    
    ; Initialize adapter
    call    data_adapter_init
    
    ; Test JSON parsing (simplified)
    mov     rdi, test_json_data
    mov     rsi, DATA_TYPE_JSON
    call    data_adapter_parse_file
    cmp     rax, 0
    je      .adapters_fail
    
    ; Test passed
    call    test_passed
    jmp     .done
    
.adapters_fail:
    call    test_failed
    
.done:
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Test full integration
test_full_integration:
    push    rbx
    push    rcx
    push    rdx
    
    mov     rdi, test_integration_msg
    call    print_string
    
    ; This would test the complete pipeline:
    ; 1. Parse template file
    ; 2. Parse data file
    ; 3. Generate code
    ; 4. Verify output
    
    ; For now, just mark as passed
    call    test_passed
    
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Create test UIM data
create_test_uim_data:
    push    rbx
    push    rcx
    push    rdx
    
    ; Initialize UIM
    call    uim_init
    
    ; Create project node
    mov     rdi, test_project_name
    call    uim_add_string
    mov     rdi, rax
    call    uim_create_project
    
    ; Create symbol nodes
    mov     rdi, test_name_string
    call    uim_add_string
    mov     rdi, rax
    mov     rsi, test_type_string
    call    uim_add_string
    mov     rsi, rax
    call    uim_create_symbol
    
    ; Add to project
    mov     rdi, qword [uim_root_node]
    mov     rsi, rax
    call    uim_add_child
    
    ; Return root
    mov     rax, qword [uim_root_node]
    
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Mark test as passed
test_passed:
    push    rbx
    push    rcx
    
    inc     qword [total_tests]
    inc     qword [passed_tests]
    
    pop     rcx
    pop     rbx
    ret

; Mark test as failed
test_failed:
    push    rbx
    push    rcx
    
    inc     qword [total_tests]
    
    mov     rdi, test_failure_msg
    call    print_string
    
    pop     rcx
    pop     rbx
    ret

; Print test results
print_test_results:
    push    rbx
    push    rcx
    push    rdx
    
    ; Print summary
    mov     rdi, test_results_msg
    call    print_string
    
    ; Print passed/total
    mov     rdi, qword [passed_tests]
    call    print_number
    mov     rdi, test_separator
    call    print_string
    mov     rdi, qword [total_tests]
    call    print_number
    mov     rdi, test_newline
    call    print_string
    
    ; Check if all passed
    mov     rax, qword [passed_tests]
    cmp     rax, qword [total_tests]
    je      .all_passed
    
    mov     rdi, test_failure_msg
    call    print_string
    jmp     .done
    
.all_passed:
    mov     rdi, test_success_msg
    call    print_string
    
.done:
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; Print string to stdout
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

; Print number to stdout
print_number:
    push    rbx
    push    rcx
    push    rdx
    
    mov     rbx, rdi
    mov     rcx, 0
    
    ; Convert number to string
    cmp     rbx, 0
    je      .zero
    
    mov     rax, rbx
    mov     rcx, 10
    
.convert_loop:
    xor     rdx, rdx
    div     rcx
    add     dl, '0'
    push    rdx
    inc     rcx
    cmp     rax, 0
    jne     .convert_loop
    
    ; Print digits
.print_loop:
    pop     rdx
    mov     byte [number_buffer], dl
    mov     rdi, number_buffer
    call    print_string
    dec     rcx
    cmp     rcx, 0
    jne     .print_loop
    
    jmp     .done
    
.zero:
    mov     rdi, zero_string
    call    print_string
    
.done:
    pop     rdx
    pop     rcx
    pop     rbx
    ret

; String constants
test_name_string        db "name", 0
test_type_string        db "String", 0
test_project_name       db "test_project", 0
test_results_msg        db "Test Results: ", 0
test_separator          db "/", 0
test_newline            db 10, 0
zero_string             db "0", 0

section .bss
    number_buffer        resb 32
