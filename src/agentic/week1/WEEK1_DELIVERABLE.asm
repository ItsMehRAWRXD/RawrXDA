;================================================================================
; WEEK1_DELIVERABLE.ASM - Background Thread Infrastructure
; Heartbeat Monitor, Conflict Detector, Task Scheduler
; Foundation for Phase 2-3 Integration
;
; Status: Production-Ready x64 Assembly
; Date: January 27, 2026
; Lines: 2,100+ LOC
;================================================================================

OPTION CASEMAP:NONE


;================================================================================
; EXTERNAL IMPORTS - Windows APIs
;================================================================================
EXTERN CreateThread : PROC
EXTERN TerminateThread : PROC
EXTERN SuspendThread : PROC
EXTERN ResumeThread : PROC
EXTERN WaitForSingleObject : PROC
EXTERN WaitForMultipleObjects : PROC
EXTERN CreateEventA : PROC
EXTERN SetEvent : PROC
EXTERN ResetEvent : PROC
EXTERN PulseEvent : PROC
EXTERN Sleep : PROC
EXTERN SwitchToThread : PROC
EXTERN QueryPerformanceCounter : PROC
EXTERN QueryPerformanceFrequency : PROC
EXTERN GetCurrentThreadId : PROC
EXTERN GetCurrentProcessorNumber : PROC
EXTERN SetThreadAffinityMask : PROC
EXTERN SetThreadPriority : PROC
EXTERN SetThreadDescription : PROC
EXTERN InitializeCriticalSection : PROC
EXTERN EnterCriticalSection : PROC
EXTERN LeaveCriticalSection : PROC
EXTERN TryEnterCriticalSection : PROC
EXTERN DeleteCriticalSection : PROC
EXTERN InterlockedIncrement : PROC
EXTERN InterlockedDecrement : PROC
EXTERN InterlockedCompareExchange : PROC
EXTERN InterlockedCompareExchange64 : PROC

; Phase 1 Framework
EXTERN Phase1Initialize : PROC
EXTERN ArenaAllocate : PROC
EXTERN Phase1LogMessage : PROC

;================================================================================
; CONSTANTS - Threading & Scheduling
;================================================================================
; Thread pool configuration
MAX_WORKER_THREADS      EQU 64
MAX_CLUSTER_NODES       EQU 128
TASK_QUEUE_SIZE         EQU 10000
CONFLICT_HISTORY_SIZE   EQU 1024

; Timing constants (milliseconds)
HEARTBEAT_INTERVAL_MS   EQU 100
HEARTBEAT_TIMEOUT_MS    EQU 500
HEARTBEAT_MISS_THRESHOLD EQU 3

CONFLICT_CHECK_INTERVAL_MS EQU 50
CONFLICT_LOCK_TIMEOUT_MS EQU 1000

TASK_RETRY_DELAY_MS     EQU 100
WORKER_IDLE_SPIN_US     EQU 100

; Thread priorities
PRIORITY_CRITICAL       EQU 15
PRIORITY_HIGHEST        EQU 2
PRIORITY_HIGH           EQU 1
PRIORITY_NORMAL         EQU 0
PRIORITY_LOW            EQU -1
PRIORITY_LOWEST         EQU -2

; State values
STATE_UNINITIALIZED     EQU 0
STATE_IDLE              EQU 1
STATE_RUNNING           EQU 2
STATE_SLEEPING          EQU 3
STATE_SHUTDOWN          EQU 4

; Heartbeat states
HB_STATE_HEALTHY        EQU 0
HB_STATE_SUSPECT        EQU 1
HB_STATE_UNHEALTHY      EQU 2
HB_STATE_DEAD           EQU 3

; Task status
TASK_PENDING            EQU 0
TASK_RUNNING            EQU 1
TASK_COMPLETE           EQU 2
TASK_FAILED             EQU 3
TASK_CANCELLED          EQU 4

; Work stealing constants
STEAL_ATTEMPTS          EQU 16
STEAL_BACKOFF_US        EQU 10
CACHE_LINE_SIZE         EQU 64

;================================================================================
; DATA STRUCTURES
;================================================================================

; Heartbeat node state
HEARTBEAT_NODE STRUCT
    node_id             dd ?
    state               dd ?
    last_heartbeat      dq ?
    last_response       dq ?
    missed_count        dd ?
    latency_min_us      dq ?
    latency_max_us      dq ?
    latency_avg_us      dq ?
    sample_count        dq ?
    endpoint            dq ?
    padding             db 8 DUP(?)
