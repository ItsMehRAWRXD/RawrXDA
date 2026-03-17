; Titan_Streaming_Orchestrator_Fixed.asm
; Fixed version with proper unwind info, large immediate handling, and alignment

OPTION ALIGN:64
OPTION CASEMAP:NONE
OPTION WIN64:3

; ============================================================
; INLINED STRUCTURES (replacing masm64rt.inc)
; ============================================================

; SRWLOCK structure
SRWLOCK STRUCT
    Ptr QWORD ?
SRWLOCK ENDS

; OVERLAPPED structure  
OVERLAPPED STRUCT
    Internal      QWORD ?
    InternalHigh  QWORD ?
    UNION
        STRUCT
            Offset      DWORD ?
            OffsetHigh  DWORD ?
        ENDS
        Pointer PVOID ?
    ENDS
    hEvent        HANDLE ?
OVERLAPPED ENDS

; SOCKADDR_INET union
SOCKADDR_INET UNION
    Ipv4  SOCKADDR_IN <>
    Ipv6  SOCKADDR_IN6 <>
    si_family SHORT ?
SOCKADDR_INET ENDS

; SOCKADDR_IN structure
SOCKADDR_IN STRUCT
    sin_family    WORD ?
    sin_port      WORD ?
    sin_addr      DWORD ?
    sin_zero      BYTE 8 DUP(?)
SOCKADDR_IN ENDS

; SOCKADDR_IN6 structure
SOCKADDR_IN6 STRUCT
    sin6_family     WORD ?
    sin6_port       WORD ?
    sin6_flowinfo   DWORD ?
    sin6_addr       BYTE 16 DUP(?)
    sin6_scope_id   DWORD ?
SOCKADDR_IN6 ENDS

; ============================================================
; CORE CONSTANTS
; ============================================================

; Memory constants
MEM_COMMIT              EQU 00001000h
MEM_RESERVE             EQU 00002000h
MEM_RELEASE             EQU 00008000h
MEM_LARGE_PAGES         EQU 20000000h
MEM_PHYSICAL            EQU 00400000h
PAGE_READWRITE          EQU 04h
PAGE_EXECUTE_READWRITE  EQU 40h

; File constants
FILE_FLAG_SEQUENTIAL_SCAN   EQU 08000000h
FILE_FLAG_NO_BUFFERING      EQU 20000000h
FILE_FLAG_OVERLAPPED        EQU 40000000h
FILE_ATTRIBUTE_NORMAL       EQU 00000080h
GENERIC_READ                EQU 80000000h
FILE_SHARE_READ             EQU 00000001h
OPEN_EXISTING               EQU 3

; Wait constants
WAIT_OBJECT_0       EQU 0
WAIT_TIMEOUT        EQU 00000102h
WAIT_FAILED         EQU 0FFFFFFFFh
INFINITE            EQU 0FFFFFFFFh

; Error constants
ERROR_SUCCESS           EQU 0
ERROR_INVALID_PARAMETER EQU 57
ERROR_NOT_ENOUGH_MEMORY EQU 8
ERROR_IO_PENDING        EQU 997

; Socket constants
AF_INET         EQU 2
AF_INET6        EQU 23
SOCK_DGRAM      EQU 2
SOCK_STREAM     EQU 1
IPPROTO_UDP     EQU 17
IPPROTO_TCP     EQU 6

; Alignment
CACHE_LINE_SIZE EQU 64

; ============================================================
; EXTERNAL IMPORTS
; ============================================================

EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN CreateFileA:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN GetFileSizeEx:PROC
EXTERN GetLastError:PROC
EXTERN SetLastError:PROC
EXTERN CloseHandle:PROC
EXTERN CreateEventA:PROC
EXTERN WaitForSingleObject:PROC
EXTERN WaitForMultipleObjects:PROC
EXTERN ResetEvent:PROC
EXTERN SetEvent:PROC
EXTERN CreateThread:PROC
EXTERN ExitThread:PROC
EXTERN Sleep:PROC
EXTERN QueryPerformanceCounter:PROC
EXTERN QueryPerformanceFrequency:PROC
EXTERN InitializeSRWLock:PROC
EXTERN AcquireSRWLockExclusive:PROC
EXTERN ReleaseSRWLockExclusive:PROC
EXTERN AcquireSRWLockShared:PROC
EXTERN ReleaseSRWLockShared:PROC
EXTERN TryAcquireSRWLockExclusive:PROC
EXTERN RtlCopyMemory:PROC
EXTERN RtlZeroMemory:PROC
EXTERN RtlFillMemory:PROC

; Winsock
EXTERN WSAStartup:PROC
EXTERN WSACleanup:PROC
EXTERN socket:PROC
EXTERN bind:PROC
EXTERN sendto:PROC
EXTERN recvfrom:PROC
EXTERN closesocket:PROC
EXTERN WSAGetLastError:PROC
EXTERN htons:PROC
EXTERN inet_addr:PROC

