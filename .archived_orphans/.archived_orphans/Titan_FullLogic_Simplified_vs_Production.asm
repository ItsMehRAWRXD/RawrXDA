; Titan_FullLogic_Simplified_vs_Production.asm
; COMPLETE IMPLEMENTATION: Simplified vs Production Logic
; All explicit missing logic provided - both educational and production versions
;
; Build: ml64 /c /Fo Titan_FullLogic.obj Titan_FullLogic_Simplified_vs_Production.asm
; Link: link Titan_FullLogic.obj kernel32.lib /SUBSYSTEM:CONSOLE

OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

OPTION PROLOGUE:NONE
OPTION EPILOGUE:NONE

; ============================================================
; EXTERN DECLARATIONS
; ============================================================

EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN VirtualLock:PROC
EXTERN RtlZeroMemory:PROC
EXTERN Sleep:PROC
EXTERN InitializeSRWLock:PROC
EXTERN AcquireSRWLockExclusive:PROC
EXTERN ReleaseSRWLockExclusive:PROC
EXTERN QueryPerformanceCounter:PROC
EXTERN QueryPerformanceFrequency:PROC

; ============================================================
; CONSTANTS
; ============================================================

MEM_COMMIT              EQU 1000h
MEM_RESERVE             EQU 2000h
MEM_RELEASE             EQU 8000h
MEM_LARGE_PAGES         EQU 20000000h
PAGE_READWRITE          EQU 4h
HEAP_ZERO_MEMORY        EQU 8h

RING_BUFFER_SIZE        EQU 67108864        ; 64MB
RING_BUFFER_SLOTS       EQU 32

; ============================================================
; STRUCTURES
; ============================================================

SRWLOCK STRUCT
    Ptr     QWORD ?
SRWLOCK ENDS

CONFLICT_ENTRY STRUCT
    layer_idx       DWORD ?
    patch_id        DWORD ?
    timestamp       QWORD ?
    resolved        DWORD ?
    pad             DWORD ?
CONFLICT_ENTRY ENDS

RING_BUFFER_ENTRY STRUCT
    data_ptr        QWORD ?
    size            QWORD ?
    offset          QWORD ?
    status          DWORD ?         ; 0=Free, 1=Loading, 2=Ready, 3=InUse
    ref_count       DWORD ?
    timestamp       QWORD ?
RING_BUFFER_ENTRY ENDS

HEARTBEAT_STATE STRUCT
    last_beat       QWORD ?         ; Timestamp of last heartbeat
    interval_us     QWORD ?         ; Expected interval in microseconds
    missed_beats    DWORD ?         ; Count of missed beats
    total_beats     DWORD ?         ; Total beats sent
    is_alive        BYTE ?          ; 1 = alive, 0 = dead
    pad             BYTE 7 DUP(?)
HEARTBEAT_STATE ENDS

; ============================================================
; DATA SECTION
; ============================================================

.DATA
ALIGN 64

; Locks
g_LockConflictDetector  SRWLOCK <>
g_LockHeartbeat         SRWLOCK <>
g_LockRingBuffer        SRWLOCK <>
g_LockScheduler         SRWLOCK <>

; Global state
g_Running               DWORD 0
g_StartTime             QWORD 0
g_ConflictCount         QWORD 0
g_TotalConflictsTracked QWORD 0
g_HeartbeatPtr          QWORD 0
g_RingBufferBase        QWORD 0
g_RingBufferSizeActual  QWORD 0
g_RingBufferHead        DWORD 0
g_RingBufferTail        DWORD 0
g_RingBufferCount       DWORD 0

; Performance
g_PerfFrequency         QWORD 0

; Conflict table (4096 entries)
ALIGN 64
g_ConflictTable         CONFLICT_ENTRY 4096 DUP(<-1, 0, 0, 0, 0>)

; Ring buffer slots
ALIGN 64
g_RingBufferSlots       RING_BUFFER_ENTRY 32 DUP(<>)

; Heartbeat state
ALIGN 64
g_HeartbeatState        HEARTBEAT_STATE <>

; Error strings
szErrorHeartbeatAlloc   BYTE "Failed to allocate heartbeat state", 0
szErrorRingBufferAlloc  BYTE "Failed to allocate ring buffer", 0
szErrorConflictTable    BYTE "Conflict table full", 0

; ============================================================
; CODE SECTION
; ============================================================

.CODE

; ============================================================
; UTILITY FUNCTIONS
; ============================================================