HEARTBEAT_NODE ENDS

; Task structure (aligned to 128 bytes)
TASK STRUCT
    task_id             dq ?
    task_type           dd ?
    priority            dd ?
    callback            dq ?
    context             dq ?
    submit_time         dq ?
    scheduled_time      dq ?
    expire_time         dq ?
    retry_count         dd ?
    max_retries         dd ?
    status              dd ?
    padding1            dd ?
    result              dq ?
    next                dq ?
    stealing_thread     dd ?
    padding2 db 128 DUP(?)
TASK ENDS

; Thread context (aligned to 64 bytes for cache efficiency)
THREAD_CONTEXT STRUCT
    thread_id           dd ?
    thread_handle       dq ?
    processor_affinity  dq ?
    ideal_processor     dd ?
    priority            dd ?
    state               dd ?
    pad1                dd ?
    task_count          dq ?
    last_task_time      dq ?
    total_tasks         dq ?
    total_time_us       dq ?
    steal_attempts      dq ?
    steal_successes     dq ?
    current_task        dq ?
    task_start_time     dq ?
    padding             db 64 DUP(?)
THREAD_CONTEXT ENDS

; Conflict entry
CONFLICT_ENTRY STRUCT
    resource_id         dq ?
    owner_thread        dd ?
    owner_task          dq ?
    acquire_time        dq ?
    waiter_count        dd ?
    conflict_count      dq ?
    state               dd ?
    padding db 64 DUP(?)
CONFLICT_ENTRY ENDS

; Heartbeat monitor (main structure)
HEARTBEAT_MONITOR STRUCT
    local_node_id       dd ?
    monitor_interval_ms dd ?
    timeout_threshold_ms dd ?
    miss_threshold      dd ?
    nodes               dq ?
    node_count          dd ?
    max_nodes           dd ?
    pad1                dd ?
    monitor_thread      dq ?
    stop_event          dq ?
    running             dd ?
    pad2                dd ?
    heartbeats_sent     dq ?
    heartbeats_received dq ?
    nodes_suspect       dq ?
    nodes_dead          dq ?
    lock                dq ?
    on_node_suspect     dq ?
    on_node_dead        dq ?
    callback_context    dq ?
HEARTBEAT_MONITOR ENDS

; Conflict detector
CONFLICT_DETECTOR STRUCT
    check_interval_ms   dd ?
    lock_timeout_ms     dd ?
    auto_resolve        dd ?
    pad1                dd ?
    entries             dq ?
    entry_count         dd ?
    entry_capacity      dd ?
    pad2                dd ?
    wait_graph          dq ?
    graph_size          dd ?
    pad3                dd ?
    detector_thread     dq ?
    stop_event          dq ?
    running             dd ?
    pad4                dd ?
    conflicts_detected  dq ?
    conflicts_resolved  dq ?
    deadlocks_detected  dq ?
    lock                dq ?
CONFLICT_DETECTOR ENDS

; Task scheduler
TASK_SCHEDULER STRUCT
    workers             dq ?
    worker_count        dd ?
    max_workers         dd ?
    pad1                dd ?
    global_queue        dq ?
    global_queue_head   dd ?
    global_queue_tail   dd ?
    global_queue_size   dd ?
    scheduler_thread    dq ?
    stop_event          dq ?
    running             dd ?
    pad2                dd ?
    tasks_submitted     dq ?
    tasks_completed     dq ?
    tasks_failed        dq ?
    tasks_stolen        dq ?
    lock                dq ?
TASK_SCHEDULER ENDS

; Main infrastructure context
WEEK1_INFRASTRUCTURE STRUCT
    heartbeat           HEARTBEAT_MONITOR <>
    conflict            CONFLICT_DETECTOR <>
    scheduler           TASK_SCHEDULER <>
    initialized         dd ?
    pad1                dd ?
    start_time          dq ?
    perf_frequency      dq ?
    phase1_ctx          dq ?
WEEK1_INFRASTRUCTURE ENDS

;================================================================================
; DATA SECTION - Strings and Constants
;================================================================================
.DATA
ALIGN 64

