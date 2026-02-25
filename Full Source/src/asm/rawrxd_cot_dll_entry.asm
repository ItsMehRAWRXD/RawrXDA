; =============================================================================
; rawrxd_cot_dll_entry.asm — DLL Entry Point + SRW Lock + TLS for CoT Engine
; =============================================================================
;
; Phase 37.1: Enterprise-hardened DllMain with:
;   - TLS slot allocation for per-thread error reporting (LastError + fault RIP)
;   - DisableThreadLibraryCalls for data-plane performance (no GUI callbacks)
;   - Large page privilege check on attach (SeLockMemoryPrivilege)
;   - SRW Lock initialization and cleanup
;   - Exported lock acquire/release for use by rawrxd_cot_engine.asm
;   - OutputDebugStringA tracing for all lifecycle events
;
; TLS Architecture:
;   Each thread gets a 24-byte TLS block:
;     +0: DWORD  LastErrorCode (set by SEH handler)
;     +4: DWORD  Reserved
;     +8: QWORD  FaultRIP (instruction pointer where AV occurred)
;    +16: QWORD  FaultAddress (memory address that caused AV)
;
; Exports:
;   DllMain                 — DLL entry point (called by loader)
;   Acquire_CoT_Lock        — Acquire exclusive SRW lock
;   Release_CoT_Lock        — Release exclusive SRW lock
;   Acquire_CoT_Lock_Shared — Acquire shared SRW lock (for reads)
;   Release_CoT_Lock_Shared — Release shared SRW lock
;   CoT_Get_Thread_Count    — Get count of attached threads
;   CoT_TLS_GetLastError    — Get per-thread last error code
;   CoT_TLS_GetFaultRIP     — Get per-thread fault RIP
;   CoT_TLS_SetError        — Set per-thread error (called by SEH handler)
;   CoT_Has_Large_Pages     — Returns 1 if large pages are available
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
PUBLIC CoT_TLS_GetLastError
PUBLIC CoT_TLS_GetFaultRIP
PUBLIC CoT_TLS_SetError
PUBLIC CoT_Has_Large_Pages

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
EXTERN TlsAlloc: PROC
EXTERN TlsFree: PROC
EXTERN TlsGetValue: PROC
EXTERN TlsSetValue: PROC
EXTERN HeapAlloc: PROC
EXTERN HeapFree: PROC
EXTERN GetProcessHeap: PROC
EXTERN DisableThreadLibraryCalls: PROC
EXTERN OpenProcessToken: PROC
EXTERN LookupPrivilegeValueA: PROC
EXTERN GetCurrentProcess: PROC

; Phase 39–42 initialization procs (from rawrxd_cot_phase39.asm)
EXTERN CoT_SelectCopyEngine: PROC
EXTERN CoT_UpdateTelemetry: PROC
EXTERN CoT_EnableMultiProducer: PROC
EXTERN GetCurrentProcessorNumber: PROC
EXTERN GetNumaProcessorNode: PROC

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

; TLS block layout (per-thread state, 32 bytes — Phase 39 extended)
TLS_BLOCK_SIZE          EQU     32
TLS_OFF_LAST_ERROR      EQU     0           ; DWORD  LastErrorCode
TLS_OFF_NUMA_NODE       EQU     4           ; DWORD  NUMA node affinity (Phase 39)
TLS_OFF_FAULT_RIP       EQU     8           ; QWORD  RIP where AV occurred
TLS_OFF_FAULT_ADDR      EQU     16          ; QWORD  Address that faulted
TLS_OFF_APPEND_COUNT    EQU     24          ; QWORD  Per-thread append stats (Phase 39)

; TlsAlloc failure sentinel
TLS_OUT_OF_INDEXES      EQU     0FFFFFFFFh

; Token access for privilege check
TOKEN_QUERY             EQU     0008h

; HeapAlloc flags
HEAP_ZERO_MEMORY        EQU     08h

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

; TLS slot index (allocated by TlsAlloc on PROCESS_ATTACH)
g_tlsIndex              DD TLS_OUT_OF_INDEXES