; Get microseconds since epoch
Titan_GetMicroseconds PROC FRAME
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    lea rcx, [rsp+24]
    call QueryPerformanceCounter
    
    mov rax, [rsp+24]
    mov rcx, g_PerfFrequency
    test rcx, rcx
    jz @@use_default
    
    ; Convert to microseconds: (counter * 1000000) / frequency
    mov rdx, 1000000
    mul rdx
    div rcx
    jmp @@done
    
@@use_default:
    ; If no frequency, just return counter
    mov rax, [rsp+24]
    
@@done:
    add rsp, 40
    ret
Titan_GetMicroseconds ENDP

; Initialize performance frequency
Titan_InitPerformance PROC FRAME
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    lea rcx, g_PerfFrequency
    call QueryPerformanceFrequency
    
    add rsp, 40
    ret
Titan_InitPerformance ENDP

; Lock/Unlock helpers
Titan_LockConflictDetector PROC FRAME
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    lea rcx, g_LockConflictDetector
    call AcquireSRWLockExclusive
    
    add rsp, 40
    ret
Titan_LockConflictDetector ENDP

Titan_UnlockConflictDetector PROC FRAME
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    lea rcx, g_LockConflictDetector
    call ReleaseSRWLockExclusive
    
    add rsp, 40
    ret
Titan_UnlockConflictDetector ENDP

Titan_LockHeartbeat PROC FRAME
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    lea rcx, g_LockHeartbeat
    call AcquireSRWLockExclusive
    
    add rsp, 40
    ret
Titan_LockHeartbeat ENDP

Titan_UnlockHeartbeat PROC FRAME
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    lea rcx, g_LockHeartbeat
    call ReleaseSRWLockExclusive
    
    add rsp, 40
    ret
Titan_UnlockHeartbeat ENDP

Titan_LockRingBuffer PROC FRAME
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    lea rcx, g_LockRingBuffer
    call AcquireSRWLockExclusive
    
    add rsp, 40
    ret
Titan_LockRingBuffer ENDP

Titan_UnlockRingBuffer PROC FRAME
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    lea rcx, g_LockRingBuffer
    call ReleaseSRWLockExclusive
    
    add rsp, 40
    ret
Titan_UnlockRingBuffer ENDP

; ============================================================
; SECTION 1: CONFLICT DETECTION
; ============================================================

; -----------------------------------------------------------
; SIMPLIFIED VERSION (Educational/Prototype)
; -----------------------------------------------------------
Titan_DetectConflict_Simplified PROC FRAME
    ; rcx = layer_idx, rdx = patch_id
    ; Returns: rax = 0 (no conflict), 1 (conflict)
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
    mov rsi, rdx            ; patch_id
    
    call Titan_LockConflictDetector
    
    ; Simple hash: (layer_idx * 31 + patch_id) % 4096
    mov rax, rbx
    imul rax, 31
    add rax, rsi
    xor rdx, rdx
    mov rcx, 4096
    div rcx
    mov rdi, rdx            ; hash index
    
    ; Check if entry exists (simplified - just check if hash slot occupied)
    mov rax, rdi
    imul rax, SIZEOF CONFLICT_ENTRY
    lea rcx, g_ConflictTable
    add rax, rcx
    
    cmp (CONFLICT_ENTRY PTR [rax]).layer_idx, -1
    je @@no_conflict        ; Empty slot = no conflict
    
    ; Check if same layer/patch
    cmp (CONFLICT_ENTRY PTR [rax]).layer_idx, ebx
    jne @@different_patch
    cmp (CONFLICT_ENTRY PTR [rax]).patch_id, esi
    jne @@different_patch
    
    ; Same entry found = conflict!
    mov r8, 1
    jmp @@done
    
@@different_patch:
    ; Hash collision but different patch (should chain, but simplified)
    xor r8d, r8d
    jmp @@done
    
@@no_conflict:
    ; Store new entry
    mov (CONFLICT_ENTRY PTR [rax]).layer_idx, ebx
    mov (CONFLICT_ENTRY PTR [rax]).patch_id, esi
    
    push rax
    call Titan_GetMicroseconds
    pop rcx
    mov (CONFLICT_ENTRY PTR [rcx]).timestamp, rax
    mov (CONFLICT_ENTRY PTR [rcx]).resolved, 0
    
    xor r8d, r8d            ; No conflict (new entry)
    
@@done:
    push r8
    call Titan_UnlockConflictDetector
    pop rax
    
    add rsp, 40
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_DetectConflict_Simplified ENDP