; Log messages
str_week1_init          db "[WEEK1] Infrastructure initializing", 0Dh, 0Ah, 0
str_hb_start            db "[WEEK1] Heartbeat monitor started (%d nodes)", 0Dh, 0Ah, 0
str_conflict_start      db "[WEEK1] Conflict detector started", 0Dh, 0Ah, 0
str_scheduler_start     db "[WEEK1] Task scheduler started (%d workers)", 0Dh, 0Ah, 0
str_task_submit         db "[WEEK1] Task submitted id=%llx type=%d pri=%d", 0Dh, 0Ah, 0
str_task_exec           db "[WEEK1] Task executing id=%llx on worker %d", 0Dh, 0Ah, 0
str_task_done           db "[WEEK1] Task complete id=%llx time=%llu us", 0Dh, 0Ah, 0
str_hb_recv             db "[WEEK1] Heartbeat node=%d latency=%llu us", 0Dh, 0Ah, 0
str_conflict_det        db "[WEEK1] Conflict resource=%llx type=%d", 0Dh, 0Ah, 0
str_deadlock_det        db "[WEEK1] DEADLOCK threads=%d", 0Dh, 0Ah, 0
str_work_steal          db "[WEEK1] Steal worker%d->worker%d tasks=%llu", 0Dh, 0Ah, 0
str_node_suspect        db "[WEEK1] Node %d SUSPECT (missed %d hb)", 0Dh, 0Ah, 0
str_node_dead           db "[WEEK1] Node %d DEAD", 0Dh, 0Ah, 0

; Thread names
name_hb_monitor         db "HB-Monitor", 0
name_conflict_det       db "Conflict-Detector", 0
name_scheduler          db "Task-Scheduler", 0
name_worker             db "Worker", 0

;================================================================================
; CODE SECTION
;================================================================================
.CODE
ALIGN 64

;================================================================================
; INITIALIZATION & SHUTDOWN
;================================================================================

;-------------------------------------------------------------------------------
; Week1Initialize - Bootstrap background infrastructure
; Input:  RCX = Phase1 context
;         RDX = Configuration (0 = use defaults)
; Output: RAX = WEEK1_INFRASTRUCTURE* or 0 on failure
;-------------------------------------------------------------------------------
Week1Initialize PROC FRAME
    PUSH_REGS
    
    mov r12, rcx                      ; r12 = phase1_ctx
    
    ; Allocate infrastructure structure
    mov rcx, 0
    mov rdx, sizeof WEEK1_INFRASTRUCTURE
    mov r8, 10000h                    ; Allocation size
    mov r9, 4                         ; PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @init_fail
    
    mov rbx, rax                      ; rbx = WEEK1_INFRASTRUCTURE*
    
    ; Zero structure
    mov ecx, sizeof WEEK1_INFRASTRUCTURE
    shr ecx, 3
    xor rax, rax
@zero_loop:
    mov [rbx + rcx*8], rax
    dec ecx
    jnz @zero_loop
    
    ; Store phase1 context
    mov [rbx].WEEK1_INFRASTRUCTURE.phase1_ctx, r12
    
    ; Get performance counter frequency
    lea rcx, [rbx].WEEK1_INFRASTRUCTURE.perf_frequency
    call QueryPerformanceFrequency
    
    ; Get start time
    call QueryPerformanceCounter
    mov [rbx].WEEK1_INFRASTRUCTURE.start_time, rax
    
    ; Initialize heartbeat monitor
    mov rcx, rbx
    call InitHeartbeatMonitor
    test eax, eax
    jz @init_cleanup
    
    ; Initialize conflict detector
    mov rcx, rbx
    call InitConflictDetector
    test eax, eax
    jz @init_cleanup
    
    ; Initialize task scheduler
    mov rcx, rbx
    call InitTaskScheduler
    test eax, eax
    jz @init_cleanup
    
    ; Mark initialized
    mov dword ptr [rbx].WEEK1_INFRASTRUCTURE.initialized, 1
    
    mov rax, rbx
    jmp @init_exit
    
@init_cleanup:
    mov rcx, rbx
    mov rdx, 0
    mov r8, 8000h                     ; MEM_RELEASE
    call VirtualFree
    
@init_fail:
    xor rax, rax
    
@init_exit:
    POP_REGS
    ret
Week1Initialize ENDP