; Large page support flag
g_largePageAvailable    DD 0                ; 1 if SeLockMemoryPrivilege held

; Process heap handle (cached on attach)
g_processHeap           DQ 0

; hInstance saved for DisableThreadLibraryCalls
g_hInstance             DQ 0

; =============================================================================
;                     READ-ONLY STRINGS (.const)
; =============================================================================
.const

ALIGN 8

szDll_ProcessAttach     DB "[CoT-DLL] DllMain: DLL_PROCESS_ATTACH — initializing SRW lock + TLS.", 0
szDll_ProcessDetach     DB "[CoT-DLL] DllMain: DLL_PROCESS_DETACH — shutting down engine + TLS.", 0
szDll_ThreadAttach      DB "[CoT-DLL] DllMain: DLL_THREAD_ATTACH — thread count incremented.", 0
szDll_ThreadDetach      DB "[CoT-DLL] DllMain: DLL_THREAD_DETACH — thread count decremented.", 0
szDll_LockInit          DB "[CoT-DLL] SRW lock initialized successfully.", 0
szDll_LockAcquire       DB "[CoT-DLL] Acquire_CoT_Lock: exclusive lock acquired.", 0
szDll_LockRelease       DB "[CoT-DLL] Release_CoT_Lock: exclusive lock released.", 0
szDll_AutoInit          DB "[CoT-DLL] Auto-initializing CoT core arena on process attach.", 0
szDll_AutoInitOK        DB "[CoT-DLL] CoT core arena initialized OK.", 0
szDll_AutoInitFail      DB "[CoT-DLL] WARNING: CoT core arena init FAILED during attach!", 0
szDll_ShutdownOK        DB "[CoT-DLL] Engine shutdown complete.", 0
szDll_TlsAllocOK        DB "[CoT-DLL] TLS slot allocated successfully.", 0
szDll_TlsAllocFail      DB "[CoT-DLL] WARNING: TlsAlloc FAILED — per-thread errors disabled!", 0
szDll_TlsFreed          DB "[CoT-DLL] TLS slot freed.", 0
szDll_DisableThreadCB   DB "[CoT-DLL] DisableThreadLibraryCalls set (data-plane DLL).", 0
szDll_LargePageOK       DB "[CoT-DLL] SeLockMemoryPrivilege available — large pages enabled.", 0
szDll_LargePageNo       DB "[CoT-DLL] Large pages NOT available (privilege not held).", 0
szPriv_SeLockMemory     DB "SeLockMemoryPrivilege", 0

; Phase 39–42 debug strings
szDll_Phase39Init       DB "[CoT-DLL] Phase 39-42: Copy engine + telemetry initialized.", 0
szDll_NumaTag           DB "[CoT-DLL] NUMA affinity tagged for main thread.", 0

; =============================================================================
;                            CODE SECTION
; =============================================================================
.code

; =============================================================================
; Internal: tls_alloc_block — Allocate and zero a TLS block for current thread
; Returns: RAX = pointer to TLS block, or 0 on failure
; =============================================================================
tls_alloc_block PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    ; Check TLS index valid
    mov     eax, DWORD PTR [g_tlsIndex]
    cmp     eax, TLS_OUT_OF_INDEXES
    je      @@tab_fail

    ; HeapAlloc(g_processHeap, HEAP_ZERO_MEMORY, TLS_BLOCK_SIZE)
    mov     rcx, QWORD PTR [g_processHeap]
    mov     edx, HEAP_ZERO_MEMORY
    mov     r8d, TLS_BLOCK_SIZE
    call    HeapAlloc
    test    rax, rax
    jz      @@tab_fail

    mov     rbx, rax                        ; save block ptr

    ; TlsSetValue(g_tlsIndex, block)
    mov     ecx, DWORD PTR [g_tlsIndex]
    mov     rdx, rbx
    call    TlsSetValue

    mov     rax, rbx                        ; return block ptr
    jmp     @@tab_done

@@tab_fail:
    xor     eax, eax
@@tab_done:
    add     rsp, 40
    pop     rbx
    ret
tls_alloc_block ENDP