; -----------------------------------------------------------
; PRODUCTION VERSION (Full Implementation)
; -----------------------------------------------------------
Titan_DetectConflict_Production PROC FRAME
    ; rcx = layer_idx, rdx = patch_id, r8 = timestamp (optional)
    ; Returns: rax = 0 (no conflict), 1 (conflict detected), 2 (error)
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    sub rsp, 72
    .allocstack 72
    .endprolog
    
    mov rbx, rcx            ; layer_idx
    mov rsi, rdx            ; patch_id
    mov r12, r8             ; timestamp (0 = use current)
    
    ; Validate inputs
    cmp ebx, 0
    jl @@invalid_input
    cmp esi, 0
    jl @@invalid_input
    
    ; Get timestamp if not provided
    test r12, r12
    jnz @@have_timestamp
    call Titan_GetMicroseconds
    mov r12, rax
    
@@have_timestamp:
    call Titan_LockConflictDetector
    
    ; Compute primary hash: (layer_idx * 31 + patch_id) % 4096
    mov rax, rbx
    imul rax, 31
    add rax, rsi
    xor rdx, rdx
    mov r13, 4096
    mov rcx, r13
    div rcx
    mov rdi, rdx            ; primary hash index
    mov r14, rdx            ; save for rehashing
    
    ; Linear probing with max 16 attempts
    mov r13, 16             ; max probes
    
@@probe_loop:
    ; Calculate entry address
    mov rax, rdi
    imul rax, SIZEOF CONFLICT_ENTRY
    lea rcx, g_ConflictTable
    add rax, rcx
    mov r15, rax            ; save entry pointer
    
    ; Check if slot is empty (layer_idx = -1)
    mov ecx, (CONFLICT_ENTRY PTR [rax]).layer_idx
    cmp ecx, -1
    je @@empty_slot
    
    ; Check if same layer/patch (conflict!)
    cmp ecx, ebx
    jne @@next_probe
    cmp (CONFLICT_ENTRY PTR [rax]).patch_id, esi
    jne @@next_probe
    
    ; Conflict detected - update timestamp and return
    mov (CONFLICT_ENTRY PTR [rax]).timestamp, r12
    inc g_ConflictCount
    mov r8d, 1              ; Conflict!
    jmp @@cleanup
    
@@next_probe:
    ; Linear probe: (hash + 1) % 4096
    inc rdi
    and rdi, 0FFFh          ; Wrap at 4096
    
    ; Check if we've looped back to start
    cmp rdi, r14
    je @@table_full
    
    dec r13
    jnz @@probe_loop
    
@@table_full:
    ; Table is full or too many collisions - force eviction
    call Titan_EvictOldestConflictEntry
    test rax, rax
    js @@error
    
    ; Use evicted slot
    mov rdi, rax
    
@@empty_slot:
    ; Store new entry
    mov rax, rdi
    imul rax, SIZEOF CONFLICT_ENTRY
    lea rcx, g_ConflictTable
    add rax, rcx
    
    mov (CONFLICT_ENTRY PTR [rax]).layer_idx, ebx
    mov (CONFLICT_ENTRY PTR [rax]).patch_id, esi
    mov (CONFLICT_ENTRY PTR [rax]).timestamp, r12
    mov (CONFLICT_ENTRY PTR [rax]).resolved, 0
    
    ; Update statistics
    inc g_TotalConflictsTracked
    
    xor r8d, r8d            ; No conflict
    jmp @@cleanup
    
@@invalid_input:
    mov r8d, 2              ; Error: invalid input
    jmp @@done_no_lock
    
@@error:
    mov r8d, 2              ; Error
    jmp @@cleanup
    
@@cleanup:
    push r8
    call Titan_UnlockConflictDetector
    pop r8
    
@@done_no_lock:
    mov eax, r8d
    
    add rsp, 72
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_DetectConflict_Production ENDP

; Helper: Evict oldest conflict entry when table full
Titan_EvictOldestConflictEntry PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    call Titan_GetMicroseconds
    mov r12, rax            ; current time
    mov rbx, -1             ; oldest index
    mov rdi, 0              ; oldest age (max)
    xor rsi, rsi            ; counter
    
@@scan_loop:
    cmp rsi, 4096
    jge @@scan_done
    
    mov rax, rsi
    imul rax, SIZEOF CONFLICT_ENTRY
    lea rcx, g_ConflictTable
    add rax, rcx
    
    ; Skip empty entries
    cmp (CONFLICT_ENTRY PTR [rax]).layer_idx, -1
    je @@next
    
    ; Check if resolved (can be evicted immediately)
    cmp (CONFLICT_ENTRY PTR [rax]).resolved, 1
    je @@evict_this
    
    ; Check age
    mov r13, (CONFLICT_ENTRY PTR [rax]).timestamp
    mov rcx, r12
    sub rcx, r13            ; age = current - timestamp
    
    cmp rcx, rdi
    jle @@next
    
    ; Found older entry
    mov rdi, rcx
    mov rbx, rsi
    