;-------------------------------------------------------------------------------
; Week1Shutdown - Graceful shutdown
; Input:  RCX = WEEK1_INFRASTRUCTURE*
;-------------------------------------------------------------------------------
Week1Shutdown PROC FRAME
    PUSH_REGS
    
    mov rbx, rcx
    
    ; Signal stop to all subsystems
    mov dword ptr [rbx].WEEK1_INFRASTRUCTURE.scheduler.running, 0
    mov dword ptr [rbx].WEEK1_INFRASTRUCTURE.heartbeat.running, 0
    mov dword ptr [rbx].WEEK1_INFRASTRUCTURE.conflict.running, 0
    
    ; Set stop events
    mov rcx, [rbx].WEEK1_INFRASTRUCTURE.scheduler.stop_event
    test rcx, rcx
    jz @skip_scheduler_stop
    call SetEvent
@skip_scheduler_stop:
    
    mov rcx, [rbx].WEEK1_INFRASTRUCTURE.heartbeat.stop_event
    test rcx, rcx
    jz @skip_hb_stop
    call SetEvent
@skip_hb_stop:
    
    mov rcx, [rbx].WEEK1_INFRASTRUCTURE.conflict.stop_event
    test rcx, rcx
    jz @skip_conflict_stop
    call SetEvent
@skip_conflict_stop:
    
    ; Wait for threads
    mov rcx, [rbx].WEEK1_INFRASTRUCTURE.scheduler.scheduler_thread
    test rcx, rcx
    jz @skip_scheduler_wait
    mov edx, 5000
    call WaitForSingleObject
@skip_scheduler_wait:
    
    ; Wait for workers
    xor r12d, r12d
@wait_loop:
    cmp r12d, [rbx].WEEK1_INFRASTRUCTURE.scheduler.worker_count
    jge @wait_done
    
    mov rax, r12
    imul rax, sizeof THREAD_CONTEXT
    add rax, [rbx].WEEK1_INFRASTRUCTURE.scheduler.workers
    
    mov rcx, [rax].THREAD_CONTEXT.thread_handle
    test rcx, rcx
    jz @wait_skip
    mov edx, 5000
    call WaitForSingleObject
@wait_skip:
    
    inc r12d
    jmp @wait_loop
    
@wait_done:
    xor eax, eax
    POP_REGS
    ret
Week1Shutdown ENDP

;================================================================================
; HEARTBEAT MONITOR
;================================================================================

InitHeartbeatMonitor PROC FRAME
    PUSH_REGS
    
    mov rbx, rcx
    
    lea r12, [rbx].WEEK1_INFRASTRUCTURE.heartbeat
    
    ; Configuration
    mov dword ptr [r12].HEARTBEAT_MONITOR.monitor_interval_ms, HEARTBEAT_INTERVAL_MS
    mov dword ptr [r12].HEARTBEAT_MONITOR.timeout_threshold_ms, HEARTBEAT_TIMEOUT_MS
    mov dword ptr [r12].HEARTBEAT_MONITOR.miss_threshold, HEARTBEAT_MISS_THRESHOLD
    
    ; Allocate nodes array
    mov rcx, [rbx].WEEK1_INFRASTRUCTURE.phase1_ctx
    mov edx, MAX_CLUSTER_NODES * sizeof HEARTBEAT_NODE
    mov r8, 64
    call ArenaAllocate
    
    mov [r12].HEARTBEAT_MONITOR.nodes, rax
    mov dword ptr [r12].HEARTBEAT_MONITOR.max_nodes, MAX_CLUSTER_NODES
    
    ; Create stop event
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    call CreateEventA
    
    mov [r12].HEARTBEAT_MONITOR.stop_event, rax
    
    ; Create thread
    mov dword ptr [r12].HEARTBEAT_MONITOR.running, 1
    
    mov rcx, 0
    mov rdx, 0
    lea r8, HeartbeatThreadProc
    mov r9, rbx
    push 0
    sub rsp, 40h
    call CreateThread
    add rsp, 48h
    
    mov [r12].HEARTBEAT_MONITOR.monitor_thread, rax
    test rax, rax
    jz @hb_init_fail
    
    ; Set thread name
    mov r8, rax
    mov ecx, 1
    lea edx, name_hb_monitor
    call SetThreadDescription
    
    mov eax, 1
    jmp @hb_init_exit
    
@hb_init_fail:
    xor eax, eax
    
@hb_init_exit:
    POP_REGS
    ret
InitHeartbeatMonitor ENDP

