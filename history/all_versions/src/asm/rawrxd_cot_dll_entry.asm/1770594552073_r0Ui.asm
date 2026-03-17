; =============================================================================
; rawrxd_cot_dll_entry.asm — DLL Entry Point + SRW Lock for CoT Engine
; =============================================================================
;
; Phase 37: DllMain + synchronization primitives for the CoT MASM DLL.
;
; Implements:
;   - DllMain (DLL_PROCESS_ATTACH / DETACH / THREAD_ATTACH / DETACH)
;   - SRW Lock initialization and cleanup
;   - Exported lock acquire/release for use by rawrxd_cot_engine.asm
;   - OutputDebugStringA tracing for all lifecycle events
;
; Exports:
;   DllMain             — DLL entry point (called by loader)
;   Acquire_CoT_Lock    — Acquire exclusive SRW lock
;   Release_CoT_Lock    — Release exclusive SRW lock
;   Acquire_CoT_Lock_Shared — Acquire shared SRW lock (for reads)
;   Release_CoT_Lock_Shared — Release shared SRW lock
;   CoT_Get_Thread_Count    — Get count of attached threads
;
; Architecture: x64 MASM | Windows ABI | No CRT | No exceptions
; Build: ml64 /c /Zi /Zd /Fo rawrxd_cot_dll_entry.obj rawrxd_cot_dll_entry.asm
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

INCLUDE RawrXD_Common.inc

; =============================================================================
;                             EXPORTS
; =============================================================================
PUBLIC DllMain
PUBLIC Acquire_CoT_Lock
PUBLIC Release_CoT_Lock
PUBLIC Acquire_CoT_Lock_Shared
PUBLIC Release_CoT_Lock_Shared
PUBLIC CoT_Get_Thread_Count

; =============================================================================
;                          EXTERNAL IMPORTS
; =============================================================================
EXTERN InitializeSRWLock: PROC
EXTERN AcquireSRWLockExclusive: PROC
EXTERN ReleaseSRWLockExclusive: PROC
EXTERN AcquireSRWLockShared: PROC
EXTERN ReleaseSRWLockShared: PROC
EXTERN OutputDebugStringA: PROC
EXTERN CoT_Initialize_Core: PROC
EXTERN CoT_Shutdown_Core: PROC

; =============================================================================
;                            CONSTANTS
; =============================================================================

; DLL attach/detach reason codes
DLL_PROCESS_ATTACH      EQU     1
DLL_THREAD_ATTACH       EQU     2
DLL_THREAD_DETACH       EQU     3
DLL_PROCESS_DETACH      EQU     0

; SRW Lock is a single pointer-sized value on Windows (8 bytes on x64)
SRWLOCK_SIZE            EQU     8

; =============================================================================
;                        GLOBAL STATE (.data)
; =============================================================================
.data

ALIGN 16

; SRW Lock for CoT engine synchronization
g_cotSRWLock            DQ 0                ; SRWLOCK (init to 0 = unlocked)

; Thread tracking
g_threadCount           DD 0                ; number of attached threads
g_processAttached       DD 0                ; 1 if DLL_PROCESS_ATTACH succeeded
g_lockInitialized       DD 0                ; 1 if SRW lock has been initialized

; =============================================================================
;                     READ-ONLY STRINGS (.const)
; =============================================================================
.const

ALIGN 8

szDll_ProcessAttach     DB "[CoT-DLL] DllMain: DLL_PROCESS_ATTACH — initializing SRW lock.", 0
szDll_ProcessDetach     DB "[CoT-DLL] DllMain: DLL_PROCESS_DETACH — shutting down engine.", 0
szDll_ThreadAttach      DB "[CoT-DLL] DllMain: DLL_THREAD_ATTACH — thread count incremented.", 0
szDll_ThreadDetach      DB "[CoT-DLL] DllMain: DLL_THREAD_DETACH — thread count decremented.", 0
szDll_LockInit          DB "[CoT-DLL] SRW lock initialized successfully.", 0
szDll_LockAcquire       DB "[CoT-DLL] Acquire_CoT_Lock: exclusive lock acquired.", 0
szDll_LockRelease       DB "[CoT-DLL] Release_CoT_Lock: exclusive lock released.", 0
szDll_AutoInit          DB "[CoT-DLL] Auto-initializing CoT core arena on process attach.", 0
szDll_AutoInitOK        DB "[CoT-DLL] CoT core arena initialized OK.", 0
szDll_AutoInitFail      DB "[CoT-DLL] WARNING: CoT core arena init FAILED during attach!", 0
szDll_ShutdownOK        DB "[CoT-DLL] Engine shutdown complete.", 0

; =============================================================================
;                            CODE SECTION
; =============================================================================
.code

; =============================================================================
; DllMain — DLL entry point
;
; BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
;
; RCX = hinstDLL
; RDX = fdwReason
; R8  = lpvReserved
;
; Returns: EAX = TRUE (1) on success, FALSE (0) on failure
; =============================================================================
DllMain PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 48
    .allocstack 48
    .endprolog

    mov     r12, rcx                        ; hinstDLL (saved for future use)
    mov     r13d, edx                       ; fdwReason

    ; Dispatch on reason code
    cmp     r13d, DLL_PROCESS_ATTACH
    je      @@dm_process_attach

    cmp     r13d, DLL_PROCESS_DETACH
    je      @@dm_process_detach

    cmp     r13d, DLL_THREAD_ATTACH
    je      @@dm_thread_attach

    cmp     r13d, DLL_THREAD_DETACH
    je      @@dm_thread_detach

    ; Unknown reason — return TRUE
    jmp     @@dm_ok

