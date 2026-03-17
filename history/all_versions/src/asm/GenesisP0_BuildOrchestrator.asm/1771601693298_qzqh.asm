; ============================================================
; GenesisP0_BuildOrchestrator.asm — Parallel job scheduler for builds
; Exports: BuildOrc_Init, BuildOrc_AddJob, BuildOrc_ExecuteParallel, BuildOrc_WaitAll, BuildOrc_Shutdown
; ============================================================
OPTION CASEMAP:NONE

EXTERN CreateEventA:PROC
EXTERN CreateThread:PROC
EXTERN WaitForMultipleObjects:PROC
EXTERN SetEvent:PROC
EXTERN ResetEvent:PROC
EXTERN CloseHandle:PROC
EXTERN Sleep:PROC
EXTERN GetSystemInfo:PROC
EXTERN ExitThread:PROC
EXTERN UTC_LogEvent:PROC

PUBLIC BuildOrc_Init
PUBLIC BuildOrc_AddJob
PUBLIC BuildOrc_ExecuteParallel
PUBLIC BuildOrc_WaitAll
PUBLIC BuildOrc_Shutdown

MAX_JOBS EQU 32

.data
ALIGN 8
g_hThreads      QWORD MAX_JOBS DUP(0)
g_hEvents       QWORD MAX_JOBS DUP(0)
g_jobFuncs      QWORD MAX_JOBS DUP(0)    ; Function pointers
g_jobContexts   QWORD MAX_JOBS DUP(0)    ; Context data
g_jobCount      DWORD 0
g_maxParallel   DWORD 4
g_systemInfo    BYTE 64 DUP(0)           ; SYSTEM_INFO struct
sz_evt_borc_init BYTE "[GenesisP0] BuildOrchestrator Init",0

.code
; ------------------------------------------------------------
; BuildOrc_Init(UINT32 maxParallel) -> BOOL
; ------------------------------------------------------------
BuildOrc_Init PROC
    push rbx
    sub rsp, 28h

    mov ebx, ecx

    lea rcx, sz_evt_borc_init
    call UTC_LogEvent
    cmp ebx, MAX_JOBS
    ja @init_use_default
    mov [g_maxParallel], ebx
    jmp @init_cont

@init_use_default:
    mov DWORD PTR [g_maxParallel], 4

@init_cont:
    ; Get CPU count
    lea rcx, g_systemInfo
    call GetSystemInfo
    mov eax, DWORD PTR [g_systemInfo+20] ; dwNumberOfProcessors
    cmp eax, [g_maxParallel]
    cmova eax, [g_maxParallel]
    mov [g_maxParallel], eax

    xor eax, eax
    mov [g_jobCount], eax

    mov eax, 1
    add rsp, 28h
    pop rbx
    ret
BuildOrc_Init ENDP

; ------------------------------------------------------------
; BuildOrc_AddJob(void* func, void* context) -> INT (job index)
; ------------------------------------------------------------
BuildOrc_AddJob PROC
    push rbx
    sub rsp, 28h

    mov rbx, rcx                ; func
    mov rax, rdx                ; context

    mov ecx, [g_jobCount]
    cmp ecx, MAX_JOBS
    jae @add_fail

    mov [g_jobFuncs + rcx*8], rbx
    mov [g_jobContexts + rcx*8], rax

    inc ecx
    mov [g_jobCount], ecx
    mov eax, ecx
    dec eax
    jmp @add_done

@add_fail:
    mov eax, -1

@add_done:
    add rsp, 28h
    pop rbx
    ret
BuildOrc_AddJob ENDP

; ------------------------------------------------------------
; ThreadProc for parallel execution. RCX = job index.
; ------------------------------------------------------------
JobThreadProc PROC
    push rbx
    sub rsp, 28h

    mov rbx, rcx                ; job index

    ; Call job function (rcx = context)
    mov rax, [g_jobFuncs + rbx*8]
    mov rcx, [g_jobContexts + rbx*8]
    test rax, rax
    jz @no_call
    call rax

@no_call:
    ; Signal completion
    mov rcx, [g_hEvents + rbx*8]
    test rcx, rcx
    jz @no_signal
    call SetEvent

@no_signal:
    xor ecx, ecx
    call ExitThread

    add rsp, 28h
    pop rbx
    ret
JobThreadProc ENDP

; ------------------------------------------------------------
; BuildOrc_ExecuteParallel() -> BOOL
; ------------------------------------------------------------
BuildOrc_ExecuteParallel PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 40h

    mov esi, [g_jobCount]
    xor edi, edi

    ; Create events and threads
@create_loop:
    cmp edi, esi
    jae @execute_wait

    ; Create manual reset event (bManualReset=TRUE, bInitialState=FALSE)
    xor ecx, ecx                ; lpEventAttributes
    mov edx, 1                  ; bManualReset = TRUE
    xor r8d, r8d                ; bInitialState = FALSE
    xor r9d, r9d                ; lpName
    call CreateEventA
    mov [g_hEvents + rdi*8], rax

    ; Create thread
    xor ecx, ecx                ; Security
    mov edx, 0                  ; Stack size
    lea r8, JobThreadProc
    mov r9d, edi                ; Context = job index
    mov QWORD PTR [rsp+20h], 0  ; Creation flags
    mov QWORD PTR [rsp+28h], 0  ; Thread ID
    call CreateThread
    mov [g_hThreads + rdi*8], rax

    inc edi
    jmp @create_loop

@execute_wait:
    ; Wait for all (on events)
    mov ecx, esi                ; Count
    lea rdx, g_hEvents          ; Handle array
    mov r8d, 1                  ; Wait all
    mov r9d, 0FFFFFFFFh         ; INFINITE
    call WaitForMultipleObjects

    mov eax, 1
    add rsp, 40h
    pop rdi
    pop rsi
    pop rbx
    ret
BuildOrc_ExecuteParallel ENDP

; ------------------------------------------------------------
; BuildOrc_WaitAll(UINT32 timeoutMs) -> BOOL
; ------------------------------------------------------------
BuildOrc_WaitAll PROC
    push rbx
    sub rsp, 28h

    mov ebx, ecx

    mov ecx, [g_jobCount]
    test ecx, ecx
    jz @wait_ok

    lea rdx, g_hEvents
    mov r8d, 1                  ; Wait all
    mov r9d, ebx
    call WaitForMultipleObjects

    cmp eax, 0FFFFFF00h         ; WAIT_FAILED
    setne al
    movzx eax, al
    jmp @wait_done

@wait_ok:
    mov eax, 1

@wait_done:
    add rsp, 28h
    pop rbx
    ret
BuildOrc_WaitAll ENDP

; ------------------------------------------------------------
; BuildOrc_Shutdown() -> void
; ------------------------------------------------------------
BuildOrc_Shutdown PROC
    push rbx
    push rsi
    sub rsp, 28h

    mov esi, [g_jobCount]
    dec esi
    js @shutdown_done

@close_loop:
    mov rcx, [g_hThreads + rsi*8]
    test rcx, rcx
    jz @skip_thread
    call CloseHandle

@skip_thread:
    mov rcx, [g_hEvents + rsi*8]
    test rcx, rcx
    jz @skip_event
    call CloseHandle

@skip_event:
    dec esi
    jns @close_loop

@shutdown_done:
    xor eax, eax
    mov [g_jobCount], eax

    add rsp, 28h
    pop rsi
    pop rbx
    ret
BuildOrc_Shutdown ENDP

END
