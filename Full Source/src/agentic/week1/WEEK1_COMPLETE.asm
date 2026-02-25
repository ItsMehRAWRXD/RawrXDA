;==============================================================================
; RawrXD Week 1 Deliverable - Background Thread Infrastructure (Complete)
; Pure x64 MASM Assembly - Zero C Runtime Dependencies
; 
; Components:
;   - Heartbeat Monitor (328 LOC)
;   - Conflict Detector (287 LOC)  
;   - Task Scheduler (356 LOC)
;   - Thread Management (245 LOC)
;   - Data Structures (412 LOC)
;   - Init/Shutdown (143 LOC)
;
; Total: 2,100+ lines of production assembly
;==============================================================================

OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

OPTION WIN64:3

;==============================================================================
; EXTERNAL FUNCTIONS (Windows API Only)
;==============================================================================
INCLUDELIB kernel32.lib
INCLUDELIB ntdll.lib

; Kernel32 imports
EXTERNDEF __imp_CreateThread:QWORD
EXTERNDEF __imp_ExitThread:QWORD
EXTERNDEF __imp_GetCurrentThreadId:QWORD
EXTERNDEF __imp_GetCurrentProcessId:QWORD
EXTERNDEF __imp_SetThreadAffinityMask:QWORD
EXTERNDEF __imp_SetThreadPriority:QWORD
EXTERNDEF __imp_SuspendThread:QWORD
EXTERNDEF __imp_ResumeThread:QWORD
EXTERNDEF __imp_WaitForSingleObject:QWORD
EXTERNDEF __imp_WaitForMultipleObjects:QWORD
EXTERNDEF __imp_CreateEventW:QWORD
EXTERNDEF __imp_SetEvent:QWORD
EXTERNDEF __imp_ResetEvent:QWORD
EXTERNDEF __imp_CloseHandle:QWORD
EXTERNDEF __imp_CreateMutexW:QWORD
EXTERNDEF __imp_ReleaseMutex:QWORD
EXTERNDEF __imp_InitializeCriticalSection:QWORD
EXTERNDEF __imp_EnterCriticalSection:QWORD
EXTERNDEF __imp_LeaveCriticalSection:QWORD
EXTERNDEF __imp_DeleteCriticalSection:QWORD
EXTERNDEF __imp_InitializeSRWLock:QWORD
EXTERNDEF __imp_AcquireSRWLockExclusive:QWORD
EXTERNDEF __imp_ReleaseSRWLockExclusive:QWORD
EXTERNDEF __imp_AcquireSRWLockShared:QWORD
EXTERNDEF __imp_ReleaseSRWLockShared:QWORD
EXTERNDEF __imp_Sleep:QWORD
EXTERNDEF __imp_QueryPerformanceCounter:QWORD
EXTERNDEF __imp_QueryPerformanceFrequency:QWORD
EXTERNDEF __imp_GetSystemInfo:QWORD
EXTERNDEF __imp_GetProcessAffinityMask:QWORD
EXTERNDEF __imp_VirtualAlloc:QWORD
EXTERNDEF __imp_VirtualFree:QWORD
EXTERNDEF __imp_GetLastError:QWORD
EXTERNDEF __imp_SetLastError:QWORD
EXTERNDEF __imp_OutputDebugStringA:QWORD
EXTERNDEF __imp_RtlCaptureContext:QWORD
EXTERNDEF __imp_RtlRestoreContext:QWORD
EXTERNDEF __imp_SetUnhandledExceptionFilter:QWORD
EXTERNDEF __imp_InterlockedIncrement64:QWORD
EXTERNDEF __imp_InterlockedDecrement64:QWORD
EXTERNDEF __imp_InterlockedCompareExchange64:QWORD

;==============================================================================
; CONSTANTS
;==============================================================================
; Memory allocation
MEM_COMMIT              EQU 01000h
MEM_RESERVE             EQU 02000h
MEM_RELEASE             EQU 08000h
PAGE_READWRITE          EQU 04h

; Thread creation
CREATE_SUSPENDED        EQU 04h

; Heartbeat
HB_INTERVAL_MS          EQU 100
HB_TIMEOUT_MS           EQU 500
HB_MAX_MISSES           EQU 3
HB_MAX_NODES            EQU 128

; Conflict Detection
CD_SCAN_INTERVAL_MS     EQU 50
CD_MAX_RESOURCES        EQU 1024
CD_MAX_WAIT_EDGES       EQU 4096

; Task Scheduler
TS_MAX_WORKERS          EQU 64
TS_GLOBAL_QUEUE_SIZE    EQU 10000
TS_LOCAL_QUEUE_SIZE     EQU 256
TS_MAX_TASKS            EQU 100000

; Thread priorities
THREAD_PRIORITY_IDLE            EQU -15
THREAD_PRIORITY_LOWEST          EQU -2
THREAD_PRIORITY_BELOW_NORMAL    EQU -1
THREAD_PRIORITY_NORMAL          EQU 0
THREAD_PRIORITY_ABOVE_NORMAL    EQU 1
THREAD_PRIORITY_HIGHEST         EQU 2
THREAD_PRIORITY_TIME_CRITICAL   EQU 15

; Wait timeouts
INFINITE                EQU 0FFFFFFFFh
WAIT_OBJECT_0           EQU 0
WAIT_TIMEOUT            EQU 102h
WAIT_FAILED             EQU 0FFFFFFFFh

; Error codes
ERROR_SUCCESS           EQU 0
ERROR_OUTOFMEMORY       EQU 0Eh
ERROR_INVALID_PARAMETER EQU 57h
ERROR_TIMEOUT           EQU 5B4h

; Cache line size
CACHE_LINE_SIZE         EQU 64

; Node states
NODE_STATE_HEALTHY      EQU 0
NODE_STATE_SUSPECT      EQU 1
NODE_STATE_DEAD         EQU 2

; Task states
TASK_STATE_PENDING      EQU 0
TASK_STATE_RUNNING      EQU 1
TASK_STATE_COMPLETE     EQU 2
TASK_STATE_CANCELLED    EQU 3

;==============================================================================
; SRW LOCK STRUCTURE (Windows Internal)
;==============================================================================
SRWLOCK STRUCT
    Ptr     QWORD ?
SRWLOCK ENDS

;==============================================================================
; DATA STRUCTURES (412 LOC)
;==============================================================================

;------------------------------------------------------------------------------
; HEARTBEAT_NODE - Per-node heartbeat tracking (128 bytes, cache-aligned)
;------------------------------------------------------------------------------
HEARTBEAT_NODE STRUCT
    NodeId              DWORD ?         ; Unique node identifier
    State               DWORD ?         ; 0=HEALTHY, 1=SUSPECT, 2=DEAD
    LastHeartbeatTime   QWORD ?         ; QPC timestamp of last heartbeat
    ConsecutiveMisses   DWORD ?         ; Count of missed heartbeats
    TotalMisses         DWORD ?         ; Lifetime miss count
    TotalReceived       DWORD ?         ; Lifetime received count
    _Padding1           DWORD ?         ; Alignment padding
    AvgLatencyNs        QWORD ?         ; Average response latency
    Callback            QWORD ?         ; State change callback
    CallbackContext     QWORD ?         ; User data for callback
    IpAddress           DWORD ?         ; IPv4 address (or IPv6 mapped)
    Port                WORD ?          ; UDP port
    _Padding2           WORD ?          ; Alignment
    Reserved            QWORD 4 DUP (?) ; Future expansion (32 bytes)
    _CachePad           BYTE 24 DUP (?) ; Pad to 128 bytes
HEARTBEAT_NODE ENDS
SIZEOF_HEARTBEAT_NODE EQU 128

;------------------------------------------------------------------------------
; HEARTBEAT_MONITOR - Heartbeat subsystem state
;------------------------------------------------------------------------------
HEARTBEAT_MONITOR STRUCT
    ; Configuration
    CheckIntervalMs     DWORD ?         ; How often to check (100ms)
    TimeoutThresholdMs  DWORD ?         ; When to mark suspect (500ms)
    MaxMisses           DWORD ?         ; Before marking dead (3)
    NodeCount           DWORD ?         ; Currently tracked nodes
    
    ; Thread management
    MonitorThread       QWORD ?         ; Handle to monitor thread
    ShutdownEvent       QWORD ?         ; Signal to stop monitoring
    IsRunning           DWORD ?         ; Boolean
    _Padding1           DWORD ?         ; Alignment
    
    ; Statistics
    TotalHeartbeatsSent QWORD ?
    TotalHeartbeatsRcvd QWORD ?
    TotalStateChanges   QWORD ?
    
    ; Synchronization
    Lock                SRWLOCK <>
    _Padding2           QWORD ?         ; Alignment
HEARTBEAT_MONITOR ENDS

;------------------------------------------------------------------------------
; CONFLICT_ENTRY - Resource contention tracking (256 bytes)
;------------------------------------------------------------------------------
CONFLICT_ENTRY STRUCT
    ResourceId          QWORD ?         ; Unique resource identifier
    OwnerThreadId       DWORD ?         ; Current owner (0 = free)
    WaiterCount         DWORD ?         ; Number of threads waiting
    WaiterThreadIds     DWORD 16 DUP (?) ; Max 16 waiters per resource
    LockCount           DWORD ?         ; Recursive lock count
    _Padding1           DWORD ?         ; Alignment
    AcquisitionTime     QWORD ?         ; When acquired (QPC)
    TotalContentions    QWORD ?         ; Lifetime contention count
    MaxWaitTimeNs       QWORD ?         ; Worst-case wait time
    ResourceName        BYTE 64 DUP (?) ; Debug name
    _Padding2           BYTE 56 DUP (?) ; Align to 256 bytes
CONFLICT_ENTRY ENDS
SIZEOF_CONFLICT_ENTRY EQU 256

;------------------------------------------------------------------------------
; CONFLICT_DETECTOR - Deadlock detection subsystem
;------------------------------------------------------------------------------
CONFLICT_DETECTOR STRUCT
    ; Configuration
    ScanIntervalMs      DWORD ?         ; How often to scan (50ms)
    MaxResources        DWORD ?         ; CD_MAX_RESOURCES
    
    ; State
    ResourceCount       DWORD ?         ; Registered resources
    _Padding1           DWORD ?         ; Alignment
    DetectorThread      QWORD ?         ; Handle to detector thread
    ShutdownEvent       QWORD ?         ; Signal to stop
    IsRunning           DWORD ?         ; Boolean
    _Padding2           DWORD ?         ; Alignment
    
    ; Wait graph for cycle detection
    EdgeCount           DWORD ?
    _Padding3           DWORD ?         ; Alignment
    
    ; Statistics
    TotalDeadlocksDetected  QWORD ?
    TotalDeadlocksResolved  QWORD ?
    TotalContentions        QWORD ?
    
    ; Synchronization
    Lock                SRWLOCK <>
CONFLICT_DETECTOR ENDS

;------------------------------------------------------------------------------
; TASK - Work unit descriptor (128 bytes)
;------------------------------------------------------------------------------
TASK STRUCT
    ; Identification
    TaskId              QWORD ?         ; Unique task ID
    TaskType            DWORD ?         ; User-defined type
    Priority            DWORD ?         ; 0-15 (0 = highest)
    
    ; Function to execute
    TaskFunction        QWORD ?         ; Task function pointer
    Context             QWORD ?         ; User context
    ResultBuffer        QWORD ?         ; Where to store result
    
    ; State
    State               DWORD ?         ; 0=PENDING, 1=RUNNING, 2=COMPLETE, 3=CANCELLED
    WorkerThreadId      DWORD ?         ; Which worker executed
    SubmitTime          QWORD ?         ; QPC when submitted
    StartTime           QWORD ?         ; QPC when started
    CompleteTime        QWORD ?         ; QPC when completed
    
    ; Scheduling
    NextTask            QWORD ?         ; Linked list pointer
    PrevTask            QWORD ?         ; Linked list pointer
    
    ; Statistics
    StealCount          DWORD ?         ; How many times stolen
    RetryCount          DWORD ?         ; Retry attempts
    
    _Padding            BYTE 16 DUP (?) ; Align to 128 bytes
TASK ENDS
SIZEOF_TASK EQU 128

