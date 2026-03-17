.code

; void SwarmLink_FastCopySIMD(void* dest, const void* src, size_t numBytes);
; rcx = dest
; rdx = src
; r8 = numBytes ( assumed multiple of 64 bytes for page-aligned fast path)

SwarmLink_FastCopySIMD PROC
    ; Save volatile registers used
    push rbp
    mov rbp, rsp
    
    ; Setup iteration counter 
    ; Divide by 64 explicitly (processing 64 bytes per loop using AVX2)
    shr r8, 6

copy_loop:
    test r8, r8
    jz copy_done

    ; Load 64 bytes using YMM registers (AVX)
    vmovdqa ymm0, ymmword ptr [rdx]
    vmovdqa ymm1, ymmword ptr [rdx + 32]
    
    ; Store non-temporal (bypass cache for direct writing/L3 streaming)
    vmovntdq ymmword ptr [rcx], ymm0
    vmovntdq ymmword ptr [rcx + 32], ymm1
    
    add rcx, 64
    add rdx, 64
    dec r8
    jmp copy_loop

copy_done:
    ; Zero out YMM state
    vzeroupper
    
    mov rsp, rbp
    pop rbp
    ret
SwarmLink_FastCopySIMD ENDP

END
