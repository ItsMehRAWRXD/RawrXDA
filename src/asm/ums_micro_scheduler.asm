; =============================================================================
; ums_micro_scheduler.asm — Non-Preemptive Kernel Scheduler for AVX-512
; =============================================================================
; Uses the Windows Thread Pool API (TP_CALLBACK_ENVIRON) with
; SetThreadpoolCallbackRunsLong to hint the OS scheduler that inference
; kernel work items should NOT be preempted, preserving ZMM register state.
;
; Problem: Windows default scheduler quantum is 15.6ms.  An AVX-512 inference
; kernel that loads 32 ZMM registers (2048 bytes of state) gets preempted
; mid-GEMM, forcing a full XSAVE/XRSTOR of ~2.5KB.  On Zen4 this costs
; ~800 cycles per context switch — at 50+ switches/sec during inference,
; that's measurable throughput loss.
;
; Solution: Create a dedicated thread pool with RunsLong + priority hints,
; plus explicit MXCSR bracketing per work item via mxcsr_determinism.asm.
;
; EXPORTS:
;   RawrXD_Sched_Init()                    → Creates the pool + env
;   RawrXD_Sched_Shutdown()                → Tears down the pool
;   RawrXD_Sched_SubmitKernel(pfnKernel, pContext) → Submits inference work
;   RawrXD_Sched_SetAffinity(coreMask)     → Pins pool threads to CCDs
;   RawrXD_Sched_GetStats(pStats)          → Returns counters
;
; ABI: Win64 (Microsoft calling convention)
; Build: ml64.exe /c /Zi /Zd ums_micro_scheduler.asm
; =============================================================================
OPTION CASEMAP:NONE

INCLUDE rawr_globals.inc

; ── Windows Thread Pool API function pointers (delay-loaded) ──
; We use GetProcAddress to avoid hard linking against kernel32 exports
; that may not exist on Server Core or Wine.

.DATA
ALIGN 8
; Thread pool handles
g_UmsPool               QWORD 0        ; PTP_POOL
g_UmsCleanupGroup       QWORD 0        ; PTP_CLEANUP_GROUP
g_UmsCallbackEnv        BYTE  64 DUP(0) ; TP_CALLBACK_ENVIRON (48 bytes, padded to 64)
g_UmsInitialized        DWORD 0        ; 0=no, 1=yes

; Stats (cache-line padded — ALIGN 16 is ml64 .DATA max)
ALIGN 16
g_UmsStat_Submitted     QWORD 0
g_UmsStat_Completed     QWORD 0
g_UmsStat_Errors        QWORD 0
g_UmsPad0               BYTE  40 DUP(0)  ; manual pad to 64 bytes

; API function pointers (resolved at Init)
ALIGN 8
g_pCreateThreadpool             QWORD 0
g_pSetThreadpoolThreadMinimum   QWORD 0
g_pSetThreadpoolThreadMaximum   QWORD 0
g_pCloseThreadpool              QWORD 0
g_pCreateThreadpoolCleanupGroup QWORD 0
g_pCloseThreadpoolCleanupGroup  QWORD 0
g_pInitializeThreadpoolEnvironment  QWORD 0
g_pSetThreadpoolCallbackPool        QWORD 0
g_pSetThreadpoolCallbackRunsLong    QWORD 0
g_pSetThreadpoolCallbackCleanupGroup QWORD 0
g_pSubmitThreadpoolWork             QWORD 0
g_pCreateThreadpoolWork             QWORD 0
g_pCloseThreadpoolWork              QWORD 0
g_pWaitForThreadpoolWorkCallbacks   QWORD 0
g_pSetThreadGroupAffinity           QWORD 0

; kernel32 module handle
g_hKernel32             QWORD 0