; ============================================================
; INCLUDELIBS
; ============================================================

includelib kernel32.lib
includelib ws2_32.lib

; ============================================================
; DATA SECTION
; ============================================================

.DATA

; Large constants for comparisons (loaded into registers)
CONST_7B    QWORD 7000000000      ; 7 billion
CONST_13B   QWORD 13000000000     ; 13 billion  
CONST_70B   QWORD 70000000000     ; 70 billion
CONST_200B  QWORD 200000000000    ; 200 billion
CONST_1GB   QWORD 1073741824      ; 1 GB
CONST_64MB  QWORD 67108864        ; 64 MB

; Global state
ALIGN 64
g_OrchestratorState     QWORD 0
g_hCompletionPort       QWORD 0
g_RingBufferBase        QWORD 0
g_RingBufferSize        QWORD 67108864    ; 64MB

; SRW locks
ALIGN 64
g_LockScheduler         SRWLOCK <>
g_LockConflictDetector  SRWLOCK <>
g_LockHeartbeat         SRWLOCK <>

; Statistics
ALIGN 64
g_TotalBytesStreamed    QWORD 0
g_TotalChunksProcessed  QWORD 0
g_ConflictCount         QWORD 0
g_StartTime             QWORD 0

; ============================================================
; CODE SECTION
; ============================================================

.CODE

; ============================================================
; HELPER: Get current time in microseconds
; ============================================================
Titan_GetMicroseconds PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 16
    .allocstack 16
    .endprolog
    
    lea rcx, [rsp+8]        ; QueryPerformanceCounter parameter
    call QueryPerformanceCounter
    
    mov rbx, [rsp+8]        ; Current counter
    
    lea rcx, [rsp]
    call QueryPerformanceFrequency
    
    mov rax, [rsp]          ; Frequency
    
    ; Convert to microseconds: (counter * 1000000) / frequency
    mov rcx, 1000000
    mov rax, rbx
    mul rcx                 ; rdx:rax = counter * 1000000
    div QWORD PTR [rsp]     ; rax = (counter * 1000000) / frequency
    
    add rsp, 16
    pop rsi
    pop rbx
    ret
Titan_GetMicroseconds ENDP

; ============================================================
; LOCK HELPERS (with proper unwind info)
; ============================================================

Titan_LockScheduler PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 8
    .allocstack 8
    .endprolog
    
    lea rcx, g_LockScheduler
    call AcquireSRWLockExclusive
    
    add rsp, 8
    pop rbx
    ret
Titan_LockScheduler ENDP

Titan_UnlockScheduler PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 8
    .allocstack 8
    .endprolog
    
    lea rcx, g_LockScheduler
    call ReleaseSRWLockExclusive
    
    add rsp, 8
    pop rbx
    ret
Titan_UnlockScheduler ENDP

Titan_LockConflictDetector PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 8
    .allocstack 8
    .endprolog
    
    lea rcx, g_LockConflictDetector
    call AcquireSRWLockExclusive
    
    add rsp, 8
    pop rbx
    ret
Titan_LockConflictDetector ENDP

Titan_UnlockConflictDetector PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 8
    .allocstack 8
    .endprolog
    
    lea rcx, g_LockConflictDetector
    call ReleaseSRWLockExclusive
    
    add rsp, 8
    pop rbx
    ret
Titan_UnlockConflictDetector ENDP

Titan_LockHeartbeat PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 8
    .allocstack 8
    .endprolog
    
    lea rcx, g_LockHeartbeat
    call AcquireSRWLockExclusive
    
    add rsp, 8
    pop rbx
    ret
Titan_LockHeartbeat ENDP

Titan_UnlockHeartbeat PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 8
    .allocstack 8
    .endprolog
    
    lea rcx, g_LockHeartbeat
    call ReleaseSRWLockExclusive
    
    add rsp, 8
    pop rbx
    ret
Titan_UnlockHeartbeat ENDP

; ============================================================
; DMA TRANSFER HELPERS (with proper unwind info)
; ============================================================

Titan_DMA_Transfer_Layer PROC FRAME
    ; Parameters: rcx = layer_idx, rdx = dst_buffer, r8 = size
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov rbx, rcx            ; layer_idx
    mov rsi, rdx            ; dst_buffer
    mov rdi, r8             ; size
    
    ; Calculate source address in ring buffer
    mov rax, g_RingBufferBase
    mov rcx, rbx
    imul rcx, rdi           ; layer_idx * size
    add rax, rcx            ; source = base + offset
    
    ; Perform async DMA transfer (simulated with copy for now)
    mov rcx, rsi            ; dst
    mov rdx, rax            ; src
    mov r8, rdi             ; size
    call RtlCopyMemory
    
    ; Update statistics
    call Titan_LockScheduler
    add g_TotalBytesStreamed, rdi
    inc g_TotalChunksProcessed
    call Titan_UnlockScheduler
    
    xor rax, rax            ; Return success
    
    add rsp, 40
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_DMA_Transfer_Layer ENDP

