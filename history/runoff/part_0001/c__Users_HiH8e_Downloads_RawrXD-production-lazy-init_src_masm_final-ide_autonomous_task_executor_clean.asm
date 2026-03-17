; autonomous_task_executor_clean.asm - Complete production-ready task executor
; Provides autonomous task scheduling with full queue management, threading, and retry logic

option casemap:none

.code

; External Win32 functions
EXTERN GetTickCount:PROC
EXTERN CreateEventA:PROC
EXTERN SetEvent:PROC
EXTERN WaitForSingleObject:PROC
EXTERN CloseHandle:PROC
EXTERN Sleep:PROC
EXTERN CreateThread:PROC
EXTERN ExitThread:PROC
EXTERN WaitForMultipleObjects:PROC

; External utility functions
EXTERN console_log:PROC
EXTERN asm_malloc:PROC
EXTERN asm_free:PROC
EXTERN asm_mutex_create:PROC
EXTERN asm_mutex_lock:PROC
EXTERN asm_mutex_unlock:PROC
EXTERN asm_mutex_destroy:PROC

; External agentic system functions (real implementations)
EXTERN AgenticEngine_ExecuteTask:PROC
EXTERN masm_detect_failure:PROC
EXTERN masm_puppeteer_correct_response:PROC

; Task structure (64 bytes)
TASK_ENTRY STRUCT
    task_id         QWORD ?     ; Unique task ID
    goal_ptr        QWORD ?     ; Pointer to goal string
    priority        DWORD ?     ; Task priority (0-10)
    auto_retry      DWORD ?     ; Auto retry flag
    retry_count     DWORD ?     ; Current retry count
    max_retries     DWORD ?     ; Maximum retries
    status          DWORD ?     ; 0=pending, 1=running, 2=complete, 3=failed
    created_time    QWORD ?     ; Creation timestamp
    next_task       QWORD ?     ; Pointer to next task
TASK_ENTRY ENDS

; Task queue structure
TASK_QUEUE STRUCT
    head            QWORD ?     ; Head of queue
    tail            QWORD ?     ; Tail of queue
    count           DWORD ?     ; Number of tasks
    max_tasks       DWORD ?     ; Maximum tasks
    queue_mutex     QWORD ?     ; Mutex for thread safety
TASK_QUEUE ENDS

.data?
g_task_queue        TASK_QUEUE <>
g_worker_thread     QWORD ?
g_shutdown_event    QWORD ?
g_task_event        QWORD ?
g_next_task_id      QWORD ?

.code

; autonomous_task_schedule(goal: LPCSTR, priority: DWORD, autoRetry: BYTE)
; Schedule a task for autonomous execution
PUBLIC autonomous_task_schedule
autonomous_task_schedule PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov rbx, rcx        ; goal string
    mov esi, edx        ; priority
    mov edi, r8d        ; auto retry
    
    ; Allocate task entry
    mov rcx, SIZEOF TASK_ENTRY
    call asm_malloc
    test rax, rax
    jz schedule_fail
    
    mov r9, rax         ; task entry
    
    ; Generate unique task ID
    mov rcx, [g_next_task_id]
    inc rcx
    mov [g_next_task_id], rcx
    mov [r9 + TASK_ENTRY.task_id], rcx
    
    ; Fill task entry
    mov [r9 + TASK_ENTRY.goal_ptr], rbx
    mov [r9 + TASK_ENTRY.priority], esi
    mov [r9 + TASK_ENTRY.auto_retry], edi
    mov DWORD PTR [r9 + TASK_ENTRY.retry_count], 0
    mov DWORD PTR [r9 + TASK_ENTRY.max_retries], 3
    mov DWORD PTR [r9 + TASK_ENTRY.status], 0  ; pending
    mov QWORD PTR [r9 + TASK_ENTRY.next_task], 0
    
    ; Get timestamp
    sub rsp, 20h
    call GetTickCount
    add rsp, 20h
    mov [r9 + TASK_ENTRY.created_time], rax
    
    ; Add to queue (thread-safe)
    mov rcx, [g_task_queue.queue_mutex]
    call asm_mutex_lock
    
    ; Add to tail of queue
    mov rax, [g_task_queue.tail]
    test rax, rax
    jz queue_empty
    
    ; Queue not empty, add to tail
    mov [rax + TASK_ENTRY.next_task], r9
    mov [g_task_queue.tail], r9
    jmp queue_added
    
queue_empty:
    ; Queue empty, set as head and tail
    mov [g_task_queue.head], r9
    mov [g_task_queue.tail], r9
    