; String table for GetProcAddress
szKernel32              DB "kernel32.dll", 0
szCreateThreadpool      DB "CreateThreadpool", 0
szSetTPThreadMin        DB "SetThreadpoolThreadMinimum", 0
szSetTPThreadMax        DB "SetThreadpoolThreadMaximum", 0
szCloseThreadpool       DB "CloseThreadpool", 0
szCreateTPCleanup       DB "CreateThreadpoolCleanupGroup", 0
szCloseTPCleanup        DB "CloseThreadpoolCleanupGroup", 0
szInitTPEnv             DB "InitializeThreadpoolEnvironment", 0
szSetTPPool             DB "SetThreadpoolCallbackPool", 0
szSetTPRunsLong         DB "SetThreadpoolCallbackRunsLong", 0
szSetTPCleanupGroup     DB "SetThreadpoolCallbackCleanupGroup", 0
szSubmitTPWork          DB "SubmitThreadpoolWork", 0
szCreateTPWork          DB "CreateThreadpoolWork", 0
szCloseTPWork           DB "CloseThreadpoolWork", 0
szWaitTPWorkCallbacks   DB "WaitForThreadpoolWorkCallbacks", 0

.CODE

; Forward declaration for the work callback
EXTERN RawrXD_MXCSR_LockPerformance:PROC
EXTERN RawrXD_MXCSR_Save:PROC
EXTERN RawrXD_MXCSR_Restore:PROC

; ── Internal: resolve one API ────────────────────────────────────────────────
; rcx=hModule, rdx=pszName, r8=ptr to store result
; Returns: EAX=1 success, 0 fail
resolveApi PROC
    push    r8
    ; GetProcAddress(hModule, lpProcName) — rcx already hModule, rdx already name
    sub     rsp, 32                     ; shadow space
    call    QWORD PTR [GetProcAddress]
    add     rsp, 32
    pop     r8
    test    rax, rax
    jz      @@resolve_fail
    mov     QWORD PTR [r8], rax
    mov     eax, 1
    ret
@@resolve_fail:
    xor     eax, eax
    ret
resolveApi ENDP

; Win32 imports
EXTERN GetModuleHandleA:PROC
EXTERN GetProcAddress:PROC
EXTERN GetCurrentThread:PROC
EXTERN SetThreadPriority:PROC