Titan_Evict_LRU_Slot PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 24
    .allocstack 24
    .endprolog
    
    ; Find least recently used slot
    ; Simplified: just evict slot 0 for now
    
    xor rbx, rbx            ; slot 0
    
    ; Mark slot as free
    ; (Actual implementation would track usage timestamps)
    
    mov rax, rbx            ; Return evicted slot index
    
    add rsp, 24
    pop rsi
    pop rbx
    ret
Titan_Evict_LRU_Slot ENDP

; ============================================================
; SCHEDULER INITIALIZATION (with explicit register setup for HeapAlloc)
; ============================================================

Titan_InitScheduler PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 56
    .allocstack 56
    .endprolog
    
    ; Allocate scheduler state
    call GetProcessHeap
    mov rbx, rax            ; Save heap handle
    
    mov rcx, rbx            ; hHeap
    xor edx, edx            ; dwFlags = 0
    mov r8d, 4096           ; dwBytes = 4KB
    call HeapAlloc
    
    test rax, rax
    jz @@alloc_failed
    
    mov g_OrchestratorState, rax
    
    ; Zero the memory
    mov rcx, rax
    xor edx, edx
    mov r8d, 4096
    call RtlZeroMemory
    
    ; Initialize SRW lock
    lea rcx, g_LockScheduler
    call InitializeSRWLock
    
    ; Initialize ring buffer
    mov rcx, g_RingBufferSize
    mov rdx, MEM_COMMIT
    or rdx, MEM_RESERVE
    or rdx, MEM_LARGE_PAGES
    mov r8, PAGE_READWRITE
    call VirtualAlloc
    
    test rax, rax
    jz @@ring_failed
    
    mov g_RingBufferBase, rax
    
    xor rax, rax            ; Success
    jmp @@done
    
@@ring_failed:
    ; Free scheduler state
    mov rcx, rbx
    xor edx, edx
    mov r8, g_OrchestratorState
    call HeapFree
    mov g_OrchestratorState, 0
    
@@alloc_failed:
    mov rax, -1             ; Failure
    
@@done:
    add rsp, 56
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_InitScheduler ENDP

; ============================================================
; CONFLICT DETECTOR (with explicit register setup)
; ============================================================

Titan_InitConflictDetector PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    ; Allocate conflict tracking table
    call GetProcessHeap
    mov rbx, rax
    
    mov rcx, rbx
    xor edx, edx
    mov r8d, 65536          ; 64KB for conflict table
    call HeapAlloc
    
    test rax, rax
    jz @@failed
    
    ; Zero table
    mov rcx, rax
    xor edx, edx
    mov r8d, 65536
    call RtlZeroMemory
    
    ; Initialize lock
    lea rcx, g_LockConflictDetector
    call InitializeSRWLock
    
    xor rax, rax
    jmp @@done
    
@@failed:
    mov rax, -1
    
@@done:
    add rsp, 40
    pop rsi
    pop rbx
    ret
Titan_InitConflictDetector ENDP

Titan_DetectConflict PROC FRAME
    ; rcx = layer_idx, rdx = patch_id
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 24
    .allocstack 24
    .endprolog
    
    mov rbx, rcx
    mov rsi, rdx
    
    call Titan_LockConflictDetector
    
    ; Check if this layer+patch combination already exists
    ; (Simplified hash check)
    mov rax, rbx
    xor rax, rsi
    and rax, 0FFFh          ; 4096 entries
    
    ; In real implementation: check hash table
    ; For now: assume no conflict
    xor rax, rax            ; No conflict
    
    call Titan_UnlockConflictDetector
    
    add rsp, 24
    pop rsi
    pop rbx
    ret
Titan_DetectConflict ENDP

; ============================================================
; HEARTBEAT SYSTEM (with explicit register setup)
; ============================================================

Titan_InitHeartbeat PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    ; Allocate heartbeat state
    call GetProcessHeap
    mov rbx, rax
    
    mov rcx, rbx
    xor edx, edx
    mov r8d, 1024           ; 1KB for heartbeat state
    call HeapAlloc
    
    test rax, rax
    jz @@failed
    
    ; Zero state
    mov rcx, rax
    xor edx, edx
    mov r8d, 1024
    call RtlZeroMemory
    
    ; Initialize lock
    lea rcx, g_LockHeartbeat
    call InitializeSRWLock
    
    ; Record start time
    call Titan_GetMicroseconds
    mov g_StartTime, rax
    
    xor rax, rax
    jmp @@done
    
