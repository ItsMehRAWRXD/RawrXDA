; RawrXD-Shell - Byte-Level Pattern Search (AVX-2/512)
.code

; extern "C" const void* find_pattern_asm(const void* haystack, size_t haystack_len, const void* needle, size_t needle_len)
; RCX = haystack, RDX = haystack_len, R8 = needle, R9 = needle_len
find_pattern_asm proc
    push rdi
    push rsi
    
    mov rdi, rcx ; haystack
    mov rsi, r8  ; needle
    
    cmp r9, 0
    je found_start
    cmp rdx, r9
    jl not_found

    ; Simple scan for now, SIMD version would use PCMPEQB
loop_start:
    mov al, [rsi] ; first byte of needle
    mov rcx, rdx
    repne scasb
    jne not_found
    
    ; Found first byte at [rdi-1]
    mov r10, rdi
    dec r10
    mov r11, rcx ; remaining haystack len
    
    ; Check rest of needle
    push rdi
    push rsi
    push rcx
    
    mov rdi, r10
    mov rsi, r8
    mov rcx, r9
    repe cmpsb
    je match
    
    pop rcx
    pop rsi
    pop rdi
    mov rdx, rcx
    jmp loop_start

match:
    pop rcx
    pop rsi
    pop rdi
    mov rax, r10
    jmp done

found_start:
    mov rax, rcx
    jmp done

not_found:
    xor rax, rax

done:
    pop rsi
    pop rdi
    ret
find_pattern_asm endp

end
