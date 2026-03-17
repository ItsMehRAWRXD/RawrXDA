; =============================================================================
; RawrXD MultiWindow Task Scheduler Kernel
; =============================================================================
; Custom MASM64 kernel driver for multi-window IDE task scheduling
; Manages concurrent AI sessions, priority queues, memory-mapped IPC,
; and lock-free ring buffers for inter-window communication.
;
; Build: ml64 /c /Fo RawrXD_MultiWindow_Kernel.obj RawrXD_MultiWindow_Kernel.asm
; Link:  link /DLL /DEF:MultiWindowKernel.def RawrXD_MultiWindow_Kernel.obj
; =============================================================================

OPTION CASEMAP:NONE

; --- Windows API Imports ---
EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN VirtualProtect:PROC
EXTERN CreateThread:PROC
EXTERN WaitForSingleObject:PROC
EXTERN WaitForMultipleObjects:PROC
EXTERN CloseHandle:PROC
EXTERN InitializeCriticalSection:PROC
EXTERN EnterCriticalSection:PROC
EXTERN LeaveCriticalSection:PROC
EXTERN DeleteCriticalSection:PROC
EXTERN CreateEventW:PROC
EXTERN SetEvent:PROC
EXTERN ResetEvent:PROC
EXTERN CreateFileMappingW:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN InterlockedIncrement:PROC
EXTERN InterlockedDecrement:PROC
EXTERN InterlockedCompareExchange:PROC
EXTERN InterlockedExchange:PROC
EXTERN GetTickCount64:PROC
EXTERN QueryPerformanceCounter:PROC
EXTERN QueryPerformanceFrequency:PROC
EXTERN GetCurrentThreadId:PROC
EXTERN SwitchToThread:PROC
EXTERN Sleep:PROC
EXTERN OutputDebugStringA:PROC
EXTERN RtlZeroMemory:PROC
EXTERN RtlCopyMemory:PROC
EXTERN CreateIoCompletionPort:PROC
EXTERN PostQueuedCompletionStatus:PROC
EXTERN GetQueuedCompletionStatus:PROC

; --- Constants ---
HEAP_ZERO_MEMORY        EQU 00000008h
MEM_COMMIT              EQU 00001000h
MEM_RESERVE             EQU 00002000h
MEM_RELEASE             EQU 00008000h
PAGE_READWRITE          EQU 00000004h
PAGE_EXECUTE_READWRITE  EQU 00000040h
INFINITE_WAIT           EQU 0FFFFFFFFh
FILE_MAP_ALL_ACCESS     EQU 000F001Fh
INVALID_HANDLE          EQU 0FFFFFFFFFFFFFFFFh

; --- Task Types (matches HTML IDE TaskType enum) ---
TASK_CHAT               EQU 0
TASK_AUDIT              EQU 1
TASK_COT                EQU 2   ; Chain of Thought
TASK_SWARM              EQU 3
TASK_CODE_EDIT          EQU 4
TASK_TERMINAL           EQU 5
TASK_FILE_BROWSE        EQU 6
TASK_MODEL_MANAGE       EQU 7
TASK_PERF_MONITOR       EQU 8
TASK_HOTPATCH           EQU 9
TASK_REVERSE_ENG        EQU 10
TASK_DEBUG              EQU 11
TASK_BENCHMARK          EQU 12
TASK_COMPILE            EQU 13
TASK_DEPLOY             EQU 14
TASK_CUSTOM             EQU 15
MAX_TASK_TYPES          EQU 16

; --- Task States ---
STATE_IDLE              EQU 0
STATE_QUEUED            EQU 1
STATE_RUNNING           EQU 2
STATE_PAUSED            EQU 3
STATE_COMPLETE          EQU 4
STATE_FAILED            EQU 5
STATE_CANCELLED         EQU 6

; --- Priority Levels ---
PRIORITY_CRITICAL       EQU 0
PRIORITY_HIGH           EQU 1
PRIORITY_NORMAL         EQU 2
PRIORITY_LOW            EQU 3
PRIORITY_BACKGROUND     EQU 4
MAX_PRIORITIES          EQU 5

; --- Ring Buffer Config ---
RING_BUFFER_SIZE        EQU 4096        ; entries per ring
RING_ENTRY_SIZE         EQU 256         ; bytes per entry
MAX_WINDOWS             EQU 64          ; max concurrent windows
MAX_TASKS               EQU 1024        ; max concurrent tasks
MAX_WORKER_THREADS      EQU 16          ; max worker threads
IPC_SHARED_SIZE         EQU 1048576     ; 1MB shared memory

