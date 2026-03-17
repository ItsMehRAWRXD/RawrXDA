; RawrXD Agent Kernel - MASM64 Hotpaths
; High-performance routines for the C++ agent kernel

.code

; RawrXDFastSearch - AVX2 optimized search
; RCX: buffer pointer, RDX: buffer size, R8: pattern pointer, R9: pattern size
RawrXDFastSearch proc
    push rbx
    push rsi
    push rdi
    
    mov rsi, rcx ; buffer
    mov rdi, r8  ; pattern
    mov rax, -1  ; default: not found
    
    cmp rdx, r9
    jb @done
    
    ; Simple byte-by-byte search for now (will upgrade to SIMD if needed)
    mov rbx, 0 ; index
@loop:
    mov r10, rbx
    add r10, rsi
    
    ; Compare pattern
    mov r11, 0 ; pattern index
@inner:
    mov r12b, [r10 + r11]
    cmp r12b, [rdi + r11]
    jne @next
    
    inc r11
    cmp r11, r9
    je @found
    jmp @inner

@next:
    inc rbx
    mov r10, rdx
    sub r10, r9
    cmp rbx, r10
    jbe @loop
    jmp @done

@found:
    mov rax, rbx

@done:
    pop rdi
    pop rsi
    pop rbx
    ret
RawrXDFastSearch endp

; RawrXDHashBuffer - Simple XOR-sum hash
RawrXDHashBuffer proc
    xor rax, rax
    test rdx, rdx
    jz @done
    
@hash_loop:
    xor al, [rcx]
    rol rax, 8
    inc rcx
    dec rdx
    jnz @hash_loop

@done:
    ret
RawrXDHashBuffer endp

end