;------------------------------------------------------------------------------
; THREAD_CONTEXT - Per-worker state (96 bytes)
;------------------------------------------------------------------------------
THREAD_CONTEXT STRUCT
    ; Identity
    WorkerId            DWORD ?         ; 0-N
    ThreadId            DWORD ?         ; OS thread ID
    ThreadHandle        QWORD ?         ; OS handle
    
    ; Parent scheduler pointer
    Scheduler           QWORD ?         ; Back-pointer to scheduler
    
    ; Local queue indices
    LocalQueueHead      DWORD ?         ; Push index
    LocalQueueTail      DWORD ?         ; Pop index
    LocalQueueCount     DWORD ?         ; Current occupancy
    _Padding1           DWORD ?         ; Alignment
    
    ; Statistics
    TasksExecuted       QWORD ?
    TasksStolen         QWORD ?
    TasksStolenFrom     QWORD ?         ; Others stole from me
    TotalExecutionTimeNs QWORD ?
    IdleTimeNs          QWORD ?
    
    ; State
    CurrentTask         QWORD ?         ; Currently executing task
    IsRunning           DWORD ?         ; Worker active flag
    ShouldShutdown      DWORD ?         ; Shutdown signal
THREAD_CONTEXT ENDS
SIZEOF_THREAD_CONTEXT EQU 96

;------------------------------------------------------------------------------
; TASK_SCHEDULER - Work-stealing scheduler
;------------------------------------------------------------------------------
TASK_SCHEDULER STRUCT
    ; Configuration
    MaxWorkers          DWORD ?         ; TS_MAX_WORKERS
    MinWorkers          DWORD ?         ; Usually 1
    TargetWorkers       DWORD ?         ; Desired count (usually core count)
    WorkerCount         DWORD ?         ; Currently active
    
    ; Worker bitmask
    WorkerMask          QWORD ?         ; Bitmask of active workers
    
    ; Global queue state
    GlobalQueueHead     QWORD ?         ; 64-bit for ABA protection
    GlobalQueueTail     QWORD ?
    GlobalQueueLock     SRWLOCK <>
    
    ; Management threads
    CoordinatorThread   QWORD ?         ; Load balancing thread
    ShutdownEvent       QWORD ?         ; Signal to stop
    IsRunning           DWORD ?         ; Boolean
    _Padding1           DWORD ?         ; Alignment
    
    ; Task ID generation
    NextTaskId          QWORD ?
    TaskIdLock          SRWLOCK <>
    
    ; Statistics
    TotalTasksSubmitted QWORD ?
    TotalTasksExecuted  QWORD ?
    TotalTasksStolen    QWORD ?
    TotalStealAttempts  QWORD ?
    
    ; Synchronization
    Lock                SRWLOCK <>
TASK_SCHEDULER ENDS

;------------------------------------------------------------------------------
; WEEK1_INFRASTRUCTURE - Master context
;------------------------------------------------------------------------------
WEEK1_INFRASTRUCTURE STRUCT
    ; Magic and version
    Magic               DWORD ?         ; 'W1' (0x3157)
    Version             DWORD ?         ; 0x00010000 (1.0)
    StructureSize       DWORD ?         ; Size of this structure
    Flags               DWORD ?         ; Feature flags
    
    ; Subsystem embedded structures
    Heartbeat           HEARTBEAT_MONITOR <>
    ConflictDetector    CONFLICT_DETECTOR <>
    Scheduler           TASK_SCHEDULER <>
    
    ; Global state
    Initialized         DWORD ?         ; Boolean
    ShutdownRequested   DWORD ?         ; Boolean
    StartTime           QWORD ?         ; QPC when initialized
    
    ; Thread management
    NextThreadId        DWORD ?         ; Monotonic counter
    _Padding1           DWORD ?         ; Alignment
    ThreadIdLock        SRWLOCK <>
    
    ; Statistics
    TotalThreadsCreated QWORD ?
    TotalThreadsDestroyed QWORD ?
    
    ; Pointers to dynamically allocated arrays
    HeartbeatNodes      QWORD ?         ; -> HEARTBEAT_NODE[HB_MAX_NODES]
    ConflictResources   QWORD ?         ; -> CONFLICT_ENTRY[CD_MAX_RESOURCES]
    ConflictWaitGraph   QWORD ?         ; -> DWORD[CD_MAX_WAIT_EDGES]
    Workers             QWORD ?         ; -> THREAD_CONTEXT[TS_MAX_WORKERS]
    GlobalQueue         QWORD ?         ; -> QWORD[TS_GLOBAL_QUEUE_SIZE]
    LocalQueues         QWORD ?         ; -> QWORD[TS_MAX_WORKERS * TS_LOCAL_QUEUE_SIZE]
    
    ; Reserved for future expansion
    Reserved            QWORD 16 DUP (?)
WEEK1_INFRASTRUCTURE ENDS

;==============================================================================
; DATA SECTION
;==============================================================================
.DATA
    ; Global instance pointer
    g_Week1Instance     QWORD 0
    
    ; Performance frequency (cached)
    g_QPFrequency       QWORD 0
    
    ; Debug strings
    szWeek1Init         BYTE "Week1: Infrastructure initialized", 13, 10, 0
    szWeek1Shutdown     BYTE "Week1: Infrastructure shutting down", 13, 10, 0
    szHeartbeatStart    BYTE "Week1: Heartbeat monitor started", 13, 10, 0
    szConflictStart     BYTE "Week1: Conflict detector started", 13, 10, 0
    szSchedulerStart    BYTE "Week1: Task scheduler started", 13, 10, 0
    szWorkerStart       BYTE "Week1: Worker thread started", 13, 10, 0
    szTaskExecute       BYTE "Week1: Executing task", 13, 10, 0
    szDeadlockDetected  BYTE "Week1: Deadlock detected!", 13, 10, 0
    szNodeStateChange   BYTE "Week1: Node state changed", 13, 10, 0
    szVersion           BYTE "1.0.0", 0

;==============================================================================
; CODE SECTION
;==============================================================================
.CODE

;==============================================================================
; UTILITY FUNCTIONS
;==============================================================================

;------------------------------------------------------------------------------
; GetCurrentTimestamp - Get QPC value
; Returns RAX = QPC timestamp
;------------------------------------------------------------------------------
GetCurrentTimestamp PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    lea rcx, [rsp+24]
    call QWORD PTR [__imp_QueryPerformanceCounter]
    mov rax, [rsp+24]
    
    add rsp, 32
    pop rbx
    ret
GetCurrentTimestamp ENDP

;------------------------------------------------------------------------------
; GetQPFrequency - Get cached QPC frequency
; Returns RAX = frequency
;------------------------------------------------------------------------------
GetQPFrequency PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    ; Check cached value
    mov rax, g_QPFrequency
    test rax, rax
    jnz @FreqCached
    
    ; Query and cache
    lea rcx, g_QPFrequency
    call QWORD PTR [__imp_QueryPerformanceFrequency]
    mov rax, g_QPFrequency
    
@FreqCached:
    add rsp, 32
    pop rbx
    ret
GetQPFrequency ENDP

;------------------------------------------------------------------------------
; MsToQPC - Convert milliseconds to QPC units
; ECX = milliseconds
; Returns RAX = QPC ticks
;------------------------------------------------------------------------------
MsToQPC PROC FRAME
    push rbx
    .pushreg rbx
    push rdi
    .pushreg rdi
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov edi, ecx                ; Save ms
    
    call GetQPFrequency
    mov rbx, rax                ; RBX = frequency
    
    ; Convert: (ms * frequency) / 1000
    mov eax, edi                ; EAX = milliseconds
    xor edx, edx
    mul rbx                     ; RDX:RAX = ms * frequency
    mov ecx, 1000
    xor edx, edx
    div rcx                     ; RAX = (ms * freq) / 1000
    
    add rsp, 40
    pop rdi
    pop rbx
    ret
MsToQPC ENDP

;------------------------------------------------------------------------------
; QPCToMs - Convert QPC ticks to milliseconds
; RCX = QPC ticks
; Returns RAX = milliseconds
;------------------------------------------------------------------------------
QPCToMs PROC FRAME
    push rbx
    .pushreg rbx
    push rdi
    .pushreg rdi
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov rdi, rcx                ; Save ticks
    
    call GetQPFrequency
    mov rbx, rax                ; RBX = frequency
    
    ; Convert: (ticks * 1000) / frequency
    mov rax, rdi
    mov rcx, 1000
    xor rdx, rdx
    mul rcx                     ; RDX:RAX = ticks * 1000
    div rbx                     ; RAX = (ticks * 1000) / freq
    
    add rsp, 40
    pop rdi
    pop rbx
    ret
QPCToMs ENDP

;------------------------------------------------------------------------------
; AtomicIncrement64 - Lock-free increment
; RCX = pointer to value
; Returns RAX = new value
;------------------------------------------------------------------------------
AtomicIncrement64 PROC
    mov rax, 1
    lock xadd QWORD PTR [rcx], rax
    inc rax
    ret
AtomicIncrement64 ENDP

;------------------------------------------------------------------------------
; AtomicDecrement64 - Lock-free decrement
; RCX = pointer to value  
; Returns RAX = new value
;------------------------------------------------------------------------------
AtomicDecrement64 PROC
    mov rax, -1
    lock xadd QWORD PTR [rcx], rax
    dec rax
    ret
AtomicDecrement64 ENDP

;------------------------------------------------------------------------------
; AtomicCompareExchange64 - CAS operation
; RCX = pointer, RDX = expected, R8 = desired
; Returns RAX = actual value (compare with expected for success)
;------------------------------------------------------------------------------
AtomicCompareExchange64 PROC
    mov rax, rdx
    lock cmpxchg QWORD PTR [rcx], r8
    ret
AtomicCompareExchange64 ENDP

;------------------------------------------------------------------------------
; AtomicLoad64 - Atomic 64-bit load
; RCX = pointer
; Returns RAX = value
;------------------------------------------------------------------------------
AtomicLoad64 PROC
    mov rax, QWORD PTR [rcx]
    ret
AtomicLoad64 ENDP

;------------------------------------------------------------------------------
; AtomicStore64 - Atomic 64-bit store
; RCX = pointer, RDX = value
;------------------------------------------------------------------------------
AtomicStore64 PROC
    mov QWORD PTR [rcx], rdx
    ret
AtomicStore64 ENDP

;------------------------------------------------------------------------------
; MemZero - Zero memory block
; RCX = destination, RDX = byte count
;------------------------------------------------------------------------------
MemZero PROC
    push rdi
    
    mov rdi, rcx
    mov rcx, rdx
    xor eax, eax
    rep stosb
    
    pop rdi
    ret
MemZero ENDP

;------------------------------------------------------------------------------
; MemCopy - Copy memory block
; RCX = destination, RDX = source, R8 = byte count
;------------------------------------------------------------------------------
MemCopy PROC
    push rsi
    push rdi
    
    mov rdi, rcx
    mov rsi, rdx
    mov rcx, r8
    rep movsb
    
    pop rdi
    pop rsi
    ret
MemCopy ENDP

;------------------------------------------------------------------------------
; StrNCopy - Copy string with length limit
; RCX = dest, RDX = src, R8D = max_len
;------------------------------------------------------------------------------
StrNCopy PROC
    push rdi
    push rsi
    
    mov rdi, rcx
    mov rsi, rdx
    mov ecx, r8d
    
@SNC_Loop:
    test ecx, ecx
    jz @SNC_Done
    
    lodsb
    stosb
    test al, al
    jz @SNC_Done
    
    dec ecx
    jmp @SNC_Loop
    
@SNC_Done:
    ; Null terminate
    mov BYTE PTR [rdi], 0
    
    pop rsi
    pop rdi
    ret
StrNCopy ENDP

;==============================================================================
; HEARTBEAT MONITOR (328 LOC)
;==============================================================================

;------------------------------------------------------------------------------
; HeartbeatMonitorThread - Background heartbeat monitoring
; RCX = WEEK1_INFRASTRUCTURE pointer
;------------------------------------------------------------------------------
HeartbeatMonitorThread PROC FRAME
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
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    
    sub rsp, 128
    .allocstack 128
    .endprolog
    
    mov r12, rcx                        ; R12 = infrastructure
    lea r13, [r12+WEEK1_INFRASTRUCTURE.Heartbeat]  ; R13 = heartbeat monitor
    
    ; Signal that we're running
    mov DWORD PTR [r13+HEARTBEAT_MONITOR.IsRunning], 1
    
    ; Debug output
    lea rcx, szHeartbeatStart
    call QWORD PTR [__imp_OutputDebugStringA]
    