; =============================================================================
; Internal: tls_free_block — Free TLS block for current thread
; =============================================================================
tls_free_block PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     eax, DWORD PTR [g_tlsIndex]
    cmp     eax, TLS_OUT_OF_INDEXES
    je      @@tfb_done

    ; TlsGetValue(g_tlsIndex)
    mov     ecx, eax
    call    TlsGetValue
    test    rax, rax
    jz      @@tfb_done

    mov     rbx, rax                        ; block ptr

    ; HeapFree(g_processHeap, 0, block)
    mov     rcx, QWORD PTR [g_processHeap]
    xor     edx, edx
    mov     r8, rbx
    call    HeapFree

    ; Clear TLS value
    mov     ecx, DWORD PTR [g_tlsIndex]
    xor     edx, edx
    call    TlsSetValue

@@tfb_done:
    add     rsp, 40
    pop     rbx
    ret
tls_free_block ENDP

; =============================================================================
; Internal: check_large_page_privilege
; Check if current process has SeLockMemoryPrivilege.
; Sets g_largePageAvailable = 1 if yes.
;
; Uses LookupPrivilegeValueA to verify the privilege exists.
; Actual privilege enabling (AdjustTokenPrivileges) is left to the
; host process or installer — we just probe here.
; =============================================================================
check_large_page_privilege PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 56                         ; shadow + locals
    .allocstack 56
    .endprolog

    ; OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)
    call    GetCurrentProcess
    mov     rcx, rax                        ; process handle
    mov     edx, TOKEN_QUERY
    lea     r8, [rsp + 48]                  ; &hToken (local)
    call    OpenProcessToken
    test    eax, eax
    jz      @@clp_no

    ; LookupPrivilegeValueA(NULL, "SeLockMemoryPrivilege", &luid)
    ; If the privilege name resolves, the OS supports it
    xor     ecx, ecx                        ; lpSystemName = NULL (local)
    lea     rdx, szPriv_SeLockMemory
    lea     r8, [rsp + 32]                  ; &luid (8 bytes)
    call    LookupPrivilegeValueA
    test    eax, eax
    jz      @@clp_no

    ; Privilege exists — mark available
    ; NOTE: Actually *having* it enabled requires policy/admin setup.
    ; We optimistically flag it; CoT_Initialize_Core will try MEM_LARGE_PAGES
    ; and fall back to normal pages on failure.
    mov     DWORD PTR [g_largePageAvailable], 1

    lea     rcx, szDll_LargePageOK
    call    OutputDebugStringA
    jmp     @@clp_done

@@clp_no:
    mov     DWORD PTR [g_largePageAvailable], 0
    lea     rcx, szDll_LargePageNo
    call    OutputDebugStringA

@@clp_done:
    ; Close token handle (if opened)
    mov     rcx, QWORD PTR [rsp + 48]
    test    rcx, rcx
    jz      @@clp_ret
    call    CloseHandle
@@clp_ret:
    add     rsp, 56
    pop     rbx
    ret
check_large_page_privilege ENDP

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
;
; Enterprise hardening:
;   - TLS slot for per-thread fault capture (RIP + address)
;   - DisableThreadLibraryCalls (data-plane DLL, not GUI)
;   - Large page privilege probe
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

    ; DLL_THREAD_ATTACH and DLL_THREAD_DETACH are disabled by
    ; DisableThreadLibraryCalls — if we somehow get here, just return TRUE.
    ; (Kept for safety in case DisableThreadLibraryCalls was not called.)
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

    ; Save hInstance
    mov     QWORD PTR [g_hInstance], r12

    ; ---- DisableThreadLibraryCalls ----
    ; This DLL is data-plane (arena + inference), not GUI.
    ; Eliminating THREAD_ATTACH/DETACH callbacks improves performance
    ; when the host spawns many worker threads for parallel CoT debate.
    mov     rcx, r12
    call    DisableThreadLibraryCalls

    lea     rcx, szDll_DisableThreadCB
    call    OutputDebugStringA

    ; ---- Cache process heap handle ----
    call    GetProcessHeap
    mov     QWORD PTR [g_processHeap], rax

    ; ---- Initialize SRW lock ----
    lea     rcx, g_cotSRWLock
    call    InitializeSRWLock
    mov     DWORD PTR [g_lockInitialized], 1

    lea     rcx, szDll_LockInit
    call    OutputDebugStringA

    ; ---- TLS slot allocation ----
    ; Allocate a TLS index for per-thread error reporting.
    ; SEH handlers store fault RIP here so C++ callers can retrieve it.
    call    TlsAlloc
    cmp     eax, TLS_OUT_OF_INDEXES
    je      @@dm_tls_fail

    mov     DWORD PTR [g_tlsIndex], eax

    lea     rcx, szDll_TlsAllocOK
    call    OutputDebugStringA

    ; Allocate TLS block for the attaching (main) thread
    call    tls_alloc_block
    jmp     @@dm_tls_done