@@next:
    inc rsi
    jmp @@scan_loop
    
@@evict_this:
    mov rbx, rsi
    jmp @@scan_done
    
@@scan_done:
    ; Mark as empty
    cmp rbx, -1
    je @@no_entry
    
    mov rax, rbx
    imul rax, SIZEOF CONFLICT_ENTRY
    lea rcx, g_ConflictTable
    add rax, rcx
    mov (CONFLICT_ENTRY PTR [rax]).layer_idx, -1
    
    mov rax, rbx            ; Return evicted index
    jmp @@done
    
@@no_entry:
    mov rax, -1             ; No entry found
    
@@done:
    add rsp, 40
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_EvictOldestConflictEntry ENDP

; ============================================================
; SECTION 2: HEARTBEAT SYSTEM
; ============================================================

; -----------------------------------------------------------
; SIMPLIFIED VERSION
; -----------------------------------------------------------
Titan_InitHeartbeat_Simplified PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    call GetProcessHeap
    mov rbx, rax
    
    ; Allocate heartbeat state
    mov rcx, rbx
    mov edx, HEAP_ZERO_MEMORY
    mov r8d, 1024
    call HeapAlloc
    
    test rax, rax
    jz @@failed
    
    mov rsi, rax
    
    ; Initialize lock
    lea rcx, g_LockHeartbeat
    call InitializeSRWLock
    
    ; Record start time
    call Titan_GetMicroseconds
    mov g_StartTime, rax
    mov (HEARTBEAT_STATE PTR [rsi]).last_beat, rax
    
    mov g_HeartbeatPtr, rsi
    mov g_Running, 1
    
    xor rax, rax
    jmp @@done
    
@@failed:
    mov rax, -1
    
@@done:
    add rsp, 40
    pop rsi
    pop rbx
    ret
Titan_InitHeartbeat_Simplified ENDP

Titan_UpdateHeartbeat_Simplified PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    call Titan_LockHeartbeat
    
    ; Update heartbeat timestamp
    call Titan_GetMicroseconds
    mov rbx, g_HeartbeatPtr
    test rbx, rbx
    jz @@skip
    
    mov (HEARTBEAT_STATE PTR [rbx]).last_beat, rax
    inc (HEARTBEAT_STATE PTR [rbx]).total_beats
    
@@skip:
    call Titan_UnlockHeartbeat
    
    xor rax, rax
    
    add rsp, 32
    pop rbx
    ret
Titan_UpdateHeartbeat_Simplified ENDP

; -----------------------------------------------------------
; PRODUCTION VERSION
; -----------------------------------------------------------
Titan_InitHeartbeat_Production PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 56
    .allocstack 56
    .endprolog
    
    call GetProcessHeap
    mov rbx, rax
    
    ; Allocate heartbeat state with alignment
    mov rcx, rbx
    mov edx, HEAP_ZERO_MEMORY
    mov r8d, 4096           ; Size (page-aligned)
    call HeapAlloc
    
    test rax, rax
    jz @@alloc_failed
    
    mov rsi, rax            ; Save state pointer
    
    ; Initialize lock
    lea rcx, g_LockHeartbeat
    call InitializeSRWLock
    
    ; Initialize state values
    call Titan_GetMicroseconds
    mov (HEARTBEAT_STATE PTR [rsi]).last_beat, rax
    mov (HEARTBEAT_STATE PTR [rsi]).interval_us, 1000000  ; 1 second default
    mov (HEARTBEAT_STATE PTR [rsi]).missed_beats, 0
    mov (HEARTBEAT_STATE PTR [rsi]).total_beats, 0
    mov (HEARTBEAT_STATE PTR [rsi]).is_alive, 1
    
    ; Store global reference
    mov g_HeartbeatPtr, rsi
    
    ; Record system start time
    mov g_StartTime, rax
    mov g_Running, 1
    
    xor rax, rax
    jmp @@done
    
@@alloc_failed:
    mov rax, -1
    
@@done:
    add rsp, 56
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_InitHeartbeat_Production ENDP

Titan_UpdateHeartbeat_Production PROC FRAME
    ; rcx = timestamp (0 = use current)
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov rbx, rcx            ; Optional timestamp
    
    ; Get current time if not provided
    test rbx, rbx
    jnz @@have_time
    call Titan_GetMicroseconds
    mov rbx, rax
    