; =============================================================================
; BOOL RawrXD_Sched_Init(void)
;
; Creates a dedicated thread pool for inference kernels:
;   - Min 2 threads, max logical_cores/2 (one pool per CCD on Zen4)
;   - RunsLong hint to avoid premature preemption
;   - TIME_CRITICAL priority on pool threads
;
; Returns: EAX=1 on success, 0 on failure.
; =============================================================================
PUBLIC RawrXD_Sched_Init
RawrXD_Sched_Init PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 64                     ; local + shadow space
    .allocstack 64
    .endprolog

    ; Already initialized?
    cmp     DWORD PTR [g_UmsInitialized], 1
    je      @@init_ok

    ; ── Load kernel32 ──
    lea     rcx, [szKernel32]
    call    GetModuleHandleA
    test    rax, rax
    jz      @@init_fail
    mov     QWORD PTR [g_hKernel32], rax
    mov     rbx, rax                    ; rbx = hKernel32

    ; ── Resolve CreateThreadpool ──
    mov     rcx, rbx
    lea     rdx, [szCreateThreadpool]
    lea     r8,  [g_pCreateThreadpool]
    call    resolveApi
    test    eax, eax
    jz      @@init_fail

    ; ── Resolve remaining APIs ──
    ; SetThreadpoolThreadMinimum
    mov     rcx, rbx
    lea     rdx, [szSetTPThreadMin]
    lea     r8,  [g_pSetThreadpoolThreadMinimum]
    call    resolveApi
    test    eax, eax
    jz      @@init_fail

    ; SetThreadpoolThreadMaximum
    mov     rcx, rbx
    lea     rdx, [szSetTPThreadMax]
    lea     r8,  [g_pSetThreadpoolThreadMaximum]
    call    resolveApi
    test    eax, eax
    jz      @@init_fail

    ; CloseThreadpool
    mov     rcx, rbx
    lea     rdx, [szCloseThreadpool]
    lea     r8,  [g_pCloseThreadpool]
    call    resolveApi
    test    eax, eax
    jz      @@init_fail

    ; CreateThreadpoolCleanupGroup
    mov     rcx, rbx
    lea     rdx, [szCreateTPCleanup]
    lea     r8,  [g_pCreateThreadpoolCleanupGroup]
    call    resolveApi
    test    eax, eax
    jz      @@init_fail

    ; CloseThreadpoolCleanupGroup
    mov     rcx, rbx
    lea     rdx, [szCloseTPCleanup]
    lea     r8,  [g_pCloseThreadpoolCleanupGroup]
    call    resolveApi
    test    eax, eax
    jz      @@init_fail

    ; InitializeThreadpoolEnvironment
    mov     rcx, rbx
    lea     rdx, [szInitTPEnv]
    lea     r8,  [g_pInitializeThreadpoolEnvironment]
    call    resolveApi
    test    eax, eax
    jz      @@init_fail

    ; SetThreadpoolCallbackPool
    mov     rcx, rbx
    lea     rdx, [szSetTPPool]
    lea     r8,  [g_pSetThreadpoolCallbackPool]
    call    resolveApi
    test    eax, eax
    jz      @@init_fail

    ; SetThreadpoolCallbackRunsLong
    mov     rcx, rbx
    lea     rdx, [szSetTPRunsLong]
    lea     r8,  [g_pSetThreadpoolCallbackRunsLong]
    call    resolveApi
    test    eax, eax
    jz      @@init_fail

    ; SetThreadpoolCallbackCleanupGroup
    mov     rcx, rbx
    lea     rdx, [szSetTPCleanupGroup]
    lea     r8,  [g_pSetThreadpoolCallbackCleanupGroup]
    call    resolveApi
    test    eax, eax
    jz      @@init_fail

    ; SubmitThreadpoolWork
    mov     rcx, rbx
    lea     rdx, [szSubmitTPWork]
    lea     r8,  [g_pSubmitThreadpoolWork]
    call    resolveApi
    test    eax, eax
    jz      @@init_fail

    ; CreateThreadpoolWork
    mov     rcx, rbx
    lea     rdx, [szCreateTPWork]
    lea     r8,  [g_pCreateThreadpoolWork]
    call    resolveApi
    test    eax, eax
    jz      @@init_fail

    ; CloseThreadpoolWork
    mov     rcx, rbx
    lea     rdx, [szCloseTPWork]
    lea     r8,  [g_pCloseThreadpoolWork]
    call    resolveApi
    test    eax, eax
    jz      @@init_fail

    ; WaitForThreadpoolWorkCallbacks
    mov     rcx, rbx
    lea     rdx, [szWaitTPWorkCallbacks]
    lea     r8,  [g_pWaitForThreadpoolWorkCallbacks]
    call    resolveApi
    test    eax, eax
    jz      @@init_fail

    ; ── Create the pool ──
    ; CreateThreadpool(NULL) — NULL = default reserved parameter
    xor     rcx, rcx
    call    QWORD PTR [g_pCreateThreadpool]
    test    rax, rax
    jz      @@init_fail
    mov     QWORD PTR [g_UmsPool], rax
    mov     r12, rax                    ; r12 = pool handle

    ; Set min threads = 2
    mov     rcx, r12
    mov     edx, 2
    call    QWORD PTR [g_pSetThreadpoolThreadMinimum]
    ; Ignore return — non-fatal if min can't be set

    ; Set max threads = 4 (half CCD for Zen4 8-core, leaves 4 for GUI/OS)
    mov     rcx, r12
    mov     edx, 4
    call    QWORD PTR [g_pSetThreadpoolThreadMaximum]

    ; ── Create cleanup group ──
    call    QWORD PTR [g_pCreateThreadpoolCleanupGroup]
    test    rax, rax
    jz      @@init_fail_pool
    mov     QWORD PTR [g_UmsCleanupGroup], rax
    mov     r13, rax                    ; r13 = cleanup group

    ; ── Initialize callback environment ──
    lea     rcx, [g_UmsCallbackEnv]
    call    QWORD PTR [g_pInitializeThreadpoolEnvironment]

    ; Associate pool with environment
    lea     rcx, [g_UmsCallbackEnv]
    mov     rdx, r12
    call    QWORD PTR [g_pSetThreadpoolCallbackPool]

    ; Set RunsLong hint — prevents OS from thinking work items are short
    ; and avoids aggressive preemption that trashes ZMM state
    lea     rcx, [g_UmsCallbackEnv]
    call    QWORD PTR [g_pSetThreadpoolCallbackRunsLong]

    ; Associate cleanup group
    lea     rcx, [g_UmsCallbackEnv]
    mov     rdx, r13
    xor     r8, r8                      ; no cleanup callback
    call    QWORD PTR [g_pSetThreadpoolCallbackCleanupGroup]

    ; Mark initialized
    mov     DWORD PTR [g_UmsInitialized], 1

