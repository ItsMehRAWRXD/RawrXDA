; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_Swarm_Orchestrator.asm
; Job scheduling, MPSC queue, VRAM pressure management
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
include RawrXD_Defs.inc

; Imports
EXTERNDEF InferenceEngine_Submit:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; CONSTANTS
; ═══════════════════════════════════════════════════════════════════════════════
MAX_JOBS                EQU 1024

; ═══════════════════════════════════════════════════════════════════════════════
; STRUCTURES
; ═══════════════════════════════════════════════════════════════════════════════



SwarmContext STRUCT
    JobHead             QWORD       ?       ; Volatile head
    JobTail             QWORD       ?       ; Volatile tail
    ActiveJobs          DWORD       ?
    hWorkerThread       QWORD       ?
    ShutdownFlag        DWORD       ?
SwarmContext ENDS

; ═══════════════════════════════════════════════════════════════════════════════
; DATA SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.DATA
align 16
g_SwarmContext          SwarmContext <>
g_SwarmLock             DWORD 0

; ═══════════════════════════════════════════════════════════════════════════════
; CODE SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; Swarm_Initialize
; ═══════════════════════════════════════════════════════════════════════════════
Swarm_Initialize PROC FRAME
    push rbx
    sub rsp, 48
    .endprolog
    
    xor eax, eax
    mov g_SwarmContext.JobHead, rax
    mov g_SwarmContext.JobTail, rax
    mov g_SwarmContext.ActiveJobs, eax
    mov g_SwarmContext.ShutdownFlag, eax
    mov g_SwarmLock, eax
    
    ; Create worker thread
    lea r8, SwarmWorkerProc
    mov rcx, 0
    mov rdx, 0
    mov r9, 0
    mov qword ptr [rsp + 32], 0     ; FLAGS
    mov qword ptr [rsp + 40], 0     ; ThreadID
    call CreateThread
    mov g_SwarmContext.hWorkerThread, rax
    
    mov rax, 1
    add rsp, 48
    pop rbx
    ret
Swarm_Initialize ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Swarm_SubmitJob
; RCX = Job Pointer
; ═══════════════════════════════════════════════════════════════════════════════
Swarm_SubmitJob PROC FRAME
    push rbx
    push rsi
    sub rsp, 32
    .endprolog
    
    mov rsi, rcx
    mov [rsi].SwarmJob.NextJob, 0
    mov [rsi].SwarmJob.Status, 1 ; STATUS_PENDING
    
    ; Acquire Lock
    lea rcx, g_SwarmLock
    call Spinlock_Acquire
    
    ; Enqueue
    cmp g_SwarmContext.JobTail, 0
    jz @queue_empty
    
    ; Link active tail to new job
    mov rax, g_SwarmContext.JobTail
    mov [rax].SwarmJob.NextJob, rsi
    mov g_SwarmContext.JobTail, rsi
    jmp @queue_done
    
@queue_empty:
    mov g_SwarmContext.JobHead, rsi
    mov g_SwarmContext.JobTail, rsi
    
@queue_done:
    inc g_SwarmContext.ActiveJobs
    
    ; Release Lock
    lea rcx, g_SwarmLock
    call Spinlock_Release
    
    add rsp, 32
    pop rsi
    pop rbx
    ret
Swarm_SubmitJob ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; SwarmWorkerProc
; ═══════════════════════════════════════════════════════════════════════════════
SwarmWorkerProc PROC FRAME
    push rbx
    push rsi
    sub rsp, 48
    .endprolog
    
@worker_loop:
    ; Check shutdown
    cmp g_SwarmContext.ShutdownFlag, 1
    je @worker_exit
    
    ; Check for jobs
    lea rcx, g_SwarmLock
    call Spinlock_Acquire
    
    cmp g_SwarmContext.JobHead, 0
    jz @no_jobs
    
    ; Dequeue
    mov rsi, g_SwarmContext.JobHead
    mov rax, [rsi].SwarmJob.NextJob
    mov g_SwarmContext.JobHead, rax
    
    ; Update Tail if empty
    test rax, rax
    jnz @update_count
    mov g_SwarmContext.JobTail, 0
    
@update_count:
    dec g_SwarmContext.ActiveJobs
    
    lea rcx, g_SwarmLock
    call Spinlock_Release
    
    ; Process Job (RSI)
    mov [rsi].SwarmJob.Status, 2 ; STATUS_RUNNING
    
    mov rcx, [rsi].SwarmJob.ModelId
    mov rdx, rsi
    call InferenceEngine_Submit ; Assume this returns when done or submits async? 
                                ; If async, this worker might pick up next job. 
                                ; For now assume blocking for "InferenceEngine_Submit" logic flow
    
    mov [rsi].SwarmJob.Status, 3 ; STATUS_COMPLETE
    
    ; Callback
    mov rax, [rsi].SwarmJob.Callback
    test rax, rax
    jz @worker_loop
    
    mov rcx, rsi
    call rax
    jmp @worker_loop
    
@no_jobs:
    lea rcx, g_SwarmLock
    call Spinlock_Release
    
    ; Sleep to avoid burn
    mov rcx, 1
    call Sleep
    jmp @worker_loop
    
@worker_exit:
    add rsp, 48
    pop rsi
    pop rbx
    xor eax, eax
    ret
SwarmWorkerProc ENDP
; ═══════════════════════════════════════════════════════════════════════════════
SwarmWorkerProc PROC FRAME
    sub rsp, 40
    .endprolog
    
@worker_loop:
    cmp g_SwarmContext.ShutdownFlag, 0
    jne @exit_worker
    
    ; Check queue
    ; If empty, sleep
    mov rcx, 10
    call Sleep
    jmp @worker_loop
    
@exit_worker:
    xor eax, eax
    add rsp, 40
    ret
SwarmWorkerProc ENDP

PUBLIC Swarm_Initialize
PUBLIC Swarm_SubmitJob

END