@@failed:
    mov rax, -1
    
@@done:
    add rsp, 40
    pop rsi
    pop rbx
    ret
Titan_InitHeartbeat ENDP

Titan_UpdateHeartbeat PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 16
    .allocstack 16
    .endprolog
    
    call Titan_LockHeartbeat
    
    ; Update last heartbeat time
    call Titan_GetMicroseconds
    ; Store in heartbeat state
    
    call Titan_UnlockHeartbeat
    
    xor rax, rax
    
    add rsp, 16
    pop rbx
    ret
Titan_UpdateHeartbeat ENDP

; ============================================================
; MODEL SIZE HANDLING (large immediates loaded into registers)
; ============================================================

Titan_GetModelSizeClass PROC FRAME
    ; rcx = parameter_count
    push rbx
    .pushreg rbx
    sub rsp, 8
    .allocstack 8
    .endprolog
    
    mov rbx, rcx            ; parameter_count
    
    ; Load large constants into registers for comparison
    mov r8, CONST_7B
    cmp rbx, r8
    jl @@tiny
    
    mov r8, CONST_13B
    cmp rbx, r8
    jl @@small
    
    mov r8, CONST_70B
    cmp rbx, r8
    jl @@medium
    
    mov r8, CONST_200B
    cmp rbx, r8
    jl @@large
    
    jmp @@massive
    
@@tiny:
    mov rax, 0
    jmp @@done
    
@@small:
    mov rax, 1
    jmp @@done
    
@@medium:
    mov rax, 2
    jmp @@done
    
@@large:
    mov rax, 3
    jmp @@done
    
@@massive:
    mov rax, 4
    
@@done:
    add rsp, 8
    pop rbx
    ret
Titan_GetModelSizeClass ENDP

; ============================================================
; SUBMIT WITH EXPLICIT REGISTER SETUP
; ============================================================

Titan_SubmitChunk PROC FRAME
    ; rcx = chunk_ptr, rdx = chunk_size, r8 = completion_callback
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    mov rsi, rdx
    mov rdi, r8
    
    ; Allocate completion context
    call GetProcessHeap
    mov rcx, rax
    xor edx, edx
    mov r8d, 64             ; sizeof(CompletionContext)
    call HeapAlloc
    
    test rax, rax
    jz @@failed
    
    ; Setup context
    mov [rax], rbx          ; chunk_ptr
    mov [rax+8], rsi        ; chunk_size
    mov [rax+16], rdi       ; callback
    
    ; Submit to queue (simplified)
    call Titan_LockScheduler
    ; Add to pending queue
    call Titan_UnlockScheduler
    
    xor rax, rax
    jmp @@done
    
@@failed:
    mov rax, -1
    
@@done:
    add rsp, 48
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_SubmitChunk ENDP

; ============================================================
; MAIN INITIALIZATION
; ============================================================

Titan_InitOrchestrator PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    ; Initialize scheduler
    call Titan_InitScheduler
    test rax, rax
    jnz @@failed
    
    ; Initialize conflict detector
    call Titan_InitConflictDetector
    test rax, rax
    jnz @@failed
    
    ; Initialize heartbeat
    call Titan_InitHeartbeat
    test rax, rax
    jnz @@failed
    
    xor rax, rax
    jmp @@done
    
@@failed:
    mov rax, -1
    
@@done:
    add rsp, 40
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_InitOrchestrator ENDP

; ============================================================
; CLEANUP
; ============================================================

Titan_CleanupOrchestrator PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 24
    .allocstack 24
    .endprolog
    
    ; Free ring buffer
    mov rcx, g_RingBufferBase
    test rcx, rcx
    jz @@no_ring
    xor edx, edx
    mov r8d, MEM_RELEASE
    call VirtualFree
    mov g_RingBufferBase, 0
    
@@no_ring:
    ; Free scheduler state
    call GetProcessHeap
    mov rcx, rax
    xor edx, edx
    mov r8, g_OrchestratorState
    test r8, r8
    jz @@no_state
    call HeapFree
    mov g_OrchestratorState, 0
    
@@no_state:
    xor rax, rax
    
    add rsp, 24
    pop rsi
    pop rbx
    ret
Titan_CleanupOrchestrator ENDP

; ============================================================
; ENTRY POINT
; ============================================================

main PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    ; Initialize orchestrator
    call Titan_InitOrchestrator
    test rax, rax
    jnz @@init_failed
    
    ; Test model size classification
    mov rcx, 13000000000    ; 13B parameters (loaded as immediate)
    call Titan_GetModelSizeClass
    
    ; Cleanup
    call Titan_CleanupOrchestrator
    
    xor eax, eax
    jmp @@done
    
@@init_failed:
    mov eax, 1
    
@@done:
    add rsp, 40
    pop rdi
    pop rsi
    pop rbx
    ret
main ENDP

END
