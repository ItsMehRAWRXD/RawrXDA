;==============================================================================
; autonomous_task_executor.asm
; Autonomous Agent Task Execution Engine
; Execute tasks autonomously without user triggers
; Production MASM64 - ml64 compatible
;==============================================================================

option casemap:none
option noscoped
option proc:private

include windows.inc

includelib kernel32.lib
includelib user32.lib

; Win32 API externs
extern masm_malloc : proc
extern masm_free : proc
; CreateThread, WaitForSingleObject, CloseHandle, OutputDebugStringA, GetTickCount are in windows.inc

; Constants
TASK_STATE_SIZE         EQU 96
MAX_PENDING_TASKS       EQU 32
TASK_EXECUTION_TIMEOUT  EQU 300
WAIT_OBJECT_0           EQU 0

;==============================================================================
; DATA SEGMENT
;==============================================================================

.data
    ; Task pool and state
    taskPool                QWORD MAX_PENDING_TASKS DUP(0)
    taskPoolLock            QWORD 0
    pendingTaskCount        QWORD 0
    executingTaskCount      QWORD 0
    completedTaskCount      QWORD 0
    failedTaskCount         QWORD 0
    
    ; Configuration
    autonomousExecutionEnabled QWORD 1
    defaultTaskPriority     DWORD 50
    defaultAutoRetry        BYTE 1
    
    ; External handles
    outputLogHandle         QWORD 0
    agenticChatHandle       QWORD 0
    failureDetectorHandle   QWORD 0

.code

;==============================================================================
; PUBLIC: autonomous_task_executor_init() -> eax (1=success, 0=failure)
; Initialize autonomous task execution engine
;==============================================================================
PUBLIC autonomous_task_executor_init
ALIGN 16
autonomous_task_executor_init PROC
    push rbx
    sub rsp, 32
    
    ; Initialize task pool to NULL
    xor rbx, rbx
    lea rcx, taskPool
    
init_loop:
    cmp rbx, MAX_PENDING_TASKS
    jge init_done
    mov QWORD PTR [rcx + rbx*8], 0
    inc rbx
    jmp init_loop
    
init_done:
    mov QWORD PTR pendingTaskCount, 0
    mov QWORD PTR executingTaskCount, 0
    mov QWORD PTR completedTaskCount, 0
    mov QWORD PTR failedTaskCount, 0
    
    mov eax, 1
    add rsp, 32
    pop rbx
    ret
autonomous_task_executor_init ENDP

;==============================================================================
; PUBLIC: autonomous_task_submit(goal: rcx, priority: edx) -> eax (taskId)
; Submit a new autonomous task for execution
; rcx = goal description string
; edx = priority (0-100)
; Returns: eax = task ID (or 0 on failure)
;==============================================================================
PUBLIC autonomous_task_submit
ALIGN 16
autonomous_task_submit PROC
    push rbx
    push r12
    push r13
    sub rsp, 48
    
    mov r12, rcx            ; Save goal
    mov r13d, edx           ; Save priority
    
    ; Allocate task state
    mov rcx, TASK_STATE_SIZE
    call masm_malloc
    test rax, rax
    jz submit_fail
    
    mov rbx, rax            ; rbx = new task
    
    ; Initialize task fields
    mov QWORD PTR [rbx + 0], rax        ; taskId = address
    mov QWORD PTR [rbx + 8], r12        ; goalDescription = goal
    mov DWORD PTR [rbx + 48], 0         ; status = pending
    mov DWORD PTR [rbx + 52], r13d      ; priority
    mov DWORD PTR [rbx + 56], 0         ; retryCount = 0
    mov DWORD PTR [rbx + 60], 3         ; maxRetries = 3
    mov BYTE PTR [rbx + 88], 1          ; autoRetry = TRUE
    
    ; Get current time
    call GetTickCount
    mov DWORD PTR [rbx + 24], eax       ; createdTime
    
    ; Find slot in task pool
    lea rcx, taskPool
    xor r8, r8
    
find_slot:
    cmp r8, MAX_PENDING_TASKS
    jge submit_fail
    mov rax, QWORD PTR [rcx + r8*8]
    test rax, rax
    jz found_slot
    inc r8
    jmp find_slot
    
found_slot:
    mov QWORD PTR [rcx + r8*8], rbx
    mov rcx, QWORD PTR pendingTaskCount
    inc rcx
    mov QWORD PTR pendingTaskCount, rcx
    
    ; Return task ID
    mov rax, rbx
    add rsp, 48
    pop r13
    pop r12
    pop rbx
    ret
    
submit_fail:
    xor eax, eax
    add rsp, 48
    pop r13
    pop r12
    pop rbx
    ret
autonomous_task_submit ENDP

