; SwarmLink v2 Consensus Layer - v23.0.0-D
; Strict ABI for Core_v23.dll Fusion
; (c) 2026 RawrXD - Sovereign Hybrid

.code

PUBLIC SwarmLink_FastCopySIMD
PUBLIC SwarmLink_SyncConsensus

; SwarmLink_FastCopySIMD
; RCX = Dest, RDX = Source, R8 = Size (Bytes)
; Optimized for vmovntdq (Non-Temporal Streaming for L3 Ring Buffer)
SwarmLink_FastCopySIMD proc
    push rbp
    mov rbp, rsp
    
    ; Ensure 64-byte alignment check
    test rcx, 63
    jnz @slow_copy
    test rdx, 63
    jnz @slow_copy
    
    shr r8, 6 ; Number of 64-byte chunks
    jz @slow_copy

@stream_loop:
    vmovdqa64 zmm0, [rdx]
    vmovntdq [rcx], zmm0 ; Stream to memory, bypass cache
    add rdx, 64
    add rcx, 64
    dec r8
    jnz @stream_loop
    
    sfence ; Global visibility for non-temporal stores
    jmp @done

@slow_copy:
    ; Fallback to standard rep movsb for unaligned/small blocks
    mov rcx, rcx
    mov rsi, rdx
    mov rdi, rcx
    mov rcx, r8
    rep movsb

@done:
    pop rbp
    ret
SwarmLink_FastCopySIMD endp

; SwarmLink_SyncConsensus
; RCX = LocalGen, RDX = RemoteGen, R8 = ConsensusStatusPtr
; Atomic tensor state verification for 800B-D sharding
SwarmLink_SyncConsensus proc
    ; 1. Atomic compare of generation counters
    cmp rcx, rdx
    jne @desync
    
    ; 2. Lock-step acquisition
    mov eax, 1
    lock xchg [r8], eax ; Atomic status set to 1 (Consensus Reached)
    mov rax, 1
    ret

@desync:
    xor eax, eax
    lock xchg [r8], eax ; Atomic status set to 0 (Rollback Required)
    xor rax, rax
    ret
SwarmLink_SyncConsensus endp

END
