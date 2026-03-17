; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_System_Primitives.asm  ─  Threading, Memory, Synchronization
; Core primitives that everything else depends on
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE
OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc
include \masm64\include64\ntdll.inc

includelib \masm64\lib64\kernel32.lib
includelib \masm64\lib64\ntdll.lib

; ═══════════════════════════════════════════════════════════════════════════════
; CONSTANTS
; ═══════════════════════════════════════════════════════════════════════════════
SPINLOCK_RETRIES        EQU 1000    ; Spin attempts before yielding
CACHE_LINE_SIZE         EQU 64      ; x64 cache line
MEMORY_ALLOCATION_ALIGNMENT EQU 64  ; For SIMD operations

; ═══════════════════════════════════════════════════════════════════════════════
; DATA SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.DATA
align 16
g_SystemPageSize        QWORD       0
g_NumberOfProcessors    DWORD       0
g_AllocationGranularity QWORD       0

; ═══════════════════════════════════════════════════════════════════════════════
; CODE SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; System_InitializePrimitives
; Must be called before any other unit
; ═══════════════════════════════════════════════════════════════════════════════
System_InitializePrimitives PROC FRAME
    push rbx
    sub rsp, 32
    
    ; Get system info
    lea rcx, [rsp + 16]
    call GetSystemInfo
    
    mov eax, [rsp + 16 + OFFSET SYSTEM_INFO.dwPageSize]
    mov g_SystemPageSize, rax
    
    mov eax, [rsp + 16 + OFFSET SYSTEM_INFO.dwNumberOfProcessors]
    mov g_NumberOfProcessors, eax
    
    mov eax, [rsp + 16 + OFFSET SYSTEM_INFO.dwAllocationGranularity]
    mov g_AllocationGranularity, rax
    
    add rsp, 32
    pop rbx
    ret
System_InitializePrimitives ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Spinlock_Acquire
; Exponential backoff spinlock with PAUSE instruction for HT efficiency
; RCX = pointer to lock variable (DWORD)
; ═══════════════════════════════════════════════════════════════════════════════
Spinlock_Acquire PROC FRAME
    push rbx
    push rsi
    
    mov rbx, rcx
    xor esi, esi                    ; Retry counter
    
@spin_loop:
    ; Test and test-and-set
    cmp dword ptr [rbx], 0
    jne @spin_wait
    
    ; Attempt acquire
    mov eax, 1
    lock cmpxchg dword ptr [rbx], eax
    jz @acquired
    
@spin_wait:
    ; Exponential backoff
    inc esi
    cmp esi, SPINLOCK_RETRIES
    jge @yield_thread
    
    ; PAUSE hint for hyperthreading
    pause
    jmp @spin_loop
    
@yield_thread:
    ; Spin too long, yield CPU
    call SwitchToThread
    xor esi, esi
    jmp @spin_loop
    
@acquired:
    pop rsi
    pop rbx
    ret
Spinlock_Acquire ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Spinlock_Release
; RCX = pointer to lock variable
; ═══════════════════════════════════════════════════════════════════════════════
Spinlock_Release PROC FRAME
    mov dword ptr [rcx], 0
    
    ; Memory barrier to ensure prior stores are visible
    lock add dword ptr [rsp], 0
    
    ret
Spinlock_Release ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; RWLock_AcquireRead
; Slim reader-writer lock, favor readers (many readers, few writers)
; RCX = pointer to lock (DWORD: high 16 bits = writers waiting, low 16 = active readers)
; ═══════════════════════════════════════════════════════════════════════════════
RWLock_AcquireRead PROC FRAME
    push rbx
    
    mov rbx, rcx
    
@read_retry:
    mov eax, [rbx]
    test eax, 0FFFF0000h            ; Writers waiting?
    jne @read_wait
    
    ; Try increment reader count
    lea edx, [eax + 1]
    lock cmpxchg [rbx], edx
    jne @read_retry
    
    pop rbx
    ret
    
@read_wait:
    pause
    jmp @read_retry
    
RWLock_AcquireRead ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; RWLock_ReleaseRead
; ═══════════════════════════════════════════════════════════════════════════════
RWLock_ReleaseRead PROC FRAME
    lock dec word ptr [rcx]         ; Decrement reader count
    ret
RWLock_ReleaseRead ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; RWLock_AcquireWrite
; ═══════════════════════════════════════════════════════════════════════════════
RWLock_AcquireWrite PROC FRAME
    push rbx
    
    mov rbx, rcx
    
    ; Increment writers waiting count (high word)
