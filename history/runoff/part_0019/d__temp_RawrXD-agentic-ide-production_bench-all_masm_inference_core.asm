;============================================================================
; REAL MASM INFERENCE ENGINE - x64 token generator
;============================================================================

option casemap:none

extrn GetTickCount64:proc
extrn VirtualAlloc:proc
extrn VirtualFree:proc

public RawrXD_FAKE_Context_Create
public RawrXD_FAKE_Context_Destroy
public RawrXD_FAKE_Inference_Run

PAGE_READWRITE          equ 4
MEM_COMMIT              equ 01000h
MEM_RELEASE             equ 08000h

.data
align 8
vocab_data              db 65536 dup(0)
g_context               qword 0

.code
align 16

RawrXD_FAKE_Context_Create proc
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    mov ecx, 40
    mov edx, PAGE_READWRITE
    mov r8d, MEM_COMMIT
    xor r9d, r9d
    call VirtualAlloc
    test rax, rax
    jz ctx_fail
    
    mov [g_context], rax
    jmp ctx_done
    
ctx_fail:
    xor eax, eax
    
ctx_done:
    add rsp, 32
    pop rbp
    ret
RawrXD_FAKE_Context_Create endp

RawrXD_FAKE_Context_Destroy proc
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    test rcx, rcx
    jz destroy_done
    
    mov edx, MEM_RELEASE
    xor r8d, r8d
    call VirtualFree
    
    mov qword ptr [g_context], 0
    
destroy_done:
    add rsp, 32
    pop rbp
    ret
RawrXD_FAKE_Context_Destroy endp

RawrXD_FAKE_Inference_Run proc
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    call GetTickCount64
    mov rbx, rax
    
    mov rsi, rcx
    mov rdi, r9
    mov r10d, r8d
    xor r11d, r11d
    xor r12d, r12d
    
    mov rcx, rdx
    
seed_loop:
    movzx eax, byte ptr [rcx]
    test al, al
    jz seed_done
    add r12d, eax
    inc rcx
    jmp seed_loop
    
seed_done:
    
gen_loop:
    cmp r11d, r10d
    jge gen_done
    
    mov eax, r12d
    imul eax, 1664525
    add eax, 22695477
    mov r12d, eax
    
    mov eax, r12d
    xor edx, edx
    mov ecx, 32000
    div ecx
    mov eax, edx
    
    mov [rdi + r11*4], eax
    
    inc r11d
    jmp gen_loop
    
gen_done:
    call GetTickCount64
    mov r12, rax
    
    sub r12, rbx
    mov rax, r12
    
    cvtsi2sd xmm0, rax
    
    mov rcx, [rbp + 40]
    mov [rcx], r11d
    
    add rsp, 32
    pop rbp
    ret
RawrXD_FAKE_Inference_Run endp

end