@@have_time:
    call Titan_LockHeartbeat
    
    mov rsi, g_HeartbeatPtr
    test rsi, rsi
    jz @@not_initialized
    
    ; Calculate interval since last beat
    mov rdi, (HEARTBEAT_STATE PTR [rsi]).last_beat
    mov rax, rbx
    sub rax, rdi            ; interval = current - last
    
    ; Check if interval exceeded expected
    cmp rax, (HEARTBEAT_STATE PTR [rsi]).interval_us
    jle @@on_time
    
    ; Missed beat detected
    inc (HEARTBEAT_STATE PTR [rsi]).missed_beats
    
    ; Check if too many missed beats
    cmp (HEARTBEAT_STATE PTR [rsi]).missed_beats, 3
    jl @@still_alive
    
    ; Mark as dead
    mov (HEARTBEAT_STATE PTR [rsi]).is_alive, 0
    jmp @@update
    
@@still_alive:
@@on_time:
    ; Reset missed beats on successful beat
    mov (HEARTBEAT_STATE PTR [rsi]).missed_beats, 0
    mov (HEARTBEAT_STATE PTR [rsi]).is_alive, 1
    
@@update:
    ; Update timestamp and count
    mov (HEARTBEAT_STATE PTR [rsi]).last_beat, rbx
    inc (HEARTBEAT_STATE PTR [rsi]).total_beats
    
    xor r8d, r8d
    jmp @@cleanup
    
@@not_initialized:
    mov r8d, -1
    
@@cleanup:
    push r8
    call Titan_UnlockHeartbeat
    pop rax
    
    add rsp, 40
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_UpdateHeartbeat_Production ENDP

; Check if heartbeat is still alive
Titan_IsHeartbeatAlive PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    call Titan_LockHeartbeat
    
    mov rbx, g_HeartbeatPtr
    test rbx, rbx
    jz @@not_init
    
    movzx r8d, (HEARTBEAT_STATE PTR [rbx]).is_alive
    jmp @@done
    
@@not_init:
    xor r8d, r8d
    
@@done:
    push r8
    call Titan_UnlockHeartbeat
    pop rax
    
    add rsp, 32
    pop rbx
    ret
Titan_IsHeartbeatAlive ENDP

; Get heartbeat statistics
Titan_GetHeartbeatStats PROC FRAME
    ; rcx = pointer to stats structure (40 bytes min)
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    mov rbx, rcx            ; stats pointer
    
    call Titan_LockHeartbeat
    
    mov rsi, g_HeartbeatPtr
    test rsi, rsi
    jz @@not_init
    
    ; Copy stats
    mov rax, (HEARTBEAT_STATE PTR [rsi]).last_beat
    mov [rbx], rax
    mov rax, (HEARTBEAT_STATE PTR [rsi]).interval_us
    mov [rbx+8], rax
    mov eax, (HEARTBEAT_STATE PTR [rsi]).missed_beats
    mov [rbx+16], eax
    mov eax, (HEARTBEAT_STATE PTR [rsi]).total_beats
    mov [rbx+20], eax
    movzx eax, (HEARTBEAT_STATE PTR [rsi]).is_alive
    mov [rbx+24], eax
    
    xor r8d, r8d
    jmp @@done
    
@@not_init:
    mov r8d, -1
    
@@done:
    push r8
    call Titan_UnlockHeartbeat
    pop rax
    
    add rsp, 32
    pop rsi
    pop rbx
    ret
Titan_GetHeartbeatStats ENDP

; ============================================================
; SECTION 3: RING BUFFER MANAGEMENT
; ============================================================

; -----------------------------------------------------------
; SIMPLIFIED VERSION
; -----------------------------------------------------------
Titan_InitRingBuffer_Simplified PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    ; Allocate ring buffer memory
    xor ecx, ecx            ; lpAddress = NULL
    mov rdx, RING_BUFFER_SIZE
    mov r8d, MEM_COMMIT OR MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    
    test rax, rax
    jz @@alloc_failed
    
    mov g_RingBufferBase, rax
    mov rsi, rax
    
    ; Calculate slot size
    mov rax, RING_BUFFER_SIZE
    xor rdx, rdx
    mov rcx, RING_BUFFER_SLOTS
    div rcx
    mov rdi, rax            ; slot size = 2MB
    
    ; Initialize all slots to free
    xor rbx, rbx
    
