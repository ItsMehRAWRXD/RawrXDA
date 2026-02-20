; Brutal deflate compression (MASM x64)
; Simple LZ77-based compression for speed
option casemap:none

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


PUBLIC deflate_brutal_masm

; Compression parameters
LZ_WINDOW_SIZE equ 4096
LZ_MIN_MATCH equ 3
LZ_MAX_MATCH equ 258

.code
deflate_brutal_masm PROC
    ; RCX = input buffer, RDX = input size, R8 = output buffer, R9 = output size ptr
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    
    sub rsp, 40h
    
    ; Validate inputs
    test rcx, rcx
    jz @@error
    test rdx, rdx
    jz @@error
    test r8, r8
    jz @@error
    test r9, r9
    jz @@error
    
    mov rsi, rcx        ; input
    mov rdi, r8         ; output
    mov r12, rdx        ; input size
    mov r13, r9         ; output size ptr
    xor r14, r14        ; input pos
    xor r15, r15        ; output pos
    
    ; Simple copy for now (no compression)
    cmp r12, r15
    jbe @@done_compress
    
    mov rcx, r12
    rep movsb
    
    mov r15, r12
    
@@done_compress:
    ; Store output size
    mov rax, r13
    mov [rax], r15
    
    ; Return success
    xor rax, rax
    jmp @@exit
    
@@error:
    mov rax, 1
    
@@exit:
    add rsp, 40h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
deflate_brutal_masm ENDP

END