@write_retry:
    mov eax, [rbx]
    add eax, 10000h
    lock cmpxchg [rbx], eax
    jne @write_retry
    
    ; Wait for all readers to finish and no other writer
@write_wait:
    mov eax, [rbx]
    test eax, 0FFFFh                ; Active readers?
    jnz @write_pause
    
    ; Try to grab write lock (set high bit pattern)
    or eax, 80000000h               ; Mark as writing
    lock cmpxchg [rbx], eax
    jne @write_wait
    
    pop rbx
    ret
    
@write_pause:
    pause
    jmp @write_wait
    
RWLock_AcquireWrite ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; RWLock_ReleaseWrite
; ═══════════════════════════════════════════════════════════════════════════════
RWLock_ReleaseWrite PROC FRAME
    ; Clear write bit and decrement waiting count
    mov eax, [rcx]
    and eax, 7FFFFFFFh              ; Clear writing bit
    sub eax, 10000h                 ; Decrement waiting count
    mov [rcx], eax
    ret
RWLock_ReleaseWrite ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Aligned_Allocate
; Allocate memory aligned to specified boundary (for SIMD/DMA)
; RCX = size, RDX = alignment (must be power of 2)
; Returns RAX = aligned pointer, [RAX-8] = original pointer for free
; ═══════════════════════════════════════════════════════════════════════════════
Aligned_Allocate PROC FRAME
    push rbx
    push rsi
    
    mov rbx, rcx                    ; Size
    mov rsi, rdx                    ; Alignment
    
    ; Overallocate to ensure alignment space + room to store original
    add rcx, rsi
    add rcx, 16
    call HeapAlloc
    
    test rax, rax
    jz @alloc_fail
    
    ; Calculate aligned address
    mov rdx, rax
    add rdx, rsi
    add rdx, 8
    dec rsi                         ; Alignment - 1
    not rsi                         ; Mask
    and rdx, rsi                    ; Aligned address
    
    ; Store original pointer before aligned address
    mov [rdx - 8], rax
    
    mov rax, rdx
    
@alloc_fail:
    pop rsi
    pop rbx
    ret
Aligned_Allocate ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Aligned_Free
; RCX = aligned pointer from Aligned_Allocate
; ═══════════════════════════════════════════════════════════════════════════════
Aligned_Free PROC FRAME
    mov rcx, [rcx - 8]              ; Get original pointer
    jmp HeapFree                    ; Tail call
Aligned_Free ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Memory_PrefetchRead
; Software prefetch for cache optimization
; RCX = address, RDX = locality (0=NTA, 1=L1, 2=L2, 3=L3)
; ═══════════════════════════════════════════════════════════════════════════════
Memory_PrefetchRead PROC FRAME
    cmp edx, 0
    je @prefetch_nta
    cmp edx, 1
    je @prefetch_t0
    cmp edx, 2
    je @prefetch_t1
    cmp edx, 3
    je @prefetch_t2
    
@prefetch_nta:
    prefetchnta [rcx]
    ret
    
@prefetch_t0:
    prefetcht0 [rcx]
    ret
    
@prefetch_t1:
    prefetcht1 [rcx]
    ret
    
@prefetch_t2:
    prefetcht2 [rcx]
    ret
    
Memory_PrefetchRead ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Thread_AffinitySet
; Pin thread to specific CPU core for cache affinity
; RCX = thread handle, RDX = core index
; ═══════════════════════════════════════════════════════════════════════════════
Thread_AffinitySet PROC FRAME
    mov r8, 1
    mov rcx, rdx
    shl r8, cl                      ; 1 << core_index
    
    ; SetThreadAffinityMask
    mov rdx, r8
    ; RCX already has thread handle
    jmp SetThreadAffinityMask
Thread_AffinitySet ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; EXPORTS
; ═══════════════════════════════════════════════════════════════════════════════
PUBLIC System_InitializePrimitives
PUBLIC Spinlock_Acquire
PUBLIC Spinlock_Release
PUBLIC RWLock_AcquireRead
PUBLIC RWLock_ReleaseRead
PUBLIC RWLock_AcquireWrite
PUBLIC RWLock_ReleaseWrite
PUBLIC Aligned_Allocate
PUBLIC Aligned_Free
PUBLIC Memory_PrefetchRead
PUBLIC Thread_AffinitySet

END