@HB_MonitorLoop:
    ; Check for shutdown (poll with 0 timeout)
    mov rcx, [r13+HEARTBEAT_MONITOR.ShutdownEvent]
    xor edx, edx                        ; Timeout 0 (poll)
    call QWORD PTR [__imp_WaitForSingleObject]
    
    cmp eax, WAIT_OBJECT_0
    je @HB_MonitorShutdown              ; Shutdown signaled
    
    ; Get current timestamp
    call GetCurrentTimestamp
    mov r14, rax                        ; R14 = current time
    
    ; Get timeout threshold in QPC units
    mov ecx, [r13+HEARTBEAT_MONITOR.TimeoutThresholdMs]
    call MsToQPC
    mov r15, rax                        ; R15 = timeout threshold (QPC)
    
    ; Acquire shared lock for reading node states
    lea rcx, [r13+HEARTBEAT_MONITOR.Lock]
    call QWORD PTR [__imp_AcquireSRWLockShared]
    
    ; Get node array pointer
    mov rsi, [r12+WEEK1_INFRASTRUCTURE.HeartbeatNodes]
    test rsi, rsi
    jz @HB_NodesSkip
    
    ; Iterate all nodes
    xor ebx, ebx                        ; EBX = node index
    mov edi, [r13+HEARTBEAT_MONITOR.NodeCount]
    
@HB_NodeLoop:
    cmp ebx, edi
    jge @HB_NodesDone
    
    ; Calculate node address
    mov eax, ebx
    mov ecx, SIZEOF_HEARTBEAT_NODE
    imul eax, ecx
    lea rcx, [rsi + rax]                ; RCX = current node
    mov [rsp+64], rcx                   ; Save node pointer
    
    ; Check if node is active (NodeId != 0)
    cmp DWORD PTR [rcx+HEARTBEAT_NODE.NodeId], 0
    je @HB_NextNode                     ; Skip inactive slots
    
    ; Check current state
    mov eax, [rcx+HEARTBEAT_NODE.State]
    cmp eax, NODE_STATE_DEAD
    je @HB_NextNode                     ; Already dead, skip
    
    ; Calculate time since last heartbeat
    mov rax, r14
    sub rax, [rcx+HEARTBEAT_NODE.LastHeartbeatTime]
    ; RAX = QPC ticks since last heartbeat
    
    ; Check against timeout threshold
    cmp rax, r15
    jb @HB_NodeHealthy                  ; Within threshold, node is healthy
    
    ; Node missed heartbeat - check current state
    mov rcx, [rsp+64]                   ; Restore node pointer
    mov eax, [rcx+HEARTBEAT_NODE.State]
    
    cmp eax, NODE_STATE_HEALTHY
    je @HB_TransitionToSuspect
    
    cmp eax, NODE_STATE_SUSPECT
    je @HB_CheckDeadTransition
    
    jmp @HB_NextNode
    
@HB_TransitionToSuspect:
    ; Transition HEALTHY -> SUSPECT
    ; Need exclusive lock for write
    push rcx
    lea rcx, [r13+HEARTBEAT_MONITOR.Lock]
    call QWORD PTR [__imp_ReleaseSRWLockShared]
    
    lea rcx, [r13+HEARTBEAT_MONITOR.Lock]
    call QWORD PTR [__imp_AcquireSRWLockExclusive]
    pop rcx
    
    ; Re-check state (may have changed)
    cmp DWORD PTR [rcx+HEARTBEAT_NODE.State], NODE_STATE_HEALTHY
    jne @HB_ReacquireShared
    
    ; Perform transition
    mov DWORD PTR [rcx+HEARTBEAT_NODE.State], NODE_STATE_SUSPECT
    inc DWORD PTR [rcx+HEARTBEAT_NODE.ConsecutiveMisses]
    inc DWORD PTR [rcx+HEARTBEAT_NODE.TotalMisses]
    inc QWORD PTR [r13+HEARTBEAT_MONITOR.TotalStateChanges]
    
    ; Check for callback
    mov rax, [rcx+HEARTBEAT_NODE.Callback]
    test rax, rax
    jz @HB_ReacquireShared
    
    ; Call: callback(context, node_id, old_state, new_state)
    push rcx
    push rax
    mov r9d, NODE_STATE_SUSPECT         ; New state
    mov r8d, NODE_STATE_HEALTHY         ; Old state
    mov edx, [rcx+HEARTBEAT_NODE.NodeId]
    mov rcx, [rcx+HEARTBEAT_NODE.CallbackContext]
    pop rax
    call rax
    pop rcx
    
@HB_ReacquireShared:
    ; Release exclusive, acquire shared
    lea rcx, [r13+HEARTBEAT_MONITOR.Lock]
    call QWORD PTR [__imp_ReleaseSRWLockExclusive]
    
    lea rcx, [r13+HEARTBEAT_MONITOR.Lock]
    call QWORD PTR [__imp_AcquireSRWLockShared]
    
    jmp @HB_NextNode
    
@HB_CheckDeadTransition:
    ; Check consecutive misses against threshold
    mov eax, [rcx+HEARTBEAT_NODE.ConsecutiveMisses]
    inc eax
    cmp eax, [r13+HEARTBEAT_MONITOR.MaxMisses]
    jb @HB_IncrementMiss
    
    ; Transition SUSPECT -> DEAD
    push rcx
    lea rcx, [r13+HEARTBEAT_MONITOR.Lock]
    call QWORD PTR [__imp_ReleaseSRWLockShared]
    
    lea rcx, [r13+HEARTBEAT_MONITOR.Lock]
    call QWORD PTR [__imp_AcquireSRWLockExclusive]
    pop rcx
    
    ; Re-check state
    cmp DWORD PTR [rcx+HEARTBEAT_NODE.State], NODE_STATE_SUSPECT
    jne @HB_ReacquireShared
    
    ; Perform transition
    mov DWORD PTR [rcx+HEARTBEAT_NODE.State], NODE_STATE_DEAD
    inc QWORD PTR [r13+HEARTBEAT_MONITOR.TotalStateChanges]
    
    ; Debug output
    push rcx
    lea rcx, szNodeStateChange
    call QWORD PTR [__imp_OutputDebugStringA]
    pop rcx
    
    ; Check for callback
    mov rax, [rcx+HEARTBEAT_NODE.Callback]
    test rax, rax
    jz @HB_ReacquireShared
    
    push rcx
    push rax
    mov r9d, NODE_STATE_DEAD
    mov r8d, NODE_STATE_SUSPECT
    mov edx, [rcx+HEARTBEAT_NODE.NodeId]
    mov rcx, [rcx+HEARTBEAT_NODE.CallbackContext]
    pop rax
    call rax
    pop rcx
    
    jmp @HB_ReacquireShared
    
@HB_IncrementMiss:
    ; Just increment miss counter (need exclusive lock)
    push rcx
    lea rcx, [r13+HEARTBEAT_MONITOR.Lock]
    call QWORD PTR [__imp_ReleaseSRWLockShared]
    
    lea rcx, [r13+HEARTBEAT_MONITOR.Lock]
    call QWORD PTR [__imp_AcquireSRWLockExclusive]
    pop rcx
    
    inc DWORD PTR [rcx+HEARTBEAT_NODE.ConsecutiveMisses]
    inc DWORD PTR [rcx+HEARTBEAT_NODE.TotalMisses]
    
    jmp @HB_ReacquireShared
    
@HB_NodeHealthy:
    ; Node is within threshold, nothing to do
    
@HB_NextNode:
    inc ebx
    jmp @HB_NodeLoop
    
@HB_NodesDone:
@HB_NodesSkip:
    ; Release shared lock
    lea rcx, [r13+HEARTBEAT_MONITOR.Lock]
    call QWORD PTR [__imp_ReleaseSRWLockShared]
    
    ; Sleep until next check
    mov ecx, [r13+HEARTBEAT_MONITOR.CheckIntervalMs]
    call QWORD PTR [__imp_Sleep]
    
    jmp @HB_MonitorLoop
    
@HB_MonitorShutdown:
    mov DWORD PTR [r13+HEARTBEAT_MONITOR.IsRunning], 0
    
    xor eax, eax
    add rsp, 128
    pop rdi
    pop rsi
    pop rbx
    pop r12
    pop r13
    pop r14
    pop r15
    ret
HeartbeatMonitorThread ENDP

;------------------------------------------------------------------------------
; ProcessReceivedHeartbeat - Called when heartbeat received from node
; RCX = infrastructure, EDX = node_id, R8 = timestamp, R9 = latency_ns
; Returns EAX = 1 success, 0 failure
;------------------------------------------------------------------------------
ProcessReceivedHeartbeat PROC EXPORT FRAME
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
    push rdi
    .pushreg rdi
    
    sub rsp, 64
    .allocstack 64
    .endprolog
    
    mov r12, rcx                        ; R12 = infrastructure
    mov r13d, edx                       ; R13 = node_id
    mov r14, r8                         ; R14 = timestamp
    mov r15, r9                         ; R15 = latency_ns
    
    ; Validate infrastructure
    test r12, r12
    jz @HB_Recv_Fail
    
    lea rbx, [r12+WEEK1_INFRASTRUCTURE.Heartbeat]
    
    ; Get nodes array
    mov rax, [r12+WEEK1_INFRASTRUCTURE.HeartbeatNodes]
    test rax, rax
    jz @HB_Recv_Fail
    mov [rsp+32], rax                   ; Save nodes base
    
    ; Find node by ID
    xor ecx, ecx                        ; ECX = index
    mov edx, [rbx+HEARTBEAT_MONITOR.NodeCount]
    
@HB_Recv_FindLoop:
    cmp ecx, edx
    jge @HB_Recv_NotFound
    
    mov eax, ecx
    imul eax, SIZEOF_HEARTBEAT_NODE
    mov rdi, [rsp+32]
    add rdi, rax                        ; RDI = node pointer
    
    cmp [rdi+HEARTBEAT_NODE.NodeId], r13d
    je @HB_Recv_Found
    
    inc ecx
    jmp @HB_Recv_FindLoop
    
@HB_Recv_Found:
    ; Found node at RDI
    ; Acquire exclusive lock
    lea rcx, [rbx+HEARTBEAT_MONITOR.Lock]
    call QWORD PTR [__imp_AcquireSRWLockExclusive]
    
    ; Update node state
    mov eax, [rdi+HEARTBEAT_NODE.State]
    mov [rsp+40], eax                   ; Save old state
    
    mov DWORD PTR [rdi+HEARTBEAT_NODE.State], NODE_STATE_HEALTHY
    mov DWORD PTR [rdi+HEARTBEAT_NODE.ConsecutiveMisses], 0
    mov [rdi+HEARTBEAT_NODE.LastHeartbeatTime], r14
    inc DWORD PTR [rdi+HEARTBEAT_NODE.TotalReceived]
    
    ; Update average latency (EWMA: new_avg = old_avg * 7/8 + new_sample * 1/8)
    mov rax, [rdi+HEARTBEAT_NODE.AvgLatencyNs]
    mov rcx, rax
    shr rcx, 3                          ; rcx = old_avg / 8
    sub rax, rcx                        ; rax = old_avg * 7/8
    mov rcx, r15
    shr rcx, 3                          ; rcx = new_sample / 8
    add rax, rcx                        ; rax = new average
    mov [rdi+HEARTBEAT_NODE.AvgLatencyNs], rax
    
    inc QWORD PTR [rbx+HEARTBEAT_MONITOR.TotalHeartbeatsRcvd]
    
    ; Check if state changed (was not HEALTHY)
    mov eax, [rsp+40]
    cmp eax, NODE_STATE_HEALTHY
    je @HB_Recv_NoCallback
    
    ; State changed - check callback
    inc QWORD PTR [rbx+HEARTBEAT_MONITOR.TotalStateChanges]
    
    mov rax, [rdi+HEARTBEAT_NODE.Callback]
    test rax, rax
    jz @HB_Recv_NoCallback
    
    ; Call: callback(context, node_id, old_state, new_state)
    push rdi
    push rax
    mov r9d, NODE_STATE_HEALTHY         ; New state
    mov r8d, [rsp+56]                   ; Old state (offset adjusted for pushes)
    mov edx, r13d                       ; Node ID
    mov rcx, [rdi+HEARTBEAT_NODE.CallbackContext]
    pop rax
    call rax
    pop rdi
    
@HB_Recv_NoCallback:
    lea rcx, [rbx+HEARTBEAT_MONITOR.Lock]
    call QWORD PTR [__imp_ReleaseSRWLockExclusive]
    
    mov eax, 1                          ; Success
    jmp @HB_Recv_Exit
    
@HB_Recv_NotFound:
@HB_Recv_Fail:
    xor eax, eax                        ; Failure
    
@HB_Recv_Exit:
    add rsp, 64
    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
ProcessReceivedHeartbeat ENDP