;==============================================================================
; PUBLIC: autonomous_task_execute(taskId: rcx) -> eax (1=success)
; Start execution of a pending task
;==============================================================================
PUBLIC autonomous_task_execute
ALIGN 16
autonomous_task_execute PROC
    push rbx
    sub rsp, 32
    
    mov rbx, rcx            ; rbx = task
    
    ; Check if already executing
    mov eax, DWORD PTR [rbx + 48]
    cmp eax, 0              ; status = pending?
    jne exec_fail
    
    ; Update status to running
    mov DWORD PTR [rbx + 48], 1         ; status = running
    
    ; Record start time
    call GetTickCount
    mov DWORD PTR [rbx + 32], eax       ; startedTime
    
    ; Create worker thread
    mov rcx, rbx
    call CreateThread
    mov QWORD PTR [rbx + 16], rax       ; hExecutionThread
    
    ; Update counters
    mov rcx, QWORD PTR executingTaskCount
    inc rcx
    mov QWORD PTR executingTaskCount, rcx
    
    mov eax, 1
    add rsp, 32
    pop rbx
    ret
    
exec_fail:
    xor eax, eax
    add rsp, 32
    pop rbx
    ret
autonomous_task_execute ENDP

;==============================================================================
; PUBLIC: autonomous_task_get_status(taskId: rcx) -> eax (status)
; Get status of a task (0=pending, 1=running, 2=completed, 3=failed)
;==============================================================================
PUBLIC autonomous_task_get_status
ALIGN 16
autonomous_task_get_status PROC
    mov rax, rcx
    test rax, rax
    jz status_error
    
    mov eax, DWORD PTR [rax + 48]
    ret
    
status_error:
    mov eax, 3              ; 3 = failed
    ret
autonomous_task_get_status ENDP

;==============================================================================
; PUBLIC: autonomous_task_get_result(taskId: rcx) -> eax (result string)
; Get result string from completed task
;==============================================================================
PUBLIC autonomous_task_get_result
ALIGN 16
autonomous_task_get_result PROC
    mov rax, rcx
    test rax, rax
    jz result_error
    
    mov rax, QWORD PTR [rax + 72]       ; result pointer
    ret
    
result_error:
    xor eax, eax
    ret
autonomous_task_get_result ENDP

;==============================================================================
; PUBLIC: autonomous_task_wait(taskId: rcx, timeout: edx) -> eax (status)
; Wait for a task to complete with timeout
;==============================================================================
PUBLIC autonomous_task_wait
ALIGN 16
autonomous_task_wait PROC
    push rbx
    sub rsp, 32
    
    mov rbx, rcx            ; rbx = task
    mov r8d, edx            ; r8d = timeout
    
    ; Wait for thread
    mov rcx, QWORD PTR [rbx + 16]       ; hExecutionThread
    mov edx, r8d
    call WaitForSingleObject
    
    cmp eax, WAIT_OBJECT_0
    je wait_done
    
    ; Timeout or error
    mov DWORD PTR [rbx + 48], 3         ; status = failed
    
wait_done:
    mov eax, DWORD PTR [rbx + 48]
    add rsp, 32
    pop rbx
    ret
autonomous_task_wait ENDP

;==============================================================================
; PUBLIC: autonomous_task_cancel(taskId: rcx) -> eax (1=success)
; Cancel an executing or pending task
;==============================================================================
PUBLIC autonomous_task_cancel
ALIGN 16
autonomous_task_cancel PROC
    push rbx
    sub rsp, 32
    
    mov rbx, rcx            ; rbx = task
    
    ; Mark as failed
    mov DWORD PTR [rbx + 48], 3         ; status = failed
    
    ; Wait for thread to terminate
    mov rcx, QWORD PTR [rbx + 16]
    mov edx, TASK_EXECUTION_TIMEOUT
    call WaitForSingleObject
    
    ; Close thread handle
    mov rcx, QWORD PTR [rbx + 16]
    call CloseHandle
    
    ; Free task
    mov rcx, rbx
    call masm_free
    
    mov eax, 1
    add rsp, 32
    pop rbx
    ret
autonomous_task_cancel ENDP

;==============================================================================
; PUBLIC: autonomous_task_executor_shutdown() -> eax (1=success)
; Shutdown task executor and clean up all tasks
;==============================================================================
PUBLIC autonomous_task_executor_shutdown
ALIGN 16
autonomous_task_executor_shutdown PROC
    push rbx
    sub rsp, 32
    
    ; Cancel all pending and running tasks
    xor rbx, rbx
    lea rcx, taskPool
    
shutdown_loop:
    cmp rbx, MAX_PENDING_TASKS
    jge shutdown_done
    
    mov rax, QWORD PTR [rcx + rbx*8]
    test rax, rax
    jz skip_cleanup
    
    ; Check if executing
    mov edx, DWORD PTR [rax + 48]
    cmp edx, 1              ; running?
    jne skip_cleanup
    
    ; Cancel it
    mov rcx, rax
    call autonomous_task_cancel
    
skip_cleanup:
    inc rbx
    jmp shutdown_loop
    
shutdown_done:
    mov QWORD PTR pendingTaskCount, 0
    mov QWORD PTR executingTaskCount, 0
    
    mov eax, 1
    add rsp, 32
    pop rbx
    ret
autonomous_task_executor_shutdown ENDP

END
