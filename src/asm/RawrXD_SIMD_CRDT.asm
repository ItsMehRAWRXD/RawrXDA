; =========================================================================================
; RawrXD_SIMD_CRDT.asm - Lock-free SIMD-Accelerated Convergent Replicated Data Type
; Part of RawrXD-Win32IDE Sovereign V2 Distributed Memory Index
; =========================================================================================

.code

; --- Procedures ---

; RawrXD_SIMD_CRDT_Merge
; Merges two memory index pages using SIMD-accelerated Last-Write-Wins (LWW).
; Parameters: RCX = Local Index, RDX = Remote Index, R8 = Total Entries
RawrXD_SIMD_CRDT_Merge proc
    push rbp
    mov rbp, rsp
    
    ; 1. Load 4 timestamp/vector counters via YMM (AVX2/SIMD)
    ; 2. Compare-Greater-Than-Timestamp (VPCMPGTQ)
    ; 3. Blend new values into local index (VPBLENDVB)
    
    ; Pointer increments
    mov r9, 0
@@simd_loop:
    ; Load 4 timestamp/counter pairs = 32 bytes (YMM size)
    vmovdqu ymm0, [rcx + r9] ; Local
    vmovdqu ymm1, [rdx + r9] ; Remote
    
    ; Vectorized comparison: RDX[i] > RCX[i]
    vpcmpgtq ymm2, ymm1, ymm0
    
    ; Blend Remote into Local using mask
    vpblendvb ymm0, ymm0, ymm1, ymm2
    
    ; Store back to local index
    vmovdqu [rcx + r9], ymm0
    
    add r9, 32
    cmp r9, r8
    jb @@simd_loop

    pop rbp
    ret
RawrXD_SIMD_CRDT_Merge endp

end
