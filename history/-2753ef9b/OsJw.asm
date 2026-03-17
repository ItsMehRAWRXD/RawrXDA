; zen4_streaming_store.asm
; Zen4-optimized streaming store using non-temporal instructions
; void zen4_streaming_store(void* dest, const void* src, size_t size)
; RCX = dest, RDX = src, R8 = size

.code

zen4_streaming_store PROC FRAME
    ; Align to 64-byte boundary
    mov rax, rcx
    and rax, 63
    jz @aligned
    
    ; Handle misaligned head
    sub r8, rax
    mov r9b, [rdx]
    mov [rcx], r9b
    add rcx, rax
    add rdx, rax
    
@aligned:
    ; Process in 512-byte chunks (8 cache lines)
    mov rax, r8
    shr rax, 9  ; /512
    
    test rax, rax
    jz @remainder
    
@loop:
    ; Prefetch next 512 bytes
    prefetcht0 [rdx + 512]    ; L1
    prefetcht1 [rdx + 1024]   ; L2
    prefetcht2 [rdx + 2048]   ; L3
    
    ; Load 512 bytes (8x zmm)
    vmovdqu32 zmm0, [rdx]
    vmovdqu32 zmm1, [rdx + 64]
    vmovdqu32 zmm2, [rdx + 128]
    vmovdqu32 zmm3, [rdx + 192]
    vmovdqu32 zmm4, [rdx + 256]
    vmovdqu32 zmm5, [rdx + 320]
    vmovdqu32 zmm6, [rdx + 384]
    vmovdqu32 zmm7, [rdx + 448]
    
    ; Non-temporal stores (bypass cache)
    vmovntdq [rcx], zmm0
    vmovntdq [rcx + 64], zmm1
    vmovntdq [rcx + 128], zmm2
    vmovntdq [rcx + 192], zmm3
    vmovntdq [rcx + 256], zmm4
    vmovntdq [rcx + 320], zmm5
    vmovntdq [rcx + 384], zmm6
    vmovntdq [rcx + 448], zmm7
    
    add rcx, 512
    add rdx, 512
    sub rax, 1
    jnz @loop
    
    sfence  ; Ensure all NT stores complete
    
@remainder:
    ; Handle remaining bytes (< 512)
    mov rax, r8
    and rax, 511
    
    test rax, rax
    jz @done
    
    ; Fallback to regular stores for remainder
    rep movsb
    
@done:
    ret
zen4_streaming_store ENDP

END