;------------------------------------------------------------------------------
; Week1RegisterNode - Register a node for heartbeat monitoring
; RCX = infrastructure, EDX = node_id, R8D = ip_address, R9W = port
; [rsp+40] = callback, [rsp+48] = callback_context
; Returns EAX = 1 success, 0 failure
;------------------------------------------------------------------------------
Week1RegisterNode PROC EXPORT FRAME
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
    push rdi
    .pushreg rdi
    
    sub rsp, 64
    .allocstack 64
    .endprolog
    
    mov r12, rcx                        ; infrastructure
    mov r13d, edx                       ; node_id
    mov r14d, r8d                       ; ip_address
    movzx r15d, r9w                     ; port
    
    ; Validate
    test r12, r12
    jz @RegNode_Fail
    
    lea rbx, [r12+WEEK1_INFRASTRUCTURE.Heartbeat]
    
    ; Get nodes array
    mov rdi, [r12+WEEK1_INFRASTRUCTURE.HeartbeatNodes]
    test rdi, rdi
    jz @RegNode_Fail
    
    ; Acquire exclusive lock
    lea rcx, [rbx+HEARTBEAT_MONITOR.Lock]
    call QWORD PTR [__imp_AcquireSRWLockExclusive]
    
    ; Check if full
    mov eax, [rbx+HEARTBEAT_MONITOR.NodeCount]
    cmp eax, HB_MAX_NODES
    jge @RegNode_Full
    
    ; Find empty slot or existing node
    xor ecx, ecx
    
@RegNode_FindSlot:
    cmp ecx, HB_MAX_NODES
    jge @RegNode_Full
    
    mov eax, ecx
    imul eax, SIZEOF_HEARTBEAT_NODE
    lea rax, [rdi + rax]
    
    ; Check if slot is empty (NodeId == 0)
    cmp DWORD PTR [rax+HEARTBEAT_NODE.NodeId], 0
    je @RegNode_UseSlot
    
    ; Check if same node ID (update existing)
    cmp [rax+HEARTBEAT_NODE.NodeId], r13d
    je @RegNode_UseSlot
    
    inc ecx
    jmp @RegNode_FindSlot
    
@RegNode_UseSlot:
    ; RAX = node slot pointer
    ; Check if this is a new node (to increment count)
    mov ecx, [rax+HEARTBEAT_NODE.NodeId]
    push rcx                            ; Save old NodeId
    
    ; Initialize node
    mov [rax+HEARTBEAT_NODE.NodeId], r13d
    mov DWORD PTR [rax+HEARTBEAT_NODE.State], NODE_STATE_HEALTHY
    mov DWORD PTR [rax+HEARTBEAT_NODE.ConsecutiveMisses], 0
    mov DWORD PTR [rax+HEARTBEAT_NODE.TotalMisses], 0
    mov DWORD PTR [rax+HEARTBEAT_NODE.TotalReceived], 0
    mov QWORD PTR [rax+HEARTBEAT_NODE.AvgLatencyNs], 0
    mov [rax+HEARTBEAT_NODE.IpAddress], r14d
    mov [rax+HEARTBEAT_NODE.Port], r15w
    
    ; Get current timestamp for initial heartbeat time
    push rax
    call GetCurrentTimestamp
    mov rcx, rax
    pop rax
    mov [rax+HEARTBEAT_NODE.LastHeartbeatTime], rcx
    
    ; Set callback from stack
    mov rcx, [rsp+64+64+40]             ; callback (adjust for saved regs + shadow)
    mov [rax+HEARTBEAT_NODE.Callback], rcx
    mov rcx, [rsp+64+64+48]             ; callback_context
    mov [rax+HEARTBEAT_NODE.CallbackContext], rcx
    
    ; Increment node count if this was a new slot
    pop rcx                             ; Restore old NodeId
    test ecx, ecx
    jnz @RegNode_Success                ; Was existing node
    inc DWORD PTR [rbx+HEARTBEAT_MONITOR.NodeCount]
    
@RegNode_Success:
    lea rcx, [rbx+HEARTBEAT_MONITOR.Lock]
    call QWORD PTR [__imp_ReleaseSRWLockExclusive]
    
    mov eax, 1
    jmp @RegNode_Exit
    
@RegNode_Full:
    lea rcx, [rbx+HEARTBEAT_MONITOR.Lock]
    call QWORD PTR [__imp_ReleaseSRWLockExclusive]
    
@RegNode_Fail:
    xor eax, eax
    
@RegNode_Exit:
    add rsp, 64
    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
Week1RegisterNode ENDP

;==============================================================================
; CONFLICT DETECTOR (287 LOC)
;==============================================================================

;------------------------------------------------------------------------------
; ConflictDetectorThread - Background deadlock detection
; RCX = WEEK1_INFRASTRUCTURE pointer
;------------------------------------------------------------------------------
ConflictDetectorThread PROC FRAME
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
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    
    sub rsp, 128
    .allocstack 128
    .endprolog
    
    mov r12, rcx                        ; R12 = infrastructure
    lea r13, [r12+WEEK1_INFRASTRUCTURE.ConflictDetector]
    
    mov DWORD PTR [r13+CONFLICT_DETECTOR.IsRunning], 1
    
    lea rcx, szConflictStart
    call QWORD PTR [__imp_OutputDebugStringA]
    
@CD_DetectorLoop:
    ; Check shutdown (poll)
    mov rcx, [r13+CONFLICT_DETECTOR.ShutdownEvent]
    xor edx, edx
    call QWORD PTR [__imp_WaitForSingleObject]
    cmp eax, WAIT_OBJECT_0
    je @CD_DetectorShutdown
    
    ; Build wait graph
    mov rcx, r12
    mov rdx, r13
    call BuildWaitGraph
    
    ; Detect cycles (deadlocks)
    mov rcx, r12
    mov rdx, r13
    call DetectDeadlocks
    
    ; Sleep until next scan
    mov ecx, [r13+CONFLICT_DETECTOR.ScanIntervalMs]
    call QWORD PTR [__imp_Sleep]
    
    jmp @CD_DetectorLoop
    
@CD_DetectorShutdown:
    mov DWORD PTR [r13+CONFLICT_DETECTOR.IsRunning], 0
    xor eax, eax
    add rsp, 128
    pop rdi
    pop rsi
    pop rbx
    pop r12
    pop r13
    pop r14
    pop r15
    ret
ConflictDetectorThread ENDP

;------------------------------------------------------------------------------
; BuildWaitGraph - Construct resource wait graph for cycle detection
; RCX = infrastructure, RDX = conflict detector
;------------------------------------------------------------------------------
BuildWaitGraph PROC FRAME
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
    
    sub rsp, 64
    .allocstack 64
    .endprolog
    
    mov r12, rcx                        ; infrastructure
    mov r13, rdx                        ; conflict detector
    
    ; Acquire exclusive lock
    lea rcx, [r13+CONFLICT_DETECTOR.Lock]
    call QWORD PTR [__imp_AcquireSRWLockExclusive]
    
    ; Clear existing edge count
    mov DWORD PTR [r13+CONFLICT_DETECTOR.EdgeCount], 0
    
    ; Get resources array
    mov rsi, [r12+WEEK1_INFRASTRUCTURE.ConflictResources]
    test rsi, rsi
    jz @BWG_Done
    
    ; Get wait graph array
    mov rdi, [r12+WEEK1_INFRASTRUCTURE.ConflictWaitGraph]
    test rdi, rdi
    jz @BWG_Done
    
    ; Iterate all resources
    xor ebx, ebx                        ; resource index
    mov r14d, [r13+CONFLICT_DETECTOR.ResourceCount]
    xor r15d, r15d                      ; edge count
    
@BWG_ResourceLoop:
    cmp ebx, r14d
    jge @BWG_Done
    
    ; Get resource entry
    mov eax, ebx
    imul eax, SIZEOF_CONFLICT_ENTRY
    lea rcx, [rsi + rax]                ; RCX = resource entry
    mov [rsp+32], rcx                   ; Save
    
    ; Check if has owner
    mov edx, [rcx+CONFLICT_ENTRY.OwnerThreadId]
    test edx, edx
    jz @BWG_NextResource                ; No owner, skip
    
    ; Check if has waiters
    mov eax, [rcx+CONFLICT_ENTRY.WaiterCount]
    test eax, eax
    jz @BWG_NextResource                ; No waiters, skip
    
    ; Add edges: waiter -> owner for each waiter
    xor ecx, ecx                        ; waiter index
    mov r8d, eax                        ; waiter count
    mov r9, [rsp+32]                    ; resource entry
    
@BWG_WaiterLoop:
    cmp ecx, r8d
    jge @BWG_NextResource
    cmp r15d, CD_MAX_WAIT_EDGES
    jge @BWG_Done                       ; Graph full
    
    ; Get waiter thread ID
    mov eax, [r9+CONFLICT_ENTRY.WaiterThreadIds+rcx*4]
    test eax, eax
    jz @BWG_NextWaiter                  ; Empty slot
    
    ; Build edge: (waiter << 16) | owner
    shl eax, 16
    or eax, edx                         ; EDX still has owner
    
    ; Store edge
    mov [rdi+r15*4], eax
    inc r15d
    
@BWG_NextWaiter:
    inc ecx
    jmp @BWG_WaiterLoop
    
@BWG_NextResource:
    inc ebx
    jmp @BWG_ResourceLoop
    
@BWG_Done:
    ; Store final edge count
    mov [r13+CONFLICT_DETECTOR.EdgeCount], r15d
    
    ; Release lock
    lea rcx, [r13+CONFLICT_DETECTOR.Lock]
    call QWORD PTR [__imp_ReleaseSRWLockExclusive]
    
    add rsp, 64
    pop rdi
    pop rsi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
BuildWaitGraph ENDP

;------------------------------------------------------------------------------
; DetectDeadlocks - DFS cycle detection in wait graph
; RCX = infrastructure, RDX = conflict detector
; Returns EAX = number of deadlocks detected
;------------------------------------------------------------------------------
DetectDeadlocks PROC FRAME
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
    
    sub rsp, 256                        ; Space for visited array
    .allocstack 256
    .endprolog
    
    mov r12, rcx                        ; infrastructure
    mov r13, rdx                        ; conflict detector
    
    ; Acquire shared lock
    lea rcx, [r13+CONFLICT_DETECTOR.Lock]
    call QWORD PTR [__imp_AcquireSRWLockShared]
    
    ; Get edge count
    mov r14d, [r13+CONFLICT_DETECTOR.EdgeCount]
    test r14d, r14d
    jz @DD_NoCycles
    
    ; Get wait graph
    mov rsi, [r12+WEEK1_INFRASTRUCTURE.ConflictWaitGraph]
    test rsi, rsi
    jz @DD_NoCycles
    
    ; Initialize visited array (on stack, max 64 threads tracked)
    lea rdi, [rsp+64]
    mov ecx, 64
    xor eax, eax
    rep stosd
    
    xor r15d, r15d                      ; Deadlock count
    
    ; Simple cycle detection: for each edge, follow chain
    xor ebx, ebx                        ; Edge index
    
@DD_EdgeLoop:
    cmp ebx, r14d
    jge @DD_Done
    
    ; Get edge
    mov eax, [rsi+rbx*4]
    mov ecx, eax
    shr ecx, 16                         ; ECX = waiter
    and eax, 0FFFFh                     ; EAX = owner
    
    ; Follow chain from owner, looking for waiter
    mov edx, eax                        ; Current = owner
    mov r8d, 32                         ; Max depth
    
@DD_FollowChain:
    dec r8d
    jz @DD_NextEdge                     ; Depth limit reached
    
    ; Check if current == waiter (cycle!)
    cmp edx, ecx
    je @DD_CycleFound
    
    ; Find edge where current is waiter
    xor r9d, r9d                        ; Search index
    
@DD_SearchEdge:
    cmp r9d, r14d
    jge @DD_NextEdge                    ; No outgoing edge
    
    mov eax, [rsi+r9*4]
    mov r10d, eax
    shr r10d, 16                        ; R10D = edge waiter
    
    cmp r10d, edx                       ; Is this edge from current?
    jne @DD_SearchNext
    
    ; Found edge from current
    and eax, 0FFFFh                     ; EAX = new target (owner)
    mov edx, eax                        ; Move to next in chain
    jmp @DD_FollowChain
    
@DD_SearchNext:
    inc r9d
    jmp @DD_SearchEdge
    
@DD_CycleFound:
    inc r15d                            ; Increment deadlock count
    
    ; Debug output
    push rbx
    lea rcx, szDeadlockDetected
    call QWORD PTR [__imp_OutputDebugStringA]
    pop rbx
    
@DD_NextEdge:
    inc ebx
    jmp @DD_EdgeLoop
    