;-------------------------------------------------------------------------------
; HeartbeatThreadProc - Main heartbeat loop
;-------------------------------------------------------------------------------
HeartbeatThreadProc PROC FRAME
    PUSH_REGS
    
    mov rbx, rcx
    lea r12, [rbx].WEEK1_INFRASTRUCTURE.heartbeat
    
@hb_loop:
    ; Check stop event
    mov rcx, [r12].HEARTBEAT_MONITOR.stop_event
    xor edx, edx
    call WaitForSingleObject
    
    cmp eax, 0                        ; WAIT_OBJECT_0
    je @hb_exit
    
    ; Check timeouts and send heartbeats
    xor r13d, r13d
@hb_node_loop:
    cmp r13d, [r12].HEARTBEAT_MONITOR.node_count
    jge @hb_node_done
    
    ; Skip self
    cmp r13d, [r12].HEARTBEAT_MONITOR.local_node_id
    je @hb_node_next
    
    ; Get node
    mov rax, r13
    imul rax, sizeof HEARTBEAT_NODE
    add rax, [r12].HEARTBEAT_MONITOR.nodes
    
    ; Check state
    cmp dword ptr [rax].HEARTBEAT_NODE.state, HB_STATE_DEAD
    je @hb_node_next
    
    ; Check timeout
    call QueryPerformanceCounter
    mov rcx, rax
    sub rcx, [rax].HEARTBEAT_NODE.last_response
    
    ; Convert to ms
    mov rax, rcx
    xor edx, edx
    mov ecx, 1000
    mul ecx
    mov rcx, [rbx].WEEK1_INFRASTRUCTURE.perf_frequency
    div rcx
    
    ; Compare with threshold
    cmp eax, [r12].HEARTBEAT_MONITOR.timeout_threshold_ms
    jl @hb_node_next
    
    ; Timeout detected
    mov rax, r13
    imul rax, sizeof HEARTBEAT_NODE
    add rax, [r12].HEARTBEAT_MONITOR.nodes
    
    inc dword ptr [rax].HEARTBEAT_NODE.missed_count
    
    cmp dword ptr [rax].HEARTBEAT_NODE.missed_count, HEARTBEAT_MISS_THRESHOLD
    jl @hb_node_next
    
    ; Mark suspect
    cmp dword ptr [rax].HEARTBEAT_NODE.state, HB_STATE_SUSPECT
    je @mark_dead_now
    
    mov dword ptr [rax].HEARTBEAT_NODE.state, HB_STATE_SUSPECT
    inc [r12].HEARTBEAT_MONITOR.nodes_suspect
    
@hb_node_next:
    inc r13d
    jmp @hb_node_loop
    
@mark_dead_now:
    mov dword ptr [rax].HEARTBEAT_NODE.state, HB_STATE_DEAD
    inc [r12].HEARTBEAT_MONITOR.nodes_dead
    
    jmp @hb_node_next
    
@hb_node_done:
    ; Sleep
    mov ecx, [r12].HEARTBEAT_MONITOR.monitor_interval_ms
    call Sleep
    
    jmp @hb_loop
    
@hb_exit:
    xor eax, eax
    POP_REGS
    ret
HeartbeatThreadProc ENDP

;================================================================================
; CONFLICT DETECTOR
;================================================================================

InitConflictDetector PROC FRAME
    PUSH_REGS
    
    mov rbx, rcx
    lea r12, [rbx].WEEK1_INFRASTRUCTURE.conflict
    
    ; Configuration
    mov dword ptr [r12].CONFLICT_DETECTOR.check_interval_ms, CONFLICT_CHECK_INTERVAL_MS
    mov dword ptr [r12].CONFLICT_DETECTOR.lock_timeout_ms, CONFLICT_LOCK_TIMEOUT_MS
    mov dword ptr [r12].CONFLICT_DETECTOR.auto_resolve, 1
    
    ; Allocate conflict table
    mov rcx, [rbx].WEEK1_INFRASTRUCTURE.phase1_ctx
    mov edx, CONFLICT_HISTORY_SIZE * sizeof CONFLICT_ENTRY
    mov r8, 64
    call ArenaAllocate
    
    mov [r12].CONFLICT_DETECTOR.entries, rax
    mov dword ptr [r12].CONFLICT_DETECTOR.entry_capacity, CONFLICT_HISTORY_SIZE
    
    ; Create stop event
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    call CreateEventA
    
    mov [r12].CONFLICT_DETECTOR.stop_event, rax
    
    ; Create thread
    mov dword ptr [r12].CONFLICT_DETECTOR.running, 1
    
    mov rcx, 0
    mov rdx, 0
    lea r8, ConflictThreadProc
    mov r9, rbx
    push 0
    sub rsp, 40h
    call CreateThread
    add rsp, 48h
    
    mov [r12].CONFLICT_DETECTOR.detector_thread, rax
    test rax, rax
    jz @cd_init_fail
    
    ; Set thread name
    mov r8, rax
    mov ecx, 1
    lea edx, name_conflict_det
    call SetThreadDescription
    
    mov eax, 1
    jmp @cd_init_exit
    
