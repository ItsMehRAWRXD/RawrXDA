; Titan_Streaming_Orchestrator_Fixed.asm
; Fixed version with proper unwind info, large immediate handling, and alignment

OPTION CASEMAP:NONE

; ============================================================
; BASIC TYPE DEFINITIONS
; ============================================================
PVOID   TYPEDEF QWORD
HANDLE  TYPEDEF QWORD
SHORT   TYPEDEF WORD

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
            dOffset      DWORD ?
            OffsetHigh   DWORD ?
        ENDS
        Pointer QWORD ?
    ENDS
    hEvent        QWORD ?
OVERLAPPED ENDS

; SOCKADDR_IN structure - define first
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

; Heap constants
HEAP_ZERO_MEMORY    EQU 00000008h

; File mapping constants
PAGE_READONLY       EQU 02h
FILE_MAP_READ       EQU 04h
FILE_MAP_WRITE      EQU 02h

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

; File Mapping (High-Level API)
EXTERN CreateFileMappingA:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC

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

; Global state (cache-line aligned via padding)
align 8
g_OrchestratorState     QWORD 0
g_hCompletionPort       QWORD 0
g_RingBufferBase        QWORD 0
g_RingBufferSize        QWORD 67108864    ; 64MB

; SRW locks
align 8
g_LockScheduler         SRWLOCK <>
g_LockConflictDetector  SRWLOCK <>
g_LockHeartbeat         SRWLOCK <>

; Statistics
align 8
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
; HIGH-LEVEL API LAYER (Option A) - CLI/GUI Consumer Interface
; ============================================================

; Constants for context management
MAX_CONTEXTS        EQU 16
CTX_MAGIC           EQU 054435458h   ; 'TCTX'
CTX_STATE_IDLE      EQU 0
CTX_STATE_LOADING   EQU 1
CTX_STATE_ACTIVE    EQU 2
CTX_STATE_COMPLETE  EQU 3

; Ring header offsets (matches 64MB ring layout)
OFF_WRITE_IDX       EQU 0
OFF_READ_IDX        EQU 8
OFF_SEQ_ID          EQU 16
OFF_FLAGS           EQU 24
FLAG_STREAMING      EQU 1
FLAG_COMPLETE       EQU 2

; Context slots (BSS)
.DATA?
ALIGN 64
g_ContextPtrs       QWORD MAX_CONTEXTS DUP(?)
g_ContextCount      DWORD ?
g_RingHeaderPtr     QWORD ?          ; Points to ring header (write/read idx)

.CODE

; ----------------------------------------------------------------------------
; Titan_CreateContext - Allocate consumer context handle
; Output: RAX = Context handle (NULL if maxed out)
; ----------------------------------------------------------------------------
PUBLIC Titan_CreateContext
Titan_CreateContext PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    ; Find free slot
    xor esi, esi
@@scan:
    cmp esi, MAX_CONTEXTS
    jae @@full
    
    mov rax, g_ContextPtrs[rsi*8]
    test rax, rax
    jz @@found_slot
    
    inc esi
    jmp @@scan
    
@@found_slot:
    ; Allocate 128-byte context struct (cache-line aligned)
    call GetProcessHeap
    mov rcx, rax
    mov edx, HEAP_ZERO_MEMORY
    mov r8d, 128
    call HeapAlloc
    
    test rax, rax
    jz @@full
    mov rbx, rax
    
    ; Initialize context
    mov DWORD PTR [rbx], CTX_MAGIC       ; +0: Magic
    mov DWORD PTR [rbx+4], CTX_STATE_IDLE ; +4: State
    mov QWORD PTR [rbx+8], 0             ; +8: hModelFile
    mov QWORD PTR [rbx+16], 0            ; +16: hModelMap
    mov QWORD PTR [rbx+24], 0            ; +24: pModelBase
    mov DWORD PTR [rbx+32], 0            ; +32: ModelSizeClass
    mov QWORD PTR [rbx+40], 0            ; +40: RingReadIdx (consumer pointer)
    mov QWORD PTR [rbx+48], 0            ; +48: LastHeartbeat
    mov QWORD PTR [rbx+56], 0            ; +56: UserData (callback context)
    
    ; Store in slot
    mov g_ContextPtrs[rsi*8], rbx
    inc g_ContextCount
    
    mov rax, rbx
    jmp @@done
    
@@full:
    xor eax, eax
    
@@done:
    add rsp, 40
    pop rsi
    pop rbx
    ret
Titan_CreateContext ENDP