queue_added:
    inc [g_task_queue.count]
    
    mov rcx, [g_task_queue.queue_mutex]
    call asm_mutex_unlock
    
    ; Signal worker thread
    mov rcx, [g_task_event]
    sub rsp, 20h
    call SetEvent
    add rsp, 20h
    
    ; Log the scheduling request
    lea rcx, szTaskScheduled
    sub rsp, 20h
    call console_log
    add rsp, 20h
    
    ; Return task ID
    mov rax, [r9 + TASK_ENTRY.task_id]
    jmp schedule_done
    
schedule_fail:
    xor rax, rax
    
schedule_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
autonomous_task_schedule ENDP

; task_worker_thread() - Main worker thread function
ALIGN 16
task_worker_thread PROC
    push rbx
    push rsi
    sub rsp, 32
    
worker_loop:
    ; Wait for task event or shutdown event - build handles array
    sub rsp, 40h            ; local space + shadow
    mov rax, [g_task_event]
    mov [rsp+20h], rax      ; handles[0] = task_event
    mov rax, [g_shutdown_event]
    mov [rsp+28h], rax      ; handles[1] = shutdown_event
    
    mov ecx, 2              ; nCount = 2
    lea rdx, [rsp+20h]      ; lpHandles = &handles
    xor r8d, r8d            ; bWaitAll = FALSE
    mov r9d, 5000           ; dwMilliseconds = 5000
    call WaitForMultipleObjects
    add rsp, 40h
    
    cmp eax, 1              ; WAIT_OBJECT_0 + 1 = shutdown
    je worker_shutdown
    
    cmp eax, 0              ; WAIT_OBJECT_0 = task event
    jne worker_loop         ; Timeout or error, continue
    
    ; Process tasks
process_tasks:
    ; Get next task from queue
    mov rcx, [g_task_queue.queue_mutex]
    call asm_mutex_lock
    
    mov rbx, [g_task_queue.head]
    test rbx, rbx
    jz no_tasks
    
    ; Remove from head
    mov rax, [rbx + TASK_ENTRY.next_task]
    mov [g_task_queue.head], rax
    
    ; If queue now empty, clear tail
    test rax, rax
    jnz queue_not_empty
    mov QWORD PTR [g_task_queue.tail], 0
    
queue_not_empty:
    dec [g_task_queue.count]
    
    mov rcx, [g_task_queue.queue_mutex]
    call asm_mutex_unlock
    
    ; Execute task
    mov rcx, rbx
    call execute_task
    
    ; Free task entry
    mov rcx, rbx
    call asm_free
    
    jmp process_tasks       ; Check for more tasks
    
no_tasks:
    mov rcx, [g_task_queue.queue_mutex]
    call asm_mutex_unlock
    jmp worker_loop
    
worker_shutdown:
    lea rcx, szWorkerShutdown
    sub rsp, 20h
    call console_log
    add rsp, 20h
    
    add rsp, 32
    pop rsi
    pop rbx
    
    xor rcx, rcx
    call ExitThread
task_worker_thread ENDP

; execute_task(task: rcx) - Execute a single task
ALIGN 16
execute_task PROC
    push rbx
    sub rsp, 32
    
    mov rbx, rcx            ; task entry
    
    ; Mark as running
    mov DWORD PTR [rbx + TASK_ENTRY.status], 1
    
    ; Log task execution
    lea rcx, szTaskExecuting
    sub rsp, 20h
    call console_log
    add rsp, 20h
    
    ; Execute task via AgenticEngine (real implementation)
    mov rcx, [rbx + TASK_ENTRY.goal_ptr]
    sub rsp, 20h
    call AgenticEngine_ExecuteTask
    add rsp, 20h
    mov rsi, rax            ; rsi = agent response pointer
    
    ; Detect failures in agent response
    mov rcx, rsi
    sub rsp, 20h
    call masm_detect_failure
    add rsp, 20h
    test rax, rax
    jnz task_failed         ; Non-zero = failure detected
    
    ; Task succeeded
    mov DWORD PTR [rbx + TASK_ENTRY.status], 2  ; complete
    lea rcx, szTaskComplete
    sub rsp, 20h
    call console_log
    add rsp, 20h
    jmp execute_done
    
task_failed:
    ; Task failed, check retry
    mov eax, [rbx + TASK_ENTRY.retry_count]
    inc eax
    mov [rbx + TASK_ENTRY.retry_count], eax
    
    cmp eax, [rbx + TASK_ENTRY.max_retries]
    jge task_max_retries
    
    ; Check auto retry flag
    cmp DWORD PTR [rbx + TASK_ENTRY.auto_retry], 0
    je task_no_retry
    
    ; Reschedule task
    lea rcx, szTaskRetrying
    sub rsp, 20h
    call console_log
    add rsp, 20h
    
    ; Exponential backoff: 2^retry_count * 100ms
    mov ecx, 1
    mov edx, [rbx + TASK_ENTRY.retry_count]
    shl ecx, cl             ; 2^retry_count
    imul ecx, 100           ; * 100ms
    sub rsp, 20h
    call Sleep
    add rsp, 20h
    
    ; Reset status to pending and re-queue
    mov DWORD PTR [rbx + TASK_ENTRY.status], 0
    
    ; Re-add to queue (simplified - add to head for immediate retry)
    mov rcx, [g_task_queue.queue_mutex]
    call asm_mutex_lock
    
    mov rax, [g_task_queue.head]
    mov [rbx + TASK_ENTRY.next_task], rax
    mov [g_task_queue.head], rbx
    
    ; If queue was empty, set as tail too
    cmp QWORD PTR [g_task_queue.tail], 0
    jne retry_queue_done
    mov [g_task_queue.tail], rbx
    
