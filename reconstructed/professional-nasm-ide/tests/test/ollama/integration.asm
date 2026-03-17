; ==============================================================================
; Ollama Integration Test Harness
; Tests end-to-end: HTTP building, body finding, JSON parsing
; ==============================================================================

BITS 64
DEFAULT REL

%ifdef WINDOWS
    %define PLATFORM_WIN 1
%else
    %define PLATFORM_LINUX 1
%endif

; External functions from ollama_native.asm
extern ollama_init
extern ollama_connect
extern ollama_close
extern append_string
extern append_cached_headers
extern build_http_post
extern find_http_body
extern parse_http_response

; External from json_parser.asm
extern json_parse_response

; ==============================================================================
; Test Data
; ==============================================================================
section .data
    test_host:      db "localhost", 0
    test_port:      dw 11434
    
    ; Sample HTTP response with JSON body
    test_response:  db "HTTP/1.1 200 OK", 0x0D, 0x0A
                    db "Content-Type: application/json", 0x0D, 0x0A
                    db "Content-Length: 89", 0x0D, 0x0A
                    db 0x0D, 0x0A
                    db '{"response":"Hello world","done":true,"context":[1,2,3]}', 0
    test_response_len: equ $ - test_response
    
    test_method:    db "/api/generate", 0
    test_method_len: equ $ - test_method - 1
    
    test_body:      db '{"model":"llama2","prompt":"test"}', 0
    test_body_len:  equ $ - test_body - 1
    
    msg_test_start:     db "=== Ollama Integration Test Harness ===", 0x0A, 0
    msg_test_1:         db "[1] Testing append_string...", 0x0A, 0
    msg_test_2:         db "[2] Testing find_http_body...", 0x0A, 0
    msg_test_3:         db "[3] Testing parse_http_response...", 0x0A, 0
    msg_test_4:         db "[4] Testing append_cached_headers...", 0x0A, 0
    msg_pass:           db "    PASS", 0x0A, 0
    msg_fail:           db "    FAIL", 0x0A, 0
    msg_test_complete:  db "=== All Tests Complete ===", 0x0A, 0

section .bss
    test_buffer:    resb 4096
    test_config:    resb 64
    test_ollama_response: resb 128

section .text
    global _start
    global test_main

; ==============================================================================
; Main test entry point
; ==============================================================================
_start:
test_main:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Print test start message
    lea rdi, [rel msg_test_start]
    call print_string
    
    ; Test 1: append_string
    lea rdi, [rel msg_test_1]
    call print_string
    call test_append_string
    test rax, rax
    jnz .test1_fail
    lea rdi, [rel msg_pass]
    call print_string
    jmp .test2
.test1_fail:
    lea rdi, [rel msg_fail]
    call print_string
    
.test2:
    ; Test 2: find_http_body
    lea rdi, [rel msg_test_2]
    call print_string
    call test_find_body
    test rax, rax
    jnz .test2_fail
    lea rdi, [rel msg_pass]
    call print_string
    jmp .test3
.test2_fail:
    lea rdi, [rel msg_fail]
    call print_string
    
.test3:
    ; Test 3: parse_http_response
    lea rdi, [rel msg_test_3]
    call print_string
    call test_parse_response
    test rax, rax
    jnz .test3_fail
    lea rdi, [rel msg_pass]
    call print_string
    jmp .test4
.test3_fail:
    lea rdi, [rel msg_fail]
    call print_string
    
.test4:
    ; Test 4: append_cached_headers
    lea rdi, [rel msg_test_4]
    call print_string
    call test_cached_headers
    test rax, rax
    jnz .test4_fail
    lea rdi, [rel msg_pass]
    call print_string
    jmp .complete
.test4_fail:
    lea rdi, [rel msg_fail]
    call print_string
    
.complete:
    lea rdi, [rel msg_test_complete]
    call print_string
    
    ; Exit
    xor eax, eax
    add rsp, 32
    pop rbp
    ret

