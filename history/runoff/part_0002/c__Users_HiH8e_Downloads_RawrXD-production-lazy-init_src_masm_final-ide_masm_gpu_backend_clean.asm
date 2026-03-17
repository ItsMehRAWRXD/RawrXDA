; masm_gpu_backend_clean.asm - Simple, working GPU backend stub
; Minimal implementation to support orchestration

option casemap:none

.code

; External functions we use
EXTERN console_log:PROC

; Simple GPU backend initialization
PUBLIC gpu_backend_init
gpu_backend_init PROC

    ; Simple GPU backend initialization (stub)
    ; Returns: rax = backend type (1 = CPU, 2 = GPU, etc.)
    
    ; Log initialization
    lea rcx, szGpuInit
    call console_log
    
    ; Return CPU backend for now
    mov eax, 1                    ; CPU backend
    ret
gpu_backend_init ENDP

.data
szGpuInit    BYTE "[gpu] GPU backend initialized (CPU fallback)", 13, 10, 0

.code

END
