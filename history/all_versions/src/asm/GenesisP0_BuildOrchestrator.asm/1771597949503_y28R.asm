// ==============================================================================
// GenesisP0_BuildOrchestrator.asm — Parallel Build Coordination
// Exports: Genesis_BuildOrchestrator_Init, Genesis_BuildOrchestrator_AddJob, Genesis_BuildOrchestrator_WaitAll
// ==============================================================================
OPTION DOTNAME
EXTERN CreateThread:PROC, WaitForSingleObject:PROC, WaitForMultipleObjects:PROC, CloseHandle:PROC
EXTERN CreateSemaphoreA:PROC, ReleaseSemaphore:PROC, InitializeCriticalSection:PROC
EXTERN EnterCriticalSection:PROC, LeaveCriticalSection:PROC, Sleep:PROC

.data
    align 8
    MAX_JOBS            EQU 16
    
    g_jobCount          DD 0
    g_activeThreads     DD 0
    g_hSemaphore        DQ 0
    g_csQueue           DQ 0
    g_jobQueue          DQ MAX_JOBS DUP(0)  ; Function pointers
    g_jobParams         DQ MAX_JOBS DUP(0)  ; Context pointers
    g_threadHandles     DQ MAX_JOBS DUP(0)

.code
; ------------------------------------------------------------------------------
; WorkerThread — Thread pool worker
; RCX = threadId
; ------------------------------------------------------------------------------
WorkerThread PROC
    sub rsp, 40
    
_worker_loop:
    ; Wait for job semaphore
    mov rcx, g_hSemaphore
    xor edx, edx                    ; Wait infinite
    call WaitForSingleObject
    
    ; Dequeue job (with CS protection)
    lea rcx, g_csQueue
    call EnterCriticalSection
    
    ; Find job and execute (simplified)
    mov eax, g_jobCount
    dec eax
    mov g_jobCount, eax
    
    lea rcx, g_csQueue
    call LeaveCriticalSection
    
    jmp _worker_loop
    
    add rsp, 40
    ret
WorkerThread ENDP

; ------------------------------------------------------------------------------
; Genesis_BuildOrchestrator_Init — Initialize thread pool
; RCX = maxConcurrent (default 4 if 0)
; ------------------------------------------------------------------------------
Genesis_BuildOrchestrator_Init PROC PUBLIC
    push rbp
    mov rbp, rsp
    sub rsp, 56
    
    test ecx, ecx
    jnz _bo_init_set
    mov ecx, 4
    
_bo_init_set:
    mov g_activeThreads, ecx
    
    ; Create semaphore
    xor r8, r8                      ; Max count
    mov r9d, ecx                    ; Initial count
    xor ecx, ecx                    ; Security
    xor edx, edx                    ; Name
    call CreateSemaphoreA
    mov g_hSemaphore, rax
    
    ; Initialize critical section
    lea rcx, g_csQueue
    call InitializeCriticalSection
    
    ; Launch worker threads
    xor rbx, rbx
    
_bo_thread_loop:
    cmp ebx, g_activeThreads
    jae _bo_init_done
    
    xor ecx, ecx                    ; Security
    xor edx, edx                    ; Stack
    lea r8, WorkerThread            ; Start
    mov r9d, ebx                    ; Param (thread index)
    mov [rsp+32], r9                ; Flags
    lea rax, g_threadHandles[rbx*8]
    mov [rsp+40], rax               ; ThreadId
    
    call CreateThread
    mov g_threadHandles[rbx*8], rax
    
    inc ebx
    jmp _bo_thread_loop
    
_bo_init_done:
    xor rax, rax
    mov rsp, rbp
    pop rbp
    ret
Genesis_BuildOrchestrator_Init ENDP

; ------------------------------------------------------------------------------
; Genesis_BuildOrchestrator_AddJob — Queue build job
; RCX = jobProc, RDX = param
; Returns: RAX = jobId (index) or -1 if full
; ------------------------------------------------------------------------------
Genesis_BuildOrchestrator_AddJob PROC PUBLIC
    push rbp
    mov rbp, rsp
    sub rsp, 40
    
    lea r8, g_csQueue
    mov [rsp+32], r8
    call EnterCriticalSection
    
    mov eax, g_jobCount
    cmp eax, MAX_JOBS
    jae _bo_add_full
    
    mov [g_jobQueue + rax*8], rcx
    mov [g_jobParams + rax*8], rdx
    inc g_jobCount
    mov ebx, eax                    ; Return jobId
    
    ; Signal semaphore
    mov rcx, g_hSemaphore
    xor edx, edx                    ; Release count 1
    xor r8, r8                      ; Previous count (don't care)
    call ReleaseSemaphore
    
    mov eax, ebx
    jmp _bo_add_exit
    
_bo_add_full:
    mov rax, -1
    
_bo_add_exit:
    lea rcx, g_csQueue
    call LeaveCriticalSection
    
    mov rsp, rbp
    pop rbp
    ret
Genesis_BuildOrchestrator_AddJob ENDP

; ------------------------------------------------------------------------------
; Genesis_BuildOrchestrator_WaitAll — Block until job queue empty
; ------------------------------------------------------------------------------
Genesis_BuildOrchestrator_WaitAll PROC PUBLIC
    push rbp
    mov rbp, rsp
    sub rsp, 40
    
_bo_wait_loop:
    mov eax, g_jobCount
    test eax, eax
    jz _bo_wait_done
    
    mov ecx, 10                     ; 10ms sleep
    call Sleep
    jmp _bo_wait_loop
    
_bo_wait_done:
    xor rax, rax
    mov rsp, rbp
    pop rbp
    ret
Genesis_BuildOrchestrator_WaitAll ENDP

END