retry_queue_done:
    inc [g_task_queue.count]
    
    mov rcx, [g_task_queue.queue_mutex]
    call asm_mutex_unlock
    
    jmp execute_done
    
task_max_retries:
task_no_retry:
    ; Task permanently failed
    mov DWORD PTR [rbx + TASK_ENTRY.status], 3  ; failed
    lea rcx, szTaskFailed
    sub rsp, 20h
    call console_log
    add rsp, 20h
    
execute_done:
    add rsp, 32
    pop rbx
    ret
execute_task ENDP

; ai_orchestration_coordinator_init() - Initialize coordinator
PUBLIC ai_orchestration_coordinator_init
ai_orchestration_coordinator_init PROC
    push rbx
    sub rsp, 32
    
    ; Initialize task queue mutex
    call asm_mutex_create
    test rax, rax
    jz init_fail
    mov [g_task_queue.queue_mutex], rax
    
    ; Initialize queue
    mov QWORD PTR [g_task_queue.head], 0
    mov QWORD PTR [g_task_queue.tail], 0
    mov DWORD PTR [g_task_queue.count], 0
    mov DWORD PTR [g_task_queue.max_tasks], 100
    
    ; Create task event (auto-reset, initially nonsignaled)
    xor rcx, rcx                    ; lpSecurityAttributes = NULL
    xor edx, edx                    ; bManualReset = FALSE (auto-reset)
    xor r8d, r8d                    ; bInitialState = FALSE
    xor r9d, r9d                    ; lpName = NULL
    sub rsp, 20h
    call CreateEventA
    add rsp, 20h
    test rax, rax
    jz init_fail
    mov [g_task_event], rax
    
    ; Create shutdown event (manual-reset, initially nonsignaled)
    xor rcx, rcx                    ; lpSecurityAttributes = NULL
    mov edx, 1                      ; bManualReset = TRUE (manual-reset)
    xor r8d, r8d                    ; bInitialState = FALSE
    xor r9d, r9d                    ; lpName = NULL
    sub rsp, 20h
    call CreateEventA
    add rsp, 20h
    test rax, rax
    jz init_fail
    mov [g_shutdown_event], rax
    
    ; Create worker thread
    ; HANDLE CreateThread(lpAttr, dwStackSize, lpStartAddr, lpParam, dwFlags, lpThreadId)
    xor rcx, rcx                    ; lpThreadAttributes = NULL
    xor rdx, rdx                    ; dwStackSize = 0 (default)
    lea r8, task_worker_thread      ; lpStartAddress
    xor r9, r9                      ; lpParameter = NULL
    sub rsp, 30h                    ; shadow + 2 stack params
    mov dword ptr [rsp+20h], 0      ; dwCreationFlags = 0
    mov qword ptr [rsp+28h], 0      ; lpThreadId = NULL
    call CreateThread
    add rsp, 30h
    test rax, rax
    jz init_fail
    mov [g_worker_thread], rax
    
    ; Initialize task ID counter
    mov qword ptr [g_next_task_id], 1000
    
    ; Log initialization
    lea rcx, szInitComplete
    sub rsp, 20h
    call console_log
    add rsp, 20h
    
    mov eax, 1                      ; Success
    jmp init_done
    
init_fail:
    xor eax, eax                    ; Failure
    
init_done:
    add rsp, 32
    pop rbx
    ret
ai_orchestration_coordinator_init ENDP

; ai_orchestration_coordinator_shutdown() - Cleanup coordinator
PUBLIC ai_orchestration_coordinator_shutdown
ai_orchestration_coordinator_shutdown PROC
    push rbx
    sub rsp, 32
    
    ; Signal shutdown
    mov rcx, [g_shutdown_event]
    test rcx, rcx
    jz shutdown_no_event
    sub rsp, 20h
    call SetEvent
    add rsp, 20h
    
