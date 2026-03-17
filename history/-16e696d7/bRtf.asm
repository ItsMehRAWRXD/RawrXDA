; Universal Compiler Runtime - Minimal Dependencies
; Provides symbols required by all compiler_from_scratch.asm files

section .data
    compiler_state dd 0
    error_count dd 0
    warning_count dd 0
    
    ; Basic string constants
    str_compiler_name db "Universal Compiler Runtime", 0
    str_compiler_version db "1.0.0", 0
    
    ; Error messages
    str_error_unknown db "Unknown error", 0
    str_error_syntax db "Syntax error", 0
    str_error_semantic db "Semantic error", 0
    str_error_codegen db "Code generation error", 0
    
section .text
    global compiler_init
    global compiler_cleanup
    global compiler_get_error_count
    global compiler_get_warning_count
    global compiler_set_state
    global compiler_get_state
    
    ; Basic I/O functions
    global runtime_print_string
    global runtime_print_char
    global runtime_print_int
    global runtime_read_string
    global runtime_read_char
    global runtime_read_int
    
    ; Memory management
    global runtime_malloc
    global runtime_free
    global runtime_realloc
    global runtime_memset
    global runtime_memcpy
    global runtime_memcmp
    
    ; String operations
    global runtime_strlen
    global runtime_strcpy
    global runtime_strcat
    global runtime_strcmp
    global runtime_strchr
    global runtime_strstr
    
    ; Math operations
    global runtime_add
    global runtime_sub
    global runtime_mul
    global runtime_div
    global runtime_mod
    global runtime_pow
    
compiler_init:
    push rbp
    mov rbp, rsp
    mov dword [compiler_state], 1
    mov dword [error_count], 0
    mov dword [warning_count], 0
    pop rbp
    ret

compiler_cleanup:
    push rbp
    mov rbp, rsp
    mov dword [compiler_state], 0
    pop rbp
    ret

compiler_get_error_count:
    mov eax, [error_count]
    ret

compiler_get_warning_count:
    mov eax, [warning_count]
    ret

compiler_set_state:
    mov [compiler_state], ecx
    ret

compiler_get_state:
    mov eax, [compiler_state]
    ret

; Basic I/O functions
runtime_print_string:
    ; rcx = string pointer
    push rbp
    mov rbp, rsp
    ; Simple implementation - just return
    pop rbp
    ret

runtime_print_char:
    ; cl = character
    push rbp
    mov rbp, rsp
    pop rbp
    ret

runtime_print_int:
    ; ecx = integer
    push rbp
    mov rbp, rsp
    pop rbp
    ret

runtime_read_string:
    ; rcx = buffer, rdx = max length
    push rbp
    mov rbp, rsp
    xor rax, rax
    pop rbp
    ret

runtime_read_char:
    push rbp
    mov rbp, rsp
    xor rax, rax
    pop rbp
    ret

runtime_read_int:
    push rbp
    mov rbp, rsp
    xor rax, rax
    pop rbp
    ret

; Memory management
runtime_malloc:
    ; rcx = size
    push rbp
    mov rbp, rsp
    xor rax, rax
    pop rbp
    ret

runtime_free:
    ; rcx = pointer
    push rbp
    mov rbp, rsp
    pop rbp
    ret

runtime_realloc:
    ; rcx = pointer, rdx = new size
    push rbp
    mov rbp, rsp
    xor rax, rax
    pop rbp
    ret

runtime_memset:
    ; rcx = dest, dl = value, r8 = count
    push rbp
    mov rbp, rsp
    mov rax, rcx
    pop rbp
    ret

runtime_memcpy:
    ; rcx = dest, rdx = src, r8 = count
    push rbp
    mov rbp, rsp
    mov rax, rcx
    pop rbp
    ret

runtime_memcmp:
    ; rcx = ptr1, rdx = ptr2, r8 = count
    push rbp
    mov rbp, rsp
    xor rax, rax
    pop rbp
    ret

; String operations
runtime_strlen:
    ; rcx = string
    push rbp
    mov rbp, rsp
    xor rax, rax
    test rcx, rcx
    jz .done
.loop:
    cmp byte [rcx + rax], 0
    je .done
    inc rax
    jmp .loop
.done:
    pop rbp
    ret

runtime_strcpy:
    ; rcx = dest, rdx = src
    push rbp
    mov rbp, rsp
    mov rax, rcx
    test rdx, rdx
    jz .done
.loop:
    mov al, [rdx]
    mov [rcx], al
    test al, al
    jz .done
    inc rcx
    inc rdx
    jmp .loop
.done:
    pop rbp
    ret

runtime_strcat:
    ; rcx = dest, rdx = src
    push rbp
    mov rbp, rsp
    mov rax, rcx
    test rcx, rcx
    jz .done
.find_end:
    cmp byte [rcx], 0
    je .copy
    inc rcx
    jmp .find_end
.copy:
    test rdx, rdx
    jz .done
.loop:
    mov al, [rdx]
    mov [rcx], al
    test al, al
    jz .done
    inc rcx
    inc rdx
    jmp .loop
.done:
    pop rbp
    ret

runtime_strcmp:
    ; rcx = str1, rdx = str2
    push rbp
    mov rbp, rsp
.loop:
    mov al, [rcx]
    mov bl, [rdx]
    cmp al, bl
    jne .diff
    test al, al
    jz .equal
    inc rcx
    inc rdx
    jmp .loop
.diff:
    sub eax, ebx
    jmp .done
.equal:
    xor eax, eax
.done:
    pop rbp
    ret

runtime_strchr:
    ; rcx = string, dl = char
    push rbp
    mov rbp, rsp
    mov rax, rcx
.loop:
    mov cl, [rax]
    test cl, cl
    jz .not_found
    cmp cl, dl
    je .found
    inc rax
    jmp .loop
.not_found:
    xor rax, rax
.found:
    pop rbp
    ret

runtime_strstr:
    ; rcx = haystack, rdx = needle
    push rbp
    mov rbp, rsp
    xor rax, rax
    pop rbp
    ret

; Math operations
runtime_add:
    ; ecx = a, edx = b
    mov eax, ecx
    add eax, edx
    ret

runtime_sub:
    ; ecx = a, edx = b
    mov eax, ecx
    sub eax, edx
    ret

runtime_mul:
    ; ecx = a, edx = b
    mov eax, ecx
    imul eax, edx
    ret

runtime_div:
    ; ecx = dividend, edx = divisor
    push rdx
    xor edx, edx
    mov eax, ecx
    pop rcx
    div ecx
    ret

runtime_mod:
    ; ecx = dividend, edx = divisor
    push rdx
    xor edx, edx
    mov eax, ecx
    pop rcx
    div ecx
    mov eax, edx
    ret

runtime_pow:
    ; ecx = base, edx = exponent
    push rbp
    mov rbp, rsp
    mov eax, 1
    test edx, edx
    jz .done
.loop:
    imul eax, ecx
    dec edx
    jnz .loop
.done:
    pop rbp
    ret