; ----------------------------------------------------------------------------
; Titan_LoadModel_GGUF - Map GGUF model file and classify size
; RCX = Context handle, RDX = Path (ANSI string)
; Output: RAX = 1 success, 0 fail
; ----------------------------------------------------------------------------
PUBLIC Titan_LoadModel_GGUF
Titan_LoadModel_GGUF PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    sub rsp, 64
    .allocstack 64
    .endprolog
    
    mov rbx, rcx            ; Context handle
    mov r12, rdx            ; Path
    
    ; Validate context magic
    cmp DWORD PTR [rbx], CTX_MAGIC
    jne @@invalid
    
    ; Lock ConflictDetector during model load
    call Titan_LockConflictDetector
    
    ; Open file: CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL)
    mov rcx, r12
    mov edx, GENERIC_READ
    mov r8d, FILE_SHARE_READ
    xor r9, r9
    mov QWORD PTR [rsp+32], OPEN_EXISTING
    mov QWORD PTR [rsp+40], FILE_FLAG_SEQUENTIAL_SCAN
    mov QWORD PTR [rsp+48], 0
    call CreateFileA
    
    cmp rax, -1             ; INVALID_HANDLE_VALUE
    je @@unlock_fail
    mov r13, rax            ; hFile
    mov [rbx+8], rax        ; Store in context
    
    ; Get file size for mapping
    lea rcx, [rsp+56]       ; LARGE_INTEGER for size
    mov rdx, r13
    xchg rcx, rdx
    call GetFileSizeEx
    mov r14, [rsp+56]       ; File size
    
    ; Create file mapping: CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL)
    mov rcx, r13
    xor rdx, rdx
    mov r8d, PAGE_READONLY
    xor r9, r9
    mov QWORD PTR [rsp+32], 0
    mov QWORD PTR [rsp+40], 0
    call CreateFileMappingA
    
    test rax, rax
    jz @@close_file
    mov [rbx+16], rax       ; hModelMap
    mov r13, rax
    
    ; Map view: MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0)
    mov rcx, r13
    mov edx, FILE_MAP_READ
    xor r8, r8
    xor r9, r9
    mov QWORD PTR [rsp+32], 0
    call MapViewOfFile
    
    test rax, rax
    jz @@close_map
    mov [rbx+24], rax       ; pModelBase
    
    ; Classify model size using existing Titan_GetModelSizeClass
    ; For GGUF: parameter count is in metadata; approximate from file size
    ; Rough heuristic: file_size / 2 bytes per param (Q4 average)
    mov rcx, r14
    shr rcx, 1              ; Divide by 2 for rough param count
    call Titan_GetModelSizeClass
    mov [rbx+32], eax       ; ModelSizeClass
    
    ; Transition state to LOADING
    mov DWORD PTR [rbx+4], CTX_STATE_LOADING
    
    call Titan_UnlockConflictDetector
    mov eax, 1
    jmp @@done
    
@@close_map:
    mov rcx, [rbx+16]
    call CloseHandle
    mov QWORD PTR [rbx+16], 0
    
@@close_file:
    mov rcx, [rbx+8]
    call CloseHandle
    mov QWORD PTR [rbx+8], 0
    
@@unlock_fail:
    call Titan_UnlockConflictDetector
    
@@invalid:
    xor eax, eax
    
@@done:
    add rsp, 64
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
Titan_LoadModel_GGUF ENDP

; ----------------------------------------------------------------------------
; Titan_BeginStreamingInference - Start inference, activate ring producer
; RCX = Context, RDX = Prompt (char*), R8 = PromptLen
; Output: RAX = 1 success, 0 fail
; ----------------------------------------------------------------------------
PUBLIC Titan_BeginStreamingInference
Titan_BeginStreamingInference PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov rbx, rcx            ; Context
    mov r12, rdx            ; Prompt
    mov r13, r8             ; PromptLen
    
    ; Validate context
    cmp DWORD PTR [rbx], CTX_MAGIC
    jne @@fail
    cmp DWORD PTR [rbx+4], CTX_STATE_LOADING
    jb @@fail               ; Must have model loaded
    
    ; Lock Scheduler for ring coordination
    call Titan_LockScheduler
    
    ; Reset consumer read pointer for fresh inference
    mov QWORD PTR [rbx+40], 0
    
    ; Initialize ring header if first use (set g_RingHeaderPtr)
    mov rax, g_RingBufferBase
    mov g_RingHeaderPtr, rax
    
    ; Reset ring indices (producer starts at 0)
    mov rcx, g_RingHeaderPtr
    mov QWORD PTR [rcx+OFF_WRITE_IDX], 0
    mov QWORD PTR [rcx+OFF_READ_IDX], 0
    mov DWORD PTR [rcx+OFF_FLAGS], FLAG_STREAMING
    
    ; Submit prompt as initial chunk via existing primitive
    mov rcx, r12            ; chunk_ptr = prompt
    mov rdx, r13            ; chunk_size = prompt_len
    xor r8, r8              ; No callback
    call Titan_SubmitChunk
    
    ; Transition to ACTIVE
    mov DWORD PTR [rbx+4], CTX_STATE_ACTIVE
    
    call Titan_UnlockScheduler
    mov eax, 1
    jmp @@done
    