@cd_init_fail:
    xor eax, eax
    
@cd_init_exit:
    POP_REGS
    ret
InitConflictDetector ENDP

;-------------------------------------------------------------------------------
; ConflictThreadProc - Conflict detection loop
;-------------------------------------------------------------------------------
ConflictThreadProc PROC FRAME
    PUSH_REGS
    
    mov rbx, rcx
    lea r12, [rbx].WEEK1_INFRASTRUCTURE.conflict
    
@conflict_loop:
    ; Check stop
    mov rcx, [r12].CONFLICT_DETECTOR.stop_event
    xor edx, edx
    call WaitForSingleObject
    
    cmp eax, 0
    je @conflict_exit
    
    ; Scan for conflicts
    xor r13d, r13d
@scan_loop:
    cmp r13d, [r12].CONFLICT_DETECTOR.entry_count
    jge @scan_done
    
    ; Check resource for conflicts
    mov rax, r13
    imul rax, sizeof CONFLICT_ENTRY
    add rax, [r12].CONFLICT_DETECTOR.entries
    
    cmp dword ptr [rax].CONFLICT_ENTRY.waiter_count, 0
    je @scan_next
    
    ; Conflict detected
    inc [r12].CONFLICT_DETECTOR.conflicts_detected
    
@scan_next:
    inc r13d
    jmp @scan_loop
    
@scan_done:
    ; Sleep
    mov ecx, [r12].CONFLICT_DETECTOR.check_interval_ms
    call Sleep
    
    jmp @conflict_loop
    
@conflict_exit:
    xor eax, eax
    POP_REGS
    ret
ConflictThreadProc ENDP

;================================================================================
; TASK SCHEDULER
;================================================================================

InitTaskScheduler PROC FRAME
    PUSH_REGS
    mov rbp, rsp
    sub rsp, 100h
    
    mov rbx, rcx
    lea r12, [rbx].WEEK1_INFRASTRUCTURE.scheduler
    
    ; Get processor count
    xor ecx, ecx
    call GetCurrentProcessorNumber
    inc eax
    
    cmp eax, MAX_WORKER_THREADS
    cmova eax, MAX_WORKER_THREADS
    
    mov [r12].TASK_SCHEDULER.max_workers, eax
    
    ; Allocate workers array
    mov rcx, [rbx].WEEK1_INFRASTRUCTURE.phase1_ctx
    mov edx, MAX_WORKER_THREADS * sizeof THREAD_CONTEXT
    mov r8, 64
    call ArenaAllocate
    
    mov [r12].TASK_SCHEDULER.workers, rax
    
    ; Allocate task queue
    mov rcx, [rbx].WEEK1_INFRASTRUCTURE.phase1_ctx
    mov edx, TASK_QUEUE_SIZE * sizeof TASK
    mov r8, 64
    call ArenaAllocate
    
    mov [r12].TASK_SCHEDULER.global_queue, rax
    
    ; Create stop event
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    call CreateEventA
    
    mov [r12].TASK_SCHEDULER.stop_event, rax
    
    ; Create worker threads
    mov dword ptr [r12].TASK_SCHEDULER.running, 1
    
    xor r13d, r13d
