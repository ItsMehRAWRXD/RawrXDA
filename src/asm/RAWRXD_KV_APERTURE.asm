;==============================================================================
; RAWRXD_KV_APERTURE.asm
; APERTURE MEMORY BYPASS - KV-CACHE PAGING & NON-TEMPORAL STREAMING
;==============================================================================
OPTION CASEMAP:NONE

.DATA
; Diagnostic counters for memory pressure
g_kv_pages_flushed   QWORD 0
g_kv_aperture_hits   QWORD 0

.CODE

;------------------------------------------------------------------------------
; KV_ApertureMap(pBase:rcx, nSize:rdx)
; Establishes a non-cached or non-temporal mapping pattern for the KV context.
; Currently uses prefetch hints to prepare the TLB for high-velocity access.
;------------------------------------------------------------------------------
KV_ApertureMap PROC
    test rcx, rcx
    jz @@done
    
    ; Warm up the TLB for the aperture
    mov rax, rcx
    mov r8, rdx
    shr r8, 12           ; Number of 4KB pages
    jz @@done
    
@@map_loop:
    prefetchw [rax]      ; Prefetch for write
    add rax, 4096
    dec r8
    jnz @@map_loop

    lock inc g_kv_aperture_hits
@@done:
    ret
KV_ApertureMap ENDP

;------------------------------------------------------------------------------
; KV_PageFlush(pData:rcx, nBytes:rdx)
; Forces write-back of cache lines to memory without full serialization.
; Uses CLWB (if available) or CLFLUSHOPT for minimal latency.
;------------------------------------------------------------------------------
KV_PageFlush PROC
    test rcx, rcx
    jz @@done
    
    mov rax, rcx
    mov r8, rdx
    shr r8, 6            ; 64-byte cache lines
    jz @@done

@@flush_loop:
    clwb [rax]           ; Cache Line Write Back (Intel/AMD Zen2+)
    add rax, 64
    dec r8
    jnz @@flush_loop
    
    sfence               ; Ensure flushes are ordered before next GEMV
    lock inc g_kv_pages_flushed
@@done:
    ret
KV_PageFlush ENDP

;------------------------------------------------------------------------------
; KV_StreamStore_AVX512(pDst:rcx, pSrc:rdx, nFloats:r8d)
; Non-temporal (streaming) store of KV tensors to avoid L3 cache pollution.
; Process 16 floats (64 bytes) per iteration using ZMM registers.
;------------------------------------------------------------------------------
KV_StreamStore_AVX512 PROC
    mov eax, r8d
    shr eax, 4           ; n / 16
    jz @@done

@@store_loop:
    vmovups zmm0, zmmword ptr [rdx]  ; Load from compute buffer (likely in L1/L2)
    vmovntps zmmword ptr [rcx], zmm0 ; Streaming store to KV Aperture (bypasses cache)
    add rcx, 64
    add rdx, 64
    dec eax
    jnz @@store_loop

    sfence               ; Canonical sync for NT stores
@@done:
    ret
KV_StreamStore_AVX512 ENDP

END