@@fail:
    xor eax, eax
    
@@done:
    add rsp, 40
    pop r13
    pop r12
    pop rbx
    ret
Titan_BeginStreamingInference ENDP

; ----------------------------------------------------------------------------
; Titan_ConsumeToken - Read tokens from ring buffer (consumer side)
; RCX = Context, RDX = DestBuffer, R8 = MaxBytes
; Output: RAX = Bytes read (0 if empty or complete)
; ----------------------------------------------------------------------------
PUBLIC Titan_ConsumeToken
Titan_ConsumeToken PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov rbx, rcx            ; Context
    mov r12, rdx            ; DestBuffer
    mov r13, r8             ; MaxBytes
    
    ; Validate context is active
    cmp DWORD PTR [rbx], CTX_MAGIC
    jne @@zero
    cmp DWORD PTR [rbx+4], CTX_STATE_ACTIVE
    jne @@check_complete
    
    ; Lock Scheduler for atomic read
    call Titan_LockScheduler
    
    ; Get write index (producer position)
    mov rax, g_RingHeaderPtr
    mov r14, [rax+OFF_WRITE_IDX]    ; Producer write position
    mov r15, [rbx+40]               ; Our read position
    
    ; Calculate available bytes
    mov rax, r14
    sub rax, r15                    ; Available = write - read
    
    test rax, rax
    jz @@unlock_empty               ; No data available
    
    ; Cap to MaxBytes
    cmp rax, r13
    cmova rax, r13
    mov r13, rax                    ; r13 = bytes to copy
    
    ; Calculate source address: RingBase + HeaderSize + (ReadIdx mod RingDataSize)
    mov rsi, g_RingBufferBase
    add rsi, 4096                   ; Skip 4KB header
    mov rax, r15
    mov rcx, g_RingBufferSize
    sub rcx, 4096                   ; Data area size
    xor rdx, rdx
    div rcx                         ; rdx = readIdx mod dataSize
    add rsi, rdx                    ; Source address
    
    ; Copy to user buffer
    mov rdi, r12
    mov rcx, r13
    rep movsb
    
    ; Update consumer read pointer
    add r15, r13
    mov [rbx+40], r15
    
    ; Also update ring header read index (for producer backpressure)
    mov rax, g_RingHeaderPtr
    mov [rax+OFF_READ_IDX], r15
    
    call Titan_UnlockScheduler
    mov rax, r13                    ; Return bytes read
    jmp @@done
    
@@unlock_empty:
    call Titan_UnlockScheduler
    
@@check_complete:
    ; Check if inference completed
    mov rax, g_RingHeaderPtr
    test rax, rax
    jz @@zero
    mov eax, [rax+OFF_FLAGS]
    test eax, FLAG_COMPLETE
    jnz @@signal_complete
    
@@zero:
    xor eax, eax
    jmp @@done
    
@@signal_complete:
    ; Mark context as complete
    mov DWORD PTR [rbx+4], CTX_STATE_COMPLETE
    xor eax, eax                    ; Return 0 to signal EOF
    
@@done:
    add rsp, 40
    pop rdi
    pop rsi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
Titan_ConsumeToken ENDP

; ----------------------------------------------------------------------------
; Titan_Shutdown - Full cleanup (contexts + core orchestrator)
; ----------------------------------------------------------------------------
PUBLIC Titan_Shutdown
Titan_Shutdown PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    ; Free all contexts
    xor ebx, ebx
@@free_loop:
    cmp ebx, MAX_CONTEXTS
    jae @@cleanup_core
    
    mov rsi, g_ContextPtrs[rbx*8]
    test rsi, rsi
    jz @@next_slot
    
    ; Unmap model if present
    mov rcx, [rsi+24]               ; pModelBase
    test rcx, rcx
    jz @@no_unmap
    call UnmapViewOfFile
    
@@no_unmap:
    mov rcx, [rsi+16]               ; hModelMap
    test rcx, rcx
    jz @@no_close_map
    call CloseHandle
    
@@no_close_map:
    mov rcx, [rsi+8]                ; hModelFile
    test rcx, rcx
    jz @@no_close_file
    call CloseHandle
    
@@no_close_file:
    ; Free context struct
    call GetProcessHeap
    mov rcx, rax
    xor edx, edx
    mov r8, rsi
    call HeapFree
    
    mov g_ContextPtrs[rbx*8], 0
    
@@next_slot:
    inc ebx
    jmp @@free_loop
    
@@cleanup_core:
    mov g_ContextCount, 0
    
    ; Call original orchestrator cleanup
    call Titan_CleanupOrchestrator
    
    add rsp, 40
    pop rsi
    pop rbx
    ret
Titan_Shutdown ENDP

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
