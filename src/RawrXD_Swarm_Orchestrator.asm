; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_Swarm_Orchestrator.asm
; High-performance Job Scheduler and Worker Pool
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
include RawrXD_Defs.inc

; ═══════════════════════════════════════════════════════════════════════════════
; CONSTANTS
; ═══════════════════════════════════════════════════════════════════════════════
SWARM_MAX_WORKERS       EQU 4
SWARM_QUEUE_SIZE        EQU 1024

; ═══════════════════════════════════════════════════════════════════════════════
; DATA SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.DATA
align 16
g_SwarmQueueHead        QWORD       0
g_SwarmQueueTail        QWORD       0
g_SwarmQueueLock        DWORD       0
g_SwarmJobCount         DWORD       0

g_WorkerThreads         QWORD SWARM_MAX_WORKERS DUP (0)
g_WorkerShutdown        DWORD       0
g_JobAvailableEvent     QWORD       0

; ═══════════════════════════════════════════════════════════════════════════════
; CODE SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; Swarm_Initialize
; ═══════════════════════════════════════════════════════════════════════════════
Swarm_Initialize PROC FRAME
    push rbx
    push rsi
    push rdi
    sub rsp, 40
    .endprolog
    
    ; Create auto-reset event for job availability
    mov rcx, 0              ; Security attributes
    mov rdx, 0              ; Manual reset (FALSE = Auto)
    mov r8, 0               ; Initial state (Unsignaled)
    mov r9, 0               ; Name
    call CreateEventA
    mov g_JobAvailableEvent, rax
    
    ; Spawn workers
    xor ebx, ebx
@spawn_loop:
    cmp ebx, SWARM_MAX_WORKERS
    jge @spawn_done
    
    mov rcx, 0              ; Security
    mov rdx, 0              ; Stack size
    lea r8, Swarm_WorkerThread
    mov r9, rbx             ; Parameter (Thread ID)
    push 0                  ; Creation flags (stack alignment)
    push 0                  ; ThreadId ptr
    sub rsp, 32             ; Shadow space
    call CreateThread
    add rsp, 48             ; Pop args + shadow
    
    lea rcx, g_WorkerThreads
    mov [rcx + rbx*8], rax
    
    inc ebx
    jmp @spawn_loop
    
@spawn_done:
    mov eax, 1
    add rsp, 40
    pop rdi
    pop rsi
    pop rbx
    ret
Swarm_Initialize ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Swarm_SubmitJob
; RCX = SwarmJob*
; ═══════════════════════════════════════════════════════════════════════════════
Swarm_SubmitJob PROC FRAME
    push rbx
    sub rsp, 32
    .endprolog
    
    mov rbx, rcx
    
    ; 1. Acquire Lock
    lea rcx, g_SwarmQueueLock
    call Spinlock_Acquire
    
    ; 2. Enqueue (Append to tail)
    mov [rbx].SwarmJob.NextJob, 0
    
    cmp g_SwarmQueueTail, 0
    je @queue_empty
    
    ; Tail->Next = Job
    mov rax, g_SwarmQueueTail
    mov [rax].SwarmJob.NextJob, rbx
    mov g_SwarmQueueTail, rbx
    jmp @queue_updated
    
@queue_empty:
    mov g_SwarmQueueHead, rbx
    mov g_SwarmQueueTail, rbx
    
@queue_updated:
    inc g_SwarmJobCount
    
    ; 3. Release Lock
    lea rcx, g_SwarmQueueLock
    call Spinlock_Release
    
    ; 4. Signal Workers
    mov rcx, g_JobAvailableEvent
    call SetEvent
    
    mov rax, 1
    add rsp, 32
    pop rbx
    ret
Swarm_SubmitJob ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Swarm_WorkerThread
; RCX = Thread ID
; ═══════════════════════════════════════════════════════════════════════════════
Swarm_WorkerThread PROC FRAME
    push rbx
    push rsi
    push rdi
    sub rsp, 40
    .endprolog
    
@worker_loop:
    ; Check shutdown
    cmp g_WorkerShutdown, 0
    jne @worker_exit
    
    ; 1. Wait for job
    mov rcx, g_JobAvailableEvent
    mov rdx, INFINITE
    call WaitForSingleObject
    
    ; 2. Try dequeue
    lea rcx, g_SwarmQueueLock
    call Spinlock_Acquire
    
    mov rsi, g_SwarmQueueHead
    test rsi, rsi
    jz @nothing_to_do_unlock
    
    ; Pop head
    mov rax, [rsi].SwarmJob.NextJob
    mov g_SwarmQueueHead, rax
    
    ; If head is now null, tail is null
    test rax, rax
    jnz @update_count
    mov g_SwarmQueueTail, 0
    
@update_count:
    dec g_SwarmJobCount
    
    ; Unlock
    lea rcx, g_SwarmQueueLock
    call Spinlock_Release
    
    ; 3. Process Job
    ; Resolve Model
    mov rcx, [rsi].SwarmJob.ModelId
    call ModelState_AcquireInstance
    
    test rax, rax
    jz @model_fail
    
    ; Call Inference Engine
    mov rcx, rax            ; ModelHandle
    mov rdx, rsi            ; Job
    call InferenceEngine_Submit
    
    jmp @worker_loop
    
@model_fail:
    mov [rsi].SwarmJob.Status, 6 ; ERROR
    ; Signal completion anyway if event exists
    mov rcx, [rsi].SwarmJob.hCompleteEvent
    test rcx, rcx
    jz @worker_loop
    call SetEvent
    jmp @worker_loop
    
@nothing_to_do_unlock:
    lea rcx, g_SwarmQueueLock
    call Spinlock_Release
    jmp @worker_loop
    
@worker_exit:
    xor eax, eax
    add rsp, 40
    pop rdi
    pop rsi
    pop rbx
    ret
Swarm_WorkerThread ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; EXPORTS
; ═══════════════════════════════════════════════════════════════════════════════
PUBLIC Swarm_Initialize
PUBLIC Swarm_SubmitJob

END