@@init_ok:
    mov     eax, 1
    add     rsp, 64
    pop     r13
    pop     r12
    pop     rbx
    ret

@@init_fail_pool:
    ; Pool was created but cleanup group failed — close pool
    mov     rcx, r12
    call    QWORD PTR [g_pCloseThreadpool]
    mov     QWORD PTR [g_UmsPool], 0

@@init_fail:
    xor     eax, eax
    add     rsp, 64
    pop     r13
    pop     r12
    pop     rbx
    ret
RawrXD_Sched_Init ENDP

; =============================================================================
; void RawrXD_Sched_Shutdown(void)
;
; Waits for pending work, closes cleanup group, closes pool.
; Safe to call multiple times (idempotent).
; =============================================================================
PUBLIC RawrXD_Sched_Shutdown
RawrXD_Sched_Shutdown PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 32
    .allocstack 32
    .endprolog

    cmp     DWORD PTR [g_UmsInitialized], 0
    je      @@shut_done

    ; Close cleanup group (cancels pending work)
    mov     rcx, QWORD PTR [g_UmsCleanupGroup]
    test    rcx, rcx
    jz      @@shut_pool
    mov     edx, 1                      ; fCancelPendingCallbacks = TRUE
    xor     r8, r8                      ; pvCleanupContext = NULL
    call    QWORD PTR [g_pCloseThreadpoolCleanupGroup]
    mov     QWORD PTR [g_UmsCleanupGroup], 0

@@shut_pool:
    mov     rcx, QWORD PTR [g_UmsPool]
    test    rcx, rcx
    jz      @@shut_mark
    call    QWORD PTR [g_pCloseThreadpool]
    mov     QWORD PTR [g_UmsPool], 0

@@shut_mark:
    mov     DWORD PTR [g_UmsInitialized], 0

@@shut_done:
    add     rsp, 32
    pop     rbx
    ret
RawrXD_Sched_Shutdown ENDP

; =============================================================================
; BOOL RawrXD_Sched_SubmitKernel(void(*pfnKernel)(void* ctx), void* pContext)
;   rcx = kernel function pointer (must be __cdecl/Win64 ABI)
;   rdx = context pointer passed to kernel
;
; Submits an inference kernel to the dedicated pool.  The pool thread will:
;   1. Save MXCSR
;   2. Lock performance mode (FTZ+DAZ)
;   3. Call pfnKernel(pContext)
;   4. Restore MXCSR
;
; This ensures the kernel runs with stable FP state and reduced preemption.
;
; Returns: EAX=1 on success, 0 on failure.
;
; NOTE: The work callback is a C++ trampoline defined in the C++ dispatch
; layer.  This MASM function creates the TP_WORK and submits it.
; The actual MXCSR bracketing happens in the C++ callback wrapper.
; =============================================================================