shutdown_no_event:
    ; Wait for worker thread to finish
    mov rcx, [g_worker_thread]
    test rcx, rcx
    jz shutdown_no_thread
    
    mov rdx, 5000           ; 5 second timeout
    sub rsp, 20h
    call WaitForSingleObject
    add rsp, 20h
    
    mov rcx, [g_worker_thread]
    sub rsp, 20h
    call CloseHandle
    add rsp, 20h
    
shutdown_no_thread:
    ; Close events
    mov rcx, [g_task_event]
    test rcx, rcx
    jz shutdown_no_task_event
    sub rsp, 20h
    call CloseHandle
    add rsp, 20h
    
shutdown_no_task_event:
    mov rcx, [g_shutdown_event]
    test rcx, rcx
    jz shutdown_no_shutdown_event
    sub rsp, 20h
    call CloseHandle
    add rsp, 20h
    
shutdown_no_shutdown_event:
    ; Clean up remaining tasks
    mov rcx, [g_task_queue.queue_mutex]
    test rcx, rcx
    jz shutdown_done
    
    call asm_mutex_lock
    
    ; Free all remaining tasks
cleanup_tasks:
    mov rbx, [g_task_queue.head]
    test rbx, rbx
    jz cleanup_done
    
    mov rax, [rbx + TASK_ENTRY.next_task]
    mov [g_task_queue.head], rax
    
    mov rcx, rbx
    call asm_free
    
    jmp cleanup_tasks
    
cleanup_done:
    mov qword ptr [g_task_queue.tail], 0
    mov dword ptr [g_task_queue.count], 0
    
    mov rcx, [g_task_queue.queue_mutex]
    call asm_mutex_unlock
    
    ; Destroy mutex
    mov rcx, [g_task_queue.queue_mutex]
    call asm_mutex_destroy
    
    lea rcx, szShutdownComplete
    sub rsp, 20h
    call console_log
    add rsp, 20h
    
shutdown_done:
    add rsp, 32
    pop rbx
    ret
ai_orchestration_coordinator_shutdown ENDP

; get_task_queue_status(status_ptr: rcx) -> void
; Fills status structure with queue information
PUBLIC get_task_queue_status
get_task_queue_status PROC
    push rbx
    sub rsp, 32
    
    test rcx, rcx
    jz status_done
    
    mov rbx, rcx
    
    ; Lock queue for reading
    mov rcx, [g_task_queue.queue_mutex]
    call asm_mutex_lock
    
    ; Copy queue status
    mov eax, [g_task_queue.count]
    mov [rbx], eax              ; current count
    
    mov eax, [g_task_queue.max_tasks]
    mov [rbx + 4], eax          ; max tasks
    
    mov rax, [g_next_task_id]
    mov [rbx + 8], rax          ; next task ID
    
    mov rcx, [g_task_queue.queue_mutex]
    call asm_mutex_unlock
    
status_done:
    add rsp, 32
    pop rbx
    ret
get_task_queue_status ENDP

; get_task_status(task_id: rcx) -> eax (status or -1 if not found)
PUBLIC get_task_status
get_task_status PROC
    push rbx
    push rsi
    sub rsp, 32
    
    mov rsi, rcx                    ; task_id
    
    ; Lock queue for search
    mov rcx, [g_task_queue.queue_mutex]
    call asm_mutex_lock
    
    ; Search for task
    mov rbx, [g_task_queue.head]
    
search_loop:
    test rbx, rbx
    jz task_not_found
    
    cmp [rbx + TASK_ENTRY.task_id], rsi
    je task_found
    
    mov rbx, [rbx + TASK_ENTRY.next_task]
    jmp search_loop
    
task_found:
    mov eax, [rbx + TASK_ENTRY.status]
    jmp status_done
    
task_not_found:
    mov eax, -1                     ; Task not found
    
status_done:
    push rax
    mov rcx, [g_task_queue.queue_mutex]
    call asm_mutex_unlock
    pop rax
    
    add rsp, 32
    pop rsi
    pop rbx
    ret
get_task_status ENDP

; get_queue_stats() -> eax (returns count)
PUBLIC get_queue_stats
get_queue_stats PROC
    mov eax, [g_task_queue.count]
    ret
get_queue_stats ENDP

; output_pane_init() - Stub for compatibility
PUBLIC output_pane_init
output_pane_init PROC
    mov eax, 1
    ret
output_pane_init ENDP

.data
; String constants
szTaskScheduled     DB "Task scheduled for autonomous execution", 0
szTaskExecuting     DB "Executing autonomous task", 0
szTaskComplete      DB "Task completed successfully", 0
szTaskFailed        DB "Task failed permanently", 0
szTaskRetrying      DB "Task failed, retrying with exponential backoff", 0
szWorkerShutdown    DB "Worker thread shutting down", 0
szInitComplete      DB "AI Orchestration Coordinator initialized successfully", 0
szShutdownComplete  DB "AI Orchestration Coordinator shutdown complete", 0

END
