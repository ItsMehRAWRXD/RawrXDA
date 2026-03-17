; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_Swarm_Orchestrator.asm
; Job scheduling, MPSC queue, VRAM pressure management
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
include RawrXD_Defs.inc

; Imports
EXTERNDEF InferenceEngine_Submit:PROC
EXTERN ModelState_AcquireInstance : PROC

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
    sub rsp, 40
    .endprolog
    
@worker_loop:
    cmp g_SwarmContext.ShutdownFlag, 0
    jne @exit_worker
    
    ; Acquire Lock
    lea rcx, g_SwarmLock
    call Spinlock_Acquire
    
    cmp g_SwarmContext.JobHead, 0
    jz @no_jobs
    
    ; Dequeue
    mov rbx, g_SwarmContext.JobHead
    mov rax, [rbx].SwarmJob.NextJob
    mov g_SwarmContext.JobHead, rax
    
    cmp rax, 0
    jnz @tail_ok
    mov g_SwarmContext.JobTail, 0
@tail_ok:
    dec g_SwarmContext.ActiveJobs
    
    ; Release Lock
    lea rcx, g_SwarmLock
    call Spinlock_Release
    
    ; Process Job (RBX)
    mov [rbx].SwarmJob.Status, 2 ; RUNNING
    
    ; Get Model Instance (Need ModelID resolution)
    ; For now, assume single model or ID=0
    mov rcx, 0
    call ModelState_AcquireInstance
    
    test rax, rax
    jz @job_fail
    
    ; Run Inference
    mov rcx, rax    ; ModelInstance
    mov rdx, rbx    ; JobPtr
    call InferenceEngine_Submit
    
    jmp @worker_loop
    
@job_fail:
    mov [rbx].SwarmJob.Status, 6 ; ERROR
    jmp @worker_loop

@no_jobs:
    ; Release Lock
    lea rcx, g_SwarmLock
    call Spinlock_Release
    
    ; Sleep
    mov rcx, 10
    call Sleep
    jmp @worker_loop
    
@exit_worker:
    xor eax, eax
    add rsp, 40
    pop rsi
    pop rbx
    ret
SwarmWorkerProc ENDP



PUBLIC Swarm_Initialize
PUBLIC Swarm_SubmitJob

END