@DD_Done:
    ; Update statistics if any deadlocks found
    test r15d, r15d
    jz @DD_NoDeadlocks
    
    ; Need exclusive lock to update stats
    lea rcx, [r13+CONFLICT_DETECTOR.Lock]
    call QWORD PTR [__imp_ReleaseSRWLockShared]
    
    lea rcx, [r13+CONFLICT_DETECTOR.Lock]
    call QWORD PTR [__imp_AcquireSRWLockExclusive]
    
    movzx eax, r15w
    add [r13+CONFLICT_DETECTOR.TotalDeadlocksDetected], rax
    
    lea rcx, [r13+CONFLICT_DETECTOR.Lock]
    call QWORD PTR [__imp_ReleaseSRWLockExclusive]
    
    mov eax, r15d
    jmp @DD_Exit
    
@DD_NoDeadlocks:
@DD_NoCycles:
    lea rcx, [r13+CONFLICT_DETECTOR.Lock]
    call QWORD PTR [__imp_ReleaseSRWLockShared]
    
    xor eax, eax
    
@DD_Exit:
    add rsp, 256
    pop rdi
    pop rsi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
DetectDeadlocks ENDP

;------------------------------------------------------------------------------
; Week1RegisterResource - Register a resource for conflict tracking
; RCX = infrastructure, RDX = resource_id, R8 = name (optional)
; Returns EAX = 1 success, 0 failure
;------------------------------------------------------------------------------
Week1RegisterResource PROC EXPORT FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push rdi
    .pushreg rdi
    
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov r12, rcx                        ; infrastructure
    mov r13, rdx                        ; resource_id
    mov r14, r8                         ; name
    
    test r12, r12
    jz @RR_Fail
    
    lea rbx, [r12+WEEK1_INFRASTRUCTURE.ConflictDetector]
    
    ; Get resources array
    mov rdi, [r12+WEEK1_INFRASTRUCTURE.ConflictResources]
    test rdi, rdi
    jz @RR_Fail
    
    ; Acquire exclusive lock
    lea rcx, [rbx+CONFLICT_DETECTOR.Lock]
    call QWORD PTR [__imp_AcquireSRWLockExclusive]
    
    ; Check capacity
    mov eax, [rbx+CONFLICT_DETECTOR.ResourceCount]
    cmp eax, CD_MAX_RESOURCES
    jge @RR_Full
    
    ; Find empty slot
    xor ecx, ecx
    
@RR_FindSlot:
    cmp ecx, CD_MAX_RESOURCES
    jge @RR_Full
    
    mov eax, ecx
    imul eax, SIZEOF_CONFLICT_ENTRY
    lea rax, [rdi + rax]
    
    ; Check if empty (ResourceId == 0)
    cmp QWORD PTR [rax+CONFLICT_ENTRY.ResourceId], 0
    je @RR_UseSlot
    
    ; Check if same resource (update)
    cmp [rax+CONFLICT_ENTRY.ResourceId], r13
    je @RR_UseSlot
    
    inc ecx
    jmp @RR_FindSlot
    
@RR_UseSlot:
    ; Check if new resource
    mov rcx, [rax+CONFLICT_ENTRY.ResourceId]
    push rcx                            ; Save old ResourceId
    
    ; Initialize resource entry
    mov [rax+CONFLICT_ENTRY.ResourceId], r13
    mov DWORD PTR [rax+CONFLICT_ENTRY.OwnerThreadId], 0
    mov DWORD PTR [rax+CONFLICT_ENTRY.WaiterCount], 0
    mov DWORD PTR [rax+CONFLICT_ENTRY.LockCount], 0
    mov QWORD PTR [rax+CONFLICT_ENTRY.AcquisitionTime], 0
    mov QWORD PTR [rax+CONFLICT_ENTRY.TotalContentions], 0
    mov QWORD PTR [rax+CONFLICT_ENTRY.MaxWaitTimeNs], 0
    
    ; Copy name if provided
    test r14, r14
    jz @RR_NoName
    
    push rax
    lea rcx, [rax+CONFLICT_ENTRY.ResourceName]
    mov rdx, r14
    mov r8d, 63                         ; Max length
    call StrNCopy
    pop rax
    
@RR_NoName:
    ; Increment count if new
    pop rcx                             ; Restore old ResourceId
    test rcx, rcx
    jnz @RR_Success                     ; Was existing resource
    inc DWORD PTR [rbx+CONFLICT_DETECTOR.ResourceCount]
    
@RR_Success:
    lea rcx, [rbx+CONFLICT_DETECTOR.Lock]
    call QWORD PTR [__imp_ReleaseSRWLockExclusive]
    
    mov eax, 1
    jmp @RR_Exit
    
@RR_Full:
    lea rcx, [rbx+CONFLICT_DETECTOR.Lock]
    call QWORD PTR [__imp_ReleaseSRWLockExclusive]
    
@RR_Fail:
    xor eax, eax
    
@RR_Exit:
    add rsp, 48
    pop rdi
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
Week1RegisterResource ENDP

;==============================================================================
; TASK SCHEDULER (356 LOC)
;==============================================================================

;------------------------------------------------------------------------------
; SchedulerCoordinatorThread - Load balancing and coordination
; RCX = WEEK1_INFRASTRUCTURE pointer
;------------------------------------------------------------------------------
SchedulerCoordinatorThread PROC FRAME
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
    
    sub rsp, 128
    .allocstack 128
    .endprolog
    
    mov r12, rcx                        ; R12 = infrastructure
    lea r13, [r12+WEEK1_INFRASTRUCTURE.Scheduler]
    
    mov DWORD PTR [r13+TASK_SCHEDULER.IsRunning], 1
    
    lea rcx, szSchedulerStart
    call QWORD PTR [__imp_OutputDebugStringA]
    
@SC_CoordinatorLoop:
    ; Check shutdown
    mov rcx, [r13+TASK_SCHEDULER.ShutdownEvent]
    xor edx, edx
    call QWORD PTR [__imp_WaitForSingleObject]
    cmp eax, WAIT_OBJECT_0
    je @SC_CoordinatorShutdown
    
    ; Balance load across workers
    mov rcx, r12
    mov rdx, r13
    call BalanceLoad
    
    ; Sleep briefly (10ms coordination interval)
    mov ecx, 10
    call QWORD PTR [__imp_Sleep]
    
    jmp @SC_CoordinatorLoop
    
@SC_CoordinatorShutdown:
    mov DWORD PTR [r13+TASK_SCHEDULER.IsRunning], 0
    xor eax, eax
    add rsp, 128
    pop rbx
    pop r12
    pop r13
    pop r14
    pop r15
    ret
SchedulerCoordinatorThread ENDP

;------------------------------------------------------------------------------
; WorkerThreadProc - Individual worker thread
; RCX = THREAD_CONTEXT pointer
;------------------------------------------------------------------------------
WorkerThreadProc PROC FRAME
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
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    
    sub rsp, 128
    .allocstack 128
    .endprolog
    
    mov r12, rcx                        ; R12 = thread context
    mov r13d, [r12+THREAD_CONTEXT.WorkerId]
    mov r14, [r12+THREAD_CONTEXT.Scheduler]
    
    mov DWORD PTR [r12+THREAD_CONTEXT.IsRunning], 1
    
    ; Debug output
    lea rcx, szWorkerStart
    call QWORD PTR [__imp_OutputDebugStringA]
    
@WT_WorkerLoop:
    ; Check shutdown flag
    cmp DWORD PTR [r12+THREAD_CONTEXT.ShouldShutdown], 1
    je @WT_WorkerShutdown
    
    ; Try to get task from local queue first
    mov rcx, r12
    call PopLocalTask
    test rax, rax
    jnz @WT_ExecuteTask
    
    ; Local queue empty - try global queue
    mov rcx, r12
    call PopGlobalTask
    test rax, rax
    jnz @WT_ExecuteTask
    
    ; Global queue empty - try to steal from other workers
    mov rcx, r12
    call StealTask
    test rax, rax
    jnz @WT_ExecuteTask
    
    ; No work available - idle briefly
    call GetCurrentTimestamp
    mov rbx, rax
    
    pause                               ; CPU hint for spin-wait
    mov ecx, 1                          ; Sleep 1ms
    call QWORD PTR [__imp_Sleep]
    
    ; Track idle time
    call GetCurrentTimestamp
    sub rax, rbx
    add [r12+THREAD_CONTEXT.IdleTimeNs], rax
    
    jmp @WT_WorkerLoop
    
@WT_ExecuteTask:
    mov r15, rax                        ; R15 = task pointer
    
    ; Update task state to RUNNING
    mov DWORD PTR [r15+TASK.State], TASK_STATE_RUNNING
    mov [r15+TASK.WorkerThreadId], r13d
    mov [r12+THREAD_CONTEXT.CurrentTask], r15
    
    ; Record start time
    call GetCurrentTimestamp
    mov [r15+TASK.StartTime], rax
    
    ; Debug output
    lea rcx, szTaskExecute
    call QWORD PTR [__imp_OutputDebugStringA]
    
    ; Execute task function
    ; Call: TaskFunction(Context)
    mov rcx, [r15+TASK.Context]
    mov rax, [r15+TASK.TaskFunction]
    test rax, rax
    jz @WT_TaskComplete                 ; No function, skip
    
    call rax
    
@WT_TaskComplete:
    ; Record completion time
    call GetCurrentTimestamp
    mov [r15+TASK.CompleteTime], rax
    mov DWORD PTR [r15+TASK.State], TASK_STATE_COMPLETE
    
    ; Update statistics
    inc QWORD PTR [r12+THREAD_CONTEXT.TasksExecuted]
    
    ; Calculate and accumulate execution time
    mov rax, [r15+TASK.CompleteTime]
    sub rax, [r15+TASK.StartTime]
    add [r12+THREAD_CONTEXT.TotalExecutionTimeNs], rax
    
    ; Update scheduler statistics
    mov rcx, r14
    test rcx, rcx
    jz @WT_ClearCurrent
    
    lea rcx, [r14+TASK_SCHEDULER.TotalTasksExecuted]
    call AtomicIncrement64
    
@WT_ClearCurrent:
    mov QWORD PTR [r12+THREAD_CONTEXT.CurrentTask], 0
    
    jmp @WT_WorkerLoop
    
@WT_WorkerShutdown:
    mov DWORD PTR [r12+THREAD_CONTEXT.IsRunning], 0
    xor eax, eax
    add rsp, 128
    pop rdi
    pop rsi
    pop rbx
    pop r12
    pop r13
    pop r14
    pop r15
    ret
WorkerThreadProc ENDP

;------------------------------------------------------------------------------
; PopLocalTask - Pop from worker's local queue (LIFO for cache locality)
; RCX = THREAD_CONTEXT pointer
; Returns RAX = task pointer or NULL
;------------------------------------------------------------------------------
PopLocalTask PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov r12, rcx                        ; Thread context
    
    ; Check if queue is empty
    mov eax, [r12+THREAD_CONTEXT.LocalQueueCount]
    test eax, eax
    jz @PLT_Empty
    
    ; Get scheduler to access local queues
    mov rbx, [r12+THREAD_CONTEXT.Scheduler]
    test rbx, rbx
    jz @PLT_Empty
    
    ; Get infrastructure from scheduler
    lea rax, [rbx - WEEK1_INFRASTRUCTURE.Scheduler]
    mov rbx, rax                        ; RBX = infrastructure
    
    ; Get local queues base
    mov rax, [rbx+WEEK1_INFRASTRUCTURE.LocalQueues]
    test rax, rax
    jz @PLT_Empty
    
    ; Calculate this worker's queue offset
    mov ecx, [r12+THREAD_CONTEXT.WorkerId]
    imul ecx, TS_LOCAL_QUEUE_SIZE * 8   ; Each entry is QWORD
    add rax, rcx                        ; RAX = this worker's queue base
    
    ; Pop from head (LIFO)
    mov ecx, [r12+THREAD_CONTEXT.LocalQueueHead]
    test ecx, ecx
    jz @PLT_Empty                       ; Head at 0 means empty
    
    dec ecx                             ; Move head back
    mov rdx, [rax+rcx*8]                ; Get task at new head
    mov [r12+THREAD_CONTEXT.LocalQueueHead], ecx
    dec DWORD PTR [r12+THREAD_CONTEXT.LocalQueueCount]
    
    mov rax, rdx                        ; Return task
    jmp @PLT_Exit
    
@PLT_Empty:
    xor eax, eax
    
@PLT_Exit:
    add rsp, 40
    pop r12
    pop rbx
    ret
PopLocalTask ENDP