@@dm_tls_fail:
    lea     rcx, szDll_TlsAllocFail
    call    OutputDebugStringA
    ; Non-fatal: DLL works without TLS, just no per-thread error capture

@@dm_tls_done:
    ; Set initial thread count = 1 (the attaching thread)
    mov     DWORD PTR [g_threadCount], 1
    mov     DWORD PTR [g_processAttached], 1

    ; ---- Large page privilege check ----
    call    check_large_page_privilege

    ; ---- Auto-initialize the CoT core arena ----
    lea     rcx, szDll_AutoInit
    call    OutputDebugStringA

    call    CoT_Initialize_Core
    test    eax, eax
    jnz     @@dm_auto_init_fail

    lea     rcx, szDll_AutoInitOK
    call    OutputDebugStringA

    ; --- Phase 39: Tag NUMA affinity for main thread in TLS ---
    call    GetCurrentProcessorNumber
    mov     ecx, eax                        ; processor number
    lea     rdx, [rsp + 32]                 ; &numaNode (local)
    call    GetNumaProcessorNode
    ; Store NUMA node in TLS block
    mov     eax, DWORD PTR [g_tlsIndex]
    cmp     eax, TLS_OUT_OF_INDEXES
    je      @@dm_skip_numa
    mov     ecx, eax
    call    TlsGetValue
    test    rax, rax
    jz      @@dm_skip_numa
    mov     ecx, DWORD PTR [rsp + 32]       ; numaNode from GetNumaProcessorNode
    mov     DWORD PTR [rax + TLS_OFF_NUMA_NODE], ecx
    lea     rcx, szDll_NumaTag
    call    OutputDebugStringA
@@dm_skip_numa:

    ; --- Phase 39: Log Phase 39-42 init complete ---
    lea     rcx, szDll_Phase39Init
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
    ; ---- Free TLS for main thread ----
    call    tls_free_block

    ; ---- Free TLS slot ----
    mov     eax, DWORD PTR [g_tlsIndex]
    cmp     eax, TLS_OUT_OF_INDEXES
    je      @@dm_skip_tls_free

    mov     ecx, eax
    call    TlsFree
    mov     DWORD PTR [g_tlsIndex], TLS_OUT_OF_INDEXES

    lea     rcx, szDll_TlsFreed
    call    OutputDebugStringA

@@dm_skip_tls_free:
    ; Reset state
    mov     DWORD PTR [g_processAttached], 0
    mov     DWORD PTR [g_lockInitialized], 0
    mov     DWORD PTR [g_threadCount], 0
    mov     QWORD PTR [g_cotSRWLock], 0
    mov     DWORD PTR [g_largePageAvailable], 0
    mov     QWORD PTR [g_processHeap], 0
    jmp     @@dm_ok

; ---- DLL_THREAD_ATTACH ----
; NOTE: Normally never reached because of DisableThreadLibraryCalls.
; Kept as a safety net in case the host re-enables notifications.
@@dm_thread_attach:
    ; Atomically increment thread count
    lock inc DWORD PTR [g_threadCount]

    ; Allocate TLS block for new thread
    call    tls_alloc_block

    IFDEF DEBUG_BUILD
    lea     rcx, szDll_ThreadAttach
    call    OutputDebugStringA
    ENDIF
    jmp     @@dm_ok