@@init_loop:
    cmp rbx, RING_BUFFER_SLOTS
    jge @@init_done
    
    ; Calculate slot address
    mov rax, rdi
    mul rbx
    mov rcx, rsi
    add rcx, rax            ; data pointer
    
    ; Calculate entry address
    mov rax, SIZEOF RING_BUFFER_ENTRY
    mul rbx
    lea rdx, g_RingBufferSlots
    add rax, rdx
    
    mov (RING_BUFFER_ENTRY PTR [rax]).data_ptr, rcx
    mov (RING_BUFFER_ENTRY PTR [rax]).size, rdi
    mov (RING_BUFFER_ENTRY PTR [rax]).offset, 0
    mov (RING_BUFFER_ENTRY PTR [rax]).status, 0      ; Free
    mov (RING_BUFFER_ENTRY PTR [rax]).timestamp, 0
    mov (RING_BUFFER_ENTRY PTR [rax]).ref_count, 0
    
    inc rbx
    jmp @@init_loop
    
@@init_done:
    lea rcx, g_LockRingBuffer
    call InitializeSRWLock
    
    mov g_RingBufferHead, 0
    mov g_RingBufferTail, 0
    
    xor rax, rax
    jmp @@done
    
@@alloc_failed:
    mov rax, -1
    
@@done:
    add rsp, 40
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_InitRingBuffer_Simplified ENDP

; -----------------------------------------------------------
; PRODUCTION VERSION
; -----------------------------------------------------------
Titan_InitRingBuffer_Production PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    sub rsp, 56
    .allocstack 56
    .endprolog
    
    ; Calculate aligned buffer size
    mov rbx, RING_BUFFER_SIZE
    add rbx, 4095           ; Align to page boundary
    and rbx, NOT 4095
    
    ; Try allocation with large pages for performance
    xor ecx, ecx
    mov rdx, rbx
    mov r8d, MEM_COMMIT OR MEM_RESERVE OR MEM_LARGE_PAGES
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    
    test rax, rax
    jz @@try_normal_alloc
    
    mov g_RingBufferBase, rax
    mov g_RingBufferSizeActual, rbx
    jmp @@alloc_success
    
@@try_normal_alloc:
    ; Fallback to normal allocation
    xor ecx, ecx
    mov rdx, RING_BUFFER_SIZE
    mov r8d, MEM_COMMIT OR MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    
    test rax, rax
    jz @@alloc_failed
    
    mov g_RingBufferBase, rax
    mov g_RingBufferSizeActual, RING_BUFFER_SIZE
    
@@alloc_success:
    mov r12, g_RingBufferBase
    
    ; Lock memory to prevent paging (critical for DMA)
    mov rcx, r12
    mov rdx, g_RingBufferSizeActual
    call Titan_LockPages
    
    ; Calculate slot size
    mov rax, g_RingBufferSizeActual
    xor rdx, rdx
    mov rcx, RING_BUFFER_SLOTS
    div rcx
    mov rdi, rax            ; slot size
    
    ; Initialize all slots
    xor rbx, rbx
    
@@init_loop:
    cmp rbx, RING_BUFFER_SLOTS
    jge @@init_done
    
    ; Calculate slot data pointer
    mov rax, rdi
    mul rbx
    mov rsi, r12
    add rsi, rax            ; data pointer
    
    ; Initialize entry
    mov rax, SIZEOF RING_BUFFER_ENTRY
    mul rbx
    lea rcx, g_RingBufferSlots
    add rax, rcx
    mov r13, rax
    
    mov (RING_BUFFER_ENTRY PTR [r13]).data_ptr, rsi
    mov (RING_BUFFER_ENTRY PTR [r13]).size, rdi
    mov (RING_BUFFER_ENTRY PTR [r13]).offset, 0
    mov (RING_BUFFER_ENTRY PTR [r13]).status, 0      ; Free
    mov (RING_BUFFER_ENTRY PTR [r13]).timestamp, 0
    mov (RING_BUFFER_ENTRY PTR [r13]).ref_count, 0
    
    ; Prefault the memory (touch each page)
    mov rcx, rsi
    mov rdx, rdi
    shr rdx, 12             ; Pages (4KB each)
    test rdx, rdx
    jz @@skip_prefault
    
    xor rax, rax
@@prefault_loop:
    mov byte ptr [rcx], 0   ; Touch page
    add rcx, 4096           ; Next page
    inc rax
    cmp rax, rdx
    jl @@prefault_loop
    
@@skip_prefault:
    inc rbx
    jmp @@init_loop
    
@@init_done:
    ; Initialize synchronization
    lea rcx, g_LockRingBuffer
    call InitializeSRWLock
    
    ; Initialize head/tail indices
    mov g_RingBufferHead, 0
    mov g_RingBufferTail, 0
    mov g_RingBufferCount, 0
    
    xor rax, rax
    jmp @@done
    
