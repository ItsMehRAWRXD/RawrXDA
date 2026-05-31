; rawr_aperture_bypass_simple.asm
; Simplified x64 MASM for DDR5-to-GPU aperture bypass
; Core functions only - no unwind info for simplicity

.code

; ============================================================================
; EXPORTS
; ============================================================================

PUBLIC RawrAllocateHugePages
PUBLIC RawrPinMemory
PUBLIC RawrUnpinMemory
PUBLIC RawrPrefetchMemory
PUBLIC RawrSetThreadAffinityToNUMA0
PUBLIC RawrFlushCacheLines
PUBLIC RawrMemoryBarrier
PUBLIC RawrLargePagesAvailable

; ============================================================================
; LARGE PAGE ALLOCATION
; ============================================================================

RawrAllocateHugePages PROC
    ; rcx = size
    
    ; Align to 2MB
    mov rax, 1FFFFFh
    add rcx, rax
    not rax
    and rcx, rax
    
    ; VirtualAlloc(NULL, size, MEM_LARGE_PAGES|MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE)
    xor r9, r9              ; lpAddress = NULL
    mov r8, rcx             ; dwSize
    mov edx, 20003000h      ; flAllocationType
    mov ecx, 4              ; flProtect = PAGE_READWRITE
    sub rsp, 28h
    call VirtualAlloc
    add rsp, 28h
    ret
RawrAllocateHugePages ENDP

; ============================================================================
; MEMORY PINNING
; ============================================================================

RawrPinMemory PROC
    ; rcx = ptr, rdx = size
    
    ; Align to page boundary
    mov r8, rcx
    and r8, -4096
    
    ; Adjust size
    mov r9, rcx
    sub r9, r8
    add rdx, r9
    add rdx, 4095
    and rdx, -4096
    
    ; VirtualLock
    mov rcx, r8
    sub rsp, 28h
    call VirtualLock
    add rsp, 28h
    ret
RawrPinMemory ENDP

; ============================================================================
; MEMORY UNPINNING
; ============================================================================

RawrUnpinMemory PROC
    ; rcx = ptr, rdx = size
    
    mov r8, rcx
    and r8, -4096
    
    mov r9, rcx
    sub r9, r8
    add rdx, r9
    add rdx, 4095
    and rdx, -4096
    
    mov rcx, r8
    sub rsp, 28h
    call VirtualUnlock
    add rsp, 28h
    ret
RawrUnpinMemory ENDP

; ============================================================================
; PREFETCH MEMORY
; ============================================================================

RawrPrefetchMemory PROC
    ; rcx = ptr, rdx = size
    
    mov rax, rcx
    add rdx, rcx          ; rdx = end
    
prefetch_loop:
    cmp rax, rdx
    jae prefetch_done
    
    prefetchnta [rax]
    add rax, 64
    jmp prefetch_loop
    
prefetch_done:
    ret
RawrPrefetchMemory ENDP

; ============================================================================
; SET THREAD AFFINITY TO NUMA 0
; ============================================================================

RawrSetThreadAffinityToNUMA0 PROC
    sub rsp, 28h
    call GetCurrentThread
    mov rcx, rax
    mov edx, 0FFFFFFFFh    ; First 32 cores
    call SetThreadAffinityMask
    add rsp, 28h
    ret
RawrSetThreadAffinityToNUMA0 ENDP

; ============================================================================
; FLUSH CACHE LINES
; ============================================================================

RawrFlushCacheLines PROC
    ; rcx = ptr, rdx = size
    
    mov rax, rcx
    add rdx, rcx          ; rdx = end
    
flush_loop:
    cmp rax, rdx
    jae flush_done
    
    clflush [rax]
    add rax, 64
    jmp flush_loop
    
flush_done:
    mfence
    ret
RawrFlushCacheLines ENDP

; ============================================================================
; MEMORY BARRIER
; ============================================================================

RawrMemoryBarrier PROC
    mfence
    ret
RawrMemoryBarrier ENDP

; ============================================================================
; LARGE PAGES AVAILABLE (stub - always returns true)
; ============================================================================

RawrLargePagesAvailable PROC
    mov eax, 1
    ret
RawrLargePagesAvailable ENDP

; ============================================================================
; IMPORTS
; ============================================================================

EXTRN VirtualAlloc:PROC
EXTRN VirtualLock:PROC
EXTRN VirtualUnlock:PROC
EXTRN GetCurrentThread:PROC
EXTRN SetThreadAffinityMask:PROC

END