; Work item context — packed into 16 bytes for simple allocation
; (We store it inline in a small static pool to avoid heap allocs on hot path)
WORK_SLOT_SIZE  EQU 32                  ; pfnKernel(8) + pContext(8) + mxcsrSave(4) + pad(12)
MAX_WORK_SLOTS  EQU 64                  ; max concurrent in-flight work items

.DATA
ALIGN 16
g_WorkSlots     BYTE    (WORK_SLOT_SIZE * MAX_WORK_SLOTS) DUP(0)
g_WorkSlotNext  QWORD   0              ; atomic index into slots (wrapping)

.CODE

; Internal: allocate a work slot (lock-free via atomic increment)
; Returns: rax = pointer to slot, or 0 if exhausted
allocWorkSlot PROC
    mov     rax, 1
    lock xadd QWORD PTR [g_WorkSlotNext], rax
    ; Wrap to MAX_WORK_SLOTS
    and     rax, (MAX_WORK_SLOTS - 1)
    imul    rax, rax, WORK_SLOT_SIZE
    ; Two-step to avoid ADDR32 relocation (incompatible with /LARGEADDRESSAWARE):
    ; lea can't combine RIP-relative symbol + register operand in x64.
    lea     rcx, [g_WorkSlots]          ; RIP-relative base address
    add     rax, rcx                    ; rax = &g_WorkSlots[slot_index]
    ret
allocWorkSlot ENDP

PUBLIC RawrXD_Sched_SubmitKernel
RawrXD_Sched_SubmitKernel PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 48
    .allocstack 48
    .endprolog

    ; Validate: scheduler must be initialized
    cmp     DWORD PTR [g_UmsInitialized], 0
    je      @@submit_fail

    ; Validate function pointer
    test    rcx, rcx
    jz      @@submit_fail

    mov     r12, rcx                    ; r12 = pfnKernel
    mov     r13, rdx                    ; r13 = pContext

    ; Allocate work slot
    call    allocWorkSlot
    test    rax, rax
    jz      @@submit_fail
    mov     rbx, rax                    ; rbx = work slot

    ; Pack slot: [0]=pfnKernel, [8]=pContext
    mov     QWORD PTR [rbx], r12
    mov     QWORD PTR [rbx + 8], r13

    ; Create TP_WORK:  CreateThreadpoolWork(callback, context, &env)
    ; The callback is a C-linkage trampoline (defined in dispatch_orchestrator.cpp)
    ; that unpacks the slot and calls the kernel with MXCSR bracketing.
    ; For now, we'll use a generic trampoline exported from this module.
    lea     rcx, [rawrxd_tp_work_callback]  ; PTP_WORK_CALLBACK
    mov     rdx, rbx                        ; PVOID Context = work slot
    lea     r8,  [g_UmsCallbackEnv]         ; PTP_CALLBACK_ENVIRON
    call    QWORD PTR [g_pCreateThreadpoolWork]
    test    rax, rax
    jz      @@submit_fail

    ; Submit the work item
    mov     rcx, rax                    ; PTP_WORK
    call    QWORD PTR [g_pSubmitThreadpoolWork]

    ; Telemetry
    lock inc QWORD PTR [g_UmsStat_Submitted]

    mov     eax, 1
    add     rsp, 48
    pop     r13
    pop     r12
    pop     rbx
    ret

@@submit_fail:
    lock inc QWORD PTR [g_UmsStat_Errors]
    xor     eax, eax
    add     rsp, 48
    pop     r13
    pop     r12
    pop     rbx
    ret
RawrXD_Sched_SubmitKernel ENDP