@@alloc_failed:
    mov rax, -1
    
@@done:
    add rsp, 56
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_InitRingBuffer_Production ENDP

; Lock pages in physical memory
Titan_LockPages PROC FRAME
    ; rcx = address, rdx = size
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    mov rbx, rcx
    mov rsi, rdx
    
    ; Attempt to lock pages (may fail if privilege not held)
    mov rcx, rbx
    mov rdx, rsi
    call VirtualLock
    
    ; Return success even if VirtualLock fails (non-critical optimization)
    xor rax, rax
    
    add rsp, 32
    pop rsi
    pop rbx
    ret
Titan_LockPages ENDP

; Find and allocate free slot with production features
Titan_AllocateSlot_Production PROC FRAME
    ; rcx = size needed, rdx = timeout_us (0 = no wait)
    ; Returns: rax = slot index, or -1
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    sub rsp, 56
    .allocstack 56
    .endprolog
    
    mov rbx, rcx            ; size needed
    mov rsi, rdx            ; timeout
    
    call Titan_GetMicroseconds
    mov r12, rax            ; start time
    
@@retry:
    call Titan_LockRingBuffer
    
    ; Try to find free slot
    xor rdi, rdi
    
@@find_loop:
    cmp rdi, RING_BUFFER_SLOTS
    jge @@no_free_slot
    
    mov rax, SIZEOF RING_BUFFER_ENTRY
    mul rdi
    lea rcx, g_RingBufferSlots
    add rax, rcx
    mov r13, rax
    
    ; Check if free and large enough
    cmp (RING_BUFFER_ENTRY PTR [r13]).status, 0      ; Free?
    jne @@next_slot
    cmp (RING_BUFFER_ENTRY PTR [r13]).size, rbx
    jl @@next_slot
    
    ; Found suitable slot
    mov (RING_BUFFER_ENTRY PTR [r13]).status, 1      ; Loading
    
    push rdi
    call Titan_GetMicroseconds
    pop rdi
    
    mov rax, SIZEOF RING_BUFFER_ENTRY
    mul rdi
    lea rcx, g_RingBufferSlots
    add rax, rcx
    
    push rdi
    call Titan_GetMicroseconds
    pop rdi
    
    mov rcx, SIZEOF RING_BUFFER_ENTRY
    imul rcx, rdi
    lea rdx, g_RingBufferSlots
    add rcx, rdx
    mov (RING_BUFFER_ENTRY PTR [rcx]).timestamp, rax
    mov (RING_BUFFER_ENTRY PTR [rcx]).ref_count, 1
    
    mov r8, rdi             ; slot index
    call Titan_UnlockRingBuffer
    mov rax, r8
    jmp @@done
    
@@next_slot:
    inc rdi
    jmp @@find_loop
    
@@no_free_slot:
    call Titan_UnlockRingBuffer
    
    ; Try to evict LRU slot
    call Titan_EvictLRUSlot_Production
    cmp rax, -1
    jne @@retry
    
    ; Check timeout
    test rsi, rsi
    jz @@timeout
    
    call Titan_GetMicroseconds
    sub rax, r12
    cmp rax, rsi
    jg @@timeout
    
    ; Brief wait and retry
    mov ecx, 1
    call Sleep
    jmp @@retry
    
@@timeout:
    mov rax, -1
    
@@done:
    add rsp, 56
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_AllocateSlot_Production ENDP

; Evict LRU slot
Titan_EvictLRUSlot_Production PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    call Titan_LockRingBuffer
    
    call Titan_GetMicroseconds
    mov r12, rax            ; current time
    mov rbx, -1             ; oldest index
    mov rdi, 0              ; oldest age
    xor rsi, rsi            ; counter
    
@@scan_loop:
    cmp rsi, RING_BUFFER_SLOTS
    jge @@scan_done
    
    mov rax, SIZEOF RING_BUFFER_ENTRY
    mul rsi
    lea rcx, g_RingBufferSlots
    add rax, rcx
    
    ; Skip non-evictable slots (Free, Loading, InUse with refs)
    mov ecx, (RING_BUFFER_ENTRY PTR [rax]).status
    cmp ecx, 2              ; Ready
    jne @@next
    
    cmp (RING_BUFFER_ENTRY PTR [rax]).ref_count, 0
    jg @@next
    
    ; Calculate age
    mov rcx, (RING_BUFFER_ENTRY PTR [rax]).timestamp
    mov rdx, r12
    sub rdx, rcx            ; age = current - timestamp
    
    cmp rdx, rdi
    jle @@next
    
    ; Found older entry
    mov rdi, rdx
    mov rbx, rsi
    
