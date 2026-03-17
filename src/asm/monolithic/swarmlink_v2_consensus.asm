; v23.0.0-SOVEREIGN-SWARM: 800B SHARD CONFIGURATION - SwarmLink v2 Consensus
; [D] FULL v23 INTEGRATION

; include \masm32\include\masm64rt.inc

.DATA
    align 16
    RingBuffer_ActiveIndex QWORD 0
    RingBuffer_Pointers    QWORD 256 dup(0) ; 256 active IO_URING/IOCP chunks
    Latency_Budget_Thresh  QWORD 85         ; 85ms target

.CODE

; -----------------------------------------------------------------------------
; SwarmLink_FastCopySIMD
; RCX = Destination (L2 or VRAM mapped buffer)
; RDX = Source (NVMe NO_BUFFERING L3 chunk)
; R8  = Size in bytes (Must be 64-byte aligned)
; -----------------------------------------------------------------------------
ALIGN 16
SwarmLink_FastCopySIMD PROC EXPORT
    ; Preserve non-volatile registers if necessary
    push rbx
    push rsi

    ; Ensure size is a multiple of 64 bytes for AVX2 streaming
    mov r9, r8
    shr r9, 6       ; unroll by 64 bytes (4x 128-bit or 2x 256-bit ymm)
    test r9, r9
    jz @@cleanup

@@simd_loop:
    ; Prefetch source
    prefetchnta [rdx + 256]

    ; Load 64 bytes using AVX2
    vmovdqa ymm0, ymmword ptr [rdx]
    vmovdqa ymm1, ymmword ptr [rdx + 32]

    ; Non-temporal streaming store to bypass L1/L2 cache pollution
    vmovntdq ymmword ptr [rcx], ymm0
    vmovntdq ymmword ptr [rcx + 32], ymm1

    add rdx, 64
    add rcx, 64
    dec r9
    jnz @@simd_loop

@@cleanup:
    ; Issue memory fence to ensure non-temporal stores are globally visible
    sfence
    
    pop rsi
    pop rbx
    ret
SwarmLink_FastCopySIMD ENDP

; -----------------------------------------------------------------------------
; SwarmLink_SyncConsensus
; Atomically updates the tensor pointer when the NVMe IO completes
; RCX = Tensor Pointer Pointer (ggml_tensor** data_ptr)
; RDX = New mapped physical address
; -----------------------------------------------------------------------------
ALIGN 16
SwarmLink_SyncConsensus PROC EXPORT
    mov rax, rdx
    lock xchg [rcx], rax  ; Atomic pointer swap
    ret
SwarmLink_SyncConsensus ENDP

END
