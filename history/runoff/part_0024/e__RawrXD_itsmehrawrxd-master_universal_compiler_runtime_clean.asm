; Universal Compiler Runtime - NASM x64 Library
; Minimal core symbols needed by all compiler_from_scratch.asm implementations
; Windows x64 calling convention (RCX, RDX, R8, R9 for params)

default rel

section .data
    compiler_state: dd 0
    error_count: dd 0
    warning_count: dd 0
    heap_ptr: dq 0
    heap_base: dq 0
    heap_size: dq 1048576
    
    str_compiler_version db "1.0.0", 0

section .bss
    heap: resb 1048576

section .text

; ============ COMPILER STATE MANAGEMENT ============
global compiler_init
global compiler_cleanup
global compiler_get_error_count
global compiler_get_warning_count
global compiler_set_state
global compiler_get_state

compiler_init:
    mov dword [rel compiler_state], 1
    mov dword [rel error_count], 0
    mov dword [rel warning_count], 0
    lea rax, [rel heap]
    mov qword [rel heap_ptr], rax
    mov qword [rel heap_base], rax
    ret

compiler_cleanup:
    mov dword [rel compiler_state], 0
    ret

compiler_get_error_count:
    mov eax, [rel error_count]
    ret

compiler_get_warning_count:
    mov eax, [rel warning_count]
    ret

compiler_set_state:
    mov [rel compiler_state], ecx
    ret

compiler_get_state:
    mov eax, [rel compiler_state]
    ret

; ============ BASIC I/O (stubs) ============
global runtime_print_string
global runtime_print_char
global runtime_print_int
global runtime_read_string
global runtime_read_char
global runtime_read_int

runtime_print_string:
    xor eax, eax
    ret

runtime_print_char:
    xor eax, eax
    ret

runtime_print_int:
    xor eax, eax
    ret

runtime_read_string:
    xor eax, eax
    ret

runtime_read_char:
    xor eax, eax
    ret

runtime_read_int:
    xor eax, eax
    ret

; ============ MEMORY MANAGEMENT ============
global runtime_malloc
global runtime_free
global runtime_realloc
global runtime_memset
global runtime_memcpy
global runtime_memcmp

runtime_malloc:
    push rbp
    mov rbp, rsp
    mov rax, [rel heap_ptr]
    add qword [rel heap_ptr], rcx
    mov rdx, [rel heap_base]
    add rdx, [rel heap_size]
    cmp [rel heap_ptr], rdx
    jle .malloc_ok
    xor eax, eax
.malloc_ok:
    pop rbp
    ret

runtime_free:
    ret

runtime_realloc:
    mov rcx, rdx
    call runtime_malloc
    ret

runtime_memset:
    push rbp
    mov rbp, rsp
    test r8, r8
    jz .done
    mov rax, rcx
.loop:
    mov byte [rax], dl
    inc rax
    dec r8
    jnz .loop
.done:
    pop rbp
    ret

runtime_memcpy:
    push rbp
    mov rbp, rsp
    test r8, r8
    jz .done
    mov rax, rcx
    mov rbx, rdx
.loop:
    mov r9b, byte [rbx]
    mov byte [rax], r9b
    inc rax
    inc rbx
    dec r8
    jnz .loop
.done:
    pop rbp
    ret

runtime_memcmp:
    push rbp
    mov rbp, rsp
    test r8, r8
    jz .equal
    mov rax, rcx
    mov rbx, rdx
.loop:
    movzx r9d, byte [rax]
    movzx r10d, byte [rbx]
    cmp r9d, r10d
    jne .ne
    inc rax
    inc rbx
    dec r8
    jnz .loop
.equal:
    xor eax, eax
    jmp .done
.ne:
    sub r9d, r10d
    mov eax, r9d
.done:
    pop rbp
    ret

; ============ STRING OPERATIONS ============
global runtime_strlen
global runtime_strcpy
global runtime_strcat
global runtime_strcmp
global runtime_strchr
global runtime_strstr

runtime_strlen:
    push rbp
    mov rbp, rsp
    mov rax, rcx
    xor r8, r8
.loop:
    cmp byte [rax], 0
    je .end
    inc rax
    inc r8
    jmp .loop
.end:
    mov rax, r8
    pop rbp
    ret

runtime_strcpy:
    push rbp
    mov rbp, rsp
    mov rax, rcx
    mov rbx, rdx
.loop:
    mov r8b, byte [rbx]
    mov byte [rax], r8b
    test r8b, r8b
    je .end
    inc rax
    inc rbx
    jmp .loop
.end:
    mov rax, rcx
    pop rbp
    ret

runtime_strcat:
    push rbp
    mov rbp, rsp
    mov rax, rcx
.find_end:
    cmp byte [rax], 0
    je .copy
    inc rax
    jmp .find_end
.copy:
    mov rbx, rdx
.loop:
    mov r8b, byte [rbx]
    mov byte [rax], r8b
    test r8b, r8b
    je .end
    inc rax
    inc rbx
    jmp .loop
.end:
    mov rax, rcx
    pop rbp
    ret

runtime_strcmp:
    push rbp
    mov rbp, rsp
    mov rax, rcx
    mov rbx, rdx
.loop:
    movzx r8d, byte [rax]
    movzx r9d, byte [rbx]
    cmp r8d, r9d
    jne .ne
    test r8b, r8b
    je .eq
    inc rax
    inc rbx
    jmp .loop
.eq:
    xor eax, eax
    jmp .end
.ne:
    sub r8d, r9d
    mov eax, r8d
.end:
    pop rbp
    ret

runtime_strchr:
    push rbp
    mov rbp, rsp
    mov rax, rcx
    mov r8b, dl
.loop:
    mov r9b, byte [rax]
    cmp r9b, r8b
    je .found
    test r9b, r9b
    je .notfound
    inc rax
    jmp .loop
.found:
    jmp .end
.notfound:
    xor eax, eax
.end:
    pop rbp
    ret

runtime_strstr:
    xor eax, eax
    ret

; ============ MATH OPERATIONS ============
global runtime_add
global runtime_sub
global runtime_mul
global runtime_div
global runtime_mod
global runtime_pow

runtime_add:
    add rcx, rdx
    mov rax, rcx
    ret

runtime_sub:
    sub rcx, rdx
    mov rax, rcx
    ret

runtime_mul:
    mov rax, rcx
    imul rax, rdx
    ret

runtime_div:
    mov rax, rcx
    cdq
    idiv rdx
    ret

runtime_mod:
    mov rax, rcx
    cdq
    idiv rdx
    mov rax, rdx
    ret

runtime_pow:
    mov rax, rcx
    mov r8, rdx
    mov r9, 1
.loop:
    test r8, r8
    jz .end
    imul r9, rax
    dec r8
    jmp .loop
.end:
    mov rax, r9
    ret

; ============ COMPILER-SPECIFIC STUBS ============
global compiler_error
global compiler_warning
global compiler_parse_token
global compiler_emit_code

compiler_error:
    inc dword [rel error_count]
    ret

compiler_warning:
    inc dword [rel warning_count]
    ret

compiler_parse_token:
    xor eax, eax
    ret

compiler_emit_code:
    xor eax, eax
    ret
