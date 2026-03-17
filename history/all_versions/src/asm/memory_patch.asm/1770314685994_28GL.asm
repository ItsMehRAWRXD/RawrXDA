; RawrXD-Shell - Memory Layer Hotpatching (MASM64)
extern VirtualProtect : proc

.code

; extern "C" int apply_memory_patch_asm(void* addr, size_t size, const void* data)
; RCX = addr, RDX = size, R8 = data
apply_memory_patch_asm proc
    push rbp
    mov rbp, rsp
    sub rsp, 40 ; shadow space + oldProtect
    
    mov r10, rcx ; addr
    mov r11, rdx ; size
    mov r12, r8  ; data
    
    lea r9, [rbp-8] ; lpflOldProtect
    mov r8, 40h     ; PAGE_EXECUTE_READWRITE
    ; RCX = addr, RDX = size already
    call VirtualProtect
    
    test rax, rax
    jz fail
    
    ; Copy data
    mov rcx, r11 ; size
    mov rsi, r12 ; src
    mov rdi, r10 ; dest
    rep movsb
    
    ; Restore protection (optional, but safer)
    ; lea r9, [rbp-8] ; use new oldProtect
    ; mov r8, [rbp-8] ; previous oldProtect
    ; mov rdx, r11
    ; mov rcx, r10
    ; call VirtualProtect
    
    mov rax, 1 ; success
    jmp done

fail:
    xor rax, rax ; failure

done:
    add rsp, 40
    pop rbp
    ret
apply_memory_patch_asm endp

end