; ==============================================================================
; Test Functions
; ==============================================================================

test_append_string:
    push rbp
    mov rbp, rsp
    
    ; Test: append "Hello" to empty buffer
    lea rdi, [rel test_buffer]
    xor esi, esi        ; position = 0
    mov edx, 4096       ; capacity
    lea rcx, [rel test_host]
    mov r8, 9           ; strlen("localhost")
    call append_string
    
    ; Check result
    cmp rax, 9
    jne .fail
    
    xor eax, eax        ; success
    pop rbp
    ret
.fail:
    mov rax, -1
    pop rbp
    ret

test_find_body:
    push rbp
    mov rbp, rsp
    
    ; Test: find body in test_response
    lea rdi, [rel test_response]
    mov esi, test_response_len
    call find_http_body
    
    ; Check that body was found
    test rax, rax
    jz .fail
    
    ; Verify it points to JSON
    test rax, rax
    jz .fail
    cmp byte [rax], '{'
    jne .fail
    cmp byte [rax+1], '"'
    jne .fail
    cmp byte [rax+2], 'r'
    jne .fail
    cmp byte [rax+3], 'e'
    jne .fail
    cmp byte [rax+4], 's'
    jne .fail
    cmp byte [rax+5], 'p'
    jne .fail
    cmp byte [rax+6], 'o'
    jne .fail
    cmp byte [rax+7], 'n'
    jne .fail
    cmp byte [rax+8], 's'
    jne .fail
    cmp byte [rax+9], 'e'
    jne .fail
    
    xor eax, eax
    pop rbp
    ret
.fail:
    mov rax, -1
    pop rbp
    ret

test_parse_response:
    push rbp
    mov rbp, rsp
    
    ; Test: parse test_response
    lea rdi, [rel test_response]
    mov esi, test_response_len
    lea rdx, [rel test_ollama_response]
    call parse_http_response
    
    ; Check result
    test rax, rax
    jnz .fail
    
    ; Verify response_len is non-zero
    lea rdi, [rel test_ollama_response]
    mov rax, [rdi + 8]  ; OllamaResponse.response_len offset
    test rax, rax
    jz .fail
    
    xor eax, eax
    pop rbp
    ret
.fail:
    mov rax, -1
    pop rbp
    ret

test_cached_headers:
    push rbp
    mov rbp, rsp
    
    ; Test: append cached headers
    lea rdi, [rel test_buffer]
    xor esi, esi        ; position = 0
    mov edx, 4096       ; capacity
    call append_cached_headers
    
    ; Check result
    test rax, rax
    js .fail
    cmp rax, 0
    je .fail            ; Should have added some bytes
    
    xor eax, eax
    pop rbp
    ret
.fail:
    mov rax, -1
    pop rbp
    ret

; ==============================================================================
; Helper: print_string (simplified)
; ==============================================================================
print_string:
    push rbp
    mov rbp, rsp
    push rbx
    
    mov rbx, rdi
    
    ; Calculate length
    xor rcx, rcx
.strlen_loop:
    cmp byte [rbx + rcx], 0
    je .strlen_done
    inc rcx
    jmp .strlen_loop
.strlen_done:
    
    test rcx, rcx
    jz .done
    
%ifdef PLATFORM_WIN
    ; Windows: WriteFile to stdout
    push rcx
    sub rsp, 32
    
    mov rcx, -11        ; STD_OUTPUT_HANDLE
    ; Call GetStdHandle (stub - would need proper import)
    ; For now, just skip actual printing
    
    add rsp, 32
    pop rcx
%else
    ; Linux: sys_write
    mov rax, 1          ; sys_write
    mov rdi, 1          ; stdout
    mov rsi, rbx        ; buffer
    mov rdx, rcx        ; length
    syscall
%endif
    
.done:
    pop rbx
    pop rbp
    ret