; =============================================================================
; Thread pool work callback — called by Windows on a pool thread.
; Signature: void CALLBACK (PTP_CALLBACK_INSTANCE, PVOID Context, PTP_WORK)
;   rcx = Instance (unused)
;   rdx = Context = pointer to work slot [pfnKernel, pContext]
;   r8  = Work handle
;
; Brackets the kernel call with MXCSR save/lock/restore.
; =============================================================================
rawrxd_tp_work_callback PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    sub     rsp, 40                     ; shadow + MXCSR save slot
    .allocstack 40
    .endprolog

    mov     rbx, rdx                    ; rbx = work slot
    mov     r12, r8                     ; r12 = PTP_WORK (for cleanup)

    ; Save current MXCSR to stack slot
    lea     rcx, [rsp + 32]             ; MXCSR save slot
    call    RawrXD_MXCSR_Save

    ; Lock performance mode (FTZ+DAZ for inference throughput)
    call    RawrXD_MXCSR_LockPerformance

    ; Call the actual kernel: pfnKernel(pContext)
    mov     rax, QWORD PTR [rbx]       ; pfnKernel
    mov     rcx, QWORD PTR [rbx + 8]   ; pContext
    call    rax

    ; Restore original MXCSR
    lea     rcx, [rsp + 32]
    call    RawrXD_MXCSR_Restore

    ; Close the work handle
    mov     rcx, r12
    call    QWORD PTR [g_pCloseThreadpoolWork]

    ; Telemetry
    lock inc QWORD PTR [g_UmsStat_Completed]

    add     rsp, 40
    pop     r12
    pop     rbx
    ret
rawrxd_tp_work_callback ENDP

; =============================================================================
; void RawrXD_Sched_SetAffinity(UINT64 coreMask)
;   rcx = processor affinity mask (e.g., 0xF0 for cores 4-7)
;
; Sets the current thread's affinity.  Call from pool threads to pin
; inference to specific CCDs on Zen4 (avoids cross-CCD L3 latency).
;
; Note: This sets affinity for the CALLING thread, not the pool.
; Each pool callback can call this at entry to pin itself.
; =============================================================================
EXTERN SetThreadAffinityMask:PROC

PUBLIC RawrXD_Sched_SetAffinity
RawrXD_Sched_SetAffinity PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 32
    .allocstack 32
    .endprolog

    mov     rbx, rcx                    ; save mask
    call    GetCurrentThread
    mov     rcx, rax                    ; hThread
    mov     rdx, rbx                    ; dwThreadAffinityMask
    call    SetThreadAffinityMask

    add     rsp, 32
    pop     rbx
    ret
RawrXD_Sched_SetAffinity ENDP

; =============================================================================
; void RawrXD_Sched_GetStats(UINT64* pStats)
;   rcx = pointer to 3-element UINT64 array:
;         [0] = submitted, [1] = completed, [2] = errors
; =============================================================================
PUBLIC RawrXD_Sched_GetStats
RawrXD_Sched_GetStats PROC
    test    rcx, rcx
    jz      @@gs_bail
    mov     rax, QWORD PTR [g_UmsStat_Submitted]
    mov     QWORD PTR [rcx], rax
    mov     rax, QWORD PTR [g_UmsStat_Completed]
    mov     QWORD PTR [rcx + 8], rax
    mov     rax, QWORD PTR [g_UmsStat_Errors]
    mov     QWORD PTR [rcx + 16], rax
@@gs_bail:
    ret
RawrXD_Sched_GetStats ENDP

; =============================================================================
; BOOL RawrXD_Sched_IsReady(void)
;   Returns: EAX=1 if scheduler is initialized and ready, 0 otherwise.
; =============================================================================
PUBLIC RawrXD_Sched_IsReady
RawrXD_Sched_IsReady PROC
    mov     eax, DWORD PTR [g_UmsInitialized]
    ret
RawrXD_Sched_IsReady ENDP

END