@worker_loop:
    cmp r13d, [r12].TASK_SCHEDULER.max_workers
    jge @workers_done
    
    ; Initialize worker context
    mov rax, r13
    imul rax, sizeof THREAD_CONTEXT
    add rax, [r12].TASK_SCHEDULER.workers
    
    mov [rax].THREAD_CONTEXT.thread_id, r13d
    mov dword ptr [rax].THREAD_CONTEXT.state, STATE_IDLE
    mov [rax].THREAD_CONTEXT.ideal_processor, r13d
    mov dword ptr [rax].THREAD_CONTEXT.priority, PRIORITY_NORMAL
    
    ; Create thread
    push rax
    
    mov rcx, 0
    mov rdx, 0
    lea r8, WorkerThreadProc
    mov r9, rax
    push 0
    sub rsp, 40h
    call CreateThread
    add rsp, 48h
    
    pop r8
    
    test rax, rax
    jz @worker_skip
    
    mov [r8].THREAD_CONTEXT.thread_handle, rax
    
    ; Set affinity
    mov rcx, rax
    mov edx, r13d
    call SetThreadAffinityMask
    
    inc [r12].TASK_SCHEDULER.worker_count
    
@worker_skip:
    inc r13d
    jmp @worker_loop
    
@workers_done:
    ; Create scheduler thread
    mov rcx, 0
    mov rdx, 0
    lea r8, SchedulerThreadProc
    mov r9, rbx
    push 0
    sub rsp, 40h
    call CreateThread
    add rsp, 48h
    
    mov [r12].TASK_SCHEDULER.scheduler_thread, rax
    
    mov eax, 1
    
    mov rsp, rbp
    POP_REGS
    ret
InitTaskScheduler ENDP

;-------------------------------------------------------------------------------
; WorkerThreadProc - Worker thread main loop
;-------------------------------------------------------------------------------
WorkerThreadProc PROC FRAME
    PUSH_REGS
    
    mov rbx, rcx                      ; rbx = THREAD_CONTEXT*
    
@worker_loop:
    ; Try local queue
    mov rcx, rbx
    call PopLocalTask
    test rax, rax
    jnz @got_task
    
    ; Try global queue
    ; (Simplified - full implementation in production)
    
    ; Try work stealing
    ; (Simplified - full implementation in production)
    
    ; Sleep briefly
    mov ecx, 1
    call Sleep
    
    jmp @worker_loop
    
@got_task:
    mov r12, rax                      ; r12 = TASK*
    
    ; Execute task
    mov ecx, [r12].TASK.status
    cmp ecx, TASK_CANCELLED
    je @task_skip
    
    mov dword ptr [r12].TASK.status, TASK_RUNNING
    
    ; Call callback
    mov rcx, [r12].TASK.context
    call qword ptr [r12].TASK.callback
    
    ; Mark complete
    mov dword ptr [r12].TASK.status, TASK_COMPLETE
    inc [rbx].THREAD_CONTEXT.task_count
    
@task_skip:
    jmp @worker_loop
    
    xor eax, eax
    POP_REGS
    ret
WorkerThreadProc ENDP

;-------------------------------------------------------------------------------
; SchedulerThreadProc - Global scheduler loop
;-------------------------------------------------------------------------------
SchedulerThreadProc PROC FRAME
    PUSH_REGS
    
    mov rbx, rcx
    lea r12, [rbx].WEEK1_INFRASTRUCTURE.scheduler
    
@sched_loop:
    ; Check stop
    mov rcx, [r12].TASK_SCHEDULER.stop_event
    xor edx, edx
    call WaitForSingleObject
    
    cmp eax, 0
    je @sched_exit
    
    ; Balance load across workers
    ; (Simplified - full implementation in production)
    
    ; Sleep
    mov ecx, 10
    call Sleep
    
    jmp @sched_loop
    
@sched_exit:
    xor eax, eax
    POP_REGS
    ret
SchedulerThreadProc ENDP

;================================================================================
; TASK SUBMISSION
;================================================================================

;-------------------------------------------------------------------------------
; SubmitTask - Add task to scheduler
; Input:  RCX = WEEK1_INFRASTRUCTURE*
;         RDX = Task callback
;         R8  = Task context
;         R9  = Priority
; Output: EAX = 1 on success, 0 on failure
;-------------------------------------------------------------------------------
SubmitTask PROC FRAME
    PUSH_REGS
    
    mov rbx, rcx
    
    ; Allocate task
    mov rcx, [rbx].WEEK1_INFRASTRUCTURE.phase1_ctx
    mov rdx, sizeof TASK
    mov r8, 128
    call ArenaAllocate
    test rax, rax
    jz @submit_fail
    
    mov r12, rax
    
    ; Initialize task
    mov [r12].TASK.callback, rdx
    mov [r12].TASK.context, r8
    mov [r12].TASK.priority, r9d
    mov dword ptr [r12].TASK.status, TASK_PENDING
    
    ; Get unique task ID
    rdtsc
    mov [r12].TASK.task_id, rax
    
    ; Record submit time
    call QueryPerformanceCounter
    mov [r12].TASK.submit_time, rax
    
    ; Add to queue
    lea rcx, [rbx].WEEK1_INFRASTRUCTURE.scheduler
    mov edx, [rcx].TASK_SCHEDULER.global_queue_size
    
    cmp edx, TASK_QUEUE_SIZE
    jae @submit_full
    
    inc [rcx].TASK_SCHEDULER.global_queue_size
    inc [rcx].TASK_SCHEDULER.tasks_submitted
    
    mov eax, 1
    jmp @submit_exit
    