; ---- DLL_PROCESS_ATTACH ----
@@dm_process_attach:
    lea     rcx, szDll_ProcessAttach
    call    OutputDebugStringA

    ; Initialize SRW lock
    lea     rcx, g_cotSRWLock
    call    InitializeSRWLock
    mov     DWORD PTR [g_lockInitialized], 1

    lea     rcx, szDll_LockInit
    call    OutputDebugStringA

    ; Set initial thread count = 1 (the attaching thread)
    mov     DWORD PTR [g_threadCount], 1
    mov     DWORD PTR [g_processAttached], 1

    ; Auto-initialize the CoT core arena
    lea     rcx, szDll_AutoInit
    call    OutputDebugStringA

    call    CoT_Initialize_Core
    test    eax, eax
    jnz     @@dm_auto_init_fail

    lea     rcx, szDll_AutoInitOK
    call    OutputDebugStringA
    jmp     @@dm_ok

@@dm_auto_init_fail:
    lea     rcx, szDll_AutoInitFail
    call    OutputDebugStringA
    ; Non-fatal: DLL loads even if arena fails (can retry later)
    jmp     @@dm_ok

; ---- DLL_PROCESS_DETACH ----
@@dm_process_detach:
    lea     rcx, szDll_ProcessDetach
    call    OutputDebugStringA

    ; Shutdown engine (releases 1GB arena)
    cmp     DWORD PTR [g_processAttached], 0
    je      @@dm_skip_shutdown

    call    CoT_Shutdown_Core

    lea     rcx, szDll_ShutdownOK
    call    OutputDebugStringA

@@dm_skip_shutdown:
    ; Reset state
    mov     DWORD PTR [g_processAttached], 0
    mov     DWORD PTR [g_lockInitialized], 0
    mov     DWORD PTR [g_threadCount], 0
    mov     QWORD PTR [g_cotSRWLock], 0
    jmp     @@dm_ok

; ---- DLL_THREAD_ATTACH ----
@@dm_thread_attach:
    ; Atomically increment thread count
    lock inc DWORD PTR [g_threadCount]

    IFDEF DEBUG_BUILD
    lea     rcx, szDll_ThreadAttach
    call    OutputDebugStringA
    ENDIF
    jmp     @@dm_ok

; ---- DLL_THREAD_DETACH ----
@@dm_thread_detach:
    ; Atomically decrement thread count (floor at 0)
    mov     eax, DWORD PTR [g_threadCount]
    test    eax, eax
    jz      @@dm_ok
    lock dec DWORD PTR [g_threadCount]

    IFDEF DEBUG_BUILD
    lea     rcx, szDll_ThreadDetach
    call    OutputDebugStringA
    ENDIF
    jmp     @@dm_ok

@@dm_ok:
    mov     eax, 1                          ; return TRUE
    jmp     @@dm_ret

@@dm_fail:
    xor     eax, eax                        ; return FALSE

@@dm_ret:
    add     rsp, 48
    pop     r13
    pop     r12
    pop     rbx
    ret
DllMain ENDP

; =============================================================================
; Acquire_CoT_Lock
; Acquire the SRW lock in exclusive (write) mode.
; Safe to call from any thread. Blocks until acquired.
; =============================================================================
Acquire_CoT_Lock PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog

    ; Verify lock was initialized
    cmp     DWORD PTR [g_lockInitialized], 0
    je      @@acl_skip

    lea     rcx, g_cotSRWLock
    call    AcquireSRWLockExclusive

@@acl_skip:
    add     rsp, 40
    ret
Acquire_CoT_Lock ENDP

; =============================================================================
; Release_CoT_Lock
; Release the SRW lock from exclusive (write) mode.
; =============================================================================
Release_CoT_Lock PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog

    cmp     DWORD PTR [g_lockInitialized], 0
    je      @@rcl_skip

    lea     rcx, g_cotSRWLock
    call    ReleaseSRWLockExclusive

@@rcl_skip:
    add     rsp, 40
    ret
Release_CoT_Lock ENDP

; =============================================================================
; Acquire_CoT_Lock_Shared
; Acquire the SRW lock in shared (read) mode.
; Multiple readers can hold simultaneously; blocks if exclusive held.
; =============================================================================
Acquire_CoT_Lock_Shared PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog

    cmp     DWORD PTR [g_lockInitialized], 0
    je      @@acls_skip

    lea     rcx, g_cotSRWLock
    call    AcquireSRWLockShared

@@acls_skip:
    add     rsp, 40
    ret
Acquire_CoT_Lock_Shared ENDP

; =============================================================================
; Release_CoT_Lock_Shared
; Release the SRW lock from shared (read) mode.
; =============================================================================
Release_CoT_Lock_Shared PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog

    cmp     DWORD PTR [g_lockInitialized], 0
    je      @@rcls_skip

    lea     rcx, g_cotSRWLock
    call    ReleaseSRWLockShared

@@rcls_skip:
    add     rsp, 40
    ret
Release_CoT_Lock_Shared ENDP

; =============================================================================
; CoT_Get_Thread_Count
; Returns: EAX = current count of attached threads
; =============================================================================
CoT_Get_Thread_Count PROC
    mov     eax, DWORD PTR [g_threadCount]
    ret
CoT_Get_Thread_Count ENDP

END