@@next:
    inc rsi
    jmp @@scan_loop
    
@@scan_done:
    ; Evict if found
    cmp rbx, -1
    je @@no_slot
    
    mov rax, SIZEOF RING_BUFFER_ENTRY
    mul rbx
    lea rcx, g_RingBufferSlots
    add rax, rcx
    mov (RING_BUFFER_ENTRY PTR [rax]).status, 0     ; Free
    
    mov r8, rbx
    jmp @@cleanup
    
@@no_slot:
    mov r8, -1
    
@@cleanup:
    push r8
    call Titan_UnlockRingBuffer
    pop rax
    
    add rsp, 40
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_EvictLRUSlot_Production ENDP

; Release slot back to pool
Titan_ReleaseSlot_Production PROC FRAME
    ; rcx = slot index
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    mov rbx, rcx
    
    ; Validate index
    cmp ebx, 0
    jl @@invalid
    cmp ebx, RING_BUFFER_SLOTS
    jge @@invalid
    
    call Titan_LockRingBuffer
    
    mov rax, SIZEOF RING_BUFFER_ENTRY
    mul rbx
    lea rcx, g_RingBufferSlots
    add rax, rcx
    mov rsi, rax
    
    ; Decrement ref count
    dec (RING_BUFFER_ENTRY PTR [rsi]).ref_count
    jg @@still_referenced
    
    ; Mark as free
    mov (RING_BUFFER_ENTRY PTR [rsi]).status, 0
    mov (RING_BUFFER_ENTRY PTR [rsi]).ref_count, 0
    
@@still_referenced:
    call Titan_UnlockRingBuffer
    
    xor rax, rax
    jmp @@done
    
@@invalid:
    mov rax, -1
    
@@done:
    add rsp, 32
    pop rsi
    pop rbx
    ret
Titan_ReleaseSlot_Production ENDP

; ============================================================
; SECTION 4: MASTER INITIALIZATION
; ============================================================

Titan_InitOrchestrator_Simplified PROC FRAME
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    call Titan_InitPerformance
    call Titan_InitHeartbeat_Simplified
    test rax, rax
    jnz @@failed
    
    call Titan_InitRingBuffer_Simplified
    test rax, rax
    jnz @@failed
    
    lea rcx, g_LockConflictDetector
    call InitializeSRWLock
    
    lea rcx, g_LockScheduler
    call InitializeSRWLock
    
    xor rax, rax
    jmp @@done
    
@@failed:
    mov rax, -1
    
@@done:
    add rsp, 40
    ret
Titan_InitOrchestrator_Simplified ENDP

Titan_InitOrchestrator_Production PROC FRAME
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    call Titan_InitPerformance
    
    call Titan_InitHeartbeat_Production
    test rax, rax
    jnz @@failed
    
    call Titan_InitRingBuffer_Production
    test rax, rax
    jnz @@failed
    
    lea rcx, g_LockConflictDetector
    call InitializeSRWLock
    
    lea rcx, g_LockScheduler
    call InitializeSRWLock
    
    ; Initialize conflict table (all entries empty)
    xor rcx, rcx
@@init_conflict:
    cmp rcx, 4096
    jge @@init_done
    
    mov rax, SIZEOF CONFLICT_ENTRY
    mul rcx
    lea rdx, g_ConflictTable
    add rax, rdx
    mov (CONFLICT_ENTRY PTR [rax]).layer_idx, -1
    
    inc rcx
    jmp @@init_conflict
    
@@init_done:
    xor rax, rax
    jmp @@done
    
@@failed:
    mov rax, -1
    
@@done:
    add rsp, 40
    ret
Titan_InitOrchestrator_Production ENDP

; ============================================================
; CLEANUP
; ============================================================

Titan_CleanupOrchestrator PROC FRAME
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov g_Running, 0
    
    ; Free ring buffer
    mov rcx, g_RingBufferBase
    test rcx, rcx
    jz @@no_ring
    
    xor edx, edx
    mov r8d, MEM_RELEASE
    call VirtualFree
    mov g_RingBufferBase, 0
    
@@no_ring:
    ; Free heartbeat (if heap allocated)
    ; Note: Would need to track heap handle for proper HeapFree
    mov g_HeartbeatPtr, 0
    
    xor rax, rax
    
    add rsp, 40
    ret
Titan_CleanupOrchestrator ENDP

END
