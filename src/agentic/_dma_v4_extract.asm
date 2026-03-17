    call UpdateStatistics
    
    mov eax, r12d
    jmp copy_cleanup
    
copy_invalid:
    mov eax, TITAN_ERROR_INVALID_PARAM
    jmp copy_cleanup
    
copy_cleanup:
    add rsp, 96
    pop rbx
    pop r12
    pop r13
    pop r14
    pop r15
    ret
    
ALIGN 4
const_0_001 REAL4 0.001
Titan_PerformCopy ENDP

;-----------------------------------------------------------------------------
; Titan_PerformDMA - Perform DMA operation
; RCX = DMA type
; RDX = DMA_REQUEST ptr
; R8  = completion event ptr (optional)
; R9  = timeout ms
; Returns: EAX = status
;-----------------------------------------------------------------------------
Titan_PerformDMA PROC EXPORT FRAME
    push r15
    .pushreg r15
    push r14
    .pushreg r14
    push r13
    .pushreg r13
    push r12
    .pushreg r12
    push rbx
    .pushreg rbx
    sub rsp, 64
    .allocstack 64
    .endprolog
    
    mov r12d, ecx
    mov r13, rdx
    mov r14, r8
    mov r15, r9
    
    ; Validate type
    cmp r12d, 2
    ja dma_invalid
    
    ; Validate request
    test r13, r13
    jz dma_invalid
    
    mov rsi, [r13].DMA_REQUEST.srcAddr
    mov rdi, [r13].DMA_REQUEST.dstAddr
    mov rbx, [r13].DMA_REQUEST.size
    
    test rsi, rsi
    jz dma_invalid
    test rdi, rdi
    jz dma_invalid
    test rbx, rbx
    jz dma_invalid
    
    ; Get start time
    call GetCurrentTimestamp
    mov [r13].DMA_REQUEST.timestamp, rax
    
    ; DMA type routing: CPU=0, VULKAN=1, DIRECTSTORAGE=2
    cmp r12d, DMA_TYPE_DIRECTSTORAGE
    je dma_exec_ds
    cmp r12d, DMA_TYPE_VULKAN
    je dma_exec_vk
    
    ; DMA_TYPE_CPU: optimized memory copy
dma_exec_cpu:
    mov rcx, rsi
    mov rdx, rdi
    mov r8, rbx
    
    ; Size-based temporal mode selection
    cmp rbx, NONTEMPORAL_THRESHOLD
    jbe dma_cpu_small
    
    ; Non-temporal for large DMA (>256KB cache bypass)
    mov r9, 1
    jmp dma_cpu_exec
    
dma_cpu_small:
    xor r9, r9
    
dma_cpu_exec:
    ; Setup kernel params
    lea rdi, [rsp+32]
    mov [rdi], rcx
    mov [rdi+8], rdx
    mov [rdi+16], r8
    mov DWORD PTR [rdi+56], r9d
    
    mov rcx, rdi
    call Kernel_Copy
    mov ebx, eax
    jmp dma_exec_done
    
dma_exec_ds:
    ; DirectStorage DMA: ReadFile non-buffered bypass
    push rsi
    push rdi
    
    mov rcx, rsi               ; File handle
    mov rdx, rdi               ; GPU buffer
    mov r8, rbx                ; Transfer size
    
    sub rsp, 48
    xor rax, rax
    mov [rsp], rax             ; OVERLAPPED.Internal
    mov [rsp+8], rax           ; OVERLAPPED.InternalHigh
    mov [rsp+16], eax          ; Offset
    mov [rsp+24], eax          ; OffsetHigh
    mov rax, [r13].DMA_REQUEST.eventHandle
    mov [rsp+32], rax          ; hEvent
    
    mov r9, rsp
    lea rax, [rsp+40]
    push rax
    sub rsp, 32
    call QWORD PTR [__imp_ReadFile]
    add rsp, 40
    
    test eax, eax
    jnz @@dma_ds_ok
    
    call QWORD PTR [__imp_GetLastError]
    cmp eax, 997
    jne @@dma_ds_err
    
    mov rcx, [r13].DMA_REQUEST.eventHandle
    mov edx, r15d
    call QWORD PTR [__imp_WaitForSingleObject]
    test eax, eax
    jnz @@dma_ds_err
    
@@dma_ds_ok:
    add rsp, 48
    pop rdi
    pop rsi
    xor ebx, ebx
    jmp dma_exec_done

@@dma_ds_err:
    add rsp, 48
    pop rdi
    pop rsi
    mov ebx, TITAN_ERROR_DMA_FAILED
    jmp dma_exec_done

dma_exec_vk:
    ; Vulkan DMA: AVX-512 non-temporal GPU-bound transfer
    push rsi
    push rdi
