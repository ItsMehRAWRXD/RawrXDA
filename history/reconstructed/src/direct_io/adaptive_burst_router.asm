; src/direct_io/adaptive_burst_router.asm
; RAWRXD v1.2.0 — ADAPTIVE BURST ROUTER (Stub for linking)

.code

Router_Init PROC
    xor rax, rax
    ret
Router_Init ENDP

Router_GetRoute PROC
    mov eax, ecx
    ret
Router_GetRoute ENDP

Router_GetStats PROC
    push rdi
    mov rdi, rcx
    xor eax, eax
    mov ecx, 320
    rep stosb
    mov eax, 5
    pop rdi
    ret
Router_GetStats ENDP

END