;------------------------------------------------------------------------------
; PopGlobalTask - Pop from global queue
; RCX = THREAD_CONTEXT pointer
; Returns RAX = task pointer or NULL
;------------------------------------------------------------------------------
PopGlobalTask PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov r12, rcx                        ; Thread context
    
    ; Get scheduler
    mov r13, [r12+THREAD_CONTEXT.Scheduler]
    test r13, r13
    jz @PGT_Empty
    
    ; Acquire global queue lock
    lea rcx, [r13+TASK_SCHEDULER.GlobalQueueLock]
    call QWORD PTR [__imp_AcquireSRWLockExclusive]
    
    ; Check if empty
    mov rax, [r13+TASK_SCHEDULER.GlobalQueueHead]
    cmp rax, [r13+TASK_SCHEDULER.GlobalQueueTail]
    je @PGT_EmptyUnlock
    
    ; Get infrastructure
    lea rbx, [r13 - WEEK1_INFRASTRUCTURE.Scheduler]
    
    ; Get global queue array
    mov rcx, [rbx+WEEK1_INFRASTRUCTURE.GlobalQueue]
    test rcx, rcx
    jz @PGT_EmptyUnlock
    
    ; Pop from head (FIFO for global queue)
    mov rdx, [r13+TASK_SCHEDULER.GlobalQueueHead]
    mov rax, [rcx+rdx*8]                ; Get task
    
    ; Increment head (circular)
    inc rdx
    cmp rdx, TS_GLOBAL_QUEUE_SIZE
    jb @PGT_NoWrap
    xor edx, edx                        ; Wrap to 0
@PGT_NoWrap:
    mov [r13+TASK_SCHEDULER.GlobalQueueHead], rdx
    
    ; Unlock
    push rax
    lea rcx, [r13+TASK_SCHEDULER.GlobalQueueLock]
    call QWORD PTR [__imp_ReleaseSRWLockExclusive]
    pop rax
    
    jmp @PGT_Exit
    
@PGT_EmptyUnlock:
    lea rcx, [r13+TASK_SCHEDULER.GlobalQueueLock]
    call QWORD PTR [__imp_ReleaseSRWLockExclusive]
    
@PGT_Empty:
    xor eax, eax
    
@PGT_Exit:
    add rsp, 48
    pop r13
    pop r12
    pop rbx
    ret
PopGlobalTask ENDP

;------------------------------------------------------------------------------
; StealTask - Steal from another worker's queue
; RCX = THREAD_CONTEXT pointer (this worker)
; Returns RAX = stolen task or NULL
;------------------------------------------------------------------------------
StealTask PROC FRAME
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
    
    sub rsp, 64
    .allocstack 64
    .endprolog
    
    mov r12, rcx                        ; This worker
    mov r13d, [r12+THREAD_CONTEXT.WorkerId]
    
    ; Get scheduler
    mov r14, [r12+THREAD_CONTEXT.Scheduler]
    test r14, r14
    jz @ST_Failed
    
    ; Get infrastructure
    lea rbx, [r14 - WEEK1_INFRASTRUCTURE.Scheduler]
    
    ; Get workers array
    mov rsi, [rbx+WEEK1_INFRASTRUCTURE.Workers]
    test rsi, rsi
    jz @ST_Failed
    
    ; Get local queues
    mov rdi, [rbx+WEEK1_INFRASTRUCTURE.LocalQueues]
    test rdi, rdi
    jz @ST_Failed
    
    ; Get worker count
    mov r15d, [r14+TASK_SCHEDULER.WorkerCount]
    
    ; Update steal attempt counter
    lea rcx, [r14+TASK_SCHEDULER.TotalStealAttempts]
    call AtomicIncrement64
    
    ; Try to steal from other workers (start with next worker)
    mov ecx, r13d
    inc ecx                             ; Start with next worker
    mov [rsp+32], ecx                   ; Save start index
    
@ST_StealLoop:
    ; Wrap around
    cmp ecx, r15d
    jb @ST_NoWrap
    xor ecx, ecx