; --- Shared Memory Layout (Formalized - Fix #2) ---
; Each priority queue gets RING_BUFFER_SIZE * 8 bytes (4096 * 8 = 32KB)
; Total queue region = MAX_PRIORITIES * 32KB = 160KB
; Replay log region starts after queues
SHM_QUEUE_STRIDE        EQU (RING_BUFFER_SIZE * 8)     ; 32768 bytes per priority
SHM_QUEUE_REGION        EQU (MAX_PRIORITIES * SHM_QUEUE_STRIDE) ; 163840 bytes
SHM_REPLAY_BASE         EQU SHM_QUEUE_REGION           ; replay log starts here
SHM_REPLAY_ENTRY        EQU 64                          ; bytes per replay entry
SHM_REPLAY_MAX          EQU 8192                        ; max replay entries
SHM_REPLAY_SIZE         EQU (SHM_REPLAY_MAX * SHM_REPLAY_ENTRY) ; 524288 bytes

; =============================================================================
; DATA STRUCTURES
; =============================================================================

.DATA

; --- Kernel State ---
ALIGN 16
g_KernelInitialized     DQ 0            ; atomic init flag
g_KernelHeap            DQ 0            ; process heap handle
g_TaskIdCounter         DQ 0            ; monotonic task ID
g_WindowIdCounter       DQ 0            ; monotonic window ID
g_ActiveTaskCount       DD 0            ; current active tasks
g_ActiveWindowCount     DD 0            ; current open windows
g_TotalTasksProcessed   DQ 0            ; lifetime counter
g_TotalTasksFailed      DQ 0            ; lifetime failures
g_KernelStartTick       DQ 0            ; startup timestamp
g_PerfFrequency         DQ 0            ; QPC frequency

; --- Replay Log State ---
ALIGN 16
g_ReplayHead            DQ 0            ; next write position in replay ring
g_ReplayEnabled         DQ 1            ; 1 = logging on, 0 = off

; --- IOCP Handle (replaces auto-reset event for worker wake) ---
g_IOCPHandle            DQ 0            ; I/O Completion Port handle

; --- Worker Thread Pool ---
ALIGN 16
g_WorkerHandles         DQ MAX_WORKER_THREADS DUP(0)
g_WorkerIds             DD MAX_WORKER_THREADS DUP(0)
g_WorkerCount           DD 0
g_WorkerShutdown        DQ 0            ; atomic shutdown flag
g_WorkerWakeEvent       DQ 0            ; event to wake workers

; --- Priority Queues (lock-free ring buffers) ---
; Each priority level has its own ring buffer
ALIGN 64
g_PQ_Head               DD MAX_PRIORITIES DUP(0)    ; write position
g_PQ_Tail               DD MAX_PRIORITIES DUP(0)    ; read position
g_PQ_Count              DD MAX_PRIORITIES DUP(0)    ; item count

; --- Window Registry ---
ALIGN 16
g_WindowSlots           DQ MAX_WINDOWS DUP(0)       ; pointers to WindowState
g_WindowActive          DB MAX_WINDOWS DUP(0)       ; active flags

; --- IPC Shared Memory ---
g_SharedMemHandle       DQ 0
g_SharedMemBase         DQ 0

; --- Critical Sections (40 bytes each on x64) ---
ALIGN 16
g_TaskQueueCS           DB 48 DUP(0)
g_WindowRegistryCS      DB 48 DUP(0)
g_IPCBufferCS           DB 48 DUP(0)
g_StatsCS               DB 48 DUP(0)

; --- Debug/Trace Strings ---
szKernelInit            DB "RawrXD MultiWindow Kernel: Initialized", 0Ah, 0
szKernelShutdown        DB "RawrXD MultiWindow Kernel: Shutdown", 0Ah, 0
szTaskQueued            DB "RawrXD Kernel: Task Queued", 0Ah, 0
szTaskStarted           DB "RawrXD Kernel: Task Started", 0Ah, 0
szTaskCompleted         DB "RawrXD Kernel: Task Completed", 0Ah, 0
szWindowCreated         DB "RawrXD Kernel: Window Created", 0Ah, 0
szWindowDestroyed       DB "RawrXD Kernel: Window Destroyed", 0Ah, 0
szWorkerStarted         DB "RawrXD Kernel: Worker Thread Started", 0Ah, 0

; --- IPC Shared Memory Name ---
szSharedMemName         DW 'R','a','w','r','X','D','_','M','W','_','I','P','C', 0

; =============================================================================
; TASK DESCRIPTOR (128 bytes, cache-line aligned)
; =============================================================================
; Offset  Size  Field
; 0x00    8     task_id
; 0x08    4     task_type (TASK_*)
; 0x0C    4     task_state (STATE_*)
; 0x10    4     priority (PRIORITY_*)
; 0x14    4     window_id (owner window)
; 0x18    8     callback_fn (function pointer)
; 0x20    8     user_data (opaque pointer)
; 0x28    8     model_id (which model to use)
; 0x30    8     submit_time (QPC tick)
; 0x38    8     start_time
; 0x40    8     end_time
; 0x48    8     result_ptr (output buffer)
; 0x50    4     result_size
; 0x54    4     error_code
; 0x58    8     next_task (linked list)
; 0x60    8     depends_on (dependency task_id, 0=none)
; 0x68    8     progress (0-10000 = 0.00%-100.00%)
; 0x70    8     flags (bitfield)
; 0x78    8     reserved

TASK_DESC_SIZE          EQU 128

; =============================================================================
; WINDOW STATE (256 bytes)
; =============================================================================
; Offset  Size  Field
; 0x00    8     window_id
; 0x08    4     window_type (TASK_* reused)
; 0x0C    4     window_state (0=hidden, 1=normal, 2=maximized, 3=minimized)
; 0x10    4     pos_x
; 0x14    4     pos_y
; 0x18    4     width
; 0x1C    4     height
; 0x20    4     z_order
; 0x24    4     flags
; 0x28    8     model_id (selected model for this window)
; 0x30    8     active_task_id
; 0x38    8     task_count (total tasks in this window)
; 0x40    8     create_time
; 0x48    64    title (ASCII, null-terminated)
; 0x88    8     parent_window_id (0=root)
; 0x90    8     ring_buffer_ptr (per-window message ring)
; 0x98    8     ring_head
; 0xA0    8     ring_tail
; 0xA8    56    reserved

WINDOW_STATE_SIZE       EQU 256

; =============================================================================
; RING BUFFER ENTRY (256 bytes)
; =============================================================================
; Offset  Size  Field
; 0x00    4     msg_type
; 0x04    4     src_window
; 0x08    4     dst_window
; 0x0C    4     payload_size
; 0x10    8     timestamp
; 0x18    8     sequence_num
; 0x20    224   payload

RING_ENTRY_TOTAL        EQU 256

; =============================================================================
; IPC MESSAGE TYPES
; =============================================================================
MSG_TASK_SUBMIT         EQU 1
MSG_TASK_CANCEL         EQU 2
MSG_TASK_STATUS         EQU 3
MSG_TASK_RESULT         EQU 4
MSG_WINDOW_FOCUS        EQU 5
MSG_WINDOW_RESIZE       EQU 6
MSG_WINDOW_CLOSE        EQU 7
MSG_MODEL_SWITCH        EQU 8
MSG_SWARM_BROADCAST     EQU 9
MSG_COT_CHAIN           EQU 10
MSG_AUDIT_REQUEST       EQU 11
MSG_HOTPATCH_APPLY      EQU 12
MSG_PERF_SNAPSHOT       EQU 13
MSG_STREAM_CHUNK        EQU 14
MSG_HEARTBEAT           EQU 15
MSG_SHUTDOWN            EQU 16

.CODE

; =============================================================================
; KERNEL INITIALIZATION
; =============================================================================
; BOOL __fastcall KernelInit(DWORD workerCount)
; rcx = workerCount (0 = auto-detect)
; Returns: 1 on success, 0 on failure
; =============================================================================
ALIGN 16
KernelInit PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 88                         ; shadow + locals

    mov     r12d, ecx                       ; save workerCount

    ; --- Atomic init check (compare-exchange) --- FIX #1: cmpxchg uses RAX
    lea     rcx, [g_KernelInitialized]
    xor     eax, eax                        ; expected = 0 (cmpxchg compares RAX)
    mov     r8, 1                           ; desired = 1
    lock cmpxchg QWORD PTR [rcx], r8
    jnz     @already_init                   ; already initialized

    ; --- Get process heap ---
    call    GetProcessHeap
    test    rax, rax
    jz      @init_fail
    mov     [g_KernelHeap], rax

    ; --- Record start time ---
    call    GetTickCount64
    mov     [g_KernelStartTick], rax

    ; --- Get perf frequency ---
    lea     rcx, [g_PerfFrequency]
    call    QueryPerformanceFrequency

    ; --- Initialize critical sections ---
    lea     rcx, [g_TaskQueueCS]
    call    InitializeCriticalSection
    lea     rcx, [g_WindowRegistryCS]
    call    InitializeCriticalSection
    lea     rcx, [g_IPCBufferCS]
    call    InitializeCriticalSection
    lea     rcx, [g_StatsCS]
    call    InitializeCriticalSection

    ; --- Create worker wake event (auto-reset, kept for shutdown signaling) ---
    xor     ecx, ecx                        ; lpSecurityAttribs = NULL
    xor     edx, edx                        ; bManualReset = FALSE
    xor     r8d, r8d                        ; bInitialState = FALSE
    xor     r9d, r9d                        ; lpName = NULL
    call    CreateEventW
    test    rax, rax
    jz      @init_fail
    mov     [g_WorkerWakeEvent], rax

    ; --- Create IOCP for worker wake (FIX #6 & #14: replaces polling) ---
    mov     rcx, INVALID_HANDLE             ; FileHandle (none)
    xor     edx, edx                        ; ExistingCompletionPort (create new)
    xor     r8, r8                          ; CompletionKey
    xor     r9d, r9d                        ; NumberOfConcurrentThreads (0=auto)
    call    CreateIoCompletionPort
    test    rax, rax
    jz      @init_fail
    mov     [g_IOCPHandle], rax

    ; --- Setup IPC shared memory ---
    call    SetupSharedMemory
    test    eax, eax
    jz      @init_fail

    ; --- Allocate priority queue ring buffers ---
    call    AllocatePriorityQueues
    test    eax, eax
    jz      @init_fail

    ; --- Determine worker count ---
    test    r12d, r12d
    jnz     @use_specified
    mov     r12d, 4                         ; default 4 workers
@use_specified:
    cmp     r12d, MAX_WORKER_THREADS
    jbe     @count_ok
    mov     r12d, MAX_WORKER_THREADS
@count_ok:
    mov     [g_WorkerCount], r12d

    ; --- Spawn worker threads ---
    xor     r13d, r13d                      ; thread index
@spawn_loop:
    cmp     r13d, r12d
    jge     @spawn_done

    ; CreateThread(NULL, 0, WorkerThreadProc, index, 0, &threadId)
    xor     ecx, ecx                        ; lpSecurityAttribs
    xor     edx, edx                        ; dwStackSize (default)
    lea     r8, [WorkerThreadProc]           ; lpStartAddress
    mov     r9, r13                          ; lpParameter = index
    mov     QWORD PTR [rsp+32], 0           ; dwCreationFlags
    lea     rax, [g_WorkerIds]
    lea     rax, [rax + r13*4]
    mov     QWORD PTR [rsp+40], rax         ; lpThreadId
    call    CreateThread
    test    rax, rax
    jz      @init_fail

    lea     rcx, [g_WorkerHandles]
    mov     [rcx + r13*8], rax

    inc     r13d
    jmp     @spawn_loop

@spawn_done:
    ; --- Debug trace ---
    lea     rcx, [szKernelInit]
    call    OutputDebugStringA

    mov     eax, 1                          ; success
    jmp     @init_exit

@already_init:
    mov     eax, 1                          ; already initialized = OK
    jmp     @init_exit

@init_fail:
    xor     eax, eax

@init_exit:
    add     rsp, 88
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
KernelInit ENDP

; =============================================================================
; KERNEL SHUTDOWN
; =============================================================================
ALIGN 16
KernelShutdown PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 48

    ; --- Signal shutdown ---
    mov     QWORD PTR [g_WorkerShutdown], 1

    ; --- Wake all workers ---
    mov     r12d, [g_WorkerCount]
    xor     ebx, ebx
@wake_loop:
    cmp     ebx, r12d
    jge     @wake_done
    mov     rcx, [g_WorkerWakeEvent]
    call    SetEvent
    inc     ebx
    jmp     @wake_loop
@wake_done:

    ; --- Wait for workers to exit (5 second timeout per thread) ---
    xor     ebx, ebx
@wait_loop:
    cmp     ebx, r12d
    jge     @wait_done
    lea     rcx, [g_WorkerHandles]
    mov     rcx, [rcx + rbx*8]
    test    rcx, rcx
    jz      @wait_next
    mov     edx, 5000                       ; 5 sec timeout
    call    WaitForSingleObject
    lea     rcx, [g_WorkerHandles]
    mov     rcx, [rcx + rbx*8]
    call    CloseHandle
@wait_next:
    inc     ebx
    jmp     @wait_loop
@wait_done:

    ; --- Cleanup shared memory ---
    mov     rcx, [g_SharedMemBase]
    test    rcx, rcx
    jz      @no_unmap
    call    UnmapViewOfFile
@no_unmap:
    mov     rcx, [g_SharedMemHandle]
    test    rcx, rcx
    jz      @no_close_shm
    call    CloseHandle
@no_close_shm:

    ; --- Close IOCP handle ---
    mov     rcx, [g_IOCPHandle]
    test    rcx, rcx
    jz      @no_close_iocp
    call    CloseHandle
@no_close_iocp:

    ; --- Close wake event ---
    mov     rcx, [g_WorkerWakeEvent]
    test    rcx, rcx
    jz      @no_close_evt
    call    CloseHandle
@no_close_evt:

    ; --- Delete critical sections ---
    lea     rcx, [g_TaskQueueCS]
    call    DeleteCriticalSection
    lea     rcx, [g_WindowRegistryCS]
    call    DeleteCriticalSection
    lea     rcx, [g_IPCBufferCS]
    call    DeleteCriticalSection
    lea     rcx, [g_StatsCS]
    call    DeleteCriticalSection

    ; --- Free all window states ---
    call    FreeAllWindows

    ; --- Mark uninitialized ---
    mov     QWORD PTR [g_KernelInitialized], 0

    lea     rcx, [szKernelShutdown]
    call    OutputDebugStringA

    add     rsp, 48
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
KernelShutdown ENDP

; =============================================================================
; SUBMIT TASK
; =============================================================================
; UINT64 __fastcall SubmitTask(
;   DWORD taskType,    ; rcx
;   DWORD priority,    ; rdx
;   DWORD windowId,    ; r8d
;   UINT64 callbackFn, ; r9
;   UINT64 userData,    ; [rsp+40]
;   UINT64 modelId,    ; [rsp+48]
;   UINT64 dependsOn   ; [rsp+56]
; )
; Returns: task_id (0 on failure)
; =============================================================================
ALIGN 16
SubmitTask PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 72

    ; --- Save params ---
    mov     r12d, ecx                       ; taskType
    mov     r13d, edx                       ; priority
    mov     r14d, r8d                       ; windowId
    mov     r15, r9                         ; callbackFn

    ; --- Validate ---
    cmp     r12d, MAX_TASK_TYPES
    jge     @submit_fail
    cmp     r13d, MAX_PRIORITIES
    jge     @submit_fail

    ; --- Allocate task descriptor ---
    mov     rcx, [g_KernelHeap]
    mov     edx, HEAP_ZERO_MEMORY
    mov     r8d, TASK_DESC_SIZE
    call    HeapAlloc
    test    rax, rax
    jz      @submit_fail
    mov     rbx, rax                        ; rbx = task descriptor

    ; --- Assign task ID (atomic increment) ---
    lea     rcx, [g_TaskIdCounter]
    mov     rax, 1
    lock xadd QWORD PTR [rcx], rax
    inc     rax                             ; ID starts at 1
    mov     [rbx], rax                      ; task_id
    mov     rsi, rax                        ; save task_id for return

    ; --- Fill descriptor ---
    mov     DWORD PTR [rbx+08h], r12d       ; task_type
    mov     DWORD PTR [rbx+0Ch], STATE_QUEUED ; task_state
    mov     DWORD PTR [rbx+10h], r13d       ; priority
    mov     DWORD PTR [rbx+14h], r14d       ; window_id
    mov     [rbx+18h], r15                  ; callback_fn

    ; Load stack params
    mov     rax, [rsp+72+56+40]             ; userData
    mov     [rbx+20h], rax
    mov     rax, [rsp+72+56+48]             ; modelId
    mov     [rbx+28h], rax
    mov     rax, [rsp+72+56+56]             ; dependsOn
    mov     [rbx+60h], rax

    ; --- Record submit time ---
    lea     rcx, [rbx+30h]
    call    QueryPerformanceCounter

    ; --- Enqueue to priority queue ---
    lea     rcx, [g_TaskQueueCS]
    call    EnterCriticalSection

    ; --- FIX #2/#3/#7: Correct ring indexing with priority isolation and overflow ---
    mov     eax, r13d                        ; priority index
    lea     rcx, [g_PQ_Head]
    mov     edx, [rcx + rax*4]              ; current head for this priority

    ; Overflow check (FIX #7): if (head+1) mod SIZE == tail, queue is full
    lea     ecx, [edx + 1]
    and     ecx, (RING_BUFFER_SIZE - 1)      ; next_head
    lea     r8, [g_PQ_Tail]
    cmp     ecx, [r8 + rax*4]               ; compare next_head vs tail
    je      @queue_full                      ; ring buffer full - reject task

    ; Calculate base address: shared_base + (priority * SHM_QUEUE_STRIDE)
    mov     rcx, [g_SharedMemBase]
    test    rcx, rcx
    jz      @queue_full
    mov     r8, rax                          ; save priority index
    imul    rax, SHM_QUEUE_STRIDE            ; priority * 32768
    add     rcx, rax                         ; rcx = base of this priority's ring

    ; Store task pointer at slot [base + head*8]
    mov     [rcx + rdx*8], rbx               ; write task ptr to isolated ring slot

    ; Memory fence: ensure store is visible before advancing head (FIX #8)
    sfence

    ; Advance head atomically
    lea     rax, [edx + 1]
    and     eax, (RING_BUFFER_SIZE - 1)
    lea     rcx, [g_PQ_Head]
    mov     [rcx + r8*4], eax                ; update head

    ; Increment count
    lea     rcx, [g_PQ_Count]
    lock inc DWORD PTR [rcx + r8*4]

    ; Increment active count
    lock inc DWORD PTR [g_ActiveTaskCount]

    ; --- Write replay log entry (FIX #15: deterministic replay) ---
    cmp     QWORD PTR [g_ReplayEnabled], 0
    je      @skip_replay_submit
    push    r8
    mov     ecx, 1                           ; replay op = SUBMIT
    mov     edx, r13d                        ; priority
    mov     r8, rsi                          ; task_id
    call    WriteReplayEntry
    pop     r8
@skip_replay_submit:

    lea     rcx, [g_TaskQueueCS]
    call    LeaveCriticalSection

    ; --- Wake a worker via IOCP (FIX #6: deterministic wake, no thundering herd) ---
    mov     rcx, [g_IOCPHandle]
    test    rcx, rcx
    jz      @wake_fallback
    xor     edx, edx                        ; dwNumberOfBytesTransferred
    mov     r8, rsi                          ; CompletionKey = task_id
    xor     r9, r9                           ; lpOverlapped = NULL
    call    PostQueuedCompletionStatus
    jmp     @enqueue_done

@wake_fallback:
    mov     rcx, [g_WorkerWakeEvent]
    call    SetEvent
    jmp     @enqueue_done

@queue_full:
    ; Ring buffer overflow: free task, return 0
    lea     rcx, [g_TaskQueueCS]
    call    LeaveCriticalSection
    ; Free the allocated descriptor
    mov     rcx, [g_KernelHeap]
    xor     edx, edx
    mov     r8, rbx
    call    HeapFree
    xor     esi, esi                         ; return 0
    jmp     @submit_exit

@enqueue_done:

    ; --- Debug trace ---
    lea     rcx, [szTaskQueued]
    call    OutputDebugStringA

    mov     rax, rsi                        ; return task_id
    jmp     @submit_exit

@submit_fail:
    xor     rax, rax

@submit_exit:
    add     rsp, 72
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SubmitTask ENDP

; =============================================================================
; CANCEL TASK
; =============================================================================
; BOOL __fastcall CancelTask(UINT64 taskId)
; rcx = taskId
; Returns: 1 on success, 0 if not found
; =============================================================================
ALIGN 16
CancelTask PROC
    push    rbx
    push    rsi
    sub     rsp, 40

    mov     rbx, rcx                        ; save taskId

    lea     rcx, [g_TaskQueueCS]
    call    EnterCriticalSection

    ; Search all priority queues for this task
    mov     rcx, [g_SharedMemBase]
    test    rcx, rcx
    jz      @cancel_not_found

    xor     esi, esi                        ; priority index
@cancel_search_pri:
    cmp     esi, MAX_PRIORITIES
    jge     @cancel_not_found

    lea     rax, [g_PQ_Tail]
    mov     edx, [rax + rsi*4]              ; tail
    lea     rax, [g_PQ_Head]
    mov     r8d, [rax + rsi*4]              ; head

@cancel_search_entry:
    cmp     edx, r8d
    je      @cancel_next_pri

    ; Calculate base for this priority (FIX #3: isolated regions)
    push    rcx
    mov     eax, esi
    imul    eax, SHM_QUEUE_STRIDE            ; priority * 32768
    add     rcx, rax                         ; rcx = base of priority ring

    ; Get task pointer from correct slot
    mov     r9, [rcx + rdx*8]               ; use tail index directly
    pop     rcx
    test    r9, r9
    jz      @cancel_skip

    ; Check task_id
    cmp     [r9], rbx
    jne     @cancel_skip

    ; Found! Mark as cancelled (FIX #5: do NOT decrement active count here;
    ; worker will handle it when the cancelled task is dequeued and skipped)
    mov     DWORD PTR [r9+0Ch], STATE_CANCELLED
    ; REMOVED: lock dec DWORD PTR [g_ActiveTaskCount] — prevents double-decrement

    lea     rcx, [g_TaskQueueCS]
    call    LeaveCriticalSection

    mov     eax, 1
    jmp     @cancel_exit

@cancel_skip:
    inc     edx
    and     edx, (RING_BUFFER_SIZE - 1)
    jmp     @cancel_search_entry

@cancel_next_pri:
    inc     esi
    jmp     @cancel_search_pri

@cancel_not_found:
    lea     rcx, [g_TaskQueueCS]
    call    LeaveCriticalSection
    xor     eax, eax

@cancel_exit:
    add     rsp, 40
    pop     rsi
    pop     rbx
    ret
CancelTask ENDP

; =============================================================================
; REGISTER WINDOW
; =============================================================================
; DWORD __fastcall RegisterWindow(
;   DWORD windowType,  ; rcx
;   DWORD posX,        ; rdx
;   DWORD posY,        ; r8d
;   DWORD width,       ; r9d
;   DWORD height       ; [rsp+40]
; )
; Returns: windowId (0 on failure)
; =============================================================================
ALIGN 16
RegisterWindow PROC
    push    rbx
    push    rsi
    push    r12
    push    r13
    push    r14
    sub     rsp, 56

    mov     r12d, ecx                       ; windowType
    mov     r13d, edx                       ; posX
    mov     r14d, r8d                       ; posY
    mov     ebx, r9d                        ; width

    ; --- Allocate window state ---
    mov     rcx, [g_KernelHeap]
    mov     edx, HEAP_ZERO_MEMORY
    mov     r8d, WINDOW_STATE_SIZE
    call    HeapAlloc
    test    rax, rax
    jz      @regwin_fail
    mov     rsi, rax                        ; rsi = WindowState

    ; --- Assign window ID ---
    lea     rcx, [g_WindowIdCounter]
    mov     rax, 1
    lock xadd QWORD PTR [rcx], rax
    inc     rax
    mov     [rsi], rax                      ; window_id
    mov     ecx, eax                        ; save for return (lower 32 bits)

    ; --- Fill state ---
    mov     DWORD PTR [rsi+08h], r12d       ; window_type
    mov     DWORD PTR [rsi+0Ch], 1          ; window_state = normal
    mov     DWORD PTR [rsi+10h], r13d       ; pos_x
    mov     DWORD PTR [rsi+14h], r14d       ; pos_y
    mov     DWORD PTR [rsi+18h], ebx        ; width
    mov     eax, [rsp+56+40+40]             ; height from stack
    mov     DWORD PTR [rsi+1Ch], eax        ; height

    ; Record create time
    push    rcx
    lea     rcx, [rsi+40h]
    call    QueryPerformanceCounter
    pop     rcx

    ; --- Register in window slots ---
    push    rcx
    lea     rcx, [g_WindowRegistryCS]
    call    EnterCriticalSection
    pop     rcx

    ; Find free slot
    xor     edx, edx
@find_slot:
    cmp     edx, MAX_WINDOWS
    jge     @no_slot
    lea     rax, [g_WindowActive]
    cmp     BYTE PTR [rax + rdx], 0
    je      @found_slot
    inc     edx
    jmp     @find_slot

@found_slot:
    lea     rax, [g_WindowActive]
    mov     BYTE PTR [rax + rdx], 1
    lea     rax, [g_WindowSlots]
    mov     [rax + rdx*8], rsi
    lock inc DWORD PTR [g_ActiveWindowCount]

    push    rcx
    lea     rcx, [g_WindowRegistryCS]
    call    LeaveCriticalSection
    pop     rcx

    ; --- Allocate per-window ring buffer ---
    push    rcx
    mov     rcx, 0                          ; lpAddress
    mov     edx, RING_BUFFER_SIZE * RING_ENTRY_TOTAL
    mov     r8d, MEM_COMMIT OR MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    pop     rcx
    mov     [rsi+90h], rax                  ; ring_buffer_ptr

    lea     r8, [szWindowCreated]
    push    rcx
    mov     rcx, r8
    call    OutputDebugStringA
    pop     rcx

    mov     eax, ecx                        ; return windowId
    jmp     @regwin_exit

@no_slot:
    lea     rcx, [g_WindowRegistryCS]
    call    LeaveCriticalSection

    ; Free the allocated state
    mov     rcx, [g_KernelHeap]
    xor     edx, edx
    mov     r8, rsi
    call    HeapFree

@regwin_fail:
    xor     eax, eax

@regwin_exit:
    add     rsp, 56
    pop     r14
    pop     r13
    pop     r12
    pop     rsi
    pop     rbx
    ret
RegisterWindow ENDP

; =============================================================================
; UNREGISTER WINDOW
; =============================================================================
ALIGN 16
UnregisterWindow PROC
    push    rbx
    push    rsi
    sub     rsp, 40

    mov     ebx, ecx                        ; windowId

    lea     rcx, [g_WindowRegistryCS]
    call    EnterCriticalSection

    ; Find the window slot
    xor     edx, edx
@unreg_search:
    cmp     edx, MAX_WINDOWS
    jge     @unreg_not_found

    lea     rax, [g_WindowActive]
    cmp     BYTE PTR [rax + rdx], 0
    je      @unreg_next

    lea     rax, [g_WindowSlots]
    mov     rsi, [rax + rdx*8]
    test    rsi, rsi
    jz      @unreg_next

    cmp     DWORD PTR [rsi], ebx
    jne     @unreg_next

    ; Found - deactivate
    lea     rax, [g_WindowActive]
    mov     BYTE PTR [rax + rdx], 0
    lea     rax, [g_WindowSlots]
    mov     QWORD PTR [rax + rdx*8], 0
    lock dec DWORD PTR [g_ActiveWindowCount]

    ; Free ring buffer
    mov     rcx, [rsi+90h]
    test    rcx, rcx
    jz      @no_ring_free
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@no_ring_free:

    ; Free window state
    mov     rcx, [g_KernelHeap]
    xor     edx, edx
    mov     r8, rsi
    call    HeapFree

    lea     rcx, [g_WindowRegistryCS]
    call    LeaveCriticalSection

    lea     rcx, [szWindowDestroyed]
    call    OutputDebugStringA

    mov     eax, 1
    jmp     @unreg_exit

@unreg_next:
    inc     edx
    jmp     @unreg_search

@unreg_not_found:
    lea     rcx, [g_WindowRegistryCS]
    call    LeaveCriticalSection
    xor     eax, eax

@unreg_exit:
    add     rsp, 40
    pop     rsi
    pop     rbx
    ret
UnregisterWindow ENDP

; =============================================================================
; SEND IPC MESSAGE (inter-window communication)
; =============================================================================
; BOOL __fastcall SendIPCMessage(
;   DWORD msgType,      ; rcx
;   DWORD srcWindow,    ; rdx
;   DWORD dstWindow,    ; r8d (0 = broadcast)
;   void* payload,      ; r9
;   DWORD payloadSize   ; [rsp+40]
; )
; =============================================================================
ALIGN 16
SendIPCMessage PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 56

    mov     r12d, ecx                       ; msgType
    mov     r13d, edx                       ; srcWindow
    mov     ebx, r8d                        ; dstWindow
    mov     rsi, r9                         ; payload

    ; Find destination window
    test    ebx, ebx
    jz      @ipc_broadcast

    ; Single target - FindWindowState returns WindowState ptr
    call    FindWindowState
    test    rax, rax
    jz      @ipc_fail
    mov     rdi, rax                        ; WindowState ptr

    ; Write entry to ring (WriteRingEntry now takes WindowState ptr)
    mov     rcx, rdi
    mov     edx, r12d
    mov     r8d, r13d
    mov     r9d, ebx
    call    WriteRingEntry
    jmp     @ipc_ok

@ipc_broadcast:
    ; Broadcast to all active windows
    xor     ecx, ecx
@bcast_loop:
    cmp     ecx, MAX_WINDOWS
    jge     @ipc_ok

    push    rcx
    lea     rax, [g_WindowActive]
    cmp     BYTE PTR [rax + rcx], 0
    je      @bcast_skip

    lea     rax, [g_WindowSlots]
    mov     rdi, [rax + rcx*8]
    test    rdi, rdi
    jz      @bcast_skip

    ; Skip source window
    cmp     DWORD PTR [rdi], r13d
    je      @bcast_skip

    ; Pass WindowState ptr to WriteRingEntry
    mov     rcx, rdi                         ; WindowState ptr
    mov     edx, r12d
    mov     r8d, r13d
    mov     r9d, DWORD PTR [rdi]
    call    WriteRingEntry

@bcast_skip:
    pop     rcx
    inc     ecx
    jmp     @bcast_loop

@ipc_ok:
    mov     eax, 1
    jmp     @ipc_exit

@ipc_fail:
    xor     eax, eax

@ipc_exit:
    add     rsp, 56
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SendIPCMessage ENDP

; =============================================================================
; GET KERNEL STATS
; =============================================================================
; void __fastcall GetKernelStats(KernelStats* out)
; rcx = pointer to output struct (64 bytes)
; [0x00] activeWindows (4)
; [0x04] activeTasks (4)
; [0x08] totalProcessed (8)
; [0x10] totalFailed (8)
; [0x18] uptimeMs (8)
; [0x20] workerCount (4)
; [0x24] queueDepth[5] (20 bytes = 5*4)
; [0x38] reserved (8)
; =============================================================================
ALIGN 16
GetKernelStats PROC
    push    rbx
    sub     rsp, 32

    mov     rbx, rcx                        ; output ptr

    mov     eax, [g_ActiveWindowCount]
    mov     [rbx], eax
    mov     eax, [g_ActiveTaskCount]
    mov     [rbx+4], eax
    mov     rax, [g_TotalTasksProcessed]
    mov     [rbx+8], rax
    mov     rax, [g_TotalTasksFailed]
    mov     [rbx+10h], rax

    ; Uptime
    call    GetTickCount64
    sub     rax, [g_KernelStartTick]
    mov     [rbx+18h], rax

    mov     eax, [g_WorkerCount]
    mov     [rbx+20h], eax

    ; Queue depths per priority
    xor     ecx, ecx
@stat_qd:
    cmp     ecx, MAX_PRIORITIES
    jge     @stat_done
    lea     rax, [g_PQ_Count]
    mov     edx, [rax + rcx*4]
    mov     [rbx+24h+rcx*4], edx
    inc     ecx
    jmp     @stat_qd

@stat_done:
    add     rsp, 32
    pop     rbx
    ret
GetKernelStats ENDP

; =============================================================================
; WORKER THREAD PROCEDURE
; =============================================================================
ALIGN 16
WorkerThreadProc PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 56

    mov     r12, rcx                        ; thread index

    lea     rcx, [szWorkerStarted]
    call    OutputDebugStringA

@worker_loop:
    ; Check shutdown
    cmp     QWORD PTR [g_WorkerShutdown], 0
    jne     @worker_exit

    ; --- FIX #6: IOCP-based blocking wait (no polling, no thundering herd) ---
    ; GetQueuedCompletionStatus blocks until a task is posted
    sub     rsp, 32                          ; space for out params
    mov     rcx, [g_IOCPHandle]
    test    rcx, rcx
    jz      @fallback_wait
    lea     rdx, [rsp]                       ; lpNumberOfBytesTransferred
    lea     r8, [rsp+8]                      ; lpCompletionKey
    lea     r9, [rsp+16]                     ; lpOverlapped
    mov     DWORD PTR [rsp+32], INFINITE_WAIT ; dwMilliseconds
    call    GetQueuedCompletionStatus
    add     rsp, 32
    jmp     @check_shutdown_2

@fallback_wait:
    add     rsp, 32
    ; Fallback to event wait if IOCP not available
    mov     rcx, [g_WorkerWakeEvent]
    mov     edx, INFINITE_WAIT               ; FIX #6: INFINITE, not 100ms
    call    WaitForSingleObject

@check_shutdown_2:
    ; Check shutdown again
    cmp     QWORD PTR [g_WorkerShutdown], 0
    jne     @worker_exit

    ; --- Dequeue highest priority task (FIX #3: correct ring layout) ---
    lea     rcx, [g_TaskQueueCS]
    call    EnterCriticalSection

    xor     esi, esi                        ; priority scan
    xor     edi, edi                        ; found flag
@deq_scan:
    cmp     esi, MAX_PRIORITIES
    jge     @deq_none

    lea     rax, [g_PQ_Count]
    cmp     DWORD PTR [rax + rsi*4], 0
    je      @deq_next_pri

    ; Has items - calculate isolated ring base (FIX #2/#3)
    lea     rax, [g_PQ_Tail]
    mov     edx, [rax + rsi*4]              ; tail index

    mov     rcx, [g_SharedMemBase]
    test    rcx, rcx
    jz      @deq_next_pri

    ; Base = shared_base + (priority * SHM_QUEUE_STRIDE)
    mov     eax, esi
    imul    rax, SHM_QUEUE_STRIDE            ; priority * 32768
    add     rcx, rax                         ; rcx = base of this priority ring

    ; Load acquire fence (FIX #8)
    lfence

    mov     rbx, [rcx + rdx*8]              ; task descriptor ptr at tail
    test    rbx, rbx
    jz      @deq_advance_empty

    ; --- FIX #4: Dependency resolution ---
    mov     rax, [rbx+60h]                   ; depends_on task_id
    test    rax, rax
    jz      @dep_ok                          ; no dependency
    ; Check if dependency is complete
    push    rcx
    push    rdx
    push    rsi
    push    rdi
    mov     rcx, rax                         ; dependency task_id
    call    IsTaskComplete
    pop     rdi
    pop     rsi
    pop     rdx
    pop     rcx
    test    al, al
    jz      @deq_next_pri                    ; dependency not met - skip this priority

@dep_ok:
    ; Clear slot
    mov     QWORD PTR [rcx + rdx*8], 0

    ; Advance tail
    inc     edx
    and     edx, (RING_BUFFER_SIZE - 1)
    lea     rax, [g_PQ_Tail]
    mov     [rax + rsi*4], edx

    ; Decrement count
    lea     rax, [g_PQ_Count]
    lock dec DWORD PTR [rax + rsi*4]

    mov     edi, 1                          ; found
    jmp     @deq_done

@deq_advance_empty:
    ; Empty slot in ring (shouldn't happen if count > 0, but handle gracefully)
    inc     edx
    and     edx, (RING_BUFFER_SIZE - 1)
    lea     rax, [g_PQ_Tail]
    mov     [rax + rsi*4], edx
    lea     rax, [g_PQ_Count]
    lock dec DWORD PTR [rax + rsi*4]
    jmp     @deq_scan                        ; retry same priority

@deq_next_pri:
    inc     esi
    jmp     @deq_scan

@deq_none:
@deq_done:
    lea     rcx, [g_TaskQueueCS]
    call    LeaveCriticalSection

    test    edi, edi
    jz      @worker_loop                    ; nothing found, loop back

    ; --- Execute task ---
    ; Check if cancelled (FIX #5: decrement active count for skipped cancelled tasks)
    cmp     DWORD PTR [rbx+0Ch], STATE_CANCELLED
    jne     @not_cancelled
    lock dec DWORD PTR [g_ActiveTaskCount]   ; only place active count is decremented
    jmp     @task_skip
@not_cancelled:

    ; Dependency already checked during dequeue (FIX #4)


    ; Mark running
    mov     DWORD PTR [rbx+0Ch], STATE_RUNNING
    lea     rcx, [rbx+38h]
    call    QueryPerformanceCounter

    lea     rcx, [szTaskStarted]
    call    OutputDebugStringA

    ; Call task callback if set
    mov     rax, [rbx+18h]                  ; callback_fn
    test    rax, rax
    jz      @no_callback

    ; callback(task_descriptor*)
    mov     rcx, rbx
    call    rax

    ; Store result
    test    eax, eax
    jz      @task_failed
    mov     DWORD PTR [rbx+0Ch], STATE_COMPLETE
    lock inc QWORD PTR [g_TotalTasksProcessed]
    jmp     @task_finish

@task_failed:
    mov     DWORD PTR [rbx+0Ch], STATE_FAILED
    lock inc QWORD PTR [g_TotalTasksFailed]
    jmp     @task_finish

@no_callback:
    ; No callback - auto-complete
    mov     DWORD PTR [rbx+0Ch], STATE_COMPLETE
    lock inc QWORD PTR [g_TotalTasksProcessed]

@task_finish:
    ; Record end time
    lea     rcx, [rbx+40h]
    call    QueryPerformanceCounter

    lock dec DWORD PTR [g_ActiveTaskCount]

    lea     rcx, [szTaskCompleted]
    call    OutputDebugStringA

@task_skip:
    jmp     @worker_loop

@worker_exit:
    xor     eax, eax
    add     rsp, 56
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
WorkerThreadProc ENDP

; =============================================================================
; HELPER: Setup Shared Memory
; =============================================================================
ALIGN 16
SetupSharedMemory PROC
    push    rbx
    sub     rsp, 48

    ; CreateFileMappingW(INVALID_HANDLE, NULL, PAGE_READWRITE, 0, size, name)
    mov     rcx, INVALID_HANDLE
    xor     edx, edx                        ; lpSecurityAttribs
    mov     r8d, PAGE_READWRITE
    xor     r9d, r9d                        ; high DWORD
    mov     DWORD PTR [rsp+32], IPC_SHARED_SIZE ; low DWORD
    lea     rax, [szSharedMemName]
    mov     QWORD PTR [rsp+40], rax
    call    CreateFileMappingW
    test    rax, rax
    jz      @shm_fail
    mov     [g_SharedMemHandle], rax

    ; MapViewOfFile
    mov     rcx, rax
    mov     edx, FILE_MAP_ALL_ACCESS
    xor     r8d, r8d
    xor     r9d, r9d
    mov     DWORD PTR [rsp+32], IPC_SHARED_SIZE
    call    MapViewOfFile
    test    rax, rax
    jz      @shm_fail
    mov     [g_SharedMemBase], rax

    ; Zero the shared memory
    mov     rcx, rax
    xor     edx, edx
    mov     r8d, IPC_SHARED_SIZE
    call    RtlZeroMemory

    mov     eax, 1
    jmp     @shm_exit

@shm_fail:
    xor     eax, eax

@shm_exit:
    add     rsp, 48
    pop     rbx
    ret
SetupSharedMemory ENDP

; =============================================================================
; HELPER: Allocate Priority Queues
; =============================================================================
ALIGN 16
AllocatePriorityQueues PROC
    ; Priority queues use the shared memory region
    ; Already zeroed in SetupSharedMemory
    mov     eax, 1
    ret
AllocatePriorityQueues ENDP

; =============================================================================
; HELPER: Find Window State (returns WindowState ptr for window ebx)
; =============================================================================
; rax = WindowState ptr for window ebx, or 0 if not found
ALIGN 16
FindWindowState PROC
    push    rcx
    push    rdx

    xor     ecx, ecx
@fws_loop:
    cmp     ecx, MAX_WINDOWS
    jge     @fws_none

    lea     rax, [g_WindowActive]
    cmp     BYTE PTR [rax + rcx], 0
    je      @fws_next

    lea     rax, [g_WindowSlots]
    mov     rdx, [rax + rcx*8]
    test    rdx, rdx
    jz      @fws_next

    cmp     DWORD PTR [rdx], ebx
    jne     @fws_next

    mov     rax, rdx                         ; return WindowState ptr
    pop     rdx
    pop     rcx
    ret

@fws_next:
    inc     ecx
    jmp     @fws_loop

@fws_none:
    xor     rax, rax
    pop     rdx
    pop     rcx
    ret
FindWindowState ENDP

; =============================================================================
; HELPER: Write Ring Entry (FIX #3/#9: proper ring with head/tail/overflow)
; =============================================================================
; rcx = WindowState ptr (contains ring_buffer_ptr, ring_head, ring_tail)
; edx = msgType, r8d = srcWindow, r9d = dstWindow
; Returns: eax = 1 success, 0 = ring full
ALIGN 16
WriteRingEntry PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 48

    mov     r12, rcx                         ; WindowState ptr
    mov     r13d, edx                        ; msgType
    mov     esi, r8d                         ; srcWindow
    mov     edi, r9d                         ; dstWindow

    ; Load ring buffer pointer
    mov     rbx, [r12+90h]                   ; ring_buffer_ptr
    test    rbx, rbx
    jz      @wre_fail

    ; Load ring_head and check overflow against ring_tail
    mov     eax, DWORD PTR [r12+98h]         ; ring_head (lower 32 bits)
    lea     ecx, [eax + 1]
    and     ecx, (RING_BUFFER_SIZE - 1)      ; next_head
    cmp     ecx, DWORD PTR [r12+0A0h]        ; ring_tail
    je      @wre_fail                        ; ring full - drop message

    ; Calculate slot address: ring_base + (head * RING_ENTRY_TOTAL)
    mov     ecx, eax                         ; head index
    imul    rcx, RING_ENTRY_TOTAL            ; head * 256
    add     rcx, rbx                         ; rcx = slot address

    ; Write full 256-byte ring entry
    mov     DWORD PTR [rcx+00h], r13d        ; msg_type
    mov     DWORD PTR [rcx+04h], esi         ; src_window
    mov     DWORD PTR [rcx+08h], edi         ; dst_window
    mov     DWORD PTR [rcx+0Ch], 0           ; payload_size (default)

    ; Timestamp
    push    rcx
    call    GetTickCount64
    pop     rcx
    mov     [rcx+10h], rax                   ; timestamp

    ; Sequence number
    mov     rax, [g_TotalTasksProcessed]
    mov     [rcx+18h], rax                   ; sequence_num

    ; Zero payload region (224 bytes from offset 0x20)
    push    rcx
    lea     rcx, [rcx+20h]
    xor     edx, edx
    mov     r8d, 224
    call    RtlZeroMemory
    pop     rcx

    ; Store fence: ensure all writes are visible before head update (FIX #8)
    sfence

    ; Advance head atomically
    mov     eax, DWORD PTR [r12+98h]         ; current head
    inc     eax
    and     eax, (RING_BUFFER_SIZE - 1)
    mov     DWORD PTR [r12+98h], eax         ; update head

    mov     eax, 1                           ; success
    jmp     @wre_exit

@wre_fail:
    xor     eax, eax                         ; ring full or no buffer

@wre_exit:
    add     rsp, 48
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
WriteRingEntry ENDP

; =============================================================================
; DEPENDENCY RESOLUTION: IsTaskComplete (FIX #4)
; =============================================================================
; Checks if a task ID is in a terminal state (COMPLETE, FAILED, CANCELLED)
; Input:  rcx = task_id to check
; Output: al = 1 if complete/failed/cancelled/not-found, 0 if still active
;
; Strategy: Scan all priority queues for this task_id.
; If found and still active -> return 0
; If found and terminal    -> return 1
; If not found             -> return 1 (already dequeued and completed)
ALIGN 16
IsTaskComplete PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 32

    mov     r12, rcx                         ; target task_id
    xor     ebx, ebx                         ; priority counter

@itc_pri_loop:
    cmp     ebx, MAX_PRIORITIES
    jge     @itc_not_found                   ; not in any queue -> assume complete

    ; Calculate base of this priority's ring
    mov     eax, ebx
    imul    rax, SHM_QUEUE_STRIDE
    mov     r13, [g_SharedMemBase]
    add     r13, rax                         ; r13 = ring base

    ; Get tail and head for bounds
    lea     rax, [g_PQ_Tail]
    mov     esi, [rax + rbx*4]               ; tail
    lea     rax, [g_PQ_Head]
    mov     edi, [rax + rbx*4]               ; head

@itc_slot_loop:
    cmp     esi, edi
    je      @itc_next_pri                    ; exhausted this queue

    mov     rax, [r13 + rsi*8]               ; load task pointer
    test    rax, rax
    jz      @itc_next_slot

    ; Compare task_id (offset 0x00)
    cmp     [rax], r12
    jne     @itc_next_slot

    ; Found! Check state (offset 0x0C)
    mov     edx, DWORD PTR [rax+0Ch]
    cmp     edx, STATE_COMPLETE
    je      @itc_yes
    cmp     edx, STATE_FAILED
    je      @itc_yes
    cmp     edx, STATE_CANCELLED
    je      @itc_yes

    ; Still active (QUEUED or RUNNING)
    xor     al, al
    jmp     @itc_exit

@itc_next_slot:
    inc     esi
    and     esi, (RING_BUFFER_SIZE - 1)
    jmp     @itc_slot_loop

@itc_next_pri:
    inc     ebx
    jmp     @itc_pri_loop

@itc_not_found:
    ; Task not in any queue -> already completed and dequeued
@itc_yes:
    mov     al, 1

@itc_exit:
    add     rsp, 32
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
IsTaskComplete ENDP

; =============================================================================
; DETERMINISTIC REPLAY LOGGER (FIX #15)
; =============================================================================
; Records ring buffer transitions for post-mortem / replay analysis.
; Entry format (64 bytes):
;   [0x00] 8  timestamp (QPC)
;   [0x08] 4  operation (1=SUBMIT, 2=DEQUEUE, 3=COMPLETE, 4=CANCEL, 5=IPC)
;   [0x0C] 4  priority
;   [0x10] 8  task_id
;   [0x18] 4  head_before
;   [0x1C] 4  tail_before
;   [0x20] 4  count_before
;   [0x24] 4  thread_id
;   [0x28] 24 reserved
;
; ecx = operation, edx = priority, r8 = task_id
ALIGN 16
WriteReplayEntry PROC
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 48

    mov     esi, ecx                         ; operation
    mov     edi, edx                         ; priority
    mov     rbx, r8                          ; task_id

    ; Atomically claim a replay slot
    lea     rcx, [g_ReplayHead]
    mov     rax, 1
    lock xadd QWORD PTR [rcx], rax           ; rax = our slot index

    ; Wrap to replay ring size
    and     rax, (SHM_REPLAY_MAX - 1)

    ; Calculate address = shared_base + SHM_REPLAY_BASE + slot * 64
    imul    rax, SHM_REPLAY_ENTRY
    add     rax, SHM_REPLAY_BASE
    add     rax, [g_SharedMemBase]
    test    rax, rax
    jz      @replay_skip

    ; Write timestamp (QPC)
    push    rax
    lea     rcx, [rax]
    call    QueryPerformanceCounter
    pop     rax

    ; Write fields
    mov     DWORD PTR [rax+08h], esi         ; operation
    mov     DWORD PTR [rax+0Ch], edi         ; priority
    mov     [rax+10h], rbx                   ; task_id

    ; Snapshot queue state
    lea     rcx, [g_PQ_Head]
    mov     ecx, [rcx + rdi*4]
    mov     DWORD PTR [rax+18h], ecx         ; head_before
    lea     rcx, [g_PQ_Tail]
    mov     ecx, [rcx + rdi*4]
    mov     DWORD PTR [rax+1Ch], ecx         ; tail_before
    lea     rcx, [g_PQ_Count]
    mov     ecx, [rcx + rdi*4]
    mov     DWORD PTR [rax+20h], ecx         ; count_before

    ; Thread ID
    push    rax
    call    GetCurrentThreadId
    pop     rcx
    mov     DWORD PTR [rcx+24h], eax         ; thread_id

@replay_skip:
    add     rsp, 48
    pop     rdi
    pop     rsi
    pop     rbx
    ret
WriteReplayEntry ENDP

; =============================================================================
; HELPER: Free All Windows
; =============================================================================
ALIGN 16
FreeAllWindows PROC
    push    rbx
    push    rsi
    sub     rsp, 32

    xor     ebx, ebx
@faw_loop:
    cmp     ebx, MAX_WINDOWS
    jge     @faw_done

    lea     rax, [g_WindowActive]
    cmp     BYTE PTR [rax + rbx], 0
    je      @faw_next

    lea     rax, [g_WindowSlots]
    mov     rsi, [rax + rbx*8]
    test    rsi, rsi
    jz      @faw_next

    ; Free ring buffer
    mov     rcx, [rsi+90h]
    test    rcx, rcx
    jz      @faw_no_ring
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@faw_no_ring:

    ; Free window state
    mov     rcx, [g_KernelHeap]
    xor     edx, edx
    mov     r8, rsi
    call    HeapFree

    lea     rax, [g_WindowActive]
    mov     BYTE PTR [rax + rbx], 0

@faw_next:
    inc     ebx
    jmp     @faw_loop

@faw_done:
    add     rsp, 32
    pop     rsi
    pop     rbx
    ret
FreeAllWindows ENDP

; =============================================================================
; SWARM COORDINATOR
; =============================================================================
; Broadcasts a task to N windows simultaneously for swarm processing
; DWORD __fastcall SwarmBroadcast(
;   DWORD taskType,     ; rcx
;   void* payload,      ; rdx
;   DWORD payloadSize,  ; r8d
;   DWORD modelCount    ; r9d (how many models to swarm)
; )
; Returns: number of windows that received the swarm task
; =============================================================================
ALIGN 16
SwarmBroadcast PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 48

    mov     r12d, ecx                       ; taskType
    mov     rsi, rdx                        ; payload
    mov     r13d, r8d                       ; payloadSize
    mov     edi, r9d                        ; modelCount

    xor     ebx, ebx                        ; counter
    xor     ecx, ecx                        ; window index

@swarm_loop:
    cmp     ecx, MAX_WINDOWS
    jge     @swarm_done
    cmp     ebx, edi
    jge     @swarm_done

    push    rcx
    lea     rax, [g_WindowActive]
    cmp     BYTE PTR [rax + rcx], 0
    je      @swarm_skip

    ; Submit task to this window
    mov     ecx, r12d                       ; taskType
    mov     edx, PRIORITY_HIGH              ; priority
    lea     rax, [g_WindowSlots]
    pop     r8                              ; restore window index
    push    r8
    mov     r8, [rax + r8*8]
    test    r8, r8
    jz      @swarm_skip
    mov     r8d, DWORD PTR [r8]             ; windowId
    xor     r9, r9                          ; callback (NULL - async)
    mov     QWORD PTR [rsp+40], 0           ; userData
    mov     QWORD PTR [rsp+48], 0           ; modelId
    mov     QWORD PTR [rsp+56], 0           ; dependsOn
    call    SubmitTask
    test    rax, rax
    jz      @swarm_skip
    inc     ebx

@swarm_skip:
    pop     rcx
    inc     ecx
    jmp     @swarm_loop

@swarm_done:
    mov     eax, ebx                        ; return count
    add     rsp, 48
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SwarmBroadcast ENDP

; =============================================================================
; CHAIN OF THOUGHT PIPELINE
; =============================================================================
; Chains tasks sequentially using dependencies
; UINT64 __fastcall ChainOfThought(
;   DWORD windowId,     ; rcx
;   DWORD stepCount,    ; edx
;   UINT64* callbacks   ; r8 (array of function pointers)
; )
; Returns: first task_id in chain (0 on failure)
; =============================================================================
ALIGN 16
ChainOfThought PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    sub     rsp, 72

    mov     r12d, ecx                       ; windowId
    mov     r13d, edx                       ; stepCount
    mov     r14, r8                         ; callbacks array

    xor     esi, esi                        ; step index
    xor     edi, edi                        ; previous task_id
    xor     ebx, ebx                        ; first task_id

@cot_loop:
    cmp     esi, r13d
    jge     @cot_done

    ; SubmitTask(TASK_COT, PRIORITY_NORMAL, windowId, callback, 0, 0, prevTaskId)
    mov     ecx, TASK_COT
    mov     edx, PRIORITY_NORMAL
    mov     r8d, r12d                       ; windowId
    mov     r9, [r14 + rsi*8]              ; callback
    mov     QWORD PTR [rsp+72+48+40], 0    ; userData
    mov     QWORD PTR [rsp+72+48+48], 0    ; modelId
    mov     QWORD PTR [rsp+72+48+56], rdi  ; dependsOn = previous task
    call    SubmitTask
    test    rax, rax
    jz      @cot_fail

    ; Save first task_id
    test    ebx, ebx
    jnz     @cot_not_first
    mov     rbx, rax
@cot_not_first:
    mov     rdi, rax                        ; previous = current

    inc     esi
    jmp     @cot_loop

@cot_done:
    mov     rax, rbx                        ; return first task_id
    jmp     @cot_exit

@cot_fail:
    xor     rax, rax

@cot_exit:
    add     rsp, 72
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
ChainOfThought ENDP

; =============================================================================
; HIGH-RESOLUTION TIMER
; =============================================================================
ALIGN 16
GetMicroseconds PROC
    sub     rsp, 24
    lea     rcx, [rsp+8]
    call    QueryPerformanceCounter
    mov     rax, [rsp+8]
    ; Convert to microseconds: (count * 1000000) / freq
    imul    rax, 1000000
    mov     rcx, [g_PerfFrequency]
    test    rcx, rcx
    jz      @us_zero
    xor     edx, edx
    div     rcx
    add     rsp, 24
    ret
@us_zero:
    xor     eax, eax
    add     rsp, 24
    ret
GetMicroseconds ENDP

; =============================================================================
; MEMORY BARRIER HELPERS (for lock-free structures)
; =============================================================================
ALIGN 16
MemoryFence PROC
    mfence
    ret
MemoryFence ENDP

ALIGN 16
LoadFence PROC
    lfence
    ret
LoadFence ENDP

ALIGN 16
StoreFence PROC
    sfence
    ret
StoreFence ENDP

; =============================================================================
; EXPORTS
; =============================================================================
PUBLIC KernelInit
PUBLIC KernelShutdown
PUBLIC SubmitTask
PUBLIC CancelTask
PUBLIC RegisterWindow
PUBLIC UnregisterWindow
PUBLIC SendIPCMessage
PUBLIC GetKernelStats
PUBLIC SwarmBroadcast
PUBLIC ChainOfThought
PUBLIC GetMicroseconds
PUBLIC MemoryFence
PUBLIC LoadFence
PUBLIC StoreFence
PUBLIC IsTaskComplete
PUBLIC WriteReplayEntry

END