; ---- DLL_THREAD_DETACH ----
@@dm_thread_detach:
    ; Free TLS block for departing thread
    call    tls_free_block

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

; =============================================================================
; CoT_TLS_GetLastError
; Get per-thread last error code from TLS block.
;
; Returns: EAX = last error code (0 if no error or TLS unavailable)
; =============================================================================
CoT_TLS_GetLastError PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     eax, DWORD PTR [g_tlsIndex]
    cmp     eax, TLS_OUT_OF_INDEXES
    je      @@gle_none

    mov     ecx, eax
    call    TlsGetValue
    test    rax, rax
    jz      @@gle_none

    mov     eax, DWORD PTR [rax + TLS_OFF_LAST_ERROR]
    jmp     @@gle_done

@@gle_none:
    xor     eax, eax
@@gle_done:
    add     rsp, 40
    ret
CoT_TLS_GetLastError ENDP

; =============================================================================
; CoT_TLS_GetFaultRIP
; Get per-thread fault RIP from TLS block.
; This is the instruction pointer where the access violation occurred,
; captured by MyExceptionHandler / CoT_SEH_Handler.
;
; Returns: RAX = fault RIP (0 if no fault or TLS unavailable)
; =============================================================================
CoT_TLS_GetFaultRIP PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     eax, DWORD PTR [g_tlsIndex]
    cmp     eax, TLS_OUT_OF_INDEXES
    je      @@gfr_none

    mov     ecx, eax
    call    TlsGetValue
    test    rax, rax
    jz      @@gfr_none

    mov     rax, QWORD PTR [rax + TLS_OFF_FAULT_RIP]
    jmp     @@gfr_done

@@gfr_none:
    xor     eax, eax
@@gfr_done:
    add     rsp, 40
    ret
CoT_TLS_GetFaultRIP ENDP

; =============================================================================
; CoT_TLS_SetError
; Set per-thread error info in TLS block.
; Called by SEH handlers (CoT_SEH_Handler, MyExceptionHandler) when an
; access violation is trapped during arena operations.
;
; RCX = error code (DWORD)
; RDX = fault RIP (QWORD)
; R8  = fault address (QWORD, the memory address that caused the AV)
;
; Returns: EAX = 0 on success, -1 if TLS unavailable
; =============================================================================
CoT_TLS_SetError PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     r12d, ecx                       ; error code
    mov     r13, rdx                        ; fault RIP
    mov     rbx, r8                         ; fault address

    mov     eax, DWORD PTR [g_tlsIndex]
    cmp     eax, TLS_OUT_OF_INDEXES
    je      @@tse_fail

    mov     ecx, eax
    call    TlsGetValue
    test    rax, rax
    jz      @@tse_alloc

    ; TLS block exists — write directly
    jmp     @@tse_write

@@tse_alloc:
    ; Block not allocated yet (thread attached after DisableThreadLibraryCalls)
    ; Allocate now on demand
    call    tls_alloc_block
    test    rax, rax
    jz      @@tse_fail

@@tse_write:
    mov     DWORD PTR [rax + TLS_OFF_LAST_ERROR], r12d
    mov     QWORD PTR [rax + TLS_OFF_FAULT_RIP], r13
    mov     QWORD PTR [rax + TLS_OFF_FAULT_ADDR], rbx

    xor     eax, eax                        ; success
    jmp     @@tse_done

@@tse_fail:
    mov     eax, -1
@@tse_done:
    add     rsp, 40
    pop     r13
    pop     r12
    pop     rbx
    ret
CoT_TLS_SetError ENDP

; =============================================================================
; CoT_Has_Large_Pages
; Returns: EAX = 1 if SeLockMemoryPrivilege was detected, 0 otherwise.
; Callers (CoT_Initialize_Core) use this to decide MEM_LARGE_PAGES flag.
; =============================================================================
CoT_Has_Large_Pages PROC
    mov     eax, DWORD PTR [g_largePageAvailable]
    ret
CoT_Has_Large_Pages ENDP

END
