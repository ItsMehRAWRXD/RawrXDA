; CUDA interop placeholders (MASM x64)
option casemap:none

PUBLIC cuda_init
PUBLIC cuda_shutdown

.code
cuda_init PROC
    ; TODO: Dynamically probe CUDA (if present)
    xor rax, rax
    ret
cuda_init ENDP

cuda_shutdown PROC
    xor rax, rax
    ret
cuda_shutdown ENDP

END