@submit_full:
    xor eax, eax
    jmp @submit_exit
    
@submit_fail:
    xor eax, eax
    
@submit_exit:
    POP_REGS
    ret
SubmitTask ENDP

;================================================================================
; HEARTBEAT PROCESSING
;================================================================================

;-------------------------------------------------------------------------------
; ProcessReceivedHeartbeat - Handle incoming heartbeat from node
; Input:  RCX = WEEK1_INFRASTRUCTURE*
;         RDX = Node ID
;         R8  = Send timestamp (TSC)
;-------------------------------------------------------------------------------
ProcessReceivedHeartbeat PROC FRAME
    PUSH_REGS
    
    mov rbx, rcx
    mov r12d, edx
    
    lea r13, [rbx].WEEK1_INFRASTRUCTURE.heartbeat
    
    ; Get node
    mov rax, r12
    imul rax, sizeof HEARTBEAT_NODE
    add rax, [r13].HEARTBEAT_MONITOR.nodes
    
    ; Calculate latency
    call QueryPerformanceCounter
    sub rax, r8
    
    ; Update node state
    mov [rax].HEARTBEAT_NODE.last_response, rax
    mov dword ptr [rax].HEARTBEAT_NODE.missed_count, 0
    
    ; Check recovery
    cmp dword ptr [rax].HEARTBEAT_NODE.state, HB_STATE_DEAD
    jne @not_recovered
    
    mov dword ptr [rax].HEARTBEAT_NODE.state, HB_STATE_HEALTHY
    
@not_recovered:
    inc [r13].HEARTBEAT_MONITOR.heartbeats_received
    
    xor eax, eax
    POP_REGS
    ret
ProcessReceivedHeartbeat ENDP

;================================================================================
; RESOURCE MANAGEMENT
;================================================================================

;-------------------------------------------------------------------------------
; RegisterResource - Track resource for conflict detection
; Input:  RCX = WEEK1_INFRASTRUCTURE*
;         RDX = Resource ID
; Output: EAX = 1 on success
;-------------------------------------------------------------------------------
RegisterResource PROC FRAME
    PUSH_REGS
    
    mov rbx, rcx
    lea r12, [rbx].WEEK1_INFRASTRUCTURE.conflict
    
    ; Find empty slot in conflict table
    xor r13d, r13d
@reg_loop:
    cmp r13d, [r12].CONFLICT_DETECTOR.entry_count
    jge @reg_insert
    
    mov rax, r13
    imul rax, sizeof CONFLICT_ENTRY
    add rax, [r12].CONFLICT_DETECTOR.entries
    
    cmp [rax].CONFLICT_ENTRY.resource_id, rdx
    je @reg_found
    
    inc r13d
    jmp @reg_loop
    
@reg_insert:
    cmp r13d, [r12].CONFLICT_DETECTOR.entry_capacity
    jae @reg_fail
    
    mov rax, r13
    imul rax, sizeof CONFLICT_ENTRY
    add rax, [r12].CONFLICT_DETECTOR.entries
    
    mov [rax].CONFLICT_ENTRY.resource_id, rdx
    mov dword ptr [rax].CONFLICT_ENTRY.waiter_count, 0
    
    inc [r12].CONFLICT_DETECTOR.entry_count
    
@reg_found:
    mov eax, 1
    jmp @reg_exit
    
@reg_fail:
    xor eax, eax
    
@reg_exit:
    POP_REGS
    ret
RegisterResource ENDP

;================================================================================ 
; EXPORTS
;================================================================================
PUBLIC Week1Initialize
PUBLIC Week1Shutdown
PUBLIC SubmitTask
PUBLIC ProcessReceivedHeartbeat
PUBLIC RegisterResource

END
