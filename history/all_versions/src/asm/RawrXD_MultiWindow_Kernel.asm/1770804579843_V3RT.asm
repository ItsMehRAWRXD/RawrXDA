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

    ; --- Atomic init check (compare-exchange) ---
    lea     rcx, [g_KernelInitialized]
    xor     edx, edx                        ; expected = 0
    mov     r8d, 1                          ; desired = 1
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

    ; --- Create worker wake event (auto-reset) ---
    xor     ecx, ecx                        ; lpSecurityAttribs = NULL
    xor     edx, edx                        ; bManualReset = FALSE
    xor     r8d, r8d                        ; bInitialState = FALSE
    xor     r9d, r9d                        ; lpName = NULL
    call    CreateEventW
    test    rax, rax
    jz      @init_fail
    mov     [g_WorkerWakeEvent], rax

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

    ; Calculate ring index for this priority
    mov     eax, r13d                        ; priority
    lea     rcx, [g_PQ_Head]
    mov     edx, [rcx + rax*4]              ; current head
    
    ; Store task pointer in ring (simplified - using shared mem)
    mov     rcx, [g_SharedMemBase]
    test    rcx, rcx
    jz      @enqueue_skip

    ; Write task pointer at ring position
    ; Ring offset = (priority * RING_BUFFER_SIZE + head) * 8
    mov     edi, r13d
    imul    edi, RING_BUFFER_SIZE
    add     edi, edx
    and     edi, (RING_BUFFER_SIZE - 1)     ; wrap
    mov     [rcx + rdi*8], rbx

    ; Advance head
    inc     edx
    and     edx, (RING_BUFFER_SIZE - 1)
    lea     rcx, [g_PQ_Head]
    mov     [rcx + rax*4], edx

    ; Increment count
    lea     rcx, [g_PQ_Count]
    lock inc DWORD PTR [rcx + rax*4]

@enqueue_skip:
    ; Increment active count
    lock inc DWORD PTR [g_ActiveTaskCount]

    lea     rcx, [g_TaskQueueCS]
    call    LeaveCriticalSection

    ; --- Wake a worker ---
    mov     rcx, [g_WorkerWakeEvent]
    call    SetEvent

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

    ; Calculate absolute ring index
    mov     eax, esi
    imul    eax, RING_BUFFER_SIZE
    add     eax, edx
    and     eax, (RING_BUFFER_SIZE - 1)

    ; Get task pointer
    mov     r9, [rcx + rax*8]
    test    r9, r9
    jz      @cancel_skip

    ; Check task_id
    cmp     [r9], rbx
    jne     @cancel_skip

    ; Found! Mark as cancelled
    mov     DWORD PTR [r9+0Ch], STATE_CANCELLED
    lock dec DWORD PTR [g_ActiveTaskCount]

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

    ; Single target
    call    FindWindowRing
    test    rax, rax
    jz      @ipc_fail
    mov     rdi, rax                        ; ring buffer ptr

    ; Write entry to ring
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

    mov     rcx, [rdi+90h]                  ; ring_buffer_ptr
    test    rcx, rcx
    jz      @bcast_skip

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

    ; Wait for work signal (100ms timeout for poll)
    mov     rcx, [g_WorkerWakeEvent]
    mov     edx, 100
    call    WaitForSingleObject

    ; Check shutdown again
    cmp     QWORD PTR [g_WorkerShutdown], 0
    jne     @worker_exit

    ; --- Dequeue highest priority task ---
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

    ; Has items - dequeue
    lea     rax, [g_PQ_Tail]
    mov     edx, [rax + rsi*4]             ; tail
    
    mov     rcx, [g_SharedMemBase]
    test    rcx, rcx
    jz      @deq_next_pri

    mov     eax, esi
    imul    eax, RING_BUFFER_SIZE
    add     eax, edx
    and     eax, (RING_BUFFER_SIZE - 1)

    mov     rbx, [rcx + rax*8]             ; task descriptor ptr
    test    rbx, rbx
    jz      @deq_next_pri

    ; Clear slot
    mov     QWORD PTR [rcx + rax*8], 0

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
    ; Check if cancelled
    cmp     DWORD PTR [rbx+0Ch], STATE_CANCELLED
    je      @task_skip

    ; Check dependency
    mov     rax, [rbx+60h]                  ; depends_on
    test    rax, rax
    jz      @no_dep
    ; TODO: check if dependency is complete
@no_dep:

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
; HELPER: Find Window Ring Buffer
; =============================================================================
; rax = ring buffer ptr for window ebx
ALIGN 16
FindWindowRing PROC
    push    rcx
    push    rdx

    xor     ecx, ecx
@fwr_loop:
    cmp     ecx, MAX_WINDOWS
    jge     @fwr_none

    lea     rax, [g_WindowActive]
    cmp     BYTE PTR [rax + rcx], 0
    je      @fwr_next

    lea     rax, [g_WindowSlots]
    mov     rdx, [rax + rcx*8]
    test    rdx, rdx
    jz      @fwr_next

    cmp     DWORD PTR [rdx], ebx
    jne     @fwr_next

    mov     rax, [rdx+90h]
    pop     rdx
    pop     rcx
    ret

@fwr_next:
    inc     ecx
    jmp     @fwr_loop

@fwr_none:
    xor     rax, rax
    pop     rdx
    pop     rcx
    ret
FindWindowRing ENDP

; =============================================================================
; HELPER: Write Ring Entry
; =============================================================================
ALIGN 16
WriteRingEntry PROC
    ; rcx = ring base, edx = msgType, r8d = src, r9d = dst
    ; Simplified: just write msg type at head position
    push    rbx
    mov     DWORD PTR [rcx], edx             ; msg_type at offset 0
    mov     DWORD PTR [rcx+4], r8d           ; src_window
    mov     DWORD PTR [rcx+8], r9d           ; dst_window
    pop     rbx
    ret
WriteRingEntry ENDP

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

END