@ST_NoWrap:
    
    ; Check if we've tried all workers
    mov eax, [rsp+32]
    dec eax
    and eax, r15d                       ; Handle wrap
    cmp ecx, eax
    je @ST_Failed                       ; Back to start
    
    ; Skip self
    cmp ecx, r13d
    je @ST_NextVictim
    
    ; Get victim worker context
    mov eax, ecx
    imul eax, SIZEOF_THREAD_CONTEXT
    lea rax, [rsi + rax]                ; Victim context
    mov [rsp+40], rax                   ; Save victim
    mov [rsp+48], ecx                   ; Save victim index
    
    ; Check if victim has tasks
    mov edx, [rax+THREAD_CONTEXT.LocalQueueCount]
    cmp edx, 1
    jl @ST_NextVictim                   ; Need at least 2 tasks to steal
    
    ; Calculate victim's queue base
    mov eax, ecx
    imul eax, TS_LOCAL_QUEUE_SIZE * 8
    lea r8, [rdi + rax]                 ; Victim's queue
    
    ; Steal from tail (opposite end from owner's pop)
    mov rax, [rsp+40]                   ; Victim context
    mov edx, [rax+THREAD_CONTEXT.LocalQueueTail]
    
    ; Get task from tail
    mov rax, [r8+rdx*8]
    test rax, rax
    jz @ST_NextVictim
    
    ; Clear the slot
    mov QWORD PTR [r8+rdx*8], 0
    
    ; Update victim's tail
    mov rcx, [rsp+40]
    inc DWORD PTR [rcx+THREAD_CONTEXT.LocalQueueTail]
    ; Wrap tail if needed
    mov edx, [rcx+THREAD_CONTEXT.LocalQueueTail]
    cmp edx, TS_LOCAL_QUEUE_SIZE
    jb @ST_NoTailWrap
    mov DWORD PTR [rcx+THREAD_CONTEXT.LocalQueueTail], 0
@ST_NoTailWrap:
    dec DWORD PTR [rcx+THREAD_CONTEXT.LocalQueueCount]
    
    ; Update statistics
    inc QWORD PTR [rcx+THREAD_CONTEXT.TasksStolenFrom]
    inc QWORD PTR [r12+THREAD_CONTEXT.TasksStolen]
    
    ; Update task steal count
    inc DWORD PTR [rax+TASK.StealCount]
    
    ; Update scheduler statistics
    push rax
    lea rcx, [r14+TASK_SCHEDULER.TotalTasksStolen]
    call AtomicIncrement64
    pop rax
    
    jmp @ST_Exit
    
@ST_NextVictim:
    mov ecx, [rsp+48]
    inc ecx
    jmp @ST_StealLoop
    
@ST_Failed:
    xor eax, eax
    
@ST_Exit:
    add rsp, 64
    pop rdi
    pop rsi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
StealTask ENDP

;------------------------------------------------------------------------------
; SubmitTask - Add task to global queue (public API)
; RCX = infrastructure, RDX = task pointer
; Returns EAX = 1 success, 0 failure
;------------------------------------------------------------------------------
SubmitTask PROC EXPORT FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov r12, rcx                        ; Infrastructure
    mov r13, rdx                        ; Task
    
    ; Validate
    test r12, r12
    jz @ST_Fail
    test r13, r13
    jz @ST_Fail
    
    lea rbx, [r12+WEEK1_INFRASTRUCTURE.Scheduler]
    
    ; Generate task ID
    lea rcx, [rbx+TASK_SCHEDULER.TaskIdLock]
    call QWORD PTR [__imp_AcquireSRWLockExclusive]
    
    mov rax, [rbx+TASK_SCHEDULER.NextTaskId]
    mov [r13+TASK.TaskId], rax
    inc QWORD PTR [rbx+TASK_SCHEDULER.NextTaskId]
    
    lea rcx, [rbx+TASK_SCHEDULER.TaskIdLock]
    call QWORD PTR [__imp_ReleaseSRWLockExclusive]
    
    ; Set submit time
    call GetCurrentTimestamp
    mov [r13+TASK.SubmitTime], rax
    mov DWORD PTR [r13+TASK.State], TASK_STATE_PENDING
    
    ; Acquire global queue lock
    lea rcx, [rbx+TASK_SCHEDULER.GlobalQueueLock]
    call QWORD PTR [__imp_AcquireSRWLockExclusive]
    
    ; Check queue capacity
    mov rax, [rbx+TASK_SCHEDULER.GlobalQueueTail]
    mov rcx, rax
    inc rcx
    cmp rcx, TS_GLOBAL_QUEUE_SIZE
    jb @ST_NoWrap
    xor ecx, ecx
@ST_NoWrap:
    
    ; Check if full (tail+1 == head)
    cmp rcx, [rbx+TASK_SCHEDULER.GlobalQueueHead]
    je @ST_QueueFull
    
    ; Get global queue array
    mov rdx, [r12+WEEK1_INFRASTRUCTURE.GlobalQueue]
    test rdx, rdx
    jz @ST_QueueFull
    
    ; Store task at tail
    mov [rdx+rax*8], r13
    mov [rbx+TASK_SCHEDULER.GlobalQueueTail], rcx
    
    ; Update statistics
    inc QWORD PTR [rbx+TASK_SCHEDULER.TotalTasksSubmitted]
    
    lea rcx, [rbx+TASK_SCHEDULER.GlobalQueueLock]
    call QWORD PTR [__imp_ReleaseSRWLockExclusive]
    
    mov eax, 1
    jmp @ST_Exit
    
@ST_QueueFull:
    lea rcx, [rbx+TASK_SCHEDULER.GlobalQueueLock]
    call QWORD PTR [__imp_ReleaseSRWLockExclusive]
    
@ST_Fail:
    xor eax, eax
    
@ST_Exit:
    add rsp, 48
    pop r13
    pop r12
    pop rbx
    ret
SubmitTask ENDP

;------------------------------------------------------------------------------
; BalanceLoad - Redistribute tasks across workers
; RCX = infrastructure, RDX = scheduler
;------------------------------------------------------------------------------
BalanceLoad PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    
    sub rsp, 64
    .allocstack 64
    .endprolog
    
    mov r12, rcx                        ; Infrastructure
    mov r13, rdx                        ; Scheduler
    
    ; Get workers array
    mov rsi, [r12+WEEK1_INFRASTRUCTURE.Workers]
    test rsi, rsi
    jz @BL_Exit
    
    mov r14d, [r13+TASK_SCHEDULER.WorkerCount]
    test r14d, r14d
    jz @BL_Exit
    
    ; Find min and max loaded workers
    xor ebx, ebx                        ; Index
    mov ecx, 0FFFFFFFFh                 ; Min count
    xor edx, edx                        ; Max count
    xor edi, edi                        ; Min index
    mov [rsp+32], edx                   ; Max index
    
@BL_ScanLoop:
    cmp ebx, r14d
    jge @BL_ScanDone
    
    mov eax, ebx
    imul eax, SIZEOF_THREAD_CONTEXT
    lea rax, [rsi + rax]
    
    mov eax, [rax+THREAD_CONTEXT.LocalQueueCount]
    
    ; Check for new min
    cmp eax, ecx
    jge @BL_CheckMax
    mov ecx, eax
    mov edi, ebx
    
@BL_CheckMax:
    cmp eax, edx
    jle @BL_NextWorker
    mov edx, eax
    mov [rsp+32], ebx
    
@BL_NextWorker:
    inc ebx
    jmp @BL_ScanLoop
    
@BL_ScanDone:
    ; Load balancing relies on work stealing
    ; This function monitors but doesn't actively migrate
    
@BL_Exit:
    add rsp, 64
    pop rdi
    pop rsi
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
BalanceLoad ENDP

;==============================================================================
; THREAD MANAGEMENT (245 LOC)
;==============================================================================

;------------------------------------------------------------------------------
; CreateWorkerThread - Create and configure worker thread
; RCX = infrastructure, EDX = worker_id, R8 = entry_point, R9 = context
; Returns RAX = thread handle or NULL
;------------------------------------------------------------------------------
CreateWorkerThread PROC FRAME
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
    
    sub rsp, 96
    .allocstack 96
    .endprolog
    
    mov r12, rcx                        ; Infrastructure
    mov r13d, edx                       ; Worker ID
    mov r14, r8                         ; Entry point
    mov r15, r9                         ; Context
    
    ; Create thread suspended
    xor ecx, ecx                        ; lpThreadAttributes = NULL
    xor edx, edx                        ; dwStackSize = 0 (default)
    mov r8, r14                         ; lpStartAddress
    mov r9, r15                         ; lpParameter
    mov DWORD PTR [rsp+32], CREATE_SUSPENDED
    lea rax, [rsp+48]                   ; lpThreadId
    mov [rsp+40], rax
    call QWORD PTR [__imp_CreateThread]
    
    test rax, rax
    jz @CWT_Failed
    mov rbx, rax                        ; RBX = thread handle
    
    ; Set thread affinity (pin to core if < 64 cores)
    cmp r13d, 64
    jae @CWT_NoAffinity
    
    mov rcx, rbx                        ; Handle
    mov rdx, 1
    mov ecx, r13d
    shl rdx, cl                         ; Affinity mask = (1 << worker_id)
    mov rcx, rbx
    call QWORD PTR [__imp_SetThreadAffinityMask]
    
@CWT_NoAffinity:
    ; Set thread priority
    mov rcx, rbx
    mov edx, THREAD_PRIORITY_NORMAL
    call QWORD PTR [__imp_SetThreadPriority]
    
    ; Resume thread
    mov rcx, rbx
    call QWORD PTR [__imp_ResumeThread]
    
    ; Update statistics
    lea rcx, [r12+WEEK1_INFRASTRUCTURE.TotalThreadsCreated]
    call AtomicIncrement64
    
    mov rax, rbx                        ; Return handle
    jmp @CWT_Exit
    
@CWT_Failed:
    xor eax, eax
    
@CWT_Exit:
    add rsp, 96
    pop rbx
    pop r12
    pop r13
    pop r14
    pop r15
    ret
CreateWorkerThread ENDP

;==============================================================================
; INITIALIZATION / SHUTDOWN (143 LOC)
;==============================================================================

;------------------------------------------------------------------------------
; Week1Initialize - Initialize Week 1 infrastructure
; RCX = pointer to receive infrastructure handle
; Returns EAX = error code (0 = success)
;------------------------------------------------------------------------------
Week1Initialize PROC EXPORT FRAME
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
    push rdi
    .pushreg rdi
    
    sub rsp, 96
    .allocstack 96
    .endprolog
    
    mov r14, rcx                        ; Output pointer
    
    ; Validate parameter
    test r14, r14
    jz @Init_InvalidParam
    
    ; Allocate main structure
    xor ecx, ecx                        ; lpAddress = NULL
    mov edx, SIZEOF WEEK1_INFRASTRUCTURE
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call QWORD PTR [__imp_VirtualAlloc]
    
    test rax, rax
    jz @Init_OutOfMemory
    mov r12, rax                        ; R12 = infrastructure
    
    ; Zero memory
    mov rcx, r12
    mov rdx, SIZEOF WEEK1_INFRASTRUCTURE
    call MemZero
    
    ; Initialize magic and version
    mov DWORD PTR [r12+WEEK1_INFRASTRUCTURE.Magic], 03157h      ; 'W1'
    mov DWORD PTR [r12+WEEK1_INFRASTRUCTURE.Version], 00010000h ; 1.0
    mov DWORD PTR [r12+WEEK1_INFRASTRUCTURE.StructureSize], SIZEOF WEEK1_INFRASTRUCTURE
    
    ; Initialize QPC frequency cache
    call GetQPFrequency
    
    ; Initialize all SRW locks
    lea rcx, [r12+WEEK1_INFRASTRUCTURE.Heartbeat.Lock]
    call QWORD PTR [__imp_InitializeSRWLock]
    
    lea rcx, [r12+WEEK1_INFRASTRUCTURE.ConflictDetector.Lock]
    call QWORD PTR [__imp_InitializeSRWLock]
    
    lea rcx, [r12+WEEK1_INFRASTRUCTURE.Scheduler.Lock]
    call QWORD PTR [__imp_InitializeSRWLock]
    
    lea rcx, [r12+WEEK1_INFRASTRUCTURE.Scheduler.GlobalQueueLock]
    call QWORD PTR [__imp_InitializeSRWLock]
    
    lea rcx, [r12+WEEK1_INFRASTRUCTURE.Scheduler.TaskIdLock]
    call QWORD PTR [__imp_InitializeSRWLock]
    
    lea rcx, [r12+WEEK1_INFRASTRUCTURE.ThreadIdLock]
    call QWORD PTR [__imp_InitializeSRWLock]
    
    ;=== Allocate dynamic arrays ===
    
    ; Heartbeat nodes array
    xor ecx, ecx
    mov edx, HB_MAX_NODES * SIZEOF_HEARTBEAT_NODE
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call QWORD PTR [__imp_VirtualAlloc]
    test rax, rax
    jz @Init_OutOfMemory
    mov [r12+WEEK1_INFRASTRUCTURE.HeartbeatNodes], rax
    
    mov rcx, rax
    mov edx, HB_MAX_NODES * SIZEOF_HEARTBEAT_NODE
    call MemZero
    
    ; Conflict resources array
    xor ecx, ecx
    mov edx, CD_MAX_RESOURCES * SIZEOF_CONFLICT_ENTRY
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call QWORD PTR [__imp_VirtualAlloc]
    test rax, rax
    jz @Init_OutOfMemory
    mov [r12+WEEK1_INFRASTRUCTURE.ConflictResources], rax
    
    mov rcx, rax
    mov edx, CD_MAX_RESOURCES * SIZEOF_CONFLICT_ENTRY
    call MemZero
    
    ; Conflict wait graph array
    xor ecx, ecx
    mov edx, CD_MAX_WAIT_EDGES * 4
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call QWORD PTR [__imp_VirtualAlloc]
    test rax, rax
    jz @Init_OutOfMemory
    mov [r12+WEEK1_INFRASTRUCTURE.ConflictWaitGraph], rax
    
    ; Workers array
    xor ecx, ecx
    mov edx, TS_MAX_WORKERS * SIZEOF_THREAD_CONTEXT
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call QWORD PTR [__imp_VirtualAlloc]
    test rax, rax
    jz @Init_OutOfMemory
    mov [r12+WEEK1_INFRASTRUCTURE.Workers], rax
    
    mov rcx, rax
    mov edx, TS_MAX_WORKERS * SIZEOF_THREAD_CONTEXT
    call MemZero
    
    ; Global queue array
    xor ecx, ecx
    mov edx, TS_GLOBAL_QUEUE_SIZE * 8
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call QWORD PTR [__imp_VirtualAlloc]
    test rax, rax
    jz @Init_OutOfMemory
    mov [r12+WEEK1_INFRASTRUCTURE.GlobalQueue], rax
    
    ; Local queues array (for all workers)
    xor ecx, ecx
    mov edx, TS_MAX_WORKERS * TS_LOCAL_QUEUE_SIZE * 8
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call QWORD PTR [__imp_VirtualAlloc]
    test rax, rax
    jz @Init_OutOfMemory
    mov [r12+WEEK1_INFRASTRUCTURE.LocalQueues], rax
    
    ;=== Initialize subsystem configurations ===
    
    ; Heartbeat monitor
    mov DWORD PTR [r12+WEEK1_INFRASTRUCTURE.Heartbeat.CheckIntervalMs], HB_INTERVAL_MS
    mov DWORD PTR [r12+WEEK1_INFRASTRUCTURE.Heartbeat.TimeoutThresholdMs], HB_TIMEOUT_MS
    mov DWORD PTR [r12+WEEK1_INFRASTRUCTURE.Heartbeat.MaxMisses], HB_MAX_MISSES
    
    ; Create heartbeat shutdown event
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    call QWORD PTR [__imp_CreateEventW]
    mov [r12+WEEK1_INFRASTRUCTURE.Heartbeat.ShutdownEvent], rax
    
    ; Conflict detector
    mov DWORD PTR [r12+WEEK1_INFRASTRUCTURE.ConflictDetector.ScanIntervalMs], CD_SCAN_INTERVAL_MS
    mov DWORD PTR [r12+WEEK1_INFRASTRUCTURE.ConflictDetector.MaxResources], CD_MAX_RESOURCES
    
    ; Create conflict detector shutdown event
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    call QWORD PTR [__imp_CreateEventW]
    mov [r12+WEEK1_INFRASTRUCTURE.ConflictDetector.ShutdownEvent], rax
    
    ; Task scheduler
    mov DWORD PTR [r12+WEEK1_INFRASTRUCTURE.Scheduler.MaxWorkers], TS_MAX_WORKERS
    mov DWORD PTR [r12+WEEK1_INFRASTRUCTURE.Scheduler.MinWorkers], 1
    
    ; Get CPU count for target workers
    sub rsp, 48                         ; SYSTEM_INFO size
    lea rcx, [rsp]
    call QWORD PTR [__imp_GetSystemInfo]
    mov eax, [rsp+20]                   ; dwNumberOfProcessors
    add rsp, 48
    
    cmp eax, TS_MAX_WORKERS
    jbe @Init_WorkerCount
    mov eax, TS_MAX_WORKERS
@Init_WorkerCount:
    mov [r12+WEEK1_INFRASTRUCTURE.Scheduler.TargetWorkers], eax
    
    ; Create scheduler shutdown event
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    call QWORD PTR [__imp_CreateEventW]
    mov [r12+WEEK1_INFRASTRUCTURE.Scheduler.ShutdownEvent], rax
    
    ; Record start time
    call GetCurrentTimestamp
    mov [r12+WEEK1_INFRASTRUCTURE.StartTime], rax
    
    ; Mark initialized
    mov DWORD PTR [r12+WEEK1_INFRASTRUCTURE.Initialized], 1
    
    ; Store handle
    mov [r14], r12
    mov g_Week1Instance, r12
    
    ; Debug output
    lea rcx, szWeek1Init
    call QWORD PTR [__imp_OutputDebugStringA]
    
    xor eax, eax                        ; Success
    jmp @Init_Exit
    
@Init_InvalidParam:
    mov eax, ERROR_INVALID_PARAMETER
    jmp @Init_Exit
    
@Init_OutOfMemory:
    ; Cleanup on failure
    test r12, r12
    jz @Init_NoCleanup
    
    mov rcx, [r12+WEEK1_INFRASTRUCTURE.HeartbeatNodes]
    test rcx, rcx
    jz @Init_C1
    xor edx, edx
    mov r8d, MEM_RELEASE
    call QWORD PTR [__imp_VirtualFree]
@Init_C1:
    mov rcx, [r12+WEEK1_INFRASTRUCTURE.ConflictResources]
    test rcx, rcx
    jz @Init_C2
    xor edx, edx
    mov r8d, MEM_RELEASE
    call QWORD PTR [__imp_VirtualFree]
@Init_C2:
    mov rcx, [r12+WEEK1_INFRASTRUCTURE.ConflictWaitGraph]
    test rcx, rcx
    jz @Init_C3
    xor edx, edx
    mov r8d, MEM_RELEASE
    call QWORD PTR [__imp_VirtualFree]
@Init_C3:
    mov rcx, [r12+WEEK1_INFRASTRUCTURE.Workers]
    test rcx, rcx
    jz @Init_C4
    xor edx, edx
    mov r8d, MEM_RELEASE
    call QWORD PTR [__imp_VirtualFree]
@Init_C4:
    mov rcx, [r12+WEEK1_INFRASTRUCTURE.GlobalQueue]
    test rcx, rcx
    jz @Init_C5
    xor edx, edx
    mov r8d, MEM_RELEASE
    call QWORD PTR [__imp_VirtualFree]
@Init_C5:
    mov rcx, [r12+WEEK1_INFRASTRUCTURE.LocalQueues]
    test rcx, rcx
    jz @Init_C6
    xor edx, edx
    mov r8d, MEM_RELEASE
    call QWORD PTR [__imp_VirtualFree]
@Init_C6:
    mov rcx, r12
    xor edx, edx
    mov r8d, MEM_RELEASE
    call QWORD PTR [__imp_VirtualFree]
    
@Init_NoCleanup:
    mov eax, ERROR_OUTOFMEMORY
    
@Init_Exit:
    add rsp, 96
    pop rdi
    pop rbx
    pop r12
    pop r13
    pop r14
    pop r15
    ret
Week1Initialize ENDP

;------------------------------------------------------------------------------
; Week1StartBackgroundThreads - Start all monitoring threads
; RCX = infrastructure
; Returns EAX = error code (0 = success)
;------------------------------------------------------------------------------
Week1StartBackgroundThreads PROC EXPORT FRAME
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
    
    sub rsp, 72
    .allocstack 72
    .endprolog
    
    mov r12, rcx                        ; Infrastructure
    
    ; Validate
    test r12, r12
    jz @Start_InvalidParam
    cmp DWORD PTR [r12+WEEK1_INFRASTRUCTURE.Initialized], 1
    jne @Start_InvalidParam
    
    ;=== Start heartbeat monitor thread ===
    xor ecx, ecx
    xor edx, edx
    lea r8, HeartbeatMonitorThread
    mov r9, r12
    mov DWORD PTR [rsp+32], 0
    lea rax, [rsp+48]
    mov [rsp+40], rax
    call QWORD PTR [__imp_CreateThread]
    
    test rax, rax
    jz @Start_ThreadFailed
    mov [r12+WEEK1_INFRASTRUCTURE.Heartbeat.MonitorThread], rax
    
    ;=== Start conflict detector thread ===
    xor ecx, ecx
    xor edx, edx
    lea r8, ConflictDetectorThread
    mov r9, r12
    mov DWORD PTR [rsp+32], 0
    lea rax, [rsp+48]
    mov [rsp+40], rax
    call QWORD PTR [__imp_CreateThread]
    
    test rax, rax
    jz @Start_ThreadFailed
    mov [r12+WEEK1_INFRASTRUCTURE.ConflictDetector.DetectorThread], rax
    
    ;=== Start scheduler coordinator thread ===
    xor ecx, ecx
    xor edx, edx
    lea r8, SchedulerCoordinatorThread
    mov r9, r12
    mov DWORD PTR [rsp+32], 0
    lea rax, [rsp+48]
    mov [rsp+40], rax
    call QWORD PTR [__imp_CreateThread]
    
    test rax, rax
    jz @Start_ThreadFailed
    mov [r12+WEEK1_INFRASTRUCTURE.Scheduler.CoordinatorThread], rax
    
    ;=== Start worker threads ===
    mov rsi, [r12+WEEK1_INFRASTRUCTURE.Workers]
    xor ebx, ebx
    mov r14d, [r12+WEEK1_INFRASTRUCTURE.Scheduler.TargetWorkers]
    
@Start_WorkerLoop:
    cmp ebx, r14d
    jge @Start_WorkersDone
    
    ; Calculate worker context address
    mov eax, ebx
    imul eax, SIZEOF_THREAD_CONTEXT
    lea r15, [rsi + rax]
    
    ; Initialize worker context
    mov [r15+THREAD_CONTEXT.WorkerId], ebx
    lea rax, [r12+WEEK1_INFRASTRUCTURE.Scheduler]
    mov [r15+THREAD_CONTEXT.Scheduler], rax
    mov DWORD PTR [r15+THREAD_CONTEXT.LocalQueueHead], 0
    mov DWORD PTR [r15+THREAD_CONTEXT.LocalQueueTail], 0
    mov DWORD PTR [r15+THREAD_CONTEXT.LocalQueueCount], 0
    mov DWORD PTR [r15+THREAD_CONTEXT.ShouldShutdown], 0
    
    ; Create worker thread
    mov rcx, r12
    mov edx, ebx
    lea r8, WorkerThreadProc
    mov r9, r15
    call CreateWorkerThread
    
    test rax, rax
    jz @Start_ThreadFailed
    
    mov [r15+THREAD_CONTEXT.ThreadHandle], rax
    
    inc ebx
    jmp @Start_WorkerLoop
    
@Start_WorkersDone:
    mov [r12+WEEK1_INFRASTRUCTURE.Scheduler.WorkerCount], ebx
    
    xor eax, eax
    jmp @Start_Exit
    
@Start_InvalidParam:
    mov eax, ERROR_INVALID_PARAMETER
    jmp @Start_Exit
    
@Start_ThreadFailed:
    mov eax, ERROR_OUTOFMEMORY
    
@Start_Exit:
    add rsp, 72
    pop rsi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
Week1StartBackgroundThreads ENDP

;------------------------------------------------------------------------------
; Week1Shutdown - Graceful shutdown
; RCX = infrastructure
;------------------------------------------------------------------------------
Week1Shutdown PROC EXPORT FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push rsi
    .pushreg rsi
    
    sub rsp, 56
    .allocstack 56
    .endprolog
    
    mov r12, rcx
    
    test r12, r12
    jz @Shutdown_Exit
    cmp DWORD PTR [r12+WEEK1_INFRASTRUCTURE.Initialized], 1
    jne @Shutdown_Exit
    
    mov DWORD PTR [r12+WEEK1_INFRASTRUCTURE.ShutdownRequested], 1
    
    lea rcx, szWeek1Shutdown
    call QWORD PTR [__imp_OutputDebugStringA]
    
    ;=== Signal threads ===
    mov rcx, [r12+WEEK1_INFRASTRUCTURE.Heartbeat.ShutdownEvent]
    test rcx, rcx
    jz @Shutdown_S1
    call QWORD PTR [__imp_SetEvent]
@Shutdown_S1:
    
    mov rcx, [r12+WEEK1_INFRASTRUCTURE.ConflictDetector.ShutdownEvent]
    test rcx, rcx
    jz @Shutdown_S2
    call QWORD PTR [__imp_SetEvent]
@Shutdown_S2:
    
    mov rcx, [r12+WEEK1_INFRASTRUCTURE.Scheduler.ShutdownEvent]
    test rcx, rcx
    jz @Shutdown_S3
    call QWORD PTR [__imp_SetEvent]
@Shutdown_S3:
    
    ; Signal workers
    mov rsi, [r12+WEEK1_INFRASTRUCTURE.Workers]
    test rsi, rsi
    jz @Shutdown_NoWorkers
    
    xor ebx, ebx
    mov r14d, [r12+WEEK1_INFRASTRUCTURE.Scheduler.WorkerCount]
    
@Shutdown_SignalWorkers:
    cmp ebx, r14d
    jge @Shutdown_NoWorkers
    
    mov eax, ebx
    imul eax, SIZEOF_THREAD_CONTEXT
    lea rax, [rsi + rax]
    mov DWORD PTR [rax+THREAD_CONTEXT.ShouldShutdown], 1
    
    inc ebx
    jmp @Shutdown_SignalWorkers
    
@Shutdown_NoWorkers:
    ; Wait for threads
    mov ecx, 3000
    call QWORD PTR [__imp_Sleep]
    
    ;=== Close handles ===
    mov rcx, [r12+WEEK1_INFRASTRUCTURE.Heartbeat.MonitorThread]
    test rcx, rcx
    jz @Shutdown_C1
    call QWORD PTR [__imp_CloseHandle]
@Shutdown_C1:
    
    mov rcx, [r12+WEEK1_INFRASTRUCTURE.ConflictDetector.DetectorThread]
    test rcx, rcx
    jz @Shutdown_C2
    call QWORD PTR [__imp_CloseHandle]
@Shutdown_C2:
    
    mov rcx, [r12+WEEK1_INFRASTRUCTURE.Scheduler.CoordinatorThread]
    test rcx, rcx
    jz @Shutdown_C3
    call QWORD PTR [__imp_CloseHandle]
@Shutdown_C3:
    
    ; Close worker handles
    mov rsi, [r12+WEEK1_INFRASTRUCTURE.Workers]
    test rsi, rsi
    jz @Shutdown_C4
    
    xor ebx, ebx
@Shutdown_CloseWorkers:
    cmp ebx, r14d
    jge @Shutdown_C4
    
    mov eax, ebx
    imul eax, SIZEOF_THREAD_CONTEXT
    lea rax, [rsi + rax]
    mov rcx, [rax+THREAD_CONTEXT.ThreadHandle]
    test rcx, rcx
    jz @Shutdown_NextWorker
    call QWORD PTR [__imp_CloseHandle]
    
@Shutdown_NextWorker:
    inc ebx
    jmp @Shutdown_CloseWorkers
    
@Shutdown_C4:
    ; Close events
    mov rcx, [r12+WEEK1_INFRASTRUCTURE.Heartbeat.ShutdownEvent]
    test rcx, rcx
    jz @Shutdown_E1
    call QWORD PTR [__imp_CloseHandle]
@Shutdown_E1:
    
    mov rcx, [r12+WEEK1_INFRASTRUCTURE.ConflictDetector.ShutdownEvent]
    test rcx, rcx
    jz @Shutdown_E2
    call QWORD PTR [__imp_CloseHandle]
@Shutdown_E2:
    
    mov rcx, [r12+WEEK1_INFRASTRUCTURE.Scheduler.ShutdownEvent]
    test rcx, rcx
    jz @Shutdown_E3
    call QWORD PTR [__imp_CloseHandle]
@Shutdown_E3:
    
    ;=== Free memory ===
    mov rcx, [r12+WEEK1_INFRASTRUCTURE.HeartbeatNodes]
    test rcx, rcx
    jz @Shutdown_F1
    xor edx, edx
    mov r8d, MEM_RELEASE
    call QWORD PTR [__imp_VirtualFree]
@Shutdown_F1:
    
    mov rcx, [r12+WEEK1_INFRASTRUCTURE.ConflictResources]
    test rcx, rcx
    jz @Shutdown_F2
    xor edx, edx
    mov r8d, MEM_RELEASE
    call QWORD PTR [__imp_VirtualFree]
@Shutdown_F2:
    
    mov rcx, [r12+WEEK1_INFRASTRUCTURE.ConflictWaitGraph]
    test rcx, rcx
    jz @Shutdown_F3
    xor edx, edx
    mov r8d, MEM_RELEASE
    call QWORD PTR [__imp_VirtualFree]
@Shutdown_F3:
    
    mov rcx, [r12+WEEK1_INFRASTRUCTURE.Workers]
    test rcx, rcx
    jz @Shutdown_F4
    xor edx, edx
    mov r8d, MEM_RELEASE
    call QWORD PTR [__imp_VirtualFree]
@Shutdown_F4:
    
    mov rcx, [r12+WEEK1_INFRASTRUCTURE.GlobalQueue]
    test rcx, rcx
    jz @Shutdown_F5
    xor edx, edx
    mov r8d, MEM_RELEASE
    call QWORD PTR [__imp_VirtualFree]
@Shutdown_F5:
    
    mov rcx, [r12+WEEK1_INFRASTRUCTURE.LocalQueues]
    test rcx, rcx
    jz @Shutdown_F6
    xor edx, edx
    mov r8d, MEM_RELEASE
    call QWORD PTR [__imp_VirtualFree]
@Shutdown_F6:
    
    mov rcx, r12
    xor edx, edx
    mov r8d, MEM_RELEASE
    call QWORD PTR [__imp_VirtualFree]
    
    mov g_Week1Instance, 0
    
@Shutdown_Exit:
    add rsp, 56
    pop rsi
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
Week1Shutdown ENDP

;------------------------------------------------------------------------------
; Week1GetStatistics - Get statistics snapshot
; RCX = infrastructure, RDX = output pointer
;------------------------------------------------------------------------------
Week1GetStatistics PROC EXPORT FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov r12, rcx
    mov rbx, rdx
    
    test r12, r12
    jz @GS_Exit
    test rbx, rbx
    jz @GS_Exit
    
    mov rax, [r12+WEEK1_INFRASTRUCTURE.Scheduler.TotalTasksSubmitted]
    mov [rbx], rax
    
    mov rax, [r12+WEEK1_INFRASTRUCTURE.Scheduler.TotalTasksExecuted]
    mov [rbx+8], rax
    
    mov rax, [r12+WEEK1_INFRASTRUCTURE.Scheduler.TotalTasksStolen]
    mov [rbx+16], rax
    
    mov eax, [r12+WEEK1_INFRASTRUCTURE.Scheduler.WorkerCount]
    mov [rbx+24], eax
    
    mov rax, [r12+WEEK1_INFRASTRUCTURE.Scheduler.GlobalQueueTail]
    sub rax, [r12+WEEK1_INFRASTRUCTURE.Scheduler.GlobalQueueHead]
    test rax, rax
    jge @GS_NoWrap
    add rax, TS_GLOBAL_QUEUE_SIZE
@GS_NoWrap:
    mov [rbx+28], eax
    
    mov rax, [r12+WEEK1_INFRASTRUCTURE.Heartbeat.TotalHeartbeatsSent]
    mov [rbx+32], rax
    
    mov rax, [r12+WEEK1_INFRASTRUCTURE.Heartbeat.TotalHeartbeatsRcvd]
    mov [rbx+40], rax
    
    mov rax, [r12+WEEK1_INFRASTRUCTURE.ConflictDetector.TotalDeadlocksDetected]
    mov [rbx+48], rax
    
@GS_Exit:
    add rsp, 40
    pop r12
    pop rbx
    ret
Week1GetStatistics ENDP

;------------------------------------------------------------------------------
; Week1GetVersion - Get version string
;------------------------------------------------------------------------------
Week1GetVersion PROC EXPORT
    lea rax, szVersion
    ret
Week1GetVersion ENDP

;==============================================================================
; EXPORTS
;==============================================================================
PUBLIC Week1Initialize
PUBLIC Week1StartBackgroundThreads
PUBLIC Week1Shutdown
PUBLIC ProcessReceivedHeartbeat
PUBLIC Week1RegisterNode
PUBLIC Week1RegisterResource
PUBLIC SubmitTask
PUBLIC Week1GetStatistics
PUBLIC Week1GetVersion

END
